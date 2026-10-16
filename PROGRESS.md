# PROGRESS — Checklist Implementasi Firmware Etching Otomatis

Firmware: **Etching v0.5.0** · Board: **ESP32-S3 DevKitC**
Rencana teknis lengkap: `PROCESS.md`

Legenda: ✅ **Sudah ada & berfungsi** · ⚠️ **Ada tapi belum fungsional/terpakai** · ⬜ **Belum ada**

---

## 1. GPIO & Hardware

- [x] ✅ **Migrasi stepper DRV8825 → TMC2209 (v0.6.0)** — pin map baru di `config.h`: `PIN_TMC_EN=1`, `PIN_TMC_PDN=2` (1 pasang, **shared 3 driver**), `PIN_STEP1/2/3 = 38/40/42`, `PIN_DIR1/2/3 = 48/39/41`. `PIN_SLP1/2/3` **dihapus**. Polaritas: `TMC_EN_AKTIF_LOW=1` (EN aktif LOW — kebalikan dari SLP DRV8825), reset pulse PDN `TMC_PDN_RESET_US=5`. `motorInit()` urut: pin mode → `stepperSleep(true)` → pulse reset PDN → `stepperSleep(false)` → `hitungLangkahPerMl()`. Mikrostep **1/16 → `STEP_PER_REV 3200`** (rumus umum `200 × 2^n`). Driver bisa dimatikan total: `stepperSleep(bool)` + `pompaMatikanTotal()` (EN = HIGH) dipakai `semuaAktuatorMati()`. GPIO **21 & 47 kini bebas** (dari `PIN_DIR3`/`PIN_STEP3` lama).
- [x] ✅ **Kalibrasi dosing wajib diulang** — `langkahPerMl` berubah dari basis 7200 (DRV8825 1/36) ke 3200 step/rev (TMC2209 1/16). Nilai kalibrasi lama di NVS blob `"kal3"` **tidak valid** — jalankan ulang `Kalibrasi → Motor HCl/H2O2/Air`.
- [x] ✅ I2C SDA=4, SCL=5, 400 kHz (`config.h:5-7`, `.ino:601-602`)
- [x] ✅ OLED SSD1306 0x3C 128×64 terinisialisasi (`.ino:618`)
- [x] ✅ PCF8574 0x20 — 6 tombol, inisialisasi tulis `0xFF` (`.ino:604`)
- [x] ✅ Error I2C dideteksi & dicatat ke Serial (`.ino:604,618`)
- [x] ✅ 5 GPIO sensor level (11/12/13/14/3) `INPUT` (`sensor.h:36-38`)
- [x] ✅ 4 PWM TB6612FNG (15/16/17/18) (`config.h:79-82`, `dcmotor.h`)
- [x] ✅ Buzzer pasif GPIO 6 via LEDC (`buzzer.h:14-18`)
- [x] ✅ **LED status WS2812 GPIO 7** — file baru `led_strip.h`, 8 mode (Boot/Idle/Proses/Jeda/Selesai/Error/E-stop/Warning), animasi non-blocking di `ledStripUpdate()`, integrasi di `process.h` (`aksiFase`, `prosesToggleJeda`, `prosesBatal`, `prosesKeluarSelesai`) + `.ino` (`setup()` init, `loop()` update, handler E-stop)
- [x] ✅ DS18B20 **x3 pada satu bus 1-Wire GPIO 10** (`PIN_DS18B20` = 10, `config.h`), 3 DeviceAddress hardcode (`sensor.h:119-123`), non-blocking `setWaitForConversion(false)`, offset kalibrasi per blok `suhuOffsetX10[3]` + pilih address `kalibSuhuAddr[3]`, halaman kalibrasi `HAL_KALIB_SUHU` (`ui.h:gambarKalibSuhu()`) & `HAL_KALIB_UDARA` (`ui.h:gambarKalibUdara()`), NVS blob `"kal2"` versi 2 — **catatan:** `PIN_DS_CAIRAN=45` & `PIN_DS_MOTOR=36` di `config.h` **tidak dipakai** (sisa desain lama 3-bus)
- [x] ✅ MQ-135 analog GPIO 9 + divider R1=10k/R2=15k (`config.h:109-114`, `sensor.h:210-228`), rekam acuan `mq135Normal`/`mq135Kotor` di halaman kalibrasi
- [x] ✅ E-stop GPIO 8 `INPUT_PULLUP` (`.ino:616`)
- [x] ✅ **RGB WS2812 indikator per sensor (5 sensor)** — aktif di `config.h` (`SENSOR_LED_ENABLE 1`, `PIN_LED_SENSOR 47` — bebas sejak migrasi TMC2209). `led_strip.h`: `ledSensorInit()`/`ledSensorUpdate()`, 5 pixel, warna hijau ≥20% / kuning 5–19% / merah <5%, tiap komponen warna diskalakan level sehingga kecerahan = bar liquid, throttle 250 ms + redraw hanya bila sensor berubah ≥2%. Non-blocking, hook di `.ino` `setup()` + `loop()`.

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
- [x] ✅ Tampilkan nomor versi firmware di OLED — `FW_VERSI` di pojok kanan atas halaman Menu Utama (`ui.h:gambarMenuUtama()`, `areaAtas` list dinaikkan ke 10 px supaya tidak menimpa baris versi)
- [x] ✅ Level Cairan: status ISI/KOSONG + ambang — `sensor.h:sensorStatusUpdate()` memakai `AMBANG_MARGIN` (10%) naik & `AMBANG_HYST` (3%) turun supaya tidak berkedip di ambang; `sensorAdaCairan(i)` dipakai `ui.h:gambarLevel()` (label `ISI` blok terbalik / `KOSONG`, bar geser ke kanan)
- [x] ✅ Halaman `HAL_PLACEHOLDER` & `gambarPlaceholder()` dihapus —enum `data.h`, `namaHalaman[]`, `case` di `ui.h` & `.ino`, fungsi `gambarPlaceholder()` tidak pernah terjangkau sejak v0.5.2
- [x] ✅ Halaman Kalibrasi Agitasi/Fan tidak lagi pakai `delay()` — `mulaiTestMotor(jenis, durasiMs)` + `upkeepKalibrasiMotor()` dijadwalkan via `millis()`, dicek di `loop()`. `kalibMotorRunning` yang tadinya di-set tapi tidak pernah dibaca kini jadi flag aktif. BACK saat test sedang jalan mematikan motor
- [ ] ⬜ **Halaman ALARM baru** (`HAL_ALARM`) untuk semua kondisi gagal — butuh `prosesGagal()` + `KodeError` dari Bab 6
- [x] ✅ Layar proses: progress **per fase** (bukan persen total) (v0.7.2) - loading bar menormalisasi `frac` ke dalam band fase yang sedang jalan via `process.h:prosesFraksiFase()` + `FASE_AWAL_F[]`/`FASE_AKHIR_F[]` (tunggal dengan `faseDariFrac()`, jadi bar 100% tidak pernah bertentangan dengan pergantian fase). Masalahnya: bar lama memakai fraksi total, sehingga selama fase ETCHING - 65% dari total waktu preset - bar terlihat beku di ~10% hampir dua thirds durasi. Persen total tetap di kanan atas supaya konteks tidak hilang
- [x] ✅ Layar proses: panel ml terpompa per bahan (HCl/H2O2/H2O) (v0.7.2) - `ui.h:gambarPanelMl()` baris 45-49 (font mini). **Butuh perbaiki bug lebih dulu:** `pompaMulai()` me-reset `jalanLangkah[]`, dan `aksiFase(FASE_RINSING)` memanggil `pompaMulai(2, 100)` tanpa menghentikan pompa H2O dulu, sehingga ml H2O dari DOSING terhapus diam-diam. Akumulator baru `motor.h:totalMlTerpompa[3]` bertambah di `pompaRekamHasil()` dari tiga titik: `motorUpdate()` saat target tercapai, `pompaStopAll()` (ml yang sudah keluar sebelum E-stop tetap tercatat - cairan itu benar-benar keluar dari tangki), dan `pompaMulai()` **sebelum** reset `jalanLangkah`. Dihapus `pompaResetTotal()` di `prosesMulai()`
- [x] ✅ Layar proses: ikon status aktuator (agitasi/drain/fan/soap) (v0.7.2) - `ui.h:gambarIkonAktuator()` baris 52-61, 4 ikon `AGI/LIM/FAN/SBN`. Butuh shadow state di `dcmotor.h` (`agitasiDutyAktif`, `pompaSabunOn`, `pompaLimbahOn`, `fanOn`) karena `analogWrite()` menulis langsung ke register PWM dan tidak bisa dibaca balik. Dicatat di setiap setter termasuk `dcmotorStopAll()`. OLED monochrome jadi motor aktif ditandai kotak di sekeliling label
- [ ] ⬜ Layar proses: badge peringatan gas MQ-135 / suhu tinggi — butuh ambang proteksi dari Bab 4
- [ ] ⬜ Layar proses: tampilkan siklus agitasi ke berapa — butuh `prosesSiklus` dari Bab 4
- [ ] ⬜ Badge permanen "E-STOP" di header saat latch aktif — butuh latch `estopAktif` dari Bab 6
- [x] ✅ Layar selesai: ringkasan total ml terpompa + durasi aktual (v0.7.2) - `ui.h:gambarSelesai()` sebagai **mode gambar dari `HAL_PROSES`** (bukan halaman baru; isinya statis jadi tidak perlu menambah enum `Halaman`), dipanggil dari `gambarProses()` saat `prosesSelesai()`. Menampilkan durasi aktual vs rencana, total ml, rincian per bahan, dan "BACK: keluar". Baris 43-63 OLED sebelumnya tidak terpakai sama sekali. **Temuan:** durasi aktual tidak bisa diambil dari `prosesAkumMs` karena dijepit ke `totalMs` di `prosesUpdate()` supaya fraksi progress selalu 0..1 - di akhir proses nilainya identik dengan `totalDetik` dan tidak ada informasi baru. Ditambah `prosesWaktuMs` yang tidak dijepit, khusus untuk laporan

---

## 3. Modul Dosing (`process.h` § DOSING, `motor.h`, `presetops.h`)

- [x] ✅ Driver stepper non-blocking (puls STEP tiap `LANGKAH_INTERVAL_US`) (`motor.h:motorUpdate()`)
- [x] ✅ Konversi ml → jumlah langkah (`targetLangkah = ml × langkahPerMl`) (`motor.h:pompaMulai()`)
- [x] ✅ 3 pompa (HCl/H2O2/H2O) dijalankan **paralel** saat fase DOSING (`process.h:aksiFase()`)
- [x] ✅ `pompaMulai()` / `pompaStopAll()` / `pompaSelesai()` tersedia
- [x] ✅ `pompaMatikanTotal()` — stop + `stepperSleep(true)` (EN = HIGH) supaya driver TMC2209 benar-benar tidur
- [x] ✅ Hitung otomatis %H2O dan ml tiap bahan dari %HCl/%H2O2 + volume (`presetops.h:hitungUlang()`)
- [x] ✅ Validasi rentang semua field preset (`presetops.h:validasiPreset()`)
- [x] ✅ Halaman kalibrasi stepper: uji pompa + set ml/rotasi + simpan ke NVS (`HAL_KALIB_STEPPER`)
- [x] ✅ `langkahPerMl = STEP_PER_REV / mlPerRotasi` — `hitungLangkahPerMl()` memakai `STEP_PER_REV` (3200, TMC2209 1/16) bukan asumsi tetap 7200
- [x] ✅ `kalibStepperMl[]` **dipakai** `pompaMulai()` — dibaca `muatKalibStepper()` saat boot, dihitung di `motorInit()`, ditulis `simpanKalibStepper()` saat BACK dari halaman kalibrasi
- [x] ✅ Simpan kalibrasi ml/rotasi ke NVS — blob `"kal3"` (`DataKalib3`: versi + `float stepperMl[3]` + CRC32), terpisah dari `"kal"`/`"kal2"`
- [x] ✅ `STEP_INTERVAL_US` 400 → **1200 µs** (aman 3 pompa paralel, ~830 Hz)
- [x] ✅ Pre-flight `TAHAP_CEK`: sensor mati + stok cukup, **sebelum** pompa menyala (`process.h:dosingCekStok()`)
- [x] ✅ Dosis = **resep** (bukan `target − sisa`); sensor hanya mengesahkan ketersediaan
- [x] ✅ Cek ketersediaan → `ERR_LEVEL_KURANG` sebelum pompa nyala
- [x] ✅ Cek ulang setelah prime + guard sumber habis **di tengah** dosing
- [x] ✅ Selesai dosing **berbasis kondisi** (`pompaSemuaSelesai()`), bukan fraksi waktu
- [x] ✅ Prime selang (`PRIME_STEPS`) sebelum dosing
- [x] ✅ Anti-drip: balik `DIR` `ANTI_DRIP_STEPS` setelah pompa selesai
- [x] ✅ Guard volume `DOSE_MIN_ML` / `DOSE_MAX_ML` per pompa
- [x] ✅ Watchdog dosing **dinamis** `dosingWatchdogMs()` (2× estimasi + margin, plafon 5 menit)
- [x] ✅ Deteksi macet/jekot (langkah beku > 3 s)
- [x] ✅ `pompaSiapkan()`: target langkah tanpa menyalakan pompa (batas durasi diketahui di awal)
- [x] ✅ Volume tangki `VOL_TANGKI_*` tersedia (dengan `#ifndef`, siap pindah ke `config.h`)
- [ ] ⚠️ `LANGKAH_INTERVAL_US = 1200` masih asumsi B — **belum diukur**; 0.26 ml/s membuat dosis 300 ml butuh ±19 menit (lihat §11 uji laju step)
- [ ] ⬜ Ditolak `ERR_POMPA_KALIB` bila pompa belum dikalibrasi (butuh flag `pompaValid[]` di NVS — sisi B)
- [ ] ⬜ Deteksi macet rotor sungguhan (butuh TMC2209 StallGuard / sensor arus)
- [ ] ⬜ Wizard kalibrasi gravimetrik + auto-stop non-blocking (UI sisi B, `dosing-design.md` §6)

---

## 4. Modul Etching (`process.h` § ETCHING, `dcmotor.h`)

- [x] ✅ Motor agitasi PWM pada speed 1–5 (map 80–255) (`dcmotor.h:22`)
- [x] ✅ Agitasi menyala di fase ETCHING dan RINSING (`process.h:36,44`)
- [x] ✅ Agitasi mati di DRAINING, DOSING, SELESAI, dan saat batal
- [x] ✅ Field `agitasiSpeed` dapat diubah di preset (1–5)
- [x] ✅ Field `agitasiDetik` dapat diubah di preset (1–60 s)
- [x] ✅ `agitasiDetik` **dipakai proses** — jadi siklus agitasi ON/OFF dan batas budget etching
- [x] ✅ Siklus agitasi periodik: ON N detik → OFF N detik → ulangi (`process.h:FASE_ETCHING`)
- [x] ✅ Exhaust fan otomatis menyala selama ETCHING (asap HCl + panas)
- [x] ✅ Exhaust fan menyala selama RINSE_SABUN (busa)
- [x] ✅ Ramp-up agitasi 1 s (`agitasiSetRamp`/`agitasiUpdate()`, dipanggil dari `prosesUpdate()`)
- [x] ✅ Proteksi suhu: `sensorSuhu(1)` > 70 °C → badge peringatan; > 85 °C → `ERR_SUHU`
- [x] ✅ Proteksi gas: `mq135PPM()` > `GAS_AMANG_PPM` → `badgeWarning` + `badgeTeks`
- [x] ✅ Budget waktu etching dihitung dari preset (`max(agitasiDetik, totalDetik − overhead)`)
- [ ] ⚠️ Badge gas/suhu belum dirender `ui.h` (nilai `badgeWarning` & `badgeTeks` sudah siap — 1 baris di `gambarProses()`)
- [ ] ⚠️ Auto-stop saat gas di luar batas (saat ini hanya peringatan, disengaja)
- [ ] ⬜ Turunkan speed agitasi otomatis saat suhu 70–85 °C (badge dulu, belum auto-turunkan)

---

## 5. Modul Draining & Rinsing (`process.h` § DRAIN/RINSE)

- [x] ✅ Pompa Limbah (TB6612FNG #2 PWA) PWM on/off (`dcmotor.h:28`)
- [x] ✅ Pompa Limbah menyala di fase DRAINING (`process.h:40`)
- [x] ✅ Pompa Limbah mati saat SELESAI & batal
- [x] ✅ Rinsing: pompa air (stepper #3) 100 ml + agitasi (`process.h:43-45`)
- [x] ✅ `fanSet(false)` saat SELESAI
- [x] ✅ DRAINING/RINSING selesai berdasarkan **durasi tetap**, bukan fraksi preset
- [x] ✅ Durasi tetap per sub-fase: `T_DRAINING_MS`, `T_RINSE_AIR_MS`, `T_RINSE_SABUN_MS`, `T_DRAIN_AKHIR_MS`
- [x] ✅ Guard overfill: `sensorPersen[LIMBAH] > LOKASI_LIMBAH_MAKS` (95%) → `ERR_OVERFLOW`
- [x] ✅ Pompa air **(stepper #3) dipakai di proses** untuk bilasan `ML_BILASAN`
- [x] ✅ Pompa air sabun menyala di sub-fase BILAS_SABUN + fan untuk busa
- [x] ✅ Drain akhir setelah bilas → SELESAI
- [ ] ⚠️ Volume bilasan air & sab belum bisa diubah dari preset (konstanta `ML_BILASAN`, sisi B)
- [ ] ⚠️ Histeresis/debounce level agar pompa tidak thrash (sisi B: `sensor.h`)
- [ ] ⬜ Deteksi "tangki proses kosong" (opsional, perlu sensor tambahan — GPIO 21/47 bebas)

---

## 6. Modul Emergency Stop & Keselamatan (`process.h` § ESTOP)

- [x] ✅ Baca E-stop GPIO 8 tiap loop (`.ino:634`)
- [x] ✅ E-stop LOW → `prosesBatal()` + toast "E-STOP!" (`.ino:635-638`)
- [x] ✅ E-stop LOW → matikan agitasi, pompaSabun, pompaLimbah, fan, semua pompa (`.ino:639-643`)
- [x] ✅ Semua aktuator mati saat FASE_SELESAI (`process.h:47-53`)
- [x] ✅ Semua aktuator mati saat proses dibatalkan (`.ino:110-116`)
- [x] ✅ Dialog konfirmasi batal saat proses berjalan (`ino:121-141`)
- [x] ✅ Log Serial untuk setiap transisi fase & pembatalan
- [x] ✅ Estop dipindah ke **baris pertama `loop()`** (v0.7.3) - cek `estopAktif || digitalRead(PIN_ESTOP)==LOW` jadi baris pertama `.ino:loop()` dengan `return`, sehingga tombol/proses/motor/sensor/render dilewati (sebelumnya di `.ino:701`, SETELAH `tanganiTombol()`, `prosesUpdate()`, `motorUpdate()`, `sensorUpdate()`). **Tiga hal tetap jalan saat latch aktif** karena dilewatkannya justru merusak fail-safe: `buzzerUpdate()` (nada alarm membeku di satu nada selama 10 detik), `ledStripUpdate()`, dan `watchdogFeed()` (tanpa ini menahan E-stop >10 detik memicu reset, dan reset justru ME-CLEAR latch)
- [x] ✅ Latch `estopAktif` - tidak auto-reset saat pin kembali HIGH (v0.7.3) - E-stop adalah sakelar putar yang dikunci operator; melepasnya otomatis hanya dengan pin bergoyang membuka jalan bagi proses yang mulai lagi dengan tangan masih di tombol
- [x] ✅ Reset latch hanya lewat BACK di `HAL_ALARM`, dan hanya bila pin HIGH (v0.7.3) - `case HAL_ALARM:` di `tanganiTombol()`. Kalau pin masih LOW, BACK diabaikan + toast "E-stop masih ditekan" dan layar menampilkan "E-STOP MASIH DITEKAN" supaya operator tahu kenapa tombol tidak merespons
- [x] ✅ Fungsi tunggal `semuaAktuatorMati()` - **SUDAH ADA SEJAK LAMA, item ini basi** (v0.7.3). Ada di `process.h:30`, dipanggil dari 4 tempat: `aksiFase` FASE_SELESAI, `prosesBatal`, handler E-stop, dan `>off`. Teks item menggambarkan kondisi SEBELUM refactor. Tidak dikerjakan ulang, hanya ditandai benar
- [x] ✅ Cut power driver stepper saat stop - **SUDAH TERPENUHI, tapi item aslinya salah paham** (v0.7.3). `TMC_PDN` adalah pin *reset*, bukan enable power; `LOW` = mode reset, bukan mati listrik. Dan `PROCESS.md:231` yang menulis `digitalWrite(TMC_EN, LOW)` terbalik: sejak migrasi TMC2209 (v0.6.0) EN **aktif LOW**, jadi tidur = EN **HIGH** - sudah dikerjakan `stepperSleep(true)` di dalam `pompaMatikanTotal()`. Menjalankan `TMC_PDN LOW` saat stop justru akan membuat driver dalam mode reset. Tidak diimplementasikan, alasannya dicatat di `LOG.md`
- [x] ✅ `prosesGagal(kode)` sebagai satu-satunya jalan keluar kondisi abnormal (v0.7.3) - `process.h`, dengan `enum KodeError` 8 nilai + `namaError[]` + `pesanError[]` di `data.h`. Alarm ditandai `prosesError != ERR_NONE`, **bukan** nilai baru di `enum FaseProses` (menambah `FASE_GAGAL` berarti menyentuh semua `switch` pada fase). Panggilan kedua diabaikan supaya kode pertama yang paling informatif tidak tertimpa. `prosesMulai()` menolak jalan selama alarm/E-stop aktif
- [x] ✅ Timeout/watchdog per fase (DOSING, DRAINING, RINSE) (v0.7.3) - `TIMEOUT_FASE_MS[]` per fase di `process.h`, dibandingkan terhadap `prosesMasukFaseMs`. Jaring pengaman terakhir untuk timer kacau / fase tidak pernah berganti. Diisi juga `ERR_STEP_MACET`: `pompaJalan[i]` true tapi `jalanLangkah[i]` beku >3 detik - pada pompa peristaltik "nyala tapi tidak memompa" (selang putus, rotor macet) tidak terlihat dari driver
- [x] ✅ Buzzer nada alarm (berulang) berbeda dari nada "selesai" (v0.7.3) - `N_ALARM` = 3 beep panjang 400 Hz + jeda, berulang sampai di-reset. Scheduler `buzzerPutar()` dapat parameter `ulang` supaya alarm bisa berjalan tanpa batas tanpa membuat `buzzerUpdate()` blocking
- [x] ✅ Halaman alarm + instruksi pemulihan (v0.7.3) - `HAL_ALARM` (enum `Halaman` + `namaHalaman[]`), `ui.h:gambarAlarm()` menampilkan kode dalam kotak text size 2, `pesanError[]` sebagai instruksi, detik ke-berapa proses berhenti, dan petunjuk tombol
- [x] ✅ Jeda (`prosesToggleJeda`) ikut mematikan output, bukan hanya timer (v0.7.3) - **bug nyata yang sudah lama ada**: agitasi/pompa/fan tetap berputar selama jeda. Sekarang `semuaAktuatorMati()` saat jeda, `aksiFase()` saat lanjut (shadow state v0.7.2 membuat ikon di layar proses ikut benar)
- [x] ✅ Deteksi kegagalan sensor saat proses (DS18B20/touch) → abort aman (v0.7.3) - `sensorMati[]` (v0.6.5) dipakai di `prosesUpdate()`: sensor mati saat proses jalan → `prosesGagal(ERR_SENSOR)`. Level dipakai untuk keputusan dosing dan cek akhir, jadi sensor mati berarti keputusan diambil dari data palsu. Hanya saat proses aktif & tidak dijeda
- [x] ✅ Fail-safe saat OLED/PCF8574 hilang (v0.7.3) - **sebagian sudah terpenuhi sejak dulu**: E-stop di `PIN_8` GPIO langsung, tidak lewat I2C, jadi tetap mematikan aktuator walau seluruh bus I2C mati. Yang ditambahkan: `>status` melaporkan `i2cGagalBerturut`, jumlah recovery, dan status watchdog. Peringatan tidak bisa tampil di OLED justru karena OLED itulah yang hilang - jadi lewat Serial
- [x] ⚠️ Timeout per fase jadi **dinamis** (v0.7.4) — `TIMEOUT_FASE_MS[]` konstanta diganti `batasTimeoutFase()`: dosing = `dosingWatchdogMs()` (2× estimasi + margin, plafon 5 menit), sub-fase lain = 2× budgetnya. Angka tetap akan membunuh dosis yang sah
- [x] ✅ Jeda sekarang **menahan pompa di posisi** (v0.7.4) — `pompaJedaTahanPompa` membuat `motorUpdate()` berhenti tanpa merekam ml, sehingga resume melanjutkan dari langkah tersisa. Versi v0.7.3 memakai `semuaAktuatorMati()` yang **merekam** ml dan mengulang bagian yang sudah keluar
- [ ] ⚠️ `prosesGagal()` belum punya kode `ERR_SUMBER_HABIS` & `ERR_POMPA_KALIB` (butuh enum di `data.h` — sisi B); sekarang dipetakan ke `ERR_LEVEL_KURANG`

---

## 7. Sensor & Kalibrasi (`sensor.h`, `storage.h`)

- [x] ✅ Baca 5 sensor level via `touchRead()` (strip nikel non-contact) (`sensor.h:60`)
- [x] ✅ Moving average window 8 untuk redam noise (`sensor.h:13,59`)
- [x] ✅ Konversi raw → persen dengan clamp 0–100 (`sensor.h:74`)
- [x] ✅ Kalibrasi 0% & 100% per sensor dari UI (`ino:412-460`)
- [x] ✅ Kalibrasi level disimpan ke NVS + CRC + auto-reject bila tidak valid (`storage.h:87`)
- [x] ✅ Baseline default `113000/156000` bila kalibrasi belum ada (`storage.h:121-122`, `sensor.h:54-55`) — **angka hasil ukur nyata** dari `pilot/pilot_touch_sensor.ino:58-59`, bukan tebakan; skala `touchRead()` di ESP32-S3 memang ratusan ribu. ⚠️ Default dipakai **seragam untuk kelima sensor**, padahal hasil ukur per-sensor berbeda → error ±12% di tengah skala (masih tepat di 0% dan 100%). Hilang begitu tangki dikalibrasi lewat UI, karena nilai asli dari `touchRead()` lalu tersimpan di NVS `"kal"`
- [ ] ⬜ Default baseline **per-sensor** (5 pasang nilai) — pakai hasil ukur pilot sketch: kosong `{113000, 113000, 113000, 116000, 110000}`, penuh `{156000, 161000, 161000, 157000, 152000}` (`sensor.h:54-55`, `storage.h:121-122`). **Prioritas rendah** — hanya menjaga akurasi sebelum tangki dikalibrasi; setelah `Kalibrasi → Sensor Level` tiap tangki, default tidak lagi dipakai. **DITUNDA atas permintaan pemilik** — diabaikan karena dampaknya nol setelah kalibrasi.
- [x] ✅ Halaman Kalibrasi → 4 submenu (Motor, Sensor Level, Sensor Suhu, Sensor Udara)
- [x] ✅ Kalibrasi MQ-135: rekam acuan udara bersih & kotor (`ino:388-408`)
- [x] ✅ Kalibrasi suhu: pilih sensor per blok + offset ±0,1 °C (`ino:348-373`)
- [x] ✅ Kalibrasi agitasi & fan: uji motor/kipas (`ino:292-344`)
- [x] ✅ Kalibrasi DC: atur duty 0–255 dan catat ml hasil uji (`ino:246-289`)
- [x] ✅ Log diagnostik `[SENSOR]`/`[SUHU]`/`[STORE]` ke Serial
- [x] ✅ **Address DS18B20 tidak lagi hardcode** (v0.6.3) — `sensorSuhuInit()` memindai bus 1-Wire lewat `getAddress()` lalu menyimpan hasilnya ke NVS blob `"kal5"` (`DataKalib5`: `addr[3][8]` + bitmask valid + CRC32). Boot berikutnya memakaitaddress dari NVS supaya urutan blok tidak berubah. Family code 0x28 diverifikasi saat muat — blob rusak ditolak lalu dipindai ulang. Slot kosong ditandai `sensorAddressValid[s] = false` → `sensorSuhuRaw()` mengembalikan -127 → UI tampil `--.-C`. Address dicetak ke Serial saat boot pertama untuk dicocokkan dgn label fisik Motor/Komponen/Chamber
- [x] ✅ `kalibSuhuAddr[]` & `suhuOffsetX10[]` **sudah disimpan ke NVS** (v0.6.4) — `DataKalib2` versi 2 memakai `addrSensor[3]` + `suhuOffsetX10[3]`. **Perbaikan v0.6.4:** sebelumnya status ini ditandai ✅ padahal `simpanKalib2()` tidak pernah dipanggil — struct-nya sudah ada sejak v0.6.0 tapi tidak ada pemanggilnya (bug warisan merge `2c84273`). Sekarang `simpanKalib2()` dipanggil saat BACK dari `HAL_KALIB_SUHU`
- [x] ✅ `mq135Normal`/`mq135Kotor` **sudah disimpan ke NVS** (v0.6.4) — keduanya field di `DataKalib2` versi 2, ditulis saat BACK dari `HAL_KALIB_UDARA` (perbaikan yang sama). *Sisa:* belum dipakai proteksi gas (itu item Bab 4)
- [x] ✅ `kalibMotorRunning` **sudah dibaca** (v0.6.1) — `mulaiTestMotor()` + `upkeepKalibrasiMotor()` menjadwalkan auto-stop test agitasi/fan via `millis()`, plus BACK mematikan motor lebih dulu. Lihat §2 Modul UI
- [x] ✅ `kalibDuty[]` & `kalibMl[]` **sudah disimpan ke NVS** (v0.6.2) — blob `"kal4"` (`DataKalib4`: `duty[7]` + `mlX10[2]`, versi + CRC32), ditulis `simpanKalib4()` saat BACK dari `HAL_KALIB_DC` / `HAL_KALIB_AGITASI` / `HAL_KALIB_FAN`, dibaca `muatKalib4()` di `setup()`. Default 128 (≈50% duty) bila blob belum ada
- [x] ✅ Auto-stop motor kalibrasi setelah N ms (ganti `delay()`) — selesai v0.6.1 (`upkeepKalibrasiMotor()`), lihat catatan `kalibMotorRunning` di atas
- [x] ✅ Terapkan `kalibDuty[]` ke motor (v0.6.6) — semua PWM kini diambil dari hasil kalibrasi, bukan konstanta: `pompaSabunSet()`→`kalibDuty[0]`, `pompaLimbahSet()`→`kalibDuty[1]`, `fanSet()`→`kalibDuty[6]`. Agitasi punya 5 tingkat di preset tapi hanya 3 titik kalibrasi, jadi `agitasiPwm()` meng-interpolasi linier (speed 1→slot 2, 2→antara 2-3, 3→slot 3, 4→antara 3-4, 5→slot 4) — semua hasil kalibrasi terpakai dan tidak ada tingkat yang meloncat. `dcmotorInit()` dipanggil setelah `muatKalib4()` di `setup()`, jadi nilai kalibrasi sudah tersedia saat motor pertama kali dihidupkan. Default semua slot 128 (~50%) bila blob belum ada
- [x] ✅ Simpan semua parameter kalibrasi ke NVS — 5 blob terpisah, masing-masing dengan versi + CRC32: `"kal"` (baseline 0%/100% + kapasitas 5 tangki level), `"kal2"` (addr + offset 3 blok DS18B20 + acuan MQ-135), `"kal3"` (ml/rotasi pompa stepper), `"kal4"` (duty PWM 7 motor + ml hasil uji motor DC), `"kal5"` (address DS18B20 hasil pindai bus)
- [x] ✅ Deteksi sensor touch mati (v0.6.5) — `sensor.h:sensorMatiUpdate()` menandai strip yang `touchRead()`-nya beku di **0** selama `SENSOR_BEKU_MS` (60 detik); `touchRead()` yang sehat selalu non-nol, jadi 0 beku ≈ kabel putus / strip copot. Log `[SENSOR] <nama> MATI` sekali saat berubah, dan `[SENSOR] <nama> pulih` saat hidup lagi (tanpa perlu reset). `ui.h:gambarLevel()` menampilkan `DEAD` menggantikan persen + menyembunyikan isi bar, karena angka 0% yang palsu lebih berbahaya daripada tidak menampilkan apa pun. **Batas deteksi:** strip yang sudah tidak sensitif (buildup karat, HDPE tebal) masih bergerak sedikit dan *tidak* terdeteksi — hanya kasus beku total yang tertangkap. Penting untuk `FASE_CEK` Bab 3 agar tidak memercayai level yang salah

---

## 8. Penyimpanan & Data (`storage.h`, `presetops.h`, `data.h`)

- [x] ✅ Maks 10 preset (`MAKS_PRESET`)
- [x] ✅ 5 preset default saat pertama kali jalan (`data.h:25`)
- [x] ✅ CRUD: tambah, duplikat, hapus dengan pergeseran array (`presetops.h:83-110`)
- [x] ✅ Nama preset unik otomatis "Preset N" (`presetops.h:71`)
- [x] ✅ Simpan preset ke NVS sebagai satu blob + versi + CRC32 (`storage.h:26`)
- [x] ✅ Muat preset: reject bila versi beda / CRC rusak / data tak valid (`storage.h:45`)
- [x] ✅ Auto-reset ke default bila data korup (`storage.h:70`)
- [x] ✅ Simpan kalibrasi pompa (ml/rotasi) ke NVS — blob `"kal3"` (`DataKalib3`: `float stepperMl[3]` + versi + CRC32), ditulis `simpanKalibStepper()` saat BACK dari `HAL_KALIB_STEPPER` (v0.5.7)
- [x] ✅ Simpan offset suhu & acuan MQ-135 ke NVS — blob `"kal2"` (`DataKalib2` versi 2: `addrSensor[3]` + `suhuOffsetX10[3]` + `mq135Normal`/`mq135Kotor`, versi + CRC32), ditulis `simpanKalib2()` saat BACK dari `HAL_KALIB_SUHU` / `HAL_KALIB_UDARA` (v0.6.4)
- [x] ✅ Simpan duty/kecepatan motor per kalibrasi ke NVS — blob `"kal4"` (`DataKalib4`: `duty[7]` + `mlX10[2]`, versi + CRC32), ditulis `simpanKalib4()` saat BACK dari `HAL_KALIB_DC`/`HAL_KALIB_AGITASI`/`HAL_KALIB_FAN` (v0.6.2)
- [x] ✅ Backup/restore preset via Serial (export/import) — `serialcmd.h` v0.7.1: `>export` (cetak blok teks ber-header `#AE1` + footer `#OK <crc32>`), `>import` (terapkan), `>rollback` (kembalikan daftar sebelum import terakhir), `>abort`. Snapshot otomatis ke key NVS `"prev"` sebelum overwrite. Import ditolak kalau proses jalan; setiap baris divalidasi (jumlah field, indeks berurutan, angka benar-benar angka via `strtol` + cek sisa string, `validasiPreset()`, nama unik); CRC mismatch = batal total, preset lama utuh. Baris >80 B ditandai dan ditolak, tidak dipotong diam-diam. Parser non-blocking per karakter; baris berawalan `>` membatalkan import **dan** jadi perintah (tidak ada kondisi buntu). Cakupan **preset saja** — blob kalibrasi `kal`…`kal5` sengaja tidak ikut karena terikat alat fisik

---

## 9. Infrastruktur Firmware

- [x] ✅ Semua modul non-blocking (`loop()` berbasis `millis()`), tanpa `delay()` di jalur proses
- [x] ✅ Pisah logika proses (`prosesUpdate()`) dari render UI
- [x] ✅ Log debug lengkap via `dbg.h` dengan prefix per modul (`[PROSES]`, `[POMPA]`, `[KALIB]`, …)
- [x] ✅ Struktur file modular (`config/input/data/presetops/sensor/storage/motor/dcmotor/buzzer/process/ui`)
- [x] ✅ Backup versi di folder `backup/` per rilis
- [x] ✅ Watchdog timer hardware (`esp_task_wdt`) — file baru `watchdog.h`, timeout **10 detik** + `trigger_panic`. `watchdogInit()` di `setup()`, `watchdogFeed()` tiap 1 detik di `loop()`. Kalau `init()` kena `ESP_ERR_INVALID_STATE` (Arduino core sudah daftar di boot) otomatis fallback ke `reconfigure()`. **Sengaja tidak panic** kalau gagal — alat tetap jalan, hanya tanpa jaring pengaman, supaya debugging lapangan tidak jadi sulit. Ada juga `watchdogMatikan()`/`watchdogNyalakan()` untuk operasi yang memang boleh lama
- [x] ✅ Deteksi & pemulihan I2C (PCF8574 hang → reset bus) — `input.h`: `i2cGagalBerturut` naik tiap `bacaPCF8574()` gagal, reset tiap sukses. `i2cUpkeep()` di `loop()` memicu `i2cRecovery()` setelah **5 kegagalan beruntun**: `Wire.end()` → jeda 10 ms (supaya slave lepas dari SDA) → `Wire.begin()` → verifikasi. Kalau gagal, penghitung **tidak** di-reset supaya langsung dicoba lagi di iterasi berikutnya
- [x] ✅ Kontrol via Serial (start/stop/preset/status) — file baru `serialcmd.h`, 7 perintah: `help`, `status`, `start`, `stop`, `preset N`, `sel <halaman>`, `off`. **Non-blocking**: parser baca 1 karakter per iterasi `loop()` dan eksekusi saat Enter — sengaja tidak memakai `Serial.readString()` yang memblokir sambil menunggu Enter. Prefix `>` supaya log `dbg.h` tidak ikut diparse
- [ ] ⬜ Logging proses ke file / upload via WiFi — **DITUNDA dengan sengaja**: butuh LittleFS/SPIFFS (boros flash) dan penulisan tiap detik akan mempercepat keausan flash. Serial log yang sudah ada cukup untuk kebutuhan saat ini
- [x] ✅ Cleaning `#define` mati di `config.h` — 12 define dihapus (`INTERVAL_SENSOR_MS`, `SENSOR_AVG_N`, `SENSOR_AKTIF_LOW`, `SENSOR_TOUCH_READ`, `TOUCH_DEBOUNCE`, `TOUCH_DELTA_*`, `TOUCH_ADAPT_*`, `TOUTH_KALIB_SAMPEL`, `TOUCH_DELTA_PENUH_AWAL`, `SENSOR_LOG_PERIOD_MS`). **Syarat-syarat ini sisa dari desain sensor analog lama** yang sudah diganti touch-read. *Catatan:* `AMBANG_MARGIN` & `AMBANG_HYST` **tidak dihapus** — keduanya dipakai `sensor.h:sensorStatusUpdate()` sejak v0.6.1. `SENSOR_BEKU_MS` dipindah ke `config.h` supaya konstanta ambang sensor terpusat

---

## 10. Dokumentasi

- [x] ✅ `README.md`, `GUIDE.md`, `SUMMARY.md`, `LOG.md`
- [x] ✅ `etching-otomatis.md` — rencana pengembangan + pin map awal
- [x] ✅ `water-level-sensor-plan.md` — rencana sensor level non-contact
- [x] ✅ `PROCESS.md` — rencana detail 5 modul proses + cara kerja + pin map TMC2209
- [x] ✅ `PROGRESS.md` — checklist status implementasi (dokumen ini)
- [x] ✅ Update `SUMMARY.md` agar sinkron dengan pin map TMC2209 & fitur baru — tabel pin map sudah ditulis ulang lengkap dengan `TMC_EN`/`TMC_PDN` (aktif LOW), STEP/DIR tiap channel, GPIO yang dibebaskan, dan status GPIO yang tidak dipakai
- [x] ✅ Update `LOG.md` / naikkan versi firmware untuk setiap fitur baru — dilakukan di setiap rilis (v0.5.7 → v0.6.7), tiap perubahan punya entri `LOG.md` berformat `[Tanggal] vX.Y.Z — ringkasan` + versi `FW_VERSI` di `config.h`

---

## 11. Uji Hardware yang Masih Diperlukan

- [ ] ⬜ Uji 5 sensor touch di jerican kosong vs terisi (repeatability < 5%)
- [ ] ⬜ Verifikasi address DS18B20 fisik cocok dengan `sensor.h:119`
- [ ] ⬜ Timbang air: 3 pompa × 3 ulangan → error ≤ 5% dari target
- [ ] ⬜ Uji driver TMC2209 3 channel (STEP/DIR/EN/PDN) dengan pin map baru
- [ ] ⬜ Uji respons E-stop (target < 5 ms dari_LOW ke semua output mati)
- [ ] ⬜ Uji drain: volume larutan terbuang habis dalam `T_DRAINING_MS`
- [ ] ⬜ Uji overfill guard (isi tangki limbah hampir penuh)
- [ ] ⬜ Uji siklus agitasi (ON/OFF berulang) + fan
- [ ] ⬜ Uji stabilitas level readout saat kabel AWG30 30 cm
- [ ] ⬜ **Seluruh uji wajib pakai air biasa dulu**, baru larutan encer
- [ ] ⬜ **[v0.7.4] Uji laju step 10 000 langkah** — catat waktu aktual vs teoritis (`10 000 × LANGKAH_INTERVAL_US / 1000`), deteksi miss step. Targetkan ml/s ≥ 0.6 agar dosis 300 ml < 8 menit
- [ ] ⬜ **[v0.7.4] Uji prime & anti-drip** — tidak ada air keluar sebelum dosing; tidak ada tetesan setelah pompa selesai
- [ ] ⬜ **[v0.7.4] Uji cek stok** — tangki sengaja dikosongkan → proses harus berhenti di CEK STOK sebelum pompa nyala
- [ ] ⬜ **[v0.7.4] Uji jeda di tengah dosing** — resume melanjutkan, tidak mengulang ml yang sudah keluar
- [ ] ⬜ **[v0.7.4] Uji siklus agitasi + fan** — agitasi ON/OFF sesuai `agitasiDetik`, fan menyala di ETCHING & BILAS SABUN
- [ ] ⬜ **[v0.7.4] Uji progress bar** — bar mencapai 100% tepat saat layar selesai (bukan macet di tengah)
- [ ] ⬜ **[v0.7.4] Uji kalibrasi gravimetrik** — 3 pompa × 3 ulangan × 50 ml, repeatability ≤ 3%, error ≤ 5%

---

## Ringkasan

| Kategori | ✅ | ⚠️ | ⬜ |
|---|---|---|---|
| GPIO & Hardware | 14 | 0 | 0 |
| Modul UI | 23 | 0 | 4 |
| Modul Dosing | 24 | 1 | 3 |
| Modul Etching | 13 | 2 | 1 |
| Modul Draining & Rinsing | 11 | 2 | 1 |
| Modul Emergency Stop | 20 | 2 | 0 |
| Sensor & Kalibrasi | 21 | 0 | 1 |
| Penyimpanan & Data | 11 | 0 | 0 |
| Infrastruktur | 9 | 0 | 1 |
| Dokumentasi | 7 | 0 | 0 |
| Uji Hardware | 0 | 0 | 17 |

**Total: 153 ✅ · 7 ⚠️ · 28 ⬜** (dihitung ulang dari daftar item per bab di atas,
bukan disalin manual — tabel ringkasan sebelumnya meleset karena beberapa item
pernah diubah status tanpa tabel ikut diperbarui)

Status keseluruhan: **prototype berfungsi penuh pada jalur happy-path** (menu,
preset, dosing, agitasi, drain, e-stop dasar). Akurasi dosing **sudah memakai
hasil kalibrasi tersimpan di NVS** (`langkahPerMl` dari `kalibStepperMl[]` ×
`STEP_PER_REV`), tetapi **harus dikalibrasi ulang** setelah migrasi TMC2209.
Yang masih belum ada: **proteksi proses** (watchdog per fase, validasi level
sebelum dosing, latch e-stop, proteksi suhu/gas, deteksi pompa macet).

---

*Dokumen ini harus diperbarui setiap kali ada fitur baru yang selesai.*
