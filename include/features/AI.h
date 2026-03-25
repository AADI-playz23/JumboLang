// include/features/AI.h
#ifndef JUMBOLANG_AI_H
#define JUMBOLANG_AI_H

#include <string>
#include <vector>

class AIManager {
private:
    std::string apiKey;
    std::string modelName;

public:
    // Initialize with a model name (e.g., "gemini-1.5-flash")
    AIManager(const std::string& model);

    // Feature: Set the API Key dynamically
    void setApiKey(const std::string& key);

    // Feature: The core "Thinking" function
    // This will eventually perform the actual HTTPS POST request
    std::string generateResponse(const std::string& prompt);

    // Feature: Clean the prompt (remove extra spaces/newlines)
    std::string sanitizePrompt(const std::string& raw);
};

#endif