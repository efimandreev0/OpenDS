#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct KTex {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

bool LoadKTex(const std::string& path, KTex& out);
bool LoadKTexFromMemory(const uint8_t* bytes, size_t size, KTex& out);
