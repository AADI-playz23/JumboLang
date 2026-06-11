// include/Lexer.h
#ifndef JUMBOLANG_LEXER_H
#define JUMBOLANG_LEXER_H

#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------
// 1. THE JUMBOLANG VOCABULARY (Token Types)
// This enum is how the CPU categorizes every word it reads.
// ---------------------------------------------------------
enum class TokenType {
    TAG_OPEN,           // '{'
    TAG_CLOSE_OPEN,     // '{-' (Closing a block)
    TAG_CLOSE,          // '}'
    IDENTIFIER,         // Tag names like 'https', 'llm', 'route'
    ATTRIBUTE_NAME,     // Keys like 'port', 'url', 'model'
    EQUALS,             // '='
    STRING_LITERAL,     // Values inside quotes like '"443"' or '"wss://"'
    NUMBER_LITERAL,     // Raw numbers
    BODY_TEXT,          // The raw text/code between blocks
    END_OF_FILE         // Tells the engine to stop reading safely
};

// ---------------------------------------------------------
// 2. THE TOKEN DATA STRUCTURE
// Every time the Lexer identifies a piece of text, it packs it 
// into this secure object along with its exact line number.
// ---------------------------------------------------------
struct Token {
    TokenType type;
    std::string value;
    int line;           // Crucial for telling the user where they made a typo
    int column;

    // A quick helper to print the token so we can see what the engine is doing
    void print() const {
        std::cout << "[Line " << line << ":" << column << "] Token: " << value << "\n";
    }
};

// ---------------------------------------------------------
// 3. THE LEXER ENGINE DEFINITION
// This is the blueprint for the machine that scans the .jl files.
// ---------------------------------------------------------
class Lexer {
private:
    std::string sourceCode;
    size_t position;
    int currentLine;
    int currentColumn;

    // Hardware Movement Helpers
    char peek() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();

    // The logic extractors (we will write these in Chunk 2)
    Token extractString();
    Token extractIdentifier();
    Token extractBodyText();

public:
    // The constructor that boots up the Lexer
    Lexer(const std::string& source);

    // The main function that returns a fully organized list of tokens
    std::vector<Token> tokenize();
};

#endif // JUMBOLANG_LEXER_H