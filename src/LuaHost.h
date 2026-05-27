#pragma once
#include <string>

struct lua_State;
class Engine;

class LuaHost {
public:
    bool Init(Engine* engine);
    void Shutdown();
    bool RunBoot();
    void Tick(float dt);

    lua_State* L() const { return state; }
    Engine* GetEngine() const { return engine; }

    bool DoFile(const std::string& path);

private:
    lua_State* state = nullptr;
    Engine* engine = nullptr;
};

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

void RegisterBindings(LuaHost* host);
