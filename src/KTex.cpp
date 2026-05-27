#include "KTex.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

uint8_t ChannelMul(uint8_t v, uint8_t a) {
    return static_cast<uint8_t>((static_cast<int>(v) * static_cast<int>(a) + 127) / 255);
}

void DecodeDXTColorBlock(const uint8_t* src, uint8_t* dst, int dstStride, int blockW, int blockH, bool dxt1Alpha, bool writeAlpha) {
    uint16_t c0 = src[0] | (src[1] << 8);
    uint16_t c1 = src[2] | (src[3] << 8);
    uint8_t r[4], g[4], b[4], a[4];
    auto unpack = [](uint16_t c, uint8_t& R, uint8_t& G, uint8_t& B) {
        R = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
        G = static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
        B = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
    };
    unpack(c0, r[0], g[0], b[0]);
    unpack(c1, r[1], g[1], b[1]);
    a[0] = a[1] = a[2] = 255;
    if (c0 > c1 || !dxt1Alpha) {
        r[2] = (2 * r[0] + r[1]) / 3;
        g[2] = (2 * g[0] + g[1]) / 3;
        b[2] = (2 * b[0] + b[1]) / 3;
        r[3] = (r[0] + 2 * r[1]) / 3;
        g[3] = (g[0] + 2 * g[1]) / 3;
        b[3] = (b[0] + 2 * b[1]) / 3;
        a[3] = 255;
    } else {
        r[2] = (r[0] + r[1]) / 2;
        g[2] = (g[0] + g[1]) / 2;
        b[2] = (b[0] + b[1]) / 2;
        r[3] = g[3] = b[3] = 0;
        a[3] = 0;
    }
    uint32_t indices = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
    for (int y = 0; y < blockH; ++y) {
        for (int x = 0; x < blockW; ++x) {
            int idx = (indices >> (2 * (y * 4 + x))) & 0x3;
            uint8_t* px = dst + y * dstStride + x * 4;
            px[0] = r[idx];
            px[1] = g[idx];
            px[2] = b[idx];
            if (writeAlpha) px[3] = a[idx];
        }
    }
}

void DecodeDXT5AlphaBlock(const uint8_t* src, uint8_t* dst, int dstStride, int blockW, int blockH) {
    uint8_t a0 = src[0];
    uint8_t a1 = src[1];
    uint8_t alpha[8];
    alpha[0] = a0;
    alpha[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i)
            alpha[i + 1] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; ++i)
            alpha[i + 1] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        alpha[6] = 0;
        alpha[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i)
        bits |= static_cast<uint64_t>(src[2 + i]) << (8 * i);
    for (int y = 0; y < blockH; ++y) {
        for (int x = 0; x < blockW; ++x) {
            int idx = (bits >> (3 * (y * 4 + x))) & 0x7;
            dst[y * dstStride + x * 4 + 3] = alpha[idx];
        }
    }
}

void DecodeDXT3AlphaBlock(const uint8_t* src, uint8_t* dst, int dstStride, int blockW, int blockH) {
    for (int y = 0; y < blockH; ++y) {
        uint16_t row = src[y * 2] | (src[y * 2 + 1] << 8);
        for (int x = 0; x < blockW; ++x) {
            uint8_t a4 = (row >> (4 * x)) & 0xF;
            dst[y * dstStride + x * 4 + 3] = static_cast<uint8_t>((a4 * 255) / 15);
        }
    }
}

void DecodeBlocks(int format, const uint8_t* src, int w, int h, uint8_t* dst) {
    int blockSize = (format == 0) ? 8 : 16;
    int stride = w * 4;
    for (int by = 0; by < h; by += 4) {
        for (int bx = 0; bx < w; bx += 4) {
            int blockW = std::min(4, w - bx);
            int blockH = std::min(4, h - by);
            uint8_t* blockDst = dst + by * stride + bx * 4;
            const uint8_t* p = src;
            if (format == 1) {
                DecodeDXT3AlphaBlock(p, blockDst, stride, blockW, blockH);
                p += 8;
                DecodeDXTColorBlock(p, blockDst, stride, blockW, blockH, false, false);
            } else if (format == 2) {
                DecodeDXT5AlphaBlock(p, blockDst, stride, blockW, blockH);
                p += 8;
                DecodeDXTColorBlock(p, blockDst, stride, blockW, blockH, false, false);
            } else {
                DecodeDXTColorBlock(p, blockDst, stride, blockW, blockH, true, true);
            }
            src += blockSize;
        }
    }
}

}

bool LoadKTex(const std::string& path, KTex& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "KTex: cannot open %s\n", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(size);
    if (std::fread(buf.data(), 1, size, f) != static_cast<size_t>(size)) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    if (!LoadKTexFromMemory(buf.data(), buf.size(), out)) {
        std::fprintf(stderr, "KTex: decode failed for %s\n", path.c_str());
        return false;
    }
    return true;
}

bool LoadKTexFromMemory(const uint8_t* bytes, size_t size, KTex& out) {
    if (!bytes || size < 8 || std::memcmp(bytes, "KTEX", 4) != 0) {
        std::fprintf(stderr, "KTex: bad memory image\n");
        return false;
    }

    const uint8_t* buf = bytes;
    uint32_t spec = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    uint32_t pixelFormat = (spec >> 4) & 0x1F;
    uint32_t mipCount = (spec >> 13) & 0x1F;

    size_t off = 8;
    struct MipDesc { uint16_t w, h, pitch; uint32_t dataSize; };
    std::vector<MipDesc> mips(mipCount);
    for (uint32_t i = 0; i < mipCount; ++i) {
        if (off + 10 > size) return false;
        mips[i].w = buf[off] | (buf[off + 1] << 8);
        mips[i].h = buf[off + 2] | (buf[off + 3] << 8);
        mips[i].pitch = buf[off + 4] | (buf[off + 5] << 8);
        mips[i].dataSize = buf[off + 6] | (buf[off + 7] << 8) | (buf[off + 8] << 16) | (buf[off + 9] << 24);
        off += 10;
    }

    if (mipCount == 0) return false;
    const MipDesc& m = mips[0];
    out.width = m.w;
    out.height = m.h;
    out.rgba.assign(static_cast<size_t>(m.w) * m.h * 4, 0);

    if (off + m.dataSize > size) return false;
    const uint8_t* data = buf + off;

    int fmt = -1;
    switch (pixelFormat) {
        case 0: fmt = 0; break;
        case 1: fmt = 1; break;
        case 2: fmt = 2; break;
        case 4:
        case 5: {
            for (int y = 0; y < m.h; ++y) {
                for (int x = 0; x < m.w; ++x) {
                    const uint8_t* s = data + (y * m.w + x) * 4;
                    uint8_t* d = out.rgba.data() + (y * m.w + x) * 4;
                    d[0] = s[2];
                    d[1] = s[1];
                    d[2] = s[0];
                    d[3] = s[3];
                }
            }
            return true;
        }
        case 7: {
            std::memcpy(out.rgba.data(), data, static_cast<size_t>(m.w) * m.h * 4);
            return true;
        }
        default:
            std::fprintf(stderr, "KTex: unsupported pixel format %u\n", pixelFormat);
            return false;
    }

    DecodeBlocks(fmt, data, m.w, m.h, out.rgba.data());
    return true;
}
