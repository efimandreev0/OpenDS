io.stderr:setvbuf("no")

local function log(s)
    io.stderr:write("[opends] " .. tostring(s) .. "\n")
end

print = function(...)
    local n = select("#", ...)
    local parts = {}
    for i = 1, n do parts[i] = tostring(select(i, ...)) end
    io.stderr:write(table.concat(parts, "\t"), "\n")
end

log("DATA_ROOT = " .. tostring(DATA_ROOT))

local SCREEN_W, SCREEN_H = OpenDS_ScreenSize()

dofile("engine_globals.lua")
dofile("entity_wrap.lua")
dofile("frontend_lite.lua")
dofile("ds_loader.lua")

local _wrapped_CreateEntity = CreateEntity

local main_path = DATA_ROOT .. "/scripts/main.lua"
log("running " .. main_path)
local ok, err = xpcall(function() dofile(main_path) end, function(e)
    return tostring(e) .. "\n" .. debug.traceback("", 2)
end)
log("main.lua chunk done")
if not ok then
    log("main.lua aborted: " .. tostring(err))
end

CreateEntity = _wrapped_CreateEntity
__STRICT = false

-- main.lua/mainfunctions.lua resets Purchases = {} during load. Re-stamp it
-- so DS's upsell.lua:IsGamePurchased returns true (giving "Play!" rather
-- than "Enter Key" on the main menu).
Purchases = {"GAME", "REIGN_OF_GIANTS", "CAPY_DLC", "PORKLAND_DLC"}
_G.IsGamePurchased = function() return true end

-- main.lua requires "saveindex" (which defines the SaveIndex class) but
-- only gamelogic.lua actually instantiates SaveGameIndex. Since we don't
-- run gamelogic, create the singleton ourselves so LoadGameScreen works.
if not _G.SaveGameIndex and _G.SaveIndex then
    local ok, inst = pcall(_G.SaveIndex)
    if ok then
        _G.SaveGameIndex = inst
        log("SaveGameIndex created OK")
    else
        log("SaveGameIndex creation failed: " .. tostring(inst))
    end
end

-- Let DS's own StartNextInstance/SimReset run. The real engine would reboot
-- the Lua VM at TheSim:Reset() with gamelogic.lua as the new entry point;
-- we don't reboot, instead TheSim:Reset() (defined in engine_globals.lua)
-- runs gamelogic.lua right here in the same VM.

dofile("frontend_lite.lua")
dofile("push_mainscreen.lua")
dofile("input_dispatch.lua")
