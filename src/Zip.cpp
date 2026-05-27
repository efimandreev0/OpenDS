#include "Zip.h"

#define STBI_ONLY_ZLIB
#define STBI_SUPPORT_ZLIB
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>

namespace {

uint16_t U16(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint16_t>(b[off] | (b[off + 1] << 8));
}

uint32_t U32(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

std::string NormalizeZipName(std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

}

bool ZipArchive::Load(const std::string& path) {
    data.clear();
    entries.clear();

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(f);
        return false;
    }
    data.resize(static_cast<size_t>(size));
    if (std::fread(data.data(), 1, data.size(), f) != data.size()) {
        std::fclose(f);
        data.clear();
        return false;
    }
    std::fclose(f);

    size_t off = 0;
    while (off + 30 <= data.size()) {
        uint32_t sig = U32(data, off);
        if (sig == 0x02014b50 || sig == 0x06054b50) break;
        if (sig != 0x04034b50) break;

        uint16_t flags = U16(data, off + 6);
        uint16_t method = U16(data, off + 8);
        uint32_t compressedSize = U32(data, off + 18);
        uint32_t uncompressedSize = U32(data, off + 22);
        uint16_t nameLen = U16(data, off + 26);
        uint16_t extraLen = U16(data, off + 28);
        size_t nameOff = off + 30;
        size_t dataOff = nameOff + nameLen + extraLen;

        if ((flags & 0x08) != 0 || nameOff + nameLen > data.size() || dataOff > data.size()) {
            return false;
        }
        if (dataOff + compressedSize > data.size()) {
            return false;
        }

        std::string name(reinterpret_cast<const char*>(data.data() + nameOff), nameLen);
        name = NormalizeZipName(name);
        if (!name.empty() && name.back() != '/') {
            entries[name] = Entry{method, compressedSize, uncompressedSize, dataOff};
        }

        off = dataOff + compressedSize;
    }

    return !entries.empty();
}

bool ZipArchive::HasEntry(const std::string& name) const {
    return entries.find(NormalizeZipName(name)) != entries.end();
}

bool ZipArchive::ReadEntry(const std::string& name, std::vector<uint8_t>& out) const {
    out.clear();
    auto it = entries.find(NormalizeZipName(name));
    if (it == entries.end()) return false;
    const Entry& e = it->second;
    if (e.dataOffset + e.compressedSize > data.size()) return false;

    const uint8_t* src = data.data() + e.dataOffset;
    if (e.method == 0) {
        out.assign(src, src + e.compressedSize);
        return out.size() == e.uncompressedSize;
    }
    if (e.method != 8) {
        return false;
    }

    int decodedLen = 0;
    char* decoded = stbi_zlib_decode_noheader_malloc(
        reinterpret_cast<const char*>(src),
        static_cast<int>(e.compressedSize),
        &decodedLen);
    if (!decoded) return false;

    out.assign(reinterpret_cast<uint8_t*>(decoded),
               reinterpret_cast<uint8_t*>(decoded) + decodedLen);
    std::free(decoded);
    return out.size() == e.uncompressedSize;
}
