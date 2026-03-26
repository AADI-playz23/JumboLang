#ifndef JUMBOLANG_INTERPRETER_H
#define JUMBOLANG_INTERPRETER_H

#include "Parser.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdlib>

using FeatureAction = std::function<void(std::shared_ptr<ASTNode>)>;

class Interpreter {
private:
    std::unordered_map<std::string, FeatureAction> featureRegistry;
    std::unordered_map<std::string, std::string> variables; 
    bool lastIfCondition = false; 
    
    // 🌐 NEW: Web Server State Memory
    std::string activeRoutePath = "";
    std::string currentHttpResponse = "";
    
    void executeNode(const std::shared_ptr<ASTNode>& node);

public:
    Interpreter();
    void run(std::shared_ptr<ASTNode> rootNode);

    void handleMain(std::shared_ptr<ASTNode> node);
    void handleLlm(std::shared_ptr<ASTNode> node);
    void handleFile(std::shared_ptr<ASTNode> node); 
    void handleDb(std::shared_ptr<ASTNode> node);
    void handleJson(std::shared_ptr<ASTNode> node);

    void handleVar(std::shared_ptr<ASTNode> node);
    void handlePrint(std::shared_ptr<ASTNode> node);
    void handleShell(std::shared_ptr<ASTNode> node);
    void handleIf(std::shared_ptr<ASTNode> node);
    void handleElse(std::shared_ptr<ASTNode> node);

    // 🚀 NEW WEB FRAMEWORK TAGS
    void handleHttps(std::shared_ptr<ASTNode> node); // Upgraded
    void handleRoute(std::shared_ptr<ASTNode> node);
    void handleRes(std::shared_ptr<ASTNode> node);
    void handleTunnel(std::shared_ptr<ASTNode> node);
};

#endif