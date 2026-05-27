#include "Engine.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const char* kDefaultDataRoot = "E:/Games/Dont Starve v27.04.2023/data";

int main(int argc, char* argv[]) {
    std::string dataRoot;
    if (argc >= 2) {
        dataRoot = argv[1];
    } else if (const char* env = std::getenv("OPENDS_DATA")) {
        dataRoot = env;
    } else {
        dataRoot = kDefaultDataRoot;
    }

    if (dataRoot.empty() || !fs::exists(dataRoot)) {
        std::fprintf(stderr, "Data root '%s' does not exist.\n", dataRoot.c_str());
        return 1;
    }
    if (!fs::exists(fs::path(dataRoot) / "scripts" / "main.lua")) {
        std::fprintf(stderr, "No scripts/main.lua inside '%s'\n", dataRoot.c_str());
        return 1;
    }

    Engine engine;
    if (!engine.Init(dataRoot, 1280, 720)) {
        engine.Shutdown();
        return 1;
    }
    engine.Run();
    engine.Shutdown();
    return 0;
}
