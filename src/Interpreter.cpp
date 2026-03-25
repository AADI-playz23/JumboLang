#include "../include/Interpreter.h"
#include "../include/features/Network.h"    
#include "../include/features/AI.h"         
#include "../include/features/FileSystem.h" 
#include "../include/features/Database.h"
#include "../include/features/JSON.h" 
#include <iostream>
#include <string>

Interpreter::Interpreter() {
    featureRegistry["main"]  = [this](auto node) { this->handleMain(node); };
    featureRegistry["https"] = [this](auto node) { this->handleHttps(node); };
    featureRegistry["llm"]   = [this](auto node) { this->handleLlm(node); };
    featureRegistry["file"]  = [this](auto node) { this->handleFile(node); };
    featureRegistry["db"]    = [this](auto node) { this->handleDb(node); };
    featureRegistry["json"]  = [this](auto node) { this->handleJson(node); };
}

// OPTIMIZED: Constant time lookup with no double-search
void Interpreter::executeNode(const std::shared_ptr<ASTNode>& node) {
    if (!node) return;
    auto it = featureRegistry.find(node->tagName);
    if (it != featureRegistry.end()) {
        it->second(node);
    } else {
        std::cerr << "⚠️ JumboLang Engine Warning: Unknown tag {" << node->tagName << "}\n";
    }
}

void Interpreter::run(std::shared_ptr<ASTNode> rootNode) {
    std::cout << "--- 🐘 JUMBOLANG VIRTUAL MACHINE STARTING ---\n";
    executeNode(rootNode);
    std::cout << "--- 🐘 EXECUTION FINISHED SUCCESSFULLY ---\n";
}

void Interpreter::handleMain(std::shared_ptr<ASTNode> node) {
    std::cout << "[SYSTEM] Initializing Main App Space...\n";
    for (auto& child : node->children) { executeNode(child); }
}

void Interpreter::handleHttps(std::shared_ptr<ASTNode> node) {
    int portNum = 8080; 
    if (node->attributes.count("port")) {
        try { portNum = std::stoi(node->attributes["port"]); } catch (...) {}
    }
    std::cout << "[NETWORK] Opening Hardware Port: " << portNum << "\n";
    NetworkManager net(portNum);
    if (net.initializeSocket() && net.bindToHardware()) {
        net.startListening();
        for (auto& child : node->children) { executeNode(child); }
    }
    net.shutdown();
}

void Interpreter::handleLlm(std::shared_ptr<ASTNode> node) {
    std::string model = "gemini-1.5-flash";
    if (node->attributes.count("model")) model = node->attributes["model"];
    std::cout << "[AI ENGINE] Routing context to " << model << "...\n";
    AIManager ai(model);
    if (!node->bodyContent.empty()) {
        std::string response = ai.generateResponse(node->bodyContent);
        std::cout << "    ✨ [AI RESPONSE] " << response << "\n";
    }
    for (auto& child : node->children) { executeNode(child); }
}

void Interpreter::handleFile(std::shared_ptr<ASTNode> node) {
    std::string path = "output.txt";
    if (node->attributes.count("path")) path = node->attributes["path"];
    std::string action = "write";
    if (node->attributes.count("action")) action = node->attributes["action"];
    if (action == "write") {
        FileSystem::write(path, node->bodyContent);
    } else {
        std::string data = FileSystem::read(path);
        std::cout << "    📖 [FILE READ] " << data << "\n";
    }
}



void Interpreter::handleDb(std::shared_ptr<ASTNode> node) {
    std::string action = "get";
    if (node->attributes.count("action")) action = node->attributes["action"];
    std::string key = "none";
    if (node->attributes.count("key")) key = node->attributes["key"];
    DatabaseManager db;
    if (action == "set") {
        db.set(key, node->bodyContent);
    } else {
        std::string val = db.get(key);
        std::cout << "    🗄️ [DB GET] " << key << " => " << val << "\n";
    }
}

void Interpreter::handleJson(std::shared_ptr<ASTNode> node) {
    std::cout << "[JSON ENGINE] Parsing raw data...\n";
    auto data = JSONManager::parse(node->bodyContent);
    for (auto const& [key, val] : data) {
        std::cout << "    📊 [EXTRACTED] " << key << " : " << val << "\n";
    }
}