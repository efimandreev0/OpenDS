#include "Input.h"
#include "LuaHost.h"

void Input::Init(LuaHost* h) {
    lua = h;
}

void Input::HandleEvent(const SDL_Event& ev, int vpW, int vpH) {
    lastVpW = vpW;
    lastVpH = vpH;
    switch (ev.type) {
        case SDL_MOUSEMOTION: {
            mouseX = static_cast<float>(ev.motion.x) - vpW * 0.5f;
            mouseY = vpH * 0.5f - static_cast<float>(ev.motion.y);
            DispatchMove(mouseX, mouseY);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            mouseX = static_cast<float>(ev.button.x) - vpW * 0.5f;
            mouseY = vpH * 0.5f - static_cast<float>(ev.button.y);
            int btn = -1;
            if (ev.button.button == SDL_BUTTON_LEFT) btn = 1000;
            else if (ev.button.button == SDL_BUTTON_RIGHT) btn = 1001;
            else if (ev.button.button == SDL_BUTTON_MIDDLE) btn = 1002;
            DispatchButton(btn, ev.type == SDL_MOUSEBUTTONDOWN, mouseX, mouseY);
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            DispatchKey(static_cast<int>(ev.key.keysym.sym), ev.type == SDL_KEYDOWN);
            break;
        case SDL_TEXTINPUT:
            DispatchText(ev.text.text);
            break;
    }
}

void Input::DispatchMove(float x, float y) {
    lua_State* L = lua->L();
    lua_getglobal(L, "OpenDS_OnMouseMove");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        std::fprintf(stderr, "OnMouseMove: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Input::DispatchButton(int btn, bool down, float x, float y) {
    lua_State* L = lua->L();
    lua_getglobal(L, "OpenDS_OnMouseButton");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_pushinteger(L, btn);
    lua_pushboolean(L, down ? 1 : 0);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    if (lua_pcall(L, 4, 0, 0) != 0) {
        std::fprintf(stderr, "OnMouseButton: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Input::DispatchKey(int key, bool down) {
    lua_State* L = lua->L();
    lua_getglobal(L, "OpenDS_OnKey");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_pushinteger(L, key);
    lua_pushboolean(L, down ? 1 : 0);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        std::fprintf(stderr, "OnKey: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void Input::DispatchText(const char* text) {
    lua_State* L = lua->L();
    lua_getglobal(L, "OpenDS_OnText");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_pushstring(L, text);
    if (lua_pcall(L, 1, 0, 0) != 0) {
        std::fprintf(stderr, "OnText: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}
