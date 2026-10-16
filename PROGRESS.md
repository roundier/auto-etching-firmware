# PROGRESS — Checklist Implementasi Firmware Etching Otomatis

Firmware: **Etching v0.5.0** · Board: **ESP32-S3 DevKitC**
Rencana teknis lengkap: `PROCESS.md`

Legenda: ✅ **Sudah ada & berfungsi** · ⚠️ **Ada tapi belum fungsional/terpakai** · ⬜ **Belum ada**

---

## 1. GPIO & Hardware

- [ ] ⬜ Update pin map stepper ke TMC2209 (`config.h`): STEP HCl=38, DIR HCl=48, STEP H2O2=40, DIR H2O2=39, STEP Air=42, DIR Air=41, TMC_EN=1, TMC_PDN=2
- [ ] ⬜ Hapus `PIN_SLP1/2/3`, tambahkan `PIN_TMC_EN` + `PIN_TMC_PDN`
- [ ] ⬜ Konfirmasi apakah `EN`/`PDN` (GPIO 1 & 2) shared 3 channel atau per-channel
- [ ] ⬜ Konfirmasi step/rev mikrostep TMC2209 (untuk hitung `langkahPerMl`)
- [x] ✅ I2C SDA=4, SCL=5, 400 kHz (`config.h:5-7`, `.ino:568`)
- [x] ✅ OLED SSD1306 0x3C 128×64 terinisialisasi (`.ino:586`)
- [x] ✅ PCF8574 0x20 — 6 tombol, inisialisasi tulis `0xFF` (`.ino:571`)
- [x] ✅ Error I2C dideteksi & dicatat ke Serial (`.ino:571,586`)
- [x] ✅ 5 GPIO sensor level (11/12/13/14/3) `INPUT` (`sensor.h:37`)
- [x] ✅ 4 PWM TB6612FNG (15/16/17/18) (`dcmotor.h:10`)
- [x] ✅ Buzzer pasif GPIO 6 via LEDC (`buzzer.h:14`)
- [x] ✅ DS18B20 di GPIO 10, 3 sensor pada satu bus 1-Wire (`sensor.h:116`)
- [x] ✅ MQ-135 analog GPIO 9 + divider (`config.h:99`, `sensor.h:210`)
- [x] ✅ E-stop GPIO 8 `INPUT_PULLUP` (`.ino:582`)
- [ ] ⬜ **LED status / LED strip (GPIO 7)** — `#define` ada, tidak pernah dipakai kode
- [ ] ⬜ RGB WS2812 indikator per sensor (rencana awal GPIO 48 — kini bentrok dengan DIR HCl)

---

## 2. Modul UI (`ui.h`, `.ino`, `input.h`, `data.h`)

- [x] ✅ Menu utama 4 item: Pilih Preset / Konfigurasi / Level Cairan / Kalibrasi (`data.h:38`)
- [x] ✅ Header + list dengan animasi scroll (smooth) (`ui.h:47-94`)
- [x] ✅ Toast pesan singkat (`ui.h:833-848`)
- [x] ✅ Dialog konfirmasi (simpan/batal/hapus) (`ui.h:279`)
- [x] ✅ Indikator scroll + panah atas/bawah
- [x] ✅ Halaman Daftar Preset + Detail Preset (scroll 5 px)
- [x] ✅ Halaman Konfigurasi: List → Aksi (Edit/Duplikat/Hapus) → Edit 9 field
- [x] ✅ Halaman Level Cairan: 5 sensor, persen + bar progres
- [x] ✅ Halaman Proses: nama preset, timer MM:SS, progress bar, sisa waktu, status fase
- [x] ✅ 6 tombol PCF8574 + debounce 40 ms + blocking first-press (`input.h`)
- [x] ✅ Navigasi ATAS/BAWAH/KIRI/KANAN/ENTER/BACK di semua halaman
- [x] ✅ Render dirty-flag + interval 20 ms (tidak render idle) (`.ino:658`)
- [x] ✅ Tombol dibaca ulang setelah `oled.display()` (`.ino:671`)
- [x] ✅ Kompensasi baris atas OLED rusak (`UI_Y_OFFSET` = 15 px) (`ui.h:13-46`)
- [x] ✅ Konversi suhu DS18B20 non-blocking (`setWaitForConversion(false)`) (`sensor.h:138`)
- [ ] ⬜ **Halaman ALARM baru** (`HAL_ALARM`) untuk semua kondisi gagal
- [ ] ⬜ Layar proses: progress **per fase** (bukan persen total)
- [ ] ⬜ Layar proses: panel ml terpompa per bahan (HCl/H2O2/H2O)
- [ ] ⬜ Layar proses: ikon status aktuator (agitasi/drain/fan/soap)
- [ ] ⬜ Layar proses: badge peringatan gas MQ-135 / suhu tinggi
- [ ] ⬜ Layar proses: tampilkan siklus agitasi ke berapa
- [ ] ⬜ Badge permanen "E-STOP" di header saat latch aktif
- [ ] ⬜ Layar selesai: ringkasan total ml terpompa + durasi aktual
- [ ] ⬜ Tampilkan nomor versi firmware di OLED (sekarang hanya Serial)
- [ ] ⬜ Level Cairan: status ISI/KOSONG + ambang (define `AMBANG_MARGIN`/`AMBANG_HYST` ada tapi tak terpakai)
- [ ] ⬜ Halaman `HAL_PLACEHOLDER` & `gambarPlaceholder()` — kode mati, tidak pernah dipanggil (`ui.h:336`)
- [ ] ⬜ Halaman Kalibrasi Motor/Agitasi/Fan masih pakai `delay()` 1000–2000 ms → UI macet (`ino:310,316,341`)

---

## 3. Modul Dosing (`process.h` § DOSING, `motor.h`, `presetops.h`)

Spesifikasi rinci: **`dosing-design.md`** — matematika dosing, kalibrasi
gravimetrik, error budget, prime/anti-drip, dan daftar koreksi desain
(vs `PROCESS.md` §6 dan `etching-otomatis.md` §4).

### 3.1 Sudah ada

- [x] ✅ Driver stepper non-blocking (puls STEP tiap `LANGKAH_INTERVAL_US`) (`motor.h:50`)
- [x] ✅ Konversi ml → jumlah langkah (`targetLangkah = ml × langkahPerMl`) (`motor.h:34`)
- [x] ✅ 3 pompa (HCl/H2O2/H2O) menjalankan **paralel** saat fase DOSING (`process.h:28-33`)
- [x] ✅ `pompaMulai()` / `pompaStopAll()` / `pompaSelesai()` tersedia
- [x] ✅ Hitung otomatis %H2O dan ml tiap bahan dari %HCl/%H2O2 + volume (`presetops.h:17`)
- [x] ✅ Validasi rentang semua field preset (`presetops.h:49`)
- [x] ✅ Halaman kalibrasi stepper: uji pompa + set ml/rotasi (`ino:222`)

### 3.2 Ada tapi belum fungsional

- [ ] ⚠️ `langkahPerMl` **hardcode `{1200,1200,1200}`** — sisa asumsi DRV8825 7200 step/rev (`motor.h:14`)
- [ ] ⚠️ `kalibStepperMl[]` diubah dari UI tapi **tidak pernah dipakai** `pompaMulai()` (`data.h:99`, `ino:231`) → akurasi dosing saat ini nol
- [ ] ⚠️ `sensor_extra.h` (isi `langkahPerMlX1000` + `pompaTerapkanKalibrasi()`) **tidak di-`#include` siapa pun** → kode mati (`sensor_extra.h:62-80`)
- [ ] ⚠️ Fase DOSING selesai berdasarkan **fraksi waktu** (10% `totalDetik`), bukan kondisi pompa → pompa bisa meleset ke ETCHING (`process.h:70-75`)
- [ ] ⚠️ Logika "ml dibutuhkan" di `PROCESS.md` §6 terbalik: `butuh = target − tersedia` + guard `butuh > tersedia`
- [ ] ⚠️ Watchdog 90 s bisa **lebih pendek dari dosis yang sah** (300 ml butuh 230–1150 s pada laju sekarang)
- [ ] ⚠️ "Deteksi macet" tidak mungkin bekerja: `jalanLangkah[]` naik tiap pulsa tanpa umpan balik motor
- [ ] ⚠️ `kalibMotorRunning` diset `true` tapi tidak pernah dibaca → uji pompa tak pernah auto-stop (`data.h:111`)
- [ ] ⚠️ Halaman kalibrasi stepper masih memakai `delay()` saat uji pompa (`ino:239`)
- [ ] ⚠️ `sensor_extra.h` tidak tersimpan ke NVS sama sekali (`DataKalib` hanya menyimpan titik sensor level, `storage.h:78-85`)

### 3.3 Perlu dikerjakan (urutan implementasi `dosing-design.md` §10)

**Konstanta & hardware**

- [ ] ⬜ `TMC_MICROSTEP` + `STEPPER_STEP_PER_REV` di `config.h` (ganti asumsi 1/36 yang tidak mungkin)
- [ ] ⬜ Dosing **ditolak** (`ERR_POMPA_KALIB`) selama `pompaValid[i] == false`
- [ ] ⬜ Volume tangki (`VOL_TANGKI_*`) sebagai data, bukan `#define` statis

**Motor (`motor.h`)**

- [ ] ⬜ `langkahPerMl[i] = stepPerRev / mlPerRotasi[i]`, diterapkan setiap kalibrasi disimpan
- [ ] ⬜ `pompaMlTerkira(i)` / `pompaProgres(i)` / `pompaSemuaSelesai()`
- [ ] ⬜ Guard `DOSE_MIN`/`DOSE_MAX` per pompa
- [ ] ⬜ `pompaStopAll()` + `TMC_PDN` LOW sebagai jalan keluar tunggal

**Proses (`process.h`)**

- [ ] ⬜ `FASE_CEK` pre-flight: sensor terbaca, tangki terkalibrasi, pompa terkalibrasi
- [ ] ⬜ Cek ketersediaan cairan → `ERR_LEVEL_KURANG` **sebelum** pompa nyala
- [ ] ⬜ Guard `ERR_SUMBER_HABIS` saat dosing sedang berjalan
- [ ] ⬜ Selesai dosing berbasis kondisi (hapus `faseDariFrac()` untuk dosing)
- [ ] ⬜ Watchdog **dinamis** `dosisWatchdogMs()` dari laju nyata, bukan konstanta 90 s
- [ ] ⬜ `DeteksiJekot` (langkah datar > 3 s) — ganti nama "deteksi macet"
- [ ] ⬜ Prime selang (`PRIME_STEPS`) sebelum dosis dihitung
- [ ] ⬜ Anti-drip: balik `DIR` `ANTI_DRIP_STEPS` setelah pompa selesai
- [ ] ⬜ Jeda = pompa freeze, resume dari langkah tersisa (bukan reset ke 0)
- [ ] ⬜ Volume bilasan lewat jalur dosing terkalibrasi (100 ml → `ML_BILASAN`)

**Preset (`presetops.h`)**

- [ ] ⬜ `hitungUlang()` bulatkan ke terdekat — error pembulatan tidak jatuh seluruhnya ke H2O
- [ ] ⬜ `validasiPreset()` tolak sisa pembulatan > 2 ml

**Kalibrasi & penyimpanan**

- [ ] ⬜ Wizard kalibrasi gravimetrik: target uji → volume aktual (timbangan) → `langkahPerMl`
- [ ] ⬜ Auto-stop uji pompa berbasis `millis()` (ganti `delay()`)
- [ ] ⬜ `DataKalib` versi 2: `mlPerRotasiX100[3]` + `pompaValid[3]` + CRC (satu blob, bukan key baru)
- [ ] ⬜ Simpan **tanggal kalibrasi** + peringatan "Kalibrasi sudah > 30 hari"
- [ ] ⬜ Kalibrasi dilakukan pada kondisi nyata: motor hangat, ketinggian tangki proses sebenarnya

---

## 4. Modul Etching (`process.h` § ETCHING, `dcmotor.h`)

- [x] ✅ Motor agitasi PWM pada speed 1–5 (map 80–255) (`dcmotor.h:22`)
- [x] ✅ Agitasi menyala di fase ETCHING dan RINSING (`process.h:36,44`)
- [x] ✅ Agitasi mati di DRAINING, DOSING, SELESAI, dan saat batal
- [x] ✅ Field `agitasiSpeed` dapat diubah di preset (1–5)
- [x] ✅ Field `agitasiDetik` dapat diubah di preset (1–60 s)
- [ ] ⚠️ `agitasiDetik` **tidak dipakai proses** — hanya tampil di UI (`presetops.h:42`, `ui.h:263`)
- [ ] ⬜ Siklus agitasi periodik: ON N detik → OFF N detik → ulangi
- [ ] ⬜ Exhaust fan otomatis menyala selama ETCHING (asap HCl + panas)
- [ ] ⬜ Exhaust fan menyala selama RINSE_SABUN (busa)
- [ ] ⬜ Ramp-up speed agitasi 1 s (hindari shok motor)
- [ ] ⬜ Proteksi suhu: `sensorSuhu(1)` > 70 °C → turunkan speed; > 85 °C → gagal
- [ ] ⬜ Proteksi gas: `mq135PPM()` > ambang → peringatan di UI
- [ ] ⬜ Auto-stop /pause saat suhu/gas di luar batas
- [ ] ⬜ Simpan &Remember durasi fase etching per preset (budget waktu)

---

## 5. Modul Draining & Rinsing (`process.h` § DRAIN/RINSE)

- [x] ✅ Pompa Limbah (TB6612FNG #2 PWA) PWM on/off (`dcmotor.h:28`)
- [x] ✅ Pompa Limbah menyala di fase DRAINING (`process.h:40`)
- [x] ✅ Pompa Limbah mati saat SELESAI & batal
- [x] ✅ Rinsing: pompa air (stepper #3) 100 ml + agitasi (`process.h:43-45`)
- [x] ✅ `fanSet(false)` saat SELESAI
- [ ] ⚠️ DRAINING/RINSING **selesai berbasis fraksi waktu total preset** (75%/88%/100%), bukan durasi per sub-fase
- [ ] ⚠️ Pompa air **(belum pernah dipakai di proses**, hanya di kalibrasi (`process.h:50,113`)
- [ ] ⬜ Durasi tetap per sub-fase: `T_DRAINING_MS`, `T_RINSE_AIR_MS`, `T_RINSE_SABUN_MS`, `T_DRAIN_AKHIR_MS`
- [ ] ⬜ Guard overfill: `sensorPersen[LIMBAH] > 95%` → stop + alarm
- [ ] ⬜ Pompa air sabun menyala di fase RINSE_SABUN (belum ada sama sekali)
- [ ] ⬜ Deteksi "tangki proses kosong" (opsional, bila sensor ditambahkan)
- [ ] ⬜ Histeresis/debounce level agar pompa tidak thrash
- [ ] ⬜ Volume bilasan air & sab customize dari preset

---

## 6. Modul Emergency Stop & Keselamatan (`process.h` § ESTOP)

- [x] ✅ Baca E-stop GPIO 8 tiap loop (`.ino:634`)
- [x] ✅ E-stop LOW → `prosesBatal()` + toast "E-STOP!" (`.ino:635-638`)
- [x] ✅ E-stop LOW → matikan agitasi, pompaSabun, pompaLimbah, fan, semua pompa (`.ino:639-643`)
- [x] ✅ Semua aktuator mati saat FASE_SELESAI (`process.h:47-53`)
- [x] ✅ Semua aktuator mati saat proses dibatalkan (`.ino:110-116`)
- [x] ✅ Dialog konfirmasi batal saat proses berjalan (`ino:121-141`)
- [x] ✅ Log Serial untuk setiap transisi fase & pembatalan
- [ ] ⬜ Estop dipindah ke **baris pertama `loop()`** (saat ini setelah `tanganiTombol()`, `prosesUpdate()`, `motorUpdate()`, `sensorUpdate()`)
- [ ] ⬜ Latch `estopAktif` — tidak auto-reset saat pin kembali HIGH
- [ ] ⬜ Reset latch hanya lewat BACK di `HAL_ALARM`, dan hanya bila pin HIGH
- [ ] ⬜ Fungsi tunggal `semuaAktuatorMati()` (saat ini 4× duplikasi di 3 tempat)
- [ ] ⬜ Cut power driver stepper (TMC_PDN LOW) saat stop
- [ ] ⬜ `prosesGagal(kode)` sebagai satu-satunya jalan keluar kondisi abnormal
- [ ] ⬜ Timeout/watchdog per fase (DOSING, DRAINING, RINSE)
- [ ] ⬜ Buzzer nada alarm (berulang) berbeda dari nada "selesai"
- [ ] ⬜ Halaman alarm + instruksi pemulihan
- [ ] ⬜ Jeda (`prosesToggleJeda`) ikut mematikan output, bukan hanya timer
- [ ] ⬜ Deteksi kegagalan sensor saat proses (DS18B20/touch) → abort aman
- [ ] ⬜ Fail-safe saat OLED/PCF8574 hilang (hanya E-stop berfungsi, beri peringatan)

---

## 7. Sensor & Kalibrasi (`sensor.h`, `storage.h`)

- [x] ✅ Baca 5 sensor level via `touchRead()` (strip nikel non-contact) (`sensor.h:60`)
- [x] ✅ Moving average window 8 untuk redam noise (`sensor.h:13,59`)
- [x] ✅ Konversi raw → persen dengan clamp 0–100 (`sensor.h:74`)
- [x] ✅ Kalibrasi 0% & 100% per sensor dari UI (`ino:412-460`)
- [x] ✅ Kalibrasi level disimpan ke NVS + CRC + auto-reject bila tidak valid (`storage.h:87`)
- [x] ✅ Baseline default 113000/156000 bila kalibrasi belum ada (`storage.h:120`)
- [x] ✅ Halaman Kalibrasi → 4 submenu (Motor, Sensor Level, Sensor Suhu, Sensor Udara)
- [x] ✅ Kalibrasi MQ-135: rekam acuan udara bersih & kotor (`ino:388-408`)
- [x] ✅ Kalibrasi suhu: pilih sensor per blok + offset ±0,1 °C (`ino:348-373`)
- [x] ✅ Kalibrasi agitasi & fan: uji motor/kipas (`ino:292-344`)
- [x] ✅ Kalibrasi DC: atur duty 0–255 dan catat ml hasil uji (`ino:246-289`)
- [x] ✅ Log diagnostik `[SENSOR]`/`[SUHU]`/`[STORE]` ke Serial
- [ ] ⚠️ **Address DS18B20 hardcode** di `sensor.h:119` — harus cocok address fisik
- [ ] ⚠️ `kalibSuhuAddr[]` & `suhuOffsetX10[]` **tidak disimpan ke NVS** (hilang saat reset)
- [ ] ⚠️ `mq135Normal`/`mq135Kotor` **tidak disimpan ke NVS** dan tidak dipakai proteksi
- [ ] ⚠️ `kalibMotorRunning` diset `true` tapi **tidak pernah dibaca** → motor kalibrasi tak pernah auto-stop (`data.h:103`, `ino:284`)
- [ ] ⚠️ `kalibDuty[]` & `kalibMl[]` tidak disimpan ke NVS
- [ ] ⬜ Simpan semua parameter kalibrasi ke NVS (versi format naik ke 2)
- [ ] ⬜ Auto-stop motor kalibrasi setelah N ms (ganti `delay()`)
- [ ] ⬜ Deteksi sensor touch mati (nilai tidak berubah / selalu 0%)

---

## 8. Penyimpanan & Data (`storage.h`, `presetops.h`, `data.h`)

- [x] ✅ Maks 10 preset (`MAKS_PRESET`)
- [x] ✅ 5 preset default saat pertama kali jalan (`data.h:25`)
- [x] ✅ CRUD: tambah, duplikat, hapus dengan pergeseran array (`presetops.h:83-110`)
- [x] ✅ Nama preset unik otomatis "Preset N" (`presetops.h:71`)
- [x] ✅ Simpan preset ke NVS sebagai satu blob + versi + CRC32 (`storage.h:26`)
- [x] ✅ Muat preset: reject bila versi beda / CRC rusak / data tak valid (`storage.h:45`)
- [x] ✅ Auto-reset ke default bila data korup (`storage.h:70`)
- [ ] ⬜ Simpan kalibrasi pompa (ml/rotasi) ke NVS
- [ ] ⬜ Simpan offset suhu & acuan MQ-135 ke NVS
- [ ] ⬜ Simpan duty/kecepatan motor per kalibrasi ke NVS
- [ ] ⬜ Backup/restore preset via Serial (export/import)

---

## 9. Infrastruktur Firmware

- [x] ✅ Semua modul non-blocking (`loop()` berbasis `millis()`), tanpa `delay()` di jalur proses
- [x] ✅ Pisah logika proses (`prosesUpdate()`) dari render UI
- [x] ✅ Log debug lengkap via `dbg.h` dengan prefix per modul (`[PROSES]`, `[POMPA]`, `[KALIB]`, …)
- [x] ✅ Struktur file modular (`config/input/data/presetops/sensor/storage/motor/dcmotor/buzzer/process/ui`)
- [x] ✅ Backup versi di folder `backup/` per rilis
- [ ] ⬜ Watchdog timer hardware (`esp_task_wdt`) sebagai jaring pengaman
- [ ] ⬜ Deteksi & pemulihan I2C (PCF8574 hang → reset bus)
- [ ] ⬜ Kontrol via Serial (start/stop/preset/status) untuk debug
- [ ] ⬜ Logging proses ke file / upload via WiFi (opsional)
- [ ] ⬜ Cleaning `#define` mati di `config.h`: `INTERVAL_SENSOR_MS`, `SENSOR_AVG_N`, `SENSOR_AKTIF_LOW`, `SENSOR_TOUCH_READ`, `TOUCH_DEBOUNCE`, `TOUCH_DELTA_*`, `TOUTH_KALIB_SAMPEL`, `SENSOR_LOG_PERIOD_MS`, `AMBANG_MARGIN`, `AMBANG_HYST`

---

## 10. Dokumentasi

- [x] ✅ `README.md`, `GUIDE.md`, `SUMMARY.md`, `LOG.md`
- [x] ✅ `etching-otomatis.md` — rencana pengembangan + pin map awal
- [x] ✅ `water-level-sensor-plan.md` — rencana sensor level non-contact
- [x] ✅ `PROCESS.md` — rencana detail 5 modul proses + cara kerja + pin map TMC2209
- [x] ✅ `PROGRESS.md` — checklist status implementasi (dokumen ini)
- [x] ✅ `dosing-design.md` — desain rinci modul dosing: matematika ml→langkah, kalibrasi gravimetrik, error budget, prime/anti-drip, koreksi desain
- [x] ✅ `kontrak-bab0.md` — kontrak antarmuka A ↔ B: blok kode siap tempel untuk `config.h`/`data.h`/`.ino`, API yang tidak boleh diubah, checklist compile per gelombang
- [ ] ⬜ Update `SUMMARY.md` agar sinkron dengan pin map TMC2209 & fitur baru
- [ ] ⬜ Koreksi `PROCESS.md` §6 sesuai `dosing-design.md` §2 (logika ml terbalik, watchdog dinamis, nama "deteksi macet")
- [ ] ⬜ Update `LOG.md` / naikkan versi firmware untuk setiap fitur baru
- [ ] ⬜ Tandai `etching-otomatis.md` §2 & §4 sebagai superseded (`PROCESS.md` §1.2 & `dosing-design.md`)

---

## 11. Uji Hardware yang Masih Diperlukan

- [ ] ⬜ Uji 5 sensor touch di jerican kosong vs terisi (repeatability < 5%)
- [ ] ⬜ Verifikasi address DS18B20 fisik cocok dengan `sensor.h:119`
- [ ] ⬜ **Uji laju step lebih dulu**: 10 000 langkah per pompa, bandingkan waktu aktual vs teoritis, deteksi miss step (`dosing-design.md` §10.1)
- [ ] ⬜ Kalibrasi gravimetrik: 3 pompa × 3 ulangan (50 ml) → repeatability ≤ 3%
- [ ] ⬜ Uji dosis besar 300 ml → error ≤ 5% dari target
- [ ] ⬜ Uji prime & anti-drip: tidak ada cairan keluar sebelum dosing / setelah pompa selesai
- [ ] ⬜ Uji dengan larutan encer (HCl 10%) → error ≤ 5% (viskositas berbeda)
- [ ] ⬜ Uji driver TMC2209 3 channel (STEP/DIR/EN/PDN) dengan pin map baru
- [ ] ⬜ Uji respons E-stop (target < 5 ms dari_LOW ke semua output mati)
- [ ] ⬜ Uji drain: volume larutan terbuang habis dalam `T_DRAINING_MS`
- [ ] ⬜ Uji overfill guard (isi tangki limbah hampir penuh)
- [ ] ⬜ Uji siklus agitasi (ON/OFF berulang) + fan
- [ ] ⬜ Uji stabilitas level readout saat kabel AWG30 30 cm
- [ ] ⬜ **Seluruh uji wajib pakai air biasa dulu**, baru larutan encer

---

## Ringkasan

| Kategori | ✅ | ⚠️ | ⬜ |
|---|---|---|---|
| GPIO & Hardware | 10 | 0 | 6 |
| Modul UI | 15 | 0 | 12 |
| Modul Dosing | 7 | 10 | 24 |
| Modul Etching | 5 | 1 | 8 |
| Modul Draining & Rinsing | 5 | 2 | 6 |
| Modul Emergency Stop | 7 | 0 | 12 |
| Sensor & Kalibrasi | 12 | 5 | 3 |
| Penyimpanan & Data | 7 | 0 | 4 |
| Infrastruktur | 5 | 0 | 5 |
| Dokumentasi | 6 | 0 | 4 |
| Uji Hardware | 0 | 0 | 14 |

**Total: 83 ✅ · 11 ⚠️ · 72 ⬜**

Status keseluruhan: **prototype berfungsi penuh pada jalur happy-path** (menu,
preset, dosing, agitasi, drain, e-stop dasar), tetapi **belum ada proteksi proses**
(watchdog, validasi level, latch e-stop, proteksi suhu/gas) dan **akurasi dosing
belum memakai hasil kalibrasi**.

Angka pada tabel Ringkasan dihitung ulang langsung dari isi checklist —
versi sebelumnya (83/11/72) tidak cocok dengan isi checklistnya.

Kondisi khusus dosing: **10 item ⚠️** — di antaranya fase DOSING yang selesai
berdasarkan fraksi waktu (bukan kondisi pompa), hasil kalibrasi ml/rotasi yang
tidak pernah dipakai, dan `sensor_extra.h` yang merupakan kode mati. Rincian
desain serta urutan perbaikannya ada di `dosing-design.md`.

---

*Dokumen ini harus diperbarui setiap kali ada fitur baru yang selesai.*