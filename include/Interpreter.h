// include/Interpreter.h
// The JumboLang Virtual Machine.
// All tag handler methods take `const std::shared_ptr<ASTNode>&` (const ref)
// to avoid unnecessary shared_ptr reference count modifications on every call.
#ifndef JUMBOLANG_INTERPRETER_H
#define JUMBOLANG_INTERPRETER_H

#include "Parser.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

// Handler signature: const ref avoids touching the shared_ptr refcount
using FeatureAction = std::function<void(const std::shared_ptr<ASTNode>&)>;

class Interpreter {
private:
    std::unordered_map<std::string, FeatureAction> featureRegistry;
    std::unordered_map<std::string, std::string>   variables;
    bool lastIfCondition = false;

    // Web server state (shared across route handlers inside one request)
    std::string activeRoutePath    = "";
    std::string activeRouteMethod  = "";
    std::string currentHttpResponse = "";

    // Core dispatch
    void executeNode(const std::shared_ptr<ASTNode>& node);

    // ── Security helpers ──────────────────────────────────────────────────────
    // Returns true if the shell command is safe to pass to system()
    static bool isSafeShellCommand(const std::string& cmd);

public:
    Interpreter();
    void run(std::shared_ptr<ASTNode> rootNode);

    // ── V1.0 handlers ─────────────────────────────────────────────────────────
    void handleMain  (const std::shared_ptr<ASTNode>& node);
    void handleLlm   (const std::shared_ptr<ASTNode>& node);
    void handleFile  (const std::shared_ptr<ASTNode>& node);
    void handleDb    (const std::shared_ptr<ASTNode>& node);
    void handleJson  (const std::shared_ptr<ASTNode>& node);

    // ── V1.1 state / logic handlers ───────────────────────────────────────────
    void handleVar   (const std::shared_ptr<ASTNode>& node);
    void handlePrint (const std::shared_ptr<ASTNode>& node);
    void handleShell (const std::shared_ptr<ASTNode>& node);
    void handleIf    (const std::shared_ptr<ASTNode>& node);
    void handleElse  (const std::shared_ptr<ASTNode>& node);

    // ── Web framework tags ────────────────────────────────────────────────────
    void handleHttps (const std::shared_ptr<ASTNode>& node);
    void handleRoute (const std::shared_ptr<ASTNode>& node);
    void handleRes   (const std::shared_ptr<ASTNode>& node);
    void handleTunnel(const std::shared_ptr<ASTNode>& node);

    // ── V2.0 new tags ─────────────────────────────────────────────────────────
    // {fetch}: plain HTTP API call (not AI) — GET, POST, PUT, DELETE, PATCH
    void handleFetch (const std::shared_ptr<ASTNode>& node);
};

#endif // JUMBOLANG_INTERPRETER_H