#include "FrontEnd.h"

void FrontEnd::Push(Entity* root, int ref) {
    stack.push_back({root, ref});
}

void FrontEnd::Pop() {
    if (!stack.empty()) stack.pop_back();
}

void FrontEnd::Replace(Entity* root, int ref) {
    stack.clear();
    stack.push_back({root, ref});
}

Entity* FrontEnd::Top() const {
    return stack.empty() ? nullptr : stack.back().root;
}

int FrontEnd::TopLuaRef() const {
    return stack.empty() ? -1 : stack.back().luaRef;
}

void FrontEnd::Clear() {
    stack.clear();
}
