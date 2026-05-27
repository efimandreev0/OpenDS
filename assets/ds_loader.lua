local _loadfile = loadfile
local _io_open = io.open

package.path =
    DATA_ROOT .. "/DLC0003/scripts/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/widgets/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/screens/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/components/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/languages/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/cameras/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/brains/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/behaviours/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/map/?.lua;" ..
    DATA_ROOT .. "/DLC0003/scripts/prefabs/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/widgets/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/screens/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/components/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/languages/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/cameras/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/brains/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/behaviours/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/map/?.lua;" ..
    DATA_ROOT .. "/DLC0002/scripts/prefabs/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/widgets/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/screens/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/components/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/languages/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/cameras/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/brains/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/behaviours/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/map/?.lua;" ..
    DATA_ROOT .. "/DLC0001/scripts/prefabs/?.lua;" ..
    DATA_ROOT .. "/scripts/?.lua;" ..
    DATA_ROOT .. "/scripts/widgets/?.lua;" ..
    DATA_ROOT .. "/scripts/screens/?.lua;" ..
    DATA_ROOT .. "/scripts/components/?.lua;" ..
    DATA_ROOT .. "/scripts/languages/?.lua;" ..
    DATA_ROOT .. "/scripts/cameras/?.lua;" ..
    DATA_ROOT .. "/scripts/brains/?.lua;" ..
    DATA_ROOT .. "/scripts/behaviours/?.lua;" ..
    DATA_ROOT .. "/scripts/map/?.lua;" ..
    DATA_ROOT .. "/scripts/prefabs/?.lua;" ..
    DATA_ROOT .. "/scriptlibs/?.lua;" ..
    package.path

_G.OpenDS_DSPath = package.path

local function dsLoader(modulename)
    local modulepath = string.gsub(modulename, "%.", "/")
    local search = _G.OpenDS_DSPath or package.path
    local tried = {}
    for path in string.gmatch(search, "([^;]+)") do
        local filename = string.gsub(path, "%?", modulepath)
        filename = string.gsub(filename, "\\", "/")
        local f = _io_open(filename, "r")
        if f then
            f:close()
            local chunk, err = _loadfile(filename)
            if chunk then return chunk end
            tried[#tried+1] = "load " .. filename .. ": " .. tostring(err)
        else
            tried[#tried+1] = "no " .. filename
        end
    end
    return "\n\t" .. table.concat(tried, "\n\t")
end
table.insert(package.loaders, 1, dsLoader)

-- Paths we know are missing and never want to log about (DS dofiles them
-- at boot for socket/mime/etc.; these are not relevant for our reimpl).
local silentDofileMisses = {
    ["scriptlibs/socket.lua"] = true,
    ["scriptlibs/mime.lua"] = true,
}

local _dofile = dofile
function dofile(path)
    local ok, err = xpcall(function() return _dofile(path) end, function(e)
        return tostring(e)
    end)
    if not ok and not silentDofileMisses[path] then
        io.stderr:write("[opends] dofile " .. tostring(path) .. " failed: " .. tostring(err) .. "\n")
    end
end

-- Patches applied after specific DS modules finish loading. These short-circuit
-- code paths that would otherwise crash because our reimpl can't honour them
-- (mod loading, persistent storage, etc.).
local postRequirePatches = {}
postRequirePatches["modindex"] = function()
    if _G.ModIndex and _G.ModIndex.UpdateModInfo then
        _G.ModIndex.UpdateModInfo = function() end
        _G.ModIndex.Load = function(self, cb) if cb then cb() end end
        _G.ModIndex.BeginStartupSequence = function(self, cb) if cb then cb() end end
        _G.ModIndex.GetModsToLoad = function() return {} end
        _G.ModIndex.GetEnabledModNames = function() return {} end
        _G.ModIndex.GetModInfo = function() return {} end
    end
end
postRequirePatches["mods"] = function()
    if _G.ModsManager then
        _G.ModsManager.LoadMods = function() end
    end
    if _G.ModManager then
        _G.ModManager.LoadMods = function() end
    end
end

local origRequire = require
local requireCache = {}
local requireDepth = 0
function require(name)
    if requireCache[name] ~= nil then return requireCache[name] end
    requireDepth = requireDepth + 1
    if requireDepth > 50 then
        requireDepth = requireDepth - 1
        local stub = autostub("require:" .. name)
        requireCache[name] = stub
        return stub
    end
    local ok, mod = pcall(origRequire, name)
    requireDepth = requireDepth - 1
    if ok then
        requireCache[name] = mod
        local patch = postRequirePatches[name]
        if patch then patch() end
        return mod
    end
    io.stderr:write("[opends] require " .. tostring(name) .. " failed: " .. tostring(mod) .. "\n")
    local stub = autostub("require:" .. name)
    requireCache[name] = stub
    return stub
end

_G.origRequire = origRequire

package.loaded.strict = true

_G.Vector3 = _G.Vector3 or function(x, y, z)
    return {x = x or 0, y = y or 0, z = z or 0}
end

LOC = nil
Print = print
PRINT = print
nolineprint = function() end
debugstack = function() return "" end
Settings = Settings or {}
RECIPE_PREFABS = RECIPE_PREFABS or {}
BACKEND_PREFABS = BACKEND_PREFABS or {}
FRONTEND_PREFABS = FRONTEND_PREFABS or {}
ALL_DLC_TABLE = ALL_DLC_TABLE or {}
