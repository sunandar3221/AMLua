# AMLua - Android Mod Loader (AML) Lua Script Loader

**AMLua** adalah plugin shared library (`libAMLua.so`) untuk **Android Mod Loader (AML)** yang berfungsi sebagai Lua Script Loader untuk **Grand Theft Auto: San Andreas** Android.

Proyek ini mendukung arsitektur **arm64-v8a** (64-bit) dan **armeabi-v7a** (32-bit), menggunakan engine **Lua 5.4**, dilengkapi sistem sandboxing keamanan, logging traceback komprehensif, fitur **In-Game Mod List Viewer**, serta binding API game engine GTA SA.

---

## 🚀 Fitur Utama

- **Target Engine:** GTA San Andreas Android (`libGTASA.so`).
- **Arsitektur:** `arm64-v8a` dan `armeabi-v7a`.
- **Integrasi AML:** Menggunakan header resmi AML (`amlmod.h`, `iaml.h`, `logger`, `config`).
- **In-Game Mod List Viewer:**
  - **Gesture Sentuh:** Cukup ketuk dua kali (double-tap) di bagian atas layar (area status bar) atau tap menggunakan 2 jari untuk menampilkan daftar mod Lua yang sedang aktif langsung di layar.
  - **Lua API:** Dapat dipanggil kapan saja dari script menggunakan `AMLua.ShowScriptList()` atau `Game.ShowScriptList()`.
  - **Daftar Array:** Ambil daftar nama script dalam bentuk tabel Lua via `AMLua.GetLoadedScripts()`.
- **Game Loop Hooking:** Meng-hook fungsi update engine frame utama (`CGame::Process` / `_ZN5CGame7ProcessEv`) untuk perulangan tick/frame event pada Lua.
- **Embedded Lua 5.4:** Dilengkapi engine Lua 5.4.6 native yang terintegrasi langsung dalam build CMake.
- **Keamanan Sandbox:** Fungsi berisiko tinggi (`os.execute`, `package.loadlib`, `io.popen`, `os.remove`, `os.rename`) dinonaktifkan secara aman.
- **New Game Crash Protection:**
  - Validasi pointer entitas dan vtable (`IsValidGameObject`) agar aman saat transisi New Game dan pergantian scene.
  - Struktur offset `CPed` & `CVehicle` diverifikasi presisi untuk 32-bit dan 64-bit.
  - Konversi string GXT 16-bit untuk `CMessages::AddMessageJumpQ` sehingga HUD text tidak menyebabkan memory corruption.
- **Debugging & Error Handling:** Setiap pemanggilan script dibungkus dengan `lua_pcall` dan traceback error handler (`luaL_traceback`). Traceback otomatis dicatat ke **Android Logcat** (`AMLua`) dan file log `amlua.log`.
- **Automatic Script Loading:** Secara otomatis memindai dan memuat semua file `.lua` dari folder penyimpanan game:
  `/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/`

---

## 📂 Struktur Proyek

```text
AMLua/
├── CMakeLists.txt                 # Konfigurasi CMake build C++17 & NDK toolchain
├── LICENSE                        # Lisensi MIT
├── README.md                      # Dokumentasi proyek
├── jni/
│   ├── aml/                       # Header resmi AML (aml.h, mod/aml.h, iaml.h, logger, config)
│   ├── lua/                       # Source C & header Lua 5.4.6
│   ├── lua_bindings.h             # Header binding API Lua & GTA SA
│   ├── lua_bindings.cpp           # Implementasi API Player, Vehicle, Game, Sandboxing & Mod List
│   └── main.cpp                   # Plugin entry point (OnModLoad), hooking engine & touch input
├── scripts/
│   └── test.lua                   # Script contoh: tick event, cek status pemain, auto-heal, in-game text
└── libs/
    ├── arm64-v8a/libAMLua.so      # Prebuilt binary 64-bit
    └── armeabi-v7a/libAMLua.so    # Prebuilt binary 32-bit
```

---

## 📜 Dokumentasi Lua API

### 1. `Player`
| Fungsi | Parameter | Deskripsi |
| :--- | :--- | :--- |
| `Player.GetPed()` | - | Mengembalikan pointer ped pemain utama (`FindPlayerPed(-1)`), atau `nil` jika belum aktif di game. |
| `Player.SetHealth(ped, hp)` | `ped`, `hp (number)` | Mengatur health pemain. |
| `Player.GetHealth(ped)` | `ped` | Mengambil nilai health pemain. |
| `Player.SetArmour(ped, armour)` | `ped`, `armour (number)` | Mengatur armour pemain. |
| `Player.GetArmour(ped)` | `ped` | Mengambil nilai armour pemain. |

### 2. `Vehicle`
| Fungsi | Parameter | Deskripsi |
| :--- | :--- | :--- |
| `Vehicle.Repair(vehiclePtr)` | `vehiclePtr` | Memperbaiki kendaraan (`CVehicle::Fix`) dan mereset health ke 1000.0. |
| `Vehicle.GetHealth(vehiclePtr)` | `vehiclePtr` | Mengambil health kendaraan. |
| `Vehicle.SetHealth(vehiclePtr, hp)`| `vehiclePtr`, `hp (number)` | Mengatur health kendaraan. |

### 3. `Game`
| Fungsi | Parameter | Deskripsi |
| :--- | :--- | :--- |
| `Game.PrintText(text, timeMs)` | `text (string)`, `[timeMs]` | Menampilkan pesan teks di HUD in-game (`CMessages` / `CHud`) dan Android Toast. |
| `Game.Log(message)` | `message (string)` | Menulis pesan log ke Logcat Android dan `amlua.log`. |
| `Game.OnTick(callback)` | `callback (function)` | Mendaftarkan fungsi yang dipanggil pada setiap frame game loop. |
| `Game.ShowScriptList()` | - | Membuka dialog visual daftar mod Lua yang sedang aktif. |
| `Game.GetLoadedScripts()` | - | Mengembalikan tabel array berisi nama-nama file `.lua` yang dimuat. |

### 4. `AMLua`
Tabel global alternatif yang mencakup seluruh fungsi di atas (`AMLua.Player`, `AMLua.Vehicle`, `AMLua.Game`) serta `AMLua.OnTick(callback)`, `AMLua.ShowScriptList()`, `AMLua.GetLoadedScripts()`, dan `AMLua.Version`.

---

## 📱 Cara Melihat Daftar Mod Lua di GTA SA

Ada 2 cara praktis:
1. **Gesture Sentuh (In-Game):**
   - Ketuk dua kali (**Double-Tap**) di **area atas layar** (dekat status bar / health bar).
   - Atau tap dengan **2 jari** secara bersamaan di layar.
   - Kotak dialog AMLua akan muncul menampilkan daftar seluruh mod Lua yang aktif dan lokasinya.
2. **Melalui Script Lua:**
   ```lua
   -- Tampilkan dialog daftar script
   AMLua.ShowScriptList()

   -- Atau ambil daftarnya untuk diproses sendiri
   local mods = AMLua.GetLoadedScripts()
   for i, name in ipairs(mods) do
       Game.Log("Mod: " .. name)
   end
   ```

---

## 🎮 Contoh Script (`scripts/test.lua`)

```lua
Game.Log("=== AMLua test.lua starting ===")

local scripts = AMLua.GetLoadedScripts()
Game.Log(string.format("AMLua loaded with %d script(s):", #scripts))
for i, name in ipairs(scripts) do
    Game.Log(string.format("  [%d] %s", i, name))
end

local frameCount = 0
local greeted = false

Game.OnTick(function()
    frameCount = frameCount + 1

    local ped = Player.GetPed()
    if ped ~= nil then
        if not greeted then
            greeted = true
            Game.PrintText("AMLua Active! Double-tap top screen for Mod List.", 4000)
        end

        if frameCount % 60 == 0 then
            local currentHealth = Player.GetHealth(ped)
            if currentHealth > 0.0 and currentHealth < 50.0 then
                Player.SetHealth(ped, 100.0)
                Player.SetArmour(ped, 100.0)
                Game.PrintText("Health Restored to 100!", 2000)
            end
        end
    end
end)
```

---

## 🛠️ Cara Build

Kompilasi menggunakan Android NDK toolchain lokal via CMake & Ninja:

### Build arm64-v8a (64-bit)
```powershell
cmake -B build/arm64-v8a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="arm64-v8a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/arm64-v8a
```

### Build armeabi-v7a (32-bit)
```powershell
cmake -B build/armeabi-v7a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="armeabi-v7a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/armeabi-v7a
```

---

## 📲 Cara Pemasangan di HP Android

1. Pasang **Android Mod Loader (AML)** di GTA San Andreas Anda.
2. Salin file `libAMLua.so` (pilih arsitektur `arm64-v8a` untuk HP 64-bit modern, atau `armeabi-v7a` untuk 32-bit) ke folder mods AML di Android (`/Android/data/com.rockstargames.gtasa/mods/` atau via AML app).
3. Buat folder `scripts` di:
   `/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/`
4. Taruh script Lua (misal: `test.lua`) ke dalam folder `scripts/` tersebut.
5. Buka game GTA San Andreas. Notifikasi akan muncul, dan Anda dapat melihat list mod kapan saja dengan double-tap bagian atas layar!

---

## 📄 Lisensi

Proyek ini dilisensikan di bawah [Lisensi MIT](LICENSE).
