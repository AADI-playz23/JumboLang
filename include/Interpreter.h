// include/Interpreter.h
#ifndef JUMBOLANG_INTERPRETER_H
#define JUMBOLANG_INTERPRETER_H

#include "Parser.h"
#include <map>
#include <functional>
#include <memory>

using FeatureAction = std::function<void(std::shared_ptr<ASTNode>)>;

class Interpreter {
private:
    std::map<std::string, FeatureAction> featureRegistry;
    void executeNode(std::shared_ptr<ASTNode> node);

public:
    Interpreter();
    void run(std::shared_ptr<ASTNode> rootNode);

    void handleMain(std::shared_ptr<ASTNode> node);
    void handleHttps(std::shared_ptr<ASTNode> node);
    void handleLlm(std::shared_ptr<ASTNode> node);
    void handleFile(std::shared_ptr<ASTNode> node); 
    void handleDb(std::shared_ptr<ASTNode> node);
    void handleJson(std::shared_ptr<ASTNode> node); // <--- NEW CONTRACT
};

#endif