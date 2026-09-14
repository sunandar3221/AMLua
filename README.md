# AMLua - Android Mod Loader (AML) Lua Script Loader
> **Lua Scripting Runtime & In-Game Mod Loader untuk Grand Theft Auto: San Andreas Android**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://android.com)
[![Architecture](https://img.shields.io/badge/Arch-arm64--v8a%20%7C%20armeabi--v7a-orange.svg)](#)
[![Lua Version](https://img.shields.io/badge/Lua-5.4.6-blue.svg)](https://www.lua.org)
[![Version](https://img.shields.io/badge/Version-1.0.7-brightgreen.svg)](#)

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
   - [Modul Explosion (Ledakan)](#1-modul-explosion-ledakan)
   - [Modul Player (Karakter Pemain)](#2-modul-player-karakter-pemain)
   - [Modul Vehicle (Manipulasi Kendaraan)](#3-modul-vehicle-manipulasi-kendaraan)
   - [Modul Game (Manipulasi Dunia & Lingkungan)](#4-modul-game-manipulasi-dunia--lingkungan)
   - [Modul Timer (Alternatif Mudah Berbasis Detik)](#5-modul-timer-alternatif-mudah-berbasis-detik)
   - [Modul AMLua & Global](#6-modul-amlua--global)
   - [Daftar Kode Warna & Format Dialog GTA SA](#7-daftar-kode-warna--format-dialog-gta-sa)
4. [Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)](#-belajar-kilat-amlua-sampai-bisa-tutorial-lengkap-dari-nol)
   - [Pelajaran 1: Struktur Folder & File Script](#pelajaran-1-struktur-folder--file-script)
   - [Pelajaran 2: Hello World (Script Pertama Anda)](#pelajaran-2-hello-world-script-pertama-anda)
   - [Pelajaran 3: Memahami Timer Detik (Game.Every & Game.After)](#pelajaran-3-memahami-timer-detik-gameevery--gameafter)
   - [Pelajaran 4: Mod Ledakan (Membuat Ledakan Terarah)](#pelajaran-4-mod-ledakan-membuat-ledakan-terarah)
   - [Pelajaran 5: Mod Speedometer & Nitro Boost Kendaraan](#pelajaran-5-mod-speedometer--nitro-boost-kendaraan)
   - [Pelajaran 6: Mod Teleportasi Aman dengan Ground Snapping](#pelajaran-6-mod-teleportasi-aman-dengan-ground-snapping)
   - [Pelajaran 7: Mod Cuaca, Jam Game & Bonus Uang](#pelajaran-7-mod-cuaca-jam-game--bonus-uang)
   - [Pelajaran 8: Memanggil Script Lain Berkali-kali (Modular Modding)](#pelajaran-8-memanggil-script-lain-berkali-kali-modular-modding)
   - [Pelajaran 9: Debugging & Membaca Error Log](#pelajaran-9-debugging--membaca-error-log)
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
│  - mod_ledakan.lua     - mod_mobil.lua   - menu.lua    │
└────────────────────────────────────────────────────────┘
```

### Cara Kerja AMLua
1. **Inisialisasi**: Saat GTA San Andreas dimuat, **Android Mod Loader (AML)** memanggil fungsi `OnModLoad()` pada library binary (`libAMLua64.so` atau `libAMLua32.so`).
2. **Signal Crash Handler**: AMLua segera mengaktifkan penangkap sinyal fatal Linux (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`, `SIGILL`) dengan alternate stack agar setiap crash tercatat rinci.
3. **Symbol Hooking**: AMLua mencari alamat memori internal `libGTASA.so` untuk menemukan fungsi penting seperti loop game (`CGame::Process`), pencari pemain (`FindPlayerPed`), kendaraan (`FindPlayerVehicle`), ledakan (`CExplosion::AddExplosion`, `CWorld::TriggerExplosion`), cuaca (`CWeather`), waktu (`CClock`), dan dialog native (`CHud::SetHelpMessage`).
4. **VM Initialization**: AMLua menyalakan runtime engine Lua 5.4.6, mendaftarkan seluruh API modul (`Explosion`, `Player`, `Vehicle`, `Game`, `Timer`, `AMLua`), dan mengaktifkan proteksi Sandbox.
5. **Auto-Discovery**: AMLua otomatis membaca isi folder `scripts/`, lalu mengeksekusi semua file `.lua` secara berurutan.
6. **Tick & Real-Time Timers**: AMLua mengeksekusi timer berbasis detik (`Game.Every` / `Game.After`) secara independen dari FPS dan mendispatch callback `Game.OnTick`.
7. **In-Game Dialog**: AMLua menyapa pemain dengan dialog sambutan resmi di pojok kanan atas layar begitu karakter siap dimainkan.

### Fitur Utama & Keunggulan
- **Dedicated Distinct Naming**: Nama file 64-bit (`libAMLua64.so`) dan 32-bit (`libAMLua32.so`) berbeda, mencegah salah pasang atau tertimpa tanpa sengaja.
- **Direct `.so` Download**: File `.so` dapat diunduh langsung satu per satu di GitHub Release tanpa perlu mengekstrak file ZIP di smartphone.
- **Full Crash & Error Logging**: Setiap ada error atau game crash, penyebabnya langsung disimpan di file log diagnostik (`amlua_crash.log` dan `amlua_error.log`).
- **Rich Modding API (v1.0.7)**: Mendukung modul `Explosion`, manipulasi kendaraan lengkap (`Vehicle`), teleportasi aman (`Player.Teleport`, `Game.Teleport`), deteksi ketinggian tanah (`Game.GetGroundZ`), waktu, cuaca, wanted level, uang, dan speed game.
- **Bulletproof Anti-Crash Protection (v1.0.7)**: Dilengkapi proteksi lapis baja terhadap bug engine GTA SA mobile:
  - Mengatasi fatal crash `FindPlayerVehicle(-1)` dengan helper aman `GetLocalPlayerVehicle()`.
  - Mengatasi crash `Explosion.Create` dengan pemanggilan presisi symbol native `CWorld::TriggerExplosion`.
  - Mengatasi crash `Teleport` dengan bypass ABI crash melalui direct placement & matrix update dan velocity reset.
  - Mengatasi crash `CWanted` dan manipulasi uang dengan validasi aktif pemain sebelum mengakses memori `libGTASA.so`.
  - Validasi memori kernel via syscall `mincore(2)` untuk mendeteksi unmapped page tanpa segfault.
- **Native Top-Right Dialog**: Menggunakan dialog box bawaan GTA San Andreas (`CHud::SetHelpMessage`) yang estetik di pojok kanan atas dengan teks khas GTA dan dukungan kode warna penuh (`~g~`, `~y~`, `~w~`, `~n~`).
- **Gesture Mod List Viewer**: Buka daftar mod aktif di layar cukup dengan double-tap di bagian atas layar atau tap dengan 2 jari.
- **Zero-Crash Memory Safety**: Dilengkapi validasi memori kernel Linux (`IsValidMemory`) dan verifikasi pointer entitas game (`IsValidGameObject`) agar aman dari crash New Game, cutscene, dan transisi interior.

---

## 🛡️ Fitur Crash Handler & Error Logging

AMLua dilengkapi sistem diagnostik tingkat lanjut untuk mempermudah menemukan penyebab masalah atau bug pada script maupun game.

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

### 2. Log Error Runtime Lua (`amlua_error.log`)
Jika ada kesalahan penulisan kode Lua (misal: memanggil fungsi yang tidak ada, salah tipe parameter, atau file tidak ditemukan), error akan dicatat ke `amlua_error.log` beserta **stack traceback** lengkap tanpa menyebabkan game crash:
```text
[2026-09-14 21:05:01] [Script Error in mod_test.lua]:
scripts/mod_test.lua:15: attempt to call a nil value (global 'NonExistentFunction')
stack traceback:
    scripts/mod_test.lua:15: in function <scripts/mod_test.lua:12>
```

### 3. Log Eksekusi Normal (`amlua.log`)
Mencatat informasi pemuatan script, inisialisasi AMLua, dan pesan-pesan custom yang Anda kirim lewat `Game.Log("...")`.

---

## 📜 Dokumentasi Lengkap API AMLua

Semua fungsi di bawah ini dapat diakses secara global di dalam script Lua Anda.

---

### 1. Modul `Explosion` (Ledakan)
Modul untuk memicu ledakan di koordinat tertentu atau langsung di sekitar pemain / kendaraan.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Explosion.Create(x, y, z, [type], [radius], [sound], [shake])` | `x, y, z` (number), `[type]` (int, default 3), `[radius]` (float, default 10.0), `[sound]` (bool, default true), `[shake]` (float, default 1.0) | `boolean` | Membuat ledakan di koordinat dunia `(x, y, z)`. Mengembalikan `true` jika berhasil. |
| `Explosion.CreateAtPlayer([type], [radius], [sound], [shake])` | `[type]`, `[radius]`, `[sound]`, `[shake]` | `boolean` | Membuat ledakan tepat di posisi karakter pemain saat ini. |
| `Explosion.CreateAtVehicle([veh], [type], [radius], [sound], [shake])` | `[veh]` (pointer, opsional), `[type]`, `[radius]`, `[sound]`, `[shake]` | `boolean` | Membuat ledakan di posisi kendaraan tertentu (atau kendaraan yang sedang dinaiki pemain). |

> **Daftar Tipe Ledakan (`type`):**
> - `0` : Grenade (Granat)
> - `1` : Molotov (Api molotov menyebar)
> - `2` : Rocket (Roket RPG standar)
> - `3` : Rocket Weak / Car Explosion (Ledakan mobil standar)
> - `4` : Car (Ledakan mobil besar)
> - `5` : Car Quick (Ledakan cepat instan)
> - `6` : Boat (Ledakan kapal/perahu)
> - `7` : Heli (Ledakan helikopter)
> - `8` : Mine (Ranjau darat)
> - `9` : Barrel (Tong bensin)
> - `10`: Tank Grenade (Meriam tank Rhino)
> - `11`: Heli Bomb (Bom jatuh helikopter)

#### Contoh Penggunaan:
```lua
-- Buat ledakan granat di depan pemain
local x, y, z = Player.GetPosition()
Explosion.Create(x + 5.0, y + 5.0, z, 0, 8.0, true, 1.0)

-- Ledakkan mobil saat ini dengan ledakan besar
local veh = Vehicle.GetPlayerVehicle()
if veh ~= nil then
    Explosion.CreateAtVehicle(veh, 4, 15.0, true, 2.0)
end
```

---

### 2. Modul `Player` (Karakter Pemain)
Modul untuk membaca dan memanipulasi keadaan karakter pemain (CJ).

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Player.GetPed()` | *(tidak ada)* | `lightuserdata` (pointer ped) atau `nil` | Mengambil pointer memori karakter pemain saat ini. |
| `Player.GetHealth([ped])` | `[ped]` (pointer, opsional) | `number` (float) | Mengambil jumlah darah/HP pemain saat ini. |
| `Player.SetHealth([ped], hp)` | `[ped]`, `hp` atau cukup `hp` (number) | *(tidak ada)* | Mengatur jumlah darah/HP pemain. |
| `Player.GetArmour([ped])` | `[ped]` (pointer, opsional) | `number` (float) | Mengambil nilai rompi anti-peluru (armour) pemain. |
| `Player.SetArmour([ped], armour)`| `[ped]`, `armour` atau cukup `armour` | *(tidak ada)* | Mengatur nilai rompi anti-peluru pemain. |
| `Player.GetPosition([ped])` | `[ped]` (pointer, opsional) | `x, y, z` (3 numbers) | Mengambil koordinat posisi pemain di dunia game. |
| `Player.SetPosition([ped], x, y, z)` | `[ped], x, y, z` atau `x, y, z` | `boolean` | Memindahkan (teleportasi) pemain ke koordinat baru secara instan dan aman. Jika pemain sedang di kendaraan, kendaraannya ikut dipindahkan. Alias: `Player.Teleport`. |
| `Player.Teleport(x, y, z)` | `x, y, z` (numbers) | `boolean` | Alias praktis untuk `Player.SetPosition`. |
| `Player.GetVehicle([ped])` | `[ped]` (pointer, opsional) | `lightuserdata` atau `nil` | Mengambil pointer kendaraan yang sedang dinaiki pemain. Mengembalikan `nil` jika sedang jalan kaki. |
| `Player.IsInVehicle([ped])` | `[ped]` (pointer, opsional) | `boolean` | Mengembalikan `true` jika pemain sedang berada di dalam kendaraan apa pun. |
| `Player.GetHeading([ped])` | `[ped]` (pointer, opsional) | `number` (derajat 0-360) | Mengambil sudut arah hadap pemain dalam derajat. |
| `Player.SetHeading([ped], deg)` | `[ped], deg` atau `deg` (number) | *(tidak ada)* | Mengatur sudut arah hadap pemain dalam derajat. |
| `Player.GetWantedLevel()` | *(tidak ada)* | `integer` (0 - 6) | Mengambil jumlah bintang wanted level polisi saat ini. |
| `Player.SetWantedLevel(level)` | `level` (integer 0 - 6) | *(tidak ada)* | Mengatur jumlah bintang wanted level polisi. |
| `Player.ClearWantedLevel()` | *(tidak ada)* | *(tidak ada)* | Menghapus semua bintang wanted level polisi (menjadi 0). |
| `Player.GetMoney()` | *(tidak ada)* | `integer` | Mengambil jumlah uang pemain saat ini. |
| `Player.SetMoney(amount)` | `amount` (integer) | *(tidak ada)* | Mengatur jumlah uang pemain ke nilai tertentu. |
| `Player.GiveMoney(amount)` | `amount` (integer) | *(tidak ada)* | Menambahkan uang ke saldo pemain (bisa minus untuk mengurangi). |

---

### 3. Modul `Vehicle` (Manipulasi Kendaraan)
Modul untuk memanipulasi kendaraan (mobil, motor, sepeda, pesawat, helikopter, perahu). Parameter `veh` bersifat fleksibel: jika dikosongkan, otomatis menggunakan kendaraan yang sedang dinaiki pemain!

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Vehicle.GetPlayerVehicle()` | *(tidak ada)* | `lightuserdata` atau `nil` | Mengambil pointer kendaraan pemain saat ini. |
| `Vehicle.GetPosition([veh])` | `[veh]` (pointer, opsional) | `x, y, z` (3 numbers) | Mengambil koordinat posisi kendaraan saat ini. |
| `Vehicle.SetPosition([veh], x, y, z)` | `[veh], x, y, z` atau `x, y, z` | `boolean` | Memindahkan (teleportasi) kendaraan ke posisi baru. Alias: `Vehicle.Teleport`. |
| `Vehicle.Teleport([veh], x, y, z)` | `[veh], x, y, z` atau `x, y, z` | `boolean` | Alias praktis untuk `Vehicle.SetPosition`. |
| `Vehicle.GetVelocity([veh])` | `[veh]` (pointer, opsional) | `vx, vy, vz` (numbers) | Mengambil vektor kecepatan fisik pergerakan kendaraan. |
| `Vehicle.SetVelocity([veh], vx, vy, vz)`| `[veh], vx, vy, vz` atau `vx, vy, vz` | *(tidak ada)* | Mengatur vektor kecepatan fisik kendaraan secara langsung. |
| `Vehicle.GetSpeed([veh])` | `[veh]` (pointer, opsional) | `number` (float km/h) | Mengambil kecepatan laju kendaraan dalam satuan **km/jam**. |
| `Vehicle.SetSpeed([veh], kmh)` | `[veh], kmh` atau `kmh` (number) | *(tidak ada)* | Mengatur kecepatan kendaraan dalam satuan **km/jam** (berguna untuk nitro boost atau pengereman instan). |
| `Vehicle.Repair([veh])` | `[veh]` (pointer, opsional) | *(tidak ada)* | Memperbaiki seluruh kerusakan fisik kendaraan (bodi, kaca, ban, pintu) dan mereset HP ke 1000.0. |
| `Vehicle.GetHealth([veh])` | `[veh]` (pointer, opsional) | `number` (float) | Mengambil nilai health kendaraan (1000.0 = sehat, < 250.0 = terbakar). |
| `Vehicle.SetHealth([veh], hp)` | `[veh], hp` atau `hp` (number) | *(tidak ada)* | Mengatur nilai health kendaraan. |
| `Vehicle.BlowUp([veh])` | `[veh]` (pointer, opsional) | *(tidak ada)* | Meledakkan kendaraan seketika. Alias: `Vehicle.Explode`. |
| `Vehicle.SetDoorLock([veh], lockType)` | `[veh], lockType` atau `lockType` (int) | *(tidak ada)* | Mengatur tipe kunci pintu kendaraan (0: None, 1: Unlocked, 2: Locked, 4: Locked Player Inside). |
| `Vehicle.SetLocked([veh], bool)` | `[veh], bool` atau `bool` (boolean) | *(tidak ada)* | Mengunci (`true`) atau membuka (`false`) pintu mobil. |
| `Vehicle.IsLocked([veh])` | `[veh]` (pointer, opsional) | `boolean` | Mengecek apakah pintu kendaraan sedang terkunci. |
| `Vehicle.GetHeading([veh])` | `[veh]` (pointer, opsional) | `number` (derajat 0-360) | Mengambil sudut arah hadap kendaraan dalam derajat. |
| `Vehicle.SetHeading([veh], deg)` | `[veh], deg` atau `deg` (number) | *(tidak ada)* | Mengatur sudut arah hadap kendaraan dalam derajat. |

---

### 4. Modul `Game` (Manipulasi Dunia & Lingkungan)
Modul untuk mengontrol cuaca, jam dunia, kecepatan game, teleportasi aman, dan UI dialog.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Game.Teleport(x, y, z)` | `x, y, z` (numbers) | `boolean` | Memindahkan pemain (atau kendaraannya jika sedang berkendara) ke `(x, y, z)`. |
| `Game.GetGroundZ(x, y, [z])` | `x, y` (numbers), `[z]` (opsional) | `number` (float) | Mencari ketinggian tanah (**Ground Z**) di koordinat `(x, y)`. Sangat berguna saat teleportasi agar karakter tidak amblas ke bawah map. |
| `Game.SetTime(hour, [minute])` | `hour` (0-23), `[minute]` (0-59) | *(tidak ada)* | Mengatur jam dunia game GTA San Andreas. Alias: `Game.SetClock`. |
| `Game.GetTime()` | *(tidak ada)* | `hour, minute` (2 integers) | Mengambil jam dan menit dunia game saat ini. Alias: `Game.GetClock`. |
| `Game.SetWeather(weatherId)` | `weatherId` (integer 0 - 22) | *(tidak ada)* | Mengubah cuaca dunia game seketika. |
| `Game.ReleaseWeather()` | *(tidak ada)* | *(tidak ada)* | Mengembalikan siklus cuaca ke mode normal otomatis. |
| `Game.SetGameSpeed(speed)` | `speed` (float, normal 1.0) | *(tidak ada)* | Mengatur kecepatan jalannya game (slow-motion atau fast-forward). Nilai `0.5` = slow motion, `2.0` = serba cepat. Alias: `Game.SetTimeScale`. |
| `Game.GetGameSpeed()` | *(tidak ada)* | `number` (float) | Mengambil kecepatan game saat ini. Alias: `Game.GetTimeScale`. |
| `Game.GetFPS()` | *(tidak ada)* | `number` (float) | Mengambil nilai frame rate (FPS) game saat ini. |
| `Game.CreateExplosion(...)` | *(sama dengan Explosion.Create)* | `boolean` | Alias untuk `Explosion.Create`. |
| `Game.PrintText(text, [timeMs])` | `text` (string), `[timeMs]` (ms, default 3000) | *(tidak ada)* | Menampilkan dialog teks native di pojok kanan atas layar dengan dukungan kode warna GTA. |
| `Game.ShowHelpMessage(text, [timeMs])`| `text`, `[timeMs]` | *(tidak ada)* | Alias identik untuk `Game.PrintText`. |
| `Game.Log(message)` | `message` (string) | *(tidak ada)* | Mencatat teks ke `amlua.log` dan logcat Android. |
| `Game.Every(seconds, callback)` | `seconds` (number), `callback` (fn) | `number` (timerId) | Menjalankan fungsi berulang kali setiap X detik waktu nyata. |
| `Game.After(seconds, callback)` | `seconds` (number), `callback` (fn) | `number` (timerId) | Menjalankan fungsi satu kali setelah delay X detik waktu nyata. |
| `Game.SetInterval(callback, sec)` | `callback`, `sec` *(urutan bebas)* | `number` (timerId) | Identik dengan `Game.Every`. |
| `Game.SetTimeout(callback, sec)` | `callback`, `sec` *(urutan bebas)* | `number` (timerId) | Identik dengan `Game.After`. |
| `Game.ClearTimer(timerId)` | `timerId` (number) | `boolean` | Menghapus timer / interval yang sedang aktif. |
| `Game.OnTick(callback)` | `callback(dt)` (function) | *(tidak ada)* | Mendaftarkan callback per frame game dengan parameter delta time `dt`. |
| `Game.DoFile(path)` | `path` (string) | *(hasil script)* | Menjalankan script Lua lain di folder `scripts/`. Dapat dipanggil berkali-kali! |
| `Game.RunString(code)` | `code` (string) | *(hasil code)* | Menjalankan baris kode Lua dari string secara dinamis. |
| `Game.ReloadScripts()` | *(tidak ada)* | *(tidak ada)* | Memuat ulang seluruh mod script dan mereset timer tanpa perlu restart game. |
| `Game.ShowScriptList()` | *(tidak ada)* | *(tidak ada)* | Membuka dialog visual daftar mod aktif di layar. |
| `Game.GetLoadedScripts()` | *(tidak ada)* | `table` | Mengambil array nama-nama script yang sedang aktif. |

> **Daftar ID Cuaca Populer (`weatherId`):**
> - `0` : Extra Sunny (Los Santos)
> - `1` : Sunny (Los Santos)
> - `4` : Cloudy (Los Santos)
> - `7` : Cloudy (San Fierro)
> - `8` : Rainy (Hujan lebat San Fierro)
> - `9` : Foggy (Berkabut tebal San Fierro)
> - `16`: Rainy Countryside (Hujan pedesaan)
> - `19`: Sandstorm Desert (Badai pasir gurun)
> - `20`: Underwater

---

### 5. Modul `Timer` (Alternatif Mudah Berbasis Detik)
Untuk pemula yang tidak ingin pusing menghitung frame rate atau khawatir script telat berjalan akibat frame drop:

```lua
-- Berjalan setiap 2 detik waktu nyata
local timerId = Timer.Every(2.0, function()
    Game.PrintText("~y~Berjalan Setiap 2 Detik!", 1000)
end)

-- Berjalan 1 kali setelah 4 detik
Timer.After(4.0, function()
    Game.PrintText("~g~4 Detik Berlalu!", 2000)
end)

-- Batalkan timer jika sudah tidak dibutuhkan
-- Timer.Clear(timerId)
```

Fungsi global JavaScript / browser juga tersedia:
- `setInterval(fn, detik)` / `clearInterval(id)`
- `setTimeout(fn, detik)` / `clearTimeout(id)`

---

### 6. Modul `AMLua` & Global
Namespace utama yang merangkum seluruh modul:
- `AMLua.Version` -> `"1.0.6"`
- `AMLua.Explosion` -> referensi ke modul `Explosion`
- `AMLua.Player` -> referensi ke modul `Player`
- `AMLua.Vehicle` -> referensi ke modul `Vehicle`
- `AMLua.Game` -> referensi ke modul `Game`
- `AMLua.Timer` -> referensi ke modul `Timer`
- `dofile("nama_file.lua")` -> fungsi global bawaan yang mencari file langsung di folder `scripts/`

---

### 7. Daftar Kode Warna & Format Dialog GTA SA
Teks pada `Game.PrintText()` mendukung kode warna bawaan engine GTA:

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

---

## 🎓 Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)

### Pelajaran 1: Struktur Folder & File Script
1. Buka File Manager di Android (misal: **ZArchiver**).
2. Masuk ke direktori:
   ```text
   /storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts/
   ```
3. Semua file `.lua` di dalam folder ini otomatis dimuat saat game dimulai.

---

### Pelajaran 2: Hello World (Script Pertama Anda)
Buat file baru di `scripts/halo.lua`:
```lua
Game.Log("Halo dari halo.lua!")
Game.PrintText("~y~Selamat Datang di AMLua!~n~~w~Mod Lua pertama berhasil dimuat.", 4000)
```

---

### Pelajaran 3: Memahami Timer Detik (Game.Every & Game.After)
Tidak perlu lagi pusing menghitung FPS! Cukup sebutkan detik:
```lua
-- Pulihkan darah setiap 1.5 detik
Game.Every(1.5, function()
    local ped = Player.GetPed()
    if ped ~= nil then
        Player.SetHealth(ped, 100.0)
    end
end)
```

---

### Pelajaran 4: Mod Ledakan (Membuat Ledakan Terarah)
Buat file `scripts/mod_ledakan.lua` untuk membuat ledakan di depan pemain:
```lua
-- Ledakan 10 meter di depan karakter setiap 5 detik
Game.Every(5.0, function()
    local x, y, z = Player.GetPosition()
    local headingDeg = Player.GetHeading()
    local rad = math.rad(headingDeg)

    local targetX = x - math.sin(rad) * 10.0
    local targetY = y + math.cos(rad) * 10.0
    local groundZ = Game.GetGroundZ(targetX, targetY)

    Explosion.Create(targetX, targetY, groundZ, 3, 10.0, true, 1.0)
    Game.PrintText("~r~BOOM!~n~~w~Ledakan berhasil diluncurkan.", 2000)
end)
```

---

### Pelajaran 5: Mod Speedometer & Nitro Boost Kendaraan
Buat file `scripts/nitro_boost.lua`:
```lua
Game.Every(0.5, function()
    if Player.IsInVehicle() then
        local veh = Vehicle.GetPlayerVehicle()
        local speed = Vehicle.GetSpeed(veh)

        -- Jika kecepatan pelan (< 40 km/h), beri dorongan nitro instan ke 120 km/h
        if speed > 10.0 and speed < 40.0 then
            Vehicle.SetSpeed(veh, 120.0)
            Game.PrintText("~b~NITRO BOOST AKTIF!~n~~w~Kecepatan: 120 km/h", 1500)
        end
    end
end)
```

---

### Pelajaran 6: Mod Teleportasi Aman dengan Ground Snapping
Saat teleportasi, selalu gunakan `Game.GetGroundZ(x, y)` agar karakter berdiri pas di permukaan tanah:
```lua
function TeleportKeBandaraLS()
    local targetX = 1680.0
    local targetY = -2330.0
    local targetZ = Game.GetGroundZ(targetX, targetY) + 1.0

    Game.Teleport(targetX, targetY, targetZ)
    Game.PrintText("~g~Teleportasi Berhasil!~n~~w~Tiba di Bandara Los Santos.", 3000)
end

-- Panggil fungsi setelah 3 detik game berjalan
Game.After(3.0, TeleportKeBandaraLS)
```

---

### Pelajaran 7: Mod Cuaca, Jam Game & Bonus Uang
```lua
Game.After(2.0, function()
    -- Set jam ke 12:00 siang
    Game.SetTime(12, 0)

    -- Set cuaca ke cerah (0)
    Game.SetWeather(0)

    -- Hilangkan wanted level polisi
    Player.ClearWantedLevel()

    -- Berikan uang $10,000
    Player.GiveMoney(10000)

    Game.PrintText("~y~Mod Siap!~n~~w~Siang Hari Cerah~n~~g~+$10,000 Uang Masuk!", 3500)
end)
```

---

### Pelajaran 8: Memanggil Script Lain Berkali-kali (Modular Modding)
Gunakan `dofile("nama_file.lua")` untuk mengeksekusi file Lua lain berulang kali:
```lua
-- Jalankan file senjata.lua
dofile("senjata.lua")

-- Jalankan potongan kode dinamis
Game.RunString("Game.PrintText('~p~Halo dari kode dinamis!', 2000)")
```

---

### Pelajaran 9: Debugging & Membaca Error Log
Jika terjadi kesalahan pada script, periksa:
- **`amlua_error.log`**: Menampilkan baris kode yang bermasalah dan pesan kesalahannya.
- **`amlua_crash.log`**: Menampilkan laporan jika game crash sistem.
- **`amlua.log`**: Menampilkan catatan aktivitas dan log mod.

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
1. nitro_boost.lua
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
     - Unduh **`AMLua-v1.0.6.zip`** (berisi kedua file binary `.so`, contoh script `test.lua`, dan dokumentasi lengkap).
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
