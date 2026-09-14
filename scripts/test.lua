-- ========================================================================
-- AMLua Example Test Script: test.lua (v1.0.8)
-- Target: GTA San Andreas Android (Android Mod Loader)
-- Menguji fitur:
--   1. Explosion API (Ledakan terarah, efek visual ledakan, ledakan di player/kendaraan)
--   2. Vehicle API (Cek kendaraan, kecepatan km/h, boost, perbaikan, kunci pintu)
--   3. Teleport & Position (Ground Z, teleport player & kendaraan)
--   4. Game Manipulation (Cuaca, jam/waktu, wanted level, uang, speed game)
--   5. Timer Real-Time (Game.Every / Game.After / setInterval / setTimeout)
-- ========================================================================

Game.Log("=== AMLua test.lua (v1.0.8) starting ===")

-- Tampilkan daftar mod yang dimuat di logcat / amlua.log
local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua test.lua loaded with %d active script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

-- ========================================================================
-- 1. FITUR TIMER REAL-TIME (Detik): Auto-Heal & Status
-- ========================================================================
local healCooldownSec = 0.0

Game.Every(1.0, function()
    if healCooldownSec > 0.0 then
        healCooldownSec = healCooldownSec - 1.0
    end

    local ped = Player.GetPed()
    if ped ~= nil and healCooldownSec <= 0.0 then
        local currentHealth = Player.GetHealth(ped)

        -- Pulihkan darah & armor otomatis jika HP kritis (< 50)
        if currentHealth > 0.0 and currentHealth < 50.0 then
            Player.SetHealth(ped, 100.0)
            Player.SetArmour(ped, 100.0)
            healCooldownSec = 5.0

            Game.PrintText("~g~Health & Armour Restored!~n~~w~AMLua Auto-Heal Aktif.", 3000)
            Game.Log(string.format("Player HP restored from %.1f to 100.0", currentHealth))
        end
    end
end)

-- ========================================================================
-- 2. MANIPULASI GAME (Cuaca, Jam, Uang, Wanted Level)
-- ========================================================================
Game.After(3.0, function()
    -- Set jam game menjadi 12:00 siang
    Game.SetTime(12, 0)
    local hour, min = Game.GetTime()
    Game.Log(string.format("Game Clock set to: %02d:%02d", hour, min))

    -- Set cuaca cerah (WEATHER_EXTRASUNNY_LA = 0)
    Game.SetWeather(0)
    Game.Log("Game Weather set to EXTRASUNNY_LA (0)")

    -- Bersihkan Wanted Level bintang polisi
    Player.ClearWantedLevel()
    Game.Log("Player Wanted Level cleared to 0.")

    -- Berikan bonus uang selamat datang
    Player.GiveMoney(2500)
    local money = Player.GetMoney()
    Game.Log(string.format("Player Money: $%d", money))

    Game.PrintText("~y~AMLua v1.0.8 Siap!~n~~w~Cuaca: Cerah | Jam: 12:00~n~~g~+$2,500 Bonus Masuk!", 4000)
end)

-- ========================================================================
-- 3. MANIPULASI KENDARAAN (Speedometer & Auto-Repair)
-- ========================================================================
Game.Every(2.0, function()
    if Player.IsInVehicle() then
        local veh = Vehicle.GetPlayerVehicle()
        if veh ~= nil then
            local speedKmh = Vehicle.GetSpeed(veh)
            local health = Vehicle.GetHealth(veh)

            -- Log kecepatan kendaraan saat sedang dikendarai
            Game.Log(string.format("[Vehicle HUD] Speed: %.1f km/h | Health: %.1f", speedKmh, health))

            -- Auto-repair kendaraan jika rusak parah (health < 400)
            if health > 0.0 and health < 400.0 then
                Vehicle.Repair(veh)
                Game.PrintText("~g~Kendaraan Diperbaiki Otomatis!~n~~w~Vehicle Repaired to 1000 HP.", 2500)
                Game.Log("Vehicle repaired via AMLua Vehicle.Repair API.")
            end
        end
    end
end)

-- ========================================================================
-- 4. CONTOH DEMO TELEPORT & LEDAKAN (Opsional / Helper Function)
-- ========================================================================
-- Fungsi bantuan untuk teleportasi aman ke koordinat tertentu
function TeleportSafe(x, y)
    local groundZ = Game.GetGroundZ(x, y)
    Game.Teleport(x, y, groundZ + 1.0)
    Game.PrintText(string.format("~g~Teleported to:~n~~w~X: %.1f, Y: %.1f, Z: %.1f", x, y, groundZ + 1.0), 3000)
    Game.Log(string.format("TeleportSafe executed to (%.2f, %.2f, %.2f)", x, y, groundZ + 1.0))
end

-- Fungsi bantuan untuk membuat ledakan di depan pemain
function CreateFrontExplosion(distance)
    local dist = distance or 15.0
    local x, y, z = Player.GetPosition()
    local heading = Player.GetHeading() -- dalam derajat
    local rad = math.rad(heading)

    local expX = x - math.sin(rad) * dist
    local expY = y + math.cos(rad) * dist
    local groundZ = Game.GetGroundZ(expX, expY)
    local expZ = (groundZ and groundZ > -900.0) and (groundZ + 0.8) or (z + 0.8)

    -- Buat ledakan tipe 3 (CAR explosion) dengan radius 10, ada suara & getaran kamera
    local ok = Explosion.Create(expX, expY, expZ, 3, 10.0, true, 1.0)
    if ok then
        Game.PrintText("~r~KABOOM!~n~~w~Ledakan visual berhasil dibuat!", 2500)
        Game.Log(string.format("Explosion spawned successfully at (%.2f, %.2f, %.2f)", expX, expY, expZ))
    else
        Game.Log("Explosion.Create returned false")
    end
end

-- Demonstrasi otomatis ledakan aman di depan pemain setelah 8 detik
Game.After(8.0, function()
    local ped = Player.GetPed()
    if ped ~= nil then
        CreateFrontExplosion(18.0)
    end
end)

Game.Log("=== AMLua test.lua (v1.0.8) initialized successfully ===")
