// include/features/Database.h
#ifndef JUMBOLANG_DATABASE_H
#define JUMBOLANG_DATABASE_H

#include <string>
#include <map>

class DatabaseManager {
private:
    std::string dbFile;
    std::map<std::string, std::string> data;
    void load();
    void save();

public:
    DatabaseManager(const std::string& filename = "main.jdb");
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
};

#endif