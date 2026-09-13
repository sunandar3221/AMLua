-- ========================================================================
-- AMLua Example Test Script: test.lua
-- Target: GTA San Andreas Android (Android Mod Loader)
-- ========================================================================

Game.Log("=== AMLua test.lua starting ===")

-- Tampilkan daftar mod yang dimuat di logcat / amlua.log
local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua test.lua loaded with %d active script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

-- ========================================================================
-- FITUR BARU: Timer Berbasis Detik (Game.Every / Game.After / SetInterval)
-- Tidak perlu lagi menghitung FPS atau frameCount! Cukup sebutkan detik.
-- 1 detik selalu tepat 1 detik, baik di HP kentang maupun HP spek tinggi.
-- ========================================================================

local healCooldownSec = 0.0

-- 1. Game.Every(detik, function): Berjalan berulang kali setiap X detik
Game.Every(1.0, function()
    -- Kurangi cooldown waktu nyata (1 detik per siklus)
    if healCooldownSec > 0.0 then
        healCooldownSec = healCooldownSec - 1.0
    end

    local ped = Player.GetPed()
    if ped ~= nil and healCooldownSec <= 0.0 then
        local currentHealth = Player.GetHealth(ped)

        -- Pulihkan darah & armor otomatis jika HP kurang dari 50
        if currentHealth > 0.0 and currentHealth < 50.0 then
            Player.SetHealth(ped, 100.0)
            Player.SetArmour(ped, 100.0)
            healCooldownSec = 5.0 -- Cooldown selama 5 detik

            -- Tampilkan pesan di dialog pojok kanan atas
            Game.PrintText("~g~Health Restored to 100!~n~~w~AMLua Auto-Heal (Every 1s).", 3000)
            Game.Log(string.format("Player HP restored from %.1f to 100.0 via Game.Every(1.0)", currentHealth))
        end
    end
end)

-- 2. Game.After(detik, function): Berjalan sekali setelah X detik (delay)
Game.After(4.0, function()
    Game.Log("AMLua: 4 detik telah berlalu sejak script dimuat!")
end)

-- ========================================================================
-- Game.OnTick tetap didukung dan kini menerima parameter 'dt' (delta time)
-- dt = durasi waktu nyata (dalam pecahan detik) sejak frame sebelumnya
-- ========================================================================
local heartbeatTimer = 0.0

Game.OnTick(function(dt)
    heartbeatTimer = heartbeatTimer + dt

    -- Log status setiap 10 detik tanpa terpengaruh naik/turunnya FPS
    if heartbeatTimer >= 10.0 then
        heartbeatTimer = 0.0
        local ped = Player.GetPed()
        if ped ~= nil then
            local hp = Player.GetHealth(ped)
            local arm = Player.GetArmour(ped)
            Game.Log(string.format("[Heartbeat] CJ Status | HP: %.1f | Armour: %.1f | Frame Delta: %.3fs", hp, arm, dt))
        end
    end
end)

Game.Log("=== AMLua test.lua initialized successfully ===")
