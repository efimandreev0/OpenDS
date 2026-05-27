local function collectClickables(entity, out)
    if not entity or not entity.visible then return end
    if entity.imageWidget and entity.clickable then
        out[#out+1] = entity
    end
    for _, c in ipairs(entity.children or {}) do
        collectClickables(c, out)
    end
end

local function findWidgetForEntityRaw(screen, target_ent)
    local function walk(widget)
        if not widget then return nil end
        if widget.inst and widget.inst.entity == target_ent then return widget end
        if widget.children then
            for _, c in pairs(widget.children) do
                local r = walk(c)
                if r then return r end
            end
        end
    end
    return walk(screen)
end

local SDLK_ESCAPE = 27
local SDLK_RETURN = 13

local hovered = nil
local pressed = nil

local function safeCall(label, fn, ...)
    local ok, err = pcall(fn, ...)
    if not ok then
        io.stderr:write("[opends] input " .. tostring(label) .. " failed: " .. tostring(err) .. "\n")
    end
    return ok
end

local function resolveInteractiveWidget(widget)
    -- Walk up to the nearest widget that REALLY handles input. We use
    -- rawget for OnControl because every Widget inherits a no-op OnControl
    -- from the base class — we only care about ones that override it.
    local w = widget
    while w do
        if w.onclick or w.ondown or w.whiledown or w.text or rawget(w, "OnControl") then
            return w
        end
        w = w.parent
    end
    return widget
end

local function widgetAt(screen, x, y)
    local function walk(widget)
        local hit = nil
        if widget.children then
            for _, c in pairs(widget.children) do
                local r = walk(c)
                if r then hit = r end
            end
        end
        if hit then return hit end
        if widget.inst and widget.inst.entity and widget.inst.ImageWidget then
            if OpenDS_HitTestImage(widget.inst.entity, x, y) then
                return resolveInteractiveWidget(widget)
            end
        end
        return nil
    end
    return walk(screen)
end

function OpenDS_OnMouseMove(x, y)
    for h in pairs(OpenDS_MouseHandlers) do
        if h.fn then safeCall("move handler", h.fn, x, y) end
    end

    local screen = TheFrontEnd:GetActiveScreen()
    if not screen then return end

    local under = widgetAt(screen, x, y)
    if under ~= hovered then
        if hovered then
            hovered.focus = false
            if hovered.OnLoseFocus then safeCall("lose focus", hovered.OnLoseFocus, hovered) end
            local p = hovered.parent
            while p do p.focus = false; p = p.parent end
        end
        hovered = under
        if hovered then
            local p = hovered
            while p do p.focus = true; p = p.parent end
            if hovered.OnGainFocus then safeCall("gain focus", hovered.OnGainFocus, hovered) end
        end
    end
end

function OpenDS_OnMouseButton(btn, down, x, y)
    for h in pairs(OpenDS_ButtonHandlers) do
        if h.fn then safeCall("button handler", h.fn, btn, down, x, y) end
    end

    if btn ~= 1000 then return end

    local screen = TheFrontEnd:GetActiveScreen()
    if not screen then return end
    local target = widgetAt(screen, x, y)
    if target ~= hovered then
        OpenDS_OnMouseMove(x, y)
        target = hovered
    end

    if down then
        pressed = target
        io.stderr:write("[opends] click DOWN target=" .. tostring(target and target.name) ..
                        " hasOnControl=" .. tostring(target and rawget(target, "OnControl") ~= nil) ..
                        " hasOnClick=" .. tostring(target and target.onclick ~= nil) ..
                        " focus=" .. tostring(target and target.focus) .. "\n")
        if pressed and pressed.OnControl then
            safeCall("control down", pressed.OnControl, pressed, CONTROL_ACCEPT, true)
        end
    else
        local release = pressed or target
        pressed = nil
        io.stderr:write("[opends] click UP target=" .. tostring(release and release.name) .. "\n")
        if release and release.OnControl then
            safeCall("control up", release.OnControl, release, CONTROL_ACCEPT, false)
        elseif release and release.onclick then
            safeCall("click", release.onclick)
        end
    end
end

function OpenDS_OnKey(key, down)
    for h in pairs(OpenDS_KeyHandlers) do
        if h.fn and type(h.fn) == "function" then
            safeCall("key handler", h.fn, key, down)
        elseif h.fn and h.fn.fn then
            safeCall("key handler", h.fn.fn, key, down)
        end
    end

    if not down and key == SDLK_ESCAPE then
        local screen = TheFrontEnd:GetActiveScreen()
        if screen and screen.OnControl then
            safeCall("cancel", screen.OnControl, screen, CONTROL_CANCEL, false)
        end
    elseif key == SDLK_RETURN then
        local screen = TheFrontEnd:GetActiveScreen()
        local focus = screen and screen.GetDeepestFocus and screen:GetDeepestFocus() or nil
        if focus and focus.OnControl then
            safeCall("accept", focus.OnControl, focus, CONTROL_ACCEPT, down)
        end
    end
end

function OpenDS_OnText(text)
end
