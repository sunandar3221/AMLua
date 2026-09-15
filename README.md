# AMLua - Android Mod Loader (AML) Lua Script Loader
> **Lua Scripting Runtime & In-Game Mod Loader untuk Grand Theft Auto: San Andreas Android**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://android.com)
[![Architecture](https://img.shields.io/badge/Arch-arm64--v8a%20%7C%20armeabi--v7a-orange.svg)](#)
[![Lua Version](https://img.shields.io/badge/Lua-5.4.6-blue.svg)](https://www.lua.org)
[![Version](https://img.shields.io/badge/Version-1.1.0-brightgreen.svg)](#)

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
   - [Modul Http (Koneksi Internet & REST API)](#1-modul-http-koneksi-internet--rest-api)
   - [Modul Json (Parser & Serializer JSON)](#2-modul-json-parser--serializer-json)
   - [Modul Weapon (Senjata & Amunisi)](#3-modul-weapon-senjata--amunisi)
   - [Modul Device (Fitur Android: Getar, Toast, Baterai)](#4-modul-device-fitur-android-getar-toast-baterai)
   - [Modul Audio (Efek Suara & Radio Mobil)](#5-modul-audio-efek-suara--radio-mobil)
   - [Modul File (Membaca, Menulis & Menyimpan File)](#6-modul-file-membaca-menulis--menyimpan-file)
   - [Modul Screen & Camera (Fade Layar & Goyang Kamera)](#7-modul-screen--camera-fade-layar--goyang-kamera)
   - [Modul Explosion (Ledakan)](#8-modul-explosion-ledakan)
   - [Modul Player (Karakter Pemain)](#9-modul-player-karakter-pemain)
   - [Modul Vehicle (Manipulasi & Spawn Kendaraan)](#10-modul-vehicle-manipulasi--spawn-kendaraan)
   - [Modul Game (Manipulasi Dunia & Lingkungan)](#11-modul-game-manipulasi-dunia--lingkungan)
   - [Modul Timer (Alternatif Mudah Berbasis Detik)](#12-modul-timer-alternatif-mudah-berbasis-detik)
   - [Modul Touch (Trigger Zona Layar Sentuh 1-9 & Gestur Cleo)](#13-modul-touch-trigger-zona-layar-sentuh-1-9--gestur-cleo)
   - [Modul AMLua & Global](#14-modul-amlua--global)
   - [Daftar Kode Warna & Format Dialog GTA SA](#15-daftar-kode-warna--format-dialog-gta-sa)
4. [Belajar Kilat AMLua Sampai Bisa (Tutorial Lengkap dari Nol)](#-belajar-kilat-amlua-sampai-bisa-tutorial-lengkap-dari-nol)
   - [Pelajaran 14: Trigger Script Pakai Zona Layar Sentuh 1-9 (Cleo Touch Zones)](#pelajaran-14-trigger-script-pakai-zona-layar-sentuh-1-9-cleo-touch-zones)
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
3. **Symbol Hooking**: AMLua mencari alamat memori internal `libGTASA.so` untuk menemukan fungsi penting seperti loop game (`CGame::Process`), pencari pemain (`FindPlayerPed`), kendaraan (`FindPlayerVehicle`), mesin script SCM (`CRunningScript::ProcessOneCommand`), ledakan (`CExplosion::AddExplosion`, `CWorld::TriggerExplosion`), cuaca (`CWeather`), waktu (`CClock`), dan dialog native (`CHud::SetHelpMessage`).
4. **VM Initialization**: AMLua menyalakan runtime engine Lua 5.4.6, mendaftarkan seluruh API modul (`Explosion`, `Player`, `Vehicle`, `Game`, `Timer`, `AMLua`), dan mengaktifkan proteksi Sandbox.
5. **Auto-Discovery**: AMLua otomatis membaca isi folder `scripts/`, lalu mengeksekusi semua file `.lua` secara berurutan.
6. **Tick & Real-Time Timers**: AMLua mengeksekusi timer berbasis detik (`Game.Every` / `Game.After`) secara independen dari FPS dan mendispatch callback `Game.OnTick`.
7. **In-Game Dialog**: AMLua menyapa pemain dengan dialog sambutan resmi di pojok kanan atas layar begitu karakter siap dimainkan.

### Fitur Utama & Keunggulan
- **Dedicated Distinct Naming**: Nama file 64-bit (`libAMLua64.so`) dan 32-bit (`libAMLua32.so`) berbeda, mencegah salah pasang atau tertimpa tanpa sengaja.
- **Direct `.so` Download**: File `.so` dapat diunduh langsung satu per satu di GitHub Release tanpa perlu mengekstrak file ZIP di smartphone.
- **Full Crash & Error Logging**: Setiap ada error atau game crash, penyebabnya langsung disimpan di file log diagnostik (`amlua_crash.log` dan `amlua_error.log`).
- **Rich Modding API (v1.0.9)**: Mendukung modul `Explosion` dengan render visual bola api penuh, manipulasi kendaraan lengkap (`Vehicle`), teleportasi aman (`Player.Teleport`, `Game.Teleport`), deteksi ketinggian tanah (`Game.GetGroundZ`), waktu, cuaca, wanted level, uang, dan speed game.
- **Bulletproof Anti-Crash Protection & Visual Explosion Engine (v1.0.9)**:
  - **Visual Ledakan 100% Muncul (Bola Api, Asap, Kilatan Cahaya, Scorch Mark)**: Memperbaiki masalah efek visual ledakan yang tidak terlihat pada versi sebelumnya. AMLua kini menggunakan arsitektur eksekusi multi-layer:
    1. **Native SCM Opcode Execution**: Mengeksekusi instruksi script internal GTA SA `020C` (`ADD_EXPLOSION`), `0948` (`ADD_EXPLOSION_VARIABLE_SHAKE`), dan `0565` (`ADD_EXPLOSION_NO_SOUND`) via `CRunningScript::ProcessOneCommand()`. Mesin game resmi GTA SA secara otomatis memunculkan 3D expanding mesh fireball, kilatan cahaya oranye dinamis di sekitar lokasi, kepulan asap tebal, partikel debu ledakan, dan bekas hangus (*scorch mark*) di aspal/tanah!
    2. **Direct Calling Convention Bypass**: Mendukung pemanggilan langsung `CExplosion::AddExplosion` baik pass-by-value (`float x, float y, float z` / `7CVector` seperti offset `0x5A70D0` di ARM32) maupun pass-by-reference (`const CVector&`).
    3. **CWorld::TriggerExplosion Synchronized**: Dipanggil bersamaan untuk menjamin physics impulse (terlemparnya ped/kendaraan) dan sector damage tetap terjadi 100% akurat sesuai radius kustom.
  - Mengatasi fatal crash `FindPlayerVehicle(-1)` dengan helper aman `GetLocalPlayerVehicle()`.
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

### 1. Modul `Http` (Koneksi Internet & REST API)
> 🚀 **Baru di v1.1.0!** Modul untuk menghubungkan game GTA San Andreas ke internet secara real-time! Mendukung protokol HTTP dan HTTPS native dengan verifikasi SSL sistem Android, asynchronous callback non-blocking (tidak membuat game freeze/lag), JSON REST API, webhook, dan download file langsung.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Http.Get(url, [headers], callback)` | `url` (string), `[headers]` (table, opsional), `callback(response)` (fn) | *(tidak ada)* | Mengirim HTTP/HTTPS GET request secara asinkron (latar belakang). Saat selesai, callback dieksekusi di game loop dengan tabel response. |
| `Http.Post(url, body, [headers], callback)` | `url` (string), `body` (string), `[headers]` (table, opsional), `callback(response)` (fn) | *(tidak ada)* | Mengirim HTTP/HTTPS POST request secara asinkron dengan payload string/JSON. |
| `Http.Request(options, callback)` | `options` (`{ url, method, body, headers, timeout }`), `callback(response)` | *(tidak ada)* | Format request fleksibel untuk method GET, POST, PUT, DELETE, PATCH, atau HEAD. |
| `Http.GetSync(url, [headers], [timeoutMs])` | `url` (string), `[headers]` (table, opsional), `[timeoutMs]` (int, default 10000) | `table` (response) | Mengirim GET request secara sinkron (blocking). |
| `Http.PostSync(url, body, [headers], [timeoutMs])` | `url`, `body`, `[headers]`, `[timeoutMs]` | `table` (response) | Mengirim POST request secara sinkron (blocking). |
| `Http.Download(url, destPath, callback)` | `url` (string), `destPath` (string), `callback(ok, err)` (fn) | *(tidak ada)* | Mengunduh file dari internet langsung disimpan ke direktori file perangkat (misal file audio, texture, atau mod) secara asinkron. |
| `Http.DownloadSync(url, destPath)` | `url` (string), `destPath` (string) | `boolean, [err]` | Mengunduh file secara langsung (sinkron). |

> **Format Struktur Tabel `response`:**
> ```lua
> {
>     ok = true,          -- boolean: true jika status code 200..399
>     status = 200,       -- integer: kode status HTTP (200, 404, 500, dll.)
>     body = "...",       -- string: isi konten respon dari server
>     headers = { ... },  -- table: header respon HTTP
>     error = nil         -- string/nil: pesan error jika gagal koneksi / DNS / timeout
> }
> ```

#### Contoh Penggunaan `Http`:
```lua
-- Mengambil data dari REST API publik secara asinkron
Http.Get("https://httpbin.org/get", function(res)
    if res.ok then
        local data = Json.Decode(res.body)
        Game.PrintText("~g~Koneksi Internet Berhasil!~n~~w~IP: " .. tostring(data.origin), 4000)
    else
        Game.Log("Gagal koneksi internet: " .. tostring(res.error))
    end
end)
```

---

### 2. Modul `Json` (Parser & Serializer JSON)
> 🚀 **Baru di v1.1.0!** Parser dan serializer JSON berkecepatan tinggi, zero-dependency, dan aman untuk mengolah data web REST API atau konfigurasi file `.json`.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Json.Decode(jsonStr)` | `jsonStr` (string JSON) | `table/val, [err]` | Mengubah string JSON menjadi tabel Lua. Alias: `Json.Parse`. |
| `Json.Parse(jsonStr)` | `jsonStr` (string JSON) | `table/val, [err]` | Alias praktis untuk `Json.Decode`. |
| `Json.Encode(value, [pretty])` | `value` (table/any), `[pretty]` (bool, opsional) | `string, [err]` | Mengubah tabel Lua menjadi string JSON. Jika `pretty = true`, format JSON akan rapi dengan indentasi. Alias: `Json.Stringify`. |
| `Json.Stringify(value, [pretty])` | `value` (table/any), `[pretty]` (bool, opsional) | `string, [err]` | Alias praktis untuk `Json.Encode`. |

#### Contoh Penggunaan `Json`:
```lua
local profile = { name = "CJ", money = 50000, weapons = { "AK47", "Minigun" } }
local jsonText = Json.Encode(profile, true)
Game.Log("JSON Data:\n" .. jsonText)

local parsed = Json.Decode(jsonText)
Game.PrintText("~y~Player: ~w~" .. parsed.name .. " | Saldo: $" .. parsed.money, 3000)
```

---

### 3. Modul `Weapon` (Senjata & Amunisi)
> 🚀 **Baru di v1.1.0!** Mengontrol persenjataan karakter pemain dan NPC secara lengkap.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Weapon.Give([ped], weaponId, ammo)` | `[ped]` (pointer/handle, opsional), `weaponId` (int), `ammo` (int) | `boolean` | Memberikan senjata dan amunisi kepada karakter. Jika `ped` dikosongkan, otomatis diberikan ke pemain. |
| `Weapon.SetCurrent([ped], weaponId)` | `[ped]` (opsional), `weaponId` (int) | `boolean` | Memaksa karakter untuk menggenggam/memilih senjata tertentu di tangannya. |
| `Weapon.Remove([ped], weaponId)` | `[ped]` (opsional), `weaponId` (int) | `boolean` | Menghapus senjata tertentu dari inventaris karakter. |
| `Weapon.RemoveAll([ped])` | `[ped]` (pointer/handle, opsional) | `boolean` | Menghapus seluruh senjata dari karakter (kosong melompong). |

> **Konstanta Senjata Lengkap (`Weapon.*`):**
> `Weapon.FIST` (0), `Weapon.BRASSKNUCKLE` (1), `Weapon.GOLFCLUB` (2), `Weapon.NIGHTSTICK` (3), `Weapon.KNIFE` (4), `Weapon.BASEBALLBAT` (5), `Weapon.SHOVEL` (6), `Weapon.POOLCUE` (7), `Weapon.KATANA` (8), `Weapon.CHAINSAW` (9), `Weapon.FLOWERS` (14), `Weapon.CANE` (15), `Weapon.GRENADE` (16), `Weapon.TEARGAS` (17), `Weapon.MOLOTOV` (18), `Weapon.PISTOL` (22), `Weapon.PISTOL_SILENCED` (23), `Weapon.DESERT_EAGLE` (24), `Weapon.SHOTGUN` (25), `Weapon.SAWNOFF` (26), `Weapon.SPAS12` (27), `Weapon.MICRO_UZI` (28), `Weapon.MP5` (29), `Weapon.AK47` (30), `Weapon.M4` (31), `Weapon.TEC9` (32), `Weapon.RIFLE` (33), `Weapon.SNIPER` (34), `Weapon.ROCKETLAUNCHER` (35), `Weapon.HEATSEEKER` (36), `Weapon.FLAMETHROWER` (37), `Weapon.MINIGUN` (38), `Weapon.SATCHEL` (39), `Weapon.DETONATOR` (40), `Weapon.SPRAYCAN` (41), `Weapon.EXTINGUISHER` (42), `Weapon.CAMERA` (43), `Weapon.NIGHTVISION` (44), `Weapon.THERMAL` (45), `Weapon.PARACHUTE` (46).

---

### 4. Modul `Device` (Fitur Android: Getar, Toast, Baterai)
> 🚀 **Baru di v1.1.0!** Mengakses fitur hardware smartphone Android langsung dari script mod!

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Device.Vibrate([durationMs])` | `[durationMs]` (int ms, default 100) | `boolean` | Menggetarkan smartphone Android (haptic feedback). Cocok untuk efek tabrakan, tembakan, atau ledakan! |
| `Device.CancelVibrate()` | *(tidak ada)* | `boolean` | Menghentikan getaran yang sedang berlangsung. |
| `Device.Toast(message, [longer])` | `message` (string), `[longer]` (bool) | `boolean` | Menampilkan pesan notifikasi pop-up Android (*Android Native Toast*) di bagian bawah layar HP. |
| `Device.GetBatteryLevel()` | *(tidak ada)* | `number` (float 0.0 - 100.0) | Mengambil sisa baterai smartphone saat ini dalam persen. |
| `Device.GetAndroidVersion()` | *(tidak ada)* | `integer` | Mengambil versi API level Android perangkat (misal 30 untuk Android 11, 33 untuk Android 13). |
| `Device.GetDisplaySize()` | *(tidak ada)* | `w, h` (2 integers) | Mengambil resolusi lebar dan tinggi layar smartphone. |

---

### 5. Modul `Audio` (Efek Suara & Radio Mobil)
> 🚀 **Baru di v1.1.0!** Memutar sound effect game GTA dan mengatur stasiun radio kendaraan.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Audio.PlaySound(soundId, [x, y, z])` | `soundId` (int), `[x, y, z]` (numbers, opsional) | `boolean` | Memutar sound effect game GTA SA. Jika koordinat diberikan, suara terdengar 3D dari posisi tersebut. |
| `Audio.SetRadioStation(stationId)` | `stationId` (int 0 - 13) | `boolean` | Mengganti stasiun radio mobil yang sedang dinaiki. |
| `Audio.GetRadioStation()` | *(tidak ada)* | `integer` | Mengambil ID stasiun radio kendaraan saat ini. |

> **Konstanta Radio (`Audio.*`):**
> `Audio.RADIO_OFF` (0), `Audio.RADIO_BOUNCE` (1), `Audio.RADIO_CSR` (2), `Audio.RADIO_K_ROSE` (3), `Audio.RADIO_K_DST` (4), `Audio.RADIO_BOUNCE_FM` (5), `Audio.RADIO_SF_UR` (6), `Audio.RADIO_LOS_SANTOS` (7), `Audio.RADIO_RADIO_X` (8), `Audio.RADIO_CSR_103_9` (9), `Audio.RADIO_K_JAH` (10), `Audio.RADIO_MASTER_SOUNDS` (11), `Audio.RADIO_WCTR` (12), `Audio.RADIO_USER_TRACKS` (13).

---

### 6. Modul `File` (Membaca, Menulis & Menyimpan File)
> 🚀 **Baru di v1.1.0!** Sistem file I/O yang aman untuk menyimpan data save mod, high score, membaca file config, dll.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `File.Read(filePath)` | `filePath` (string) | `string` atau `nil, err` | Membaca seluruh isi file sebagai string. |
| `File.Write(filePath, content)` | `filePath` (string), `content` (string) | `boolean, [err]` | Menulis/menimpa isi file dengan teks baru. |
| `File.Append(filePath, content)` | `filePath` (string), `content` (string) | `boolean, [err]` | Menambahkan teks ke baris paling akhir file (append). |
| `File.Exists(filePath)` | `filePath` (string) | `boolean` | Mengecek apakah file atau folder ada di penyimpanan. |
| `File.Delete(filePath)` | `filePath` (string) | `boolean` | Menghapus file dari penyimpanan. |
| `File.List(dirPath)` | `dirPath` (string) | `table` (array of strings) | Mengambil daftar nama file dan subfolder di direktori tertentu. |
| `File.GetScriptsPath()` | *(tidak ada)* | `string` | Mengambil path folder scripts (`.../files/scripts`). |
| `File.GetDataPath()` | *(tidak ada)* | `string` | Mengambil path folder data GTA SA (`.../files`). |

---

### 7. Modul `Screen` & `Camera` (Fade Layar & Goyang Kamera)
> 🚀 **Baru di v1.1.0!** Mengontrol efek visual transisi layar dan kamera sinematik.

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Screen.Fade(fadeIn, [durationMs])` | `fadeIn` (bool: true=in, false=out), `[durationMs]` (ms) | `boolean` | Efek layar hitam transisi fade in / fade out sinematik. |
| `Screen.FadeIn([durationMs])` | `[durationMs]` (int ms, default 1000) | `boolean` | Layar kembali terang dari hitam (Fade In). |
| `Screen.FadeOut([durationMs])` | `[durationMs]` (int ms, default 1000) | `boolean` | Layar menggelap menjadi hitam (Fade Out). |
| `Camera.Shake([intensity])` | `[intensity]` (int, default 70) | `boolean` | Mengguncang kamera (*camera shake*). Sangat cocok saat ada gempa, tabrakan, atau ledakan dahsyat! |
| `Camera.Restore()` | *(tidak ada)* | `boolean` | Mengembalikan posisi kamera ke belakang pemain seperti semula. |

---

### 8. Modul `Explosion` (Ledakan)
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

### 9. Modul `Player` (Karakter Pemain)
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

### 10. Modul `Vehicle` (Manipulasi & Spawn Kendaraan)
Modul untuk membuat (spawn) dan memanipulasi kendaraan (mobil, motor, sepeda, pesawat, helikopter, perahu). Parameter `veh` bersifat fleksibel: dapat berupa pointer (`lightuserdata`), handle SCM (`integer`), atau jika dikosongkan, otomatis menggunakan kendaraan yang sedang dinaiki pemain!

| Fungsi | Parameter | Nilai Balik | Deskripsi |
| :--- | :--- | :--- | :--- |
| `Vehicle.Create(modelId, [x, y, z, heading])` | `modelId` (int), `[x, y, z, heading]` (opsional) | `handle, pointer` | 🚀 **Baru di v1.1.0!** Men-spawn kendaraan baru ke dunia game secara instan dengan auto-loading streaming model tanpa crash! Jika koordinat tidak diisi, mobil muncul tepat di depan pemain. Mengembalikan ID handle dan pointer memori. Alias: `Vehicle.Spawn`. |
| `Vehicle.Spawn(modelId, ...)` | *(sama dengan Vehicle.Create)* | `handle, pointer` | Alias untuk `Vehicle.Create`. |
| `Vehicle.SetColor([veh], primary, secondary)`| `[veh]`, `primary` (int), `secondary` (int) | `boolean` | 🚀 **Baru di v1.1.0!** Mengubah warna primer dan sekunder kendaraan. |
| `Vehicle.SetEngineState([veh], state)` | `[veh]`, `state` (boolean: true=hidup, false=mati) | `boolean` | 🚀 **Baru di v1.1.0!** Menghidupkan atau mematikan mesin kendaraan seketika. |
| `Vehicle.PopTyre([veh], tyreId)` | `[veh]`, `tyreId` (0: Kiri Depan, 1: Kiri Belakang, 2: Kanan Depan, 3: Kanan Belakang) | `boolean` | 🚀 **Baru di v1.1.0!** Meletuskan ban kendaraan secara spesifik. |
| `Vehicle.GetHandle([veh])` | `[veh]` (pointer/handle) | `integer` | Mengambil handle SCM unik kendaraan. |
| `Vehicle.GetPointer([veh])` | `[veh]` (pointer/handle) | `lightuserdata` | Mengambil pointer memori native `CVehicle*`. |
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

### 11. Modul `Game` (Manipulasi Dunia & Lingkungan)
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

### 12. Modul `Timer` (Alternatif Mudah Berbasis Detik)
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

---

### 13. Modul `Touch` (Trigger Zona Layar Sentuh 1-9 & Gestur Cleo)
AMLua menghadirkan sistem deteksi layar sentuh 9 zona (3x3 grid) yang **100% identik dengan standar CLEO Android**. Dengan modul ini, Anda dapat memicu script, memunculkan dialog menu, spawn mobil/senjata, atau mengaktifkan cheat hanya dengan menyentuh zona layar tertentu, menggeser jari (*swipe/slide*), ataupun menggunakan kombinasi tombol multi-touch.

#### 📐 Peta Pembagian 9 Zona Layar Sentuh (3x3 Grid):
Layar HP dibagi menjadi 3 kolom (horizontal) dan 3 baris (vertikal) secara proporsional:
- **Kolom X**: Kiri (`0% - 33%`), Tengah (`33% - 66%`), Kanan (`66% - 100%`)
- **Baris Y**: Atas (`0% - 33%`), Tengah (`33% - 66%`), Bawah (`66% - 100%`)

```text
+---------------------+---------------------+---------------------+
|       Zona 1        |       Zona 2        |       Zona 3        |  <- Baris Atas (Top)
|  Top-Left (Kiri)    | Top-Center (Tengah) |  Top-Right (Kanan)  |
|  (0% - 33% X)       | (33% - 66% X)       | (66% - 100% X)      |
+---------------------+---------------------+---------------------+
|       Zona 4        |       Zona 5        |       Zona 6        |  <- Baris Tengah (Center)
| Center-Left (Kiri)  | Center (Tgh Layar)  | Center-Right (Kanan)|
|  (Area Tombol Arah) | (Area Bebas Menu)   | (Area Tombol Aksi)  |
+---------------------+---------------------+---------------------+
|       Zona 7        |       Zona 8        |       Zona 9        |  <- Baris Bawah (Bottom)
| Bottom-Left (Kiri)  | Bottom-Center (Tgh) | Bottom-Right (Kanan)|
|  (0% - 33% X)       | (33% - 66% X)       | (66% - 100% X)      |
+---------------------+---------------------+---------------------+
```

#### 🎮 Tiga Metode Penggunaan API Touch di AMLua:

1. **Metode Event-Driven / Listener (Sangat Disarankan, Modern & Efisien)**
   Script mendaftarkan fungsi callback yang akan langsung dieksekusi secara instan saat aksi sentuhan terjadi, tanpa membebani performa CPU game:
   - `Touch.OnPress(zone, callback)`: Menjalankan kode seketika jari menyentuh zona.
   - `Touch.OnRelease(zone, callback)`: Menjalankan kode saat jari diangkat dari zona.
   - `Touch.OnHold(zone, durationMs, callback)`: Menjalankan kode setelah zona ditekan dan ditahan terus-menerus selama durasi tertentu.
   - `Touch.OnSlide(fromZone, toZone, callback)`: Menjalankan kode saat pemain menggeser layar dari satu zona ke zona lain (misal: slide dari 2 ke 8 seperti menu Cleo).
   - `Touch.OnDoubleTap(zone, callback)`: Menjalankan kode saat zona diketuk cepat 2 kali (< 400ms).
   - `Touch.OnCombo({zoneA, zoneB, ...}, callback)`: Menjalankan kode saat beberapa zona disentuh bersamaan dengan multi-touch.

2. **Metode Hook Global Otomatis (Tanpa Perlu Setup & Registrasi)**
   Cukup buat fungsi global ini di script `.lua` Anda, AMLua akan otomatis mendeteksi dan mengeksekusinya di setiap frame:
   - `function onTouchZone(zone, eventType, x, y)`: Menerima event `"press"`, `"release"`, atau `"doubletap"`.
   - `function onTouchSlide(fromZone, toZone, durationMs)`: Menerima gestur gesekan layar antar zona.

3. **Metode Polling Loop (Kompatibel 100% Opcode CLEO Android `0DE0` & `0DE1`)**
   Jika Anda sedang mem-porting script `.csa` / `.csi` Cleo lama ke AMLua, Anda dapat langsung memeriksa status zona di dalam `Game.OnTick` atau `Game.Every`:
   - `Touch.IsPressed(zone, [minTimeMs])` atau `Touch.GetPointState(zone, [minTimeMs])` -> setara opcode `0DE0`.
   - `Touch.IsSlide(fromZone, toZone, [maxAgeMs])` atau `Touch.GetSlideState(from, to, [minMs], [maxMs])` -> setara opcode `0DE1`.

---

#### 📚 Dokumentasi Detail Seluruh Fungsi Modul `Touch`:

##### 1. `Touch.OnPress(zone, callback)`
Mendaftarkan fungsi yang dipanggil saat zona tertentu mulai ditekan oleh pemain.
- **Parameter**:
  - `zone` *(number)*: Angka zona `1` sampai `9`. Masukkan `0` jika ingin merespons sentuhan di zona mana pun.
  - `callback` *(function(zone, x, y))*: Fungsi yang dijalankan saat disentuh. Menerima parameter nomor zona dan koordinat piksel `x`, `y`.
- **Return**: `number` (ID listener, dapat digunakan untuk `Touch.RemoveListener`).
- **Contoh**:
  ```lua
  Touch.OnPress(5, function(zone, x, y)
      print("Zona 5 (Tengah) disentuh di piksel:", x, y)
      Player.SetHealth(Player.GetPed(), 100)
  end)
  ```

##### 2. `Touch.OnRelease(zone, callback)`
Mendaftarkan fungsi yang dipanggil saat jari diangkat dari suatu zona.
- **Parameter**:
  - `zone` *(number)*: Angka zona `1` sampai `9` (atau `0` untuk semua zona).
  - `callback` *(function(zone, holdDurationMs))*: Fungsi yang dijalankan saat jari terangkat. Menerima parameter nomor zona dan lama waktu tekan dalam milidetik.
- **Return**: `number` (ID listener).

##### 3. `Touch.OnHold(zone, durationMs, callback)`
Mendaftarkan aksi yang dipicu saat suatu zona ditekan dan ditahan terus-menerus selama durasi waktu tertentu.
- **Parameter**:
  - `zone` *(number)*: Angka zona `1` sampai `9` (atau `0` untuk semua zona).
  - `durationMs` *(number)*: Lama minimal zona harus ditahan (misal `1000` untuk 1 detik).
  - `callback` *(function(zone, holdDurationMs))*: Fungsi yang dijalankan saat ambang batas waktu tercapai.
- **Return**: `number` (ID listener).
- **Contoh**:
  ```lua
  -- Tahan zona 1 (Pojok Kiri Atas) selama 1.5 detik untuk mengaktifkan cheat uang
  Touch.OnHold(1, 1500, function(zone, holdMs)
      Player.GiveMoney(250000)
      Audio.PlaySound(1058)
      Game.PrintText("~g~Cheat Uang +$250,000 Aktif!", 3000)
  end)
  ```

##### 4. `Touch.OnSlide(fromZone, toZone, callback)` (Alias: `Touch.OnSwipe`)
Mendaftarkan aksi saat pemain menggeser jari di layar dari satu zona ke zona lain.
- **Parameter**:
  - `fromZone` *(number)*: Zona awal gestur (`1` sampai `9`, atau `0` untuk zona apa saja).
  - `toZone` *(number)*: Zona akhir gestur (`1` sampai `9`, atau `0` untuk zona apa saja).
  - `callback` *(function(from, to, durationMs))*: Fungsi yang dijalankan saat gestur geser selesai.
- **Return**: `number` (ID listener).
- **Contoh Gestur Menu Cleo (Geser dari Atas ke Bawah: 2 ke 8)**:
  ```lua
  Touch.OnSlide(2, 8, function(from, to, durationMs)
      Device.Vibrate(80)
      Game.PrintText("~y~Menu Mod Aktif!~n~~w~Geser 2 ke 8 terdeteksi.", 2500)
      Vehicle.Create(411) -- Spawn Infernus
  end)
  ```

##### 5. `Touch.OnDoubleTap(zone, callback)`
Mendaftarkan aksi saat pemain mengetuk layar dengan cepat dua kali pada zona yang sama (jeda < 400ms).
- **Parameter**:
  - `zone` *(number)*: Angka zona `1` sampai `9`.
  - `callback` *(function(zone, x, y))*: Fungsi yang dijalankan saat ketukan ganda terdeteksi.
- **Return**: `number` (ID listener).
- **Contoh**:
  ```lua
  Touch.OnDoubleTap(3, function(zone, x, y)
      Game.SetWeather(0) -- Cuaca Cerah
      Game.PrintText("~b~Cuaca Diubah Menjadi Cerah!", 2000)
  end)
  ```

##### 6. `Touch.OnCombo(tableOfZones, callback)`
Mendaftarkan aksi kombinasi multi-touch saat dua atau lebih zona ditekan secara bersamaan di layar HP.
- **Parameter**:
  - `tableOfZones` *(table)*: Daftar array nomor zona (contoh: `{4, 6}` atau `{1, 9}`).
  - `callback` *(function(zonesTable))*: Fungsi yang dijalankan saat seluruh zona tersebut aktif ditekan bersamaan.
- **Return**: `number` (ID listener).
- **Contoh (Tekan Tombol Kiri dan Kanan Tengah Bersamaan: 4 & 6)**:
  ```lua
  Touch.OnCombo({4, 6}, function(zones)
      Device.Vibrate(150)
      Explosion.AtPlayer(Explosion.TINY, 0, 4, 2)
      Game.PrintText("~r~Combo 4 & 6 Aktif!", 2000)
  end)
  ```

##### 7. `Touch.IsPressed(zone, [minTimeMs])` (Alias: `Touch.IsZonePressed`)
Mengecek apakah suatu zona saat ini sedang ditekan oleh jari pemain.
- **Parameter**:
  - `zone` *(number)*: Angka zona `1` sampai `9`.
  - `minTimeMs` *(number, opsional)*: Minimal waktu tekan (ms). Jika diisi, mengembalikan `true` hanya jika sudah ditahan selama minimal waktu ini.
- **Return**: `boolean` (`true` jika ditekan, `false` jika tidak).

##### 8. `Touch.GetPointState(zone, [minTimeMs])`
Fungsi kompatibilitas 100% untuk opcode CLEO Android `0DE0: get_touch_point_state`.
- **Parameter**: Sama seperti `Touch.IsPressed`.
- **Return**: `number` (`1` jika ditekan, `0` jika tidak).

##### 9. `Touch.IsSlide(fromZone, toZone, [maxAgeMs])`
Mengecek apakah pemain baru saja melakukan gestur geser dari `fromZone` ke `toZone`. Setelah dibaca, status slide otomatis di-reset agar tidak terpicu ganda.
- **Parameter**:
  - `fromZone` *(number)*: Zona awal.
  - `toZone` *(number)*: Zona tujuan.
  - `maxAgeMs` *(number, opsional, default: 600)*: Toleransi usia maksimal gestur dalam milidetik.
- **Return**: `boolean`.

##### 10. `Touch.GetSlideState(fromZone, toZone, [minTimeMs], [maxTimeMs])`
Fungsi kompatibilitas 100% untuk opcode CLEO Android `0DE1: get_touch_slide_state`.
- **Parameter**: Zona awal, zona tujuan, minimal durasi geser, maksimal durasi geser.
- **Return**: `number` (`1` jika cocok, `0` jika tidak).

##### 11. `Touch.GetZone()` / `Touch.GetActiveZone()`
Mengambil nomor zona yang saat ini sedang disentuh jari utama pemain.
- **Return**: `number` (`1` sampai `9`, atau `0` jika tidak ada sentuhan).

##### 12. `Touch.GetPressedZones()`
Mengambil daftar seluruh zona yang saat ini sedang disentuh (cocok untuk mendeteksi multi-touch secara fleksibel).
- **Return**: `table` (array angka zona, misal `{1, 5}`).

##### 13. `Touch.GetHoldTime(zone)`
Mendapatkan durasi waktu (dalam milidetik) berapa lama zona tersebut telah ditekan secara terus-menerus.
- **Return**: `number` (durasi dalam ms, `0` jika zona tidak sedang ditekan).

##### 14. `Touch.GetPos([pointerIndex])`
Mengambil informasi posisi sentuhan jari tertentu.
- **Parameter**: `pointerIndex` *(number, opsional, default: 0 untuk jari pertama)*.
- **Return**: 5 nilai: `x` *(integer piksel)*, `y` *(integer piksel)*, `normX` *(float 0.0 - 1.0)*, `normY` *(float 0.0 - 1.0)*, `zone` *(1-9)*.

##### 15. `Touch.IsAnyPressed()`
Mengecek apakah layar sedang disentuh oleh setidaknya satu jari.
- **Return**: `boolean`.

##### 16. `Touch.GetDisplaySize()`
Mengambil resolusi tampilan layar HP dalam piksel.
- **Return**: `width, height` *(number, number)*.

##### 17. `Touch.RemoveListener(id)`
Menghapus event listener sentuhan yang telah didaftarkan sebelumnya berdasarkan ID return dari fungsi registrasi.
- **Parameter**: `id` *(number)*.
- **Return**: `boolean` (`true` jika berhasil dihapus).

##### 18. `Touch.ClearListeners()`
Menghapus seluruh event listener sentuhan yang ada.

##### 19. `Touch.SetModListEnabled(enable)`
Mengatur apakah gestur bawaan AMLua (geser 2 ke 8 untuk membuka dialog daftar mod aktif) diaktifkan atau dinonaktifkan. Berguna jika Anda ingin mendedikasikan gestur geser 2 ke 8 sepenuhnya untuk menu custom buatan Anda sendiri tanpa bentrok dengan daftar mod bawaan AMLua.
- **Parameter**: `enable` *(boolean)*.
- **Contoh**: `Touch.SetModListEnabled(false)`

##### 20. `Touch.ShowGrid([durationMs])`
Menampilkan pesan bantuan visual di HUD game yang menjelaskan posisi 9 zona layar sentuh (1 sampai 9) untuk membantu pemain dan modder memetakan letak tombol di layar HP mereka.
- **Parameter**: `durationMs` *(number, opsional, default: 5000)*.

---

#### 🏷️ Konstanta Zona Layar Sentuh (`Touch.ZONE_*`):
Tersedia konstanta resmi untuk mempermudah pembacaan kode script:
- `Touch.ZONE_TOP_LEFT` = `1` (Atas Kiri)
- `Touch.ZONE_TOP_CENTER` = `2` (Atas Tengah)
- `Touch.ZONE_TOP_RIGHT` = `3` (Atas Kanan)
- `Touch.ZONE_CENTER_LEFT` = `4` (Tengah Kiri)
- `Touch.ZONE_CENTER` = `5` (Tengah Layar)
- `Touch.ZONE_CENTER_RIGHT` = `6` (Tengah Kanan)
- `Touch.ZONE_BOTTOM_LEFT` = `7` (Bawah Kiri)
- `Touch.ZONE_BOTTOM_CENTER` = `8` (Bawah Tengah)
- `Touch.ZONE_BOTTOM_RIGHT` = `9` (Bawah Kanan)

---

### 14. Modul `AMLua` & Global
Namespace utama yang merangkum seluruh modul sistem dan game:
- `AMLua.Version` -> `"1.1.0"`
- `AMLua.Http` -> referensi ke modul `Http`
- `AMLua.Json` -> referensi ke modul `Json`
- `AMLua.Touch` -> referensi ke modul `Touch`
- `AMLua.Weapon` -> referensi ke modul `Weapon`
- `AMLua.Device` -> referensi ke modul `Device`
- `AMLua.Audio` -> referensi ke modul `Audio`
- `AMLua.File` -> referensi ke modul `File`
- `AMLua.Screen` -> referensi ke modul `Screen`
- `AMLua.Camera` -> referensi ke modul `Camera`
- `AMLua.Explosion` -> referensi ke modul `Explosion`
- `AMLua.Player` -> referensi ke modul `Player`
- `AMLua.Vehicle` -> referensi ke modul `Vehicle`
- `AMLua.Game` -> referensi ke modul `Game`
- `AMLua.Timer` -> referensi ke modul `Timer`
- `dofile("nama_file.lua")` -> fungsi global bawaan yang mencari file langsung di folder `scripts/`

---

### 15. Daftar Kode Warna & Format Dialog GTA SA
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

### Pelajaran 10: Menghubungkan Game ke Internet & REST API (Http & Json)
Kini Anda bisa mengambil data cuaca asli, berita, akun online, leaderboard, atau webhook Discord langsung dari dalam game GTA San Andreas Android:
```lua
-- Contoh: Mengambil data dari REST API publik
Http.Get("https://httpbin.org/get", function(response)
    if response.ok then
        local data = Json.Decode(response.body)
        Game.PrintText("~g~Internet Connected!~n~~w~IP: " .. tostring(data.origin), 4000)
        Device.Toast("Berhasil terhubung ke Internet!", false)
    else
        Game.Log("Gagal request HTTP: " .. tostring(response.error))
    end
end)
```

---

### Pelajaran 11: Memberikan Senjata & Amunisi (Weapon API)
Beri karakter Anda senjata apa saja tanpa perlu kode cheat:
```lua
-- Berikan AK-47 dengan 500 peluru dan langsung pegang di tangan
Weapon.Give(Weapon.AK47, 500)
Weapon.SetCurrent(Weapon.AK47)

-- Berikan Minigun dan RPG
Weapon.Give(Weapon.MINIGUN, 1000)
Weapon.Give(Weapon.ROCKETLAUNCHER, 50)

Audio.PlaySound(1052) -- Suara pickup senjata
Game.PrintText("~g~Senjata Lengkap Diberikan!", 3000)
```

---

### Pelajaran 12: Men-Spawn Mobil Kustom & Ubah Warna (Vehicle.Create)
Munculkan kendaraan favorit Anda tepat di depan karakter secara instan tanpa crash:
```lua
-- Spawn Infernus (Model ID 411) di depan pemain
local vehHandle, vehPtr = Vehicle.Create(411)
if vehHandle and vehHandle > 0 then
    -- Ubah warna menjadi Merah (3) dan Hitam (0)
    Vehicle.SetColor(vehHandle, 3, 0)
    Game.PrintText("~y~Infernus Berhasil Di-Spawn!", 3000)
end
```

---

### Pelajaran 13: Membaca & Menulis File Data Mod (File API)
Simpan skor pemain atau konfigurasi mod ke file teks:
```lua
local savePath = File.GetScriptsPath() .. "/data_pemain.txt"

-- Tulis data
File.Write(savePath, "Level: 5\nUang: 100000\nStatus: Juara")

-- Cek dan baca kembali
if File.Exists(savePath) then
    local isi = File.Read(savePath)
    Game.Log("Isi data file:\n" .. isi)
end
```

---

### Pelajaran 14: Mahir Trigger Script Menggunakan 9 Zona Layar Sentuh (Cleo Touch Zones)

Pada modding **CLEO Android**, para modder mengontrol cheat menu dan script tanpa keyboard menggunakan sistem **9 Zona Layar**. AMLua mengadopsi standar yang sama persis sehingga Anda dapat membuat menu cheat interaktif, tombol spawn cepat, ataupun tombol darurat di layar HP Anda!

```text
+---------------------+---------------------+---------------------+
|       Zona 1        |       Zona 2        |       Zona 3        |  <- Baris Atas (Top)
|  Top-Left (Kiri)    | Top-Center (Tengah) |  Top-Right (Kanan)  |
+---------------------+---------------------+---------------------+
|       Zona 4        |       Zona 5        |       Zona 6        |  <- Baris Tengah (Center)
| Center-Left (Kiri)  | Center (Tgh Layar)  | Center-Right (Kanan)|
+---------------------+---------------------+---------------------+
|       Zona 7        |       Zona 8        |       Zona 9        |  <- Baris Bawah (Bottom)
| Bottom-Left (Kiri)  | Bottom-Center (Tgh) | Bottom-Right (Kanan)|
+---------------------+---------------------+---------------------+
```

---

#### 🌟 Kasus 1: Membuat Menu Gestur Geser 2 ke 8 (Classic Cleo Swipe Menu)
Gestur paling legendaris di GTA SA Android adalah menggesek layar dari atas ke bawah (dari Zona 2 ke Zona 8). Mari buat script yang merespons gestur ini:

```lua
-- Simpan sebagai scripts/cleo_menu.lua

Touch.OnSlide(2, 8, function(fromZone, toZone, durationMs)
    -- 1. Berikan efek getar di HP Android (100 milidetik)
    Device.Vibrate(100)

    -- 2. Putar suara konfirmasi khas GTA (Sound ID 1058 = Bell audio)
    Audio.PlaySound(1058)

    -- 3. Berikan paket senjata berat dan amunisi
    local ped = Player.GetPed()
    if ped ~= nil then
        Weapon.Give(ped, Weapon.MINIGUN, 1000)
        Weapon.Give(ped, Weapon.ROCKETLAUNCHER, 50)
        Weapon.SetCurrent(ped, Weapon.MINIGUN)
    end

    -- 4. Tampilkan pesan dialog di layar dan notifikasi Toast Android
    Game.PrintText("~y~Cleo Gesture (2->8) Terdeteksi!~n~~w~Minigun & RPG Siap Tempur!", 3500)
    Device.Toast("AMLua: Menu Cleo Berhasil Terbuka!", false)
end)
```

---

#### 🌟 Kasus 2: Tombol Cepat Darah & Armor di Tengah Layar (Zona 5)
Ingin tombol darurat saat baku tembak? Manfaatkan Zona 5 (tengah layar kosong yang jarang tersentuh tombol analog/tembak):

```lua
-- Simpan sebagai scripts/quick_heal.lua

Touch.OnPress(5, function(zone, x, y)
    local ped = Player.GetPed()
    if ped ~= nil then
        local hp = Player.GetHealth(ped)
        if hp < 100.0 then
            Player.SetHealth(ped, 100.0)
            Player.SetArmour(ped, 100.0)
            Audio.PlaySound(1052) -- Suara pickup item
            Device.Vibrate(50)
            Game.PrintText("~g~Zona 5 Ditekan!~n~~w~Health & Armour Full 100%!", 2000)
        end
    end
end)
```

---

#### 🌟 Kasus 3: Tombol Rahasia Multi-Jari Combo (Sentuh Zona 4 & 6 Bersamaan)
Tekan area kiri layar (Zona 4) dan kanan layar (Zona 6) secara bersamaan menggunakan jempol kiri dan jempol kanan untuk memunculkan kendaraan impian di depan karakter:

```lua
-- Simpan sebagai scripts/spawn_combo.lua

Touch.OnCombo({4, 6}, function(zones)
    Device.Vibrate(150)
    Audio.PlaySound(1057)

    -- Spawn motor super kencang NRG-500 (ID 522)
    local veh = Vehicle.Create(522)
    if veh and veh > 0 then
        Vehicle.SetColor(veh, 6, 1) -- Warna Kuning / Putih
        Game.PrintText("~y~Combo 4+6 Aktif!~n~~w~NRG-500 Berhasil Muncul di Depan Anda!", 3000)
    end
end)
```

---

#### 🌟 Kasus 4: Fitur Tahan Layar (OnHold) & Double-Tap
AMLua juga mendukung deteksi menahan layar dan ketuk ganda (*double-tap*):

```lua
-- Tahan pojok kiri atas (Zona 1) selama 1.5 detik (1500 ms) untuk mendapatkan uang
Touch.OnHold(1, 1500, function(zone, holdMs)
    Player.GiveMoney(250000)
    Audio.PlaySound(1058)
    Game.PrintText("~g~Tahan Zona 1 Sukses!~n~~w~Bonus Uang: +$250,000", 3000)
end)

-- Ketuk cepat 2 kali pada pojok kanan atas (Zona 3) untuk ganti cuaca cerah
Touch.OnDoubleTap(3, function(zone, x, y)
    Game.SetWeather(0) -- Cuaca Extra Sunny
    Game.PrintText("~b~Double-Tap Zona 3:~n~~w~Cuaca Menjadi Cerah!", 2500)
end)
```

---

#### 🌟 Kasus 5: Format Script Kilat Menggunakan Hook Global Otomatis
Bagi pemula yang tidak ingin memanggil fungsi registrasi, Anda cukup menulis fungsi global `onTouchZone` atau `onTouchSlide` langsung di script:

```lua
-- Simpan sebagai scripts/sentuh_bebas.lua

function onTouchZone(zone, eventType, x, y)
    -- eventType bernilai: "press", "release", atau "doubletap"
    if eventType == "press" and zone == 2 then
        Game.PrintText("~w~Zona Atas-Tengah (2) Disentuh!", 1500)
    end
end

function onTouchSlide(fromZone, toZone, durationMs)
    Game.Log(string.format("Pemain menggeser dari Zona %d ke Zona %d dalam %.0f ms", fromZone, toZone, durationMs))
end
```

---

#### 🌟 Kasus 6: Panduan Migrasi Script CLEO Lama ke AMLua
Berikut adalah tabel perbandingan cara membaca sentuhan layar antara kode CLEO Android kuno dan kode modern AMLua:

| Fitur Sentuhan | Kode CLEO Android (Sanny Builder) | Kode AMLua (Lua Modern) |
| :--- | :--- | :--- |
| **Cek Tekan Zona 5** | `0DE0: 0@ = get_touch_point_state 5 mintime 0`<br>`if 0@ == 1` | `if Touch.IsPressed(5) then`<br>*(atau gunakan event: `Touch.OnPress(5, fn)`)* |
| **Cek Tahan Zona 1 (1 detik)** | `0DE0: 0@ = get_touch_point_state 1 mintime 1000`<br>`if 0@ == 1` | `if Touch.IsPressed(1, 1000) then`<br>*(atau `Touch.OnHold(1, 1000, fn)`)* |
| **Cek Geser 2 ke 8 (Menu)** | `0DE1: 0@ = get_touch_slide_state 2 8 mintime 0 maxtime 1000`<br>`if 0@ == 1` | `if Touch.IsSlide(2, 8) then`<br>*(atau `Touch.OnSlide(2, 8, fn)`)* |
| **Cek Zona Tengah (Kode Opcode)**| `0DE0: 0@ = get_touch_point_state 5 mintime 0` | `0@ = Touch.GetPointState(5)` *(100% identik)* |

---

#### 💡 Tips Menguji Letak Zona di Layar HP
Untuk memastikan Anda menekan zona yang benar saat bermain di HP, panggil fungsi visual:
```lua
Touch.ShowGrid()
```
Game akan langsung menampilkan jendela bantuan HUD di pojok kanan atas dengan peta posisi 9 kotak zona layar!

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
     - Unduh **`AMLua-v1.1.0.zip`** (berisi kedua file binary `.so`, contoh script `test.lua`, dan dokumentasi lengkap).
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
