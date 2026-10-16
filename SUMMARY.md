# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.6.6** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O (stepper TMC2209), agitasi, draining, dan rinsing, dikendalikan lewat layar
OLED 128x64 + 6 tombol (PCF8574).

## Fitur Utama

- **Menu utama (4 item):** Pilih Preset, Konfigurasi Preset, Level Cairan, Kalibrasi
- **Level Cairan (read-only):** monitoring isi 5 jerigen (% + bar), scroll.
  Status per tangki: `ISI` / `KOSONG` (histeresis 10% / 3%) atau `DEAD` kalau
  sensor mati terdeteksi (raw beku di 0 > 60 detik).
- **Kalibrasi (10 item):** Sensor Level, Sensor Suhu, Sensor Udara, Motor HCl,
  Motor H2O2, Motor Air, Motor Air Sabun, Motor Limbah, Motor Agitasi,
  Exhaust Fan. Tombol seragam: ATAS/BAWAH pindah · KIRI/KANAN ubah ·
  ENTER aksi · BACK keluar + simpan.
- **Kalib Sensor Level:** pilih tangki dulu, lalu atur Kapasitas (ml), Baseline 0%,
  Delta 100% — diambil dari rata-rata 40 sampel (~2 detik), bukan 1 sampel.
- **Preset (maks 10):** nama, ukuran PCB, mode dosing auto/manual, volume, agitasi, total durasi.
  CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 5 fase** (non-blocking): `DOSING → ETCHING → DRAINING → RINSING → SELESAI`.
- **Sensor level GPIO langsung (5):** HCl/H2O2/H2O/sabun/limbah, rata-rata 4 sampel + histeresis.
- **Dosing stepper non-blocking:** 3 pompa peristaltik (TMC2209, mikrostep 1/16 =
  3200 step/rev), konversi ml → step via `langkahPerMl` hasil kalibrasi NVS.
  `EN`/`PDN` shared 3 driver; `stepperSleep(true)` mematikan semua driver saat
  proses berhenti.
- **Aktuator DC (PWM LEDC):** motor agitasi, pompa air sabun, pompa limbah, exhaust fan. PWM diambil dari `kalibDuty[]` hasil kalibrasi (interpolasi 5 tingkat agitasi dari 3 titik kalibrasi).
- **Kalibrasi level cairan:** tiap tangki punya baseline 0% (fixed) + delta 100%
  + kapasitas ml, tersimpan di NVS. Persen = `|raw − kering| / deltaPenuh`.
- **Penyimpanan NVS:** preset via `Preferences` dengan checksum.
- **Keamanan:** E-stop (GPIO), abort proses, semua aktuator mati saat batal/selesai.
- **UI:** header, list dengan animasi, toast, dialog, halaman proses dengan progress.
- **LED status WS2812 (GPIO 7):** indikator visual — biru breathing saat proses,
  kuning saat jeda, hijau solid 2 detik saat selesai, merah saat E-stop/gagal,
  hijau redup saat idle. 8 mode, animasi non-blocking.
- **LED RGB per sensor (GPIO 47):** 5 pixel WS2812, warna hijau/kuning/merah
  menurut level tangki + kecerahan_diskalakan level sebagai bar liquid.

## Struktur File

| File | Isi |
|---|---|
| `auto-etching-firmware.ino` | setup/loop utama, navigasi menu, helper konfigurasi |
| `config.h` | Pin map, konstanta global, konfigurasi hardware |
| `input.h` | Debounce & pembacaan 6 tombol via PCF8574 |
| `data.h` | Struct Preset, enum halaman, state global, list |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | 5 sensor touch (level + status ISI/KOSONG) + 3 DS18B20 1-bus (address dipindai) + MQ-135 ppm |
| `storage.h` | Preferences (NVS), checksum, simpan/muat preset & 4 blob kalibrasi |
| `motor.h` | Stepper TMC2209 non-blocking (pompa peristaltik) + `stepperSleep()`/`pompaMatikanTotal()` |
| `dcmotor.h` | DC motor via TB6612FNG, PWM dari `kalibDuty[]` (agitasi interpolasi 5 tingkat, pompa, fan) |
| `buzzer.h` | Buzzer non-blocking (state machine berbasis millis) |
| `led_strip.h` | LED status WS2812 GPIO 7, 8 mode (Boot/Idle/Proses/Jeda/Selesai/Error/E-stop/Warning), animasi non-blocking |
| `process.h` | State machine proses etching, jeda/batal/selesai |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi |
| `fontmini.h` | Font data untuk OLED |
| `dbg.h` | Debug helper (serial print) |

## Pin Map (v0.6.6)

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz, bus bersama OLED + PCF8574 |
| OLED SSD1306 | 0x3C | |
| PCF8574 (tombol) | 0x20 | 6 tombol |
| Sensor level | 11/12/13/14/3 | HCl/H2O2/H2O/sabun/limbah, touchRead |
| **TMC2209 EN (shared 3 driver)** | 1 | **aktif LOW** — HIGH = driver tidur |
| **TMC2209 PDN (shared 3 driver)** | 2 | aktif LOW, dipulse saat init |
| Stepper HCl (STEP/DIR) | 38 / 48 | TMC2209 channel 1 |
| Stepper H2O2 (STEP/DIR) | 40 / 39 | TMC2209 channel 2 |
| Stepper H2O (STEP/DIR) | 42 / 41 | TMC2209 channel 3 |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| PWM fan | 18 | TB6612FNG Driver2 PWB |
| Buzzer | 6 | pasif, LEDC |
| LED status | 7 | WS2812, 1 pixel, 8 mode |
| **LED RGB per sensor** | 47 | WS2812, 5 pixel, bar level |
| E-stop | 8 | INPUT_PULLUP |
| MQ-135 | 9 | analog + divider R1=10k/R2=15k |
| DS18B20 (bus tunggal) | 10 | 1-Wire, 3 sensor via DeviceAddress |
| — | 21 | **bebas** (dari `PIN_DIR3` DRV8825 lama) |
| — | 36 / 45 | tidak dipakai (sisa desain DS18B20 3-bus) |

## Riwayat Versi

Lihat `LOG.md` untuk detail perubahan per versi.
