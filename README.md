# Auto Etching Firmware

Firmware untuk alat etching PCB otomatis. Board: **ESP32-S3 DevKitC** —
OLED SSD1306 128×64 + 6 tombol (PCF8574), 3 pompa peristaltik (stepper
TMC2209), 4 pompa/motor DC (TB6612FNG), 5 sensor level non-contact,
1 sensor suhu (DS18B20), dan 1 sensor gas (MQ-135).

## Fitur Utama

- **Proses etching otomatis** — 5 fase: dosing → etching → draining → rinsing → selesai
- **Dosing 3 bahan** — HCl/H2O2/H2O lewat pompa peristaltik stepper, non-blocking
- **OLED UI** — menu, preset, monitoring level cairan, kalibrasi, layar proses
- **Sensor & kalibrasi** — level cairan (touch), suhu 3 blok, MQ-135, motor/pompa
- **Non-blocking** — seluruh loop berbasis `millis()`, tanpa `delay()` di jalur proses
- **E-Stop** — emergency stop yang mematikan semua aktuator
- **Preset** — maks 10 profil etching (komposisi, volume, agitasi, durasi)
- **Penyimpanan NVS** — `Preferences` dengan versi + CRC32 (bukan EEPROM)

> **Status:** prototype — jalur happy-path berfungsi, akurasi dosing dan
> proteksi proses (watchdog, latch e-stop) belum selesai. Rincian di
> `PROGRESS.md`.

## Struktur Project

| File | Deskripsi |
|------|-----------|
| `auto-etching-firmware.ino` | File utama firmware (setup, loop, navigasi halaman) |
| `config.h` | Pin map, konstanta global, versi firmware |
| `data.h` | Struct & enum, state global, daftar menu |
| `input.h` | Debounce & pembacaan 6 tombol via PCF8574 |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | Sensor level, DS18B20, MQ-135, filter |
| `storage.h` | Penyimpanan NVS (Preferences) + CRC32 |
| `motor.h` | Stepper pompa peristaltik (non-blocking) |
| `dcmotor.h` | Motor DC & pompa DC via TB6612FNG |
| `buzzer.h` | Buzzer non-blocking |
| `process.h` | State machine proses etching |
| `ui.h` | Tampilan OLED dan seluruh halaman |
| `fontmini.h` | Data font untuk OLED |
| `dbg.h` | Debug helper (log Serial per modul) |

> Catatan: `aktuator.h` yang disebut pada versi lama README sudah tidak ada —
> fungsinya terbagi ke `motor.h` (stepper), `dcmotor.h` (DC), dan `buzzer.h`.
> `sensor_extra.h` ada di repo tetapi belum dipakai (`#include` belum ada).

## Dokumentasi

| Dokumen | Isi |
|---|---|
| [`SUMMARY.md`](./SUMMARY.md) | Ringkasan fitur, struktur file, pin map |
| [`PROCESS.md`](./PROCESS.md) | Rencana detail 5 modul proses + pin map TMC2209 |
| [`PROGRESS.md`](./PROGRESS.md) | Checklist status implementasi |
| [`dosing-design.md`](./dosing-design.md) | Desain rinci modul dosing (kalibrasi, akurasi, error budget) |
| [`GUIDE.md`](./GUIDE.md) | Panduan pemilik repo & contributor (fork/PR workflow) |
| [`LOG.md`](./LOG.md) | Riwayat perubahan versi |
| [`etching-otomatis.md`](./etching-otomatis.md) | Plan awal v0.4.1 (sebagian superseded) |
| [`water-level-sensor-plan.md`](./water-level-sensor-plan.md) | Rencana sensor level non-contact |

## Build

- Board: **ESP32-S3 DevKitC**
- Framework: Arduino ESP32 (`arduino-esp32`), I2C 400 kHz
- Middleware: `Wire`, `Adafruit_SSD1306`, `OneWire`, `DallasTemperature`
- License: see repository metadata