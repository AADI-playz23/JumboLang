#include "../include/Interpreter.h"
#include "../include/features/Network.h"    
#include "../include/features/AI.h"         
#include "../include/features/FileSystem.h" 
#include "../include/features/Database.h"
#include "../include/features/JSON.h" 
#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>

// --- 🧠 V1.1 SMART HELPERS ---

auto trimWhitespace = [](std::string& s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
};

// 🧮 Recursive Descent Math Parser (Handles PEMDAS, Decimals, Parentheses)
class MathEvaluator {
    std::string expr;
    size_t pos = 0;
    std::unordered_map<std::string, std::string>& vars;

    double parseFactor() {
        while (pos < expr.length() && isspace(expr[pos])) pos++;
        if (pos >= expr.length()) return 0;

        double sign = 1;
        if (expr[pos] == '-') { sign = -1; pos++; }
        else if (expr[pos] == '+') { pos++; }

        while (pos < expr.length() && isspace(expr[pos])) pos++;

        double result = 0;
        if (expr[pos] == '(') {
            pos++;
            result = parseExpression();
            if (pos < expr.length() && expr[pos] == ')') pos++;
        } else if (isalpha(expr[pos]) || expr[pos] == '_') {
            std::string varName = "";
            while (pos < expr.length() && (isalnum(expr[pos]) || expr[pos] == '_')) varName += expr[pos++];
            if (vars.count(varName)) {
                try { result = std::stod(vars[varName]); } catch (...) { result = 0; }
            }
        } else {
            std::string numStr = "";
            while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) numStr += expr[pos++];
            try { result = std::stod(numStr); } catch (...) { result = 0; }
        }
        return sign * result;
    }

    double parseTerm() {
        double result = parseFactor();
        while (pos < expr.length()) {
            while (pos < expr.length() && isspace(expr[pos])) pos++;
            if (pos >= expr.length()) break;
            char op = expr[pos];
            if (op != '*' && op != '/' && op != '%') break;
            pos++;
            double nextFactor = parseFactor();
            if (op == '*') result *= nextFactor;
            else if (op == '/') result = (nextFactor != 0) ? result / nextFactor : 0;
            else if (op == '%') result = std::fmod(result, nextFactor);
        }
        return result;
    }

    double parseExpression() {
        double result = parseTerm();
        while (pos < expr.length()) {
            while (pos < expr.length() && isspace(expr[pos])) pos++;
            if (pos >= expr.length()) break;
            char op = expr[pos];
            if (op != '+' && op != '-') break;
            pos++;
            double nextTerm = parseTerm();
            if (op == '+') result += nextTerm;
            else if (op == '-') result -= nextTerm;
        }
        return result;
    }

public:
    MathEvaluator(std::string e, std::unordered_map<std::string, std::string>& v) : expr(e), vars(v) {}
    
    std::string evaluate() {
        bool hasMathSymbols = false;
        for (char c : expr) {
            if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '(' || c == ')') {
                hasMathSymbols = true; break;
            }
        }
        if (!hasMathSymbols) {
            std::string t = expr;
            trimWhitespace(t);
            if (vars.count(t)) return vars[t];
            return t;
        }

        try {
            double val = parseExpression();
            std::string res = std::to_string(val);
            res.erase(res.find_last_not_of('0') + 1, std::string::npos);
            if (res.back() == '.') res.pop_back();
            return res;
        } catch (...) { return expr; }
    }
};

// String Interpolator (Injects $variables into text)
std::string interpolate(std::string text, std::unordered_map<std::string, std::string>& vars) {
    std::string result = "";
    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == '$' && i + 1 < text.length() && isalpha(text[i+1])) {
            std::string varName = "";
            i++;
            while (i < text.length() && (isalnum(text[i]) || text[i] == '_')) varName += text[i++];
            i--; 
            if (vars.count(varName)) result += vars[varName];
            else result += "$" + varName; 
        } else {
            result += text[i];
        }
    }
    return result;
}

// --- CORE INTERPRETER CLASS ---

Interpreter::Interpreter() {
    featureRegistry["main"]   = [this](auto node) { this->handleMain(node); };
    featureRegistry["llm"]    = [this](auto node) { this->handleLlm(node); };
    featureRegistry["file"]   = [this](auto node) { this->handleFile(node); };
    featureRegistry["db"]     = [this](auto node) { this->handleDb(node); };
    featureRegistry["json"]   = [this](auto node) { this->handleJson(node); };
    
    featureRegistry["var"]    = [this](auto node) { this->handleVar(node); };
    featureRegistry["print"]  = [this](auto node) { this->handlePrint(node); };
    featureRegistry["shell"]  = [this](auto node) { this->handleShell(node); };

    featureRegistry["if"]     = [this](auto node) { this->handleIf(node); };
    featureRegistry["else"]   = [this](auto node) { this->handleElse(node); };

    // 🌐 Web Framework Tags
    featureRegistry["https"]  = [this](auto node) { this->handleHttps(node); };
    featureRegistry["route"]  = [this](auto node) { this->handleRoute(node); };
    featureRegistry["res"]    = [this](auto node) { this->handleRes(node); };
    featureRegistry["tunnel"] = [this](auto node) { this->handleTunnel(node); };
}

void Interpreter::executeNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    auto it = featureRegistry.find(node->tagName);
    if (it != featureRegistry.end()) it->second(node);
    else std::cerr << "⚠️ JumboLang Engine Warning: Unknown tag {" << node->tagName << "}\n";
}

void Interpreter::run(std::shared_ptr<ASTNode> rootNode) {
    std::cout << "--- 🐘 JUMBOLANG VIRTUAL MACHINE STARTING ---\n";
    executeNode(rootNode);
    std::cout << "--- 🐘 EXECUTION FINISHED SUCCESSFULLY ---\n";
}

// --- V1.0 HANDLERS ---
void Interpreter::handleMain(std::shared_ptr<ASTNode> node) {
    for (auto& child : node->children) executeNode(child);
}

void Interpreter::handleLlm(std::shared_ptr<ASTNode> node) {
    std::string model = "gemini-1.5-flash";
    if (node->attributes.count("model")) model = node->attributes["model"];
    AIManager ai(model);
    if (!node->bodyContent.empty()) std::cout << "    ✨ [AI] " << ai.generateResponse(node->bodyContent) << "\n";
    for (auto& child : node->children) executeNode(child);
}

void Interpreter::handleFile(std::shared_ptr<ASTNode> node) {
    std::string path = node->attributes["path"];
    if (node->attributes["action"] == "write") FileSystem::write(path, node->bodyContent);
    else std::cout << "    📖 [FILE] " << FileSystem::read(path) << "\n";
}

void Interpreter::handleDb(std::shared_ptr<ASTNode> node) {
    std::string key = node->attributes["key"];
    DatabaseManager db;
    if (node->attributes["action"] == "set") db.set(key, node->bodyContent);
    else std::cout << "    🗄️ [DB] " << key << " => " << db.get(key) << "\n";
}

void Interpreter::handleJson(std::shared_ptr<ASTNode> node) {
    auto data = JSONManager::parse(node->bodyContent);
    for (auto const& [key, val] : data) std::cout << "    📊 " << key << " : " << val << "\n";
}

// --- 🚀 V1.1 ADVANCED STATE HANDLERS ---
void Interpreter::handleVar(std::shared_ptr<ASTNode> node) {
    std::string content = node->bodyContent;
    size_t equalsPos = content.find('='); 

    if (equalsPos != std::string::npos) {
        std::string varName = content.substr(0, equalsPos);
        std::string varValue = content.substr(equalsPos + 1);

        trimWhitespace(varName);
        MathEvaluator evaluator(varValue, variables);
        variables[varName] = evaluator.evaluate();
    } else {
        std::cerr << "    ⚠️ [SYNTAX ERROR] {var} expects 'name = value'\n";
    }
}

void Interpreter::handlePrint(std::shared_ptr<ASTNode> node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    content = interpolate(content, variables);
    std::cout << "    🖨️ [PRINT] " << content << "\n";
}

void Interpreter::handleShell(std::shared_ptr<ASTNode> node) {
    std::cout << "    🖥️ [OS SHELL] Executing: " << node->bodyContent << "\n";
    if (system(node->bodyContent.c_str()) != 0) std::cout << "    ❌ [OS SHELL] Failed.\n";
}

// --- 🔀 LOGIC HANDLERS ---
void Interpreter::handleIf(std::shared_ptr<ASTNode> node) {
    lastIfCondition = false;
    if (node->attributes.count("var") && node->attributes.count("equals")) {
        std::string varName = node->attributes["var"];
        if (variables.count(varName) && variables[varName] == node->attributes["equals"]) {
            lastIfCondition = true;
        }
    }
    if (lastIfCondition) { for (auto& child : node->children) executeNode(child); }
}

void Interpreter::handleElse(std::shared_ptr<ASTNode> node) {
    if (!lastIfCondition) { for (auto& child : node->children) executeNode(child); }
}

// --- 🌐 WEB SERVER FRAMEWORK ---
void Interpreter::handleHttps(std::shared_ptr<ASTNode> node) {
    int portNum = 8080; 
    if (node->attributes.count("port")) portNum = std::stoi(node->attributes["port"]);
    
    NetworkManager net(portNum);
    if (net.initializeSocket() && net.bindToHardware()) {
        
        // This creates the continuous Event Loop!
        net.listenAndServe([this, node](std::string path) -> std::string {
            this->activeRoutePath = path;
            this->currentHttpResponse = ""; // Reset response for the new user
            
            // Execute all tags inside {https}. Only matching {route} tags will trigger.
            for (auto& child : node->children) {
                this->executeNode(child); 
            }

            if (this->currentHttpResponse.empty()) {
                return "{\"error\": \"404 Route Not Found in JumboLang\"}";
            }
            return this->currentHttpResponse;
        });
    }
    net.shutdown();
}

void Interpreter::handleRoute(std::shared_ptr<ASTNode> node) {
    // Only execute the code inside this route if the URL matches!
    if (node->attributes.count("path") && node->attributes["path"] == activeRoutePath) {
        for (auto& child : node->children) {
            executeNode(child);
        }
    }
}

void Interpreter::handleRes(std::shared_ptr<ASTNode> node) {
    std::string content = node->bodyContent;
    trimWhitespace(content);
    // Support dynamic variables inside the web response!
    content = interpolate(content, variables);
    this->currentHttpResponse += content;
}

void Interpreter::handleTunnel(std::shared_ptr<ASTNode> node) {
    std::string port = "8080";
    if (node->attributes.count("port")) port = node->attributes["port"];
    
    std::cout << "    🚇 [TUNNEL] Spawning secure public URL for port " << port << "...\n";
    // Uses Node's localtunnel in the background silently
    std::string cmd = "npx localtunnel --port " + port + " > tunnel.log 2>&1 &";
    system(cmd.c_str());
    std::cout << "    🔗 [TUNNEL] Tunnel active. Check tunnel.log for your URL.\n";
}