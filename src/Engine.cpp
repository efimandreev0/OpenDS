#include "Engine.h"
#include "LuaHost.h"
#include <SDL_opengl.h>
#include <cstdio>

bool Engine::Init(const std::string& dataRoot, int w, int h) {
    width = w;
    height = h;

#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    window = SDL_CreateWindow("OpenDS",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput();

    renderer.SetDataRoot(dataRoot);
    if (!renderer.Init()) return false;

    renderer.BeginFrame(width, height);
    Entity loadingRoot;
    loadingRoot.uiTransform = std::make_unique<UITransform>();
    loadingRoot.imageWidget = std::make_unique<ImageWidget>();
    loadingRoot.imageWidget->atlas = "images/bg_loading_loading_newhorizons.xml";
    loadingRoot.imageWidget->textureName = "loading_newhorizons.tex";
    loadingRoot.uiTransform->scaleMode = ScaleMode::FillScreen;
    renderer.DrawRoot(&loadingRoot);
    renderer.EndFrame();
    SDL_GL_SwapWindow(window);

    lua = new LuaHost();
    if (!lua->Init(this)) {
        std::fprintf(stderr, "LuaHost init failed\n");
        return false;
    }
    input.Init(lua);

    if (!lua->RunBoot()) {
        std::fprintf(stderr, "boot.lua failed\n");
    }
    return true;
}

void Engine::Run() {
    running = true;
    lastTick = SDL_GetTicks();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = false; continue; }
            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    width = ev.window.data1;
                    height = ev.window.data2;
                }
            }
            input.HandleEvent(ev, width, height);
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastTick = now;

        lua->Tick(dt);
        for (const auto& e : entities.All()) {
            if (e->animWidget) e->animWidget->time += dt;
        }

        renderer.BeginFrame(width, height);
        if (frontEnd.Empty()) {
            renderer.DrawAllOrphans(entities.All());
        } else {
            for (const auto& slot : frontEnd.Stack()) {
                if (slot.root) renderer.DrawRoot(slot.root);
            }
        }
        renderer.EndFrame();

        SDL_GL_SwapWindow(window);
    }
}

void Engine::Shutdown() {
    if (lua) {
        lua->Shutdown();
        delete lua;
        lua = nullptr;
    }
    renderer.Shutdown();
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}
