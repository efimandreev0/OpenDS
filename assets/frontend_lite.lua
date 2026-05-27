local sound_stub = autostub("Sound")
local graphics_options = autostub("GraphicsOptions")
local display_modes = {
    {w = 1280, h = 720, hz = 60},
    {w = 1600, h = 900, hz = 60},
    {w = 1920, h = 1080, hz = 60},
}
local graphics_state = {
    bloom = false,
    distortion = true,
    smalltextures = false,
    fullscreen = false,
    display = 0,
    mode = 0,
}

function graphics_options:DisableStencil() end
function graphics_options:DisableLightMapComponent() end
function graphics_options:GetBloomEnabled() return self:IsBloomEnabled() end
function graphics_options:IsBloomEnabled() return graphics_state.bloom end
function graphics_options:SetBloomEnabled(v) graphics_state.bloom = v and true or false end
function graphics_options:IsDistortionEnabled() return graphics_state.distortion end
function graphics_options:SetDistortionEnabled(v) graphics_state.distortion = v and true or false end
function graphics_options:IsSmallTexturesMode() return graphics_state.smalltextures end
function graphics_options:SetSmallTexturesMode(v) graphics_state.smalltextures = v and true or false end
function graphics_options:IsFullScreen() return graphics_state.fullscreen end
function graphics_options:IsFullScreenEnabled() return true end
function graphics_options:GetFullscreenDisplayID() return graphics_state.display end
function graphics_options:GetFullscreenDisplayRefreshRate() return display_modes[graphics_state.mode + 1].hz end
function graphics_options:GetCurrentDisplayModeID() return graphics_state.mode end
function graphics_options:SetDisplayMode(display, mode, fullscreen)
    graphics_state.display = display or 0
    graphics_state.mode = mode or 0
    graphics_state.fullscreen = fullscreen and true or false
end
function graphics_options:GetNumDisplays() return 1 end
function graphics_options:GetDisplayName(display) return "Display " .. tostring((display or 0) + 1) end
function graphics_options:GetNumDisplayModes() return #display_modes end
function graphics_options:GetDisplayMode(_, mode)
    local m = display_modes[(mode or 0) + 1] or display_modes[1]
    return m.w, m.h, m.hz
end
function graphics_options:GetNumRefreshRates() return 1 end
function graphics_options:GetRefreshRate() return 60 end

local FELite = {}
FELite.__index = FELite

function FELite.new()
    local self = setmetatable({}, FELite)
    self.screenstack = {}
    self.updating_widgets = {}
    self.last_focused = nil
    return self
end

function FELite:GetSize() return OpenDS_ScreenSize() end
function FELite:GetGraphicsOptions() return graphics_options end
function FELite:GetSound() return sound_stub end

function FELite:PushScreen(screen)
    table.insert(self.screenstack, screen)
    if screen and screen.inst and screen.inst.entity then
        OpenDS_FrontEndPush(screen.inst.entity)
    end
    if screen and screen.OnBecomeActive then
        local ok, err = pcall(screen.OnBecomeActive, screen)
        if not ok then io.stderr:write("OnBecomeActive: " .. tostring(err) .. "\n") end
    end
end

function FELite:PopScreen()
    local top = self.screenstack[#self.screenstack]
    if top and top.OnBecomeInactive then pcall(top.OnBecomeInactive, top) end
    table.remove(self.screenstack)
    OpenDS_FrontEndPop()
    local new_top = self.screenstack[#self.screenstack]
    if new_top and new_top.OnBecomeActive then pcall(new_top.OnBecomeActive, new_top) end
end

function FELite:ShowScreen(screen)
    while #self.screenstack > 0 do self:PopScreen() end
    self:PushScreen(screen)
end

function FELite:GetActiveScreen()
    return self.screenstack[#self.screenstack]
end

function FELite:StartUpdatingWidget(w) self.updating_widgets[w] = true end
function FELite:StopUpdatingWidget(w) self.updating_widgets[w] = nil end

function FELite:Update(dt)
    for w in pairs(self.updating_widgets) do
        if w.OnUpdate then pcall(w.OnUpdate, w, dt) end
    end
    local top = self:GetActiveScreen()
    if top and top.OnUpdate then pcall(top.OnUpdate, top, dt) end
end

function FELite:StopTrackingMouse() end
function FELite:LockFocus() end
function FELite:HideTitle() end
function FELite:ShowTitle() end
function FELite:ShowConsoleLog() end
function FELite:HideConsoleLog() end
function FELite:Fade(_to_dark, _time, cb)
    io.stderr:write("[opends] Fade hasCb=" .. tostring(type(cb) == "function") .. "\n")
    if type(cb) == "function" then cb() end
end
function FELite:FadeBack(cb)
    if type(cb) == "function" then cb() end
end

setmetatable(FELite, {__index = function(_, k)
    return function() end
end})

TheFrontEnd = FELite.new()

function Tick(dt)
    if TheFrontEnd and TheFrontEnd.Update then
        TheFrontEnd:Update(dt)
    end
end
