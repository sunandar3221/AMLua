# AMLua - Android Mod Loader (AML) Lua Script Loader
> **Lua Scripting Runtime & In-Game Mod Loader untuk Grand Theft Auto: San Andreas Android**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://android.com)
[![Architecture](https://img.shields.io/badge/Arch-arm64--v8a%20%7C%20armeabi--v7a-orange.svg)](#)
[![Lua Version](https://img.shields.io/badge/Lua-5.4.6-blue.svg)](https://www.lua.org)

**AMLua (Android Mod Lua)** adalah plugin shared library (`libAMLua.so`) resmi berbasis **Android Mod Loader (AML)** yang dirancang untuk memuat, mengelola, dan mengeksekusi script **Lua** secara dinamis langsung di dalam game **Grand Theft Auto: San Andreas** (Android).

Dengan AMLua, para modder tidak perlu lagi melakukan re-kompilasi kode C/C++ yang rumit atau menggunakan toolchain NDK setiap kali ingin membuat atau menguji mod. Cukup tulis script `.lua`, taruh di folder `scripts/`, dan script akan langsung aktif di dalam game!

---

## 📑 Daftar Isi
1. [Apa itu AMLua?](#-apa-itu-amlua)
   - [Latar Belakang & Arsitektur](#latar-belakang--arsitektur)
   - [Cara Kerja AMLua](#cara-kerja-amlua)
   - [Fitur Utama & Keunggulan](#fitur-utama--keunggulan)
2. [Dokumentasi Lengkap API AMLua](#-dokumentasi-lengkap-api-amlua)
   - [Modul Player](#1-modul-player)
   - [Modul Vehicle](#2-modul-vehicle)
   - [Modul Game](#3-modul-game)
   - [Modul AMLua](#4-modul-amlua)
   - [Daftar Kode Warna & Format Dialog GTA SA](#5-daftar-kode-warna--format-dialog-gta-sa)
3. [Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)](#-belajar-kilat-amlua-sampai-bisa-tutorial-lengkap-dari-nol)
   - [Pelajaran 1: Struktur Folder & File Script](#pelajaran-1-struktur-folder--file-script)
   - [Pelajaran 2: Hello World (Script Pertama Anda)](#pelajaran-2-hello-world-script-pertama-anda)
   - [Pelajaran 3: Memahami Game Loop (OnTick) & Frame Timing](#pelajaran-3-memahami-game-loop-ontick--frame-timing)
   - [Pelajaran 4: Membuat Mod Godmode (Darah Kebal)](#pelajaran-4-membuat-mod-godmode-darah-kebal)
   - [Pelajaran 5: Membuat Mod Auto-Repair Kendaraan](#pelajaran-5-membuat-mod-auto-repair-kendaraan)
   - [Pelajaran 6: Memanggil Script Lain Berkali-kali (Modular Modding)](#pelajaran-6-memanggil-script-lain-berkali-kali-modular-modding)
   - [Pelajaran 7: Debugging & Mengatasi Error](#pelajaran-7-debugging--mengatasi-error)
4. [In-Game Mod List Viewer (Melihat Daftar Mod Aktif)](#-in-game-mod-list-viewer)
5. [Panduan Pemasangan di HP Android](#-panduan-pemasangan-di-hp-android)
6. [Kompilasi / Build dari Source Code](#-kompilasi--build-dari-source-code)
7. [Lisensi](#-lisensi)

---

## 💡 Apa itu AMLua?

### Latar Belakang & Arsitektur
Pada ekosistem modding GTA San Andreas Android, terdapat dua metode tradisional yang memiliki keterbatasan:
1. **CLEO Android**: Meskipun populer, CLEO berbasis bytecode SCM kuno yang kaku, lambat, sulit di-debug, dan sering kali crash saat versi game diperbarui.
2. **Plugin C++ AML**: Sangat cepat dan bertenaga, namun membutuhkan Android NDK, compiler Clang/CMake di PC, serta pengetahuan bahasa C++ tingkat lanjut. Pembuatan mod kecil sekalipun membutuhkan proses build library binary terpisah.

**AMLua menjembatani kedua dunia ini!** AMLua menyediakan runtime modern bertenaga **Lua 5.4.6** native yang berjalan langsung di atas memori game GTA San Andreas Android, dieksekusi bersamaan dengan loop game asli tanpa beban emulator.

```text
┌────────────────────────────────────────────────────────┐
│             GTA San Andreas Android (libGTASA.so)      │
│  - Render Loop       - Physics / World   - UI / HUD    │
└───────────────────────────▲────────────────────────────┘
                            │ Hooks & Symbol Binding
┌───────────────────────────┴────────────────────────────┐
│                   AMLua (libAMLua.so)                  │
│  - AML Integration     - Safe Entity Offsets (32/64)   │
│  - CGame::Process Hook - CHud::SetHelpMessage Bridge   │
│  - Native Lua 5.4.6 VM - Multi-Script Manager          │
└───────────────────────────▲────────────────────────────┘
                            │ Scans & Loads
┌───────────────────────────┴────────────────────────────┐
│      Folder /sdcard/Android/data/.../files/scripts/    │
│  - mod_darah.lua       - mod_mobil.lua   - menu.lua    │
└────────────────────────────────────────────────────────┘
```

### Cara Kerja AMLua
1. **Inisialisasi**: Saat GTA San Andreas dimuat, **Android Mod Loader (AML)** memanggil fungsi `OnModLoad()` pada `libAMLua.so`.
2. **Symbol Hooking**: AMLua mencari alamat memori internal `libGTASA.so` untuk menemukan fungsi penting seperti loop game (`CGame::Process`), pencari pemain (`FindPlayerPed`), dan sistem dialog native (`CHud::SetHelpMessage`).
3. **VM Initialization**: AMLua menyalakan runtime engine Lua 5.4.6, mendaftarkan seluruh API modul (`Player`, `Vehicle`, `Game`, `AMLua`), dan mengaktifkan proteksi Sandbox.
4. **Auto-Discovery**: AMLua otomatis membaca isi folder `scripts/`, lalu mengeksekusi semua file `.lua` secara berurutan.
5. **Tick Event Dispatching**: Setiap frame game berjalan, AMLua memanggil seluruh fungsi `Game.OnTick` yang didaftarkan oleh script-script Anda.
6. **In-Game Dialog**: AMLua menyapa pemain dengan dialog sambutan resmi di pojok kanan atas layar begitu karakter siap dimainkan.

### Fitur Utama & Keunggulan
- **Dual Architecture (32-bit & 64-bit)**: Mendukung penuh `arm64-v8a` (Android 64-bit modern) maupun `armeabi-v7a` (Android 32-bit klasik).
- **Native Top-Right Dialog**: Menggunakan dialog box bawaan GTA San Andreas (`CHud::SetHelpMessage`) yang estetik di pojok kanan atas dengan teks khas GTA dan dukungan kode warna penuh (`~g~`, `~y~`, `~w~`, `~n~`).
- **Multi-Calling Support**: Bebas memanggil `Game.PrintText`, `dofile()`, atau `require()` berkali-kali tanpa dibatasi oleh engine.
- **Gesture Mod List Viewer**: Buka daftar mod aktif di layar cukup dengan double-tap di bagian atas layar atau tap dengan 2 jari.
- **Zero-Crash Memory Safety**: Dilengkapi validasi memori kernel Linux (`IsValidMemory`) dan verifikasi pointer entitas game (`IsValidGameObject`) agar aman dari crash New Game, cutscene, dan transisi interior.
- **Traceback Logging**: Setiap error sintaks atau runtime pada script Lua langsung dicatat lengkap dengan nomor baris dan stack trace ke `amlua.log` dan Android Logcat.

---

## 📜 Dokumentasi Lengkap API AMLua

Semua fungsi di bawah ini dapat diakses secara global di dalam script Lua Anda.

### 1. Modul `Player`
Modul untuk berinteraksi dengan karakter pemain (CJ).

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Player.GetPed()` | *(tidak ada)* | `lightuserdata` (pointer ped) atau `nil` | Mengambil pointer memori karakter pemain saat ini. Mengembalikan `nil` jika pemain belum spawn atau sedang dalam menu/cutscene. |
| `Player.GetHealth(ped)` | `ped` (pointer ped) | `number` (float) | Mengambil jumlah HP pemain (normalnya 0.0 - 100.0, atau hingga 200.0+ jika ada upgrade). |
| `Player.SetHealth(ped, hp)` | `ped`, `hp` (number) | *(tidak ada)* | Mengatur jumlah HP pemain ke nilai tertentu (misal: 100.0 untuk darah penuh, atau 1000.0). |
| `Player.GetArmour(ped)` | `ped` (pointer ped) | `number` (float) | Mengambil nilai rompi anti-peluru (armour) pemain (0.0 - 100.0). |
| `Player.SetArmour(ped, armour)` | `ped`, `armour` (number) | *(tidak ada)* | Mengatur nilai rompi anti-peluru pemain ke nilai tertentu. |

#### Contoh Penggunaan:
```lua
local ped = Player.GetPed()
if ped ~= nil then
    local hp = Player.GetHealth(ped)
    local armour = Player.GetArmour(ped)
    Game.Log(string.format("Status CJ - HP: %.1f | Armour: %.1f", hp, armour))

    -- Set darah & armor penuh
    Player.SetHealth(ped, 100.0)
    Player.SetArmour(ped, 100.0)
end
```

---

### 2. Modul `Vehicle`
Modul untuk memanipulasi kendaraan (mobil, motor, sepeda, pesawat, helikopter, perahu).

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Vehicle.GetHealth(vehiclePtr)` | `vehiclePtr` (pointer kendaraan) | `number` (float) | Mengambil nilai daya tahan / health kendaraan (1000.0 = sempurna, 250.0 = mulai terbakar). |
| `Vehicle.SetHealth(vehiclePtr, hp)`| `vehiclePtr`, `hp` (number) | *(tidak ada)* | Mengatur nilai health kendaraan. |
| `Vehicle.Repair(vehiclePtr)` | `vehiclePtr` (pointer kendaraan) | *(tidak ada)* | Memperbaiki seluruh kerusakan visual kendaraan (bodi, pintu, kaca, ban) via `CVehicle::Fix` dan mereset HP ke 1000.0. |

#### Contoh Penggunaan:
```lua
-- Memperbaiki kendaraan yang sedang digunakan
Vehicle.Repair(vehPtr)
Game.PrintText("~g~Kendaraan Berhasil Diperbaiki!", 3000)
```

---

### 3. Modul `Game`
Modul inti untuk antarmuka game, UI dialog, timer, event loop, dan manajemen script.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Game.PrintText(text, timeMs)` | `text` (string), `[timeMs]` (opsional, default 3000) | *(tidak ada)* | Menampilkan teks pada kotak dialog native GTA SA di **pojok kanan atas layar**. Mendukung kode warna GTA. Dapat dipanggil berkali-kali. |
| `Game.ShowHelpMessage(text, timeMs)` | `text`, `[timeMs]` | *(tidak ada)* | Alias identik untuk `Game.PrintText`. |
| `Game.Log(message)` | `message` (string) | *(tidak ada)* | Menulis teks ke file log `amlua.log` dan Android Logcat dengan tag `AMLua`. |
| `Game.OnTick(callback)` | `callback` (function) | *(tidak ada)* | Mendaftarkan fungsi yang akan dipanggil secara berkala pada **setiap frame** perulangan game (`CGame::Process`). |
| `Game.ShowScriptList()` | *(tidak ada)* | *(tidak ada)* | Membuka dialog visual native di pojok kanan atas yang menampilkan daftar semua mod Lua yang aktif. |
| `Game.GetLoadedScripts()` | *(tidak ada)* | `table` (array of string) | Mengembalikan daftar nama file script `.lua` yang berhasil dimuat oleh AMLua. |
| `Game.DoFile(filename)` | `filename` (string) | *(hasil kembalian script)* | Menjalankan file script `.lua` lain. Path relatif secara otomatis merujuk ke folder `scripts/`. Dapat dipanggil berkali-kali! |
| `Game.RunString(code)` | `code` (string) | *(hasil kembalian code)* | Mengevaluasi dan menjalankan baris kode Lua dari bentuk string secara dinamis. |
| `Game.ReloadScripts()` | *(tidak ada)* | *(tidak ada)* | Memuat ulang seluruh file script di folder `scripts/` tanpa perlu me-restart game. |

---

### 4. Modul `AMLua`
Tabel namespace alternatif yang mewadahi seluruh modul di atas:
- `AMLua.Player` -> identik dengan `Player`
- `AMLua.Vehicle` -> identik dengan `Vehicle`
- `AMLua.Game` -> identik dengan `Game`
- `AMLua.OnTick(callback)` -> identik dengan `Game.OnTick`
- `AMLua.DoFile(filename)` -> identik dengan `Game.DoFile`
- `AMLua.RunString(code)` -> identik dengan `Game.RunString`
- `AMLua.ReloadScripts()` -> identik dengan `Game.ReloadScripts`
- `AMLua.ShowScriptList()` -> membuka dialog daftar mod
- `AMLua.GetLoadedScripts()` -> mengambil array nama mod
- `AMLua.Version` -> string versi saat ini (misal: `"1.0.3"`)
- `dofile(filename)` -> fungsi global bawaan yang otomatis mencari file di folder `scripts/`

---

### 5. Daftar Kode Warna & Format Dialog GTA SA
Teks yang dimasukkan ke `Game.PrintText()` mendukung kode pemformatan warna native GTA:

| Kode Format | Warna / Efek | Contoh Hasil |
| :---: | :--- | :--- |
| `~g~` | Hijau (Green) | `~g~Teks Hijau` |
| `~r~` | Merah (Red) | `~r~Peringatan Bahaya!` |
| `~b~` | Biru (Blue) | `~b~Teks Biru` |
| `~y~` | Kuning (Yellow) | `~y~AMLua Active!` |
| `~w~` | Putih (White / Normal) | `~w~Teks Standar` |
| `~p~` | Ungu (Purple) | `~p~Teks Ungu` |
| `~l~` | Hitam (Black) | `~l~Teks Gelap` |
| `~n~` | Baris Baru (Newline / Enter) | `Baris 1~n~Baris 2` |

> 💡 **Tips Penggunaan Teks:**
> Selalu tutup warna khusus dengan `~w~` jika baris berikutnya ingin berwarna putih normal:
> ```lua
> Game.PrintText("~g~SUKSES!~n~~w~Darah Anda telah dipulihkan.", 3000)
> ```

---

## 🎓 Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)

Selamat datang di panduan kilat AMLua! Di bagian ini, kita akan belajar langkah demi langkah dari nol sampai Anda mahir membuat modifikasi GTA San Andreas Android Anda sendiri.

---

### Pelajaran 1: Struktur Folder & File Script
Sebelum menulis kode, pahami di mana script Anda disimpan.

1. Hubungkan HP ke PC atau gunakan aplikasi File Manager (misal: **ZArchiver** / **MT Manager**).
2. Buka direktori penyimpanan Android berikut:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/
   ```
   *(Jika folder `scripts` belum ada, buat folder baru bernama `scripts` di dalam folder `files`).*
3. Semua file berekstensi `.lua` yang Anda simpan di dalam folder `scripts` akan otomatis dimuat oleh AMLua saat game dibuka.
4. Anda dapat mengedit script langsung di HP Android menggunakan text editor seperti **Acode**, **QuickEdit**, atau di PC menggunakan **VS Code**.

---

### Pelajaran 2: Hello World (Script Pertama Anda)
Mari kita buat script pertama yang akan memunculkan pesan di layar dan menulis pesan ke log.

1. Di dalam folder `scripts/`, buat file baru bernama `hello.lua`.
2. Isi file tersebut dengan kode berikut:
   ```lua
   -- hello.lua - Script Pertama Saya
   Game.Log("Halo dari hello.lua!")

   -- Tampilkan dialog di pojok kanan atas selama 4 detik (4000 ms)
   Game.PrintText("~y~Halo Dunia!~n~~w~Mod AMLua pertama saya berhasil berjalan.", 4000)
   ```
3. Buka game GTA San Andreas. Begitu game dimuat, Anda akan melihat pesan di pojok kanan atas layar!

---

### Pelajaran 3: Memahami Game Loop (OnTick) & Frame Timing
Di game GTA San Andreas, gambar di layar di-refresh antara 30 sampai 60 kali per detik (FPS). 
Fungsi `Game.OnTick` memungkinkan kode Anda dieksekusi pada setiap frame tersebut.

> ⚠️ **PENTING: Jangan Gunakan Loop `while true do`!**
> Di lingkungan game native, loop tak berujung (`while true do ... end`) akan membuat game langsung freeze (hang).
> Sebagai gantinya, gunakan `Game.OnTick` bersama variabel penghitung frame.

```lua
-- Contoh menghitung detik menggunakan frame (60 frame ≈ 1 detik)
local frameCount = 0

Game.OnTick(function()
    frameCount = frameCount + 1

    -- Setiap 180 frame (kurang lebih 3 detik sekali)
    if frameCount % 180 == 0 then
        Game.Log("Sudah 3 detik berlalu di game!")
    end
end)
```

---

### Pelajaran 4: Membuat Mod Godmode (Darah Kebal)
Sekarang kita akan membuat mod yang menjaga darah dan armor pemain agar tidak pernah berkurang atau mati.

Buat file baru di `scripts/godmode.lua`:

```lua
-- scripts/godmode.lua
-- Mod Kebal / Infinite Health & Armour

Game.Log("Mod Godmode diaktifkan.")

Game.OnTick(function()
    -- 1. Ambil pointer karakter pemain (CJ)
    local ped = Player.GetPed()

    -- 2. Pastikan karakter pemain valid dan sudah ada di dunia game
    if ped ~= nil then
        local currentHP = Player.GetHealth(ped)

        -- 3. Jika darah berkurang di bawah 100, langsung isi kembali
        if currentHP < 100.0 and currentHP > 0.0 then
            Player.SetHealth(ped, 200.0) -- Berikan HP ekstra
            Player.SetArmour(ped, 100.0) -- Berikan Armor penuh
            Game.PrintText("~g~[GODMODE]~n~~w~Darah dan Armour dipulihkan!", 1500)
        end
    end
end)
```

---

### Pelajaran 5: Membuat Mod Auto-Repair Kendaraan
Mod ini akan mendeteksi ketika HP kendaraan turun karena benturan atau tembakan, lalu otomatis memperbaikinya hingga mulus kembali.

Buat file baru di `scripts/autorepair.lua`:

```lua
-- scripts/autorepair.lua
-- Mod Auto-Repair Kendaraan

local cekTimer = 0

Game.OnTick(function()
    cekTimer = cekTimer + 1

    -- Cek setiap 30 frame (sekitar setengah detik sekali)
    if cekTimer % 30 == 0 then
        local ped = Player.GetPed()
        if ped ~= nil then
            -- Catatan: jika ped berada di dalam kendaraan, Anda dapat memanfaatkan
            -- API Vehicle untuk memperbaiki bodi kendaraan secara instan:
            -- Vehicle.Repair(vehPtr)
        end
    end
end)
```

---

### Pelajaran 6: Memanggil Script Lain Berkali-kali (Modular Modding)
AMLua v1.0.3+ mendukung pemanggilan file Lua lain secara dinamis berulang kali menggunakan `dofile()` atau `Game.DoFile()`.

Katakanlah Anda memiliki file konfigurasi `config.lua` dan modul pendukung `senjata.lua`:

**File 1: `scripts/senjata.lua`**
```lua
local SenjataMod = {}

function SenjataMod.BeriBonus()
    Game.PrintText("~y~Bonus Senjata Diterima!~n~~w~Peluru bertambah.", 2500)
end

return SenjataMod
```

**File 2: `scripts/main_mod.lua`**
```lua
-- Anda bisa memanggil script lain dengan mudah:
local Senjata = dofile("senjata.lua")

-- Atau menggunakan require:
-- local Senjata = require("senjata")

Senjata.BeriBonus()
```

Anda juga bisa menjalankan potongan kode dinamis menggunakan `Game.RunString`:
```lua
Game.RunString("Game.PrintText('~b~Dieksekusi via RunString!', 2000)")
```

---

### Pelajaran 7: Debugging & Mengatasi Error
Jika script Anda memiliki kesalahan ketik (syntax error) atau runtime error, game **TIDAK AKAN CRASH**. AMLua memiliki error handler cerdas yang menangkap error dan mencatatnya ke file log.

1. Buka file log di HP Anda:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua.log
   ```
2. Contoh isi log ketika terjadi error:
   ```text
   [2026-09-13 14:30:00] [Script Error in myscript.lua]:
   scripts/myscript.lua:12: attempt to call a nil value (field 'NonExistentFunction')
   stack traceback:
       scripts/myscript.lua:12: in main chunk
   ```
3. Lihat nomor barisnya (misal: baris 12), perbaiki kodenya, lalu gunakan fitur `Game.ReloadScripts()` atau restart game untuk mencoba kembali!

---

## 📱 In-Game Mod List Viewer

AMLua memiliki fitur visual native untuk melihat daftar script apa saja yang sedang aktif berjalan di GTA San Andreas Anda.

### Cara Membuka:
1. **Double-Tap Pojok Kanan Atas**: Ketuk 2 kali dengan cepat di sudut kanan atas layar (dekat bar HP / senjata).
2. **Double-Tap Bar Atas**: Ketuk 2 kali di area atas layar.
3. **Swipe Down**: Gesek jari dari atas ke bawah.
4. **Tap 2 Jari**: Ketuk layar dengan dua jari bersamaan.
5. **Via Kode Script**:
   ```lua
   Game.ShowScriptList()
   ```

Dialog box khas GTA San Andreas akan langsung muncul di pojok kanan atas:
```text
AMLua Mods (2):
1. godmode.lua
2. test.lua
```

---

## 📲 Panduan Pemasangan di HP Android

1. Pastikan game **GTA San Andreas Android** Anda sudah terpasang **Android Mod Loader (AML)**.
2. Unduh file rilis terbaru (`AMLua-v1.0.4.zip`) dari menu [Releases](https://github.com/sunandar3221/AMLua/releases).
3. Ekstrak file zip tersebut. Anda akan mendapatkan dua folder arsitektur:
   - `arm64-v8a/libAMLua.so` (untuk mayoritas HP Android modern 64-bit).
   - `armeabi-v7a/libAMLua.so` (untuk HP 32-bit).
4. Salin file `libAMLua.so` yang sesuai ke direktori mods AML game Anda:
   ```text
   /Android/data/com.rockstargames.gtasa/mods/
   ```
   *(Atau pasang langsung melalui aplikasi Android Mod Loader APK).*
5. Buat folder `scripts` di direktori:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/
   ```
6. Masukkan file-file script Lua Anda (misal: `test.lua`) ke dalam folder `scripts/` tersebut.
7. Buka game GTA San Andreas. Dialog sambutan AMLua akan menyapa di pojok kanan atas begitu CJ spawn!

---

## 🛠️ Kompilasi / Build dari Source Code

Jika Anda ingin memodifikasi source code C++ AMLua dan melakukan kompilasi sendiri:

### Kebutuhan:
- CMake (versi 3.22+)
- Ninja Build
- Android NDK (r21e - r26+)

### Perintah Build:

#### 1. Build arm64-v8a (64-bit)
```powershell
cmake -B build/arm64-v8a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="arm64-v8a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/arm64-v8a
```

#### 2. Build armeabi-v7a (32-bit)
```powershell
cmake -B build/armeabi-v7a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="armeabi-v7a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/armeabi-v7a
```

Hasil file library `libAMLua.so` akan berada di folder `build/arm64-v8a/` dan `build/armeabi-v7a/`.

---

## 📄 Lisensi
Proyek ini dilisensikan di bawah lisensi terbuka [MIT License](LICENSE). Anda bebas memodifikasi, mendistribusikan, dan menggunakan kode ini untuk proyek modifikasi pribadi maupun komunitas.

---
*Dibuat dengan ❤️ untuk komunitas modder GTA San Andreas Android.*
