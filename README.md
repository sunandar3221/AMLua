# AMLua - Android Mod Loader (AML) Lua Script Loader

**AMLua** adalah plugin shared library (`libAMLua.so`) untuk **Android Mod Loader (AML)** yang berfungsi sebagai Lua Script Loader untuk **Grand Theft Auto: San Andreas** Android.

Proyek ini mendukung arsitektur **arm64-v8a** (64-bit) dan **armeabi-v7a** (32-bit), menggunakan engine **Lua 5.4**, dilengkapi sistem sandboxing keamanan, logging traceback komprehensif, serta binding API game engine GTA SA.

---

## 🚀 Fitur Utama

- **Target Engine:** GTA San Andreas Android (`libGTASA.so`).
- **Arsitektur:** `arm64-v8a` dan `armeabi-v7a`.
- **Integrasi AML:** Menggunakan header resmi AML (`amlmod.h`, `iaml.h`, `logger`, `config`).
- **Game Loop Hooking:** Meng-hook fungsi update engine frame utama (`CGame::Process` / `_ZN5CGame7ProcessEv`) untuk perulangan tick/frame event pada Lua.
- **Embedded Lua 5.4:** Dilengkapi engine Lua 5.4.6 native yang terintegrasi langsung dalam build CMake.
- **Keamanan Sandbox:** Fungsi berisiko tinggi (`os.execute`, `package.loadlib`, `io.popen`, `os.remove`, `os.rename`) dinonaktifkan secara aman.
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
│   ├── lua_bindings.cpp           # Implementasi API Player, Vehicle, Game, Sandboxing & Logger
│   └── main.cpp                   # Plugin entry point (OnModLoad), hooking engine
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
| `Player.GetPed()` | - | Mengembalikan pointer ped pemain utama (`FindPlayerPed(-1)`), atau `nil` jika belum dimuat. |
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

### 4. `AMLua`
Tabel global alternatif yang mencakup seluruh fungsi di atas (`AMLua.Player`, `AMLua.Vehicle`, `AMLua.Game`) serta `AMLua.OnTick(callback)` dan versi plugin `AMLua.Version`.

---

## 🎮 Contoh Script (`scripts/test.lua`)

```lua
Game.Log("=== AMLua test.lua starting ===")

-- Menampilkan teks in-game HUD
Game.PrintText("AMLua Loader Active!", 3000)

local frameCount = 0

-- Mendaftarkan event tick per frame
Game.OnTick(function()
    frameCount = frameCount + 1

    -- Cek status setiap ~60 frame (sekitar 1 detik)
    if frameCount % 60 == 0 then
        local ped = Player.GetPed()
        if ped ~= nil then
            local currentHealth = Player.GetHealth(ped)
            
            -- Jika health pemain di bawah 50, pulihkan otomatis
            if currentHealth > 0.0 and currentHealth < 50.0 then
                Player.SetHealth(ped, 100.0)
                Player.SetArmour(ped, 100.0)
                Game.PrintText("Health Restored to 100!", 2000)
                Game.Log(string.format("Player HP restored from %.1f to 100.0", currentHealth))
            end
        end
    end
end)
```

---

## 🛠️ Cara Build

Kompilasi menggunakan Android NDK toolchain via CMake & Ninja:

### Build arm64-v8a (64-bit)
```powershell
cmake -B build/arm64-v8a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="<NDK_PATH>/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="arm64-v8a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/arm64-v8a
```

### Build armeabi-v7a (32-bit)
```powershell
cmake -B build/armeabi-v7a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="<NDK_PATH>/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="armeabi-v7a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/armeabi-v7a
```

Hasil file `.so` akan berada di `build/<abi>/libAMLua.so` atau `libs/<abi>/libAMLua.so`.

---

## 📲 Cara Pemasangan di HP Android

1. Pasang **Android Mod Loader (AML)** di GTA San Andreas Anda.
2. Salin file `libAMLua.so` (sesuai arsitektur HP Anda, biasanya `arm64-v8a` untuk ponsel modern) ke folder mods AML di Android.
3. Buat folder `scripts` di path:
   `/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/`
4. Taruh script Lua (misal: `test.lua`) ke dalam folder `scripts/` tersebut.
5. Buka game GTA San Andreas. Notifikasi dan log akan muncul secara otomatis.

---

## 📄 Lisensi

Proyek ini dilisensikan di bawah lisensi [MIT License](LICENSE).
