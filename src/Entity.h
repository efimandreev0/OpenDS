#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class ScaleMode : int {
    None = 0,
    FillScreen = 1,
    Proportional = 2,
    FixedProportional = 3,
};

enum class Anchor : int {
    Middle = 0,
    Min = 1,
    Max = 2,
};

enum class BlendMode : int {
    Normal = 0,
    Premultiplied = 1,
    Additive = 2,
};

struct UITransform {
    float x = 0, y = 0, z = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    float rotation = 0;
    Anchor vAnchor = Anchor::Middle;
    Anchor hAnchor = Anchor::Middle;
    ScaleMode scaleMode = ScaleMode::None;
    float maxPropUpscale = 1.0f;
};

struct ImageWidget {
    std::string atlas;
    std::string textureName;
    int width = 0;
    int height = 0;
    float tintR = 1, tintG = 1, tintB = 1, tintA = 1;
    Anchor hRegPoint = Anchor::Middle;  // horizontal: Min=left, Max=right
    Anchor vRegPoint = Anchor::Middle;  // vertical: Min=top, Max=bottom
    BlendMode blend = BlendMode::Normal;
    float uvScaleX = 1.0f;
    float uvScaleY = 1.0f;
    bool customSize = false;
    bool clickable = true;
};

struct TextWidget {
    std::string text;
    std::string font;
    int size = 24;
    float r = 1, g = 1, b = 1, a = 1;
    float regionW = 0;
    float regionH = 0;
    Anchor hAnchor = Anchor::Middle;
    Anchor vAnchor = Anchor::Middle;
    bool wordWrap = false;
    float hsqueeze = 1.0f;
};

struct AnimWidget {
    struct QueuedAnim {
        std::string name;
        bool loop = false;
    };

    std::string bank;
    std::string build;
    std::string anim;
    bool loop = false;
    float time = 0.0f;
    std::vector<QueuedAnim> queue;
};

struct Entity : std::enable_shared_from_this<Entity> {
    std::string name;
    std::unordered_set<std::string> tags;

    std::unique_ptr<UITransform> uiTransform;
    std::unique_ptr<ImageWidget> imageWidget;
    std::unique_ptr<TextWidget> textWidget;
    std::unique_ptr<AnimWidget> animWidget;

    Entity* parent = nullptr;
    std::vector<Entity*> children;
    bool visible = true;
    bool clickable = true;
    int sortOrder = 0;

    void AddTag(const std::string& t) { tags.insert(t); }
    bool HasTag(const std::string& t) const { return tags.count(t) != 0; }
};

using EntityPtr = std::shared_ptr<Entity>;

class EntityRegistry {
public:
    EntityPtr Create();
    const std::vector<EntityPtr>& All() const { return entities; }
    void SetParent(Entity* child, Entity* newParent);
    void Detach(Entity* e);
    void MoveToFront(Entity* e);
    void MoveToBack(Entity* e);
private:
    std::vector<EntityPtr> entities;
};
