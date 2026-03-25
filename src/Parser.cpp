// src/Parser.cpp (Updated with Raw Content Support)
#include "../include/Parser.h"
#include <iostream>
#include <cstdlib>
#include <set>

// 1. Define tags that should NOT try to parse nested children
// This is how we allow JSON braces to exist safely.
const std::set<std::string> RAW_TAGS = {"json", "file", "db"};

void ASTNode::print(int indent) const {
    std::string spacing(indent, ' ');
    std::cout << spacing << "📦 BLOCK: " << tagName << "\n";
    for (const auto& attr : attributes) {
        std::cout << spacing << "   ┣━ ⚙️ ATTR: " << attr.first << " = " << attr.second << "\n";
    }
    if (!bodyContent.empty()) {
        std::cout << spacing << "   ┣━ 📄 TEXT: " << bodyContent << "\n";
    }
    for (const auto& child : children) {
        child->print(indent + 4);
    }
}

Parser::Parser(const std::vector<Token>& lexerTokens) {
    tokens = lexerTokens;
    current = 0;
}

Token Parser::peek() const { return tokens[current]; }
Token Parser::previous() const { return tokens[current - 1]; }
bool Parser::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    std::cerr << "\n❌ JUMBOLANG SYNTAX ERROR [Line " << peek().line << "]:\n";
    std::cerr << "   -> " << message << "\n";
    std::cerr << "   -> Found '" << peek().value << "' instead.\n\n";
    exit(1);
}

std::shared_ptr<ASTNode> Parser::parseBlock() {
    auto node = std::make_shared<ASTNode>();

    consume(TokenType::TAG_OPEN, "Expected '{' to start a block.");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected a tag name.");
    node->tagName = nameToken.value;

    while (check(TokenType::IDENTIFIER)) {
        std::string attrName = advance().value;
        consume(TokenType::EQUALS, "Expected '='.");
        Token attrValue = consume(TokenType::STRING_LITERAL, "Expected a string value.");
        node->attributes[attrName] = attrValue.value;
    }

    consume(TokenType::TAG_CLOSE, "Expected '}'.");

    // --- THE FIX: HANDLE RAW DATA ---
    if (RAW_TAGS.count(node->tagName)) {
        // If it's a JSON/File tag, just consume EVERYTHING as text until '{-'
        while (!isAtEnd() && !check(TokenType::TAG_CLOSE_OPEN)) {
            node->bodyContent += advance().value;
        }
    } else {
        // Otherwise, allow normal nesting (like {main} having {https} inside)
        while (!isAtEnd() && !check(TokenType::TAG_CLOSE_OPEN)) {
            if (match(TokenType::BODY_TEXT)) {
                node->bodyContent += previous().value;
            } else if (check(TokenType::TAG_OPEN)) {
                node->children.push_back(parseBlock());
            } else {
                advance();
            }
        }
    }

    consume(TokenType::TAG_CLOSE_OPEN, "Expected '{-' to close block.");
    Token closeName = consume(TokenType::IDENTIFIER, "Expected closing tag name.");
    
    if (closeName.value != node->tagName) {
        std::cerr << "\n❌ JUMBOLANG SYNTAX ERROR: Opened {" << node->tagName << "} but closed with {-" << closeName.value << "}.\n";
        exit(1);
    }

    consume(TokenType::TAG_CLOSE, "Expected '}' at the end.");
    return node;
}

std::shared_ptr<ASTNode> Parser::parse() {
    while (!isAtEnd() && !check(TokenType::TAG_OPEN)) advance();
    if (isAtEnd()) return nullptr;
    return parseBlock();
}