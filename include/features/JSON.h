// include/features/JSON.h
#ifndef JUMBOLANG_JSON_H
#define JUMBOLANG_JSON_H

#include <string>
#include <map>

class JSONManager {
public:
    // Feature: Simple Key-Value extraction from a JSON string
    static std::map<std::string, std::string> parse(const std::string& rawJson);
    
    // Feature: Convert a Map back into a JSON string
    static std::string stringify(const std::map<std::string, std::string>& data);
};

#endif