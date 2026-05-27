PLATFORM = "WIN32"
APP_REGION = ""
BRANCH = "release"
CONFIGURATION = "PRODUCTION"
MAIN = 1
CHEATS_ENABLED = false
DEBUG_MENU_ENABLED = false
CONSOLE_ENABLED = false
SHOWLOG_ENABLED = false
MODS_ENABLED = false
ACCOMPLISHMENTS_ENABLED = false
ENCODE_SAVES = false
POT_GENERATION = false
DEBUGRENDER_ENABLED = false
ENABLE_BUG_REPORTER = false
USE_SEASON_DSP = false
RUN_GLOBAL_INIT = false
METRICS_ENABLED = false
SKIP_MAXWELL_INTRO = true
EARLYACCESS_ON = false
DEBUGGER_ENABLED = false
STATS_ENABLE = false
VERBOSITY_LEVEL = 0
SOUNDDEBUG_ENABLED = false
LOAD_UPFRONT_MODE = false

RESOLUTION_X = 1280
RESOLUTION_Y = 720
ANCHOR_MIDDLE = 0
ANCHOR_LEFT = 1
ANCHOR_RIGHT = 2
ANCHOR_TOP = 1
ANCHOR_BOTTOM = 2
SCALEMODE_NONE = 0
SCALEMODE_FILLSCREEN = 1
SCALEMODE_PROPORTIONAL = 2
SCALEMODE_FIXEDPROPORTIONAL = 3

local function autostub(name)
    local t = {}
    setmetatable(t, {
        __index = function(self, k)
            local f = function(...) return self end
            rawset(self, k, f)
            return f
        end,
        __newindex = function(self, k, v) rawset(self, k, v) end,
        __call = function(self) return self end,
        __tostring = function() return "<stub:" .. name .. ">" end,
        __concat = function(a, b)
            local function s(x) if type(x) == "table" then return "" end return tostring(x) end
            return s(a) .. s(b)
        end,
    })
    return t
end
_G.autostub = autostub

TheSim = {}
function TheSim:GetScreenSize() local w, h = OpenDS_ScreenSize() return w, h end
function TheSim:GetWindowSize() local w, h = OpenDS_ScreenSize() return w, h end
function TheSim:GetRealTime() return OpenDS_GetRealTime() end
function TheSim:GetStaticTime() return OpenDS_GetRealTime() end
function TheSim:GetTickTime() return 1/60 end
function TheSim:GetTick() return math.floor((OpenDS_GetRealTime() or 0) * 60) end

-- DS's mainfunctions.lua replaces CreateEntity with a Lua wrapper around
-- TheSim:CreateEntity(). We provide a stub that returns a chainable
-- "no-op entity" so the wrapper doesn't crash. Real UI widgets go through
-- our C++ CreateEntity (which we re-stamp into _G after main.lua finishes).
do
    local _guidCounter = 100000
    function TheSim:CreateEntity()
        _guidCounter = _guidCounter + 1
        local guid = _guidCounter
        local e = setmetatable({_guid = guid}, {
            __index = function(t, k)
                local fn = function() return t end
                rawset(t, k, fn)
                return fn
            end,
        })
        e.GetGUID = function() return guid end
        return e
    end
end
function TheSim:GetFileModificationTime() return 0 end
function TheSim:GetUsersName() return "Player" end
function TheSim:GetUserID() return "OpenDSUser" end
function TheSim:GetPlatform() return "WIN32" end
function TheSim:GetGameID() return "DS" end
function TheSim:HasFocus() return true end
function TheSim:GetSetting() return nil end
function TheSim:SetSetting() end
function TheSim:GetUserHasLicenseForApp() return false end
function TheSim:IsDLCInstalled(index) return index == REIGN_OF_GIANTS or index == CAPY_DLC or index == PORKLAND_DLC end
-- DS's dlcsupport.lua overrides our top-level IsDLCEnabled/EnableDLC/...
-- to delegate to TheSim. So we have to keep the truth here. Without this,
-- NewGameScreen:MakeDLCButton runs set_enabled(true) at construction time
-- and crashes on self.menu (which is set later in the ctor).
_G._opends_dlc_enabled = _G._opends_dlc_enabled or {}
function TheSim:IsDLCEnabled(index) return _opends_dlc_enabled[index] == true end
function TheSim:SetDLCEnabled(index, enabled)
    if enabled then _opends_dlc_enabled[index] = true
    else _opends_dlc_enabled[index] = nil end
end
function TheSim:GetLanguageCode() return "english" end
function TheSim:FindFilesInDirectory() return {} end
function TheSim:GetModDirectoryNames() return {} end
-- In-memory persistent string store. Real DS uses disk files; we just keep
-- a table so Save/Load round-trips work within a single process lifetime.
_G._opends_persist = _G._opends_persist or {}
function TheSim:GetPersistentString(name, cb)
    local data = _opends_persist[name]
    if cb then cb(data ~= nil, data) end
end
function TheSim:SetPersistentString(name, data, _, cb)
    _opends_persist[name] = data
    if cb then cb(true) end
end
function TheSim:ErasePersistentString(name, cb)
    _opends_persist[name] = nil
    if cb then cb(true) end
end
function TheSim:CheckPersistentStringExists(name, cb)
    if cb then cb(_opends_persist[name] ~= nil) end
end
function TheSim:GetUserAccount() return {} end

-- TheSim:GenerateNewWorld returns serialized world savedata via callback.
-- Real engine spins up a worker, runs a long simulation, returns full table.
-- For now we just return an error string so DS's DoGenerateWorld surfaces a
-- "world gen failed" dialog instead of looping forever on an empty result.
function TheSim:GenerateNewWorld(_genparam, _modparam, cb)
    io.stderr:write("[opends] GenerateNewWorld: stub returning error\n")
    if cb then
        cb('error = "World generation not yet implemented in OpenDS"')
    end
end

-- DS's SimReset() calls SetInstanceParameters(json_string) then Reset().
-- Real engine reboots Lua and runs gamelogic.lua as the new chunk; we just
-- remember the params and on Reset() run gamelogic.lua in the same VM.
_G._opends_instance_params = nil
function TheSim:SetInstanceParameters(params)
    _opends_instance_params = params
end
function TheSim:Reset()
    if _G._opends_in_reset then return end  -- guard against recursion
    _G._opends_in_reset = true
    io.stderr:write("[opends] TheSim:Reset -> running gamelogic.lua\n")
    -- Push params into the global Settings table the way DS expects.
    if _G.SetInstanceParameters and _opends_instance_params then
        _G.SetInstanceParameters(_opends_instance_params)
    end
    -- Wipe the old MainScreen so gamelogic's frontend can show fresh.
    if _G.TheFrontEnd and _G.TheFrontEnd.ClearScreens then
        pcall(function() _G.TheFrontEnd:ClearScreens() end)
    end
    local gamelogic_path = DATA_ROOT .. "/scripts/gamelogic.lua"
    local ok, err = xpcall(function() dofile(gamelogic_path) end, function(e)
        return tostring(e) .. "\n" .. debug.traceback("", 2)
    end)
    if not ok then
        io.stderr:write("[opends] gamelogic.lua failed: " .. tostring(err) .. "\n")
    end
    _G._opends_in_reset = false
end
function TheSim:LoadFont() end
function TheSim:LoadJapaneseFont() end
function TheSim:UnloadFont() end
function TheSim:SetFontFallbacks() end
function TheSim:LoadPrefabs() end
function TheSim:UnloadPrefabs() end
function TheSim:UnregisterPrefabs() end
function TheSim:LoadAssets() end
function TheSim:UnloadAssets() end
function TheSim:LuaPrint(...)
    local n = select("#", ...)
    for i = 1, n do io.stderr:write(tostring(select(i, ...)), i < n and "\t" or "") end
    io.stderr:write("\n")
end
function TheSim:SetReverbPreset() end
local _soundVolumes = {
    set_sfx = 0.7,
    set_music = 0.7,
    set_ambience = 0.7,
    master = 1.0,
}
function TheSim:GetSoundVolume(name) return _soundVolumes[name] or 0.7 end
function TheSim:SetSoundVolume(name, value) _soundVolumes[name] = value end
function TheSim:SetUIRoot() end
function TheSim:SetStalling() end
function TheSim:EnableUserDataCollection() end
function TheSim:SendJSMessage() end
function TheSim:SetNetbookMode() end
function TheSim:IsNetbookMode() return false end
function TheSim:ShouldPlayIntroMovie() return false end
function TheSim:GetGameServer() return "" end
function TheSim:Quit() OpenDS_RequestShutdown() end
function TheSim:GenerateNewRandomUserID() return "OpenDSUser" end
function TheSim:GetTimeSinceLaunch() return OpenDS_GetRealTime() end
function TheSim:GetMemoryUsage() return 0 end
function TheSim:GetCriticalResourceFailed() return false end
function TheSim:ResetError() end
function TheSim:ReplaceColour() end

setmetatable(TheSim, {__index = function(t, k)
    local f = function() end
    rawset(t, k, f)
    return f
end})

TheInput = {}
local _moveHandlers = {}
local _btnHandlers = {}
local _keyHandlers = {}
local _ctrlHandlers = {}

local function makeHandle(list, fn)
    local h = {fn = fn}
    list[h] = true
    h.Remove = function() list[h] = nil end
    return h
end

function TheInput:AddMoveHandler(fn) return makeHandle(_moveHandlers, fn) end
function TheInput:AddMouseButtonHandler(fn) return makeHandle(_btnHandlers, fn) end
function TheInput:AddKeyDownHandler(_, fn) return makeHandle(_keyHandlers, {fn = fn, down = true}) end
function TheInput:AddKeyUpHandler(_, fn) return makeHandle(_keyHandlers, {fn = fn, down = false}) end
function TheInput:AddKeyHandler(fn) return makeHandle(_keyHandlers, {fn = fn}) end
function TheInput:AddControlHandler(_, fn) return makeHandle(_ctrlHandlers, fn) end
function TheInput:AddTextInputHandler(fn) return makeHandle(_keyHandlers, {fn = fn, text = true}) end
function TheInput:GetScreenPosition() return OpenDS_GetMousePos() end
function TheInput:GetWorldPosition() return 0, 0, 0 end
function TheInput:IsKeyDown() return false end
function TheInput:IsControlPressed() return false end
function TheInput:GetControllerID() return 0 end
function TheInput:GetLocalizedControl() return "" end
function TheInput:GetControlIsMouseWheel() return false end
function TheInput:ControllerAttached() return false end
function TheInput:ControllerConnected() return false end
function TheInput:EnableMouse() end
function TheInput:DisableAllControllers() end
function TheInput:UpdateEntitiesUnderMouse() end

setmetatable(TheInput, {__index = function(t, k)
    local f = function() end
    rawset(t, k, f)
    return f
end})

_G.OpenDS_MouseHandlers = _moveHandlers
_G.OpenDS_ButtonHandlers = _btnHandlers
_G.OpenDS_KeyHandlers = _keyHandlers

TheInputProxy = autostub("TheInputProxy")
function TheInputProxy:SetCursorVisible() end
function TheInputProxy:GetInputDeviceCount() return 0 end
function TheInputProxy:IsInputDeviceEnabled() return false end

TheSystemService = autostub("TheSystemService")
function TheSystemService:SetStalling() end

TheNet = autostub("TheNet")
TheRawImgui = autostub("TheRawImgui")
TheGameService = autostub("TheGameService")
function TheGameService:RegisterAchievement() end
function TheGameService:AwardAchievement() end

-- DS's upsell.lua decides "Play!" vs "Enter Key" by checking Purchases for "GAME".
Purchases = {"GAME"}
TheConfig = autostub("TheConfig")
function TheConfig:IsEnabled() return false end

TheCamera = autostub("TheCamera")
KnownModIndex = autostub("KnownModIndex")
KnownModIndex.Load = function(_, cb) if cb then cb() end end
KnownModIndex.BeginStartupSequence = function(_, cb) if cb then cb() end end
KnownModIndex.GetEnabledModNames = function() return {} end
KnownModIndex.GetModInfo = function() return {} end

ModManager = autostub("ModManager")
ModManager.LoadMods = function() end

TheGlobalInstance = nil
SplatManager = autostub("SplatManager")
ShadowManager = autostub("ShadowManager")
RoadManager = autostub("RoadManager")
EnvelopeManager = autostub("EnvelopeManager")
PostProcessor = autostub("PostProcessor")
FontManager = autostub("FontManager")
MapLayerManager = autostub("MapLayerManager")
InteriorManager = autostub("InteriorManager")
Roads = autostub("Roads")
TheMixer = autostub("TheMixer")
local _mixerLevels = {
    set_sfx = 0.7,
    set_music = 0.7,
    set_ambience = 0.7,
}
function TheMixer:GetLevel(name) return _mixerLevels[name] or 0.7 end
function TheMixer:SetLevel(name, value) _mixerLevels[name] = value end

Profile = nil

function IsConsole() return false end
function IsNotConsole() return true end
function IsPS4() return false end
function IsXB1() return false end
function IsSteam() return false end
function IsLinux() return false end
function IsRail() return false end
function global() end
function assert_fmt(cond, ...) assert(cond, ...) end
function AddPrintLogger() end
function EnableAllDLC() end
function LoadAchievements() end
function LoadPrefabFile() end
function GlobalInit() end
function TranslateStringTable() end
function VisitURL(url) print("VisitURL: " .. tostring(url)) end
function nolineprint() end
function GetTranslatedString(s) return tostring(s) end
function IsGamePurchased() return true end
function IsDLCInstalled(index) return index == REIGN_OF_GIANTS or index == CAPY_DLC or index == PORKLAND_DLC end
-- Track enabled state. DS's NewGameScreen relies on IsDLCEnabled reflecting
-- the most recent EnableDLC/DisableDLC call (and DisableAllDLC inside the
-- MakeDLCButton helpers expects to actually clear flags). If IsDLCEnabled
-- always returns true, MakeDLCButton's auto-set_enabled(true) at construction
-- triggers self.menu:SetItemEnabled BEFORE NewGameScreen sets self.menu.
_G._opends_dlc_enabled = _G._opends_dlc_enabled or {}
function IsDLCEnabled(index) return _opends_dlc_enabled[index] == true end
function EnableDLC(index) _opends_dlc_enabled[index] = true end
function DisableDLC(index) _opends_dlc_enabled[index] = nil end
function DisableAllDLC() for k in pairs(_opends_dlc_enabled) do _opends_dlc_enabled[k] = nil end end
function GetGameID() return "DS" end
function HasDLC(index) return IsDLCInstalled(index) end
function HasAllDLC() return true end
function IsAnyDLCEnabled() return true end
function GetActiveWorld() return nil end
function StartNextInstance(opts)
    io.stderr:write("[opends] StartNextInstance: " .. tostring(opts and opts.reset_action) ..
                    " slot=" .. tostring(opts and opts.save_slot) .. "\n")
    -- TODO: actually load the world. For now just log so we can see the click
    -- pipeline got all the way through.
end
function SetPause() end
function Sleep() end
function GetIndexedString(s) return tostring(s) end
function GetGameModeProperty() return nil end
function GetGameModeString() return "" end
function IsTableEmpty(t) return next(t) == nil end
function GetTickTime() return 1/60 end
function GetTime() return OpenDS_GetRealTime() end
function GetStaticTime() return OpenDS_GetRealTime() end
function GetTimeReal() return OpenDS_GetRealTime() end
function JapaneseOnPS4() return false end
function CheckControllers() end
function RequestShutdown() OpenDS_RequestShutdown() end
function SimReset() OpenDS_RequestShutdown() end

CWD = nil
GAME_SERVER = ""

BGCOLOURS = {
    RED = {0.65, 0.25, 0.25, 1},
    YELLOW = {0.7, 0.6, 0.2, 1},
    PURPLE = {0.4, 0.2, 0.5, 1},
    TEAL = {0.24, 0.55, 0.52, 1},
    GREEN = {0.38, 0.54, 0.28, 1},
    FULL = {1, 1, 1, 1},
    GREY = {0.3, 0.3, 0.3, 1},
}

MOVE_UP = "MOVE_UP"
MOVE_DOWN = "MOVE_DOWN"
MOVE_LEFT = "MOVE_LEFT"
MOVE_RIGHT = "MOVE_RIGHT"
CONTROL_ACCEPT = 7
CONTROL_CANCEL = 8

MAIN_GAME = 0
REIGN_OF_GIANTS = 1
CAPY_DLC = 2
PORKLAND_DLC = 3
ALL_DLC_TABLE = {REIGN_OF_GIANTS = true, CAPY_DLC = true, PORKLAND_DLC = true}

VERBOSITY = {ERROR = 0, WARNING = 1, INFO = 2, DEBUG = 3}
