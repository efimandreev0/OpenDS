#pragma once
#include "Atlas.h"
#include "Entity.h"
#include "Font.h"
#include "KTex.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

struct LoadedAnim;

struct LoadedTexture {
    uint32_t glId = 0;
    int width = 0;
    int height = 0;
};

class Renderer {
public:
    ~Renderer();
    bool Init();
    void Shutdown();
    void BeginFrame(int viewportW, int viewportH);
    void EndFrame();

    void DrawRoot(Entity* root);
    void DrawAllOrphans(const std::vector<EntityPtr>& entities);

    const Atlas* GetAtlas(const std::string& path);
    const LoadedTexture* GetTextureFromAtlas(const std::string& atlasPath);
    std::string ResolveDataPath(const std::string& path) const;

    FontCache& Fonts() { return fonts; }

    void SetDataRoot(const std::string& root);
    const std::string& GetDataRoot() const { return dataRoot; }

private:
    std::string dataRoot;
    std::unordered_map<std::string, Atlas> atlases;
    std::unordered_map<std::string, LoadedTexture> textures;
    std::unordered_map<std::string, LoadedAnim*> animations;
    FontCache fonts;

    int vpW = 0, vpH = 0;

    LoadedTexture LoadTextureFile(const std::string& texPath);
    void DrawEntity(Entity* e);
    void DrawAnim(Entity* e);
    void ApplyTransform(Entity* e, bool topLevel);
};
