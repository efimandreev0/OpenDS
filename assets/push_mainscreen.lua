__STRICT = false
local function tb(e) return tostring(e) .. "\n" .. debug.traceback("", 2) end

if not Profile then
    local ok, PlayerProfile = pcall(origRequire, "playerprofile")
    if ok and type(PlayerProfile) == "function" then
        local pok, p = pcall(PlayerProfile)
        if pok then
            Profile = p
            pcall(p.Load, p, function() end, true)
        end
    end
end
if not Profile then
    Profile = autostub("Profile")
    function Profile:GetGameLanguage() return "en" end
    function Profile:GetLanguageID() return "en" end
    function Profile:GetVolume() return 7, 7, 7 end
    function Profile:GetHUDSize() return 5 end
    function Profile:GetCraftingMenuSize() return 0 end
    function Profile:GetCraftingMenuNumPinPages() return 1 end
    function Profile:GetVibrationEnabled() return false end
    function Profile:IsScreenFlashEnabled() return false end
    function Profile:GetScreenFlash() return 1 end
    function Profile:GetSendStatsEnabled() return false end
    function Profile:GetAgreementsSetting() return true end
    function Profile:GetValue() return nil end
end

local ok, MainScreen = xpcall(function() return origRequire("screens/mainscreen") end, tb)
if not ok then
    io.stderr:write("[opends] cannot require mainscreen: " .. tostring(MainScreen) .. "\n")
    return
end

io.stderr:write("[opends] constructing MainScreen\n"); io.stderr:flush()

local ok2, ms = xpcall(function() return MainScreen(Profile) end, tb)
if not ok2 then
    io.stderr:write("[opends] MainScreen ctor failed: " .. tostring(ms) .. "\n")
    return
end

io.stderr:write("[opends] MainScreen ok, pushing\n"); io.stderr:flush()
TheFrontEnd:ShowScreen(ms)
_G.RootScreen = ms
