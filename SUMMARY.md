# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.5.4** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O (stepper TMC2209), agitasi, draining, dan rinsing, dikendalikan lewat
layar OLED 128x64 + 6 tombol (PCF8574).

> **Status akurasi dosing:** belum akurat. Nilai `langkahPerMl` masih hardcode
> dan hasil kalibrasi belum dipakai. Rinciannya di `dosing-design.md`;
> status per item di `PROGRESS.md` §3.

## Fitur Utama

- **Menu utama (4 item):** Pilih Preset, Konfigurasi Preset, Level Cairan, Kalibrasi
- **Kalibrasi (4 submenu):** Motor/Pompa, Sensor Level, Sensor Suhu (3 blok), Sensor Udara
  - Motor: 7 item — Pompa HCl, Pompa H2O2, Pompa Air (stepper), Pompa Sabun,
    Pompa Limbah (DC), Motor Agitasi, Exhaust Fan. Tombol seragam:
    ATAS/BAWAH pindah · KIRI/KANAN ubah · ENTER aksi · BACK keluar.
- **Kalibrasi Sensor Level:** strip nikel non-contact + `touchRead()`, moving
  average window 8; titik 0% & 100% per tangki bisa disetel dari UI.
- **Kalibrasi Sensor Suhu:** 3 sensor DS18B20 pada satu bus 1-Wire (Motor /
  Komponen / Chamber), pilih alamat per blok + offset ±0.1 °C.
- **Kalibrasi Sensor Udara:** rekam acuan udara bersih & kotor MQ-135 (ppm).
- **Preset (maks 10):** nama, ukuran PCB, komposisi %HCl/%H2O2 (H2O dihitung
  otomatis), volume total, agitasi, speed agitasi, durasi total.
  CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 5 fase** (non-blocking): `DOSING → ETCHING → DRAINING → RINSING → SELESAI`.
  Saat ini perpindahan fase masih berbasis fraksi waktu preset — sedang
  diarahkan ke berbasis kondisi (lihat `PROCESS.md` §6).
- **Sensor level non-contact (5):** HCl/H2O2/H2O/sabun/limbah di GPIO
  11/12/13/14/3, konversi raw → persen.
- **Dosing stepper non-blocking:** 3 pompa peristaltik (TMC2209), konversi
  ml → langkah, dijalankan **paralel**.
- **Aktuator DC (PWM LEDC):** motor agitasi, pompa air sabun, pompa limbah, exhaust fan.
- **Penyimpanan NVS:** preset & kalibrasi sensor level via `Preferences`,
  satu blob + versi + CRC32.
- **Keamanan:** E-stop (GPIO 8), abort proses, semua aktuator mati saat
  batal/selesai. Watchdog, latch e-stop, dan halaman alarm belum ada.
- **UI:** header, list dengan animasi, toast, dialog, halaman proses dengan progress,
  kompensasi baris atas OLED yang rusak (`UI_Y_OFFSET` = 15 px).

## Struktur File

| File | Isi |
|---|---|
| `auto-etching-firmware.ino` | setup/loop utama, navigasi menu, handler semua halaman |
| `config.h` | Pin map, konstanta global, konfigurasi hardware, FW_VERSI |
| `input.h` | Debounce & pembacaan 6 tombol via PCF8574 |
| `data.h` | Struct Preset, enum halaman, state global, list menu |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | 5 sensor level (touchRead), DS18B20 ×3, MQ-135, filter moving average |
| `storage.h` | Preferences (NVS), CRC32, simpan/muat preset & kalibrasi level |
| `motor.h` | Stepper pompa peristaltik non-blocking (3 kanal, `pompaMulai`/`pompaSelesai`) |
| `dcmotor.h` | DC motor & pompa DC via TB6612FNG (agitasi, sabun, limbah, fan) |
| `buzzer.h` | Buzzer non-blocking (state machine berbasis millis) |
| `process.h` | State machine proses etching, jeda/batal/selesai |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi |
| `fontmini.h` | Font data untuk OLED |
| `dbg.h` | Debug helper (serial print per modul) |
| `sensor_extra.h` | ⚠️ **Kode mati** — tidak di-`#include` mana pun; berisi sisa implementasi
  kalibrasi pompa dari lineage lama. Seharusnya dihapus atau diaktifkan
  (`dosing-design.md` §2.2) |

## Pin Map

> **Penting:** pasangan pin stepper di bawah adalah pin map **TMC2209** yang
> benar. `config.h` **masih** memuat pin map DRV8825 lama — apply perubahan
> `PROCESS.md` §1.2 lebih dulu (`PROGRESS.md` §1).

### Sudah sesuai `config.h`

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz, bus bersama OLED + PCF8574 |
| OLED SSD1306 | 0x3C | 128x64 |
| PCF8574 (tombol) | 0x20 | 6 tombol |
| Sensor level | 11/12/13/14/3 | HCl/H2O2/H2O/sabun/limbah |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| PWM fan | 18 | TB6612FNG Driver2 PWB |
| Buzzer | 6 | pasif, LEDC |
| E-stop | 8 | INPUT_PULLUP |
| MQ-135 | 9 | analog 12-bit + divider |
| DS18B20 | 10 | 1-Wire, 3 sensor pada satu bus |
| LED strip | 7 | `#define` ada, belum dipakai kode |

### Stepper — target TMC2209 (belum di-apply ke `config.h`)

| Fungsi | GPIO | `config.h` sekarang (DRV8825) |
|---|---|---|
| STEP Pompa HCl | 38 | `PIN_SLP3` 38 |
| DIR Pompa HCl | 48 | `PIN_DIR1` 42 |
| STEP Pompa H2O2 | 40 | `PIN_STEP2` 40 ✅ |
| DIR Pompa H2O2 | 39 | `PIN_DIR2` 39 ✅ |
| STEP Pompa Air | 42 | `PIN_DIR1` 42 |
| DIR Pompa Air | 41 | `PIN_SLP2` 41 |
| TMC_EN (shared) | 1 | `PIN_SLP1` 1 |
| TMC_PDN (shared) | 2 | `PIN_STEP1` 2 |
| — | 21, 47 | `PIN_DIR3` 21, `PIN_STEP3` 47 → **jadi bebas** |

## Dokumentasi

| Dokumen | Isi |
|---|---|
| `PROCESS.md` | Rencana detail 5 modul proses + cara kerja + pin map TMC2209 |
| `PROGRESS.md` | Checklist status implementasi (79 ✅ / 18 ⚠️ / 98 ⬜) |
| `dosing-design.md` | Desain rinci dosing: matematika ml→langkah, kalibrasi gravimetrik, error budget |
| `etching-otomatis.md` | Plan awal v0.4.1 — **sebagian superseded** oleh `PROCESS.md` & `dosing-design.md` |
| `water-level-sensor-plan.md` | Rencana sensor level non-contact (strip nikel) |
| `GUIDE.md` | Panduan pemilik repo & contributor (fork/PR workflow) |
| `LOG.md` | Riwayat perubahan versi |

## Riwayat Versi

Lihat `LOG.md` untuk detail perubahan per versi. Versi firmware saat ini
`config.h:112` → **v0.5.4**.