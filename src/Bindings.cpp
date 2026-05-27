#include "Engine.h"
#include "LuaHost.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr const char* kEntityMeta = "OpenDS.Entity";
constexpr const char* kImageMeta = "OpenDS.ImageWidget";
constexpr const char* kUITransformMeta = "OpenDS.UITransform";
constexpr const char* kTextMeta = "OpenDS.TextWidget";
constexpr const char* kAnimMeta = "OpenDS.AnimState";

struct EntityHandle { EntityPtr entity; };
struct ComponentHandle { EntityPtr entity; };

int LuaAbsIndex(lua_State* L, int idx) {
    if (idx > 0 || idx <= LUA_REGISTRYINDEX) return idx;
    return lua_gettop(L) + idx + 1;
}

Engine* GetEngine(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "OpenDS.Engine");
    Engine* e = static_cast<Engine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return e;
}

fs::path ResolveDataPath(Engine* eng, const std::string& rel) {
    fs::path p(rel);
    if (p.is_absolute()) return fs::exists(p) ? p : fs::path();
    const char* dlcs[] = {"DLC0003", "DLC0002", "DLC0001"};
    for (const char* dlc : dlcs) {
        fs::path candidate = fs::path(eng->DataRoot()) / dlc / p;
        if (fs::exists(candidate)) return candidate;
    }
    fs::path candidate = fs::path(eng->DataRoot()) / p;
    return fs::exists(candidate) ? candidate : fs::path();
}

float BindingScaleModeFactor(const UITransform& t, int vpW, int vpH) {
    if (t.scaleMode != ScaleMode::Proportional && t.scaleMode != ScaleMode::FixedProportional) {
        return 1.0f;
    }
    float s = std::min(vpW / 1280.0f, vpH / 720.0f);
    if (t.maxPropUpscale > 0.0f) s = std::min(s, t.maxPropUpscale);
    return s;
}

EntityPtr CheckEntity(lua_State* L, int idx) {
    auto* h = static_cast<EntityHandle*>(luaL_checkudata(L, idx, kEntityMeta));
    return h->entity;
}

EntityPtr OptEntity(lua_State* L, int idx) {
    idx = LuaAbsIndex(L, idx);
    if (lua_isnil(L, idx) || lua_isnone(L, idx)) return nullptr;
    void* p = lua_touserdata(L, idx);
    if (!p) return nullptr;
    if (!lua_getmetatable(L, idx)) return nullptr;
    luaL_getmetatable(L, kEntityMeta);
    int eq = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!eq) return nullptr;
    return static_cast<EntityHandle*>(p)->entity;
}

EntityPtr CheckComp(lua_State* L, int idx, const char* meta) {
    auto* h = static_cast<ComponentHandle*>(luaL_checkudata(L, idx, meta));
    return h->entity;
}

void PushEntity(lua_State* L, EntityPtr e) {
    auto* h = static_cast<EntityHandle*>(lua_newuserdata(L, sizeof(EntityHandle)));
    new (h) EntityHandle{e};
    luaL_getmetatable(L, kEntityMeta);
    lua_setmetatable(L, -2);
}

void PushComp(lua_State* L, EntityPtr e, const char* meta) {
    auto* h = static_cast<ComponentHandle*>(lua_newuserdata(L, sizeof(ComponentHandle)));
    new (h) ComponentHandle{e};
    luaL_getmetatable(L, meta);
    lua_setmetatable(L, -2);
}

int EntityGc(lua_State* L) {
    auto* h = static_cast<EntityHandle*>(lua_touserdata(L, 1));
    if (h) h->~EntityHandle();
    return 0;
}

int CompGc(lua_State* L) {
    auto* h = static_cast<ComponentHandle*>(lua_touserdata(L, 1));
    if (h) h->~ComponentHandle();
    return 0;
}

int NoOp(lua_State*) { return 0; }
int NoOpReturnSelf(lua_State* L) { lua_pushvalue(L, 1); return 1; }

int Entity_SetName(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (lua_isstring(L, 2)) e->name = lua_tostring(L, 2);
    return 0;
}

int Entity_AddUITransform(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (!e->uiTransform) e->uiTransform = std::make_unique<UITransform>();
    PushComp(L, e, kUITransformMeta);
    return 1;
}

int Entity_AddTransform(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (!e->uiTransform) e->uiTransform = std::make_unique<UITransform>();
    PushComp(L, e, kUITransformMeta);
    return 1;
}

int Entity_AddImageWidget(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (!e->imageWidget) e->imageWidget = std::make_unique<ImageWidget>();
    PushComp(L, e, kImageMeta);
    return 1;
}

int Entity_AddTextWidget(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (!e->textWidget) e->textWidget = std::make_unique<TextWidget>();
    PushComp(L, e, kTextMeta);
    return 1;
}

int Entity_AddAnimState(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    if (!e->animWidget) e->animWidget = std::make_unique<AnimWidget>();
    PushComp(L, e, kAnimMeta);
    return 1;
}

int Entity_SetParent(lua_State* L) {
    EntityPtr child = CheckEntity(L, 1);
    EntityPtr parent = OptEntity(L, 2);
    Engine* eng = GetEngine(L);
    eng->Entities().SetParent(child.get(), parent ? parent.get() : nullptr);
    return 0;
}

int Entity_Hide(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    e->visible = false;
    return 0;
}

int Entity_Show(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    e->visible = true;
    return 0;
}

int Entity_MoveToFront(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    GetEngine(L)->Entities().MoveToFront(e.get());
    return 0;
}

int Entity_MoveToBack(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    GetEngine(L)->Entities().MoveToBack(e.get());
    return 0;
}

int Entity_SetClickable(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    e->clickable = lua_toboolean(L, 2) != 0;
    return 0;
}

int Entity_GetGUID(lua_State* L) {
    EntityPtr e = CheckEntity(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(e.get())));
    return 1;
}

int Image_SetTexture(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    if (!e->imageWidget) return 0;
    if (lua_isstring(L, 2)) e->imageWidget->atlas = lua_tostring(L, 2);
    if (lua_isstring(L, 3)) e->imageWidget->textureName = lua_tostring(L, 3);
    return 0;
}

int Image_SetSize(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->width = static_cast<int>(luaL_checknumber(L, 2));
    e->imageWidget->height = static_cast<int>(luaL_checknumber(L, 3));
    e->imageWidget->customSize = true;
    return 0;
}

int Image_GetSize(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    int w = e->imageWidget->width;
    int h = e->imageWidget->height;
    if (w == 0 || h == 0) {
        Engine* eng = GetEngine(L);
        const Atlas* a = eng->GetRenderer().GetAtlas(e->imageWidget->atlas);
        const LoadedTexture* tex = eng->GetRenderer().GetTextureFromAtlas(e->imageWidget->atlas);
        if (a && tex) {
            auto it = a->elements.find(e->imageWidget->textureName);
            if (it != a->elements.end()) {
                if (w == 0) w = static_cast<int>((it->second.u2 - it->second.u1) * tex->width);
                if (h == 0) h = static_cast<int>((it->second.v2 - it->second.v1) * tex->height);
            }
        }
    }
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

int Image_SetTint(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->tintR = static_cast<float>(luaL_checknumber(L, 2));
    e->imageWidget->tintG = static_cast<float>(luaL_checknumber(L, 3));
    e->imageWidget->tintB = static_cast<float>(luaL_checknumber(L, 4));
    e->imageWidget->tintA = static_cast<float>(luaL_checknumber(L, 5));
    return 0;
}

int Image_SetHAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->hRegPoint = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int Image_SetVAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->vRegPoint = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int Image_SetBlendMode(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->blend = static_cast<BlendMode>(luaL_optinteger(L, 2, 0));
    return 0;
}

int Image_SetUVScale(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kImageMeta);
    e->imageWidget->uvScaleX = static_cast<float>(luaL_checknumber(L, 2));
    e->imageWidget->uvScaleY = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int UI_SetPosition(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->x = static_cast<float>(luaL_checknumber(L, 2));
    e->uiTransform->y = static_cast<float>(luaL_checknumber(L, 3));
    if (lua_isnumber(L, 4)) e->uiTransform->z = static_cast<float>(lua_tonumber(L, 4));
    return 0;
}

int UI_SetScale(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->scaleX = static_cast<float>(luaL_checknumber(L, 2));
    e->uiTransform->scaleY = static_cast<float>(luaL_optnumber(L, 3, lua_tonumber(L, 2)));
    e->uiTransform->scaleZ = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    return 0;
}

int UI_GetScale(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    lua_pushnumber(L, e->uiTransform->scaleX);
    lua_pushnumber(L, e->uiTransform->scaleY);
    lua_pushnumber(L, e->uiTransform->scaleZ);
    return 3;
}

int UI_SetRotation(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->rotation = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int UI_GetWorldPosition(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    float x = e->uiTransform->x, y = e->uiTransform->y, z = e->uiTransform->z;
    Entity* p = e->parent;
    while (p) {
        if (p->uiTransform) {
            Engine* eng = GetEngine(L);
            float modeScale = BindingScaleModeFactor(*p->uiTransform, eng->Width(), eng->Height());
            x = x * p->uiTransform->scaleX * modeScale + p->uiTransform->x;
            y = y * p->uiTransform->scaleY * modeScale + p->uiTransform->y;
        }
        p = p->parent;
    }
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

int UI_GetLocalPosition(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    lua_pushnumber(L, e->uiTransform->x);
    lua_pushnumber(L, e->uiTransform->y);
    lua_pushnumber(L, e->uiTransform->z);
    return 3;
}

int UI_SetVAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->vAnchor = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int UI_SetHAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->hAnchor = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int UI_SetScaleMode(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->scaleMode = static_cast<ScaleMode>(luaL_checkinteger(L, 2));
    return 0;
}

int UI_SetMaxPropUpscale(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kUITransformMeta);
    e->uiTransform->maxPropUpscale = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int Text_SetFont(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    if (lua_isstring(L, 2)) e->textWidget->font = lua_tostring(L, 2);
    return 0;
}

int Text_SetSize(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->size = static_cast<int>(luaL_checknumber(L, 2));
    return 0;
}

int Text_SetString(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    if (lua_isstring(L, 2)) e->textWidget->text = lua_tostring(L, 2);
    else if (lua_isnumber(L, 2)) e->textWidget->text = lua_tostring(L, 2);
    else e->textWidget->text = "";
    return 0;
}

int Text_GetString(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    lua_pushstring(L, e->textWidget->text.c_str());
    return 1;
}

int Text_SetColour(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->r = static_cast<float>(luaL_checknumber(L, 2));
    e->textWidget->g = static_cast<float>(luaL_checknumber(L, 3));
    e->textWidget->b = static_cast<float>(luaL_checknumber(L, 4));
    e->textWidget->a = static_cast<float>(luaL_checknumber(L, 5));
    return 0;
}

int Text_SetRegionSize(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->regionW = static_cast<float>(luaL_checknumber(L, 2));
    e->textWidget->regionH = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int Text_GetRegionSize(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    lua_pushnumber(L, e->textWidget->regionW);
    lua_pushnumber(L, e->textWidget->regionH);
    return 2;
}

int Text_SetHAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->hAnchor = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int Text_SetVAnchor(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->vAnchor = static_cast<Anchor>(luaL_checkinteger(L, 2));
    return 0;
}

int Text_EnableWordWrap(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->wordWrap = lua_toboolean(L, 2) != 0;
    return 0;
}

int Text_SetHorizontalSqueeze(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kTextMeta);
    e->textWidget->hsqueeze = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int Anim_SetBank(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kAnimMeta);
    if (lua_isstring(L, 2)) e->animWidget->bank = lua_tostring(L, 2);
    return 0;
}

int Anim_SetBuild(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kAnimMeta);
    if (lua_isstring(L, 2)) e->animWidget->build = lua_tostring(L, 2);
    return 0;
}

int Anim_PlayAnimation(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kAnimMeta);
    std::string name = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    bool loop = lua_toboolean(L, 3) != 0;
    if (name == e->animWidget->anim && loop == e->animWidget->loop) {
        // Same anim with same loop flag is already playing — don't reset time.
        return 0;
    }
    e->animWidget->anim = name;
    e->animWidget->loop = loop;
    e->animWidget->time = 0.0f;
    e->animWidget->queue.clear();
    return 0;
}

int Anim_PushAnimation(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kAnimMeta);
    std::string name = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    bool loop = lua_toboolean(L, 3) != 0;
    // If nothing is playing, behave like Play. Otherwise queue.
    if (e->animWidget->anim.empty()) {
        e->animWidget->anim = name;
        e->animWidget->loop = loop;
        e->animWidget->time = 0.0f;
        return 0;
    }
    if (name == e->animWidget->anim && loop == e->animWidget->loop) {
        return 0;  // already playing
    }
    e->animWidget->queue.push_back({name, loop});
    return 0;
}

int Anim_SetPercent(lua_State* L) {
    EntityPtr e = CheckComp(L, 1, kAnimMeta);
    if (lua_isstring(L, 2)) e->animWidget->anim = lua_tostring(L, 2);
    float pct = static_cast<float>(luaL_checknumber(L, 3));
    if (pct < 0) pct = 0;
    e->animWidget->time = pct;  // engine treats this as time; with frameRate baked it's a normalized cue
    return 0;
}

int L_CreateEntity(lua_State* L) {
    Engine* eng = GetEngine(L);
    EntityPtr e = eng->Entities().Create();
    PushEntity(L, e);
    return 1;
}

int L_KleiLoadLua(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    Engine* eng = GetEngine(L);
    fs::path candidate = ResolveDataPath(eng, filename);
    if (candidate.empty()) {
        lua_pushnil(L);
        return 1;
    }
    if (luaL_loadfile(L, candidate.string().c_str()) != 0) {
        std::fprintf(stderr, "kleiloadlua %s: %s\n", filename, lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }
    return 1;
}

int L_ResolveFilePath(lua_State* L) {
    const char* p = luaL_checkstring(L, 1);
    lua_pushstring(L, p);
    return 1;
}

int L_KleiFileExists(lua_State* L) {
    const char* p = luaL_checkstring(L, 1);
    Engine* eng = GetEngine(L);
    fs::path candidate = ResolveDataPath(eng, p);
    lua_pushboolean(L, !candidate.empty() ? 1 : 0);
    return 1;
}

int L_RequestShutdown(lua_State* L) {
    GetEngine(L)->RequestQuit();
    return 0;
}

int L_GetRealTime(lua_State* L) {
    lua_pushnumber(L, static_cast<double>(SDL_GetTicks()) / 1000.0);
    return 1;
}

int L_GetMousePos(lua_State* L) {
    Engine* eng = GetEngine(L);
    lua_pushnumber(L, eng->GetInput().MouseX());
    lua_pushnumber(L, eng->GetInput().MouseY());
    return 2;
}

int L_FrontEndPush(lua_State* L) {
    Engine* eng = GetEngine(L);
    if (!luaL_checkudata(L, 1, kEntityMeta)) return 0;
    EntityPtr e = CheckEntity(L, 1);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    eng->GetFrontEnd().Push(e.get(), ref);
    return 0;
}

int L_FrontEndPop(lua_State* L) {
    Engine* eng = GetEngine(L);
    int ref = eng->GetFrontEnd().TopLuaRef();
    eng->GetFrontEnd().Pop();
    if (ref >= 0) luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return 0;
}

int L_FrontEndClear(lua_State* L) {
    Engine* eng = GetEngine(L);
    while (!eng->GetFrontEnd().Empty()) {
        int ref = eng->GetFrontEnd().TopLuaRef();
        eng->GetFrontEnd().Pop();
        if (ref >= 0) luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    return 0;
}

int L_ScreenSize(lua_State* L) {
    Engine* eng = GetEngine(L);
    lua_pushnumber(L, eng->Width());
    lua_pushnumber(L, eng->Height());
    return 2;
}

int L_HitTestImage(lua_State* L) {
    if (!luaL_checkudata(L, 1, kEntityMeta)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    EntityPtr e = CheckEntity(L, 1);
    float mx = static_cast<float>(luaL_checknumber(L, 2));
    float my = static_cast<float>(luaL_checknumber(L, 3));

    // Don't filter by e->clickable: DS marks decorative children (e.g.
    // savetile.portraitbg / portrait) as not-clickable, but clicks on them
    // should still bubble up to the parent widget's OnControl. clickable=false
    // means "don't fire MY own click handler", not "ignore me for hit-testing".
    if (!e->imageWidget || !e->uiTransform || !e->visible) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Engine* eng = GetEngine(L);
    float px = 0, py = 0;
    float sx = 1, sy = 1;
    Entity* cur = e.get();
    while (cur) {
        if (cur->uiTransform) {
            float modeScale = BindingScaleModeFactor(*cur->uiTransform, eng->Width(), eng->Height());
            float csx = cur->uiTransform->scaleX * modeScale;
            float csy = cur->uiTransform->scaleY * modeScale;
            px = px * csx + cur->uiTransform->x;
            py = py * csy + cur->uiTransform->y;
            sx *= csx;
            sy *= csy;
            if (!cur->parent) {
                if (cur->uiTransform->hAnchor == Anchor::Min) px -= eng->Width() * 0.5f;
                else if (cur->uiTransform->hAnchor == Anchor::Max) px += eng->Width() * 0.5f;
                if (cur->uiTransform->vAnchor == Anchor::Min) py += eng->Height() * 0.5f;
                else if (cur->uiTransform->vAnchor == Anchor::Max) py -= eng->Height() * 0.5f;
            }
        }
        cur = cur->parent;
    }

    float w = static_cast<float>(e->imageWidget->width);
    float h = static_cast<float>(e->imageWidget->height);
    if (w == 0 || h == 0) {
        const Atlas* a = eng->GetRenderer().GetAtlas(e->imageWidget->atlas);
        const LoadedTexture* tex = eng->GetRenderer().GetTextureFromAtlas(e->imageWidget->atlas);
        if (a && tex) {
            auto it = a->elements.find(e->imageWidget->textureName);
            if (it != a->elements.end()) {
                if (w == 0) w = (it->second.u2 - it->second.u1) * tex->width;
                if (h == 0) h = (it->second.v2 - it->second.v1) * tex->height;
            }
        }
    }
    w *= sx; h *= sy;

    float hx = (e->imageWidget->hRegPoint == Anchor::Middle) ? -w * 0.5f
              : (e->imageWidget->hRegPoint == Anchor::Min) ? 0.0f : -w;
    float hy = (e->imageWidget->vRegPoint == Anchor::Middle) ? -h * 0.5f
              : (e->imageWidget->vRegPoint == Anchor::Min) ? -h : 0.0f;

    float x0 = px + hx;
    float y0 = py + hy;
    float x1 = x0 + w;
    float y1 = y0 + h;
    if (x1 < x0) std::swap(x0, x1);
    if (y1 < y0) std::swap(y0, y1);
    bool hit = mx >= x0 && mx <= x1 && my >= y0 && my <= y1;
    lua_pushboolean(L, hit ? 1 : 0);
    return 1;
}

void RegisterMeta(lua_State* L, const char* name, const luaL_Reg* methods, lua_CFunction gc) {
    luaL_newmetatable(L, name);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    for (const luaL_Reg* m = methods; m->name; ++m) {
        lua_pushcfunction(L, m->func);
        lua_setfield(L, -2, m->name);
    }
    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

}

void RegisterBindings(LuaHost* host) {
    lua_State* L = host->L();
    lua_pushlightuserdata(L, host->GetEngine());
    lua_setfield(L, LUA_REGISTRYINDEX, "OpenDS.Engine");

    static const luaL_Reg entityMethods[] = {
        {"SetName", Entity_SetName},
        {"AddUITransform", Entity_AddUITransform},
        {"AddTransform", Entity_AddTransform},
        {"AddImageWidget", Entity_AddImageWidget},
        {"AddTextWidget", Entity_AddTextWidget},
        {"AddAnimState", Entity_AddAnimState},
        {"SetParent", Entity_SetParent},
        {"Hide", Entity_Hide},
        {"Show", Entity_Show},
        {"MoveToFront", Entity_MoveToFront},
        {"MoveToBack", Entity_MoveToBack},
        {"SetClickable", Entity_SetClickable},
        {"GetGUID", Entity_GetGUID},
        {"CallPrefabConstructionComplete", NoOp},
        {"SetCanSleep", NoOp},
        {"Remove", NoOp},
        {nullptr, nullptr}
    };
    RegisterMeta(L, kEntityMeta, entityMethods, EntityGc);

    static const luaL_Reg imageMethods[] = {
        {"SetTexture", Image_SetTexture},
        {"SetSize", Image_SetSize},
        {"GetSize", Image_GetSize},
        {"SetTint", Image_SetTint},
        {"SetHAnchor", Image_SetHAnchor},
        {"SetVAnchor", Image_SetVAnchor},
        {"SetBlendMode", Image_SetBlendMode},
        {"SetUVScale", Image_SetUVScale},
        {"SetAlphaRange", NoOp},
        {"SetEffect", NoOp},
        {"SetEffectParams", NoOp},
        {"EnableEffectParams", NoOp},
        {nullptr, nullptr}
    };
    RegisterMeta(L, kImageMeta, imageMethods, CompGc);

    static const luaL_Reg uiMethods[] = {
        {"SetPosition", UI_SetPosition},
        {"SetScale", UI_SetScale},
        {"GetScale", UI_GetScale},
        {"SetRotation", UI_SetRotation},
        {"GetWorldPosition", UI_GetWorldPosition},
        {"GetLocalPosition", UI_GetLocalPosition},
        {"SetVAnchor", UI_SetVAnchor},
        {"SetHAnchor", UI_SetHAnchor},
        {"SetScaleMode", UI_SetScaleMode},
        {"SetMaxPropUpscale", UI_SetMaxPropUpscale},
        {"UpdateTransform", NoOp},
        {nullptr, nullptr}
    };
    RegisterMeta(L, kUITransformMeta, uiMethods, CompGc);

    static const luaL_Reg textMethods[] = {
        {"SetFont", Text_SetFont},
        {"SetSize", Text_SetSize},
        {"SetString", Text_SetString},
        {"GetString", Text_GetString},
        {"SetColour", Text_SetColour},
        {"SetColor", Text_SetColour},
        {"SetRegionSize", Text_SetRegionSize},
        {"GetRegionSize", Text_GetRegionSize},
        {"SetHAnchor", Text_SetHAnchor},
        {"SetVAnchor", Text_SetVAnchor},
        {"EnableWordWrap", Text_EnableWordWrap},
        {"SetHorizontalSqueeze", Text_SetHorizontalSqueeze},
        {"SetVerticalSqueeze", NoOp},
        {nullptr, nullptr}
    };
    RegisterMeta(L, kTextMeta, textMethods, CompGc);

    static const luaL_Reg animMethods[] = {
        {"SetBank", Anim_SetBank},
        {"SetBuild", Anim_SetBuild},
        {"PlayAnimation", Anim_PlayAnimation},
        {"PushAnimation", Anim_PushAnimation},
        {"SetPercent", Anim_SetPercent},
        {"SetRayTestOnBB", NoOp},
        {"SetMultColour", NoOp},
        {"SetTime", NoOp},
        {"AnimateWhilePaused", NoOp},
        {"Hide", NoOp},
        {"Show", NoOp},
        {nullptr, nullptr}
    };
    RegisterMeta(L, kAnimMeta, animMethods, CompGc);

    auto setG = [&](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setglobal(L, name);
    };
    setG("CreateEntity", L_CreateEntity);
    setG("kleiloadlua", L_KleiLoadLua);
    setG("resolvefilepath", L_ResolveFilePath);
    setG("kleifileexists", L_KleiFileExists);
    setG("OpenDS_RequestShutdown", L_RequestShutdown);
    setG("OpenDS_GetRealTime", L_GetRealTime);
    setG("OpenDS_GetMousePos", L_GetMousePos);
    setG("OpenDS_FrontEndPush", L_FrontEndPush);
    setG("OpenDS_FrontEndPop", L_FrontEndPop);
    setG("OpenDS_FrontEndClear", L_FrontEndClear);
    setG("OpenDS_ScreenSize", L_ScreenSize);
    setG("OpenDS_HitTestImage", L_HitTestImage);

    Engine* eng = host->GetEngine();
    lua_pushstring(L, eng->DataRoot().c_str());
    lua_setglobal(L, "DATA_ROOT");
}
