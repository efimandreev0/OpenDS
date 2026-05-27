#include "Renderer.h"
#include "Zip.h"
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr float kReferenceW = 1280.0f;
constexpr float kReferenceH = 720.0f;

struct BinReader {
    const std::vector<uint8_t>& b;
    size_t off = 0;
    bool ok = true;

    bool Need(size_t n) {
        if (!ok || off + n > b.size()) {
            ok = false;
            return false;
        }
        return true;
    }
    uint8_t Byte() {
        if (!Need(1)) return 0;
        return b[off++];
    }
    uint32_t U32() {
        if (!Need(4)) return 0;
        uint32_t v = static_cast<uint32_t>(b[off]) |
                     (static_cast<uint32_t>(b[off + 1]) << 8) |
                     (static_cast<uint32_t>(b[off + 2]) << 16) |
                     (static_cast<uint32_t>(b[off + 3]) << 24);
        off += 4;
        return v;
    }
    int32_t I32() {
        return static_cast<int32_t>(U32());
    }
    float F32() {
        uint32_t raw = U32();
        float v = 0;
        std::memcpy(&v, &raw, sizeof(v));
        return v;
    }
    std::string String() {
        uint32_t len = U32();
        if (!Need(len)) return {};
        std::string s(reinterpret_cast<const char*>(b.data() + off), len);
        off += len;
        return s;
    }
    std::string Magic() {
        if (!Need(4)) return {};
        std::string s(reinterpret_cast<const char*>(b.data() + off), 4);
        off += 4;
        return s;
    }
};

struct AnimVertex {
    float x = 0, y = 0, z = 0, u = 0, v = 0, atlas = 0;
};

struct BuildFrame {
    uint32_t frame = 0;
    uint32_t duration = 0;
    float x = 0, y = 0, w = 0, h = 0;
    uint32_t vertexStart = 0;
    uint32_t vertexCount = 0;
};

struct AnimElement {
    uint32_t symbolHash = 0;
    int32_t symbolFrame = 0;
    uint32_t folderHash = 0;
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0, tz = 0;
};

struct AnimFrame {
    float x = 0, y = 0, w = 0, h = 0;
    std::vector<AnimElement> elements;
};

struct AnimClip {
    std::string name;
    float frameRate = 30.0f;
    std::vector<AnimFrame> frames;
};

}

struct LoadedAnim {
    std::vector<LoadedTexture> atlases;
    std::vector<AnimVertex> vertices;
    std::unordered_map<uint32_t, std::unordered_map<int32_t, BuildFrame>> symbols;
    std::unordered_map<std::string, AnimClip> clips;
};

Renderer::~Renderer() = default;

namespace {

GLuint UploadTexture(const KTex& k) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, k.width, k.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, k.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}

bool ParseBuild(const std::vector<uint8_t>& bytes, LoadedAnim& out, std::vector<std::string>& materialNames) {
    BinReader r{bytes};
    if (r.Magic() != "BILD") return false;
    uint32_t version = r.U32();
    uint32_t symbolCount = r.U32();
    r.U32(); // total frames
    r.String(); // build name
    uint32_t materialCount = r.U32();
    if (!r.ok || version != 6) return false;

    for (uint32_t i = 0; i < materialCount; ++i) {
        materialNames.push_back(r.String());
    }

    for (uint32_t i = 0; i < symbolCount && r.ok; ++i) {
        uint32_t hash = r.U32();
        uint32_t frameCount = r.U32();
        auto& frames = out.symbols[hash];
        for (uint32_t j = 0; j < frameCount && r.ok; ++j) {
            BuildFrame f;
            f.frame = r.U32();
            f.duration = r.U32();
            f.x = r.F32();
            f.y = r.F32();
            f.w = r.F32();
            f.h = r.F32();
            f.vertexStart = r.U32();
            f.vertexCount = r.U32();
            frames[static_cast<int32_t>(f.frame)] = f;
        }
    }

    uint32_t vertexCount = r.U32();
    out.vertices.reserve(vertexCount);
    for (uint32_t i = 0; i < vertexCount && r.ok; ++i) {
        AnimVertex v;
        v.x = r.F32();
        v.y = r.F32();
        v.z = r.F32();
        v.u = r.F32();
        v.v = r.F32();
        v.atlas = r.F32();
        out.vertices.push_back(v);
    }

    return r.ok;
}

bool ParseAnim(const std::vector<uint8_t>& bytes, LoadedAnim& out) {
    BinReader r{bytes};
    if (r.Magic() != "ANIM") return false;
    uint32_t version = r.U32();
    r.U32(); // total element refs
    r.U32(); // total frames
    r.U32(); // total events
    uint32_t animCount = r.U32();
    if (!r.ok || version != 4) return false;

    for (uint32_t ai = 0; ai < animCount && r.ok; ++ai) {
        AnimClip clip;
        clip.name = r.String();
        r.Byte(); // facings
        r.U32(); // root symbol hash
        clip.frameRate = r.F32();
        uint32_t frameCount = r.U32();
        clip.frames.reserve(frameCount);

        for (uint32_t fi = 0; fi < frameCount && r.ok; ++fi) {
            AnimFrame frame;
            frame.x = r.F32();
            frame.y = r.F32();
            frame.w = r.F32();
            frame.h = r.F32();

            uint32_t eventCount = r.U32();
            for (uint32_t ei = 0; ei < eventCount && r.ok; ++ei) r.U32();

            uint32_t elementCount = r.U32();
            frame.elements.reserve(elementCount);
            for (uint32_t ei = 0; ei < elementCount && r.ok; ++ei) {
                AnimElement e;
                e.symbolHash = r.U32();
                e.symbolFrame = r.I32();
                e.folderHash = r.U32();
                e.a = r.F32();
                e.b = r.F32();
                e.c = r.F32();
                e.d = r.F32();
                e.tx = r.F32();
                e.ty = r.F32();
                e.tz = r.F32();
                frame.elements.push_back(e);
            }
            clip.frames.push_back(std::move(frame));
        }

        if (!clip.name.empty()) {
            out.clips[clip.name] = std::move(clip);
        }
    }

    return r.ok && !out.clips.empty();
}

std::unique_ptr<LoadedAnim> LoadAnimZip(Renderer& renderer, const std::string& buildName) {
    std::string path = renderer.ResolveDataPath("anim/" + buildName + ".zip");
    if (path.empty() || !fs::exists(path)) return nullptr;

    ZipArchive zip;
    if (!zip.Load(path)) return nullptr;

    std::vector<uint8_t> buildBytes;
    std::vector<uint8_t> animBytes;
    if (!zip.ReadEntry("build.bin", buildBytes) || !zip.ReadEntry("anim.bin", animBytes)) {
        return nullptr;
    }

    auto anim = std::make_unique<LoadedAnim>();
    std::vector<std::string> materialNames;
    if (!ParseBuild(buildBytes, *anim, materialNames) || !ParseAnim(animBytes, *anim)) {
        return nullptr;
    }

    for (const std::string& material : materialNames) {
        std::vector<uint8_t> texBytes;
        KTex k;
        if (zip.ReadEntry(material, texBytes) && LoadKTexFromMemory(texBytes.data(), texBytes.size(), k)) {
            LoadedTexture lt;
            lt.width = k.width;
            lt.height = k.height;
            lt.glId = UploadTexture(k);
            anim->atlases.push_back(lt);
        }
    }

    if (anim->atlases.empty()) return nullptr;
    return anim;
}

float ScaleModeFactor(const UITransform& t, int vpW, int vpH) {
    if (t.scaleMode != ScaleMode::Proportional && t.scaleMode != ScaleMode::FixedProportional) {
        return 1.0f;
    }
    float s = std::min(vpW / kReferenceW, vpH / kReferenceH);
    if (t.maxPropUpscale > 0.0f) s = std::min(s, t.maxPropUpscale);
    return s;
}

}

bool Renderer::Init() {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    fonts.Init();
    return true;
}

void Renderer::Shutdown() {
    for (auto& [_, anim] : animations) {
        if (!anim) continue;
        for (auto& t : anim->atlases) {
            if (t.glId) {
                GLuint id = t.glId;
                glDeleteTextures(1, &id);
            }
        }
        delete anim;
    }
    animations.clear();
    fonts.Shutdown();
    for (auto& [k, t] : textures) {
        if (t.glId) {
            GLuint id = t.glId;
            glDeleteTextures(1, &id);
        }
    }
    textures.clear();
    atlases.clear();
}

void Renderer::SetDataRoot(const std::string& root) {
    dataRoot = root;
    fonts.SetDataRoot(root);
}

void Renderer::BeginFrame(int viewportW, int viewportH) {
    vpW = viewportW;
    vpH = viewportH;
    glViewport(0, 0, viewportW, viewportH);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float halfW = viewportW * 0.5f;
    float halfH = viewportH * 0.5f;
    glOrtho(-halfW, halfW, -halfH, halfH, -1000.0f, 1000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Renderer::EndFrame() {}

const Atlas* Renderer::GetAtlas(const std::string& path) {
    auto it = atlases.find(path);
    if (it != atlases.end()) {
        return it->second.textureFile.empty() ? nullptr : &it->second;
    }
    Atlas a;
    std::string full = ResolveDataPath(path);
    bool ok = !full.empty() && LoadAtlas(full, a);
    auto [ins, _] = atlases.emplace(path, ok ? std::move(a) : Atlas{});
    return ok ? &ins->second : nullptr;
}

std::string Renderer::ResolveDataPath(const std::string& path) const {
    if (path.empty()) return {};
    fs::path p(path);
    if (p.is_absolute()) {
        return (fs::exists(p) && !fs::is_directory(p)) ? p.string() : std::string();
    }

    const char* dlcs[] = {"DLC0003", "DLC0002", "DLC0001"};
    for (const char* dlc : dlcs) {
        fs::path candidate = fs::path(dataRoot) / dlc / p;
        if (fs::exists(candidate) && !fs::is_directory(candidate)) return candidate.string();
    }

    fs::path candidate = fs::path(dataRoot) / p;
    if (fs::exists(candidate) && !fs::is_directory(candidate)) return candidate.string();
    return candidate.string();
}

LoadedTexture Renderer::LoadTextureFile(const std::string& texPath) {
    auto it = textures.find(texPath);
    if (it != textures.end()) return it->second;

    LoadedTexture t;
    KTex k;
    std::string full = ResolveDataPath(texPath);
    if (!LoadKTex(full, k)) {
        textures[texPath] = t;
        return t;
    }

    t.glId = UploadTexture(k);
    t.width = k.width;
    t.height = k.height;
    textures[texPath] = t;
    return t;
}

const LoadedTexture* Renderer::GetTextureFromAtlas(const std::string& atlasPath) {
    const Atlas* a = GetAtlas(atlasPath);
    if (!a) return nullptr;
    fs::path atlasDir = fs::path(atlasPath).parent_path();
    std::string texRelative = (atlasDir / a->textureFile).generic_string();
    auto it = textures.find(texRelative);
    if (it == textures.end()) {
        LoadedTexture lt = LoadTextureFile(texRelative);
        if (!lt.glId) return nullptr;
        it = textures.find(texRelative);
    }
    return &it->second;
}

void Renderer::ApplyTransform(Entity* e, bool topLevel) {
    if (!e->uiTransform) return;
    const auto& t = *e->uiTransform;

    if (topLevel) {
        float ox = 0, oy = 0;
        if (t.hAnchor == Anchor::Min) ox = -vpW * 0.5f;
        else if (t.hAnchor == Anchor::Max) ox = vpW * 0.5f;
        if (t.vAnchor == Anchor::Min) oy = vpH * 0.5f;
        else if (t.vAnchor == Anchor::Max) oy = -vpH * 0.5f;
        glTranslatef(ox, oy, 0.0f);
    }

    glTranslatef(t.x, t.y, 0.0f);
    float modeScale = ScaleModeFactor(t, vpW, vpH);
    if (modeScale != 1.0f) glScalef(modeScale, modeScale, 1.0f);
    if (t.rotation != 0.0f) glRotatef(t.rotation, 0, 0, 1);
    glScalef(t.scaleX, t.scaleY, 1.0f);
}

void Renderer::DrawAnim(Entity* e) {
    const auto& aw = *e->animWidget;
    std::string build = !aw.build.empty() ? aw.build : aw.bank;
    if (build.empty()) return;

    auto it = animations.find(build);
    if (it == animations.end()) {
        auto loaded = LoadAnimZip(*this, build);
        if (!loaded) {
            animations[build] = nullptr;
            return;
        }
        it = animations.emplace(build, loaded.release()).first;
    }
    if (!it->second) return;
    LoadedAnim& anim = *it->second;

    const AnimClip* clip = nullptr;
    auto clipIt = anim.clips.find(aw.anim);
    if (clipIt != anim.clips.end()) {
        clip = &clipIt->second;
    } else if (!anim.clips.empty()) {
        clip = &anim.clips.begin()->second;
    }
    if (!clip || clip->frames.empty()) return;

    int frameIndex = static_cast<int>(std::floor(aw.time * clip->frameRate));
    if (aw.loop) frameIndex %= static_cast<int>(clip->frames.size());
    else frameIndex = std::min(frameIndex, static_cast<int>(clip->frames.size()) - 1);
    if (frameIndex < 0) frameIndex = 0;
    const AnimFrame& frame = clip->frames[frameIndex];

    glColor4f(1, 1, 1, 1);
    // Klei kanim stores elements front-to-back (first = top-most). With no depth
    // test, the last draw wins, so we iterate in reverse to put back-most first.
    for (auto rit = frame.elements.rbegin(); rit != frame.elements.rend(); ++rit) {
        const AnimElement& el = *rit;
        auto symIt = anim.symbols.find(el.symbolHash);
        if (symIt == anim.symbols.end()) continue;

        auto frameIt = symIt->second.find(el.symbolFrame);
        if (frameIt == symIt->second.end()) {
            // Anim references a symbolFrame outside what the build stores.
            // Klei wraps modulo the symbol's frame count; if the wrapped
            // index is missing (sparse symbol), pick the nearest existing
            // one to avoid one-frame disappearances.
            if (symIt->second.empty()) continue;
            int32_t maxKey = -1;
            for (const auto& [k, _] : symIt->second) {
                if (k > maxKey) maxKey = k;
            }
            int32_t frameCount = maxKey + 1;
            int32_t target = el.symbolFrame;
            if (frameCount > 0) {
                target = ((target % frameCount) + frameCount) % frameCount;
            }
            int32_t bestKey = -1;
            int32_t bestDist = std::numeric_limits<int32_t>::max();
            for (const auto& [k, _] : symIt->second) {
                int32_t d = std::abs(k - target);
                if (d < bestDist) {
                    bestDist = d;
                    bestKey = k;
                }
            }
            frameIt = symIt->second.find(bestKey);
            if (frameIt == symIt->second.end()) continue;
        }
        const BuildFrame& bf = frameIt->second;
        if (bf.vertexStart + bf.vertexCount > anim.vertices.size()) continue;

        GLuint currentTex = 0;
        bool drawing = false;
        for (uint32_t i = 0; i < bf.vertexCount; ++i) {
            const AnimVertex& v = anim.vertices[bf.vertexStart + i];
            int atlasIndex = static_cast<int>(std::round(v.atlas));
            if (atlasIndex < 0 || atlasIndex >= static_cast<int>(anim.atlases.size())) continue;
            GLuint tex = anim.atlases[atlasIndex].glId;
            if (tex != currentTex) {
                if (drawing) glEnd();
                currentTex = tex;
                glBindTexture(GL_TEXTURE_2D, tex);
                glBegin(GL_TRIANGLES);
                drawing = true;
            }

            float x = el.a * v.x + el.c * v.y + el.tx;
            float y = -(el.b * v.x + el.d * v.y + el.ty);
            glTexCoord2f(v.u, v.v);
            glVertex2f(x, y);
        }
        if (drawing) glEnd();
    }
}

void Renderer::DrawEntity(Entity* e) {
    if (!e || !e->visible) return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    ApplyTransform(e, e->parent == nullptr);

    if (e->animWidget && e->uiTransform) {
        DrawAnim(e);
    }

    if (e->imageWidget && e->uiTransform) {
        const auto& img = *e->imageWidget;
        const auto& t = *e->uiTransform;

        const Atlas* atlas = !img.atlas.empty() ? GetAtlas(img.atlas) : nullptr;
        const LoadedTexture* tex = !img.atlas.empty() ? GetTextureFromAtlas(img.atlas) : nullptr;

        if (atlas && tex && tex->glId && !img.textureName.empty()) {
            auto eIt = atlas->elements.find(img.textureName);
            if (eIt != atlas->elements.end()) {
                const AtlasElement& region = eIt->second;

                float w, h;
                if (img.customSize) {
                    w = static_cast<float>(img.width);
                    h = static_cast<float>(img.height);
                } else {
                    w = (region.u2 - region.u1) * tex->width;
                    h = (region.v2 - region.v1) * tex->height;
                }

                if (t.scaleMode == ScaleMode::FillScreen) {
                    w = static_cast<float>(vpW);
                    h = static_cast<float>(vpH);
                }

                float hx = (img.hRegPoint == Anchor::Middle) ? -w * 0.5f
                          : (img.hRegPoint == Anchor::Min) ? 0.0f : -w;
                float hy = (img.vRegPoint == Anchor::Middle) ? -h * 0.5f
                          : (img.vRegPoint == Anchor::Min) ? -h : 0.0f;

                glBindTexture(GL_TEXTURE_2D, tex->glId);
                glColor4f(img.tintR, img.tintG, img.tintB, img.tintA);
                glBegin(GL_QUADS);
                glTexCoord2f(region.u1, region.v1); glVertex2f(hx,     hy);
                glTexCoord2f(region.u2, region.v1); glVertex2f(hx + w, hy);
                glTexCoord2f(region.u2, region.v2); glVertex2f(hx + w, hy + h);
                glTexCoord2f(region.u1, region.v2); glVertex2f(hx,     hy + h);
                glEnd();
            }
        }
    }

    if (e->textWidget && e->uiTransform && !e->textWidget->text.empty()) {
        const auto& tw = *e->textWidget;
        BakedFont* bf = fonts.Get(tw.font, tw.size);
        if (bf && bf->glTexId) {
            float totalWidth = 0;
            for (char c : tw.text) {
                int ch = static_cast<unsigned char>(c);
                auto it = bf->glyphs.find(ch);
                if (it == bf->glyphs.end()) it = bf->glyphs.find('?');
                if (it != bf->glyphs.end()) totalWidth += it->second.xadvance * bf->bitmapScale;
            }

            float cursorX = -totalWidth * 0.5f;
            // Center text vertically around the widget origin. ascent is the
            // distance from baseline to top of caps; placing the baseline at
            // -ascent/2 makes the caps' midpoint sit on Y=0.
            float baselineY = -bf->ascent * 0.5f;

            glBindTexture(GL_TEXTURE_2D, bf->glTexId);
            glColor4f(tw.r, tw.g, tw.b, tw.a);

            glBegin(GL_QUADS);
            float invW = 1.0f / bf->atlasW;
            float invH = 1.0f / bf->atlasH;
            for (char c : tw.text) {
                int ch = static_cast<unsigned char>(c);
                auto it = bf->glyphs.find(ch);
                if (it == bf->glyphs.end()) it = bf->glyphs.find('?');
                if (it == bf->glyphs.end()) continue;
                const auto& g = it->second;

                float x0 = cursorX + g.xoff * bf->bitmapScale;
                float y0 = baselineY - g.yoff * bf->bitmapScale;
                float x1 = x0 + (g.x1 - g.x0) * bf->bitmapScale;
                float y1 = y0 - (g.y1 - g.y0) * bf->bitmapScale;
                float u0 = g.x0 * invW;
                float v0 = 1.0f - g.y0 * invH;
                float u1 = g.x1 * invW;
                float v1 = 1.0f - g.y1 * invH;

                glTexCoord2f(u0, v0); glVertex2f(x0, y0);
                glTexCoord2f(u1, v0); glVertex2f(x1, y0);
                glTexCoord2f(u1, v1); glVertex2f(x1, y1);
                glTexCoord2f(u0, v1); glVertex2f(x0, y1);

                cursorX += g.xadvance * bf->bitmapScale;
            }
            glEnd();
        }
    }

    for (Entity* child : e->children) {
        DrawEntity(child);
    }

    glPopMatrix();
}

void Renderer::DrawRoot(Entity* root) {
    if (!root) return;
    DrawEntity(root);
}

void Renderer::DrawAllOrphans(const std::vector<EntityPtr>& entities) {
    for (const auto& e : entities) {
        if (e->parent) continue;
        if (!e->imageWidget && !e->textWidget && e->children.empty()) continue;
        DrawEntity(e.get());
    }
}
