#pragma once
#include "Entity.h"
#include "FrontEnd.h"
#include "Input.h"
#include "Renderer.h"
#include <SDL.h>
#include <string>

class LuaHost;

class Engine {
public:
    bool Init(const std::string& dataRoot, int width, int height);
    void Run();
    void Shutdown();
    void RequestQuit() { running = false; }

    EntityRegistry& Entities() { return entities; }
    Renderer& GetRenderer() { return renderer; }
    FrontEnd& GetFrontEnd() { return frontEnd; }
    Input& GetInput() { return input; }
    LuaHost* Lua() { return lua; }
    const std::string& DataRoot() const { return renderer.GetDataRoot(); }
    int Width() const { return width; }
    int Height() const { return height; }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    Renderer renderer;
    EntityRegistry entities;
    FrontEnd frontEnd;
    Input input;
    LuaHost* lua = nullptr;
    int width = 1280, height = 720;
    bool running = false;
    Uint32 lastTick = 0;
};
