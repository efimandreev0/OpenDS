#pragma once
#include "Entity.h"
#include <vector>

struct ScreenSlot {
    Entity* root = nullptr;
    int luaRef = -1;
};

class FrontEnd {
public:
    void Push(Entity* root, int luaRef);
    void Pop();
    void Replace(Entity* root, int luaRef);
    Entity* Top() const;
    int TopLuaRef() const;
    const std::vector<ScreenSlot>& Stack() const { return stack; }
    bool Empty() const { return stack.empty(); }
    void Clear();

private:
    std::vector<ScreenSlot> stack;
};
