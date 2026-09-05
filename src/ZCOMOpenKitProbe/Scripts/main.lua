-- Open Kit diagnostic probe.
--
-- The Core half is a pak and contains no code, so it cannot report on itself.
-- This reads the result back out of the running game. It is a test aid: it
-- installs nothing, changes nothing, and Core needs neither it nor UE4SS.
--
-- 0.1.0 established that the container applies: the class default object really
-- holds ten entries at runtime. 0.2.0 hooked the picker's filters and none of
-- them ever fired, which means the assign flow was never entered rather than
-- entered and filtered.
--
-- 0.3.0 therefore watches for the screen itself. The specialization list is
-- driven by WBP_FocusTree_AssignSpecialization_New, which only appears when a
-- character has a specialization slot to fill -- a fresh recruit, or a respec.
-- An operator whose specialization is already set never opens it, so there is
-- nothing for the mod to add to. If "assign widget constructed" never appears
-- below, the screen was not reached and the filters say nothing.
--
-- Everything is pcall-guarded. A probe that crashes the game it is measuring is
-- worse than no probe.

local VERSION = "0.3.0"
local TAG = "[ZCOM_OPEN_KIT]"

local VM = "/Game/Game/UI/Strategy/Personnel/FocusTree/BP/BP_SpecializationSelectionVM"
local CLASS = VM .. ".BP_SpecializationSelectionVM_C"
local CDO = VM .. ".Default__BP_SpecializationSelectionVM_C"

local STOCK_ENTRIES = 8
local STOCK_LOCK_TAG = "br.Customization.Part.Character.Info.Name"

local function log(m) print(TAG .. " " .. m) end

local function try(fn)
    local ok, v = pcall(fn)
    if ok then return v end
    return nil
end

-- UE4SS hands hook arguments as wrappers; some builds need :get(), some do not.
local function unwrap(v)
    if v == nil then return nil end
    local got = try(function() return v:get() end)
    if got ~= nil then return got end
    return v
end

local function object_name(v)
    local o = unwrap(v)
    if o == nil then return "nil" end
    local n = try(function() return o:GetFullName() end)
    if n ~= nil then return tostring(n) end
    return tostring(try(function() return o:GetFName():ToString() end) or o)
end

-- Just the asset name, so a line of log is readable.
local function short(full)
    return tostring(full):match("([^/%.]+)$") or tostring(full)
end

local function boolean_of(v)
    local b = unwrap(v)
    if type(b) == "boolean" then return b end
    return try(function() return b:get() end)
end

---------------------------------------------------------------- status

local function map_entries(map)
    if map == nil then return nil, "absent" end
    local n = try(function()
        local c = 0
        map:ForEach(function() c = c + 1 end)
        return c
    end)
    if n ~= nil then return n, "foreach" end
    local len = try(function() return #map end)
    if len ~= nil then return len, "length" end
    return nil, "unreadable"
end

local function lock_tag(cdo, field)
    return try(function() return cdo[field].TagDictionary[1].TagName:ToString() end)
end

local reported = false

local function report()
    local cdo = try(function() return StaticFindObject(CDO) end)
    if cdo == nil or not cdo:IsValid() then
        log("status version=" .. VERSION .. " cdo=missing")
        return false
    end
    local count, how = map_entries(try(function() return cdo.SpecializationPartMapping end))
    local spec_tag = lock_tag(cdo, "SpecLockedQuery") or "unreadable"
    local talent_tag = lock_tag(cdo, "TalentLockedQuery") or "unreadable"
    log("status version=" .. VERSION
        .. " cdo=found"
        .. " core=" .. (count == nil and "unknown" or (count > STOCK_ENTRIES and "applied" or "not-applied"))
        .. " specializations=" .. tostring(count) .. "/" .. STOCK_ENTRIES .. " (" .. how .. ")"
        .. " swap=" .. tostring(spec_tag ~= STOCK_LOCK_TAG and talent_tag ~= STOCK_LOCK_TAG))
    return count ~= nil
end

---------------------------------------------------------------- filters

-- Each of these is a candidate for what drops Padawan and Warrior between the
-- mapping and the screen. Logging the argument and the answer says which.
local ASSIGN_WIDGET = "WBP_FocusTree_AssignSpecialization_New_C"

-- Distinguishes "never opened the screen" from "opened it and the kits were
-- filtered out". Without this the absence of filter lines is ambiguous.
local function watch_screen()
    local ok = try(function()
        return NotifyOnNewObject("/Game/Game/UI/Strategy/Personnel/FocusTree/Widgets/"
            .. "WBP_FocusTree_AssignSpecialization_New." .. ASSIGN_WIDGET,
            function()
                log("assign widget constructed -- the specialization list is being built now")
            end)
    end)
    if ok == nil then log("assign widget watch=failed") else log("assign widget watch=ok") end
end

local WATCHED = {
    "IsPartValid",
    "ShouldSpecBeLocked",
    "ShouldSpecializaitonBeLocked",  -- the game's spelling, not a typo here
    "ShouldTalentsBeLocked",
    "ArePartsTheSameType",
}

local seen = {}

local function watch(name)
    local path = CLASS .. ":" .. name
    local ok = try(function()
        return RegisterHook(path, function() end, function(_, a, b, c)
            -- The return value is the last argument UE4SS passes. With one
            -- parameter that is b; with none it is a. Log both candidates
            -- rather than assume the arity of a Blueprint function.
            local subject = object_name(a)
            local answer = boolean_of(c)
            if answer == nil then answer = boolean_of(b) end
            local key = name .. "|" .. short(subject) .. "|" .. tostring(answer)
            if seen[key] then return end
            seen[key] = true
            log("filter " .. name .. " part=" .. short(subject) .. " -> " .. tostring(answer))
        end)
    end)
    if ok == nil then log("filter " .. name .. " hook=failed") else log("filter " .. name .. " hook=ok") end
end

local function setup_watch()
    local path = CLASS .. ":SetupSpecializationsForCharacter"
    local ok = try(function()
        return RegisterHook(path, function() end, function()
            ExecuteInGameThread(function()
                log("SetupSpecializationsForCharacter ran -- filter lines above this belong to it")
            end)
        end)
    end)
    if ok == nil then log("SetupSpecializationsForCharacter hook=failed") end
end

---------------------------------------------------------------- start

log("loaded version=" .. VERSION .. " -- diagnostic only, installs nothing")

LoopAsync(2000, function()
    if reported then return true end
    ExecuteInGameThread(function()
        if reported then return end
        reported = report()
        if reported then
            for _, name in ipairs(WATCHED) do watch(name) end
            setup_watch()
            watch_screen()
            log("watching " .. #WATCHED .. " filters"
                .. " -- now open a character who still has a specialization to CHOOSE"
                .. " (a new recruit, or a respec); an operator already specialised never"
                .. " opens that screen")
        end
    end)
    return false
end)
