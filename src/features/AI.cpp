// src/features/AI.cpp
#include "../../include/features/AI.h"
#include <iostream>
#include <algorithm>

AIManager::AIManager(const std::string& model) : modelName(model) {
    // Default internal key for testing, usually loaded from ENV variables
    apiKey = "JUMBO_INTERNAL_TEST_KEY"; 
}

void AIManager::setApiKey(const std::string& key) {
    apiKey = key;
}

std::string AIManager::sanitizePrompt(const std::string& raw) {
    std::string clean = raw;
    // Remove leading/trailing whitespace which usually breaks JSON payloads
    clean.erase(0, clean.find_first_not_of(" \n\r\t"));
    clean.erase(clean.find_last_not_of(" \n\r\t") + 1);
    return clean;
}

std::string AIManager::generateResponse(const std::string& prompt) {
    std::string readyPrompt = sanitizePrompt(prompt);
    
    std::cout << "    🤖 [AI ROUTER] Routing to Model: " << modelName << "\n";
    std::cout << "    📡 [AI ROUTER] Sending payload (Size: " << readyPrompt.length() << " bytes)...\n";

    // Placeholder for the real API Call
    // Once we install libcurl, the code here will perform:
    // curl_easy_perform(curl);
    
    return "JumboLang AI simulated response for: " + readyPrompt;
}