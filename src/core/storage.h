#pragma once
#include <string>

struct Storage {
    virtual ~Storage() = default;
    virtual bool available() = 0;
    virtual bool read(const char* path, std::string& out) = 0;
    virtual bool write(const char* path, const std::string& data) = 0;  // write tmp + rename
    virtual bool exists(const char* path) = 0;
};
