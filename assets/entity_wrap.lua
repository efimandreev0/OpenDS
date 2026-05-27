local _native_CreateEntity = CreateEntity

local entToInst = setmetatable({}, {__mode = "k"})

local entMeta = debug.getregistry()["OpenDS.Entity"]

local function bind(componentName)
    local orig = entMeta[componentName]
    entMeta["Add" .. componentName] = function(self, ...)
        local addFn = entMeta["Add" .. componentName .. "_raw"]
        local result = addFn(self, ...)
        local inst = entToInst[self]
        if inst then inst[componentName] = result end
        return result
    end
end

local function wrapAdd(addName, componentName)
    local orig = entMeta[addName]
    entMeta[addName .. "_raw"] = orig
    entMeta[addName] = function(self, ...)
        local comp = orig(self, ...)
        local inst = entToInst[self]
        if inst then inst[componentName] = comp end
        return comp
    end
end

wrapAdd("AddUITransform", "UITransform")
wrapAdd("AddTransform", "Transform")
wrapAdd("AddImageWidget", "ImageWidget")
wrapAdd("AddTextWidget", "TextWidget")
wrapAdd("AddAnimState", "AnimState")

local subManagerStub = function(name)
    return function(self)
        local stub = autostub(name)
        local inst = entToInst[self]
        if inst then inst[name] = stub end
        return stub
    end
end

local subManagers = {
    "SplatManager", "ShadowManager", "RoadManager", "EnvelopeManager",
    "PostProcessor", "FontManager", "MapLayerManager", "InteriorManager",
    "DynamicShadow", "Light", "Network", "Physics", "SoundEmitter",
    "MiniMapEntity", "MiniMap", "AccountManager", "AnimWatcher",
    "Platform", "TileMap",
}
for _, n in ipairs(subManagers) do
    entMeta["Add" .. n] = subManagerStub(n)
end

local function newInstMethod(name, fn) return fn end

local instProto = {}

function instProto:AddTag(t) self.tags[t] = true end
function instProto:RemoveTag(t) self.tags[t] = nil end
function instProto:HasTag(t) return self.tags[t] == true end

function instProto:AddComponent(name)
    if self.components[name] then return self.components[name] end
    local c = autostub("comp:" .. name)
    self.components[name] = c
    self[name] = c
    return c
end

function instProto:RemoveComponent(name)
    self.components[name] = nil
    self[name] = nil
end

function instProto:StartUpdatingComponent() end
function instProto:StopUpdatingComponent() end
function instProto:ListenForEvent() end
function instProto:RemoveEventCallback() end
function instProto:PushEvent() end
function instProto:DoTaskInTime(_, fn) if fn then pcall(fn, self) end end
function instProto:DoPeriodicTask() return {Cancel = function() end} end
function instProto:IsValid() return true end
function instProto:Remove() end
function instProto:Hide() if self.entity then self.entity:Hide() end end
function instProto:Show() if self.entity then self.entity:Show() end end
function instProto:GetSaveRecord() return {} end
function instProto:GetPosition() return Vector3 and Vector3(0,0,0) or {x=0,y=0,z=0} end

local instMt = {
    __index = function(t, k)
        local v = instProto[k]
        if v then return v end
        local f = function() end
        rawset(t, k, f)
        return f
    end,
}

function CreateEntity()
    local ent = _native_CreateEntity()
    local inst = setmetatable({
        entity = ent,
        components = {},
        tags = {},
        GUID = ent:GetGUID(),
    }, instMt)
    entToInst[ent] = inst
    return inst
end

_G.OpenDS_InstForEntity = function(ent) return entToInst[ent] end
