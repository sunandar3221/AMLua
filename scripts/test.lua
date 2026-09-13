-- ===================================================
-- AMLua Example Test Script: test.lua
-- Target: GTA San Andreas Android (Android Mod Loader)
-- ===================================================

Game.Log("=== AMLua test.lua starting ===")

-- Inspect and print loaded scripts count
local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua test.lua loaded with %d active script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

local frameCount = 0
local greeted = false

-- Register per-frame game loop tick callback
Game.OnTick(function()
    frameCount = frameCount + 1

    local ped = Player.GetPed()
    if ped ~= nil then
        -- Once player ped is active in game, display welcome message safely
        if not greeted then
            greeted = true
            Game.PrintText("AMLua Active! Double-tap top screen for Mod List.", 4000)
            Game.Log("Player ped detected in-game. Initial greeting displayed.")
        end

        -- Periodic condition check (~every 60 frames / 1 second)
        if frameCount % 60 == 0 then
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

        -- Periodic heartbeat log (~every 600 frames / 10 seconds)
        if frameCount % 600 == 0 then
            local hp = Player.GetHealth(ped)
            local arm = Player.GetArmour(ped)
            Game.Log(string.format("[Tick Status] Frame: %d | Ped: %s | HP: %.1f | Armour: %.1f", frameCount, tostring(ped), hp, arm))
        end
    end
end)

Game.Log("=== AMLua test.lua initialized successfully ===")
