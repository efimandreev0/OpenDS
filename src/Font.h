#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

struct BakedGlyph {
    int x0, y0, x1, y1;
    float xoff, yoff, xadvance;
};

struct BakedFont {
    uint32_t glTexId = 0;
    int atlasW = 0;
    int atlasH = 0;
    float pixelHeight = 0;
    float bitmapScale = 1.0f;
    float ascent = 0;
    std::unordered_map<int, BakedGlyph> glyphs;
};

class FontCache {
public:
    bool Init();
    void SetDataRoot(const std::string& root) { dataRoot = root; }
    BakedFont* Get(const std::string& fontFile, int pixelHeight);
    void Shutdown();

private:
    std::unordered_map<std::string, BakedFont> fonts;
    std::string systemFontPath;
    std::string dataRoot;
};
