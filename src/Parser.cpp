// src/Parser.cpp
// Builds the JumboLang AST from tokens.
// Key improvement over v1: ALL exit(1) calls have been replaced by error
// collection + synchronization so multiple errors are reported in one pass
// and the program does not crash mid-parse.
#include "../include/Parser.h"
#include <iostream>
#include <set>

// Tags whose body is raw data (not nested JumboLang blocks).
// They consume everything verbatim until their closing tag.
const std::set<std::string> RAW_TAGS = {"json", "file", "db"};

// ─────────────────────────────────────────────────────────────────────────────
// DEBUG PRINTER
// ─────────────────────────────────────────────────────────────────────────────
void ASTNode::print(int indent) const {
    std::string sp(indent, ' ');
    std::cout << sp << "📦 BLOCK: " << tagName << "\n";
    for (const auto& [k, v] : attributes)
        std::cout << sp << "   ┣━ ⚙️  ATTR: " << k << " = " << v << "\n";
    if (!bodyContent.empty())
        std::cout << sp << "   ┣━ 📄 TEXT: " << bodyContent << "\n";
    for (const auto& child : children)
        child->print(indent + 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// CONSTRUCTOR & NAVIGATION HELPERS
// ─────────────────────────────────────────────────────────────────────────────
Parser::Parser(const std::vector<Token>& lexerTokens)
    : tokens(lexerTokens), current(0) {}

Token Parser::peek()     const { return tokens[current]; }
Token Parser::previous() const { return tokens[current - 1]; }
bool  Parser::isAtEnd()  const { return peek().type == TokenType::END_OF_FILE; }

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

// ─────────────────────────────────────────────────────────────────────────────
// CONSUME — safe version (no exit())
// If the expected token is present, advances and returns it.
// Otherwise, records a descriptive error and returns a dummy token so the
// caller can continue without crashing.
// ─────────────────────────────────────────────────────────────────────────────
Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();

    // Build a rich error message and store it
    std::string err =
        "[Line " + std::to_string(peek().line) + ":" +
        std::to_string(peek().column) + "] " + message +
        " (found '" + peek().value + "' instead)";
    errors.push_back(err);

    // Return a harmless sentinel token so the parse can continue
    return {TokenType::END_OF_FILE, "", peek().line, peek().column};
}

// ─────────────────────────────────────────────────────────────────────────────
// SYNCHRONIZE — error recovery
// After a bad block, skip forward until we find the end of the current block's
// closing tag (TAG_CLOSE_OPEN + IDENTIFIER + TAG_CLOSE) or EOF.
// This lets the parser keep going and report further errors.
// ─────────────────────────────────────────────────────────────────────────────
void Parser::synchronize() {
    while (!isAtEnd()) {
        if (check(TokenType::TAG_CLOSE_OPEN)) {
            advance(); // consume '{-'
            advance(); // consume tag name
            if (check(TokenType::TAG_CLOSE)) advance(); // consume '}'
            return;
        }
        advance();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PARSE BLOCK
// Parses a single {tag attr="val"}...{-tag} unit and returns the ASTNode.
// Returns nullptr if an unrecoverable situation is detected.
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<ASTNode> Parser::parseBlock() {
    auto node = std::make_shared<ASTNode>();

    consume(TokenType::TAG_OPEN,   "Expected '{' to open a block.");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected a tag name after '{'.");
    node->tagName = nameToken.value;

    // Parse attributes: key="value" pairs
    while (check(TokenType::IDENTIFIER)) {
        std::string attrName = advance().value;
        consume(TokenType::EQUALS, "Expected '=' after attribute name '" + attrName + "'.");
        Token attrVal = consume(TokenType::STRING_LITERAL,
                                "Expected a quoted string value for attribute '" + attrName + "'.");
        node->attributes[attrName] = attrVal.value;
    }

    consume(TokenType::TAG_CLOSE, "Expected '}' to close the opening tag.");

    // ── Body parsing ──────────────────────────────────────────────────────────
    if (RAW_TAGS.count(node->tagName)) {
        // Raw data tags: consume everything verbatim until '{-'
        while (!isAtEnd() && !check(TokenType::TAG_CLOSE_OPEN)) {
            node->bodyContent += advance().value;
        }
    } else {
        // Normal tags: allow nested blocks and body text
        while (!isAtEnd() && !check(TokenType::TAG_CLOSE_OPEN)) {
            if (match(TokenType::BODY_TEXT)) {
                node->bodyContent += previous().value;
            } else if (check(TokenType::TAG_OPEN)) {
                node->children.push_back(parseBlock());
            } else {
                advance(); // skip unexpected tokens gracefully
            }
        }
    }

    // ── Closing tag: {-tagname} ───────────────────────────────────────────────
    consume(TokenType::TAG_CLOSE_OPEN, "Expected '{-' to close block '" + node->tagName + "'.");
    Token closeName = consume(TokenType::IDENTIFIER,
                              "Expected closing tag name after '{-'.");

    if (closeName.value != node->tagName && !closeName.value.empty()) {
        errors.push_back(
            "[Line " + std::to_string(closeName.line) + "] "
            "Mismatched tags: opened {" + node->tagName +
            "} but closed with {-" + closeName.value + "}.");
        synchronize();
    } else {
        consume(TokenType::TAG_CLOSE, "Expected '}' after closing tag name.");
    }

    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN ENTRY POINT
// Skips leading whitespace/body-text tokens, then parses the root block.
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<ASTNode> Parser::parse() {
    // Fast-forward to the first opening tag
    while (!isAtEnd() && !check(TokenType::TAG_OPEN)) advance();
    if (isAtEnd()) return nullptr;
    return parseBlock();
}

// ─────────────────────────────────────────────────────────────────────────────
// ERROR REPORTING HELPERS
// ─────────────────────────────────────────────────────────────────────────────
bool Parser::hasErrors() const { return !errors.empty(); }

void Parser::printErrors() const {
    std::cerr << "\n❌ JumboLang found " << errors.size()
              << " syntax error(s):\n\n";
    for (size_t i = 0; i < errors.size(); ++i) {
        std::cerr << "  [" << (i + 1) << "] " << errors[i] << "\n";
    }
    std::cerr << "\n";
}