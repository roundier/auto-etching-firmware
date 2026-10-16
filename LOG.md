# Log Perubahan

Format entri: `[Tanggal] vX.Y.Z — ringkasan`
Setiap versi di-backup ke folder `backup/vX.Y.Z_tanggal/` sebelum perubahan dilakukan.

---

## [2026-10-16] v0.7.3 — Fail-safe: prosesGagal, latch E-stop, HAL_ALARM (Bab 6)

Menutup 9 item Bab 6. Satu item ditandai stale dan satu lagi sengaja di-skip
(dijelaskan di bawah).

### Tulang punggung: `prosesGagal(kode)`

Semua kondisi abnormal sekarang lewat **satu** fungsi. Sebelumnya tiap jalur
"mati mendadak" punya efek samping sendiri-sendiri, sehingga tidak bisa diuji
di satu titik dan mudah ada yang terlewat.

`enum KodeError` (8 nilai) + `namaError[]` + `pesanError[]` di `data.h`.
Alarm ditandai `prosesError != ERR_NONE`, **bukan** lewat nilai baru di
`enum FaseProses`. Alasannya: `namaFase[]` diindeks oleh enum itu di beberapa
tempat, jadi menambah `FASE_GAGAL` berarti menyentuh setiap `switch` pada fase.
`prosesFase` tetap `FASE_IDLE`.

`prosesMulai()` juga kini menolak jalan selama `prosesError != ERR_NONE ||
estopAktif` — tanpa itu, `>start` dari Serial bisa memulai proses baru dari
dalam kondisi gagal.

### E-stop dipindah ke baris pertama `loop()`

Tadinya ada di `.ino:701`, **setelah** `tanganiTombol()`, `prosesUpdate()`,
`motorUpdate()`, dan `sensorUpdate()`. Sekarang paling atas, dengan `return`
sehingga tombol/proses/motor/sensor/render semuanya dilewati.

Yang **tetap** jalan selama latch aktif — dan ketiganya penting:

- `buzzerUpdate()` — kalau dilewati, nada alarm membeku di satu nada dan
  berbunyi tanpa henti selama 10 detik (tidak pernah maju ke jeda)
- `ledStripUpdate()` — animasi merah E-stop
- `watchdogFeed()` — menahan E-stop >10 detik akan memicu reset, dan reset
  justru **me-clear** latch `estopAktif`. Latch yang justru harus bertahan
  sampai operator mereset secara sadar. Ini jebakan yang mudah terlewat:
  `continue`/`return` yang "aman" justru bisa membatalkan fail-safe-nya sendiri.

### Latch `estopAktif`

Tidak otomatis lepas saat pin kembali HIGH. Pemulihan hanya lewat BACK di
`HAL_ALARM`, dan **hanya** kalau pin sudah HIGH — kalau masih ditekan, BACK
diabaikan dan layar menampilkan "E-STOP MASIH DITEKAN". E-stop adalah sakelar
putar yang dikunci operator; melepasnya otomatis hanya dengan pin bergoyang
akan membuka jalan bagi proses yang mulai lagi dengan tangan masih di tombol.

### Jeda akhirnya mematikan output

Bug nyata: `prosesToggleJeda()` hanya menghentikan timer, jadi agitasi, pompa,
dan fan **tetap berputar** selama jeda. Tombol jeda terlihat bekerja tapi tidak
punya efek. Sekarang `semuaAktuatorMati()` dipanggil saat jeda, dan `aksiFase()`
yang memulihkan saat lanjut — shadow state v0.7.2 otomatis membuat ikon di
layar proses ikut benar.

### Buzzer alarm berulang

`N_ALARM` = 3 beep panjang 400 Hz + jeda, berulang sampai di-reset.
Scheduler `buzzerPutar()` dapat parameter `ulang` supaya alarm bisa berjalan
tanpa batas tanpa membuat `buzzerUpdate()` blocking.

### Halaman `HAL_ALARM`

Kode error dalam kotak (text size 2), instruksi pemulihan, detik ke-berapa
proses berhenti, dan petunjuk tombol. `HAL_ALARM` ditambah ke `enum Halaman`
dan `namaHalaman[]`.

### Deteksi otomatis di `prosesUpdate()`

Tiga pemeriksaan, semua hanya saat proses aktif & tidak dijeda:

1. **Pompa macet** (`ERR_STEP_MACET`) — `pompaJalan[i]` masih true tapi
   `jalanLangkah[i]` beku >3 detik. Pada pompa peristaltik "nyala tapi tidak
   memompa" adalah kegagalan yang paling sering (selang putus, rotor macet)
   dan tidak terlihat dari driver.
2. **Sensor mati** (`ERR_SENSOR`) — `sensorMati[]` dari v0.6.5. Level dipakai
   untuk keputusan dosing dan cek akhir, jadi sensor mati saat proses jalan
   berarti keputusan diambil dari data palsu.
3. **Timeout per fase** (`ERR_TIMEOUT`) — `TIMEOUT_FASE_MS[]` per fase. Ini
   jaring pengaman terakhir untuk kasus timer kacau / fase tidak pernah berganti.

### Item yang tidak dikerjakan

- **"Fungsi tunggal `semuaAktuatorMati()`" — SUDAH ADA sejak lama.** Tersimpan
  di `process.h:30`, dipanggil dari 4 tempat (`aksiFase` FASE_SELESAI,
  `prosesBatal`, handler E-stop, `>off`). Teks item menggambarkan kondisi
  *sebelum* refactor. Diubah jadi ✅, bukan dikerjakan ulang.
- **"Cut power driver stepper (TMC_PDN LOW) saat stop" — di-skip.** `TMC_PDN`
  adalah pin *reset*, bukan enable power; `LOW` berarti masuk mode reset, bukan
  mati listrik. `PROCESS.md:231` yang menulis `digitalWrite(TMC_EN, LOW)` juga
  terbalik: sejak migrasi TMC2209 (v0.6.0) EN **aktif LOW**, jadi tidur = EN
  **HIGH** — dan itu sudah dikerjakan `stepperSleep(true)` di dalam
  `pompaMatikanTotal()`. Menjalankan item itu justru akan membuat driver
  dalam mode reset saat stop. Ditandai sebagai salah paham di `PROCESS.md`.
- **ERR_LEVEL_KURANG / ERR_OVERFLOW / ERR_SUHU** - kodenya sudah ada dan siap,
  tapi pemicunya belum ada: cek volume tangki ada di Bab 3, guard overfill
  di Bab 5, ambang suhu di Bab 4. Nanti diisi pemicunya, bukan dari sini.

### Fail-safe OLED/PCF8574

Sebagian sudah terpenuhi sejak dulu: E-stop di `PIN_8` GPIO langsung, tidak
lewat I2C, jadi tetap mematikan aktuator walau seluruh bus I2C mati. Yang
ditambahkan: `>status` melaporkan `i2cGagalBerturut`, jumlah recovery, dan
status watchdog. Peringatan **tidak bisa** tampil di OLED justru karena OLED
itulah yang hilang — jadi lewat Serial.

---

## [2026-10-16] v0.7.2 — Layar proses & layar selesai (Bab 2, 4 dari 8 item)

Menutup 4 item Bab 2 yang tidak bergantung Bab 3/4/6. Sisa 4 item Bab 2
menunggu `HAL_ALARM` + `estopAktif` (Bab 6) dan ambang MQ-135/suhu +
`prosesSiklus` (Bab 4).

### 🐛 Bug: ml H2O dari fase DOSING terhapus

`pompaMulai()` melakukan `jalanLangkah[idx] = 0`, sementara
`aksiFase(FASE_RINSING)` memanggil `pompaMulai(2, 100)` untuk bilas **tanpa
menghentikan pompa H2O lebih dulu**. Jadi begitu fase RINSE dimulai,
penogonalan ml H2O dari fase DOSING hilang — dan karena itu "total ml terpompa"
tidak boleh dibaca dari `jalanLangkah[]`.

Perbaikannya: akumulator `totalMlTerpompa[3]` yang bertambah di
`pompaRekamHasil()`, dipanggil tepat saat pompa berhenti — dari tiga titik:

- `motorUpdate()` saat target langkah tercapai
- `pompaStopAll()` (dipakai `pompaMatikanTotal()` → `semuaAktuatorMati()`, jadi
  ml yang sudah keluar sebelum E-stop/batal tetap tercatat — cairan itu memang
  benar-benar keluar dari tangki)
- `pompaMulai()` **sebelum** me-reset `jalanLangkah` — ini yang menutup celah
  pompa yang masih jalan saat RINSE dimulai

Kalibrasi stepper juga memanggil `pompaMulai(idx, 1)` untuk ditimbang dan ikut
terekam, tapi tidak masalah karena `prosesMulai()` selalu
`pompaResetTotal()` lebih dulu.

### Progress per fase

Bar lama memakai fraksi **total**. Selama fase ETCHING — yang memakan 65% dari
total waktu preset — bar terlihat nyaris beku di "~10%" selama hampir dua thirds
durasi. Sekarang bar menampilkan progres **di dalam fase yang sedang berjalan**,
dinormalisasi dengan batas `FASE_AWAL_F[]` / `FASE_AKHIR_F[]` yang nilainya
tunggal dengan `faseDariFrac()`, jadi bar 100% dan pergantian fase tidak pernah
bertentangan. Persen **total** tetap di kanan atas supaya konteks tidak hilang.

### Durasi aktual

Ditemukan saat mengerjakan: `prosesAkumMs` dijepit ke `totalMs` di
`prosesUpdate()` supaya fraksi progress selalu 0..1. Konsekuensinya di akhir
proses nilainya **identik dengan `totalDetik`** — jadi "durasi aktual" dari
variabel itu tidak akan pernah memberi informasi apa pun. Ditambah
`prosesWaktuMs` yang tidak dijepit, khusus untuk laporan.

### Shadow state aktuator

`analogWrite()` menulis langsung ke register PWM — tidak ada cara membaca
kembali "apakah agitasi jalan" tanpa menyimpan state. Ditambah
`agitasiDutyAktif`, `pompaSabunOn`, `pompaLimbahOn`, `fanOn` di `dcmotor.h`,
dicatat di setiap setter. Di layar proses ditampilkan sebagai 4 ikon
`AGI/LIM/FAN/SBN`; OLED monochrome jadi motor aktif ditandai **kotak** di
sekeliling label.

### Layar selesai

`gambarSelesai()` — mode gambar dari `HAL_PROSES`, bukan halaman baru (isinya
statis, jadi tidak perlu menambah enum `Halaman`). Menampilkan durasi aktual vs
rencana, total ml, rincian per bahan, dan petunjuk keluar. Baris 43–63 OLED
sebelumnya tidak terpakai sama sekali.

`>status` di Serial sekarang ikut mencetak total ml terpompa, supaya akumulator
bisa diverifikasi tanpa menunggu layar.

---

## [2026-10-16] v0.7.1 — Backup/restore preset via Serial (Bab 8)

Menutup item terakhir Bab 8. Tiga perintah baru di `serialcmd.h`: `>export`,
`>import`, `>rollback` (plus `>abort` untuk keluar dari mode import).

**Format blok** — teks, bukan binary, supaya bisa dibaca dan diedit manusia:

```
#AE1
1|Preset 1|100|80|33|17|50|120|60|180|5|3|270
2|Preset 2|80|60|40|20|40|100|50|100|8|4|330
3|Preset 3|150|100|25|25|50|150|150|300|4|2|420
4|Preset 4|60|40|50|10|40|90|18|72|10|5|210
5|Preset 5|120|90|30|20|50|135|90|225|6|3|360
#OK 454F211E
```

`#AE1` = header + versi format. Satu baris per preset: `indeks|nama|11 int`
sesuai urutan field di `struct Preset`. `#OK <crc32>` = footer, FNV-1a atas
seluruh karakter baris data — menangkap paste terpotong atau karakter rusak.
CRC di atas dihitung dari 5 preset default dan sudah diverifikasi: 13 field per
baris, baris terpanjang 47 B (buffer import 80 B).

Pakai `|` sebagai separator aman karena nama preset tidak bisa diketik bebas
(hanya 4 tombol). `namaAman()` tetap menyaring `|` dan karakter kontrol kalau
suatu saat nama datang dari sumber lain.

**Pengaman** — bagian yang membuat fitur ini berguna:

1. `>import` ditolak kalau proses sedang jalan. Mengubah daftar preset di tengah
   etching akan membuat satu siklus mencampur dua preset berbeda.
2. Snapshot otomatis ke key NVS `"prev"` **sebelum** overwrite. Kalau hasilnya
   salah, `>rollback` mengembalikan daftar preset lama. Satu slot saja — tujuannya
   membatalkan import yang barusan salah, bukan menyimpan histori.
3. Validasi per baris: jumlah field harus tepat, indeks harus berurutan mulai 1,
   semua angka harus benar-benar angka (`strtol` dengan pemeriksaan sisa string,
   supaya `"abc"` tidak diam-diam jadi 0 lalu lolos), `validasiPreset()` untuk
   rentang, nama tidak kosong, nama unik.
4. CRC harus cocok. Kalau tidak, import dibatalkan total dan preset lama tidak
   disentuh.
5. Baris data hanya diterima **setelah** header `#AE1`, jadi log `dbg.h` atau
   teks acak yang ikut ter-paste tidak langsung jadi preset.
6. Baris yang melebihi 80 B ditandai `impPanjang` dan ditolak saat baris
   berikutnya — buffer penuh tidak boleh jadi pemotongan diam-diam.

Semua penolakan disertai alasan spesifik di Serial
(mis. `indeks 4 tidak berurutan (harusnya 3)`), karena menyalin blok ke alat
yang salah adalah kesalahan yang mahal.

**Non-blocking.** Import menumpuk karakter per iterasi `loop()` ke buffer 80 B
dan memproses per Enter, jadi `loop()` tetap responsif selama transfer.
Escape hatch: mengetik baris berawalan `>` membatalkan import **dan** menjalankan
perintah itu — jadi tidak ada kondisi buntu di mode import. Export mencetak
~11 baris sekaligus (±5 ms di 115200 baud), dianggap wajar karena `dbg.h` sudah
mencetak di dalam `loop()`.

---

## [2026-10-16] v0.7.0 — Watchdog, recovery I2C, kontrol Serial (Bab 9)

Menutup 4 dari 5 item infrastruktur firmware.

**Watchdog hardware** (`watchdog.h`, baru). `esp_task_wdt` dengan timeout 10
detik + `trigger_panic`, sehingga `loop()` yang hang akibat bug / I2C macet
tetap menghasilkan reset otomatis — sebelumnya alat hanya bisa dipulihkan dengan
mematikan listrik manual. `watchdogInit()` dipanggil di `setup()`,
`watchdogFeed()` di `loop()` (maksimal tiap 1 detik, cukup sering). Kalau
Arduino core sudah mendaftarkan task ini saat boot (`init()` kena
`ESP_ERR_INVALID_STATE`), otomatis fallback ke `reconfigure()` timeout yang ada
— bukan membuat konfigurasi kedua. Kegagalan aktivasi **tidak dianggap fatal** — alat tetap jalan tanpa jaring
pengaman, supaya debugging lapangan tidak jadi sulit. 10 detik dipilih karena siklus loop terburuk hanya ~30 ms
(`oled.display()` memblokir I2C); jalur proses sudah bebas `delay()` panjang
sejak v0.6.1.

**Recovery bus I2C** (`input.h`). `bacaPCF8574()` sekarang menghitung
kegagalan beruntun; setelah 5 kali berturut-turut `i2cUpkeep()` (dipanggil tiap
iterasi `loop()`) memicu `i2cRecovery()`: `Wire.end()` → jeda 10 ms supaya slave
melepaskan SDA → `Wire.begin()` → verifikasi baca. Kalau recovery gagal,
penghitung sengaja **tidak** di-reset supaya langsung dicoba lagi di iterasi
berikutnya, bukan menunggu 5 kegagalan baru. Tujuannya: PCF8574 yang hiccup tidak
lagi membuat alat sepenuhnya tanpa kontrol.

**Kontrol via Serial** (`serialcmd.h`, baru). 7 perintah, format `>perintah` +
Enter: `help`, `status`, `start`, `stop`, `preset N`, `sel <halaman>`, `off`.
Parser membaca **satu karakter per iterasi `loop()`** dan mengeksekusi begitu
Enter masuk — sengaja tidak memakai `Serial.readString()` yang memblokir
sembari menunggu Enter. Prefix `>` dipilih supaya log `dbg.h` tidak ikut
diparse sebagai perintah.

**Cleaning `config.h`.** 12 `#define` mati dihapus: sisa dari desain sensor
analog yang sudah diganti touch-read (`INTERVAL_SENSOR_MS`, `SENSOR_AVG_N`,
`SENSOR_AKTIF_LOW`, `SENSOR_TOUCH_READ`, `TOUCH_DEBOUNCE`, `TOUCH_DELTA_*`,
`TOUCH_ADAPT_*`, `TOUTH_KALIB_SAMPEL`, `TOUCH_DELTA_PENUH_AWAL`,
`SENSOR_LOG_PERIOD_MS`). **`AMBANG_MARGIN` / `AMBANG_HYST` tidak dihapus** —
keduanya dipakai `sensor.h:sensorStatusUpdate()` untuk histeresis status ISI /
KOSONG. `SENSOR_BEKU_MS` dipindahkan ke `config.h` supaya konstanta ambang
sensor terpusat di satu tempat.

**Tidak dikerjakan:** logging ke file (butuh LittleFS dan mempercepat keausan flash,
ditunda dengan sengaja), `>export` / `>import` preset.

---

## [2026-10-15] v0.5.8 — LED status WS2812 (GPIO 7)

`PIN_LED_STRIP 7` sudah ada di `config.h` sejak lama tapi tidak pernah dipakai
kode. Sekarang diisi modul LED status supaya operator bisa melihat status sistem
tanpa harus melihat OLED.

**File baru `led_strip.h`** — WS2812/NeoPixel 1 LED di GPIO 7, 8 mode:

| Mode | Warna | Animasi | Kapan |
|---|---|---|---|
| `LED_BOOT` | biru | kelap-kelip 300 ms | selama boot |
| `LED_IDLE` | hijau redup | solid | siap, tidak ada proses |
| `LED_PROSES` | biru | breathing (sinus ~1.6 s) | DOSING/ETCHING/DRAINING/RINSING |
| `LED_JEDA` | kuning | breathing lambat (~2 s) | proses dijeda |
| `LED_SELESAI` | hijau | solid 2 detik → IDLE | proses tamat |
| `LED_ERROR` | merah | kelap-kelip cepat 120 ms | proses dibatalkan |
| `LED_ESTOP` | merah | solid | pin GPIO 8 LOW |
| `LED_WARNING` | kuning | solid | tersedia untuk proteksi gas/suhu |

**Non-blocking** — semua animasi dijalankan di `ledStripUpdate()` (dipanggil di
`loop()`), berbasis `millis()`. Breathing memakai `sinf()` supaya kecerahan
naik-turun halus, bukan menyala-mati bergantian.

**Integrasi**
- `.ino`: `#include "led_strip.h"`, `setup()` → `ledStripInit()` + `ledBoot()`,
  akhir `setup()` → `ledIdle()`, `loop()` → `ledStripUpdate()` tiap iterasi,
  handler E-stop → `ledEstop()`.
- `process.h`: `aksiFase()` → `ledProses()` di tiap fase kerja;
  `prosesToggleJeda()` → `ledJeda()` dan `aksiFase()` ulang saat dilanjut;
  `prosesBatal()` → `ledError()`; `prosesKeluarSelesai()` → `ledIdle()`;
  `prosesUpdate()` saat `FASE_SELESAI` → `ledSelesai()`.

**Library baru:** Adafruit NeoPixel.

**`PROGRESS.md`** — item "LED status / LED strip (GPIO 7)" dicentang; ringkasan
GPIO & Hardware 9 ✅ → 10 ✅.

---

## [2026-10-15] v0.5.9 — Perbaiki sisa merge upstream (build rusak) + indikator sensor LED (gated)

Merge upstream (`2c84273`) menyisakan **3 file tidak bisa dikompilasi** —
`.ino` masih memakai API sensor versi lama (`dsOffsetX10`, `JUMLAH_KANAL_SUHU`,
`udaraSiap()`, `mq135Baseline`/`mq135Margin`, `HAL_KALIB_SUHU_LIST`) sementara
`sensor.h` sudah berisi desain baru 3-sensor-1-bus (`sensorSuhu()`, `suhuOffsetX10[]`,
`kalibSuhuAddr[]`, `mq135PPM()`), dan `sensor_extra.h` versi upstream menggantikan
versi lama. Akibatnya `gambarKalibSuhu()`/`gambarKalibUdara()` ikut hilang dari
`ui.h`.

**Build fix** — diselaraskan ke satu desain (3 sensor pada 1 bus 1-Wire):

| File | Perubahan |
|---|---|
| `config.h` | Tambah `PIN_DS18B20 10` (bus tunggal). `PIN_DS_CHAMBER/CAIRAN/MOTOR` jadi alias + catatan "tidak dipakai" |
| `data.h` | Buang `HAL_KALIB_SUHU_LIST` (halaman tunggal, bukan list+detail). State suhu → `JUMLAH_SUHU`, `namaSensorSuhu[3]`, `kalibSuhuIdx/Scroll/Field`, `SUHU_ROW_TOP/ROW_H/CONTENT_H`. State udara → `kalibUdaraSel`, `mq135Normal`, `mq135Kotor`, `mq135Bacaan` |
| `ui.h` | Kembalikan `gambarKalibSuhu()` + `gambarKalibUdara()` (3 blok: addr + suhu; 2 baris acuan + ppm live) dan daftarkan di `gambarLayar()` |
| `auto-etching-firmware.ino` | `HAL_KALIB_SUHU`: ATAS/BAWAH pindah blok + auto-scroll, ENTER pindah field (addr↔suhu), KIRI/KANAN ubah addr 1–3 atau offset ±0,1 °C. `HAL_KALIB_UDARA`: ATAS/BAWAH pilih Normal/Kotor, ENTER rekam ppm. Loop live → `sensorSuhuUpdate()` + `mq135Bacaan` |
| `storage.h` | `FORMAT_VERSI_KALIB2` 1 → **2**. `DataKalib2` ganti `dsOffsetX10`/`mq135Baseline`/`mq135Margin` → `addrSensor[3]`/`suhuOffsetX10[3]`/`mq135Normal`/`mq135Kotor` (semua `int16_t`, CRC tetap) |

Blob `"kal2"` versi 1 otomatis ditolak (versi beda) → default, tidak merusak
kalibrasi level `"kal"` yang sudah tersimpan.

**Indikator RGB per sensor** (`led_strip.h`) — kode lengkap tapi **default non-aktif**:
`config.h` `SENSOR_LED_ENABLE 0` karena GPIO 47 masih dipakai DRV8825 sebagai
`PIN_STEP3`. Strip 5 pixel di GPIO 47 (tentatif): warna hijau ≥20%, kuning 5–19%,
merah <5%; tiap komponen warna diskalakan level sehingga kecerahan = "bar" liquid.
Throttle 250 ms + redraw hanya bila sensor berubah ≥2%. Hook `ledSensorInit()` di
`setup()` dan `ledSensorUpdate()` di `loop()`, keduanya di-`#if SENSOR_LED_ENABLE`.
Aktifkan nanti cukup set `SENSOR_LED_ENABLE 1` + `PIN_LED_SENSOR` ke pin yang
benar-benar bebas — tidak perlu ubah logika.

**Compile:** sukses — 470693 bytes (35%) program, 58592 bytes (17%) RAM.

**`PROGRESS.md`** — Bab 1 GPIO & Hardware diselaraskan dengan kode aktual:
TMC2209 jadi 1 item consolidated (butuh konfirmasi hardware), DS18B20 & MQ-135
diperbarui, item RGB per sensor dicatat "kode ada, belum diaktifkan".

---

## [2026-10-15] v0.6.0 — Migrasi stepper DRV8825 → TMC2209

Driver pompa peristaltik diganti dari DRV8825 ke TMC2209. Ini **breaking change**
di pin map: firmware lama tidak akan lagi cocok dengan wiring PCB baru.

**Konfirmasi hardware (dari pemilik repo):**
- `EN` / `PDN` di-OR-kan ke ketiga driver — **1 pasang pin untuk 3 channel**
- Mikrostep **1/16** → 3200 step/rev (rumus umum `200 × 2ⁿ`)

### Pin map baru (`config.h`)

| Fungsi | GPIO | Lama (DRV8825) |
|---|---|---|
| TMC_EN (shared 3 driver) | 1 | `PIN_SLP1` |
| TMC_PDN (shared 3 driver) | 2 | `PIN_STEP1` |
| STEP HCl | 38 | `PIN_SLP3` |
| DIR HCl | 48 | `PIN_DIR1` (42) |
| STEP H2O2 | 40 | `PIN_STEP2` (sama) |
| DIR H2O2 | 39 | `PIN_DIR2` (sama) |
| STEP Air | 42 | `PIN_DIR1` |
| DIR Air | 41 | `PIN_SLP2` |

`PIN_SLP1/2/3` dihapus. GPIO **21 & 47** kini bebas.

### ⚠️ Jebakan polaritas

DRV8825 `SLP`: **HIGH = aktif**. TMC2209 `EN`: **LOW = aktif** — kebalikan.
Kalau tidak dibalik, driver akan selalu tidur dan pompa diam total tanpa error.

Konstanta konfirmasi polaritas supaya tidak terbalik lagi:
```cpp
#define TMC_EN_AKTIF_LOW  1     // penanda di config.h
#define TMC_PDN_RESET_US  5     // pulse reset PDN (datasheet: >= 1 us)
#define STEP_PER_REV      3200  // mikrostep 1/16
```

### `motor.h`

- `motorInit()` — urutan: pin mode STEP/DIR → pin mode EN/PDN →
  `stepperSleep(true)` → pulse reset PDN (LOW 5 µs → HIGH) → `stepperSleep(false)`
  → `hitungLangkahPerMl()`. Print konfirmasi ke Serial.
- `stepperSleep(bool)` — **`true` = tidur** (EN = HIGH), `false` = aktif (EN = LOW).
- `pompaMatikanTotal()` — `pompaStopAll()` + `stepperSleep(true)`. Driver benar-benar
  tidur, motor tidak menahan torsi, catu daya tidak terbuang sia-sia.
- `hitungLangkahPerMl()` — ganti konstanta `7200.0` → `(float)STEP_PER_REV`.
- `LANGKAH_INTERVAL_US` **400 → 1200 µs** (~830 Hz). 400 µs dengan 3 pompa paralel
  berisiko step-loss pada ESP32-S3 yang menambah jitter dari render OLED.
- `pompaMulai()` kini memanggil `stepperSleep(false)` — aman dipanggil ulang.

### `process.h` — `semuaAktuatorMati()`

Tiga tempat (aksiFase SELESAI, prosesBatal, handler E-stop) sebelumnya punya 4
baris stop identik. Sekarang satu fungsi tunggal, dipanggil dari ketiganya dan
menggunakan `pompaMatikanTotal()` sehingga driver ikut tidur.

### Indikator RGB per sensor — diaktifkan

`SENSOR_LED_ENABLE 0 → 1`, `PIN_LED_SENSOR 47`. GPIO 47 bebas sejak migrasi.
5 pixel WS2812: hijau ≥20%, kuning 5–19%, merah <5%; tiap komponen warna
diskalakan level sehingga kecerahan = bar liquid. Throttle 250 ms + redraw hanya
bila sensor berubah ≥2%.

### ⚠️ Kalibrasi dosing WAJIB diulang

`langkahPerMl` berubah dari basis 7200 step/rev ke 3200 step/rev. Nilai kalibrasi
lama di NVS blob `"kal3"` **tidak valid**. Jalankan ulang:
`Kalibrasi → Motor HCl / H2O2 / Air` → set ml/rotasi → BACK (otomatis simpan).

### Dokumentasi

- `PROGRESS.md` §1: TMC2209 → ✅, RGB per sensor → ✅. §3: 8 item lama yang sudah
  terpecahkan (langkahPerMl dari kalibrasi, NVS blob `kal3`, STEP_INTERVAL_US)
  dicentang.
- `PROCESS.md` §1.2 ditulis ulang sebagai "SUDAH DITERAPKAN (v0.6.0)". §12
  pertanyaan terbuka #1 & #2 ditandai TERJAWAB.
- `SUMMARY.md`: pin map TMC2209 lengkap + GPIO 21/36/45/47 status.

**Compile:** sukses — 471253 bytes (35%) program, 58648 bytes (17%) RAM.

---

## [2026-10-15] v0.6.1 — 4 item Modul UI: versi di OLED, status ISI/KOSONG, hapus placeholder, kalibrasi non-blocking

Empat item ⬜ di `PROGRESS.md` §2 (Modul UI) yang bisa diselesaikan tanpa
menunggu logika proses lain. Item yang masih ⬜ karena bergantung Bab 3/4/6
dibiarkan apa adanya.

### 1. Nomor versi firmware di OLED

`FW_VERSI` tadinya hanya dicetak ke Serial. Sekarang tampil di pojok kanan atas
halaman Menu Utama. Halaman ini tidak memakai `gambarHeader()` (bukan halaman
daftar), jadi `areaAtas` `gambarList()` dinaikkan 0 → 10 px supaya baris list
tidak pernah menimpa baris versi.

### 2. Status ISI / KOSONG di Level Cairan

`config.h` sudah punya `AMBANG_MARGIN` (10) dan `AMBANG_HYST` (3) sejak lama
tetapi tidak pernah dipakai —Persis masalah yang dicatat `PROGRESS.md`.

`sensor.h` — status per sensor dengan histeresis:
```cpp
bool sensorAdaCairanState[JUMLAH_SENSOR];
void sensorStatusReset();
bool sensorAdaCairan(int i);
void sensorStatusUpdate();     // dipanggil dari sensorUpdate()
```
Naik ke ADA saat `persen >= AMBANG_MARGIN`, turun ke KOSONG saat
`persen < AMBANG_MARGIN - AMBANG_HYST`. State lama dipakai sebagai titik awal
agar histeresis benar-benar bekerja — bukan sekadar membandingkan dua angka,
yang akan berkedip saat level berdiri tepat di ambang.

`ui.h:gambarLevel()` — baris 1 tetap nama tangki + persen; baris 2 diisi
status di kiri (label `ISI` dibalik warnanya supaya kontras tanpa perlu warna,
`KOSONG` polos) dan bar progres digeser ke kanan (`STATUS_W = 40`).

### 3. Hapus `HAL_PLACEHOLDER`

Kode mati sejak v0.5.2 — tidak ada `pindahHalaman(HAL_PLACEHOLDER)` di mana pun,
jadi 4 bagian ini tidak pernah terjangkau: enum di `data.h`, entri
`namaHalaman[]`, `gambarPlaceholder()` + `case` di `ui.h`, `case` di `.ino`.
Semuanya dihapus.

### 4. Kalibrasi Agitasi & Fan tidak lagi membekukan UI

Tiga `delay()` (`delay(1000)`, `delay(2000)` ×2) membuat layar beku dan tombol
tidak responsif selama 1–2 detik.

`.ino` — helper baru:
```cpp
void mulaiTestMotor(int jenis, int durasiMs);   // 1=agitasi, 2=agitasi semua, 3=fan
void upkeepKalibrasiMotor();                   // dipanggil dari loop()
```
Test "semua kecepatan" tidak lagi `for` + `delay`: tiap kecepatan dijadwalkan
satu per satu lewat `kalibMotorSelesaiMs += 1000`, dengan toast `Test 1/3...`
sampai `3/3...`.

`data.h` — `kalibMotorRunning` yang tadinya **di-set `true` tapi tidak pernah
dibaca** (item ⚠️ di `PROGRESS.md` §7) kini jadi flag aktif. Ditambah
`kalibMotorSelesaiMs`, `kalibMotorJenis`, `kalibMotorStep`.

BACK di kedua halaman kalibrasi sekarang mematikan motor lebih dulu kalau test
masih jalan — sebelumnya motor bisa terus nyala sampai user tebak-tebak.

**Compile:** sukses — 471625 bytes (35%) program, 58664 bytes (17%) RAM.

**`PROGRESS.md` §2:** 16 ✅ / 12 ⬜ → **20 ✅ / 8 ⬜**. Total 93/9/65 → **97/9/61**.

---

## [2026-10-15] v0.6.2 — Blob NVS "kal4": duty PWM & ml hasil uji motor DC

MulaiBab 7 (Sensor & Kalibrasi). Item pertama: `kalibDuty[]` & `kalibMl[]`
yang tadinya **volatile** — setiap reboot semua nilai PWM motor agitasi, pompa
sabun, pompa limbah, dan exhaust fan kembali ke default inisialisasi.

**`storage.h` — blob baru `"kal4"`**

```cpp
struct DataKalib4 {
  uint32_t versi;
  int16_t  duty[JUMLAH_KALIB_DUTY];   // 7 — PWM 0..255
  int16_t  mlX10[JUMLAH_KALIB_ML];     // 2 — ml x10, 0 = belum diuji
  uint32_t crc;
};
```

Layout `kalibDuty[7]` mengikuti `data.h`:
`[0]` pompa Sabun · `[1]` pompa Limbah · `[2..4]` agitasi speed 1–3 ·
`[5]` belum dipakai · `[6]` exhaust fan.

`kalibMl[]` bertipe `float` di RAM, disimpan sebagai `x10` (`int16_t`) supaya
sizeof struct tetap pasti dan tidak ikut bergantung rounding float. Default
bila blob belum ada: duty **128** (~50% — motor tetap berputar tanpa kepanasan),
ml **0** (belum diuji).

Blob terpisah dari `"kal"`/`"kal2"`/`"kal3"` supaya perubahan struct di sini
tidak membuat kalibrasi lain hilang.

**`.ino`** — `muatKalib4()` di `setup()`, `simpanKalib4()` di BACK dari tiga
halaman: `HAL_KALIB_DC`, `HAL_KALIB_AGITASI`, `HAL_KALIB_FAN`. Di
`HAL_KALIB_DC` sekalian dimatikan motor kalau test masih jalan.

### Audit 3 item ⚠️ Bab 7 — 2 ternyata sudah selesai

- `kalibSuhuAddr[]` & `suhuOffsetX10[]` → NVS: **sudah** sejak v0.6.0
  (`DataKalib2` versi 2)
- `mq135Normal`/`mq135Kotor` → NVS: **sudah** sejak v0.6.0 (bidang yang sama)
- Auto-stop motor kalibrasi ganti `delay()`: **sudah** sejak v0.6.1

### ⚠️ Temuan: `kalibDuty[]` disimpan tapi belum dipakai motor

Setelah ditelusuri, `kalibDuty[]` **hanya dibaca untuk tampilan** (`ui.h`).
`dcmotor.h` masih `agitasiSet()` memetakan speed 1–5 → PWM 80–255 tetap dan
`fanSet()` on/off tetap, padahal `agitasiSetDuty()` dan `fanSetAt()` sudah ada
di sana tapi belum dipakai.

Artinya: tanpa perubahan di `dcmotor.h`/`process.h`, nilai yang baru disimpan
hanya bertahan — belum mengubah perilaku motor. Dicatat sebagai item baru
di `PROGRESS.md` §7, dengan catatan file tersebut milik Orang A (pembagian
kerja dua orang), jadi sengaja tidak disentuh di versi ini.

**Compile:** sukses — 472393 bytes (36%) program, 58664 bytes (17%) RAM.

**`PROGRESS.md` §7:** 15/4/3 → **18/1/4**. Total 98/8/61 → **101/5/61**.

---

## [2026-10-15] v0.6.3 — Address DS18B20 dipindai otomatis, bukan hardcode

Bab 7 item 5. Tiga `DeviceAddress` sebelumnya ditulis manual di `sensor.h:119-123`:

```cpp
DeviceAddress sensorAddress[3] = {
  { 0x28, 0xB8, 0xEF, 0x87, 0x3A, 0x25, 0x02, 0x64 },   // ??? sensor apa ini?
  ...
};
```

Kalau address di board fisik tidak persis sama —misal sensor ditukar, atau
dipindai dengan konfigurasi lain — maka `sensorSuhu()` selalu mengembalikan
`DEVICE_DISCONNECTED_C` dan ketiga blok tampil `--.-C` tanpa ada petunjuk sensor
mana yang bermasalah.

### Perubahan

**`sensor.h`**
- `sensorAddress[3]` diinisialisasi nol + `sensorAddressValid[3]` sebagai
  penanda slot terisi. Tidak ada lagi address yangbakal ditulis di firmware.
- `sensorSimpanAddress()` memindai bus lewat `getAddress()`, mengisi slot
  0..2 sesuai urutan bus, menandai sisa slot sebagai kosong.
- `sensorSuhuInit()`: coba muat dari NVS dulu; kalau belum ada, pindai bus lalu
  simpan hasilnya. `setResolution(12)` hanya dipasang ke slot yang benar-benar
  terisi.
- `sensorSuhuRaw()` mengembalikan -127 lebih awal kalau slot yang dipilih tidak
  terisi, supaya tidak memanggil `getTempC()` dengan address nol.
- `sensorPrintAddress()` mencetak 16 digit hex per slot saat boot pertama.
  Diformat dengan `snprintf` karena `DbgPort` tidak punya `print(uint8_t, base)`.

### NVS blob baru `"kal5"` (`storage.h`)

```cpp
struct DataKalib5 {
  uint32_t versi;
  uint8_t  addr[3][8];   // DeviceAddress per slot, 0 = kosong
  uint8_t  valid;         // bitmask per slot
  uint32_t crc;
};
```

**Dipisah dari `"kal2"` dengan sengaja.** Kalau address ikut disisipkan ke
`DataKalib2`, maka `FORMAT_VERSI_KALIB2` harus naik dan **semua kalibrasi suhu
yang sudah tersimpan di alat ikut ditolak**. address hasil pindaian juga tidak
boleh hilang hanya karena user menulis ulang offset. Memisahkan menjaga
`FORMAT_VERSI_KALIB2` tetap 2 sehingga blob lama tidak terganggu.

Saat muat, **family code 0x28 diverifikasi** per slot. Blob dengan isi acak atau
dari versi lama ditolak, lalu bus dipindai ulang.

### Catatan operasional

Urutan `getAddress()` mengikuti urutan enumerate di bus, **dan tidak dijamin sama
dengan posisi fisik**. Setelah boot pertama, cocokkan output Serial dengan label
kabel:

```
[SUHU] blok 0 addr=28B8EF873A250264
[SUHU] blok 1 addr=288077483A250202
[SUHU] blok 2 addr=288EFD2339250287
```

Blok 0/1/2 = "Suhu Motor" / "Suhu Komponen" / "Suhu Chamber". Kalau urutannya
salah dengan label fisik, blok bisa dipindah di halaman Kalibrasi Sensor Suhu
(KIRI/KANAN pada field `addr`) — nilainya disimpan di `"kal2"`.

Untuk memaksa pindai ulang (mis. setelah menukar sensor): ubah urutan di UI
Kalibrasi Sensor Suhu lalu BACK akan menyimpan ulang, atau hapus blob `"kal5"`
secara manual dari NVS.

### Sisa Bab 7

- ⬜ Deteksi sensor touch mati (`sensor.h`)
- ⬜ Terapkan `kalibDuty[]` ke motor — file `dcmotor.h` milik Orang A
- ⬜ Simpan `simpanKalib2()` dari BACK `HAL_KALIB_SUHU`/`HAL_KALIB_UDARA` —
  masih bug warisan merge `2c84273`, ditunda atas permintaan pemilik

**Compile:** sukses — 473305 bytes (36%) program, 58656 bytes (17%) RAM.

**`PROGRESS.md` §7:** 18/1/4 → **19/0/4**. Total 101/5/61 → **102/4/61**.

---

## [2026-10-15] v0.6.4 — Kalibrasi Sensor Suhu & MQ-135 akhirnya tersimpan ke NVS

Bug warisan merge `2c84273` ditutup. `simpanKalib2()` **ada di `storage.h`
sejak v0.6.0 tapi tidak pernah dipanggil dari mana pun** — offset suhu, pilihan
address blok, dan acuan MQ-135 hilang setiap reboot.

### Asal-usulnya

Sebelum merge, kedua halaman ini memakai dialog (`ENTER` simpan / `BACK` buang)
yang memanggil `simpanKalib2()`. Saat konflik merge di-resolve, blok `case`
di `.ino` digantikan versi upstream yang hanya `pindahHalaman()`. Pemanggilnya
ikut hilang — dan tidak ketahuan karena firmware belum dikompilasi ulang.

Yang tersisa: struct `DataKalib2` di `storage.h` lengkap dan benar. Fungsi
penyimpan dan pemuatnya ada. **Tidak ada satu pun yang menunjuk ke fungsi itu.**

### Perbaikan

`.ino` — dua blok BACK:
```cpp
case HAL_KALIB_SUHU:
  if (t == P_BACK) {
    kalibSuhuScroll = 0;
    tampilToast(simpanKalib2() ? "Kalibrasi tersimpan" : "Gagal simpan!");
    pindahHalaman(HAL_KALIB_MAIN);
    break;
  }

case HAL_KALIB_UDARA:
  if (t == P_BACK) {
    tampilToast(simpanKalib2() ? "Kalibrasi tersimpan" : "Gagal simpan!");
    pindahHalaman(HAL_KALIB_MAIN);
    break;
  }
```

Tanpa dialog, mengikuti `SUMMARY.md`: "ENTER aksi · **BACK keluar + simpan**" —
pola yang sama dengan 5 halaman kalibrasi lain. Ditambah toast supaya operator
tahu penyimpanan berhasil atau gagal; sebelumnya tidak ada umpan balik sama
sekali saat BACK ditekan.

### Koreksi dokumentasi

Ternyata `PROGRESS.md:161` (yang saya tulis sendiri di v0.6.0) **salah**. Baris
itu mengklaim kalibrasi suhu "sudah disimpan ke NVS ... ditulis `simpanKalib2()`
saat BACK dari `HAL_KALIB_SUHU`" — saya menyimpulkan itu dari **isi struct
`DataKalib2`**, tanpa memeriksa apakah fungsinya benar-benar dipanggil.

Akibatnya checklist sempat menandai ✅ untuk sesuatu yang tidak berfungsi. Baris
161 & 162 sekarang ditulis ulang: Explainsi apa adanya bug-nya, kapan ditutup,
dan mana pemanggilnya. Item yang tadinya tertunda di §7 "Simpan `simpanKalib2()`
dari BACK" juga dicentang.

Tiga item §8 "Penyimpanan & Data" yang tadinya `[ ]` padahal sudah dikerjakan
juga dicentang, karena ketidakkonsistenan yang sama: `kal3` sudah sejak v0.5.7,
`kal4` sejak v0.6.2, `kal2` sejak v0.6.4.

### Verifikasi di Serial Monitor

Setelah naik `kal2` dari versi 1 ke 2 (saat v0.6.0), blob versi lama otomatis
ditolak karena `versi` tidak cocok → default. Jadi setelah flash ulang:

```
[STORE] Kalibrasi suhu/udara belum ada (default)   ← sekali saja, wajar
...
BACK dari Kalibrasi Sensor Suhu:
[STORE] Simpan kalibrasi suhu/udara: OK
```

**Semua 7 halaman kalibrasi sekarang tersimpan ke NVS saat BACK.** Tidak ada lagi
halaman kalibrasi yang diam-diam membuang nilainya.

**Compile:** sukses — 473581 bytes (36%) program, 58656 bytes (17%) RAM.

**`PROGRESS.md`** §8: 7/0/5 → **10/0/2**. Total 102/4/61 → **105/4/58**.

---

## [2026-10-15] v0.6.5 — Deteksi sensor level mati (raw beku di 0)

Bab 7 item terakhir milik sisi Antarmuka. Sebelumnya `sensorOk` cuma satu flag
global untuk kelima sensor — kalau satu strip nikel putus, firmware tidak tahu,
dan hanya menampilkan `0%` atau `100%` seperti biasa.

Yang paling berbahaya bukan tampilannya salah, tapi **nilainya dipakai untuk
keputusan**: `FASE_CEK` (Bab 3) akan memakainya untuk mengecek "apakah tangki
HCl cukup cairan sebelum dosing". Sensor putus yang baca 0% berarti sistem mengira
tangki kosong — lalu gagal dengan pesan yang menyesatkan.

### Cara deteksi

`touchRead()` yang sehat **selalu mengembalikan nilai non-nol**. Jadi syaratnya
sempit: raw **0** dan bertahan **60 detik** (`SENSOR_BEKU_MS`).

Syaratnya sengaja tidak dibuat "nilai tidak berubah" — karena tangki kosong yang
wajar juga nilainya diam, dan itu kondisi normal, bukan kerusakan.

```cpp
bool sensorMati[JUMLAH_SENSOR];
bool sensorMatiDilaporkan[JUMLAH_SENSOR];   // cegah log berulang tiap 50 ms
unsigned long sensorBekuMs[JUMLAH_SENSOR];
```

- Nilai != 0 → reset penghitung, `sensorMati = false`. **Pulih sendiri** tanpa
  perlu reboot atau reset manual.
- Nilai == 0 > 60 s → `sensorMati = true`, log sekali:
  `[SENSOR] HCl MATI - raw beku di 0, cek strip nikel & kabel AWG30`

### Tampilan

`ui.h:gambarLevel()` — persen diganti `DEAD`, dan isi bar progres dikosongkan.
Menampilkan `0%` dengan bar kosong untuk sensor yang putus **lebih berbahaya**
daripada menampilkan `DEAD`: yang pertama terlihat seperti data valid.

### Batas deteksi (dicatat di PROGRESS.md)

Strip yang sudah tidak sensitif — buildup karat, HDPE terlalu tebal — masih
bergerak sedikit, jadi **tidak** tertangkap. Yang tertangkap hanya kasus beku
total, yaitu kabel yang benar-benar putus atau strip yang copot.

Kalau butuh deteksi yang lebih tajam, jalannya lewat uji manual: masuk ke
Kalibrasi Sensor Level, tangki dikosongkan lalu diisi, lihat apakah persen
ikut berubah. Itu masih item uji di Bab 11.

### Compile

Sukses — 474089 bytes (36%) program, 58688 bytes (17%) RAM.

**`PROGRESS.md` §7:** 19/0/4 → **20/0/3**. Total 105/4/58 → **106/4/57**.

Bab 7 sisa 2 item, keduanya **bukan milik sisi Antarmuka**:
- Terapkan `kalibDuty[]` ke motor — file `dcmotor.h`
- Simpan semua parameter kalibrasi ke NVS — sudah selesai, tinggal centang
  setelah item di atas beres

---

## [2026-10-15] v0.6.6 — PWM motor memakai hasil kalibrasi + Bab 7 tuntas

Item terakhir Bab 7. `kalibDuty[]` sudah **tersimpan ke NVS sejak v0.6.2**, tapi
tidak pernah dipakai motor — semua PWM masih konstanta. Jadi nilai yang dikalibrasi
lewat menu Kalibrasi selama ini **tidak berefek apa pun**.

### `dcmotor.h`

```cpp
// SEBELUM — konstanta, kalibrasi diabaikan
agitasiSet(speed)    → map(speed, 1..5, 80..255)
pompaSabunSet(true)  → 255
pompaLimbahSet(true) → 255
fanSet(true)         → 255

// SESUDAH — dari hasil kalibrasi
pompaSabunSet(on)    → kalibDuty[0]
pompaLimbahSet(on)   → kalibDuty[1]
fanSet(on)           → kalibDuty[6]
agitasiSet(speed)    → agitasiPwm(speed)
```

Layout `kalibDuty[7]`: `[0]` Sabun · `[1]` Limbah · `[2..4]` agitasi spd 1–3 ·
`[5]` tidak dipakai · `[6]` fan.

### Interpolasi agitasi

Preset punya **5** tingkat speed, tapi hanya **3** titik kalibrasi. Mapping
sederhana akan membuat speed 4–5 melompat ke nilai yang sama dengan speed 3 —
hasil kalibrasi di tengah skala jadi mubazir dan tingkat terasa tidak rata.

`agitasiPwm()` karena itu meng-interpolasi linier di antara titik:

| speed | posisi | duty |
|---|---|---|
| 1 | titik | `kalibDuty[2]` |
| 2 | tengah | rata-rata `kalibDuty[2]` & `[3]` |
| 3 | titik | `kalibDuty[3]` |
| 4 | tengah | rata-rata `kalibDuty[3]` & `[4]` |
| 5 | titik | `kalibDuty[4]` |

Semua titik kalibrasi terpakai, dan tidak ada tingkat yang meloncat.

`dcmotorInit()` sudah dipanggil **setelah** `muatKalib4()` di `setup()`, jadi nilai
kalibrasi tersedia sebelum motor pertama kali dihidupkan. Default semua slot
**128** (~50% duty) kalau blob belum ada — motor tetap berputar normal, tidak diam
total seperti kalau defaultnya 0.

### Catatan dokumentasi

Catatan "file milik Orang A" pada item ini dihapus. Pembagian kerja dua orang itu
hanya latihan planning, tidak pernah dipakai — repo ini dikerjakan satu orang.

### PROGRESS.md

- §7: 20/0/3 → **22/0/1**. Item tersisa hanya "default baseline per-sensor"
  (prioritas rendah, baru berpengaruh sebelum tangki dikalibrasi).
- Item "Simpan semua parameter kalibrasi ke NVS" dicentang dengan rincian
  **5 blob**: `kal` · `kal2` · `kal3` · `kal4` · `kal5`.
- Baris "Baseline default 113000/156000" dianotasi: angkanya **hasil ukur nyata**
  dari pilot sketch (skala `touchRead()` ESP32-S3 memang ratusan ribu), tapi
  dipakai seragam untuk kelima sensor → error ±12% di tengah skala.

**Total: 106/4/57 → 108/4/55.**

**Compile:** sukses.

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
