#include "../include/Lexer.h"
#include <cctype>

// ---------------------------------------------------------
// 1. ENGINE INITIALIZATION
// Boot up the Lexer and point it at the start of the file.
// ---------------------------------------------------------
Lexer::Lexer(const std::string& source) {
    sourceCode = source;
    position = 0;
    currentLine = 1;
    currentColumn = 1;
}

// ---------------------------------------------------------
// 2. HARDWARE MOVEMENT HELPERS
// Safely step through the RAM byte-by-byte.
// ---------------------------------------------------------
char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return sourceCode[position];
}

char Lexer::advance() {
    currentColumn++;
    return sourceCode[position++];
}

bool Lexer::isAtEnd() const {
    return position >= sourceCode.length();
}

// ---------------------------------------------------------
// 3. WHITESPACE & LINE TRACKER
// ---------------------------------------------------------
void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance(); 
        } else if (c == '\n') {
            currentLine++;       
            currentColumn = 1;   
            advance();
        } else {
            break; 
        }
    }
}

// ---------------------------------------------------------
// 4. THE EXTRACTION LOGIC
// Slice out specific data types safely.
// ---------------------------------------------------------
Token Lexer::extractString() {
    int startLine = currentLine;
    int startCol = currentColumn;
    std::string value = "";

    advance(); // Skip the opening quote '"'

    while (!isAtEnd() && peek() != '"') {
        value += advance();
    }

    if (!isAtEnd()) advance(); // Skip the closing quote '"'

    return {TokenType::STRING_LITERAL, value, startLine, startCol};
}

Token Lexer::extractIdentifier() {
    int startLine = currentLine;
    int startCol = currentColumn;
    std::string value = "";

    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
        value += advance();
    }

    return {TokenType::IDENTIFIER, value, startLine, startCol};
}

// ---------------------------------------------------------
// 5. THE MASTER TOKENIZE LOOP
// The heartbeat of the Lexer.
// ---------------------------------------------------------
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    bool inTag = false; 

    while (!isAtEnd()) {
        if (!inTag) {
            // STATE 0: Reading plain body text or looking for a tag
            std::string bodyText = "";
            int startLine = currentLine;
            int startCol = currentColumn;

            while (!isAtEnd() && peek() != '{') {
                bodyText += advance();
            }

            if (!bodyText.empty() && bodyText.find_first_not_of(" \t\n\r") != std::string::npos) {
                tokens.push_back({TokenType::BODY_TEXT, bodyText, startLine, startCol});
            }

            if (!isAtEnd() && peek() == '{') {
                tokens.push_back({TokenType::TAG_OPEN, "{", currentLine, currentColumn});
                advance(); 
                
                if (!isAtEnd() && peek() == '-') {
                    tokens.pop_back(); 
                    tokens.push_back({TokenType::TAG_CLOSE_OPEN, "{-", currentLine, currentColumn});
                    advance(); 
                }
                inTag = true; 
            }
        } else {
            // STATE 1: Inside a tag
            skipWhitespace();
            if (isAtEnd()) break;

            char c = peek();

            if (c == '}') {
                tokens.push_back({TokenType::TAG_CLOSE, "}", currentLine, currentColumn});
                advance();
                inTag = false; 
            } 
            else if (c == '=') {
                tokens.push_back({TokenType::EQUALS, "=", currentLine, currentColumn});
                advance();
            } 
            else if (c == '"') {
                tokens.push_back(extractString());
            } 
            else if (std::isalpha(c)) {
                tokens.push_back(extractIdentifier());
            } 
            else {
                advance(); 
            }
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "EOF", currentLine, currentColumn});
    return tokens;
}