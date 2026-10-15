# DOSING DESIGN — Desain Diri Komprehensif Modul Dosing

Firmware: **Etching v0.5.4** (`config.h:112`) · Board: **ESP32-S3 DevKitC**
Dokumen ini adalah **spesifikasi rinci** untuk satu modul saja: **dosing**
(HCl / H2O2 / H2O lewat 3 pompa peristaltik + stepper driver).

| Dokumen | Isi | Hubungannya dengan dokumen ini |
|---|---|---|
| `PROCESS.md` §6 (SECTION 2 — DOSING) | Task list 2.1–2.10 + cara kerja | **Rujukan utama.** Dokumen ini memperdalam dan mengoreksi bagian yang belum dibahas di sana (matematika dosing, kalibrasi, error budget, prime/anti-drip, UI) |
| `PROGRESS.md` §3 (Modul Dosing) | Checklist status | Diperbarui mengikuti implementasi dokumen ini |
| `etching-otomatis.md` §4 | "Kalibrasi Dosing" versi lama | **Superseded** oleh dokumen ini (lihat §2) |

Status dokumen: **spesifikasi, belum diimplementasikan.** Tidak ada kode yang
diubah oleh dokumen ini.

---

## 1. Keputusan Desain

| Topik | Keputusan | Alasan |
|---|---|---|
| Driver stepper | **TMC2209 ×3** (bukan DRV8825) | Pin map dikoreksi di `PROCESS.md` §1.2; DRV8825 hanya bisa 1/32, klaim lama 1/36 tidak mungkin |
| Urutan pompa | **Paralel** — 3 pompa nyala bersamaan di `FASE_DOSING` | Memperpendek siklus; semua bahan masuk tangki proses sekaligus |
| Akurasi | **Open-loop** (langkah × `langkahPerMl`) | Tidak ada sensor volume di tangki proses; konfirmasi hanya lewat sensor jerican sumber |
| Kalibrasi | **Gravimetrik** (timbangan / gelas ukur) | Satu-satunya cara mengukur ml peristaltik yang bisa diulang di lapangan |
| Selesai fase | **Berbasis kondisi** (semua pompa selesai), waktu hanya **watchdog** | Durasi aktual pompa tidak konstan; fraksi waktu selalu meleset |
| Durasi total | **Dihitung dari kalibrasi** (`ml × langkahPerMl × interval`) | Progress bar harus jujur dengan apa yang dijalankan mesin |
| Sumber kebenaran | `dosing-design.md` untuk dosing, `PROCESS.md` untuk orkestrasi fase | Menghindari dua dokumen mendefinisikan hal yang sama |

---

## 2. Koreksi terhadap Dokumen & Kode Lama

Bagian ini menjawab "desainnya yang perlu diperbaiki". Setiap koreksi disertai
rujukan `file:baris` dan alasannya.

### 2.1 Klaim mikrostep 1/36 (tidak mungkin)

`etching-otomatis.md:106` menyatakan "Mikrostep 1/36 → 200 × 36 = 7200
step/putaran". DRV8825 (dan TMC2209) **maksimum 1/32**, dan 1/36 bukan mode
mikrostep yang ada pada TMC2209. Angka ini ikut terbawa ke `motor.h:14` sebagai
komentar (`// 7200 step/rev; kalibrasi nanti`) sementara `langkahPerMl = 1200`
di-hardcode.

**Koreksi:** step/rev harus menjadi konstanta eksplisit di `config.h`, bukan
angka mati di komentar:

```cpp
#define TMC_MICROSTEP        16     // mode UART TMC2209 (konfirmasi fisik)
#define STEPPER_STEP_PER_REV 3200    // 200 × TMC_MICROSTEP (motor 1.8°)
```

Nilai **wajib dikonfirmasi di hardware** sebelum dosing dipakai (lihat §11).

### 2.2 `langkahPerMl` di-hardcode dan hasil kalibrasi tidak terpakai

- `motor.h:14` — `long langkahPerMl[3] = { 1200, 1200, 1200 };` (asumsi, bukan hasil ukur).
- `data.h:99` — `kalibStepperMl[3]` default `{1.0, 1.0, 1.0}` ml/rotasi.
- `data.h:99` → `.ino:231-233` (ubah dari UI) → `ui.h:511` (tampil) — **berhenti di situ**.
  Tidak ada satu baris pun yang memakainya saat menghitung langkah.
- `sensor_extra.h:62-80` punya `langkahPerMlX1000[]` + `pompaTerapkanKalibrasi()`,
  tapi **`sensor_extra.h` tidak di-`#include` siapa pun** → kode mati.

Akibatnya: **akurasi dosing saat ini nol** — user mengatur ml/rotasi, tidak ada
yang berubah di proses.

**Koreksi:** `langkahPerMl` dihitung ulang dari kalibrasi setiap kali nilai
disimpan, dan sumber tunggalnya NVS (§6.4).

### 2.3 Fase DOSING berbasis fraksi waktu (paling fatal)

`process.h:70-75`:

```cpp
FaseProses faseDariFrac(float f) {
  if (f < 0.10) return FASE_DOSING;      // 10% dari totalDetik
  ...
}
```

`process.h:28-33` menyalakan 3 pompa sekaligus di awal fase, lalu **tidak
pernah menunggu** `pompaSelesai()`. Akibatnya:

| Gejala | Penyebab |
|---|---|
| Pompa masih jalan saat fase ETCHING dimulai, HCl menetes ke tangki proses saat pengadukan | Fase berpindah hanya karena waktu |
| Pompa sudah selesai 10 s lalu; sisa 30 s fase terbuang sia-sia | Tidak ada transisi berbasis kondisi |
| Dosis jadi tidak deterministik untuk volume berbeda | 10% dari `totalDetik` tidak ada hubungannya dengan jumlah ml |

**Koreksi:** `FASE_DOSING` selesai **hanya** bila
`pompaSelesai(0) && pompaSelesai(1) && pompaSelesai(2)` (pompa dengan `ml = 0`
di-skip, tidak ikut dihitung). Logika fraksi waktu dihapus untuk fase ini.

### 2.4 Logika "ml yang dibutuhkan" di `PROCESS.md` §6 terbalik

`PROCESS.md` §6.2 baris 293-296:

```
butuh_i = max(0, mlTarget_i − tersedia_i)
if (butuh_i > tersedia_i) → prosesGagal(ERR_LEVEL_KURANG)
```

Dua masalah:

1. **Baris kedua adalah kondisi yang salah.** Setelah `butuh_i` didefinisikan
   sebagai `target − tersedia`, maka `butuh_i > tersedia_i` hanya benar bila
   `target > 2 × tersedia` — logikanya bukan "cairan kurang".
2. **ml yang dipompa bukan `target − tersedia`.** Dosis adalah **resep etching**
   (mis. 300 ml HCl untuk PCB 100×80 mm), bukan selisih antara sisa
   tangki dan resep. Kalau HCl tinggal 120 ml di jerican, yang dipompa tetap
   300 ml (atau proses gagal), bukan 180 ml.

**Koreksi:**

```
butuh_i    = mlTarget_i                        // dosis = resep, selalu
tersedia_i = sensorPersen[i] × VOL_TANGKI[i] / 100
if (mlTarget_i > tersedia_i + TOLERANSI_STOK)  → prosesGagal(ERR_LEVEL_KURANG)
```

Sensor level jerican dipakai sebagai **pengesahan ketersediaan**, bukan
penghitung dosis. `TOLERANSI_STOK` diperlukan karena sensor capacitive bukan
volumetrik; default 10 ml.

### 2.5 Deteksi macet tidak bisa mendeteksi macet

`PROCESS.md` §6.1 task 2.7 mengusulkan: "`pompaJalan[i] == true` tapi
`jalanLangkah[i]` tidak bertambah > 3 s → `ERR_STEP_MACET`". Padahal
`motor.h:50-67` menaikkan `jalanLangkah[i]` **setiap pulsa**, tanpa umpan
balik apa pun dari motor. Kalau rotor macet, kode tetap menghitung langkah,
jadi **deteksi ini mustahil menangkap macet fisik**.

**Koreksi — dua lapis, dengan nama yang jujur:**

| Lapis | Nama | Cara kerja | Yang menangkap |
|---|---|---|---|
| 1 | `DeteksiJekot` (hang) | `jalanLangkah[]` datar > 3 s | Loop macet, timer salah, driver tidak merespons (mis. PDN LOW) |
| 2 | `DeteksiMacet` (stall) | **Opsional**, butuh TMC2209 StallGuard via UART atau sensor arus | Rotor macet / selang tersumbat |

Lapis 2 tidak dipakai di versi pertama. Yang sekarang ada adalah lapis 1 —
sebaiknya tidak dinamai "macet" supaya tidak memberi rasa aman yang semu.

### 2.6 Watchdog tetap 90 s bisa lebih pendek dari dosis yang sah

`PROCESS.md` §4 menetapkan `TIMEOUT_DOSING_MS 90000UL` sebagai konstanta tetap.
Dengan `STEP_INTERVAL_US = 1200` (833 langkah/detik) dan `stepPerRev = 3200`:

| Skenario | Laju | Waktu untuk 300 ml (`1.0 ml/rotasi`) | Berhasil dalam 90 s? |
|---|---|---|---|
| 1200 µs, 1.0 ml/rot | 0.26 ml/s | **1 152 s (19 menit)** | Tidak |
| 600 µs, 1.0 ml/rot | 0.52 ml/s | 576 s | Tidak |
| 400 µs, 1.0 ml/rot | 0.78 ml/s | 384 s | Tidak |
| 1200 µs, 5.0 ml/rot | 1.30 ml/s | 230 s | Tidak |

Artinya dengan laju saat ini, **dosis 300 ml tidak akan pernah selesai dalam
konstanta watchdog mana pun yang wajar** — `TIMEOUT_DOSING_MS 90000` akan
membunuh proses yang sah. Ini juga berarti usulan `STEP_INTERVAL_US` 400 → 1200
(`PROCESS.md` §6.1 task 2.5) membuat masalah **lebih lambat**, bukan lebih aman.

**Koreksi (dua bagian):**

```cpp
// Watchdog diturunkan dari laju nyata, bukan konstanta mati
#define DOSING_MARGIN_MS        15000UL   // margin tetap
#define DOSING_TIMEOUT_MAKS_MS 300000UL   // plafon keras 5 menit

unsigned long dosingWatchdogMs() {
  unsigned long t = 0;
  for (int i = 0; i < 3; i++)
    t = max(t, (unsigned long)(targetLangkah[i] * LANGKAH_INTERVAL_US / 1000));
  return min(t * 2 + DOSING_MARGIN_MS, DOSING_TIMEOUT_MAKS_MS);
}
```

```cpp
// Laju pompa harus diukur lebih dulu (§10.1) sebelum nilainya ditetapkan
#define LANGKAH_INTERVAL_US   400      // 2 500 langkah/detik, hasil pengujian
```

Durasi yang sama dipakai untuk progress bar, supaya bar dan watchdog konsisten
dengan apa yang benar-benar dijalankan.

### 2.7 Pembulatan komposisi menggeser error ke H2O

`presetops.h:17-22`:

```cpp
p.mlHCl  = (int)((long)vol * p.persenHCl  / 100);   // truncate
p.mlH2O2 = (int)((long)vol * p.persenH2O2 / 100);   // truncate
p.mlH2O  = vol - p.mlHCl - p.mlH2O2;               // sisa masuk ke H2O
```

Untuk `vol = 360`, `HCl 33%`, `H2O2 17%`: HCl 118, H2O2 61, H2O 181 —
sebenarnya 118.8 / 61.2 / 180.0. Kesalahan pembulatan (~1.8 ml) seluruhnya
jatuh ke H2O tanpa disadari user.

**Koreksi:** bulatkan ke terdekat, sisa pembulatan ditahan/dibatasi:

```cpp
p.mlHCl  = (vol * p.persenHCl  + 50) / 100;
p.mlH2O2 = (vol * p.persenH2O2 + 50) / 100;
p.mlH2O  = vol - p.mlHCl - p.mlH2O2;   // sisa pembulatan, dibatasi ±2 ml
```

Bila `|sisa|` melebihi 2 ml, preset ditolak di `validasiPreset()` — lebih baik
preset ditolak daripada komposisi melenceng diam-diam.

### 2.8 Kalibrasi pompa tidak tersimpan ke NVS

`storage.h:78-85` — `DataKalib` (blob `"kal"`, `FORMAT_VERSI_KALIB 1`) **hanya**
menyimpan titik sensor level (`kosong[5]`, `penuh[5]`). Tidak ada tempat untuk
ml/rotasi pompa.

`PROCESS.md` §6.1 task 2.2 mengusulkan key baru `"pompa"`. **Deviasi dari
usulan itu, dengan alasan:** dua blob terpisah berarti dua operasi tulis dan
risiko state setengah ter-update (pompa terkalibrasi tapi sensor belum). Satu
blob dengan satu CRC lebih konsisten dengan pola yang sudah dipakai repo
(`storage.h:87-124`).

```cpp
#define FORMAT_VERSI_KALIB 2

struct DataKalib {
  uint32_t versi;
  int32_t  kosong[JUMLAH_SENSOR];              // sensor level, tidak berubah
  int32_t  penuh[JUMLAH_SENSOR];
  uint16_t mlPerRotasiX100[JUMLAH_POMPA];      // BARU: 3 pompa stepper
  uint8_t  pompaValid[JUMLAH_POMPA];           // BARU: sudah diukur?
  uint32_t crc;
};
```

Naik format versi → data lama otomatis ditolak → nilai bawaan dipakai →
`pompaValid = false` → dosing **ditolak sampai dikalibrasi** (§4). Ini memaksa
user melakukan pengukuran, alih-alih diam-diam memakai angka asumsi `1200`.

### 2.9 Ringkasan temuan dokumen lain (di luar cakupan)

Tidak dikerjakan di dokumen ini, tapi masuk daftar backlog dokumentasi:

| File | Temuan |
|---|---|
| `README.md:3` | Masih menyebut "Arduino Uno/Nano", RTC, EEPROM — firmware sudah ESP32-S3 + NVS |
| `README.md:25`, `GUIDE.md:195` | Sebut `aktuator.h` — file ini tidak ada di root (hanya di `backup/`) |
| `GUIDE.md:196` | `motor.h` ditulis "Motor DC control" — sebenarnya stepper peristaltik; yang DC adalah `dcmotor.h` |
| `SUMMARY.md:3` | Versi tertulis v0.5.2, `config.h:112` sudah v0.5.4 |
| `SUMMARY.md:39` | `sensor.h` masih dideskripsikan "5 sensor touch, baseline adaptif" — sudah tidak sesuai implementasi terbaru |
| `etching-otomatis.md` §2 | Pin map masih DRV8825 (SLP), sudah digantikan oleh `PROCESS.md` §1.2 |

---

## 3. Model Matematika Dosing

Definisi:

```
stepPerRev   = 200 × TMC_MICROSTEP                    (3200 untuk 1/16)
mlPerRotasi  = hasil kalibrasi gravimetrik per pompa   (tiap pompa berbeda!)
langkahPerMl = stepPerRev / mlPerRotasi
```

Konversi ml → langkah:

```cpp
long langkahUntuk(int idx, float ml) {
  return lroundf(ml * langkahPerMl[idx]);
}
```

| Besaran | Rumus | Contoh (`stepPerRev 3200`, `1.0 ml/rot`) |
|---|---|---|
| langkah per ml | `3200 / 1.0` | 3 200 langkah/ml |
| dosis 300 ml | `300 × 3200` | 960 000 langkah |
| error kuantisasi | `± 0.5 / langkahPerMl` | ± 0.00016 ml (tidak signifikan) |
| durasi @ 400 µs | `langkah × 400 / 1000` | 384 000 ms = 384 s |
| durasi fase (paralel) | `max(t₀, t₁, t₂)` | bukan jumlah |

**Kuantisasi langkah bukan sumber error utama.** Error datang dari hasil
kalibrasi ml/rotasi (dominan), perubahan karakteristik selang, dan
backpressure (lihat §8).

Pembulatan `int` pada dosis ml dihilangkan lewat `hitungUlang()` (§2.7);
`motor.h:34` memakai `ml × langkahPerMl` dengan `ml` bulat — dengan
`langkahPerMl` sebesar itu, satu ml bulat sudah jauh lebih besar daripada
kuantisasi.

---

## 4. State Machine Dosing

```
prosesMulai()
   │
   ▼
FASE_CEK ── sensor 0/1/2 tidak terbaca ──────────► prosesGagal(ERR_SENSOR)
   │        sensorPersen[i] = 0 (tak terkalibrasi) ► prosesGagal(ERR_SENSOR)
   │        pompaValid[i] = false                  ► prosesGagal(ERR_POMPA_KALIB)
   │        mlTarget_i > tersedia_i + toleransi   ► prosesGagal(ERR_LEVEL_KURANG)
   ▼
FASE_DOSING
   ├─ prime: N langkah maju per pompa (bilas udara di selang)
   ├─ pompaMulai(0, mlHCl)   ┐
   ├─ pompaMulai(1, mlH2O2)  ├─ PARALEL (pompa dengan ml = 0 dilewati)
   └─ pompaMulai(2, mlH2O)   ┘
   ├─ watchdog = dosingWatchdogMs()               (§2.6)
   ├─ jekot:   langkah datar > DETEK_JEKOT_MS     ► prosesGagal(ERR_STEP_MACET)
   ├─ sumber:  sensorPersen[i] == 0 saat jalan    ► prosesGagal(ERR_SUMBER_HABIS)
   ▼  semua pompaSelesai() == true
FASE_ETCHING
```

### 4.1 Alur tombol saat dosing

| Aksi | Dampak |
|---|---|
| BACK (buka dialog) | Tidak langsung mematikan pompa — dialog konfirmasi dulu |
| ENTER (konfirmasi batal) | `prosesBatal()` → `semuaAktuatorMati()` → semua pompa stop, TMC_PDN LOW |
| Jeda | `motorUpdate()` tidak dipanggil → pompa **freeze** di posisi; resume melanjutkan dari `jalanLangkah[]` yang tersisa, bukan mulai dari nol |
| E-stop | Latch, semua output mati < 1 siklus loop (mengikuti `PROCESS.md` §5) |

Jeda **tidak boleh** mengulang dosis: karena `targetLangkah` dan
`jalanLangkah` disimpan, resume melanjutkan — bukan reset ke 0.

### 4.2 Pelaporan ml terpompa

`motor.h` perlu mengekspor progres nyata, bukan estimasi:

```cpp
float   pompaMlTerkira(int idx);   // jalanLangkah[idx] / langkahPerMl[idx]
uint8_t pompaProgres(int idx);      // 0..100 persen
bool    pompaSemuaSelesai();
```

`ui.h` memakai ini untuk panel ml terpompa di halaman proses, menggantikan
pendekatan `prosesMlTerpompa` yang ada di `backup/v0.8.2/ui.h:315-321`
tetapi hilang di firmware root.

---

## 5. Perubahan yang Diperlukan per File

| File | Perubahan | Rujukan |
|---|---|---|
| `config.h` | Ganti pin map DRV8825 → TMC2209 (hapus `PIN_SLP1/2/3`, tambah `PIN_TMC_EN`, `PIN_TMC_PDN`), tambah `TMC_MICROSTEP`, `STEPPER_STEP_PER_REV`, `DOSING_MARGIN_MS`, `DOSING_TIMEOUT_MAKS_MS`, `DETEK_JEKOT_MS`, `TOLERANSI_STOK`, `VOL_TANGKI_*`, `PRIME_STEPS` | `PROCESS.md` §1.2 & §4 |
| `motor.h` | `langkahPerMl[]` dari kalibrasi (bukan literal), `DIR` per channel, penanganan `TMC_EN`/`TMC_PDN`, `pompaMlTerkira()`, `pompaProgres()`, `pompaSemuaSelesai()`, guard `ml > DOSE_MIN/MAX` | `motor.h:10-19, 32-42` |
| `process.h` | Hapus `faseDariFrac()` untuk dosing; `FASE_CEK` pre-flight; dosing berbasis kondisi; watchdog dinamis; guard `SUMBER_HABIS`; prime; `prosesGagal(kode)` | `process.h:28-33, 70-75` |
| `presetops.h` | Pembulatan ke terdekat di `hitungUlang()`; guard sisa pembulatan di `validasiPreset()` | `presetops.h:17-22, 49-62` |
| `storage.h` | `DataKalib` versi 2: `mlPerRotasiX100[3]`, `pompaValid[3]`, CRC & validasi rentang | `storage.h:78-124` |
| `data.h` | Ganti `kalibStepperMl[]` (data.h:99) menjadi nilai kalibrasi pompa bertipe `int`; tambah state wizard test | `data.h:99` |
| `ui.h` | Halaman kalibrasi pompa: tombol test + input hasil ukur; panel ml terpompa di halaman proses | `ui.h:483-513`, `ui.h:225-245` |
| `.ino` | Wizard test kalibrasi (non-blocking, bukan `delay()`); navigasi halaman baru | `.ino:222-243` |
| `sensor_extra.h` | **Hapus** atau di-`#include` — saat ini file mati (§2.2) | `sensor_extra.h:62-80` |

---

## 6. Prosedur Kalibrasi Gravimetrik

Prasyarat: **air biasa saja** (`PROGRESS.md` §11 — HCl pekat berbahaya).
Gunakan wadah yang sudah ditimbang (`W₀`), lalu timbang setelah dosing (`W₁`).

### 6.1 Langkah per pompa

1. Hubungkan pompa ke wadah timbang, **pastikan selang terisi penuh** (prime).
2. Kalibrasi → Motor → Pompa HCl.
3. Tekan **START** dengan target uji **50 ml** (nilai `N`).
4. Tunggu "Selesai", lalu hentikan pompa dan lepas selang dari tangki proses.
5. Timbang → volume aktual `V = W₁ − W₀` ml.
6. **Ulangi 3×** dengan wadah dibersihkan dan dikeringkan; catat semua di Serial.
7. Firmware menampilkan jumlah langkah yang sudah dijalankan, `L_total`.
   Hitung:

```
langkahPerMl[i] = L_total / V_rata        (langkah per ml)
mlPerRotasi[i]  = stepPerRev / langkahPerMl[i]   (untuk tampilan di UI)
```

dengan `V_rata` = rata-rata volume aktual dari 3 ulangan. Angka `langkahPerMl`
lama **tidak dipakai** sebagai asumsi — justru angka itu yang dicari.

### 6.2 Kriteria terima

| Kriteria | Ambang | Tindakan bila gagal |
|---|---|---|
| Repeatability antar-ulangan | simpangan ≤ 3% dari rata-rata | Periksa selang bocor, Aggregate, motor panas |
| Akurasi terhadap target | error ≤ 5% | Ulangi dengan step rate lebih rendah (600 µs) |
| Dosis besar (300 ml) | error ≤ 5% | Ulangi; suspect backpressure |
| Larutan encer (HCl 10%) | error ≤ 5% | Viskositas berbeda → kalibrasi ulang |

### 6.3 Wizard UI (halaman Kalib Pompa)

```
┌─ Pompa HCl ──────────────────────┐
│ Target uji         50 ml         │  ← ATAS/BAWAH pindah baris
│ Volume aktual      48 ml         │  ← KIRI/KANAN ±1 ml (hasil timbang)
│ → ml/rotasi      1.04 ml         │  ← hasil hitung, read-only
│ [   START TEST   ]               │  ← ENTER
└──────────────────────────────────┘
  ulangan 1/3   ERROR: 4.2%  OK
```

Alur tombol:

| Tombol | Aksi |
|---|---|
| ATAS/BAWAH | pindah baris (Target uji → Volume aktual → START) |
| KIRI/KANAN | ubah nilai baris terpilih |
| ENTER di START | jalankan pompa secara non-blocking, dengan timer + auto-stop (menggantikan `delay()` yang membekukan UI) |
| ENTER di Volume aktual | masukkan hasil timbang → hitung `langkahPerMl` → simpan draft |
| BACK | keluar + dialog simpan/batal, seperti halaman kalibrasi lain |

**Auto-stop wajib:** `kalibMotorRunning` diset `true` tapi tidak pernah dibaca
(`PROGRESS.md` §7) → kalibrasi bisa berjalan tanpa henti. Perbaikan: `loop()`
mematikan motor bila `millis() − kalibMotorStartMs > kalibMotorRunMs`
(`data.h:111-112`, `.ino:239`).

### 6.4 Penyimpanan

Disimpan saat **BACK lalu dialog simpan**, lewat `simpanKalib()` yang sama
dengan kalibrasi sensor level (`storage.h:87`), dengan validasi rentang tambahan:

```cpp
// tolak nilai tak masuk akal sebelum disimpan
if (d.mlPerRotasiX100[i] < 10 || d.mlPerRotasiX100[i] > 1000) valid = false;
```

---

## 7. Prime, Anti-Drip, dan Sisa Cairan

Hal-hal ini tidak dibahas di `PROCESS.md` tapi menentukan berhasil atau tidaknya
dosing:

| Inisiatif | Masalah nyata | Desain |
|---|---|---|
| **Prime selang** | Pompa peristaltik yang mulai kering hanya menghisap udara → 300 ml menjadi 250 ml (atau 0) tanpa error | `PRIME_STEPS` (mis. 200 langkah) maju di awal `FASE_DOSING` **sebelum** dosis dihitung; log volume prime ke Serial |
| **Anti-drip** | Tekanan di dalam selang mendorong sisa cairan menetes setelah pompa stop → over-dosis kecil | Setelah pompa selesai, balik `DIR` sejauh `ANTI_DRIP_STEPS` (mis. 50 langkah), lalu `DIR` dikembalikan |
| **Cairan di cabang selang** | Saat pompa idle, cairan di cabang HCl bisa jatuh ke tangki proses | Valve tidak tersedia; ditutupi dengan waktu — pompastarted sedekat mungkin dengan awal agitasi, atau turunkan posisi tangki proses |
| **Backpressure** | Head tinggi menurunkan laju → dosis kurang | Kalibrasi harus pada **ketinggian akhir selang yang nyata** (tangki proses), bukan wadah di meja |
| **Panas motor** | Motor NEMA17 pada step rate tinggi bisa melambat → dosis kurang | Kalibrasi setelah 2 menit operasi (motor hangat), bukan langsung setelah boot |
| **Selang aus** | Peristaltik butuh 1–2 bulan sebelum stabil | Simpan tanggal kalibrasi di NVS; toast "Kalibrasi sudah > 30 hari" |

---

## 8. Error Budget

Target: **± 5%** (`etching-otomatis.md:108`).

| Sumber | Estimasi | Cara menekan |
|---|---|---|
| Kuantisasi langkah | ± 0.0002 ml (negligible) | — |
| Pembulatan komposisi | ± 2 ml dari 360 ml = **0.5%** | `hitungUlang()` bulatkan ke terdekat (§2.7) |
| Error kalibrasi gravimetrik | ± 1–2% (timbangan digital, uap air di tabung) | 3 ulangan, rata-rata, wadah sama |
| Karakteristik selang berubah | ± 2–3% setelah 1–2 bulan | Kalibrasi ulang berkala |
| Backpressure / ketinggian | ± 1–3% | Kalibrasi pada kondisi kerja nyata |
| Motor panas melambat | ± 1–2% | Kalibrasi saat motor hangat |
| Prime udara | 0–5% (kadang besar) | Prime setiap dosing (§7) |
| **Total realistis** | **± 5–7%** | Butuh prime dan kalibrasi yang tepat |

**Kesimpulan penting:** dengan desain open-loop + pompa peristaltik, ± 5%
adalah target yang sulit. Bila akurasi lebih ketat dibutuhkan, opsi naikannya
adalah sensor level di tangki proses (GPIO 21/47 bebas — `PROCESS.md` §1.6)
untuk verifikasi volume akhir setelah dosing.

---

## 9. UI Dosing

### 9.1 Layar proses saat `FASE_DOSING`

```
┌─ Etching Otomatis ────────── 02:14 ┐
│                                      │
│  DOSING                              │
│  HCl     ▓▓▓▓▓▓▓▓░░  168/169 ml     │
│  H2O2    ▓▓▓▓▓▓▓░░░  105/112 ml     │
│  H2O     ▓▓▓▓▓▓▓▓▓▓  201/200 ml     │
│                                      │
│  ▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░  46%   │
└──────────────────────────────────────┘
```

Tiga baris per bahan adalah item "panel ml terpompa per bahan" di
`PROGRESS.md` §2 — belum ada di firmware root, tapi sudah pernah ada di
`backup/v0.8.2/ui.h:315-321` sehingga bisa dipakai ulang sebagai acuan.

### 9.2 Error dosing

| Error | Arti | Saran untuk pengguna |
|---|---|---|
| `ERR_LEVEL_KURANG` | Cairan sumber kurang untuk resep | "Isi/ganti jerican HCl" |
| `ERR_SUMBER_HABIS` | Jerican habis **saat** pompa jalan | "Isi ulang lalu ulangi" |
| `ERR_STEP_MACET` | Loop jam / driver tidak merespons | "Cek kabel STEP dan TMC_PDN" |
| `ERR_TIMEOUT_DOSING` | Dosis melebihi watchdog | "Periksa selang tersumbat" |
| `ERR_POMPA_KALIB` | Dosing ditolak: pompa belum dikalibrasi | "Kalibrasi → Motor" |

---

## 10. Rencana Implementasi & Kriteria Uji

> Seluruh pengujian wajib memakai **air biasa** dulu, baru larutan encer.

| Urutan | Pekerjaan | Kriteria lulus |
|---|---|---|
| 1 | `config.h`: pin map TMC2209 + konstanta baru | 3 pompa merespons STEP/DIR baru; tidak ada `#define` mati |
| 2 | `motor.h`: `langkahPerMl` dari kalibrasi, EN/PDN, `pompaMlTerkira()` | 1 pompa men-dose 50 ml tepat pada jumlah langkah yang diminta |
| 3 | `storage.h` `DataKalib` v2 | Kalibrasi bertahan setelah reboot; data versi lama ditolak bersih |
| 4 | Wizard kalibrasi gravimetrik (`.ino`, `ui.h`) | 3 ulangan selesai tanpa UI beku (tidak ada `delay()`) |
| 5 | `process.h`: `FASE_CEK` + dosing berbasis kondisi + watchdog dinamis | Pompa tidak pernah meleset ke ETCHING; progress bar jujur |
| 6 | Prime + anti-drip | Tidak ada cairan keluar sebelum dosing maupun setelah selesai |
| 7 | Error handling: 5 kode error + halaman alarm | Semua error bisa di-reset, aktuator mati |
| 8 | **Uji akurasi akhir**: 3 pompa × 3 ulangan × 50 ml dan × 300 ml | Error ≤ 5% (lihat §6.2) |

### 10.1 Pengujian laju step (sebelum kalibrasi volume)

Karena laju pompa menentukan **berapa lama proses boleh berjalan** (§2.6),
laju harus diukur lebih dulu:

1. Jalankan pompa 10 000 langkah pada `LANGKAH_INTERVAL_US = 400` → catat
   waktu aktual.
2. Bandingkan dengan waktu teoritis (`10 000 × 400 / 1000 = 4 000 ms`).
3. Kalau aktual jauh lebih lama → motor miss step → turunkan ke 600/800 µs.
4. Kalau ml/s terlalu lambat untuk dosis terbesar yang dikonfigurasi →
   turunkan `LANGKAH_INTERVAL_US` atau turunkan `TMC_MICROSTEP`.
5. Rekam hasilnya ke Serial sebagai referensi penyetelan.

---

## 11. Pertanyaan Terbuka

1. **Mikrostep TMC2209 berapa?** Tidak terbaca dari firmware — menentukan
   `langkahPerMl` dan seluruh dosis. (Resolver: `PROCESS.md` §12.2)
2. **Apakah `TMC_EN`/`TMC_PDN` (GPIO 1 & 2) shared 3 channel atau per-channel?**
   Menentukan apakah `motorInit()` menulis 1 atau 3 pin. (Resolver: `PROCESS.md` §12.1)
3. **ml/rotasi aktual tiap pompa?** Belum ada satu pun angka hasil ukur —
   semua angka dalam dokumen ini adalah placeholder.
4. **Kapasitas jerican** (`VOL_TANGKI_H2O2` dll.) untuk ketersediaan sensor.
5. **Volume bilasan:** `PROCESS.md` §4 memakai `ML_BILASAN 200`, kode sekarang
   100 ml (`process.h:45`). Dosisnya juga perlu lewat jalur yang sama dengan
   kalibrasi.
6. **Tangki proses perlu sensor level?** GPIO 21/47 bebas; tanpa itu, verifikasi
   volume hanya lewat timbang manual.

---

*Dokumen ini melengkapi `PROCESS.md` §6 dan menggantikan `etching-otomatis.md`
§4. Status implementasi dicatat di `PROGRESS.md` §3.*