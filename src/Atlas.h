#pragma once
#include <string>
#include <unordered_map>

struct AtlasElement {
    float u1, v1, u2, v2;
};

struct Atlas {
    std::string textureFile;
    std::unordered_map<std::string, AtlasElement> elements;
};

bool LoadAtlas(const std::string& xmlPath, Atlas& out);
