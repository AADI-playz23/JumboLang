// src/main.cpp
// JumboLang CLI entry point.
// Runs the full pipeline: source file → lexer → parser → interpreter.
// Parser errors are reported gracefully (never crashes with exit(1) mid-parse).
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include "../include/Lexer.h"
#include "../include/Parser.h"
#include "../include/Interpreter.h"

int main(int argc, char* argv[]) {
    // 1. COMMAND LINE VALIDATION
    if (argc < 2) {
        std::cerr << "🐘 JumboLang CLI Error: No input file specified.\n";
        std::cerr << "   Usage: ./jumbol <filename.jl>\n";
        return 1;
    }

    // 2. LOAD SOURCE FILE
    std::string filePath = argv[1];
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "🐘 JumboLang File Error: Could not open '" << filePath << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    if (sourceCode.empty()) {
        std::cerr << "⚠️  JumboLang: Input file is empty.\n";
        return 0;
    }

    // 3. PHASE 1 — LEXICAL ANALYSIS
    Lexer lexer(sourceCode);
    std::vector<Token> tokens = lexer.tokenize();

    // 4. PHASE 2 — PARSING
    std::cout << "--- 🐘 JUMBOLANG COMPILER FRONTEND ---\n";
    Parser parser(tokens);
    std::shared_ptr<ASTNode> root = parser.parse();

    // Report all syntax errors collected during parsing (no crash)
    if (parser.hasErrors()) {
        parser.printErrors();
        // If we have a (partial) root, warn but still attempt execution
        if (!root) {
            std::cerr << "🐘 JumboLang: Could not build AST — execution aborted.\n";
            return 1;
        }
        std::cerr << "⚠️  JumboLang: Proceeding with partial AST. "
                  << "Some blocks may be skipped.\n\n";
    }

    if (!root) {
        std::cerr << "⚠️  JumboLang: Empty script or invalid root structure.\n";
        return 0;
    }

    // 5. PHASE 3 — INTERPRETATION (THE VIRTUAL MACHINE)
    Interpreter vm;
    vm.run(root);

    return parser.hasErrors() ? 1 : 0;
}