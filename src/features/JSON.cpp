// src/features/JSON.cpp
#include "../../include/features/JSON.h"
#include <sstream>
#include <algorithm>

std::map<std::string, std::string> JSONManager::parse(const std::string& rawJson) {
    std::map<std::string, std::string> result;
    std::string clean = rawJson;
    
    // Remove braces and whitespace
    clean.erase(std::remove(clean.begin(), clean.end(), '{'), clean.end());
    clean.erase(std::remove(clean.begin(), clean.end(), '}'), clean.end());
    clean.erase(std::remove(clean.begin(), clean.end(), '\"'), clean.end());

    std::stringstream ss(clean);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        size_t colonPos = item.find(':');
        if (colonPos != std::string::npos) {
            std::string key = item.substr(0, colonPos);
            std::string value = item.substr(colonPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            result[key] = value;
        }
    }
    return result;
}

std::string JSONManager::stringify(const std::map<std::string, std::string>& data) {
    std::string json = "{";
    for (auto const& [key, val] : data) {
        json += "\"" + key + "\": \"" + val + "\",";
    }
    if (json.length() > 1) json.pop_back(); // Remove last comma
    json += "}";
    return json;
}