# AMLua - Android Mod Loader (AML) Lua Script Loader
> **Lua Scripting Runtime & In-Game Mod Loader untuk Grand Theft Auto: San Andreas Android**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://android.com)
[![Architecture](https://img.shields.io/badge/Arch-arm64--v8a%20%7C%20armeabi--v7a-orange.svg)](#)
[![Lua Version](https://img.shields.io/badge/Lua-5.4.6-blue.svg)](https://www.lua.org)
[![Version](https://img.shields.io/badge/Version-1.0.5-brightgreen.svg)](#)

**AMLua (Android Mod Lua)** adalah plugin shared library resmi berbasis **Android Mod Loader (AML)** yang dirancang untuk memuat, mengelola, dan mengeksekusi script **Lua** secara dinamis langsung di dalam game **Grand Theft Auto: San Andreas** (Android).

Binary AMLua hadir dengan penamaan arsitektur yang terpisah dan jelas:
- **`libAMLua64.so`** — Khusus arsitektur **ARM64 (arm64-v8a)** untuk perangkat Android 64-bit modern.
- **`libAMLua32.so`** — Khusus arsitektur **ARM32 (armeabi-v7a)** untuk perangkat Android 32-bit klasik.

Dengan AMLua, para modder tidak perlu lagi melakukan re-kompilasi kode C/C++ yang rumit atau menggunakan toolchain NDK setiap kali ingin membuat atau menguji mod. Cukup tulis script `.lua`, taruh di folder `scripts/`, dan script akan langsung aktif di dalam game! Tersedia file `.so` langsung di halaman Release tanpa harus repot mengekstrak file ZIP.

---

## 📑 Daftar Isi
1. [Apa itu AMLua?](#-apa-itu-amlua)
   - [Latar Belakang & Arsitektur](#latar-belakang--arsitektur)
   - [Cara Kerja AMLua](#cara-kerja-amlua)
   - [Fitur Utama & Keunggulan](#fitur-utama--keunggulan)
2. [Fitur Crash Handler & Error Logging](#-fitur-crash-handler--error-logging)
   - [Log Crash Sistem (amlua_crash.log)](#1-log-crash-sistem-amlua_crashlog)
   - [Log Error Runtime Lua (amlua_error.log)](#2-log-error-runtime-lua-amlua_errorlog)
   - [Log Eksekusi Normal (amlua.log)](#3-log-eksekusi-normal-amlualog)
3. [Dokumentasi Lengkap API AMLua](#-dokumentasi-lengkap-api-amlua)
   - [Modul Player](#1-modul-player)
   - [Modul Vehicle](#2-modul-vehicle)
   - [Modul Game & OnTick](#3-modul-game)
   - [Modul Timer (Game.Every / Game.After / SetInterval)](#4-modul-timer-alternatif-mudah-berbasis-detik)
   - [Modul AMLua & Global](#5-modul-amlua)
   - [Daftar Kode Warna & Format Dialog GTA SA](#6-daftar-kode-warna--format-dialog-gta-sa)
4. [Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)](#-belajar-kilat-amlua-sampai-bisa-tutorial-lengkap-dari-nol)
   - [Pelajaran 1: Struktur Folder & File Script](#pelajaran-1-struktur-folder--file-script)
   - [Pelajaran 2: Hello World (Script Pertama Anda)](#pelajaran-2-hello-world-script-pertama-anda)
   - [Pelajaran 3: Memahami Timer: Game.Every (Detik) vs Game.OnTick (Frame)](#pelajaran-3-memahami-timer-gameevery-detik-vs-gameontick-frame)
   - [Pelajaran 4: Membuat Mod Godmode (Darah Kebal)](#pelajaran-4-membuat-mod-godmode-darah-kebal)
   - [Pelajaran 5: Membuat Mod Auto-Repair Kendaraan](#pelajaran-5-membuat-mod-auto-repair-kendaraan)
   - [Pelajaran 6: Memanggil Script Lain Berkali-kali (Modular Modding)](#pelajaran-6-memanggil-script-lain-berkali-kali-modular-modding)
   - [Pelajaran 7: Debugging & Mengatasi Error](#pelajaran-7-debugging--mengatasi-error)
5. [In-Game Mod List Viewer (Melihat Daftar Mod Aktif)](#-in-game-mod-list-viewer)
6. [Panduan Pemasangan di HP Android](#-panduan-pemasangan-di-hp-android)
7. [Kompilasi / Build dari Source Code](#-kompilasi--build-dari-source-code)
8. [Lisensi](#-lisensi)

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
│         AMLua (libAMLua64.so / libAMLua32.so)          │
│  - AML Integration     - Safe Entity Offsets (32/64)   │
│  - Crash Signal Handler- CHud::SetHelpMessage Bridge   │
│  - Native Lua 5.4.6 VM - Multi-Script Manager          │
└───────────────────────────▲────────────────────────────┘
                            │ Scans & Loads
┌───────────────────────────┴────────────────────────────┐
│      Folder /sdcard/Android/data/.../files/scripts/    │
│  - mod_darah.lua       - mod_mobil.lua   - menu.lua    │
└────────────────────────────────────────────────────────┘
```

### Cara Kerja AMLua
1. **Inisialisasi**: Saat GTA San Andreas dimuat, **Android Mod Loader (AML)** memanggil fungsi `OnModLoad()` pada library binary (`libAMLua64.so` atau `libAMLua32.so`).
2. **Signal Crash Handler**: AMLua segera mengaktifkan penangkap sinyal fatal Linux (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`, `SIGILL`) dengan alternate stack agar setiap crash tercatat rinci.
3. **Symbol Hooking**: AMLua mencari alamat memori internal `libGTASA.so` untuk menemukan fungsi penting seperti loop game (`CGame::Process`), pencari pemain (`FindPlayerPed`), dan sistem dialog native (`CHud::SetHelpMessage`).
4. **VM Initialization**: AMLua menyalakan runtime engine Lua 5.4.6, mendaftarkan seluruh API modul (`Player`, `Vehicle`, `Game`, `AMLua`), dan mengaktifkan proteksi Sandbox.
5. **Auto-Discovery**: AMLua otomatis membaca isi folder `scripts/`, lalu mengeksekusi semua file `.lua` secara berurutan.
6. **Tick Event Dispatching**: Setiap frame game berjalan, AMLua memanggil seluruh fungsi `Game.OnTick` yang didaftarkan oleh script-script Anda.
7. **In-Game Dialog**: AMLua menyapa pemain dengan dialog sambutan resmi di pojok kanan atas layar begitu karakter siap dimainkan.

### Fitur Utama & Keunggulan
- **Dedicated Distinct Naming**: Nama file 64-bit (`libAMLua64.so`) dan 32-bit (`libAMLua32.so`) berbeda, mencegah salah pasang atau tertimpa tanpa sengaja.
- **Direct `.so` Download**: File `.so` dapat diunduh langsung satu per satu di GitHub Release tanpa perlu mengekstrak file ZIP di smartphone.
- **Full Crash & Error Logging**: Setiap ada error atau game crash, penyebabnya langsung disimpan di file log diagnostik (`amlua_crash.log` dan `amlua_error.log`).
- **Native Top-Right Dialog**: Menggunakan dialog box bawaan GTA San Andreas (`CHud::SetHelpMessage`) yang estetik di pojok kanan atas dengan teks khas GTA dan dukungan kode warna penuh (`~g~`, `~y~`, `~w~`, `~n~`).
- **Multi-Calling Support**: Bebas memanggil `Game.PrintText`, `dofile()`, atau `require()` berkali-kali tanpa dibatasi oleh engine.
- **Gesture Mod List Viewer**: Buka daftar mod aktif di layar cukup dengan double-tap di bagian atas layar atau tap dengan 2 jari.
- **Zero-Crash Memory Safety**: Dilengkapi validasi memori kernel Linux (`IsValidMemory`) dan verifikasi pointer entitas game (`IsValidGameObject`) agar aman dari crash New Game, cutscene, dan transisi interior.

---

## 🛡️ Fitur Crash Handler & Error Logging

AMLua v1.0.4 dilengkapi sistem diagnostik tingkat lanjut untuk mempermudah menemukan penyebab masalah atau bug pada script maupun game.

Terdapat 3 file log yang otomatis dibuat di folder data GTA San Andreas:
```text
/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/
├── amlua_crash.log   <-- Catatan saat game crash fatal (Segmentation fault, dsb)
├── amlua_error.log   <-- Catatan error sintaks / runtime Lua
└── amlua.log         <-- Catatan aktivitas normal Game.Log
```

### 1. Log Crash Sistem (`amlua_crash.log`)
Jika terjadi crash sistem (seperti salah membaca pointer memori di luar AMLua atau game crash bawaan), AMLua menangkap sinyal POSIX (`SIGSEGV`, `SIGABRT`, `SIGBUS`) menggunakan alternate signal stack (`sigaltstack`). Handler ini tetap dapat menulis log meskipun heap memory rusak!

Informasi yang dicatat di `amlua_crash.log`:
- **Waktu & Tanggal Kejadian**
- **Jenis Sinyal**: Contoh: `SIGSEGV (Segmentation Fault - Invalid Memory Access)`
- **Script & Aksi Aktif**: Menunjukkan script Lua dan fungsi yang sedang dieksekusi saat crash terjadi.
- **Crash PC Address & Module**: Alamat memori penyebab crash yang dipetakan ke library terkait (misal: `libGTASA.so + 0x3F4120` atau `libAMLua64.so + 0x12A0`).
- **Caller Return Address (LR)**: Alamat fungsi pemanggil.
- **Register CPU Lengkap**: Nilai seluruh register prosesor (`X0-X30`, `SP`, `PC` pada 64-bit; `R0-R10`, `FP`, `IP`, `SP`, `LR`, `PC` pada 32-bit).

Contoh isi `amlua_crash.log`:
```text
================================================================================
                           AMLUA CRASH LOG REPORT                               
================================================================================
Date/Time       : 2026-09-13 14:40:12
Signal Caught   : SIGSEGV (Segmentation Fault - Invalid Memory Access) (11)
Signal Code     : 1 SEGV_MAPERR (Address not mapped to object)
Fault Address   : 0x0
Active Script   : mod_test.lua
Last Action     : Script Execution (Startup)
--------------------------------------------------------------------------------
Crash PC Address: 0x73841a3024 (/data/app/.../lib/arm64/libGTASA.so + 0x3d2024)
Crash Function  : _ZN5CPed10GetVehicleEv
Caller LR Return: 0x7385201150 (/data/app/.../lib/arm64/libAMLua64.so + 0x22150)
Stack Pointer SP: 0x7fe32b9180
--------------------------------------------------------------------------------
CPU REGISTERS DUMP:
  X00: 0x0000000000000000   X01: 0x00000073841a2f00
  ...
================================================================================
```

### 2. Log Error Runtime Lua (`amlua_error.log`)
Jika ada kesalahan penulisan kode Lua (misal: memanggil fungsi yang tidak ada, salah tipe parameter, atau file tidak ditemukan), error akan dicatat ke `amlua_error.log` beserta **stack traceback** lengkap tanpa menyebabkan game crash:

```text
[2026-09-13 14:42:01] [Script Error in player_mod.lua]:
scripts/player_mod.lua:15: attempt to index a nil value (global 'Plyer')
stack traceback:
    scripts/player_mod.lua:15: in function <scripts/player_mod.lua:12>
```

### 3. Log Eksekusi Normal (`amlua.log`)
Mencatat informasi pemuatan script, inisialisasi AMLua, dan pesan-pesan custom yang Anda kirim lewat `Game.Log("...")`.

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
| `Game.Every(seconds, callback)` | `seconds` (number), `callback` (function) | `number` (timerId) | **(Direkomendasikan)** Menjalankan fungsi berulang kali setiap X detik waktu nyata tanpa ribet menghitung FPS. |
| `Game.After(seconds, callback)` | `seconds` (number), `callback` (function) | `number` (timerId) | **(Direkomendasikan)** Menjalankan fungsi satu kali saja setelah X detik waktu nyata (delay). |
| `Game.SetInterval(callback, seconds)` | `callback`, `seconds` *(urutan bebas)* | `number` (timerId) | Identik dengan `Game.Every`. Format standar mirip JavaScript / game engine. |
| `Game.SetTimeout(callback, seconds)` | `callback`, `seconds` *(urutan bebas)* | `number` (timerId) | Identik dengan `Game.After`. Menjalankan fungsi 1x setelah delay X detik. |
| `Game.ClearTimer(timerId)` | `timerId` (number) | `boolean` | Menghentikan dan menghapus timer / interval yang sedang berjalan. Alias: `Game.ClearInterval`, `Game.ClearTimeout`. |
| `Game.OnTick(callback)` | `callback(dt)` (function) | *(tidak ada)* | Mendaftarkan fungsi per frame loop game. Menerima parameter `dt` (delta time dalam detik sejak frame terakhir). |
| `Game.PrintText(text, timeMs)` | `text` (string), `[timeMs]` (default 3000) | *(tidak ada)* | Menampilkan teks pada dialog native GTA SA di **pojok kanan atas layar**. Mendukung kode warna GTA. Dapat dipanggil berkali-kali. |
| `Game.ShowHelpMessage(text, timeMs)` | `text`, `[timeMs]` | *(tidak ada)* | Alias identik untuk `Game.PrintText`. |
| `Game.Log(message)` | `message` (string) | *(tidak ada)* | Menulis teks ke file log `amlua.log` dan Android Logcat dengan tag `AMLua`. |
| `Game.ShowScriptList()` | *(tidak ada)* | *(tidak ada)* | Membuka dialog visual native di pojok kanan atas yang menampilkan daftar semua mod Lua yang aktif. |
| `Game.GetLoadedScripts()` | *(tidak ada)* | `table` (array of string) | Mengembalikan daftar nama file script `.lua` yang berhasil dimuat oleh AMLua. |
| `Game.DoFile(filename)` | `filename` (string) | *(hasil kembalian script)* | Menjalankan file script `.lua` lain. Path relatif secara otomatis merujuk ke folder `scripts/`. Dapat dipanggil berkali-kali! |
| `Game.RunString(code)` | `code` (string) | *(hasil kembalian code)* | Mengevaluasi dan menjalankan baris kode Lua dari bentuk string secara dinamis. |
| `Game.ReloadScripts()` | *(tidak ada)* | *(tidak ada)* | Memuat ulang seluruh file script dan mereset timer tanpa perlu me-restart game. |

---

### 4. Modul `Timer` (Alternatif Mudah Berbasis Detik)
Untuk pemula yang tidak ingin pusing menghitung FPS atau menghadapi masalah OnTick yang telat karena frame drop, gunakan modul `Timer` atau fungsi praktis berbasis detik langsung:

```lua
-- Jalankan setiap 1.5 detik
local myTimer = Timer.Every(1.5, function()
    Game.PrintText("~y~Pesan Muncul Setiap 1.5 Detik!", 1000)
end)

-- Batalkan timer jika sudah selesai
-- Timer.Clear(myTimer)

-- Jalankan sekali setelah 5 detik
Timer.After(5.0, function()
    Game.PrintText("~g~5 Detik Berlalu!", 2000)
end)
```

> 💡 **Fleksibilitas Parameter**:
> Anda bebas menukar urutan parameter: `Game.Every(2.0, fn)` ataupun `Game.Every(fn, 2.0)` keduanya valid dan langsung bekerja tanpa error!
> Tersedia pula fungsi global praktis: `setInterval`, `setTimeout`, `clearInterval`, `clearTimeout`.

---

### 5. Modul `AMLua`
Tabel namespace alternatif yang mewadahi seluruh modul:
- `AMLua.Player` -> identik dengan `Player`
- `AMLua.Vehicle` -> identik dengan `Vehicle`
- `AMLua.Game` -> identik dengan `Game`
- `AMLua.Timer` -> modul timer berbasis detik
- `AMLua.Every(sec, fn)` / `AMLua.After(sec, fn)` -> timer detik praktis
- `AMLua.OnTick(fn)` -> identik dengan `Game.OnTick`
- `AMLua.DoFile(path)` -> identik dengan `Game.DoFile`
- `AMLua.RunString(code)` -> identik dengan `Game.RunString`
- `AMLua.ReloadScripts()` -> memuat ulang script & timer
- `AMLua.ShowScriptList()` -> membuka dialog daftar mod
- `AMLua.GetLoadedScripts()` -> mengambil array nama mod
- `AMLua.Version` -> string versi saat ini (`"1.0.5"`)
- `dofile(filename)` -> fungsi global bawaan yang otomatis mencari file di folder `scripts/`

---

### 6. Daftar Kode Warna & Format Dialog GTA SA
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

### Pelajaran 3: Memahami Timer: Game.Every (Detik) vs Game.OnTick (Frame)

#### Kenapa `Game.OnTick` dengan Hitungan Frame Suka Telat?
Di HP Android, FPS game sering kali naik-turun (misalnya dibatasi 30 FPS, atau turun ke 15–20 FPS saat jalanan ramai).
- Jika Anda membuat kode `if frameCount % 30 == 0`:
  - Pada 60 FPS: dipanggil setiap **0.5 detik**.
  - Pada 30 FPS: dipanggil setiap **1.0 detik**.
  - Pada 15 FPS: dipanggil setiap **2.0 detik (terasa telat/ngelag!)**.
Bagi pemula, menghitung frame (`frameCount % 30`) sangat bikin pusing dan tidak konsisten.

#### Solusi Terbaik & Paling Mudah: `Game.Every(detik, callback)`
Gunakan **`Game.Every`**! Timer ini berjalan berdasarkan **waktu nyata (real seconds)** prosesor, bukan hitungan frame. 1 detik akan selalu tepat 1 detik di HP apa pun:

```lua
-- Sangat mudah dan ga bikin pusing! Berjalan setiap 1 detik:
Game.Every(1.0, function()
    Game.Log("Tepat 1 detik berlalu secara konsisten!")
end)

-- Ingin delay sekali jalan setelah 3 detik? Pakai Game.After:
Game.After(3.0, function()
    Game.PrintText("~g~3 detik telah berlalu!", 2000)
end)
```

#### Kapan Harus Menggunakan `Game.OnTick`?
Gunakan `Game.OnTick` jika Anda ingin kode berjalan **setiap kali frame dirender** (misalnya untuk animasi mulus atau manipulasi pergerakan terus-menerus). AMLua v1.0.5 menyertakan parameter **`dt` (delta time)**:

```lua
Game.OnTick(function(dt)
    -- dt bernilai sekitar 0.033 detik (pada 30 FPS) atau 0.016 detik (pada 60 FPS)
    -- Anda bisa menambah timer secara akurat tanpa menghitung frame:
    -- myTimer = myTimer + dt
end)
```

---

### Pelajaran 4: Membuat Mod Godmode (Darah Kebal)
Sekarang kita buat mod kebal menggunakan `Game.Every(0.5, ...)` yang memeriksa darah setiap setengah detik secara konsisten tanpa lag:

Buat file baru di `scripts/godmode.lua`:

```lua
-- scripts/godmode.lua
-- Mod Kebal / Infinite Health & Armour (Menggunakan Timer Detik)

Game.Log("Mod Godmode diaktifkan.")

-- Periksa dan isi darah setiap 0.5 detik
Game.Every(0.5, function()
    local ped = Player.GetPed()

    -- Pastikan karakter pemain valid dan sudah spawn di dunia game
    if ped ~= nil then
        local currentHP = Player.GetHealth(ped)

        -- Jika darah berkurang di bawah 100, langsung isi kembali
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
Mod ini memeriksa kendaraan setiap 1 detik dan otomatis memperbaikinya saat bodi rusak:

Buat file baru di `scripts/autorepair.lua`:

```lua
-- scripts/autorepair.lua
-- Mod Auto-Repair Kendaraan

Game.Log("Mod Auto-Repair diaktifkan.")

Game.Every(1.0, function()
    local ped = Player.GetPed()
    if ped ~= nil then
        -- Jika CJ berada di kendaraan dan bodi penyok/rusak,
        -- panggil Vehicle.Repair(vehPtr) untuk memuluskan kembali bodi mobil!
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
Jika script Anda memiliki kesalahan sintaks atau runtime error, game **TIDAK AKAN CRASH**. AMLua menangkap error tersebut dan menyimpannya di file log khusus:

1. **Error Script Lua**:
   Buka file:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua_error.log
   ```
   Contoh isi log error:
   ```text
   [2026-09-13 14:30:00] [Script Error in myscript.lua]:
   scripts/myscript.lua:12: attempt to call a nil value (field 'NonExistentFunction')
   stack traceback:
       scripts/myscript.lua:12: in main chunk
   ```
   Lihat nomor barisnya (misal: baris 12), perbaiki kodenya di text editor, lalu gunakan fitur `Game.ReloadScripts()` atau restart game untuk mencoba kembali.

2. **Game Crash Fatal (Crash Handler)**:
   Jika game tertutup mendadak / force close karena mod lain atau bug fatal, buka:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua_crash.log
   ```
   Crash log akan memberikan informasi detail:
   - Modul library penyebab crash (misal: `libGTASA.so + 0x...` atau `libAMLua64.so`)
   - Script yang sedang berjalan saat crash (`Active Script`)
   - Nilai register prosesor saat kejadian.

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

Anda tidak perlu lagi ribet mengekstrak file ZIP di HP jika hanya butuh file `.so`-nya!

1. Pastikan game **GTA San Andreas Android** Anda sudah terpasang **Android Mod Loader (AML)**.
2. Buka menu [Releases](https://github.com/sunandar3221/AMLua/releases) di GitHub AMLua:
   - **Opsi Cepat (Tanpa ZIP)**:
     - Unduh **`libAMLua64.so`** jika HP Anda 64-bit (kebanyakan HP Android rilisan 2018 ke atas).
     - Unduh **`libAMLua32.so`** jika HP Anda 32-bit.
   - **Opsi Lengkap (ZIP)**:
     - Unduh **`AMLua-v1.0.5.zip`** (berisi kedua file binary `.so`, contoh script `test.lua`, dan dokumentasi lengkap).
3. Salin file `.so` yang sesuai ke direktori mods AML game Anda:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/mods/
   ```
   *(Atau pasang file `.so` langsung melalui menu Mod Manager di aplikasi Android Mod Loader APK).*
4. Buat folder `scripts` di direktori:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/
   ```
5. Masukkan file-file script Lua Anda (misal: `test.lua`) ke dalam folder `scripts/` tersebut.
6. Buka game GTA San Andreas. Dialog sambutan AMLua akan menyapa di pojok kanan atas begitu permainan siap dimulai!

---

## 🛠️ Kompilasi / Build dari Source Code

Jika Anda ingin memodifikasi source code C++ AMLua dan melakukan kompilasi sendiri:

### Kebutuhan:
- CMake (versi 3.22+)
- Ninja Build
- Android NDK (r21e - r26+)

### Perintah Build:

#### 1. Build arm64-v8a (Output: `libAMLua64.so`)
```powershell
cmake -B build/arm64-v8a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="arm64-v8a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/arm64-v8a
```

#### 2. Build armeabi-v7a (Output: `libAMLua32.so`)
```powershell
cmake -B build/armeabi-v7a -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/26.3.11579264/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI="armeabi-v7a" `
  -DANDROID_PLATFORM=android-21 `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/armeabi-v7a
```

Hasil file library `libAMLua64.so` dan `libAMLua32.so` akan berada di folder `build/arm64-v8a/` dan `build/armeabi-v7a/`.

---

## 📄 Lisensi
Proyek ini dilisensikan di bawah lisensi terbuka [MIT License](LICENSE). Anda bebas memodifikasi, mendistribusikan, dan menggunakan kode ini untuk proyek modifikasi pribadi maupun komunitas.

---
*Dibuat dengan ❤️ untuk komunitas modder GTA San Andreas Android.*
