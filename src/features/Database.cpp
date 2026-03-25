// src/features/Database.cpp
#include "../../include/features/Database.h"
#include <fstream>
#include <iostream>

DatabaseManager::DatabaseManager(const std::string& filename) : dbFile(filename) {
    load();
}

void DatabaseManager::load() {
    std::ifstream file(dbFile);
    std::string key, value;
    while (file >> key >> value) {
        data[key] = value;
    }
}

void DatabaseManager::save() {
    std::ofstream file(dbFile);
    for (auto const& [key, val] : data) {
        file << key << " " << val << "\n";
    }
}

void DatabaseManager::set(const std::string& key, const std::string& value) {
    data[key] = value;
    save(); // Auto-save for "Python-like" ease
}

std::string DatabaseManager::get(const std::string& key) {
    if (data.find(key) != data.end()) return data[key];
    return "NULL";
}