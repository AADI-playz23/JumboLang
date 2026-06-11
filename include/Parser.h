// include/Parser.h
// The JumboLang Parser — builds an AST from the token stream.
// Errors are collected and reported without crashing; the parser attempts
// to synchronize and continue after each bad block.
#ifndef JUMBOLANG_PARSER_H
#define JUMBOLANG_PARSER_H

#include "Lexer.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// AST NODE — one parsed {tag} block with its attributes, body, and children
// ─────────────────────────────────────────────────────────────────────────────
struct ASTNode {
    std::string tagName;

    // e.g. {https port="8080"} → attributes["port"] = "8080"
    std::map<std::string, std::string> attributes;

    // Raw text/body written inside the tag (used by {var}, {print}, etc.)
    std::string bodyContent;

    // Nested blocks (e.g. {route} nodes inside {https})
    std::vector<std::shared_ptr<ASTNode>> children;

    // Debug printer
    void print(int indent = 0) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// PARSER ENGINE
// ─────────────────────────────────────────────────────────────────────────────
class Parser {
private:
    std::vector<Token> tokens;
    size_t             current;

    // Navigation helpers
    Token peek()     const;
    Token previous() const;
    bool  isAtEnd()  const;
    Token advance();
    bool  check(TokenType type) const;
    bool  match(TokenType type);

    // Consume the next token if it matches; otherwise record an error and return
    // a sentinel token (does NOT call exit() — returns a dummy token instead).
    Token consume(TokenType type, const std::string& message);

    // Advance past the current block's closing tag to resume after a bad block
    void synchronize();

    // Build one complete ASTNode (may return nullptr on unrecoverable error)
    std::shared_ptr<ASTNode> parseBlock();

public:
    // All parse errors are accumulated here (never causes a crash)
    std::vector<std::string> errors;

    explicit Parser(const std::vector<Token>& lexerTokens);

    // Parse the entire token stream; returns the root node (or nullptr)
    std::shared_ptr<ASTNode> parse();

    bool hasErrors()   const;
    void printErrors() const;
};

#endif // JUMBOLANG_PARSER_H