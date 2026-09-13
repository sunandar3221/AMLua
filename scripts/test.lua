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

-- Register per-frame game loop tick callback
Game.OnTick(function()
    frameCount = frameCount + 1

    local ped = Player.GetPed()
    if ped ~= nil then
        -- Periodic condition check (~every 60 frames / 1 second)
        if frameCount % 60 == 0 then
            local currentHealth = Player.GetHealth(ped)
            local currentArmour = Player.GetArmour(ped)

            -- Auto-restore health if injured
            if currentHealth > 0.0 and currentHealth < 50.0 then
                Player.SetHealth(ped, 100.0)
                Player.SetArmour(ped, 100.0)
                -- Display in GTA SA top-right dialog box using GTA color formatting
                Game.PrintText("~g~Health Restored to 100!~n~~w~AMLua Auto-Heal Active.", 3000)
                Game.Log(string.format("Player HP restored from %.1f to 100.0", currentHealth))
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
