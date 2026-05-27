#include "LuaHost.h"
#include "Engine.h"
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

bool LuaHost::Init(Engine* eng) {
    engine = eng;
    state = luaL_newstate();
    if (!state) return false;
    luaL_openlibs(state);
    RegisterBindings(this);
    return true;
}

void LuaHost::Shutdown() {
    if (state) {
        lua_close(state);
        state = nullptr;
    }
}

bool LuaHost::DoFile(const std::string& path) {
    if (luaL_dofile(state, path.c_str()) != 0) {
        std::fprintf(stderr, "Lua error: %s\n", lua_tostring(state, -1));
        lua_pop(state, 1);
        return false;
    }
    return true;
}

bool LuaHost::RunBoot() {
    return DoFile("boot.lua");
}

void LuaHost::Tick(float dt) {
    lua_getglobal(state, "Tick");
    if (lua_isfunction(state, -1)) {
        lua_pushnumber(state, dt);
        if (lua_pcall(state, 1, 0, 0) != 0) {
            std::fprintf(stderr, "Tick error: %s\n", lua_tostring(state, -1));
            lua_pop(state, 1);
        }
    } else {
        lua_pop(state, 1);
    }
}
