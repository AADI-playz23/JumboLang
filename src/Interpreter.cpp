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
    std::function<std::string(const std::string&)> getVar;

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
            std::string v = getVar(varName);
            if (!v.empty()) {
                try { result = std::stod(v); } catch (...) { result = 0; }
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
    MathEvaluator(std::string e, std::function<std::string(const std::string&)> getter)
        : expr(std::move(e)), getVar(std::move(getter)) {}

    std::string evaluate() {
        bool hasMath = false;
        for (char c : expr)
            if (c == '+' || c == '-' || c == '*' || c == '/' ||
                c == '%' || c == '(' || c == ')') { hasMath = true; break; }

        if (!hasMath) {
            std::string t = expr;
            trimWhitespace(t);
            std::string v = getVar(t);
            if (!v.empty() || t.empty()) return v.empty() ? t : v;
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
                               std::function<std::string(const std::string&)> getVar) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '$' && i + 1 < text.size() && isalpha(text[i + 1])) {
            std::string varName;
            i++;
            while (i < text.size() && (isalnum(text[i]) || text[i] == '_'))
                varName += text[i++];
            i--;
            std::string v = getVar(varName);
            result += !v.empty() ? v : ("$" + varName);
        } else {
            result += text[i];
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ARRAY ENCODING — arrays are stored as ordinary string variables joined by
// the \x01 control byte (never appears in normal text), so no new value type
// is needed and existing string-based vars/interpolation keep working.
// ─────────────────────────────────────────────────────────────────────────────
static const char ARR_SEP = '\x01';

std::vector<std::string> Interpreter::arrToVec(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ARR_SEP) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

std::string Interpreter::vecToArr(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ARR_SEP;
        out += v[i];
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// SCOPED VARIABLES — scopes.back() is current; lookups fall back to globals
// (scopes.front()) so functions can read outer state but writes inside a
// function go to the local scope unless the name already exists globally.
// ─────────────────────────────────────────────────────────────────────────────
bool Interpreter::hasVar(const std::string& name) const {
    if (scopes.back().count(name)) return true;
    return scopes.size() > 1 && scopes.front().count(name);
}

std::string Interpreter::getVar(const std::string& name) const {
    auto& local = scopes.back();
    auto it = local.find(name);
    if (it != local.end()) return it->second;
    if (scopes.size() > 1) {
        auto git = scopes.front().find(name);
        if (git != scopes.front().end()) return git->second;
    }
    return "";
}

void Interpreter::setVar(const std::string& name, const std::string& value) {
    // If we're in a function scope and the name exists globally (but not
    // locally), update the global so top-level state stays mutable.
    if (scopes.size() > 1 && !scopes.back().count(name) && scopes.front().count(name)) {
        scopes.front()[name] = value;
        return;
    }
    scopes.back()[name] = value;
}

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
    scopes.emplace_back(); // global scope

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

    // V3.0: loops, functions, arrays
    featureRegistry["for"]      = [this](const auto& n) { handleFor(n);      };
    featureRegistry["while"]    = [this](const auto& n) { handleWhile(n);    };
    featureRegistry["break"]    = [this](const auto& n) { handleBreak(n);    };
    featureRegistry["continue"] = [this](const auto& n) { handleContinue(n); };
    featureRegistry["func"]     = [this](const auto& n) { handleFunc(n);     };
    featureRegistry["return"]   = [this](const auto& n) { handleReturn(n);   };
    featureRegistry["call"]     = [this](const auto& n) { handleCall(n);     };
    featureRegistry["array"]    = [this](const auto& n) { handleArray(n);    };
    featureRegistry["push"]     = [this](const auto& n) { handlePush(n);     };
    featureRegistry["get"]      = [this](const auto& n) { handleGet(n);      };
    featureRegistry["len"]      = [this](const auto& n) { handleLen(n);      };
}

// ─────────────────────────────────────────────────────────────────────────────
// CORE DISPATCH
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::executeNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    auto it = featureRegistry.find(node->tagName);
    if (it != featureRegistry.end()) {
        it->second(node);
    } else if (functions.count(node->tagName)) {
        // Allow calling a user function directly as {funcname arg1="..." ...}
        handleCall(node);
    } else {
        std::cerr << "⚠️  JumboLang Warning: Unknown tag {" << node->tagName << "}\n";
    }
}

// Execute children in order, stopping early if a break/continue/return signal
// is raised by a nested node (the signal propagates further up to the loop
// or function call that handles it).
void Interpreter::executeBlock(const std::vector<std::shared_ptr<ASTNode>>& children) {
    for (const auto& child : children) {
        executeNode(child);
        if (flow != FlowSignal::NONE) return;
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
    executeBlock(node->children);
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
    if (node->attributes.count("envkey")) {
        ai.loadApiKeyFromEnv(node->attributes.at("envkey"));
    }
    
    // Direct key passing
    if (node->attributes.count("key")) {
        ai.setApiKey(node->attributes.at("key"));
    } else if (node->attributes.count("apikey")) {
        ai.setApiKey(node->attributes.at("apikey"));
    }

    if (!node->bodyContent.empty()) {
        std::string response = ai.generateResponse(node->bodyContent);

        // Optionally store the response in a JumboLang variable
        if (node->attributes.count("store")) {
            setVar(node->attributes.at("store"), response);
        } else {
            std::cout << "    ✨ [AI] " << response << "\n";
        }
    }

    executeBlock(node->children);
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
    MathEvaluator eval(varValue, [this](const std::string& n){ return getVar(n); });
    setVar(varName, eval.evaluate());
}

void Interpreter::handlePrint(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, [this](const std::string& n){ return getVar(n); });
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
// ─────────────────────────────────────────────────────────────────────────────
// CONDITION EVALUATION
// Supported forms on a tag (e.g. {if ...} / {while ...}):
//   var="x" equals="5"                → x == 5            (legacy, kept for compat)
//   var="x" op="<" value="5"          → x < 5
//   var="x" op="<" value="5" and_var="y" and_op=">" and_value="0"  → AND
//   ...and_* may be replaced by or_* for OR-chaining (only one chain per tag)
// op supports: == != < > <= >=  (numeric if both sides parse as numbers, else string compare)
// ─────────────────────────────────────────────────────────────────────────────
bool Interpreter::evalSingleCond(const std::string& lhs, const std::string& op, const std::string& rhs) {
    // Try numeric comparison first
    try {
        size_t p1, p2;
        double a = std::stod(lhs, &p1);
        double b = std::stod(rhs, &p2);
        if (p1 == lhs.size() && p2 == rhs.size()) {
            if (op == "==") return a == b;
            if (op == "!=") return a != b;
            if (op == "<")  return a < b;
            if (op == ">")  return a > b;
            if (op == "<=") return a <= b;
            if (op == ">=") return a >= b;
        }
    } catch (...) { /* fall through to string compare */ }

    if (op == "==") return lhs == rhs;
    if (op == "!=") return lhs != rhs;
    if (op == "<")  return lhs < rhs;
    if (op == ">")  return lhs > rhs;
    if (op == "<=") return lhs <= rhs;
    if (op == ">=") return lhs >= rhs;
    return false;
}

bool Interpreter::evalCondition(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;

    // Legacy form: var="x" equals="5"  → equivalent to op "=="
    std::string op = a.count("op") ? a.at("op") : "==";
    std::string lhs, rhs;
    bool have = false;

    if (a.count("var")) {
        lhs = getVar(a.at("var"));
        rhs = a.count("equals") ? a.at("equals") : (a.count("value") ? a.at("value") : "");
        have = true;
    }
    if (!have) return false;

    bool result = evalSingleCond(lhs, op, rhs);

    // Optional AND chain
    if (a.count("and_var")) {
        std::string l2 = getVar(a.at("and_var"));
        std::string op2 = a.count("and_op") ? a.at("and_op") : "==";
        std::string r2 = a.count("and_value") ? a.at("and_value") : "";
        result = result && evalSingleCond(l2, op2, r2);
    }
    // Optional OR chain
    if (a.count("or_var")) {
        std::string l2 = getVar(a.at("or_var"));
        std::string op2 = a.count("or_op") ? a.at("or_op") : "==";
        std::string r2 = a.count("or_value") ? a.at("or_value") : "";
        result = result || evalSingleCond(l2, op2, r2);
    }
    return result;
}

void Interpreter::handleIf(const std::shared_ptr<ASTNode>& node) {
    lastIfCondition = evalCondition(node);
    if (lastIfCondition) executeBlock(node->children);
}

void Interpreter::handleElse(const std::shared_ptr<ASTNode>& node) {
    if (!lastIfCondition) executeBlock(node->children);
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
            this->flow = FlowSignal::NONE;

            this->executeBlock(node->children);
            this->flow = FlowSignal::NONE; // don't leak signals out of a request

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
        executeBlock(node->children);
    }
}

void Interpreter::handleRes(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, [this](const std::string& n){ return getVar(n); });
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
    url = interpolate(url, [this](const std::string& n){ return getVar(n); });

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
    body = interpolate(body, [this](const std::string& n){ return getVar(n); });

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
        setVar(node->attributes.at("store"), resp.body);
    } else if (resp.success) {
        // Print to console if no store target
        std::cout << "    📥 [FETCH] Response:\n" << resp.body << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// V3.0 — LOOPS
// {for var="i" from="0" to="10" step="1"} ... {-for}   (inclusive bounds, step default 1)
// {while ...condition attrs (see evalCondition)...} ... {-while}
// {break}{-break}  {continue}{-continue}
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleFor(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    std::string varName = a.count("var") ? a.at("var") : "i";
    double from = a.count("from") ? std::stod(a.at("from")) : 0;
    double to   = a.count("to")   ? std::stod(a.at("to"))   : 0;
    double step = a.count("step") ? std::stod(a.at("step")) : 1;
    if (step == 0) step = 1;

    for (double v = from; (step > 0) ? (v <= to) : (v >= to); v += step) {
        std::string s = std::to_string(v);
        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
        if (!s.empty() && s.back() == '.') s.pop_back();
        setVar(varName, s);

        executeBlock(node->children);

        if (flow == FlowSignal::BREAK)    { flow = FlowSignal::NONE; break; }
        if (flow == FlowSignal::CONTINUE) { flow = FlowSignal::NONE; continue; }
        if (flow == FlowSignal::RETURN)   return; // propagate to enclosing function
    }
}

void Interpreter::handleWhile(const std::shared_ptr<ASTNode>& node) {
    const int MAX_ITER = 1000000; // safety guard against infinite loops
    int iter = 0;
    while (evalCondition(node)) {
        if (++iter > MAX_ITER) {
            std::cerr << "    ⚠️  [WHILE] Aborted after " << MAX_ITER << " iterations (possible infinite loop).\n";
            break;
        }
        executeBlock(node->children);

        if (flow == FlowSignal::BREAK)    { flow = FlowSignal::NONE; break; }
        if (flow == FlowSignal::CONTINUE) { flow = FlowSignal::NONE; continue; }
        if (flow == FlowSignal::RETURN)   return;
    }
}

void Interpreter::handleBreak(const std::shared_ptr<ASTNode>&)    { flow = FlowSignal::BREAK; }
void Interpreter::handleContinue(const std::shared_ptr<ASTNode>&) { flow = FlowSignal::CONTINUE; }

// ─────────────────────────────────────────────────────────────────────────────
// V3.0 — FUNCTIONS
// {func name="add" params="a,b"} ... {return}a + b{-return} ... {-func}
// {return}expr{-return}
// {call name="add" args="1,2" store="result"}{-call}
// Functions get a fresh local scope; globals remain readable/writable via
// getVar/setVar's fallback rules.
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleFunc(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    if (!a.count("name")) {
        std::cerr << "    ⚠️  [FUNC] {func} requires a 'name' attribute.\n";
        return;
    }
    std::vector<std::string> params;
    if (a.count("params")) {
        std::stringstream ss(a.at("params"));
        std::string p;
        while (std::getline(ss, p, ',')) { trimWhitespace(p); if (!p.empty()) params.push_back(p); }
    }
    functions[a.at("name")] = { params, node };
}

void Interpreter::handleReturn(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, [this](const std::string& n){ return getVar(n); });
    MathEvaluator eval(content, [this](const std::string& n){ return getVar(n); });
    returnValue = eval.evaluate();
    flow = FlowSignal::RETURN;
}

void Interpreter::handleCall(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    std::string name = a.count("name") ? a.at("name") : node->tagName;

    auto it = functions.find(name);
    if (it == functions.end()) {
        std::cerr << "    ⚠️  [CALL] Unknown function '" << name << "'.\n";
        return;
    }
    const auto& [params, funcNode] = it->second;

    // Evaluate args in the CALLER's scope before pushing the new one.
    std::vector<std::string> argVals;
    if (a.count("args")) {
        std::stringstream ss(a.at("args"));
        std::string arg;
        while (std::getline(ss, arg, ',')) {
            trimWhitespace(arg);
            arg = interpolate(arg, [this](const std::string& n){ return getVar(n); });
            MathEvaluator eval(arg, [this](const std::string& n){ return getVar(n); });
            argVals.push_back(eval.evaluate());
        }
    }

    // New local scope for the function body
    scopes.emplace_back();
    for (size_t i = 0; i < params.size(); ++i)
        scopes.back()[params[i]] = (i < argVals.size()) ? argVals[i] : "";

    returnValue.clear();
    executeBlock(funcNode->children);
    if (flow == FlowSignal::RETURN) flow = FlowSignal::NONE; // return stops at the function boundary

    scopes.pop_back();

    if (a.count("store")) setVar(a.at("store"), returnValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// V3.0 — ARRAYS (stored as \x01-joined strings in regular variables)
// {array} name = item1, item2, item3 {-array}
// {push var="list"}value{-push}
// {get var="list" index="0" store="x"}{-get}
// {len var="list" store="n"}{-len}
// ─────────────────────────────────────────────────────────────────────────────
void Interpreter::handleArray(const std::shared_ptr<ASTNode>& node) {
    std::string content = node->bodyContent;
    size_t eqPos = content.find('=');
    if (eqPos == std::string::npos) {
        std::cerr << "    ⚠️  [SYNTAX] {array} expects 'name = item1, item2, ...'\n";
        return;
    }
    std::string name = content.substr(0, eqPos);
    trimWhitespace(name);

    std::vector<std::string> items;
    std::stringstream ss(content.substr(eqPos + 1));
    std::string item;
    while (std::getline(ss, item, ',')) {
        trimWhitespace(item);
        item = interpolate(item, [this](const std::string& n){ return getVar(n); });
        items.push_back(item);
    }
    setVar(name, vecToArr(items));
}

void Interpreter::handlePush(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    if (!a.count("var")) { std::cerr << "    ⚠️  [PUSH] {push} requires a 'var' attribute.\n"; return; }
    std::string value = node->bodyContent;
    trimWhitespace(value);
    value = interpolate(value, [this](const std::string& n){ return getVar(n); });

    std::string current = getVar(a.at("var"));
    auto items = current.empty() ? std::vector<std::string>{} : arrToVec(current);
    items.push_back(value);
    setVar(a.at("var"), vecToArr(items));
}

void Interpreter::handleGet(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    if (!a.count("var") || !a.count("index")) {
        std::cerr << "    ⚠️  [GET] {get} requires 'var' and 'index' attributes.\n";
        return;
    }
    auto items = arrToVec(getVar(a.at("var")));
    std::string idxExpr = interpolate(a.at("index"), [this](const std::string& n){ return getVar(n); });
    MathEvaluator idxEval(idxExpr, [this](const std::string& n){ return getVar(n); });
    int idx = 0;
    try { idx = (int)std::stod(idxEval.evaluate()); } catch (...) { idx = 0; }
    std::string result = (idx >= 0 && idx < (int)items.size()) ? items[idx] : "";

    if (a.count("store")) setVar(a.at("store"), result);
    else std::cout << "    📋 [GET] " << result << "\n";
}

void Interpreter::handleLen(const std::shared_ptr<ASTNode>& node) {
    const auto& a = node->attributes;
    if (!a.count("var")) { std::cerr << "    ⚠️  [LEN] {len} requires a 'var' attribute.\n"; return; }
    std::string val = getVar(a.at("var"));
    int n = val.empty() ? 0 : (int)arrToVec(val).size();

    if (a.count("store")) setVar(a.at("store"), std::to_string(n));
    else std::cout << "    📏 [LEN] " << n << "\n";
}
