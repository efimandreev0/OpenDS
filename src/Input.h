#pragma once
#include <SDL.h>

class LuaHost;

class Input {
public:
    void Init(LuaHost* host);
    void HandleEvent(const SDL_Event& ev, int vpW, int vpH);

    float MouseX() const { return mouseX; }
    float MouseY() const { return mouseY; }

private:
    LuaHost* lua = nullptr;
    float mouseX = 0;
    float mouseY = 0;
    int lastVpW = 1280;
    int lastVpH = 720;

    void DispatchMove(float x, float y);
    void DispatchButton(int button, bool down, float x, float y);
    void DispatchKey(int key, bool down);
    void DispatchText(const char* text);
};
