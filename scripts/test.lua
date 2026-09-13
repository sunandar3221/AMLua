-- ===================================================
-- AMLua Example Test Script: test.lua
-- Target: GTA San Andreas Android (Android Mod Loader)
-- ===================================================

Game.Log("=== AMLua test.lua starting ===")

-- Display greeting text in-game HUD and Android toast
Game.PrintText("AMLua Loader Active!", 3000)

local frameCount = 0

-- Register per-frame game loop tick callback
Game.OnTick(function()
    frameCount = frameCount + 1

    -- Periodic condition check (~every 60 frames)
    if frameCount % 60 == 0 then
        local ped = Player.GetPed()
        if ped ~= nil then
            local currentHealth = Player.GetHealth(ped)
            local currentArmour = Player.GetArmour(ped)

            -- Auto-restore health if injured
            if currentHealth > 0.0 and currentHealth < 50.0 then
                Player.SetHealth(ped, 100.0)
                Player.SetArmour(ped, 100.0)
                Game.PrintText("Health Restored to 100!", 2000)
                Game.Log(string.format("Player HP restored from %.1f to 100.0", currentHealth))
            end
        end
    end

    -- Periodic heartbeat log (~every 600 frames / 10 seconds)
    if frameCount % 600 == 0 then
        local ped = Player.GetPed()
        if ped ~= nil then
            local hp = Player.GetHealth(ped)
            local arm = Player.GetArmour(ped)
            Game.Log(string.format("[Tick Status] Frame: %d | Ped: %s | HP: %.1f | Armour: %.1f", frameCount, tostring(ped), hp, arm))
        end
    end
end)

Game.Log("=== AMLua test.lua registered tick event successfully ===")
