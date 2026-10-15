# PROCESS — Rencana Detail Proses Mesin Etching Otomatis

Firmware: **Etching v0.5.0** · Board: **ESP32-S3 DevKitC**
Dokumen ini adalah spesifikasi implementasi proses mesin etching otomatis
(UI, Dosing, Etching, Draining & Rinsing, Emergency Stop).

Status implementasi terdokumentasi di `PROGRESS.md`.

---

## 0. Keputusan Desain (sudah disepakati)

| Topik | Keputusan |
|---|---|
| Pembagian modul | **Satu file `process.h`**, 5 section berurutan |
| Kriteria selesai DRAINING | **Fixed time** (konstanta ms); sensor level hanya sebagai pengaman overfill |
| Pompa dosing (3 stepper) | **Paralel** |
| Selesai fase | berbasis **kondisi** (pompa selesai), waktu hanya **watchdog** |

---

## 1. Tabel Konfigurasi GPIO ESP32-S3

### 1.1 Bus I2C & UI

| No. | GPIO | Connected to | Configured? |
|---|---|---|---|
| 1 | 4 | I2C SDA — OLED SSD1306 (0x3C) + PCF8574 (0x20) | ✅ Sudah |
| 2 | 5 | I2C SCL — OLED SSD1306 (0x3C) + PCF8574 (0x20) | ✅ Sudah |

### 1.2 Motor Stepper Pompa Peristaltik (TMC2209 ×3) — **PIN MAP DIPERBAIKI**

> **Perubahan hardware (terjadi setelah firmware v0.5.0 ditulis).**
> Driver pompa stepper diganti dari **DRV8825** menjadi **TMC2209**, dan
> pasangan pin setiap channel berubah. Pin `SLP` (sleep) tidak lagi dipakai;
> digantikan oleh `EN` (enable driver) dan `PDN` (power-down) yang dikendalikan
> software. Mikrostep juga diatur lewat UART resistor, bukan jumper MS1/MS2/MS3.

| No. | GPIO | Connected to | Configured? |
|---|---|---|---|
| 3 | 38 | **STEP Pompa HCl** (TMC2209 channel 1) | ⬜ Belum — `config.h` masih `PIN_SLP3` |
| 4 | 48 | **DIR Pompa HCl** (TMC2209 channel 1) | ⬜ Belum — `config.h` masih `PIN_DIR1` |
| 5 | 39 | **DIR Pompa H2O2** (TMC2209 channel 2) | ✅ Sudah (koheren: `PIN_DIR2`) |
| 6 | 40 | **STEP Pompa H2O2** (TMC2209 channel 2) | ✅ Sudah (koheren: `PIN_STEP2`) |
| 7 | 41 | **DIR Pompa Air / H2O** (TMC2209 channel 3) | ⬜ Belum — `config.h` masih `PIN_SLP2` |
| 8 | 42 | **STEP Pompa Air / H2O** (TMC2209 channel 3) | ⬜ Belum — `config.h` masih `PIN_DIR1` |
| 9 | 1 | **TMC_EN** — enable seluruh driver TMC2209 | ⬜ Belum — `config.h` masih `PIN_SLP1` |
| 10 | 2 | **TMC PDN** — power-down / reset seluruh driver | ⬜ Belum — `config.h` masih `PIN_STEP1` |

**Catatan penting pin map baru:**

1. **GPIO 1 & 2 dipakai bersama** (common) untuk ketiga channel TMC2209, bukan
   per-channel. Jadi hanya ada 1 pasang `EN`/`PDN` untuk 3 driver.
   → *Perlu konfirmasi konfigurasi fisik PCB*: apakah `EN`/`PDN` benar-benar
   di-OR-kan antar 3 driver, atau hanya driver #1 yang dikendalikan?
2. **GPIO 21 dan GPIO 47 menjadi bebas** (semula `DIR3` dan `STEP3`) — bisa
   dipakai untuk sensor level tambahan (mis. tangki proses) atau LED status.
3. **GPIO 48 sebelumnya direncanakan untuk WS2812** pada
   `water-level-sensor-plan.md`. Sekarang dipakai sebagai `DIR Pompa HCl`
   → **konflik LED strip sudah terselesaikan**; bila LED status masih
   dibutuhkan, pindahkan ke GPIO 21 atau 47.
4. **GPIO 7 (`PIN_LED_STRIP`) masih didefinisikan di `config.h` tetapi belum
   dipakai kode apa pun.** Dibiarkan sebagai status/indicator (opsional).
5. Mikrostep TMC2209 diatur via **UART** (resistor pull-up ke VCC pada pin MS1/MS2
   atau jumper), sehingga **tidak butuh GPIO MS1/MS2/MS3** seperti DRV8825.
   Konfirmasi mode mikrostep yang dipakai (mis. 1/16 = 3200 step/rev).
6. Dengan TMC2209, `langkahPerMl` harus dihitung ulang dari step/rev yang
   sebenarnya (bukan asumsi 7200 step/rev DRV8825 1/36).

**Nilai `#define` yang perlu diubah di `config.h`:**

```cpp
// LAMA (v0.5.0 - DRV8825)          BARU (TMC2209)
#define PIN_SLP1  1                   #define PIN_TMC_EN   1
#define PIN_STEP1 2                   #define PIN_TMC_PDN  2
#define PIN_DIR1  42                  #define PIN_STEP1   38
#define PIN_SLP2  41                  #define PIN_DIR1    48
#define PIN_STEP2 40                  #define PIN_STEP2   40
#define PIN_DIR2  39                  #define PIN_DIR2    39
#define PIN_SLP3  38                  #define PIN_STEP3   42
#define PIN_STEP3 47                  #define PIN_DIR3    41
#define PIN_DIR3  21                  // (SLP1/SLP2/SLP3 dihapus)
```

---

### 1.3 Sensor Level Cairan (5 buah, touchRead non-contact)

Strip nikel di luar jerican HDPE + kabel AWG30/32 → pin touch ESP32-S3.

| No. | GPIO | Connected to | Configured? |
|---|---|---|---|
| 11 | 11 | Sensor Level HCl | ✅ Sudah |
| 12 | 12 | Sensor Level H2O2 | ✅ Sudah |
| 13 | 13 | Sensor Level H2O | ✅ Sudah |
| 14 | 14 | Sensor Level Air Sabun | ✅ Sudah |
| 15 | 3 | Sensor Level Air Limbah | ✅ Sudah |

### 1.4 Motor DC / Pompa DC (TB6612FNG ×2, 1 arah, PWM)

| No. | GPIO | Connected to | Configured? |
|---|---|---|---|
| 16 | 15 | TB6612FNG #1 PWA — Motor Agitasi (PWM) | ✅ Sudah |
| 17 | 16 | TB6612FNG #1 PWB — Pompa Air Sabun (PWM) | ✅ Sudah (hanya nyala saat kalibrasi) |
| 18 | 17 | TB6612FNG #2 PWA — Pompa Limbah / drain (PWM) | ✅ Sudah |
| 19 | 18 | TB6612FNG #2 PWB — Exhaust Fan (PWM) | ✅ Sudah (hanya nyala saat kalibrasi) |

### 1.5 Lain-lain

| No. | GPIO | Connected to | Configured? |
|---|---|---|---|
| 20 | 6 | Buzzer pasif (LEDC tone) | ✅ Sudah |
| 21 | 7 | LED strip / indicator status | ⬜ Belum — `#define` saja, tidak dipakai kode |
| 22 | 8 | E-stop sense (`INPUT_PULLUP`, LOW = stop) | ✅ Sudah (non-latching, lihat Section 1) |
| 23 | 9 | MQ-135 sensor udara (ADC 12-bit + divider R1=10k/R2=15k) | ✅ Sudah (hanya halaman Kalibrasi Udara) |
| 24 | 10 | DS18B20 1-Wire (3 sensor: Motor / Komponen / Chamber) | ✅ Sudah (hanya halaman Kalibrasi Suhu) |

### 1.6 GPIO yang tidak dipakai firmware

| GPIO | Keterangan |
|---|---|
| 19, 20 | USB native ESP32-S3 |
| 43, 44 | UART debug |
| 0 | Button boot |
| 21, 47 | **Bebas** (dari pin map lama) |
| 16–37, 45, 46 | Bebas / analog |
| 48 | Sekarang dipakai `DIR Pompa HCl` |

---

## 2. Arsitektur Proses

```
                 ┌──────────────────────────────────────────────┐
                 │ SECTION 5 — ORCHESTRATOR (process.h)          │
                 │  timing, dispatch fase, jeda, watchdog       │
                 └───┬─────────┬──────────┬──────────┬───────────┘
                     │         │          │          │
      ┌──────────────┘         │          │          └────────────────┐
      ▼                        ▼          ▼                           ▼
┌────────────────┐  ┌────────────────┐  ┌──────────────────┐  ┌────────────────────┐
│ SECTION 2      │  │ SECTION 3      │  │ SECTION 4        │  │ SECTION 1          │
│ DOSING         │  │ ETCHING        │  │ DRAINING &       │  │ EMERGENCY STOP     │
│ (paralel)      │  │ (siklus agitasi│  │ RINSING          │  │ (latching + guard) │
│                │  │  + proteksi)   │  │ (fixed time)     │  │                    │
└────────────────┘  └────────────────┘  └──────────────────┘  └────────────────────┘
                                    ▲
                                    │ hanya membaca state
                              ┌─────┴──────┐
                              │  ui.h      │  halaman proses + alarm
                              └────────────┘
```

Section 1 (Emergency) diletakkan **paling atas** file karena
`prosesGagal()` dipanggil dari semua section lain.

---

## 3. State Bersama (perubahan `data.h`)

```cpp
enum FaseProses {
  FASE_IDLE, FASE_CEK, FASE_DOSING, FASE_ETCHING,
  FASE_DRAINING, FASE_RINSE_AIR, FASE_RINSE_SABUN,
  FASE_DRAIN_AKHIR, FASE_SELESAI, FASE_GAGAL
};

enum KodeError {
  ERR_NONE, ERR_ESTOP, ERR_LEVEL_KURANG, ERR_TIMEOUT_DOSING,
  ERR_STEP_MACET, ERR_OVERFLOW, ERR_SUHU, ERR_SENSOR
};

const char* namaFase[] = {
  "IDLE", "CEK", "DOSING", "ETCHING", "DRAINING",
  "BILAS AIR", "BILAS SABUN", "DRAIN AKHIR", "SELESAI", "GAGAL"
};

const char* pesanError[] = {
  "-", "EMERGENCY STOP", "Cairan kurang", "Dosing timeout",
  "Pompa macet", "Tangki limbah penuh", "Suhu terlalu tinggi",
  "Sensor tidak terbaca"
};

FaseProses prosesFase        = FASE_IDLE;
KodeError  prosesError       = ERR_NONE;
bool       estopAktif        = false;   // latch, reset manual
unsigned long faseMulaiMs   = 0;       // reset tiap transisi
unsigned long tDosingMs = 0, tEtchingMs = 0;  // budget fase berjalan
int        prosesSiklus      = 0;       // siklus agitasi ke berapa
int        totalMlTerpompa   = 0;       // ringkasan
```

enum `Halaman` ditambah: `HAL_ALARM`.

---

## 4. Konstanta Baru (`config.h`)

```cpp
// ===== DOSING (paralel) =====
#define DOSING_PARALEL        1
#define TIMEOUT_DOSING_MS     90000UL   // watchdog total fase dosing
#define DETEK_MACET_MS        3000UL    // langkah tak bertambah = macet
#define STEP_INTERVAL_US      1200      // jeda pulsa (8 kHz, aman 3 pompa)

// ===== ETCHING =====
#define SUHU_MAKS_KOMPONEN    70.0f     // °C → turunkan speed agitasi
#define SUHU_MAKS_STOP        85.0f     // °C → gagal
#define GAS_AMANG_PPM         400       // MQ-135 → badge warning
#define FAN_SELAMA_ETCHING    1         // 1 = fan nyala di fase etching

// ===== DRAINING & RINSING (fixed time) =====
#define T_DRAINING_MS         45000UL
#define T_RINSE_AIR_MS        30000UL
#define T_RINSE_SABUN_MS      10000UL
#define T_DRAIN_AKHIR_MS      15000UL
#define LOKASI_LIMBAH_MAKS    95        // % → guard overfill
#define ML_BILASAN            200       // ml air bilasan (konstanta, sementara)
#define SPEED_AGITASI_BILAS   2         // speed agitasi saat bilas

// ===== Volume tangki (untuk cek ketersediaan cairan) =====
#define VOL_TANGKI_HCL    1000   // ml
#define VOL_TANGKI_H2O2   1000   // ml
#define VOL_TANGKI_H2O    2000   // ml
#define VOL_TANGKI_SABUN  1000   // ml
#define VOL_TANGKI_LIMBAH 5000   // ml
```

Semua konstanta di atas **dapat dituning tanpa menyentuh logika**.

---

## 5. SECTION 1 — EMERGENCY STOP

### 5.1 Task list

| No | Task | File | Detail |
|---|---|---|---|
| 1.1 | `semuaAktuatorMati()` | `process.h` | agitasi 0, `pompaSabunSet(false)`, `pompaLimbahSet(false)`, `fanSet(false)`, `pompaStopAll()`, lalu `digitalWrite(TMC_EN/PIN_SLP*, LOW)` → driver benar-benar tidur, bukan hanya tanpa pulsa |
| 1.2 | `prosesGagal(kode)` | `process.h` | **satu-satunya** jalan keluar kondisi abnormal → `semuaAktuatorMati()`, `buzzerAlarm()`, `prosesError=kode`, `prosesFase=FASE_GAGAL`, `pindahHalaman(HAL_ALARM)`, log Serial (waktu + fase + kode) |
| 1.3 | Estop di baris pertama `loop()` | `.ino` | `if (estopAktif \|\| digitalRead(PIN_ESTOP)==LOW) { latch; prosesGagal(ERR_ESTOP); continue; }` — placed **sebelum** `tanganiTombol()` agar tombol tidak bisa membatalkan stop |
| 1.4 | Latch + reset | `process.h`, `.ino` | `estopAktif` hanya dilepas lewat BACK di `HAL_ALAM`, dan hanya bila `digitalRead(PIN_ESTOP)==HIGH`. Tombol `PIN_ESTOP` LOW → tombol BACK diabaikan |
| 1.5 | `buzzerAlarm()` / `buzzerAlarmStop()` | `buzzer.h` | nada panjang berulang berbeda dari nada "selesai" |
| 1.6 | Jeda ikut mematikan output | SECTION 5 | saat `prosesJeda`, semua output off (bukan hanya timer berhenti) |

### 5.2 Cara kerja

```
loop():
 ① if (estopAktif || digitalRead(PIN_ESTOP) == LOW)      // PALING ATAS
       if (!estopAktif) { estopAktif = true; prosesGagal(ERR_ESTOP); }
       semuaAktuatorMati();
       buzzerAlarmStop();
       delay(1);
       continue;                    // skip tombol, proses, motor, sensor, render
 ② tombol, prosesUpdate, motorUpdate, sensorUpdate, buzzerUpdate, render
```

- Prioritas absolut: stop berlaku dalam **< 1 siklus loop (~2 ms)**.
- Bersifat **latching**: pin kembali HIGH tidak otomatis melepas; wajib reset
  manual → mencegah proses restart tak sengaja dengan tangan masih di tombol.
- `semuaAktuatorMati()` menjadi **satu fungsi tunggal**; semaphore ini menghapus
  4 baris kode stop yang saat ini terduplikasi di 3 tempat
  (`process.h:47-53`, `process.h:111-115`, `.ino:639-643`).

---

## 6. SECTION 2 — DOSING

### 6.1 Task list

| No | Task | File | Detail |
|---|---|---|---|
| 2.1 | `langkahPerMl` dari kalibrasi | `motor.h`, `process.h` | hapus hardcode `{1200,1200,1200}`; `langkahPerMl[i] = stepPerRev / kalibStepperMl[i]`. `kalibStepperMl[]` saat ini **hanya diubah di UI dan tidak pernah dipakai** → bug akurasi dosing |
| 2.2 | Simpan ke NVS | `storage.h` | key baru `pompa` (3 × float ml/rotasi), naikkan `FORMAT_VERSI_KALIB` 1 → 2 |
| 2.3 | `FASE_CEK` pre-flight | `process.h` | cek semua 5 sensor terbaca (persen 0–100 valid) + volume tangki > 0 → gagal `ERR_SENSOR` bila tidak |
| 2.4 | Cek ketersediaan cairan | `process.h` | `butuh_i = max(0, mlTarget_i − tersedia_i)` dengan `tersedia_i = persenSensor_i × VOL_TANGKI_i / 100`. Kekurangan → `ERR_LEVEL_KURANG` **sebelum** pompa nyala |
| 2.5 | Pompa paralel | `process.h` | `pompaMulai(0..2, butuh_i)` bersamaan; `STEP_INTERVAL_US` 400 → **1200 µs** agar 3 pompa tidak step-loss |
| 2.6 | Selesai berbasis kondisi | `process.h` | `pompaSelesai(0) && pompaSelesai(1) && pompaSelesai(2)` → `FASE_ETCHING` |
| 2.7 | Deteksi macet | `process.h` | `pompaJalan[i] == true` tapi `jalanLangkah[i]` tidak bertambah > 3 s → `ERR_STEP_MACET` |
| 2.8 | Watchdog | `process.h` | `t > TIMEOUT_DOSING_MS` → `ERR_TIMEOUT_DOSING` |
| 2.9 | `pompaProgres(i)` | `motor.h` | ekspor `jalanLangkah[i] / targetLangkah[i]` untuk UI |
| 2.10 | Sesuaikan `motorInit()` | `motor.h` | `PIN_SLP*` → `PIN_TMC_EN` / `PIN_TMC_PDN`; bila `EN`/`PDN` shared oleh 3 driver, cukup ditulis sekali |

### 6.2 Cara kerja

```
Mulai → FASE_CEK
  ├─ sensor tidak valid            → prosesGagal(ERR_SENSOR)
  ├─ volume tangki = 0             → prosesGagal(ERR_SENSOR)
  └─ OK → FASE_DOSING
        for i in 0..2:
            tersedia_i = sensorPersen[i] × VOL_TANGKI_i / 100
            butuh_i    = max(0, mlTarget_i − tersedia_i)
            if (butuh_i > tersedia_i) → prosesGagal(ERR_LEVEL_KURANG)
        pompaMulai(0, butuh_0); pompaMulai(1, butuh_1); pompaMulai(2, butuh_2)
        loop:  motorUpdate() menghasilkan pulsa STEP tiap 1200 µs per pompa
        selesai?  semua pompaSelesai()        → FASE_ETCHING
        macet?    langkah flat > 3 s           → prosesGagal(ERR_STEP_MACET)
        watchdog? t > 90 s                     → prosesGagal(ERR_TIMEOUT_DOSING)
```

Yang berubah dari versi sekarang: ml yang dipompa **dikurangi sisa cairan di
tangki** (dari sensor), durasi pompa dari kalibrasi nyata, dan ada 3 lapis
pengaman (ketersediaan, macet, watchdog).

---

## 7. SECTION 3 — ETCHING

### 7.1 Task list

| No | Task | File | Detail |
|---|---|---|---|
| 3.1 | Siklus agitasi periodik | `process.h` | pakai `agitasiDetik`: agitasi ON `agitasiDetik` s → OFF `agitasiDetik` s → ulangi. Field ini sekarang **hanya tampil di UI dan tidak dipakai proses** |
| 3.2 | Exhaust fan otomatis | `process.h` | fan ON selama ETCHING (asap HCl + panas) dan selama RINSE_SABUN. Sekarang `fanSet()` hanya dipanggil `false` |
| 3.3 | Duty + ramp-up | `process.h`, `dcmotor.h` | speed 1–5 → PWM 80–255 (sudah ada) + ramp-up 1 s supaya motor tidak "shock" |
| 3.4 | Proteksi suhu | `process.h`, `sensor.h` | `sensorSuhu(1) > 70 °C` → turunkan speed agitasi; `> 85 °C` → `ERR_SUHU` |
| 3.5 | Proteksi gas | `process.h`, `sensor.h` | `mq135PPM() > GAS_AMANG_PPM` → badge warning di UI (belum auto-stop) |
| 3.6 | Indikator siklus | `process.h` | `prosesSiklus` ditampilkan di layar proses |
| 3.7 | Budget waktu | SECTION 5 | `tEtching = tEtchingPreset` (satu-satunya fase yang memang time-based) |

### 7.2 Cara kerja

```
FASE_ETCHING   (budget = tEtchingPreset, ±70% durasi total)
  t = 0; prosesSiklus = 0; fan = ON
  loop():
    faseSiklus = (t / (2 × agitasiDetik)) % 2          // 0 = agitasi, 1 = diam
    agitasiSet(faseSiklus == 0 ? speedPreset : 0)
    suhu = sensorSuhu(1)                               // Komponen
      suhu > 70 → turunkan 1 tingkat speed
      suhu > 85 → prosesGagal(ERR_SUHU)
    mq135PPM() > 400 → set badgeWarning = true (UI)
    t ≥ tEtching → FASE_DRAINING
```

Fase terpanjang. Agitasi berhenti-mulai berkala supaya etsa tidak terlalu
panas dan cairan tetap mengalir; fan menyala terus sebagai exhaust.

---

## 8. SECTION 4 — DRAINING & RINSING (fixed time)

### 8.1 Task list

| No | Task | File | Detail |
|---|---|---|---|
| 4.1 | Draining | `process.h` | pompa Limbah ON, agitasi OFF, fan OFF → selesai setelah `T_DRAINING_MS` (45 s) |
| 4.2 | Guard overfill | `process.h` | `sensorPersen[LIMBAH] > LOKASI_LIMBAH_MAKS` (95%) → stop pompa + `ERR_OVERFLOW` |
| 4.3 | Rinse air | `process.h` | `pompaMulai(2, ML_BILASAN)` + agitasi speed 2 → selesai bila `pompaSelesai(2)` **atau** `T_RINSE_AIR_MS` |
| 4.4 | Rinse soap | `process.h` | `pompaSabunSet(true)` + agitasi + fan ON → `T_RINSE_SABUN_MS` (10 s). Pompa air sabun **belum pernah dipakai di proses** |
| 4.5 | Drain akhir | `process.h` | pompa Limbah ON → `T_DRAIN_AKHIR_MS` (15 s) |
| 4.6 | Ringkasan hasil | `process.h`, `ui.h` | total ml terpompa + durasi aktual per fase → layar selesai |
| 4.7 | Kalibrasi durasi | `storage.h` | (opsional tahap lanjut) simpan konstanta t drain/rinse agar bisa diubah dari UI |

### 8.2 Cara kerja

```
FASE_DRAINING     pompa Limbah ON · agitasi OFF · fan OFF
   selesai?  t ≥ 45 s                        → FASE_RINSE_AIR
   overfill? sensorPersen[LIMBAH] > 95%      → prosesGagal(ERR_OVERFLOW)

FASE_RINSE_AIR    pompaMulai(2, 200 ml air) · agitasi speed 2
   selesai?  pompaSelesai(2) ATAU t ≥ 30 s   → FASE_RINSE_SABUN

FASE_RINSE_SABUN  pompaSabun ON · agitasi speed 2 · fan ON
   selesai?  t ≥ 10 s                        → FASE_DRAIN_AKHIR

FASE_DRAIN_AKHIR  pompa Limbah ON
   selesai?  t ≥ 15 s                        → FASE_SELESAI

FASE_SELESAI      semua aktuator OFF · buzzer "done" · tampilkan ringkasan
                  ENTER/BACK → kembali ke Preset Detail
```

Sensor level dipakai sebagai **pengaman overfill**, bukan penentu waktu —
sesuai keputusan "fixed time".

---

## 9. SECTION 5 — ORCHESTRATOR

| No | Task | Detail |
|---|---|---|
| 5.1 | `prosesMulai()` | validasi preset → hitung `tEtchingPreset` → reset semua counter (`prosesAkumMs`, `prosesSiklus`, `totalMlTerpompa`) → `FASE_CEK`, bukan langsung `FASE_DOSING` |
| 5.2 | `prosesUpdate()` | akumulasi waktu (tidak ikut jeda) → `faseMulaiMs` di-reset tiap transisi → `dispatchFase()` |
| 5.3 | `dispatchFase()` | satu `switch` yang memanggil `dosisMasuk/dosisUpdate`, `etchingUpdate`, `drainUpdate` sesuai fase |
| 5.4 | `prosesToggleJeda()` | jeda **tahan semua output**, bukan hanya menghentikan timer |
| 5.5 | `prosesBatal()` | cukup memanggil `semuaAktuatorMati()` — hapus 4 baris stop duplikat |
| 5.6 | `prosesKeluarSelesai()` | reset `FASE_IDLE`, `prosesError = ERR_NONE` |
| 5.7 | Buzzer per transisi | nada pendek beda tiap masuk fase baru (bukan hanya mulai/selesai) |

---

## 10. Perubahan di Luar `process.h`

| File | Task |
|---|---|
| `config.h` | pin map TMC2209 (Section 1.2) + 15 konstanta baru (Section 4) |
| `data.h` | enum `FaseProses`, `KodeError`, `namaFase`, `pesanError`, variabel state baru, `HAL_ALARM` |
| `motor.h` | pin EN/PDN, `langkahPerMl` dari kalibrasi, `STEP_INTERVAL_US` 1200 µs, `pompaProgres()` |
| `dcmotor.h` | `stepperSleep(bool)`, `fanOn()/fanOff()` terpisah dari `fanSet(bool)` |
| `buzzer.h` | `N_ALARM`, `buzzerAlarm()`, `buzzerAlarmStop()` |
| `storage.h` | key `pompa`, naikkan `FORMAT_VERSI_KALIB` ke 2 |
| `ui.h` | `gambarProses()` per-fase + ml terpompa + ikon aktuator + badge warning, `gambarAlarm()` baru, badge E-STOP di header |
| `.ino` | estop ke baris pertama `loop()`; BACK di `HAL_ALARM` → reset latch |

---

## 11. Urutan Implementasi & Uji

> **Seluruh pengujian wajib memakai air biasa dulu**, baru larutan encer.
> HCl pekat berbahaya dan korosif.

| Urutan | Bagian | Kriteria lulus |
|---|---|---|
| 1 | SECTION 1 — Estop | Tarik GND estop saat pompa jalan → semua output mati < 5 ms; latch aktif; BACK tidak melepas saat pin masih LOW |
| 2 | SECTION 2 — Dosing | Timbang air, 3 pompa × 3 ulangan, error ≤ 5% dari target |
| 3 | SECTION 4 — Drain/Rinse | Air larutan terbuang habis dalam `T_DRAINING_MS`; tidak overfill; busa sabutan terlihat |
| 4 | SECTION 3 — Etching | Agitasi siklus ON/OFF benar, fan menyala, tidak overheat pada siklus penuh |
| 5 | SECTION 5 + UI | Layar proses konsisten dengan log Serial pada semua transisi; semua alarm bisa di-reset |
| 6 | Ringkasan | Checklist `PROGRESS.md` diperbarui, tabel GPIO sesuai pin map TMC2209 |

---

## 12. Pertanyaan Terbuka

1. Apakah `TMC_EN` / `TMC_PDN` (GPIO 1 & 2) **shared** oleh 3 channel, atau
   hanya untuk driver #1? → menentukan apakah `motorInit()` menulis 1 atau 3 pin.
2. Mikrostep TMC2209 yang dipakai berapa (step/rev)?
3. Kapasitas tiap tangki (jerican) berapa ml → untuk `VOL_TANGKI_*`.
4. Apakah perlu sensor level tambahan di **tangki proses**? (GPIO 21/47 bebas)
5. Batas suhu aman chamber & komponen yang sesuai dengan material PCB.

---

*Dokumen ini disusun bersama `PROGRESS.md` (checklist status implementasi).*