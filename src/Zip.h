#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class ZipArchive {
public:
    bool Load(const std::string& path);
    bool ReadEntry(const std::string& name, std::vector<uint8_t>& out) const;
    bool HasEntry(const std::string& name) const;

private:
    struct Entry {
        uint16_t method = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        size_t dataOffset = 0;
    };

    std::vector<uint8_t> data;
    std::unordered_map<std::string, Entry> entries;
};
