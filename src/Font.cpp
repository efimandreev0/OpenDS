#include "Font.h"
#include "KTex.h"
#include "Zip.h"
#include "tinyxml2.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <SDL_opengl.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string AliasToFontZip(const std::string& alias) {
    if (alias == "buttonfont") return "fonts/buttonfont.zip";
    if (alias == "bp50") return "fonts/belisaplumilla50.zip";
    if (alias == "bp100") return "fonts/belisaplumilla100.zip";
    if (alias == "stint-ucr") return "fonts/stint-ucr50.zip";
    if (alias == "stint-small") return "fonts/stint-ucr20.zip";
    if (alias == "opensans") return "fonts/opensans50.zip";
    if (alias == "talkingfont") return "fonts/talkingfont.zip";
    if (alias.rfind("fonts/", 0) == 0 && alias.size() > 4) return alias;
    return "";
}

GLuint UploadRGBA(const uint8_t* pixels, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

bool LoadBitmapFontZip(const std::string& zipPath, int pixelHeight, BakedFont& bf) {
    ZipArchive zip;
    if (!zip.Load(zipPath)) return false;

    std::vector<uint8_t> fntBytes;
    std::vector<uint8_t> texBytes;
    if (!zip.ReadEntry("font.fnt", fntBytes) || !zip.ReadEntry("font.tex", texBytes)) {
        return false;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(reinterpret_cast<const char*>(fntBytes.data()), fntBytes.size()) != tinyxml2::XML_SUCCESS) {
        return false;
    }

    auto* font = doc.FirstChildElement("font");
    auto* common = font ? font->FirstChildElement("common") : nullptr;
    auto* chars = font ? font->FirstChildElement("chars") : nullptr;
    if (!common || !chars) return false;

    int lineHeight = 0;
    int base = 0;
    int scaleW = 0;
    int scaleH = 0;
    common->QueryIntAttribute("lineHeight", &lineHeight);
    common->QueryIntAttribute("base", &base);
    common->QueryIntAttribute("scaleW", &scaleW);
    common->QueryIntAttribute("scaleH", &scaleH);
    if (lineHeight <= 0 || scaleW <= 0 || scaleH <= 0) return false;

    KTex ktex;
    if (!LoadKTexFromMemory(texBytes.data(), texBytes.size(), ktex)) return false;

    bf.atlasW = ktex.width;
    bf.atlasH = ktex.height;
    bf.pixelHeight = static_cast<float>(pixelHeight);
    bf.bitmapScale = static_cast<float>(pixelHeight) / static_cast<float>(lineHeight);
    bf.ascent = static_cast<float>(base) * bf.bitmapScale;
    bf.glTexId = UploadRGBA(ktex.rgba.data(), ktex.width, ktex.height);

    for (auto* ch = chars->FirstChildElement("char"); ch; ch = ch->NextSiblingElement("char")) {
        int id = 0, x = 0, y = 0, w = 0, h = 0, xoff = 0, yoff = 0, xadv = 0;
        ch->QueryIntAttribute("id", &id);
        ch->QueryIntAttribute("x", &x);
        ch->QueryIntAttribute("y", &y);
        ch->QueryIntAttribute("width", &w);
        ch->QueryIntAttribute("height", &h);
        ch->QueryIntAttribute("xoffset", &xoff);
        ch->QueryIntAttribute("yoffset", &yoff);
        ch->QueryIntAttribute("xadvance", &xadv);
        BakedGlyph g;
        g.x0 = x;
        g.y0 = y;
        g.x1 = x + w;
        g.y1 = y + h;
        g.xoff = static_cast<float>(xoff);
        // BMFont stores yoff as pixels from top-of-line. Convert to baseline-
        // relative (negative for chars rising above baseline) so the renderer
        // can use the same formula it uses for stb_truetype baked glyphs.
        g.yoff = static_cast<float>(yoff - base);
        g.xadvance = static_cast<float>(xadv);
        bf.glyphs[id] = g;
    }

    return bf.glTexId != 0 && !bf.glyphs.empty();
}

}

bool FontCache::Init() {
    const char* candidates[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    for (const char* c : candidates) {
        if (fs::exists(c)) {
            systemFontPath = c;
            break;
        }
    }
    return !systemFontPath.empty();
}

void FontCache::Shutdown() {
    for (auto& [k, f] : fonts) {
        if (f.glTexId) {
            GLuint id = f.glTexId;
            glDeleteTextures(1, &id);
        }
    }
    fonts.clear();
}

BakedFont* FontCache::Get(const std::string& fontFile, int pixelHeight) {
    std::string key = fontFile + "@" + std::to_string(pixelHeight);
    auto it = fonts.find(key);
    if (it != fonts.end()) return &it->second;

    std::string fontZip = AliasToFontZip(fontFile);
    if (!fontZip.empty() && !dataRoot.empty()) {
        fs::path zipPath = fs::path(dataRoot) / fontZip;
        if (fs::exists(zipPath)) {
            BakedFont bf;
            if (LoadBitmapFontZip(zipPath.string(), pixelHeight, bf)) {
                auto [ins, _] = fonts.emplace(std::move(key), std::move(bf));
                return &ins->second;
            }
        }
    }

    std::string path = systemFontPath;
    if (path.empty()) return nullptr;

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    if (std::fread(data.data(), 1, size, f) != static_cast<size_t>(size)) {
        std::fclose(f);
        return nullptr;
    }
    std::fclose(f);

    BakedFont bf;
    bf.atlasW = 512;
    bf.atlasH = 512;
    bf.pixelHeight = static_cast<float>(pixelHeight);

    std::vector<uint8_t> atlasGrey(bf.atlasW * bf.atlasH, 0);
    constexpr int kFirstChar = 32;
    constexpr int kCharCount = 96;
    std::vector<stbtt_bakedchar> baked(kCharCount);
    int r = stbtt_BakeFontBitmap(data.data(), 0, static_cast<float>(pixelHeight),
                                  atlasGrey.data(), bf.atlasW, bf.atlasH,
                                  kFirstChar, kCharCount, baked.data());
    if (r <= 0 && r > -kCharCount) {
        // some chars didn't fit, but we keep what we got
    }

    std::vector<uint8_t> atlasRGBA(bf.atlasW * bf.atlasH * 4);
    for (int i = 0; i < bf.atlasW * bf.atlasH; ++i) {
        uint8_t v = atlasGrey[i];
        atlasRGBA[i * 4 + 0] = 255;
        atlasRGBA[i * 4 + 1] = 255;
        atlasRGBA[i * 4 + 2] = 255;
        atlasRGBA[i * 4 + 3] = v;
    }

    stbtt_fontinfo fi;
    if (stbtt_InitFont(&fi, data.data(), 0)) {
        int asc = 0, dsc = 0, lg = 0;
        stbtt_GetFontVMetrics(&fi, &asc, &dsc, &lg);
        float scale = stbtt_ScaleForPixelHeight(&fi, static_cast<float>(pixelHeight));
        bf.ascent = asc * scale;
    }

    bf.glTexId = UploadRGBA(atlasRGBA.data(), bf.atlasW, bf.atlasH);

    for (int i = 0; i < kCharCount; ++i) {
        BakedGlyph g;
        g.x0 = baked[i].x0;
        g.y0 = baked[i].y0;
        g.x1 = baked[i].x1;
        g.y1 = baked[i].y1;
        g.xoff = baked[i].xoff;
        g.yoff = baked[i].yoff;
        g.xadvance = baked[i].xadvance;
        bf.glyphs[kFirstChar + i] = g;
    }

    auto [ins, _] = fonts.emplace(std::move(key), std::move(bf));
    return &ins->second;
}
