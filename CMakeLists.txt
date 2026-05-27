cmake_minimum_required(VERSION 3.20)
project(OpenDS CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W3 /wd4244 /wd4267 /wd4996)
else()
    add_compile_options(-Wall -Wno-unused-parameter -Wno-unused-function)
endif()

file(GLOB LUA_SOURCES "${CMAKE_SOURCE_DIR}/lib/lua5.1/*.c")
list(REMOVE_ITEM LUA_SOURCES
        "${CMAKE_SOURCE_DIR}/lib/lua5.1/lua.c"
        "${CMAKE_SOURCE_DIR}/lib/lua5.1/luac.c"
        "${CMAKE_SOURCE_DIR}/lib/lua5.1/print.c")

add_library(lua51 STATIC ${LUA_SOURCES})
target_include_directories(lua51 PUBLIC "${CMAKE_SOURCE_DIR}/lib/lua5.1")
if(NOT WIN32)
    target_compile_definitions(lua51 PUBLIC LUA_USE_LINUX)
    target_link_libraries(lua51 PUBLIC dl m)
endif()

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.7
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

find_package(OpenGL REQUIRED)

add_executable(OpenDS
        src/main.cpp
        src/Engine.cpp
        src/KTex.cpp
        src/Zip.cpp
        src/Atlas.cpp
        src/Entity.cpp
        src/Renderer.cpp
        src/Font.cpp
        src/LuaHost.cpp
        src/Bindings.cpp
        src/FrontEnd.cpp
        src/Input.cpp
        lib/tinyxml2/tinyxml2.cpp
)

target_include_directories(OpenDS PRIVATE src "${CMAKE_SOURCE_DIR}/lib/stb" "${CMAKE_SOURCE_DIR}/lib/tinyxml2")
target_link_libraries(OpenDS PRIVATE lua51 SDL2main SDL2-static OpenGL::GL)

if(WIN32)
    target_link_libraries(OpenDS PRIVATE opengl32)
    set_target_properties(OpenDS PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set(OPENDS_LUA_ASSETS
    boot.lua
    engine_globals.lua
    entity_wrap.lua
    frontend_lite.lua
    ds_loader.lua
    push_mainscreen.lua
    input_dispatch.lua
)
foreach(asset ${OPENDS_LUA_ASSETS})
    list(APPEND OPENDS_LUA_ASSET_SOURCES "${CMAKE_SOURCE_DIR}/assets/${asset}")
    list(APPEND OPENDS_LUA_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/${asset}"
        "$<TARGET_FILE_DIR:OpenDS>/${asset}")
endforeach()

add_custom_target(OpenDSLuaAssets ALL
    ${OPENDS_LUA_COPY_COMMANDS}
    DEPENDS ${OPENDS_LUA_ASSET_SOURCES})
add_dependencies(OpenDSLuaAssets OpenDS)
