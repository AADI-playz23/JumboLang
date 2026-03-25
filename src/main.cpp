// src/main.cpp
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
    // Ensures the user actually provided a JumboLang script to run.
    if (argc < 2) {
        std::cerr << "🐘 JumboLang CLI Error: No input file specified.\n";
        std::cerr << "Usage: ./jumbol <filename.jl>\n";
        return 1;
    }

    // 2. LOADING THE SOURCE CODE
    // We read the entire file into a string buffer for processing.
    std::string filePath = argv[1];
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "🐘 JumboLang File Error: Could not open " << filePath << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    // 3. PHASE 1: LEXICAL ANALYSIS
    // Here we break the text into individual tokens (words and symbols).
    Lexer lexer(sourceCode);
    std::vector<Token> tokens = lexer.tokenize();

    // 4. PHASE 2: PARSING
    // We transform the flat list of tokens into a logical Abstract Syntax Tree (AST).
    std::cout << "--- 🐘 JUMBOLANG COMPILER FRONTEND ---\n";
    Parser parser(tokens);
    std::shared_ptr<ASTNode> root = parser.parse();

    if (!root) {
        std::cerr << "⚠️ Empty script or invalid root structure detected.\n";
        return 0;
    }

    // 5. PHASE 3: INTERPRETATION (THE VIRTUAL MACHINE)
    // The "Brain" takes the tree and starts triggering your C++ features.
    Interpreter vm;
    vm.run(root);

    return 0;
}