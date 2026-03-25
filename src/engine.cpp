#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// This function acts as our Lexer and Parser
void runJumboScript(const std::string& code) {
    bool inTag = false;
    std::string currentTag = "";
    std::string currentBody = "";

    std::cout << "--- JUMBOLANG V8 ENGINE STARTED ---\n\n";

    // Scan every single character one by one
    for (char c : code) {
        if (c == '{') {
            // We found a new tag! Print the body text we just collected
            if (!currentBody.empty() && currentBody.find_first_not_of(" \t\n\r") != std::string::npos) {
                std::cout << "[EXECUTING BODY] -> " << currentBody << "\n";
            }
            currentBody = ""; // Clear the box
            inTag = true;     // Switch to State 1: Tag Mode

        } else if (c == '}') {
            // The tag closed! 
            std::cout << "[SYSTEM TAG] -> Processing {" << currentTag << "}\n";
            
            // --- FEATURE ROUTER ---
            // This is where we will add your custom features!
            if (currentTag == "main") {
                std::cout << "  * Initializing Main Application Space...\n";
            } else if (currentTag.find("https") != std::string::npos) {
                std::cout << "  * ACTION: Spinning up Web Server...\n";
            } else if (currentTag.find("llm") != std::string::npos) {
                std::cout << "  * ACTION: Connecting to AI Model...\n";
            }

            currentTag = "";  // Clear the box
            inTag = false;    // Switch to State 0: Body Mode

        } else {
            // If it's a normal letter, put it in the correct box based on our State
            if (inTag) {
                currentTag += c;
            } else {
                currentBody += c;
            }
        }
    }
    std::cout << "\n--- EXECUTION COMPLETE ---\n";
}

int main(int argc, char* argv[]) {
    // Make sure the user provided a file (e.g., ./jumbol tests/app.jl)
    if (argc < 2) {
        std::cerr << "Usage: jumbol <file.jl>\n";
        return 1;
    }

    // Open the file and read it into memory
    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();

    // Send the code to our new engine!
    runJumboScript(code);

    return 0;
}