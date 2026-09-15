-- ========================================================================
-- AMLua Example Test Script: test.lua (v1.1.0 Major Update)
-- Target: GTA San Andreas Android (Android Mod Loader)
-- Menguji fitur:
--   1. Http API (Get, Post, Download async/sync, JSON REST API)
--   2. Json API (Json.Decode / Parse, Json.Encode / Stringify)
--   3. Weapon API (Give, SetCurrent, Remove, Weapon constants)
--   4. Device API (Vibrate, Toast, BatteryLevel, AndroidVersion, DisplaySize)
--   5. Audio API (PlaySound, SetRadioStation, GetRadioStation)
--   6. File I/O API (Read, Write, Append, Exists, Delete, List, Paths)
--   7. Vehicle API (Spawn/Create, SetColor, SetEngineState, PopTyre)
--   8. Screen & Camera API (Fade, Shake, Restore)
--   9. Explosion API & Timers (Detik, Auto-Heal, dll.)
-- ========================================================================

Game.Log("=== AMLua test.lua (v1.1.0 Major Update) starting ===")

local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua test.lua loaded with %d active script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

-- ========================================================================
-- 1. FITUR PERANGKAT (Device API: Toast & Getaran)
-- ========================================================================
Device.Toast("AMLua 1.1.0 Active! Internet & Extended APIs Loaded.", true)
Device.Vibrate(80) -- getar halus 80ms

local battery = Device.GetBatteryLevel()
local androidVer = Device.GetAndroidVersion()
Game.Log(string.format("[Device] Android SDK: %d | Battery: %.1f%%", androidVer, battery))

-- ========================================================================
-- 2. FITUR FILE I/O (File API)
-- ========================================================================
local scriptsDir = File.GetScriptsPath()
local sampleFile = scriptsDir .. "/amlua_status.txt"

-- Tulis data ke file
File.Write(sampleFile, "AMLua v1.1.0 status: RUNNING OK\nTimestamp: " .. tostring(os.time()))
if File.Exists(sampleFile) then
    local content = File.Read(sampleFile)
    Game.Log("[File I/O Test] Successfully wrote and read file:\n" .. content)
end

-- ========================================================================
-- 3. FITUR JSON (Json API: Encode & Decode)
-- ========================================================================
local sampleData = {
    modName = "AMLua",
    version = "1.1.0",
    features = { "Http", "Json", "Weapon", "Audio", "Device", "File", "Vehicle" },
    author = "sunandar3221",
    active = true
}

local jsonStr = Json.Encode(sampleData, true) -- pretty print
Game.Log("[JSON Test] Encoded JSON:\n" .. jsonStr)

local decoded = Json.Decode(jsonStr)
if decoded and decoded.modName == "AMLua" then
    Game.Log(string.format("[JSON Test] Decode Success! Mod: %s, Version: %s", decoded.modName, decoded.version))
end

-- ========================================================================
-- 4. FITUR INTERNET / HTTP (Http API: Async & Sync)
-- ========================================================================
-- Cek koneksi internet via HTTP GET async ke public test API (httpbin atau google)
Http.Get("https://httpbin.org/get", function(response)
    if response.ok then
        Game.Log(string.format("[HTTP] GET Success! Status: %d, Length: %d bytes", response.status, #response.body))
        
        -- Parse JSON response
        local resJson = Json.Decode(response.body)
        if resJson and resJson.origin then
            Game.Log("[HTTP] Detected Public IP: " .. resJson.origin)
            Device.Toast("Internet Connected! IP: " .. resJson.origin, false)
        end
    else
        Game.Log("[HTTP] GET Request finished with error / offline: " .. tostring(response.error))
    end
end)

-- ========================================================================
-- 5. FITUR SENJATA (Weapon API)
-- ========================================================================
Game.After(2.0, function()
    local ped = Player.GetPed()
    if ped ~= nil then
        -- Berikan senjata AK-47 dengan 500 peluru
        Weapon.Give(ped, Weapon.AK47, 500)
        -- Berikan senjata Desert Eagle dengan 150 peluru
        Weapon.Give(ped, Weapon.DESERT_EAGLE, 150)
        -- Pegang senjata AK-47
        Weapon.SetCurrent(ped, Weapon.AK47)

        -- Putar suara efek sukses (sound ID 1052 = pickup sound)
        Audio.PlaySound(1052)

        Game.PrintText("~g~Weapon Pack Diberikan!~n~~w~AK-47 & Desert Eagle siap pakai.", 3500)
        Game.Log("Weapons granted to player (AK-47 & Desert Eagle).")
    end
end)

-- ========================================================================
-- 6. FITUR KENDARAAN (Spawn Infernus & Audio Radio)
-- ========================================================================
Game.After(5.0, function()
    local ped = Player.GetPed()
    if ped ~= nil then
        -- Spawn Infernus (Model ID 411) di depan pemain
        local vehHandle, vehPtr = Vehicle.Create(411)
        if vehHandle and vehHandle > 0 then
            -- Ubah warna menjadi Merah (Color 3) & Hitam (Color 0)
            Vehicle.SetColor(vehHandle, 3, 0)

            Game.PrintText("~y~Infernus Berhasil Di-Spawn!~n~~w~Warna: Merah/Hitam.", 3500)
            Game.Log(string.format("Spawned Vehicle Infernus handle: %d, pointer: %p", vehHandle, vehPtr))
        else
            Game.Log("Failed to spawn vehicle 411")
        end
    end
end)

-- ========================================================================
-- 7. FITUR TIMER REAL-TIME (Detik): Auto-Heal & Status
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
-- 8. MANIPULASI GAME (Cuaca, Jam, Uang, Wanted Level)
-- ========================================================================
Game.After(3.0, function()
    Game.SetTime(12, 0)
    Game.SetWeather(0) -- Cerah
    Player.ClearWantedLevel()
    Player.GiveMoney(5000)

    Game.PrintText("~y~AMLua Ready!~n~~w~Sentuh layar tengah (Zona 5) atau Geser 2->8 kayak Cleo!", 4000)
end)

-- ========================================================================
-- 9. TRIGGER ZONA LAYAR SENTUH (CLEO TOUCH SCREEN ZONES 1-9)
-- ========================================================================
-- Layout 3x3 Layar HP (Sama persis seperti Cleo Android):
--   [1] Atas-Kiri    [2] Atas-Tengah    [3] Atas-Kanan
--   [4] Tengah-Kiri  [5] Tengah Layar   [6] Tengah-Kanan
--   [7] Bawah-Kiri   [8] Bawah-Tengah   [9] Bawah-Kanan

-- Contoh A: Trigger Geser Layar dari Atas ke Bawah (Slide 2 -> 8) kayak Menu Cleo!
Touch.OnSlide(2, 8, function(fromZone, toZone, durationMs)
    Game.Log(string.format("[Touch] Gesture Cleo Terdeteksi: Geser %d ke %d (%.0f ms)", fromZone, toZone, durationMs))
    Device.Vibrate(100) -- Getar HP saat gestur berhasil
    Audio.PlaySound(1058) -- Sound effect bell

    -- Berikan senjata Minigun & Rocket Launcher saat menu gestur Cleo dibuka!
    local ped = Player.GetPed()
    if ped ~= nil then
        Weapon.Give(ped, Weapon.MINIGUN, 1000)
        Weapon.Give(ped, Weapon.ROCKETLAUNCHER, 50)
        Game.PrintText("~g~Cleo Gesture (2->8) Triggered!~n~~w~Heavy Weapons Unlocked: Minigun & RPG!", 3500)
        Device.Toast("AMLua: Menu Cleo (Geser 2 ke 8) Aktif!", false)
    end
end)

-- Contoh B: Trigger Sentuh Tengah Layar (Zona 5) untuk Pulihkan Darah & Armor Instan
Touch.OnPress(5, function(zone, x, y)
    Game.Log(string.format("[Touch] Zona %d ditekan pada koordinat (%d, %d)", zone, x, y))
    local ped = Player.GetPed()
    if ped ~= nil then
        Player.SetHealth(ped, 100.0)
        Player.SetArmour(ped, 100.0)
        Audio.PlaySound(1052) -- Pickup sound
        Device.Vibrate(50)
        Game.PrintText("~b~Zona 5 (Tengah) Ditekan!~n~~w~Health & Armour Full 100%!", 2500)
    end
end)

-- Contoh C: Trigger Kombinasi Tombol Multi-Zona (Zona 4 & 6 Ditekan Bersamaan)
Touch.OnCombo({4, 6}, function(zones)
    Game.Log("[Touch] Combo Tombol Zona 4 & 6 Ditekan Bersamaan!")
    Device.Vibrate(150)
    -- Spawn motor NRG-500 (ID 522) seketika
    local veh = Vehicle.Create(522)
    if veh and veh > 0 then
        Vehicle.SetColor(veh, 6, 1) -- Kuning / Putih
        Game.PrintText("~y~Combo 4+6 Aktif!~n~~w~NRG-500 Di-Spawn di Depan Pemain!", 3500)
    end
end)

-- Contoh D: Trigger Global Cleo Script Hook (Otomatis dipanggil tanpa perlu register)
function onTouchZone(zone, eventType, x, y)
    -- eventType: "press", "release", "doubletap"
    if eventType == "doubletap" and zone == 1 then
        -- Double tap di pojok kiri atas (Zona 1): Ledakan aman di langit
        Explosion.AtPlayer(Explosion.TINY, 0, 5, 5)
        Game.PrintText("~r~Double-Tap Zona 1:~n~~w~Fireworks Explosion!", 2000)
    end
end

function onTouchSlide(fromZone, toZone, durationMs)
    Game.Log(string.format("[Global Hook] onTouchSlide dari Zona %d ke %d", fromZone, toZone))
end

Game.Log("=== AMLua test.lua (v1.1.0 + Cleo Touch Zones) initialized successfully ===")
