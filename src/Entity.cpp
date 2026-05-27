#include "Entity.h"
#include <algorithm>

EntityPtr EntityRegistry::Create() {
    auto e = std::make_shared<Entity>();
    entities.push_back(e);
    return e;
}

void EntityRegistry::SetParent(Entity* child, Entity* newParent) {
    if (!child) return;
    if (child->parent) {
        auto& siblings = child->parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
    }
    child->parent = newParent;
    if (newParent) newParent->children.push_back(child);
}

void EntityRegistry::Detach(Entity* e) {
    SetParent(e, nullptr);
}

void EntityRegistry::MoveToFront(Entity* e) {
    if (!e || !e->parent) return;
    auto& s = e->parent->children;
    auto it = std::find(s.begin(), s.end(), e);
    if (it == s.end()) return;
    s.erase(it);
    s.push_back(e);
}

void EntityRegistry::MoveToBack(Entity* e) {
    if (!e || !e->parent) return;
    auto& s = e->parent->children;
    auto it = std::find(s.begin(), s.end(), e);
    if (it == s.end()) return;
    s.erase(it);
    s.insert(s.begin(), e);
}
