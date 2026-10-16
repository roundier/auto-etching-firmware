#pragma once
// ===== Konfigurasi hardware & konstanta global =====

// I2C (bus bersama OLED + PCF8574)
#define SDA_PIN         4
#define SCL_PIN         5
#define I2C_CLOCK       400000

// OLED SSD1306
#define OLED_ADDR       0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// Baris paling atas panel OLED (~10px) rusak.
// Semua UI digambar pada koordinat virtual 0..SCREEN_HEIGHT-1, lalu digeser
// ke bawah UI_Y_OFFSET px (bagian paling bawah terpotong, sudah dihindari
// dengan Area_Bawah di ui.h). Set 0 bila OLED normal.
#define UI_Y_OFFSET     15

// PCF8574 (6 tombol)
#define PCF8574_ADDR    0x20
#define JUMLAH_TOMBOL   6
#define DEBOUNCE_MS     40
#define TIDAK_ADA       (-1)

#define P_ATAS   0
#define P_BAWAH  1
// KIRI/KANAN ditukar sejak v0.4 (P3 = KIRI, P2 = KANAN)
#define P_KIRI   3
#define P_KANAN  2
#define P_ENTER  4
#define P_BACK   5

// Render
#define INTERVAL_RENDER_MS  20

// Aktifkan untuk melihat jumlah render per detik di Serial
// (saat idle harus ~0, saat animasi ~50)
// #define DEBUG_RENDER

// Hanya untuk uji: perlambat render (ms) untuk membuktikan timer proses
// tidak bergantung pada render. Nonaktifkan setelah uji.
// #define DEBUG_LAMBAT_RENDER_MS  300

// Penyimpanan (Preferences/NVS, ESP32)
#define PREF_NS     "etch"

// Batas volume total campuran per proses (ml) - sesuaikan dgn kapasitas wadah/pompa
#define VOL_MIN     50
#define VOL_MAX     1000

// Sensor level cairan (GPIO langsung, ganti MPR121)
#define JUMLAH_SENSOR       5      // HCl, H2O2, H2O, air sabun, air limbah
#define PIN_LEVEL_HCL       11
#define PIN_LEVEL_H2O2      12
#define PIN_LEVEL_H2O       13
#define PIN_LEVEL_SABUN     14
#define PIN_LEVEL_LIMBAH    3

// Ambang status ISI/KOSONG (dipakai `sensor.h:sensorStatusUpdate()`):
// naik ke ADA saat persen >= MARGIN, turun lagi saat < MARGIN - HYST, supaya
// indikator tidak berkedip saat level berdiri tepat di ambang.
#define AMBANG_MARGIN       10     // %
#define AMBANG_HYST         3      // %

// Berapa lama `touchRead()` harus beku di 0 sebelum sensor dicurigai mati
// (kabel putus / strip copot). Dipakai `sensor.h:sensorMatiUpdate()`.
#define SENSOR_BEKU_MS      60000UL

// Driver TB6612FNG x2 (1 arah: INA->3.3V, INB->GND)
#define PIN_PWA_D1          15    // driver 1 PWA: motor agitasi
#define PIN_PWB_D1          16    // driver 1 PWB: pompa air sabun
#define PIN_PWA_D2          17    // driver 2 PWA: pompa limbah (drain)
#define PIN_PWB_D2          18    // driver 2 PWB: exhaust fan

// ===== PIN MAP (ESP32-S3) =====
// Stepper TMC2209 x3 (pompa peristaltik) — pengganti DRV8825.
// EN & PDN di-OR-kan ke ketiga driver (1 pasang pin untuk 3 channel).
#define PIN_TMC_EN      1      // enable seluruh driver (AKTIF LOW)
#define PIN_TMC_PDN     2      // power-down / reset seluruh driver (AKTIF LOW)
#define PIN_STEP1       38     // HCl
#define PIN_DIR1        48     // HCl
#define PIN_STEP2       40     // H2O2
#define PIN_DIR2        39     // H2O2
#define PIN_STEP3       42     // Air / H2O
#define PIN_DIR3        41     // Air / H2O

// TMC2209: EN aktif LOW (driver menyala saat LOW, mati saat HIGH).
// Konstanta ini hanya penanda polaritas supaya tidak salah tulis di motor.h —
// jangan diubah kecuali memang memakai driver dengan polaritas kebalikan.
#define TMC_EN_AKTIF_LOW    1
// Durasi pulse reset PDN saat init (us) — datasheet TMC2209 butuh reset >= 1 us.
#define TMC_PDN_RESET_US    5

// Mikrostep TMC2209 = 1/16 -> 200 step x 16 = 3200 step/putaran.
// PENTING: kalau jumper MS1/MS2 diubah, angka ini harus ikut disesuaikan.
// Rumus umum: 200 * (2^n) untuk mikrostep 1/2^n.
#define STEP_PER_REV    3200
// Lain-lain
#define PIN_BUZZER      6
#define PIN_LED_STRIP   7    // WS2812 status sistem (1 LED) - lihat led_strip.h

// ===== RGB WS2812 indikator per sensor (5 sensor) =====
// GPIO 47 (dari PIN_STEP3 DRV8825 lama) + GPIO 21 (dari PIN_DIR3) kini bebas
// setelah migrasi ke TMC2209.
#define SENSOR_LED_ENABLE  1
#define PIN_LED_SENSOR     47   // bebas sejak migrasi TMC2209
#define SENSOR_LED_PIXELS  5    // HCl, H2O2, H2O, Sabun, Limbah
#define SENSOR_LED_BRI     40   // 0-255 (lebih redup dari status supaya tidak menyilaukan)
// Periode redraw indikator sensor (ms). 250 ms = 4x per detik, cukup untuk
// level liquid yang berubah lambat tanpa membanjiri bus WS2812.
#define INTERVAL_LED_SENSOR_MS 250
// 1-Wire DS18B20. Design final: TIGA sensor pada SATU bus (GPIO10), dibedakan
// lewat DeviceAddress di `sensor.h` (Motor / Komponen / Chamber). Define
// per-kanal di bawah hanya sisa dari desain 3-bus yang sudah tidak dipakai.
#define PIN_DS18B20      10    // bus 1-Wire tunggal untuk 3 sensor DS18B20
#define PIN_DS_CHAMBER   10    // (dipakai lewat PIN_DS18B20)
#define PIN_DS_CAIRAN    45    // tidak dipakai — sisa desain 3-bus
#define PIN_DS_MOTOR     36    // tidak dipakai — sisa desain 3-bus
#define PIN_MQ135       9     // analog (kualitas udara)

// ===== MQ-135: divider analog sebelum ADC =====
#define MQ135_PIN   9
#define VREF        3.3
#define ADC_RES     4095.0
#define R1          10000.0   // R4
#define R2          15000.0   // R5
#define DIVIDER_RATIO (R2 / (R1 + R2))  // = 0.6
#define MQ135_VCC     5.0      // tegangan modul MQ-135
#define MQ135_RLOAD   10.0     // resistor load modul (kOhm)
#define PIN_ESTOP       8     // estop_sense

#define FW_VERSI  "v0.7.3"
