# Log Perubahan

Format entri: `[Tanggal] vX.Y.Z — ringkasan`
Setiap versi di-backup ke folder `backup/vX.Y.Z_tanggal/` sebelum perubahan dilakukan.

---

## [2026-10-15] docs — Sinkronisasi dokumentasi + desain dosing baru

**Perubahan hanya pada dokumen — tidak ada kode yang diubah, tidak ada versi
firmware yang naik (`config.h` masih v0.5.4).**

**Dokumen baru**
- `dosing-design.md` — desain rinci modul dosing: matematika ml→langkah,
  state machine dosing, prosedur kalibrasi gravimetrik + wizard UI, prime &
  anti-drip, error budget (±5–7%), rencana implementasi 8 tahap, dan daftar
  koreksi terhadap dokumen lama.

**Koreksi desain yang ditemukan (bagian paling penting)**
- **Fase DOSING berbasis fraksi waktu** (`process.h:70-75`) — pompa bisa masih
  jalan saat fase sudah pindah ke ETCHING. Desain baru: selesai berbasis
  kondisi (`pompaSelesai()` semua kanal).
- **Kalibrasi pompa tidak pernah dipakai** — `kalibStepperMl[]` diubah dari UI
  lalu berhenti; `sensor_extra.h` adalah kode mati (tidak di-`#include`).
  Akibatnya akurasi dosing saat ini nol.
- **Logika ml di `PROCESS.md` §6 terbalik** — `butuh = target − tersedia` dengan
  guard `butuh > tersedia` sudah diganti: **dosis = resep**, sensor hanya
  mengesahkan ketersediaan.
- **Watchdog 90 s konstan** bisa lebih pendek dari dosis yang sah (300 ml butuh
  230–1150 s pada laju sekarang) → diganti watchdog dinamis dari laju nyata.
- **"Deteksi macet" mustahil** — `jalanLangkah[]` naik tiap pulsa tanpa umpan
  balik motor. Dipecah: `DeteksiJekot` (nyata) vs `DeteksiMacet` (opsional,
  butuh TMC2209 StallGuard).
- **Klaim mikrostep 1/36 tidak mungkin** (maks 1/32) → dipakai
  `STEPPER_STEP_PER_REV` eksplisit.
- **NVS:** usulan key baru `"pompa"` di `PROCESS.md` §6.1task 2.2 diganti
  `DataKalib` versi 2 (satu blob, satu CRC) agar tidak ada state setengah
  ter-update.

**Dokumen yang disinkronkan**
- `PROCESS.md` — §2.4/2.5/2.7/2.8 & §6.2 dikoreksi, konstanta §4 ditulis ulang,
  2 kode error baru (`ERR_SUMBER_HABIS`, `ERR_POMPA_KALIB`), catatan revisi
  ditambahkan.
- `PROGRESS.md` §3 — dirombak jadi 3 sub-bagian (7 ✅ / 10 ⚠️ / 24 ⬜); §10 dan
  §11 diperbarui. **Tabel Ringkasan dihitung ulang** — angka lama (83/11/72)
  tidak cocok dengan isi checklist-nya sendiri.
- `SUMMARY.md` — versi v0.5.2 → v0.5.4, DRV8825 → TMC2209, daftar file
  disinkronkan dengan kondisi aktual, `sensor_extra.h` ditandai kode mati.
- `README.md` — Arduino Uno/Nano + EEPROM → ESP32-S3 + NVS; `aktuator.h` yang
  sudah tidak ada dihapus dari daftar; ditambah tabel dokumentasi.
- `GUIDE.md` — struktur repository diperbarui (`aktuator.h` dihapus, deskripsi
  `motor.h` diperbaiki dari "Motor DC" menjadi stepper, dokumen baru
  ditambahkan).
- `etching-otomatis.md` — banner penanda superseded per bagian.

---

## [2026-10-14] v0.5.4 — Kerangka 9 halaman Kalibrasi (menu 10 item, tanpa logika test)

Tahap ini **hanya kerangka + penyimpanan**: semua item Kalibrasi sudah punya
halaman asli (tidak ada lagi "Halaman ini belum tersedia"), nilainya bisa
diedit dan tersimpan. **Logika test/auto-kalibrasi belum ada** — itu tahap
berikutnya.

**File baru `sensor_extra.h`**
- DS18B20 (GPIO10) via OneWire + DallasTemperature: `suhuInit()`, `suhuBaca()`.
  `requestTemperatures()` blocking ~750ms → **hanya dipanggil dari halaman
  Kalib Sensor Suhu** (1x per 2.5 detik), tidak pernah di `loop()`.
- MQ-135 (GPIO9) via `analogRead`: `udaraInit()`, `udaraBaca()`.
- Nilai kalibrasi aktuator: `langkahPerMlX1000[3]`, `pompaDcMlPerDtkX10[2]`,
  `agitasiDutyMin`, `fanPwmMin`, `mq135Baseline`, `mq135Margin`,
  `ds18b20OffsetX10` + status nyala `kalibAgitasiNyala` / `kalibFanNyala`.
- `pompaTerapkanKalibrasi()` menyalin `langkahPerMlX1000` → `motor.h` (dipakai
  `pompaMulai`), jadi angka yang diedit **langsung dipakai proses dosing**.

**Halaman baru (8, untuk 9 item menu)**
| Item | Halaman |
|---|---|
| Motor HCl / H2O2 / Air | `Pilih Pompa` → `Kalib Pompa` (Langkah/ml per pompa) |
| Motor Air Sabun / Limbah | `Pilih Pompa DC` → `Kalib Pompa DC` (ml per detik) |
| Motor Agitasi | Duty PWM + status nyala/mati |
| Exhaust Fan | PWM + status nyala/mati |
| Sensor Udara | Baseline / Margin / bacaan ADC / status NORMAL |
| Sensor Suhu | Offset / bacaan / suhu terapkan |

- `L_KALIB_POMPA` + `L_KALIB_DC` ditambahkan, `JUMLAH_LIST` 6 → 8.
- `ui.h`: helper `kalibBaris()` + `kalibFooter()` supaya semua halaman
  parameter punya tampilan sama.

**Tombol**
- Pompa / Pompa DC: `KIRI/KANAN` ubah nilai, `ENTER` = toast "Test: belum"
- Agitasi & Fan: `KIRI/KANAN` ubah, `ENTER` = **nyala/mati** motor saat itu
  (langsung ada feedback, selalu dimatikan lagi saat `BACK`)
- MQ-135: `ENTER` di baris Baseline = tetapkan bacaan sekarang jadi acuan
- DS18B20: `KIRI/KANAN` geser offset ±0.1 °C
- Semua halaman: `BACK` + dialog simpan (`simpanKalib()` / `muatKalib()`)

**Penyimpanan (`storage.h`)**
- Blob baru `"kal"` (`DataKalib`, versi 1 + CRC) berisi semua nilai di atas
  dengan validasi rentang; data rusak/versi beda → nilai bawaan.

**`dcmotor.h`**
- Tambah `agitasiSetDuty(duty 0..255)` dan `fanSetAt(pwm 0..255)` — sebelumnya
  hanya ada `agitasiSet(speed 1..5)` dan `fanSet(bool)` yang tidak bisa dipakai
  untuk slider kalibrasi.

**Belum dikerjakan (tahap berikutnya)**
- Pompa stepper & pompa DC: tombol test (jalan 50 ml / 10 detik, hitung
  langkah otomatis, auto-stop 10 detik, E-stop, batal via BACK)
- DS18B20: kalibrasi cepat dengan termometer pembanding
- MQ-135: pengingat preheat 60 detik sebelum set baseline

---

## [2026-10-14] v0.5.3 — Level Cairan murni monitoring + halaman pilih tangki

**Level Cairan benar-benar read-only (`.ino`)**
- `ENTER` **tidak lagi** membuka Kalibrasi Sensor Level. Halaman ini khusus
  monitoring isi jerigen, jadi tidak ada tombol yang bisa mengubah data
  (%)/ml saat sedang memantau.
- `ENTER` sekarang hanya menampilkan toast info `raw <n>  d<n>` (angka mentah
  untuk troubleshooting), tanpa pindah halaman.
- Tombol: ATAS/BAWAH pindah baris · BACK keluar · ENTER info.

**Halaman baru: pilih tangki (`data.h`, `ui.h`, `.ino`)**
- `HAL_KALIB_SENSOR_LIST`: sebelum mengedit parameter, user memilih tangki.
  Sebelumnya menu Kalibrasi -> Sensor Level langsung membuka tangki yang
  terakhir dilihat, sehingga tidak ada cara memilih tangki dari menu itu.
- Tampilan memakai `gambarList()` yang sama dengan menu lain (terpilih di
  tengah, scroll sendiri), isi baris `HCl    72% 1800ml` / `uncal`.
- `L_KALIB_SENSOR` ditambahkan, `JUMLAH_LIST` 5 → 6.

**Alur & berlaku-nya data**
```
Menu Utama -> Level Cairan              (monitoring % + ml, live)
Menu Utama -> Kalibrasi -> Sensor Level -> pilih tangki -> parameter
```
- Setelah disimpan, nilai langsung dipakai di RAM; Level Cairan sudah
  menampilkan angka baru karena refresh tiap 50ms — tidak perlu tombol
  "terapkan"/muat ulang.
- `HAL_KALIB_SENSOR_LIST` ikut live-refresh (baris % ikut bergerak).

---

## [2026-10-14] v0.5.2 — Menu Kalibrasi (10 item) + Kalib Sensor Level, Level Cairan jadi read-only

**Menu Kalibrasi (`data.h`, `ui.h`, `.ino`)**
- Menu utama item 4 tidak lagi `HAL_PLACEHOLDER`, tapi halaman `HAL_KALIB_LIST`
  berisi 10 item: Sensor Level, Sensor Suhu, Sensor Udara, Motor HCl, Motor H2O2,
  Motor Air, Motor Air Sabun, Motor Limbah, Motor Agitasi, Exhaust Fan.
- Scroll gratis dari `gambarList()` yang sudah ada: item terpilih selalu di
  tengah, item lain bergeser sendiri (pola sama dengan menu Konfigurasi).
  `L_KALIB` ditambah, `JUMLAH_LIST` 4 → 5.
- Tombol seragam di semua item: ATAS/BAWAH pindah, KIRI/KANAN ubah nilai,
  ENTER jalankan aksi, BACK keluar + dialog simpan.
- Item selain "Sensor Level" → `HAL_PLACEHOLDER` dengan judul item tersebut
  (Fase 3 & 4 belum dikerjakan).

**Kalib Sensor Level (`sensor.h`, `ui.h`, `.ino`)**
- Halaman baru `HAL_KALIB_SENSOR`, 3 baris nilai + footer petunjuk tombol:
  `Kapasitas` (ml) · `Baseline 0%` · `Delta 100%`.
  `KIRI/KANAN` ubah Kapasitas (±50 ml) dan Delta 100% (±10, koreksi manual),
  `ENTER` pada baris Baseline/Delta memulai pengambilan sampel.
- **Kalibrasi pakai rata-rata 40 sampel (~2 detik)**, bukan 1 sampel momentan.
  Selama sampel diambil, halaman menampilkan progress bar di footer dan
  progres di header; tombol lain diabaikan (BACK = batal).
- KANAN/100% ditolak kalau delta penuh < `TOUCH_DELTA_MIN*2` (tangki kemungkinan
  belum penuh) — dicatat di Serial, tidak disimpan.

**Persen tidak lagi merayap sendiri (`sensor.h`)**
- Ditambah `sensorKering[]`: baseline 0% yang **fixed** (diisi saat kalibrasi).
  Persen dihitung dari `sensorKering`, bukan dari baseline adaptif. Inilah
  akar masalah angka yang berganti-ganti sendiri: baseline adaptif mengejar
  noise (k=8 saat dekat), jadi `delta` jadi noise ganda.
- `sensorBaseline[]` (adaptif) tetap dipakai untuk deteksi biner AKTIF/tidak.
- `sensorKapasitasMl[]` → kolom ml: `ml = persen × kapasitas / 100`.

**Fix bar baris terpilih (`ui.h`)**
- Pita putih baris terpilih sekarang setinggi **1 baris penuh** (teks + bar = 12px),
  bukan cuma 8px baris teks. Sebelumnya bar progress baris terpilih digambar
  **hitam di atas background hitam** → tidak kelihatan, dan karena baris
  terpilih-lah yang bergeser saat scroll, gejalanya "bar hilang saat discroll".
- Lebar pita diperlebar `x=0..115` → `x=0..122` supaya seluruh bar di atas pita.
- Efek sampingnya bagus: pada baris terpilih, bagian bar yang kosong tetap putih
  dan yang terisi hitam → progress-nya kelihatan pembedaannya.

**Level Cairan jadi read-only (`ui.h`, `.ino`)**
- Kolom kanan jadi `72% 1800 ml` (ml hanya kalau Kapasitas sudah diisi).
- Tombol KIRI/KANAN (set 0%/100% langsung) **dihapus** dari sini supaya tidak
  salah pakai saat monitoring; `ENTER` sekarang membuka Kalib Sensor Level
  untuk tangki yang sedang dipilih.

**Penyimpanan (`storage.h`)**
- `FORMAT_VERSI_AMBANG` 2 → 3: `DataAmbang` tambah `kering[5]` + `kapasitasMl[5]`.
  Data versi lama otomatis dianggap belum terkalibrasi → tampil `uncal`.

---

## [2026-10-14] v0.5.1 — Halaman "Level Cairan": nama + persen, bar progress, scroll

**Tampilan (`ui.h` `gambarLevel`):**
- Tabel debug (P11/raw/delta/AKTIF/counter) diganti: satu baris per tangki dengan
  **nama (kiri) + persentase (kanan) sejajar**, lalu **bar progress 3px** di
  bawahnya (x=2..121). Baris terpilih dibalik (latar putih, teks hitam,
  pita selebar x=0..115 supaya tidak menempel scrollbar).
- `BARIS_LVL = 12` = teks 8 + bar 3 + **jeda 1** → **3 baris utuh** muat di area
  konten 37px, DAN antar baris ada 1px pemisah. Tanpa jeda, pita putih baris
  terpilih menempel di bar milik baris tetangga dan saat halaman di-scroll
  terlihat seperti "bar tertutup putih".
- Baris yang terpotong di tepi layar di-culling, jadi tidak ada teks/bar separuh.
- Scrollbar tipis di x=126 + auto-scroll supaya baris terpilih selalu terlihat.
- Tangki yang belum dikalibrasi tampil `uncal` + bar kosong (bukan `0%`).

**Persen & kalibrasi (`sensor.h`, `config.h`):**
- `sensorPersen[]` = `|raw - baseline| / deltaPenuh * 100`, difilter
  (rata-rata bergerak /4) supaya bar tidak bergetar.
- `sensorDeltaPenuh[]` + `sensorKalibValid[]`: penuh dikalibrasi user, bukan
  asumsi. Default `TOUCH_DELTA_PENUH_AWAL 600` hanya sampai tangki dikalibrasi.
- `levelSetNol(i)` / `levelSetPenuh(i)`: `deltaPenuh` minimal `TOUCH_DELTA_MIN*2`
  supaya persen tidak meloncat.
- Mode digital (`SENSOR_TOUCH_READ=0`): persen hanya 0 atau 100.
- `sensorUpdate()` sekarang refresh tampilan tiap 50ms selama halaman ini aktif
  (bar ikut bergerak), bukan hanya saat state aktif/tidak berubah.

**Tombol (`.ino` `HAL_LEVEL`):**
- `ATAS/BAWAH` pindah baris terpilih (scroll mengikuti)
- `KIRI` = tangki terpilih 0% (harus kosong), `KANAN` = 100% (harus penuh)
- `ENTER` = tampilkan `nama raw delta` lewat toast + reset counter
- `BACK` = keluar; kalau ada perubahan, dialog simpan → ENTER simpan ke NVS,
  BACK buang (kembali ke nilai tersimpan lewat `muatAmbang()`)

**Penyimpanan (`storage.h`):**
- `FORMAT_VERSI_AMBANG` 1 → 2, `DataAmbang` tambah `deltaPenuh[5]` + `valid[5]`
  (data versi 1 otomatis dianggap belum terkalibrasi → tampil `uncal`).
- `sensorKalibDimuat` mencegah `sensorInit()` menimpa kalibrasi dari NVS.

---

## [2026-10-13] v0.8.3 — Fix critical bugs, non-blocking buzzer, dua tone baru

**Perbaikan Kritis:**
- **Agitator tidak resume setelah pause** (`process.h`): saat unpause, agitasi tidak
  di-restart → etching tidak rata. Fix: restore speed agitasi sesuai fase aktif.
- **Pause tidak extend fase duration** (`process.h`): `faseMulaiMs` tidak disesuaikan
  saat unpause → fase terpotong durasi jeda. Fix: `faseMulaiMs += jedaDurasi`.
- **Blocking delay(2000) di kalibrasi agitasi** (`.ino`): E-stop tidak responsif
  selama 2 detik. Fix: non-blocking pakai `agitasiTestMulaiMs` + cek di `loop()`.

**Prioritas Tinggi:**
- **Waste estimate salah** (`process.h`): `mlH2O` dihitung 3x → estimasi limbah
  terlalu besar, proses bisa gagal start. Fix: `mlHCl + mlH2O2 + mlH2O + 100 + 100`.
- **Cek sabun salah** (`process.h`): `kebutuhan[3] = p.mlH2O` — sabun dicek seolah-olah
  air. Fix: minimum tetap 100ml.
- **E-stop ISR memanggil digitalRead** (`aktuator.h`): ISR terlalu lama, berpotensi
  crash. Fix: ISR hanya set flag.
- **Buzzer blocking** (`aktuator.h`): `buzzerNada()` pakai `delay()` → seluruh sistem
  beku saat tone bermain. Fix: non-blocking state machine + `buzzerUpdate()` di loop.
- **stepperTest blocking** (`aktuator.h`): `delayMicroseconds()` selama 1.6 detik untuk
  1000 step. Fix: non-blocking `stepperTestMulai()`/`stepperTestUpdate()`.
- **hitungMl division by zero** (`sensor.h`): dua titik kalibrasi sama → NaN. Fix:
  guard `if (y0 == y1)`.

**Fitur Baru:**
- **`TONE_MULAI`** — nada mulai proses (440→660→880 Hz), dipanggil di `prosesMulai()`.
- **`TONE_PROSES_SELESAI`** — nada selesai proses (523→784→1047 Hz), dipanggil
  menggantikan `TONE_SELESAI` di `FASE_PENDINGINAN`.

**Lainnya:**
- `prosesMulai()`: guard `if (prosesJalan) return`
- `prosesBatal()`: reset `prosesMlTerpompa` & `watchdogSampaiMs`
- `dosisDurasiMs()`: hapus `+1` step (over-dosing)
- `teksTengah()`: fix unused variables
- `config.h`: `FW_VERSI` → v0.8.3

**Backup sebelum perubahan:** `backup/v0.8.2_2026-10-12/`

---

## [2026-10-12] v0.8.2 — "Durasi total" dikembalikan dan berfungsi

**Perubahan:**
- `presetops.h`: `namaField` kembali 11 field — "Durasi total"
  (index 9) dan "Nama" (index 10)
- `presetops.h` (`ubahField`): field "Durasi total" KIRI/KANAN mengubah
  **±10 detik** — durasi total tidak bisa diubah langsung karena
  dijumlah dari seluruh fase, jadi levers-nya adalah fase etching
  (`agitasiDetik`, dibatasi 1–180 s)
- `ui.h` (`formatField`): kolom "Durasi total" menampilkan **estimasi
  mesin** `hitungTotalDurasiAktual()` dalam `mm:ss`, jadi selalu sesuai
  dengan apa yang akan dijalankan (bukan lagi angka yang tidak terpakai)
- `config.h`: `FW_VERSI` → v0.8.2

**Catatan:** perubahan "Durasi total" dan "Durasi agitasi" sama-sama
mengubah `agitasiDetik` — yang pertama langkah 10 detik, yang kedua
1 detik, supaya durasi etching bisa diatur kasar dan halus.
Column "Durasi total" hanya-baca (KIRI/KANAN tidak berlaku padanya? —
tidak, berlaku lewat agitasi 10 detik).

**Backup sebelum perubahan:** `backup/v0.8.1_2026-10-12/`

---

## [2026-10-12] v0.8.1 — Hapus field "Durasi total" yang mati di layar Edit

**Perubahan:**
- **Bug:** field "Durasi total" masih bisa diedit di halaman Edit dan
  menyimpan toast "Tersimpan", tetapi sejak v0.7.1 nilainya tidak lagi
  dibaca firmware apa pun (durasi proses dihitung dari Volume +
  Durasi agitasi) → hasil edit tidak berefek apa pun
- `data.h`: `namaField` 11 → 10 field; "Durasi total" dihapus,
  "Nama" menjadi index 9 (`JUMLAH_FIELD` = 10)
- `presetops.h`: `ubahField()` — `case 9` (totalDetik) dihapus;
  `case 7` (Durasi agitasi) kini dapat diedit di mode **auto** maupun
  manual (sebelumnya hanya mode manual), sehingga ada levers waktu
  etching yang benar-benar bekerja
- `ui.h` (`formatField`): `case 9` = Nama (bukan durasi total)
- Field `totalDetik` tetap ada di struct `Preset` (kompatibilitas data
  NVS) tapi tidak lagi tampil/diedit
- `config.h`: `FW_VERSI` → v0.8.1

**Cara mengatur durasi sekarang:** ubah **Volume** (menambah durasi
dosing) dan/atau **Durasi agitasi** (menambah durasi etching); hasil
terlihat di halaman Detail pada baris `Etching` dan `Durasi proses`.

**Backup sebelum perubahan:** `backup/v0.8.0_2026-10-12/`

---

## [2026-10-12] v0.8.0 — Halaman Detail Preset: volume total, waktu etching, durasi proses

**Perubahan:**
- `ui.h` (`gambarPresetDetail`) — tampilan baru:
  - header: nama preset (kiri) + ukuran PCB `100x80mm` (kanan)
  - komposisi: baris HCl/H2O2/H2O + bar komposisi
  - **Volume total** = jumlah keseluruhan senyawa (`volumeTotal`)
  - **Etching** = waktu etching `agitasiDetik` format `mm:ss` + `Spd`
  - **Durasi proses** = estimasi mesin (`hitungTotalDurasiAktual`),
    menggantikan `totalDetik` (tebakan preset)
- **Fix scroll tersembunyi:** `terlihat()` kini memakai tinggi glyph
  (8px) sehingga baris terakhir tidak lagi terpotong;
  `maxScroll` di `.ino` memakai `(AREA_BAWAH - HEADER_H)` (sebelumnya
  `SCREEN_HEIGHT - HEADER_H` yang mengabaikan offset 14px → jangkauan
  scroll kurang, baris "Total" tak pernah terlihat);
  `DETAIL_CONTENT_H` 66 → 68, `DETAIL_SCROLL_STEP` 8 → 10
- `config.h`: `FW_VERSI` → v0.8.0

**Backup sebelum perubahan:** `backup/v0.7.1_2026-10-12/`

---

## [2026-10-12] v0.7.1 — Progress bar tidak lagi "stuck" (penyebut bar diperbaiki)

**Perubahan:**
- **Bug**: bar progres membagi `prosesAkumMs` dengan `p.totalDetik`
  (durasi tebakan preset) sementara proses selesai berdasarkan
  `prosesTotalMsAktual` → bar berhenti di ~50% dan tidak pernah 100%
- `ui.h` (`gambarProses`): penyebut bar & baris sisa memakai
  `prosesTotalMsAktual` (estimasi mesin, sama dengan nilai yang dipaksa
  di `FASE_SELESAI`) → bar mencapai 100%; `frac` di-clamp 0…1; sisa
  tidak negatif; header kanan kini `sisa/total` (mis. `02:22/02:22`)
- `process.h`: `hitungTotalDurasiAktual()` menghitung durasi fase
  dosing dari kalibrasi stepper — `dosisDurasiMs()` =
  `ml ÷ mlPerStep × 1.6 ms + 0.3 s` (sebelumnya 30 detik tetap per pompa)
- `config.h`: `FW_VERSI` → v0.7.1

**Catatan:** field `totalDetik` di preset tetap ada untuk tampilan di
halaman Detail, tetapi tidak lagi menentukan bar progres.

**Backup sebelum perubahan:** `backup/v0.7.0_2026-10-12/`

---

## [2026-10-12] v0.7.0 — Kalibrasi berfungsi penuh: flag sensor, input volume stepper, edit kapasitas

**Perubahan:**
- **P0-1 flag terkalibrasi** (`oled_pcb_baruuu.ino`): saat titik 100%
  (index terakhir) di-ENTER, `kalibrasiSensorValid[senyawa] = true` +
  toast "HCl terkalibrasi". Sebelumnya flag ini tidak pernah di-set
  sehingga `hitungMl()` selalu -1, `cekKetersediaan()` melewati
  semua sensor, dan halaman Cadangan tetap "uncal"
- **P0-2 wizard volume stepper** (`.ino`, `data.h`, `ui.h`): ENTER →
  pompa jalan N step → mode input "Volume keluar" (KIRI/KANAN ±5ml)
  → ENTER menghitung `ml/step = volume / step`, menyimpannya ke
  `kalibrasi.stepperMlPerStepX1000[]`, memperbarui `stepper[].mlPerStep`
  agar langsung dipakai dosing; BACK = batal; toast hasil
- **P0-3 edit kapasitas** (`.ino`, `ui.h`): baris "Kapasitas: <n>ml"
  jadi baris pertama halaman sensor (`kalibSensorTitik = -1`);
  KIRI/KANAN mengubah ±50ml (100–5000ml), per senyawa
- Navigasi halaman sensor: ATAS/BAWAH pilih baris; KIRI/KANAN
  ganti senyawa (atau ubah kapasitas bila baris Kapasitas dipilih)
- `ui.h`: switch senyawa menampilkan `*` bila senyawa sudah terkalibrasi
- `config.h`: `FW_VERSI` → v0.7.0

**Backup sebelum perubahan:** `backup/v0.6.1_2026-10-12/`

---

## [2026-10-12] v0.6.1 — Switch senyawa & kapasitas dipin (tidak ikut scroll)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): switch senyawa `[ < HCl > ]` dan
  kapasitas `1000ml` dipindahkan ke area tetap di y=24–33, di atas
  area scroll — sebelumnya ikut ter-scroll dan hilang saat titik
  75%/100% dipilih; 5 titik kini area scroll-nya mulai y=34
  (3 titik terlihat, auto-scroll mengikuti titik terpilih)
- `config.h`: `FW_VERSI` → v0.6.1

**Backup sebelum perubahan:** `backup/v0.6.0_2026-10-12/`

---

## [2026-10-12] v0.6.0 — Offset 14px, 4 baris per halaman

**Perubahan:**
- `config.h`: `OLED_TOP_OFF` 20 → **14** (area logo y=14–63, 50px)
- `ui.h`: dengan 6px ekstra, halaman sekarang muat **4 baris**
  teks per layar (`BARIS_H` tetap 10, jeda 3px dipertahankan):
  - Proses: waktu y=28, bar y=45–52, status y=53 (disebar ulang)
  - Splash: judul 26, versi 46, booting 54 (disebar ulang)
  - Cfg Edit: `EDIT_VISIBLE` 3 → 4
  - Halaman berbasis scroll (Cadangan, Kalib Sensor/MQ-135/
    DS18B20/Agitasi, Tentang) menyesuaikan otomatis
- `config.h`: `FW_VERSI` → v0.6.0

**Backup sebelum perubahan:** `backup/v0.5.9_2026-10-12/`

---

## [2026-10-12] v0.5.9 — Jarak baris lebih lapang (BARIS_H 8→10)

**Perubahan:**
- `ui.h`: `BARIS_H` 8 → **10** — jarak antar baris 1px → 3px
  (teks tidak lagi rapat); 3 baris per layar, sisanya via scroll
- `ui.h`: `uiScrollSet` membulatkan `uiScrollMaks` ke atas ke
  kelipatan `BARIS_H` supaya baris terakhir selalu bisa dimunculkan
- `gambarKalibStepper`: header ringkas 10px + 3 pompa (y=10/20/30)
  + 1 hint (y=37) — muat tanpa scroll
- `gambarError`: `[ENTER] reset` dipindah ke header kanan,
  sisa 3 baris (kode, pesan, Exhaust ON)
- `gambarKalibSensor`: switch senyawa disembunyikan saat ter-scroll
  keluar area
- `gambarCfgEdit`: `EDIT_VISIBLE` 4 → 3
- `gambarPresetDetail`: jarak baris `dy` 8 → `BARIS_H`
- Kalib DS18B20 & Agitasi: ditambahkan scroll (4 baris → 3 terlihat)
- `config.h`: `FW_VERSI` → v0.5.9

**Backup sebelum perubahan:** `backup/v0.5.8_2026-10-12/`

---

## [2026-10-12] v0.5.8 — Offset 20px + sistem scroll halaman (font tak dikecilkan)

**Perubahan:**
- `config.h`: `OLED_TOP_OFF` 10 → **20**
- `ui.h`:
  - `BARIS_H` = 8 (jarak baris standar font size 1, tidak dipadatkan lagi)
  - sistem scroll halaman: `uiScrollSet/uiScrollAtur/uiScrollKe` +
    `gambarScrollbar()`; scroll di-reset otomatis saat pindah halaman
  - **Scroll manual (ATAS/BAWAH):** Cadangan, Kalib MQ-135,
    DS18B20, Agitasi, Tentang
  - **Auto-scroll:** Kalib Sensor — ATAS/BAWAH tetap memilih titik,
    tampilan menggulir agar titik terpilih selalu terlihat
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 1 hint (muat tanpa scroll)
  - Proses: "sisa" pindah ke header; waktu y=33, bar 49–56,
    status (fase + %/ml) y=57
  - Cfg Edit: `EDIT_VISIBLE` 5 → 4 (gulir dengan ATAS/BAWAH)
- `oled_pcb_baruuu.ino`: ATAS/BAWAH = scroll pada halaman tersebut;
  di Cadangan, KIRI/KANAN sekarang memilih baris
- `config.h`: `FW_VERSI` → v0.5.8

**Backup sebelum perubahan:** `backup/v0.5.7_2026-10-12/`

---

## [2026-10-12] v0.5.7 — Geser seluruh UI 10px ke bawah (baris atas OLED rusak)

**Perubahan:**
- `config.h`: tambah `OLED_TOP_OFF 10`
- `ui.h`: seluruh UI digeser 10px ke bawah memakai offset
  terpusat — `TY(y)` untuk bentuk, `teksTengah/teksKanan/teksKiri`
  untuk teks (koordinat logis, y=0 = baris pertama yang terlihat);
  area logo jadi 54px (y=10–63) dan semua halaman dipadatkan agar
  tetap muat:
  - Kalib Sensor: header ringkas 10px + switch kotak putih penuh
    (y=20) + 5 titik jarak 7px (y=29/36/43/50/57)
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 3 hint (semua masuk layar)
  - Kalib MQ-135: 4 baris data + 2 hint; DS18B20 & Agitasi:
    2 baris data + 2 hint (semua hint dipendekkan &lt;=128px)
  - Proses: waktu/sisa/bar/fase dikompakan ke y=24/41/49/57
  - Cadangan: 5 baris jarak 8px; Cfg Edit: `EDIT_ROW_H` 10→8
  - Tentang: tambah baris "Sensor: GPIO 11/12/13/14/3"
- `config.h`: `FW_VERSI` → v0.5.7
- Fix compile: `teksKiri(F("PCB max"))` → `teksKiri("PCB max")` — di
  Arduino-ESP32 3.x `F()` menghasilkan `const __FlashStringHelper*`
  yang tidak otomatis bisa jadi `const char*`

**Backup sebelum perubahan:** `backup/v0.5.6_2026-10-12/`

---

## [2026-10-12] v0.5.6 — Kembali ke layout v0.5.4 (switch y=14)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): dikembalikan ke
  layout v0.5.4 — switch senyawa (kotak putih penuh
  `[ < HCl > ]`) di y=14 di bawah header, 5 titik
  kalibrasi di bawahnya (y=25/33/41/49/57, jarak 8px).
  Layout v0.5.5 (switch di y=50, titik di atas)
  dicoba dan tidak dipakai; tersimpan di
  `backup/v0.5.5_2026-10-12/`
- `config.h`: `FW_VERSI` → v0.5.6

**Backup sebelum perubahan:** `backup/v0.5.5_2026-10-12/`

---

## [2026-10-12] v0.5.5 — Switch senyawa dipindahkan ke y=50 (bawah)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susunan halaman diubah —
  5 titik kalibrasi di atas (y=13/20/27/34/41, jarak 7px),
  selektor senyawa (kotak putih penuh `[ < HCl > ]`)
  dipindahkan ke **y=50 dari atas** sesuai permintaan,
  kapasitas "1000ml" tetap di kanan switch; konstanta
  `Y_SWITCH` dipakai supaya mudah diatur
- `config.h`: `FW_VERSI` → v0.5.5

**Backup sebelum perubahan:** `backup/v0.5.4_2026-10-12/`

---

## [2026-10-12] v0.5.4 — Switch senyawa lebih menonjol (kotak putih penuh)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): selektor senyawa dari kotak
  berbingkai jadi **kotak putih penuh** (teks hitam) — lebih
  menonjol; digeser 1px ke bawah (y=14, tinggi 10px,
  baris titik tetap y=25/33/41/49/57, semua muat)
- `config.h`: `FW_VERSI` → v0.5.4

**Backup sebelum perubahan:** `backup/v0.5.3_2026-10-12/`

---

## [2026-10-12] v0.5.3 — Layout halaman kalibrasi sensor level (header + selector)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susun ulang layout atas yang
  sebelumnya padat — header jadi bar status ("Kalibrasi *" kiri,
  "Raw xxxx" kanan); senyawa ditampilkan dalam kotak berbingkai
  `[ < HCl > ]` (jelas sebagai switch, KIRI/KANAN) dengan kapasitas
  "1000ml" di kanannya; 5 titik kalibrasi mulai y=25 (semua
  terlihat, jarak lebih lega)
- `config.h`: `FW_VERSI` → v0.5.3

**Backup sebelum perubahan:** `backup/v0.5.2_2026-10-12/`

---

## [2026-10-12] v0.5.2 — Perbaikan halaman kalibrasi sensor level

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): layout diperbaiki — titik 75%/100% &
  "Raw now" sebelumnya di bawah layar; kini: header `Kalib: <senyawa>`
  (senyawa jelas kelihatan, `< >` = ganti dgn KIRI/KANAN), baris info
  `Kap 1000ml Raw <nilai>`, 5 titik jarak 8px (semua terlihat),
  titik terpilih di-highlight
- `oled_pcb_baruuu.ino`: navigasi halaman kalibrasi sensor ditukar —
  KIRI/KANAN = ganti senyawa, ATAS/BAWAH = scroll titik kalibrasi
  (sebelumnya terbalik)
- `storage.h`: kapasitas default tangki 500 ml → 1000 ml
- `config.h`: `FW_VERSI` → v0.5.2

**Backup sebelum perubahan:** `backup/v0.5.1_2026-10-12/`

---

## [2026-10-12] v0.5.1 — Migrasi sensor level ke GPIO langsung & pin TB6612FNG

**Perubahan:**
- `config.h`: hapus `MPR121_ADDR`; tambah `PIN_SENSOR_HCL/H2O2/H2O/SABUN/LIMBAH`
  (GPIO 11/12/13/14/3); pin TB6612FNG jadi 15 (agitasi), 16 (sabun), 17 (limbah),
  18 (fan); hapus `PIN_TB_STBY` (STBY di-tie 3.3V di hardware); tambah
  `SENSOR_ANALOG`, `SENSOR_ACTIVE_LOW`, `SENSOR_HYST`; `FW_VERSI` → v0.5.1
- `sensor.h`: ditulis ulang — baca 5 sensor langsung dari GPIO (analog ADC 12-bit
  via `analogRead`, atau float switch digital), rata-rata 4 sampel + histeresis;
  tambah `sensorKosong()` & `sensorPenuh()`
- `aktuator.h`: `pompaDcInit()` tidak lagi mengatur STBY (tie 3.3V)
- `process.h`: watchdog timeout dosing (estimasi 2× durasi langkah + 30 s); dosing
  error `SUMBER_HABIS` kalau sumber habis saat dosing; drain berhenti lebih awal
  saat limbah penuh; tracking ml terpompa; dosing dengan ml=0 langsung dilewati
  (perbaikan hang)
- `ui.h`: layar proses menampilkan ml terpompa/target saat dosing; halaman Tentang
  tidak lagi mencantumkan alamat 0x5A
- `storage.h`: default titik kalibrasi disesuaikan ke skala ADC 12-bit
- `oled_pcb_baruuu.ino`: pesan error sensor level (bukan MPR121)
- Fix compile: `analogSetAttenuation(ADC_ATTEN_DB_11)` dihapus (enum
  tidak tersedia di Arduino-ESP32 3.3.11); pakai default 11 dB

**Asumsi (dari "Pertanyaan Terbuka" dokumen):**
- Sensor level = analog (default `SENSOR_ANALOG=1`); ubah ke 0 untuk float
  switch digital (`SENSOR_ACTIVE_LOW=1` = LOW saat penuh)
- STBY TB6612FNG di-tie 3.3V (GPIO15 dipakai untuk agitasi)
- MS1/MS2/MS3 DRV8825 di-hardwire; `STEPPER_MICROSTEP` tetap 16 — dokumen
  menyebut 1/36, tapi DRV8825 maksimum 1/32; perlu konfirmasi hardware
- Drain "tangki proses kosong" belum terdeteksi (tidak ada sensor tangki proses):
  drain tetap pakai timeout + deteksi limbah penuh

**Backup sebelum perubahan:** `backup/v0.5.0_2026-10-12/`

---

## [2026-10-12] v0.5.0 — Baseline awal (kondisi sebelum migrasi)

Snapshot kondisi firmware saat ini, dicatat sebagai baseline sebelum perubahan lanjutan.

**File:** `oled_pcb_baruuu.ino` + `aktuator.h`, `config.h`, `data.h`, `input.h`, `presetops.h`, `process.h`, `sensor.h`, `storage.h`, `ui.h`

**Fitur yang sudah ada:**
- UI 6 tombol (PCF8574 @0x20) + OLED SSD1306 128×64 (@0x3C), animasi list 200 ms
- Preset (maks 10): tambah/edit/duplikat/hapus; mode dosing auto (47/24/29 %) atau manual; volume 50–1000 ml; mode agitasi auto/manual; speed agitasi 1–5
- Proses otomatis 16 fase: cek level → dosing HCl/H2O2/H2O (stepper non-blocking) → jeda campur → etching (agitasi) → drain → bilas sabun → bilas air → pendinginan → selesai
- Kalibrasi 5 menu: sensor level 5 titik, stepper ml/step, MQ-135 baseline, DS18B20 offset, agitasi duty min
- Penyimpanan NVS (Preferences) + CRC untuk preset & kalibrasi, format versi 2
- E-stop (GPIO8) via interrupt, abort proses, halaman ERROR
- Buzzer nada, LED strip, exhaust fan

**Diketahui belum sesuai `etching-otomatis.md` (rencana lanjutan):**
- Sensor level masih MPR121 (@0x5A), belum GPIO langsung 11/12/13/14/3
- Pin TB6612FNG masih lama: agitasi 11, limbah 12, sabun 13, fan 14, STBY 15
- `STEPPER_MICROSTEP` = 16 (dokumen: 1/36 → 7200 step/putaran)
- Drain hanya timeout tetap (15 s/10 s), belum berdasarkan level tangki proses kosong / limbah penuh
- Belum ada watchdog timeout per fase (dosing bisa hang kalau pompa macet)
- Layar proses belum menampilkan ml terpompa
- DS18B20 masih placeholder (belum pakai OneWire)
- `HAL_PLACEHOLDER` masih ada tapi tak terpakai

**Backup:** `backup/v0.5.0_2026-10-12/`
