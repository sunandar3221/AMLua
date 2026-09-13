-- ===================================================
-- AMLua Example Test Script: test.lua
-- Target: GTA San Andreas Android (Android Mod Loader)
-- ===================================================

Game.Log("=== AMLua test.lua starting ===")

-- Print list of loaded scripts to logcat / amlua.log
local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua test.lua loaded with %d active script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

local frameCount = 0
local healCooldown = 0
local greeted = false

-- Register per-frame game loop tick callback
Game.OnTick(function()
    frameCount = frameCount + 1
    if healCooldown > 0 then
        healCooldown = healCooldown - 1
    end

    local ped = Player.GetPed()
    if ped ~= nil then
        -- Welcome message once player ped is active in game
        if not greeted then
            greeted = true
            Game.PrintText("~g~AMLua Test Script Active!~n~~w~Auto-Heal Ready.", 4000)
            Game.Log("AMLua test.lua: player detected, welcome message displayed.")
        end

        -- Periodic condition check (~every 30 frames / 0.5 second)
        if frameCount % 30 == 0 and healCooldown == 0 then
            local currentHealth = Player.GetHealth(ped)

            -- Auto-restore health if injured (below 50 HP)
            if currentHealth > 0.0 and currentHealth < 50.0 then
                Player.SetHealth(ped, 100.0)
                Player.SetArmour(ped, 100.0)
                healCooldown = 150 -- Cooldown for 5 seconds (~150 frames)

                -- Display in GTA SA native top-right dialog box
                Game.PrintText("~g~Health Restored to 100!~n~~w~AMLua Auto-Heal Active.", 3000)
                Game.Log(string.format("Player HP restored from %.1f to 100.0 (Armour set to 100.0)", currentHealth))
            end
        end

        -- Periodic heartbeat log (~every 600 frames / 10 seconds)
        if frameCount % 600 == 0 then
            local hp = Player.GetHealth(ped)
            local arm = Player.GetArmour(ped)
            Game.Log(string.format("[Tick Status] Frame: %d | Ped: %s | HP: %.1f | Armour: %.1f", frameCount, tostring(ped), hp, arm))
        end
    end
end)

Game.Log("=== AMLua test.lua initialized successfully ===")
