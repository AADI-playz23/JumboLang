// src/Interpreter.cpp
// JumboLang Virtual Machine — v2.0
// Changes from v1:
//   • All handler signatures use const shared_ptr& (no refcount churn)
//   • {llm} supports provider=, envkey=, store= attributes
//   • {https} router callback passes method to {route} handlers
//   • {shell} sanitizes for metacharacter injection before calling system()
//   • {file} delegates to the secure FileSystem class (blocks ../ traversal)
//   • {fetch} is a new tag for plain HTTP API calls (GET/POST/PUT/DELETE/PATCH)
#include "../include/Interpreter.h"
#include "../include/features/Network.h"
#include "../include/features/AI.h"
#include "../include/features/FileSystem.h"
#include "../include/features/Database.h"
#include "../include/features/JSON.h"
#include "../include/features/HTTP.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
// INTERNAL HELPERS
// ─────────────────────────────────────────────────────────────────────────────

static auto trimWhitespace = [](std::string& s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    if (!s.empty()) s.erase(s.find_last_not_of(" \t\r\n") + 1);
};

// ── Recursive-descent math evaluator (PEMDAS, variables, decimals) ──────────
class MathEvaluator {
    std::string expr;
    size_t pos = 0;
    std::unordered_map<std::string, std::string>& vars;

    double parseFactor() {
        while (pos < expr.size() && isspace(expr[pos])) pos++;
        if (pos >= expr.size()) return 0;

        double sign = 1;
        if      (expr[pos] == '-') { sign = -1; pos++; }
        else if (expr[pos] == '+') { pos++; }

        while (pos < expr.size() && isspace(expr[pos])) pos++;

        double result = 0;
        if (expr[pos] == '(') {
            pos++;
            result = parseExpression();
            if (pos < expr.size() && expr[pos] == ')') pos++;
        } else if (isalpha(expr[pos]) || expr[pos] == '_') {
            std::string varName;
            while (pos < expr.size() && (isalnum(expr[pos]) || expr[pos] == '_'))
                varName += expr[pos++];
            if (vars.count(varName)) {
                try { result = std::stod(vars[varName]); } catch (...) { result = 0; }
            }
        } else {
            std::string numStr;
            while (pos < expr.size() && (isdigit(expr[pos]) || expr[pos] == '.'))
                numStr += expr[pos++];
            try { result = std::stod(numStr); } catch (...) { result = 0; }
        }
        return sign * result;
    }

    double parseTerm() {
        double result = parseFactor();
        while (pos < expr.size()) {
            while (pos < expr.size() && isspace(expr[pos])) pos++;
            if (pos >= expr.size()) break;
            char op = expr[pos];
            if (op != '*' && op != '/' && op != '%') break;
            pos++;
            double rhs = parseFactor();
            if      (op == '*') result *= rhs;
            else if (op == '/') result = (rhs != 0) ? result / rhs : 0;
            else if (op == '%') result = std::fmod(result, rhs);
        }
        return result;
    }

    double parseExpression() {
        double result = parseTerm();
        while (pos < expr.size()) {
            while (pos < expr.size() && isspace(expr[pos])) pos++;
            if (pos >= expr.size()) break;
            char op = expr[pos];
            if (op != '+' && op != '-') break;
            pos++;
            double rhs = parseTerm();
            if      (op == '+') result += rhs;
            else if (op == '-') result -= rhs;
        }
        return result;
    }

public:
    MathEvaluator(std::string e, std::unordered_map<std::string, std::string>& v)
        : expr(std::move(e)), vars(v) {}

    std::string evaluate() {
        bool hasMath = false;
        for (char c : expr)
            if (c == '+' || c == '-' || c == '*' || c == '/' ||
                c == '%' || c == '(' || c == ')') { hasMath = true; break; }

        if (!hasMath) {
            std::string t = expr;
            trimWhitespace(t);
            if (vars.count(t)) return vars[t];
            return t;
        }
        try {
            double val = parseExpression();
            std::string res = std::to_string(val);
            res.erase(res.find_last_not_of('0') + 1, std::string::npos);
            if (!res.empty() && res.back() == '.') res.pop_back();
            return res;
        } catch (...) { return expr; }
    }
};

// ── Variable interpolator — replaces $varName with its value ─────────────────
static std::string interpolate(const std::string& text,
                               std::unordered_map<std::string, std::string>& vars) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '$' && i + 1 < text.size() && isalpha(text[i + 1])) {
            std::string varName;
            i++;
            while (i < text.size() && (isalnum(text[i]) || text[i] == '_'))
                varName += text[i++];
            i--;
            result += vars.count(varName) ? vars[varName] : ("$" + varName);
        } else {
            result += text[i];
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// SHELL COMMAND SANITIZER
// Blocks the most dangerous shell metacharacters to mitigate injection risk.
// The {shell} tag should never receive unsanitized user input, but this is
// a defence-in-depth measure.
// ─────────────────────────────────────────────────────────────────────────────
bool Interpreter::isSafeShellCommand(const std::string& cmd) {
    // Metacharacters that can chain or redirect shell commands
    const std::vector<std::string> blocked = {
        "&&", "||", ";;",
        "|", ";", ">", "<",
        "`",       // backtick command substitution
        "$(", "${" // $() and ${} substitution
    };
    for (const auto& b : blocked) {
        if (cmd.find(b) != std::string::npos) {
            std::cerr << "    🔒 [SECURITY] Blocked dangerous shell pattern '"
                      << b << "' in command: " << cmd.substr(0, 80) << "\n";
            return false;
        }
    }
    // Block newlines (can embed multi-line commands)
    if (cmd.find('\n') != std::string::npos || cmd.find('\r') != std::string::npos) {
        std::cerr << "    🔒 [SECURITY] Blocked shell command with embedded newline.\n";
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// INTERPRETER CONSTRUCTOR — registers all tags
// ─────────────────────────────────────────────────────────────────────────────
Interpreter::Interpreter() {
    featureRegistry["main"]   = [this](const auto& n) { handleMain(n);   };
    featureRegistry["llm"]    = [this](const auto& n) { handleLlm(n);    };
    featureRegistry["file"]   = [this](const auto& n) { handleFile(n);   };
    featureRegistry["db"]     = [this](const auto& n) { handleDb(n);     };
    featureRegistry["json"]   = [this](const auto& n) { handleJson(n);   };

    featureRegistry["var"]    = [this](const auto& n) { handleVar(n);    };
    featureRegistry["print"]  = [this](const auto& n) { handlePrint(n);  };
    featureRegistry["shell"]  = [this](const auto& n) { handleShell(n);  };

    featureRegistry["if"]     = [this](const auto& n) { handleIf(n);     };
    featureRegistry["else"]   = [this](const auto& n) { handleElse(n);   };

    // Web framework
    featureRegistry["https"]  = [this](const auto& n) { handleHttps(n);  };
    featureRegistry["route"]  = [this](const auto& n) { handleRoute(n);  };
    featureRegistry["res"]    = [this](const auto& n) { handleRes(n);    };
    featureRegistry["tunnel"] = [this](const auto& n) { handleTunnel(n); };

    // V2.0: plain HTTP API calls
    featureRegistry["fetch"]  = [this](const auto& n) { handleFetch(n);  };
}

// ─────────────────────────────────────────────────────────────────────────────
// CORE DISPATCH
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::executeNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    auto it = featureRegistry.find(node->tagName);
    if (it != featureRegistry.end()) {
        it->second(node);
    } else {
        std::cerr << "⚠️  JumboLang Warning: Unknown tag {" << node->tagName << "}\n";
    }
}

void Interpreter::run(std::shared_ptr<ASTNode> rootNode) {
    std::cout << "--- 🐘 JUMBOLANG VIRTUAL MACHINE STARTING ---\n";
    executeNode(rootNode);
    std::cout << "--- 🐘 EXECUTION FINISHED SUCCESSFULLY ---\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// V1.0 HANDLERS
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleMain(const std::shared_ptr<ASTNode>& node) {
    for (const auto& child : node->children) executeNode(child);
}

void Interpreter::handleLlm(const std::shared_ptr<ASTNode>& node) {
    // Resolve provider
    std::string providerStr = "openai";
    if (node->attributes.count("provider")) providerStr = node->attributes.at("provider");

    AIProvider provider = AIManager::providerFromString(providerStr);

    // Model name (default changes per provider)
    std::string model = "gpt-4o";
    if (provider == AIProvider::ANTHROPIC) model = "claude-3-5-sonnet-20241022";
    if (node->attributes.count("model"))   model = node->attributes.at("model");

    // Custom base URL for OpenAI-compatible endpoints
    std::string customUrl;
    if (node->attributes.count("baseurl")) customUrl = node->attributes.at("baseurl");

    AIManager ai(model, provider, customUrl);

    // Override which env variable to read the key from
    if (node->attributes.count("envkey"))
        ai.loadApiKeyFromEnv(node->attributes.at("envkey"));

    if (!node->bodyContent.empty()) {
        std::string response = ai.generateResponse(node->bodyContent);

        // Optionally store the response in a JumboLang variable
        if (node->attributes.count("store")) {
            variables[node->attributes.at("store")] = response;
        } else {
            std::cout << "    ✨ [AI] " << response << "\n";
        }
    }

    for (const auto& child : node->children) executeNode(child);
}

void Interpreter::handleFile(const std::shared_ptr<ASTNode>& node) {
    // FileSystem::write/read already call isSafePath() internally
    std::string path = node->attributes.count("path") ? node->attributes.at("path") : "";
    if (path.empty()) { std::cerr << "    ❌ [FILE] Missing 'path' attribute.\n"; return; }

    if (node->attributes.count("action") && node->attributes.at("action") == "write") {
        FileSystem::write(path, node->bodyContent);
    } else {
        std::cout << "    📖 [FILE] " << FileSystem::read(path) << "\n";
    }
}

void Interpreter::handleDb(const std::shared_ptr<ASTNode>& node) {
    std::string key = node->attributes.count("key") ? node->attributes.at("key") : "";
    DatabaseManager db;
    if (node->attributes.count("action") && node->attributes.at("action") == "set") {
        db.set(key, node->bodyContent);
    } else {
        std::cout << "    🗄️  [DB] " << key << " => " << db.get(key) << "\n";
    }
}

void Interpreter::handleJson(const std::shared_ptr<ASTNode>& node) {
    auto data = JSONManager::parse(node->bodyContent);
    for (const auto& [key, val] : data)
        std::cout << "    📊 " << key << " : " << val << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// V1.1 HANDLERS — state & logic
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleVar(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    size_t eqPos = content.find('=');
    if (eqPos == std::string::npos) {
        std::cerr << "    ⚠️  [SYNTAX] {var} expects 'name = value'\n";
        return;
    }
    std::string varName  = content.substr(0, eqPos);
    std::string varValue = content.substr(eqPos + 1);
    trimWhitespace(varName);
    MathEvaluator eval(varValue, variables);
    variables[varName] = eval.evaluate();
}

void Interpreter::handlePrint(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, variables);
    std::cout << "    🖨️  [PRINT] " << content << "\n";
}

void Interpreter::handleShell(const std::shared_ptr<ASTNode>& node) {
    std::string cmd = node->bodyContent;
    trimWhitespace(cmd);

    if (!isSafeShellCommand(cmd)) {
        std::cerr << "    🔒 [SECURITY] Shell command blocked. "
                  << "Remove dangerous metacharacters and try again.\n";
        return;
    }

    std::cout << "    🖥️  [OS SHELL] Executing: " << cmd << "\n";
    if (system(cmd.c_str()) != 0)
        std::cerr << "    ❌ [OS SHELL] Command returned non-zero exit code.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// LOGIC HANDLERS
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleIf(const std::shared_ptr<ASTNode>& node) {
    lastIfCondition = false;
    if (node->attributes.count("var") && node->attributes.count("equals")) {
        const std::string& varName = node->attributes.at("var");
        if (variables.count(varName) &&
            variables[varName] == node->attributes.at("equals")) {
            lastIfCondition = true;
        }
    }
    if (lastIfCondition)
        for (const auto& child : node->children) executeNode(child);
}

void Interpreter::handleElse(const std::shared_ptr<ASTNode>& node) {
    if (!lastIfCondition)
        for (const auto& child : node->children) executeNode(child);
}

// ─────────────────────────────────────────────────────────────────────────────
// WEB FRAMEWORK HANDLERS
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleHttps(const std::shared_ptr<ASTNode>& node) {
    int portNum = 8080;
    if (node->attributes.count("port"))
        portNum = std::stoi(node->attributes.at("port"));

    NetworkManager net(portNum);
    if (net.initializeSocket() && net.bindToHardware()) {

        // The router lambda is called per-request from worker threads
        net.listenAndServe([this, node](std::string path, std::string method) -> std::string {
            this->activeRoutePath    = path;
            this->activeRouteMethod  = method;
            this->currentHttpResponse = "";

            for (const auto& child : node->children) this->executeNode(child);

            if (this->currentHttpResponse.empty()) {
                return "{\"error\": \"404 Route Not Found in JumboLang\"}";
            }
            return this->currentHttpResponse;
        });
    } else {
        std::cerr << "    ❌ [NETWORK] Failed to bind to port " << portNum << "\n";
    }
    net.shutdown();
}

void Interpreter::handleRoute(const std::shared_ptr<ASTNode>& node) {
    bool pathMatch   = !node->attributes.count("path")   ||
                       node->attributes.at("path") == activeRoutePath;
    bool methodMatch = !node->attributes.count("method") ||
                       node->attributes.at("method") == activeRouteMethod;

    if (pathMatch && methodMatch) {
        for (const auto& child : node->children) executeNode(child);
    }
}

void Interpreter::handleRes(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, variables);
    currentHttpResponse += content;
}

void Interpreter::handleTunnel(const std::shared_ptr<ASTNode>& node) {
    std::string port = "8080";
    if (node->attributes.count("port")) port = node->attributes.at("port");
    std::cout << "    🚇 [TUNNEL] Spawning secure public URL for port " << port << "...\n";
    // npx localtunnel — background process (fire-and-forget, exit code not meaningful)
    std::string cmd = "npx localtunnel --port " + port + " > tunnel.log 2>&1 &";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    system(cmd.c_str());
#pragma GCC diagnostic pop
    std::cout << "    🔗 [TUNNEL] Check tunnel.log for your public URL.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// V2.0 — {fetch} TAG
// Plain HTTP API calls — not AI, just raw network requests.
//
// Syntax:
//   {fetch url="https://api.example.com/users" method="GET" store="result"}{-fetch}
//   {fetch url="https://api.example.com/users" method="POST" store="result"}
//     {"name": "Alice"}
//   {-fetch}
//
// Attributes:
//   url     — Required. The full URL to request.
//   method  — Optional. GET (default), POST, PUT, DELETE, PATCH.
//   store   — Optional. Variable name to store the response body in.
//   header_* — Optional. Any attribute prefixed with "header_" becomes a request
//              header. e.g. header_Authorization="Bearer token" sends
//              Authorization: Bearer token
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleFetch(const std::shared_ptr<ASTNode>& node) {
    // Validate URL
    if (!node->attributes.count("url")) {
        std::cerr << "    ❌ [FETCH] Missing required 'url' attribute.\n";
        return;
    }
    std::string url    = node->attributes.at("url");
    std::string method = "GET";
    if (node->attributes.count("method"))
        method = node->attributes.at("method");

    // Interpolate $variables in the URL
    url = interpolate(url, variables);

    // Collect header_* attributes → headers map
    std::map<std::string, std::string> headers;
    for (const auto& [key, val] : node->attributes) {
        if (key.rfind("header_", 0) == 0) {
            std::string headerName = key.substr(7); // strip "header_"
            // Convert underscores to hyphens (e.g. header_Content_Type → Content-Type)
            std::replace(headerName.begin(), headerName.end(), '_', '-');
            headers[headerName] = val;
        }
    }

    // Body for POST/PUT/PATCH is the tag's body content
    std::string body = node->bodyContent;
    trimWhitespace(body);
    body = interpolate(body, variables);

    std::cout << "    🌐 [FETCH] " << method << " " << url << "\n";

    HttpResponse resp = HttpClient::request(method, url, body, headers);

    if (resp.success) {
        std::cout << "    ✅ [FETCH] HTTP " << resp.statusCode << " — "
                  << resp.body.size() << " bytes received\n";
    } else {
        std::cerr << "    ❌ [FETCH] Failed (HTTP " << resp.statusCode
                  << "): " << resp.errorMessage << "\n";
    }

    // Store raw response body in a variable if requested
    if (node->attributes.count("store")) {
        variables[node->attributes.at("store")] = resp.body;
    } else if (resp.success) {
        // Print to console if no store target
        std::cout << "    📥 [FETCH] Response:\n" << resp.body << "\n";
    }
}