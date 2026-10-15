# Plan Pengembangan: Mesin PCB Etching Otomatis

> ⚠️ **Dokumen plan awal v0.4.1 — sebagian besar sudah superseded.**
>
> | Bagian | Status | Digantikan oleh |
> |---|---|---|
> | §1 Alur Proses, §3 Perubahan Firmware | masih sesuai | `PROCESS.md` |
> | §2 Konfigurasi Pin | ❌ **basi** — masih DRV8825 (SLP) & asumsi 1/36 | `PROCESS.md` §1.2 (pin map TMC2209) |
> | §4 Kalibrasi Dosing | ❌ **basi** — asumsi 7200 step/rev, target ±5% tanpa error budget | `dosing-design.md` |
> | §6 Pertanyaan Terbuka | sebagian terjawab | `PROCESS.md` §12 + `dosing-design.md` §11 |
>
> Angka "mikrostep 1/36 → 7200 step/putaran" di bawah **tidak mungkin**
> terjadi: DRV8825/TMC2209 maksimum 1/32. Jangan dipakai sebagai acuan desain.

Tanggal: 2026-10-08
Target: melengkapi firmware `Etching` (v0.4.1) menjadi alat etching PCB
otomatis: dosing HCl/H2O2/H2O akurat (stepper + DRV8825, 1/36 step), agitasi
(TB6612FNG), draining (pompa limbah), rinsing.

## 1. Alur Proses

```
START (tombol ENTER di layar proses, OLED 6-tombol yang sudah ada)
  → DOSING   : DRV8825 ×3 memompa HCl, H2O2, H2O sesuai ml preset
  → AGITASI  : motor agitasi via TB6612FNG driver1-PWA (GPIO15)
               selama agitasiDetik pada agitasiSpeed (1-5)
  → DRAINING : pompa limbah via TB6612FNG driver2-PWA (GPIO17),
               sampai tangki proses kosong / timeout / limbah penuh
  → RINSING  : dosing H2O (+ opsional air sabun via driver1-PWB GPIO16),
               agitasi singkat, drain ulang
  → SELESAI  : buzzer + layar selesai
```

## 2. Konfigurasi Pin (final)

### Bus I2C — SDA=4, SCL=5, 400 kHz
| Alamat | Perangkat |
|---|---|
| 0x3C | OLED SSD1306 |
| 0x20 | PCF8574 (6 tombol) |

### Stepper DRV8825 ×3 (pompa peristaltik, mikrostep 1/36)
| Pompa | SLP | STEP | DIR |
|---|---|---|---|
| HCl | 1 | 2 | 42 |
| H2O2 | 41 | 40 | 39 |
| H2O | 38 | 47 | 21 |

### Sensor level (GPIO langsung, ganti MPR121)
| GPIO | Cairan |
|---|---|
| 11 | HCl |
| 12 | H2O2 |
| 13 | H2O |
| 14 | Air sabun |
| 3 | Air limbah |

### TB6612FNG ×2 (1 arah: INA→3.3V, INB→GND, STBY ditentukan)
| Sinyal | GPIO | Beban |
|---|---|---|
| Driver1 PWA | 15 | Motor agitasi |
| Driver1 PWB | 16 | Pompa air sabun |
| Driver2 PWA | 17 | Pompa limbah (drain) |
| Driver2 PWB | 18 | Exhaust fan |

### Lain-lain
| Fungsi | GPIO |
|---|---|
| Buzzer | 6 |
| LED strip | 7 |
| E-stop sense | 8 |
| MQ135 (analog) | 9 |
| DS18B20 (1-Wire) | 10 |

Fan GPIO14 lama dihapus → masuk driver2-PWB (GPIO18). MPR121 (0x5A) tidak dipakai.

## 3. Perubahan Firmware

### Tahap A — Konfigurasi & sensor
1. `config.h`: hapus MPR121 & pin fan lama; tambah pin TB6612FNG (15/16/17/18),
   pin sensor level (11/12/13/14/3), MS1/MS2/MS3 DRV8825 bila ada, STBY TB6612FNG.
2. `sensor.h`: tulis ulang — baca 5 sensor level langsung dari GPIO
   (digital HIGH/LOW atau analog sesuai jenis sensor), rata-rata + histeresis
   seperti perilaku lama.
3. `data.h`: sesuaikan `namaSensor` bila perlu (tetap 5: HCl, H2O2, H2O, Sabun, Limbah).

### Tahap B — Driver aktuator (file baru)
4. `motor.h`: driver stepper non-blocking (STEP/DIR/SLP), `mlToSteps`
   (stepsPerMl per pompa, hasil kalibrasi), update di `loop()`.
5. `dcmotor.h`: helper PWM untuk TB6612FNG (set speed 0-255 per PWx, 1 arah).
6. Uji manual tiap pompa via Serial.

### Tahap C — Proses nyata (ganti simulasi)
7. `process.h`: ubah fase:
   - `FASE_DOSING`: jalankan pompa HCl/H2O2/H2O sesuai ml preset; validasi level
     sumber via sensor; selesai → FASE_ETCHING
   - `FASE_ETCHING`: agitasi `agitasiDetik` @ `agitasiSpeed`
   - `FASE_DRAINING`: pompa limbah jalan; selesai saat level proses kosong /
     timeout / limbah penuh
   - `FASE_RINSING`: dosing H2O (+opsional sabun) + agitasi singkat + drain
   - `FASE_SELESAI`: buzzer, layar selesai
   - Tetap non-blocking & terpisah dari render (`prosesUpdate()` di `loop()`).

### Tahap D — UI
8. `ui.h`/`Etching.ino`: layar proses tampilkan fase + progress + ml terpompa.
9. Halaman "Kalibrasi" (placeholder): wizard kalibrasi steps/ml per pompa
   (pompa N detik → ukur ml → simpan), serta offset ambang sensor.
10. Toast & log Serial untuk error tiap fase.

### Tahap E — Keamanan & penyimpanan
11. E-stop (GPIO8): hentikan semua motor, abort proses, tampilkan peringatan.
12. Watchdog tiap fase (timeout DOSING/DRAINING), error → buzzer + toast.
13. `storage.h`: simpan parameter kalibrasi (stepsPerMl, ambang) ke NVS,
    naikkan `FORMAT_VERSI`.

## 4. Kalibrasi Dosing

> ⚠️ **Bagian ini digantikan oleh `dosing-design.md` §3 & §6.**
> Teks di bawah hanya menyimpan sisa rencana awal.

- Mikrostep 1/36 → 200 × 36 = 7200 step/putaran. ❌ Tidak mungkin (maks 1/32);
  asumsi ini masih terbawa di `motor.h` sampai hari ini.
- `stepsPerMl` per pompa diukur empiris; uji dengan air (tanpa HCl) dulu.
- Target akurasi ±5%; pantau level sumber via sensor GPIO.
- Prosedur gravimetrik (timbangan), auto-stop, wizard di UI, dan error budget
  lengkap ada di `dosing-design.md`.

## 5. Urutan Implementasi

1. Update `config.h` (pin baru)
2. Tulis ulang `sensor.h` (GPIO langsung)
3. `motor.h` + uji 1 pompa
4. `dcmotor.h` + uji agitasi/drain/fan
5. `process.h` fase nyata end-to-end
6. Kalibrasi steps/ml + halaman Kalibrasi
7. E-stop, timeout, error handling
8. Uji penuh dengan air, lalu larutan encer

## 6. Pertanyaan Terbuka

- Jenis sensor level (float switch digital vs analog)? Menentukan logika baca di `sensor.h`.
- STBY TB6612FNG: di-tie 3.3V atau ke GPIO?
- Pin MS1/MS2/MS3 DRV8825 untuk 1/36 — sudah di-hardwire atau perlu GPIO?
- RINSING: pakai pompa air (H2O) saja, atau juga air sabun?
