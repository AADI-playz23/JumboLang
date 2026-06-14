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

// Control-flow signal bubbled up through executeNode/executeBlock.
enum class FlowSignal { NONE, BREAK, CONTINUE, RETURN };

class Interpreter {
private:
    std::unordered_map<std::string, FeatureAction> featureRegistry;

    // Scope stack: back() is the current (innermost) scope. Globals = front().
    std::vector<std::unordered_map<std::string, std::string>> scopes;

    // User-defined functions: name -> {paramNames, bodyNode}
    std::unordered_map<std::string, std::pair<std::vector<std::string>, std::shared_ptr<ASTNode>>> functions;

    bool lastIfCondition = false;
    FlowSignal flow = FlowSignal::NONE;
    std::string returnValue;

    // Web server state (shared across route handlers inside one request)
    std::string activeRoutePath    = "";
    std::string activeRouteMethod  = "";
    std::string currentHttpResponse = "";

    // Core dispatch
    void executeNode(const std::shared_ptr<ASTNode>& node);
    // Execute a list of children, stopping early on any FlowSignal != NONE
    void executeBlock(const std::vector<std::shared_ptr<ASTNode>>& children);

    // ── Variable scoping helpers ─────────────────────────────────────────────
    std::string  getVar(const std::string& name) const;
    void         setVar(const std::string& name, const std::string& value);
    bool         hasVar(const std::string& name) const;

    // ── Array helpers (arrays stored as \x01-joined strings) ────────────────
    static std::vector<std::string> arrToVec(const std::string& s);
    static std::string vecToArr(const std::vector<std::string>& v);

    // ── Condition evaluator: supports ==,!=,<,>,<=,>=  and &&/|| chaining ───
    bool evalCondition(const std::shared_ptr<ASTNode>& node);
    bool evalSingleCond(const std::string& lhs, const std::string& op, const std::string& rhs);

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

    // ── V3.0 — basics: loops, functions, arrays ──────────────────────────────
    void handleFor    (const std::shared_ptr<ASTNode>& node); // {for var="i" from="0" to="10" [step="1"]}
    void handleWhile  (const std::shared_ptr<ASTNode>& node); // {while ...cond attrs...}
    void handleBreak  (const std::shared_ptr<ASTNode>& node);
    void handleContinue(const std::shared_ptr<ASTNode>& node);

    void handleFunc   (const std::shared_ptr<ASTNode>& node); // {func name="add" params="a,b"}
    void handleReturn (const std::shared_ptr<ASTNode>& node);
    void handleCall   (const std::shared_ptr<ASTNode>& node); // {call name="add" args="1,2" store="r"}

    // Array ops on \x01-joined string values
    void handleArray  (const std::shared_ptr<ASTNode>& node); // {array} name = item1,item2,... {-array}
    void handlePush   (const std::shared_ptr<ASTNode>& node); // {push var="list"}value{-push}
    void handleGet    (const std::shared_ptr<ASTNode>& node); // {get var="list" index="0" store="x"}
    void handleLen    (const std::shared_ptr<ASTNode>& node); // {len var="list" store="n"}
};

#endif // JUMBOLANG_INTERPRETER_H