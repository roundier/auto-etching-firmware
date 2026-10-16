# KONTRAK BAB 0 — Batas Pisah Tugas Orang A ↔ Orang B

Status: **menunggu persetujuan B** · Dibuat: 2026-10-16 · Untuk firmware **v0.5.4**

Dokumen ini mengunci antarmuka (interface) antara dua orang yang mengerjakan
firmware ini bersamaan. **Tidak ada kode yang boleh ditulis sebelum blok-blok
di bawah ini agreed**, karena meny disentuh keduanya secara bersamaan
menyebabkan konflik merge.

Referensi desain: `dosing-design.md` (Bab 3), `PROCESS.md` (Bab 3–6),
`PROGRESS.md` §3–§6.

---

## 0. Pembagian File (final)

| Pemilik | File |
|---|---|
| **Orang B** | `ui.h` · `auto-etching-firmware.ino` · `input.h` · `data.h` · `sensor.h` · `sensor_extra.h` · `storage.h` · `config.h` · `presetops.h` |
| **Orang A** | `process.h` · `motor.h` · `dcmotor.h` · `buzzer.h` |

**Aturan main**

1. **Orang A tidak pernah mengedit file B**, dan sebaliknya. Kalau butuh
   perubahan di file orang lain → tulis spesifikasi + blok kode siap tempel di
   dokumen ini, lalu minta yang pemilik menempelkannya.
2. **Add-only di Gelombang 1.** Fungsi yang sudah dipakai file lain tidak boleh
   di-rename, diubah tanda tangannya, atau dihapus (lihat §5).
3. **Satu file, satu pemilik, satu waktu.** PR ditutup sebelum PR berikutnya
   dibuka.
4. Urutan `loop()` dikunci di §3 — jangan diubah tanpa koordinasi.

---

## 1. Blok 1 — `config.h` (dipakai B)

### 1a. Ganti pin map stepper (`config.h:84-94`)

Hapus:

```cpp
// ===== PIN MAP (ESP32-S3) - dipakai mulai Step 5/6 =====
// Stepper DRV8825 (pompa peristaltik)
#define PIN_SLP1        1     // HCl
#define PIN_STEP1       2
#define PIN_DIR1        42
#define PIN_SLP2        41    // H2O2
#define PIN_STEP2       40
#define PIN_DIR2        39
#define PIN_SLP3        38    // H2O
#define PIN_STEP3       47
#define PIN_DIR3        21
```

Ganti dengan:

```cpp
// ===== PIN MAP (ESP32-S3) =====
// Stepper TMC2209 x3 (pompa peristaltik) — pin map dikoreksi, lihat PROCESS.md §1.2
#define PIN_TMC_EN      1      // enable seluruh driver TMC2209 (shared 3 channel)
#define PIN_TMC_PDN     2      // power-down / reset seluruh driver (shared)
#define PIN_STEP1       38     // STEP pompa HCl
#define PIN_DIR1        48     // DIR pompa HCl
#define PIN_STEP2       40     // STEP pompa H2O2
#define PIN_DIR2        39     // DIR pompa H2O2
#define PIN_STEP3       42     // STEP pompa Air
#define PIN_DIR3        41     // DIR pompa Air
// Catatan: GPIO 21 dan 47 menjadi bebas (semula DIR3 & STEP3 DRV8825)
```

> GPIO 21 dan 47 sekarang bebas — kandidat sensor level tangki proses
> (`PROCESS.md` §1.6). Tidak dipakai di Gelombang 1.

### 1b. Tambah blok konstanta dosing & proses (setelah blok TB6612FNG)

```cpp
// ===== STEPER: mikrostep & timing =====
// TMC2209 diset lewat UART (resistor), bukan jumper MS1/MS2/MS3.
// Nilai 1/16 = 3200 step/rev. WAJIB dikonfirmasi di hardware (PROCESS.md §12.2).
#define TMC_MICROSTEP        16
#define STEPPER_STEP_PER_REV 3200    // 200 × TMC_MICROSTEP (motor 1.8°)
#define LANGKAH_INTERVAL_US  400     // jeda pulsa, 2500 langkah/detik
// ^ nilai INI akan diturunkan setelah uji laju step (PROGRESS §11).
//   Jangan diubah ke 1200 tanpa mengukur ml/s — memperlambat 3x.

// ===== DOSING (paralel) =====
#define DOSING_PARALEL          1
#define DOSING_MARGIN_MS    15000UL    // margin watchdog di atas estimasi
#define DOSING_TIMEOUT_MAKS_MS 300000UL // plafon keras watchdog (5 menit)
#define DETEK_JEKOT_MS       3000UL     // langkah tak bertambah = jekot
#define TOLERANSI_STOK          10      // ml toleransi sensor capacitive
#define PRIME_STEPS            200      // langkah prime selang sebelum dosing
#define ANTI_DRIP_STEPS         50      // langkah balik DIR setelah pompa selesai
#define DOSE_MIN_ML             1       // guard volume minimum per pompa
#define DOSE_MAX_ML           500       // guard volume maksimum per pompa

// ===== Volume tangki sumber (untuk cek ketersediaan) =====
#define VOL_TANGKI_HCL      1000
#define VOL_TANGKI_H2O2     1000
#define VOL_TANGKI_H2O      2000
#define VOL_TANGKI_SABUN    1000
#define VOL_TANGKI_LIMBAH   5000

// ===== DRAINING & RINSING (fixed time) =====
#define T_DRAINING_MS      45000UL
#define T_RINSE_AIR_MS     30000UL
#define T_RINSE_SABUN_MS   10000UL
#define T_DRAIN_AKHIR_MS   15000UL
#define ML_BILASAN            200      // ml air bilasan
#define ML_BILASAN_SABUN      50      // ml air sabun (pompa DC, durasi-based)
#define LOKASI_LIMBAH_MAKS     95      // % -> guard overfill
#define SPEED_AGITASI_BILAS     2

// ===== PROTEKSI ETCHING =====
#define SUHU_MAKS_KOMPONEN   70.0f      // °C -> turunkan speed agitasi
#define SUHU_MAKS_STOP       85.0f      // °C -> gagal
#define GAS_AMANG_PPM        400       // MQ-135 -> badge peringatan di UI
#define RAMP_AGITASI_MS      1000       // ramp-up speed agitasi
```

---

## 2. Blok 2 — `data.h` (dipakai B)

**Deviasi dari `PROCESS.md` §3 (sengaja):** enum `FaseProses`, `KodeError`,
`namaFase[]`, `pesanError[]`, dan seluruh state proses **tetap di `process.h`**
(milik A), **tidak** dipindah ke `data.h`.

**Alasan:** state proses sudah tinggal di `process.h` sekarang; memindahnya
menambah diff besar tanpa manfaat, dan membuat A bergantung pada file B saat
menulis inti logikanya. `ui.h` sudah di-include **setelah** `process.h`
(`auto-etching-firmware.ino:34-35`), jadi `ui.h` tetap bisa membaca state A
secara langsung.

### 2a. Tambah halaman alarm

Di `enum Halaman` (`data.h:46`), tambahkan `HAL_ALARM` **paling akhir**:

```cpp
enum Halaman {
  // ... enum yang sudah ada ...
  HAL_KALIB_UDARA,
  HAL_ALARM                        // BARU: halaman kondisi gagal
};
```

Di `namaHalaman[]` (`data.h:71`), tambahkan `"KALIB_ALARM"` di posisi yang sama
(hanya untuk log `[NAV]`):

```cpp
  "KALIB_SUHU", "KALIB_UDARA", "KALIB_ALARM"
};
```

### 2b. Halaman alarm di `ui.h` (B)

```cpp
// ================= HALAMAN: ALARM =================
void gambarAlarm() {
  gambarHeader("GAGAL");

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  // Nama error (mis. "CAIRAN KURANG")
  teksTengah(pesanError[prosesError], HEADER_H + 6, 1);

  // Detail singkat dari A: mis. "HCl kurang 24ml" — opsional
  char buf[24];
  snprintf(buf, sizeof(buf), "Fase: %s", namaFase[faseSebelumGagal]);
  teksTengah(buf, HEADER_H + 18, 1);

  // Footer: hanya BACK, dan hanya bila pin E-stop sudah HIGH
  bool bolehReset = (digitalRead(PIN_ESTOP) == HIGH);
  teksTengah(bolehReset ? "[BACK] Reset" : "Lepas E-stop dulu",
             AREA_BAWAH - 12, 1);
}
```

Tambahkan `case` di `ui.h` dispatcher (~`ui.h:866`):

```cpp
    case HAL_ALARM:          gambarAlarm();          break;
```

> `pesanError`, `namaFase`, `faseSebelumGagal` berasal dari `process.h` (A) —
> sudah termasuk dalam blok yang di `<process.h>`.

### 2c. Reset alarm — handler di `.ino` (B)

```cpp
    // ---------- Alarm ----------
    case HAL_ALARM:
      // BACK hanya boleh melepas latch bila pin E-stop sudah HIGH
      if (t == P_BACK) prosesResetAlarm();
      break;
```

---

## 3. Blok 3 — `auto-etching-firmware.ino` (dipakai B)

### 3a. E-stop pindah ke baris pertama `loop()`

Ganti blok di `.ino:633-644`:

```cpp
  // E-stop: hentikan semua aktuator & batalkan proses
  if (digitalRead(PIN_ESTOP) == LOW) {
    if (prosesJalan) {
      prosesBatal();
      tampilToast("E-STOP!");
    }
    agitasiSet(0);
    pompaLimbahSet(false);
    pompaSabunSet(false);
    fanSet(false);
    pompaStopAll();
  }
```

dengan versi ini di **baris pertama `loop()`**:

```cpp
void loop() {
  // ① E-STOP — PALING ATAS, sebelum tombol/proses/motor/sensor/render
  if (digitalRead(PIN_ESTOP) == LOW) {
    if (!estopAktif) {
      estopAktif = true;              // latch: tidak auto-clep saat pin HIGH lagi
      prosesGagal(ERR_ESTOP);         // A: matikan semua aktuator + pindah HAL_ALARM
    }
    semuaAktuatorMati();              // A: defensif, kalau prosesGagal belum sempat
    buzzerAlarmStop();
    if (!perluGambar) { delay(1); continue; }   // skip tombol/proses/motor/sensor
  }

  int t = bacaTombolBaru();
  if (t != TIDAK_ADA) tanganiTombol(t);

  prosesUpdate();      // A — logika proses (tidak bergantung render)
  motorUpdate();       // A — stepper non-blocking (freeze sendiri saat jeda/latch)
  sensorUpdate();      // B
  buzzerUpdate();      // A
  ... (sisa loop tidak berubah)
```

**Perubahan minimum di B:** memindahkan blok E-stop ke atas + menghapus 5 baris
stop duplikat. Fungsi `estopAktif`, `prosesGagal()`, `semuaAktuatorMati()`,
`buzzerAlarmStop()` semuanya milik A.

### 3b. `delay()` di halaman kalibrasi motor harus diganti (PRIORITAS)

`.ino:309-341` memakai `delay(1000-2000)` saat uji agitasi & fan → UI beku
selama 1–2 detik, dan **menyulitkan A menguji motor**. Pola yang diminta A
(sudah punya state di `data.h`):

```cpp
// instead of: agitasiSet(s); delay(1000); agitasiSet(0);
agitasiTestMulaiMs = millis();
agitasiTestAktif   = true;
// lalu di loop(): if (agitasiTestAktif && millis() - agitasiTestMulaiMs > 1000) { agitasiSet(0); agitasiTestAktif = false; }
```

Hal sama untuk `fanSet(true) + delay(2000)` di `.ino:339-341`, dan untuk
`kalibMotorRunning` yang sudah diset `true` tapi tidak pernah dibaca
(`data.h:111`).

---

## 4. Blok 4 — Item milik B (spesifikasi dari A)

Item berikut masuk **Bab 3–6** (`PROGRESS.md`) tapi file-nya milik B, jadi
dikerjakan B dengan spesifikasi berikut:

| # | Item PROGRESS | File | Spesifikasi |
|---|---|---|---|
| B1 | §3.3 `hitungUlang()` bulatkan ke terdekat | `presetops.h` | `mlHCl = (vol*persenHCl+50)/100`, `mlH2O2 = (vol*persenH2O2+50)/100`, `mlH2O = vol - mlHCl - mlH2O2`. Saat truncate, sisa pembulatan (±1.8 ml) jatuh seluruhnya ke H2O tanpa disadari user (`dosing-design.md` §2.7) |
| B2 | §3.3 `validasiPreset()` tolak sisa pembulatan | `presetops.h` | tolak preset bila `\|vol - (persenHCl+persenH2O2+persenH2O)/100*vol\| > 2 ml` |
| B3 | §2 `HAL_ALARM` + `gambarAlarm()` | `ui.h`, `data.h` | lihat §2b |
| B4 | §6 reset latch E-stop | `.ino` | lihat §2c — BACK + `digitalRead(PIN_ESTOP) == HIGH` |
| B5 | §6 halamanalarm + instruksi pemulihan | `ui.h` | tampilkan penyebab + tindakan yang harus dilakukan user |
| B6 | §4 badge peringatan gas/suhu | `ui.h` | baca `badgeWarning` (bool, diset A) → badge kecil di header; teks: "GAS!" / "PANAS" |
| B7 | §2 panel ml terpompa per bahan | `ui.h` | layar proses: 3 baris `HCl/H2O2/H2O` → `pompaMlTerkira(i)` / `pompaTargetMl(i)`. Contoh acuan: `backup/v0.8.2/ui.h:315-321` |
| B8 | §3.3 wizard kalibrasi gravimetrik | `ui.h`, `.ino`, `data.h` | layar: Target uji (50 ml) → Volume aktual (hasil timbang) → baris hasil `ml/rotasi` + tombol START. Non-blocking (lihat §3b) |
| B9 | §7 `DataKalib` versi 2 | `storage.h` | tambah `uint16_t mlPerRotasiX100[3]` + `uint8_t pompaValid[3]`, naikkan `FORMAT_VERSI_KALIB` 1→2, CRC & validasi rentang (`dosing-design.md` §2.8) |
| B10 | §7/§8 kalibrasi motor/DC tersimpan | `storage.h` | `kalibDuty[]`, `kalibMl[]`, `kalibSuhuAddr[]`, `suhuOffsetX10[]`, `mq135Normal/Kotor` — sekarang hilang saat reset |
| B11 | §5 volume bilasan & sab custom dari preset | `presetops.h`, `data.h` | `ML_BILASAN` / `ML_BILASAN_SABUN` jadi field preset (default dari `config.h`) |
| B12 | §5 histeresis/debounce level anti-thrash | `sensor.h` | sudah ada sebagian (`TOUCH_DEBOUNCE`, `SENSOR_HYST`); pastikan guard overfill A tidak memicu rapid on/off |

> **B9 adalah blocker Gelombang 2.** Tanpa `pompaValid[]`, dosing tidak punya
> sumber "sudah terkalibrasi" dan `ERR_POMPA_KALIB` tidak bisa diimplementasikan.

---

## 5. Blok 5 - API Orang A (untuk B agar bisa compile UI)

### 5a. Fungsi yang SUDAH dipakai B — jangan diubah

```cpp
// motor.h
void motorInit();
void pompaMulai(int idx, int ml);     // .ino:239
void pompaStopAll();                   // .ino:643
// dcmotor.h
void dcmotorInit();                    // .ino:580
void agitasiSet(int speed);            // .ino:309-315 (0..5)
void pompaSabunSet(bool on);           // .ino:282, 641
void pompaLimbahSet(bool on);          // .ino:283, 640
void fanSet(bool on);                  // .ino:339-341, 642
// buzzer.h
void buzzerKlik(); void buzzerBoot(); void buzzerUpdate();
```

### 5b. Fungsi BARU yang A sediakan (B boleh pakai mulai Gelombang 2)

```cpp
// motor.h
float pompaMlTerkira(int idx);    // ml yang sudah terpompa (real-time)
int   pompaTargetMl(int idx);     // ml target fase ini
uint8_t pompaProgres(int idx);    // 0..100 persen
bool  pompaSemuaSelesai();

// process.h
void prosesGagal(int kode);       // satu-satunya jalan keluar kondisi abnormal
void prosesResetAlarm();          // lepas latch + FASE_IDLE (dipanggil B)
void semuaAktuatorMati();         // defensif, boleh dipanggil B
// state yang dibaca ui.h
FaseProses prosesFase; KodeError prosesError; FaseProses faseSebelumGagal;
bool prosesJalan, prosesJeda, estopAktif, badgeWarning;
unsigned long prosesAkumMs, prosesTotalMsAktual;
int prosesSiklus, totalMlTerpompa;
const char* namaFase[]; const char* pesanError[];
```

> Tanda tangan final di `<process.h>` dikunci bersama B sebelum Gelombang 1 merge.

---

## 6. Checklist Compile & Smoke Test (manual, tiap akhir gelombang)

1. Arduino IDE → board **ESP32-S3 DevKitC** → Verify.
2. `Select-String *.ino,*.h 'delay\('` → harus **0** di jalur proses.
3. Smoke test Serial, harus muncul sesuai transisi:
   - `[POMPA] Mulai idx 0` / `[POMPA] Selesai idx 0`
   - `[PROSES] Fase: DOSING` → `ETCHING` → …
   - `[BUZZ]` untuk alarm
4. Uji E-stop: saat dosing jalan, sambungkan GND ke pin 8 → semua aktuator mati
   **< 1 siklus loop**, latch aktif, tombol tidak membatalkan.
5. Cek `git status` bersih sebelum membuka PR.

---

## 7. Urutan Kerja & Branch

```
main
 ├─ (dokumen ini) kontrak/bab0  ← sudah di-commit
 ├─ a/gelombang-1  (A)  ──PR──► main   [PR #1 ditutup dulu]
 ├─ b/gelombang-1  (B)  ──PR──► main   [PR #2 baru dibuka setelah #1 merge]
 ├─ a/gelombang-2  (A)  ──PR──► main
 └─ ...
```

Urutan bergantung (A tidak bisa lanjut sebelum B):

| Gelombang | A butuh dari B |
|---|---|
| Gelombang 1 | Blok 1 (`config.h` — hanya bila A sudah habis dengan shim `#ifndef`) |
| Gelombang 2 | Blok 1 + Blok 2 (`data.h`) + **B9** (`DataKalib` v2) |
| Gelombang 3 | B3/B4/B5 (halaman alarm) |

---

## 8. Tanda Tangan Persetujuan

- [ ] Orang B menyetujui Blok 1 (`config.h`)
- [ ] Orang B menyetujui Blok 2 (`data.h` + `ui.h`)
- [ ] Orang B menyetujui Blok 3 (`.ino`)
- [ ] API §5b disepakati (tanda tangan frozen)
- [ ] Urutan `loop()` §3a disepakati

---

*Dokumen ini bagian dari `PROGRESS.md` Bab 0. Referensi: `dosing-design.md`,
`PROCESS.md` §5–§6. Jangan dihapus — ini acuan saat konflik merge.*