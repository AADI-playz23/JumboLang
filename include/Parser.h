// include/Parser.h
#ifndef JUMBOLANG_PARSER_H
#define JUMBOLANG_PARSER_H

#include "Lexer.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

// ---------------------------------------------------------
// 1. THE AST NODE (The Building Block)
// When the parser finds {https port=443}...{-https}, 
// it puts everything inside this data structure.
// ---------------------------------------------------------
struct ASTNode {
    std::string tagName;
    
    // A dictionary to hold attributes (e.g., ["port"] = "443")
    std::map<std::string, std::string> attributes;
    
    // The raw text written inside the tag
    std::string bodyContent;
    
    // A list of other ASTNodes that are nested inside this one!
    // We use shared_ptr for strict memory safety so the C++ engine doesn't crash.
    std::vector<std::shared_ptr<ASTNode>> children;

    // A helper function to print the 3D tree to the terminal
    void print(int indent = 0) const;
};

// ---------------------------------------------------------
// 2. THE PARSER ENGINE
// The machine that converts flat Tokens into a 3D AST.
// ---------------------------------------------------------
class Parser {
private:
    std::vector<Token> tokens;
    size_t current; // Our current position in the token list

    // Movement Helpers (Similar to the Lexer, but for Tokens instead of letters)
    Token peek() const;
    Token previous() const;
    bool isAtEnd() const;
    Token advance();
    
    // Validation Helpers
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);

    // The core logic that builds a single ASTNode
    std::shared_ptr<ASTNode> parseBlock();

public:
    // Boot up the parser with the tokens from the Lexer
    Parser(const std::vector<Token>& lexerTokens);

    // Start parsing and return the "Root" node (usually {main})
    std::shared_ptr<ASTNode> parse();
};

#endif // JUMBOLANG_PARSER_H