#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "data.h"

// ================= SENSOR LEVEL CAIRAN (nickel strip + touch) =================
// Strip nikel di luar jerican HDPE -> kabel AWG30/32 -> GPIO touch ESP32-S3.
// Moving average filter (window=8) untuk stabilkan pembacaan.
// Kalibrasi: 0% (kosong) dan 100% (penuh) -> konversi ke persentase.

#define FILTER_WINDOW 8

const int sensorPin[JUMLAH_SENSOR] = {
  PIN_LEVEL_HCL, PIN_LEVEL_H2O2, PIN_LEVEL_H2O, PIN_LEVEL_SABUN, PIN_LEVEL_LIMBAH
};

const char* namaSensor[JUMLAH_SENSOR] = {
  "HCl", "H2O2", "H2O", "Sabun", "Limbah"
};

bool sensorOk = true;
int  sensorRaw[JUMLAH_SENSOR];          // nilai mentah (moving average)
int  sensorPersen[JUMLAH_SENSOR];       // level dalam persen 0-100
int  sensorKalibKosong[JUMLAH_SENSOR];  // nilai saat 0% (kosong)
int  sensorKalibPenuh[JUMLAH_SENSOR];   // nilai saat 100% (penuh)

// ================= DETEKSI SENSOR MATI (v0.6.5) =================
// `touchRead()` yang sehat selalu mengembalikan nilai non-nol. Kalau sebuah strip
// menunjukkan 0 terus-menerus, hampir pasti kabelnya putus atau strip copot.
//
// CATATAN BATAS DETEKSI: strip yang sudah tidak sensitif (buildup karat, HDPE
// terlalu tebal) masih bergerak sedikit, jadi TIDAK terdeteksi di sini. Yang
// bisa ditangkap hanya kasus "nilai beku total" — cukup untuk mencegah FASE_CEK
// nanti memercayai angka 0% yang palsu.
//
// Ambang `SENSOR_BEKU_MS` didefinisikan di `config.h`.

bool sensorMati[JUMLAH_SENSOR] = { false, false, false, false, false };
bool sensorMatiDilaporkan[JUMLAH_SENSOR] = { false, false, false, false, false };
unsigned long sensorBekuMs[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };

// Filter state
int  filterBuf[JUMLAH_SENSOR][FILTER_WINDOW];
int  filterIdx[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
long filterSum[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
bool filterInit[JUMLAH_SENSOR] = { false, false, false, false, false };

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    pinMode(sensorPin[i], INPUT);
    sensorRaw[i]         = 0;
    sensorPersen[i]      = 0;
    sensorKalibKosong[i] = 113000;
    sensorKalibPenuh[i]  = 156000;
    filterInit[i]        = false;
    sensorMati[i]           = false;
    sensorMatiDilaporkan[i] = false;
    sensorBekuMs[i]         = 0;
  }
  sensorOk = true;
  dbg.println(F("[SENSOR] Nickel strip touch mode, filter MA(8)"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(F("  "));
    dbg.print(namaSensor[i]);
    dbg.print(F(": GPIO"));
    dbg.print(sensorPin[i]);
    dbg.print(F("  0%="));
    dbg.print(sensorKalibKosong[i]);
    dbg.print(F(" 100%="));
    dbg.println(sensorKalibPenuh[i]);
  }
  return true;
}

int bacaFiltered(int idx) {
  int raw = touchRead(sensorPin[idx]);
  if (!filterInit[idx]) {
    for (int i = 0; i < FILTER_WINDOW; i++) filterBuf[idx][i] = raw;
    filterSum[idx] = (long)raw * FILTER_WINDOW;
    filterInit[idx] = true;
    return raw;
  }
  filterSum[idx] -= filterBuf[idx][filterIdx[idx]];
  filterBuf[idx][filterIdx[idx]] = raw;
  filterSum[idx] += raw;
  filterIdx[idx] = (filterIdx[idx] + 1) % FILTER_WINDOW;
  return (int)(filterSum[idx] / FILTER_WINDOW);
}

int kePersen(int nilai, int idx) {
  int range = sensorKalibPenuh[idx] - sensorKalibKosong[idx];
  if (range <= 0) return 0;
  long persen = ((long)(nilai - sensorKalibKosong[idx]) * 100) / range;
  if (persen < 0) persen = 0;
  if (persen > 100) persen = 100;
  return (int)persen;
}

void sensorStatusUpdate();   // definisi di bawah (bagian status ISI/KOSONG)

// Panggil tiap sensorUpdate(). Sensor yang touchRead()-nya beku di 0 selama
// SENSOR_BEKU_MS ditandai mati. Log ke Serial hanya saat status berubah supaya
// tidak membanjiri monitor tiap 50 ms.
void sensorMatiUpdate() {
  unsigned long now = millis();
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (sensorRaw[i] != 0) {
      // Ada nilai: bukan sensor beku. Reset penghitung.
      if (sensorMati[i]) {
        dbg.print(F("[SENSOR] "));
        dbg.print(namaSensor[i]);
        dbg.println(F(" pulih - sensor hidup lagi"));
      }
      sensorMati[i]           = false;
      sensorMatiDilaporkan[i] = false;
      sensorBekuMs[i]         = 0;
      continue;
    }
    // Nilai 0: hitung berapa lama beku.
    if (sensorBekuMs[i] == 0) sensorBekuMs[i] = now;
    if (!sensorMati[i] && (now - sensorBekuMs[i]) >= SENSOR_BEKU_MS) {
      sensorMati[i] = true;
      dbg.print(F("[SENSOR] "));
      dbg.print(namaSensor[i]);
      dbg.println(F(" MATI - raw beku di 0, cek strip nikel & kabel AWG30"));
    }
    if (sensorMati[i] && !sensorMatiDilaporkan[i]) {
      sensorMatiDilaporkan[i] = true;
      dbg.print(F("[SENSOR] "));
      dbg.print(namaSensor[i]);
      dbg.println(F(" tidak bisa dipercaya - abaikan sampai nilai berubah"));
    }
  }
}

void sensorUpdate() {
  if (!sensorOk) return;
  bool berubah = false;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int raw = bacaFiltered(i);
    sensorRaw[i] = raw;
    int p = kePersen(raw, i);
    if (p != sensorPersen[i]) {
      sensorPersen[i] = p;
      berubah = true;
    }
  }
  if (berubah && halaman == HAL_LEVEL) perluGambar = true;
  sensorStatusUpdate();
  sensorMatiUpdate();
}

void sensorSetKosong(int i) {
  sensorKalibKosong[i] = sensorRaw[i];
}

void sensorSetPenuh(int i) {
  sensorKalibPenuh[i] = sensorRaw[i];
}

// ================= STATUS ISI / KOSONG =================
// Dua ambang dari `config.h`: AMBANG_MARGIN = batas bawah dianggap "ADA",
// AMBANG_HYST = lebar histeresis supaya indikator tidak berkedip saat level
// berada tepat di ambang (naik: >= MARGIN, turun: < MARGIN - HYST).
bool sensorAdaCairanState[JUMLAH_SENSOR];

void sensorStatusReset() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) sensorAdaCairanState[i] = false;
}

bool sensorAdaCairan(int i) {
  return sensorAdaCairanState[i];
}

// Panggil tiap sensorUpdate(). state lama dipakai sebagai titik awal supaya
// histeresis benar-benar bekerja (bukan sekadar membandingkan dua angka).
void sensorStatusUpdate() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int p = sensorPersen[i];
    if (!sensorAdaCairanState[i]) {
      if (p >= AMBANG_MARGIN) sensorAdaCairanState[i] = true;
    } else {
      if (p < AMBANG_MARGIN - AMBANG_HYST) sensorAdaCairanState[i] = false;
    }
  }
}

void levelMasuk() {
  levelScroll = 0;
}

// ================= SENSOR SUHU (DS18B20 x3, 1-Wire) =================
// Satu bus 1-Wire di PIN_DS18B20, tiga sensor dibedakan lewat DeviceAddress.
// Blok "Suhu Motor / Komponen / Chamber" masing-masing menunjuk ke salah satu
// address lewat kalibSuhuAddr[] (1..3) -> sensorAddress[alamat-1].
// Offset per blok (x10 °C) dihimpun dari kalibrasi KIRI/KANAN.
//
// v0.6.3: address TIDAK lagi hardcode. `sensorSuhuInit()` memindai bus dan
// mengisi `sensorAddress[]` dengan urutan hasil `getAddress()`, lalu menyimpannya
// ke NVS blob "kal2" supaya urutan yang sama berlaku setelah reboot. Kalau address
// hasil boot pertama (tidak ada di NVS), urutan penentuannya adalah urutan bus —
// cetak address ke Serial lalu cocokkan dengan label fisik Motor/Komponen/Chamber.

OneWire ds18Bus(PIN_DS18B20);
DallasTemperature ds18(&ds18Bus);

// Penanda slot yang tidak terisi: seluruh byte address 0.
const uint8_t DS_ADDR_KOSONG[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

DeviceAddress sensorAddress[3] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 }
};
bool sensorAddressValid[3] = { false, false, false };

// address terpilih per blok (1..3, index 0 = sensorAddress[0])
int kalibSuhuAddr[JUMLAH_SUHU] = { 1, 2, 3 };

// offset kalibrasi per blok (x10 °C)
float suhuOffsetX10[JUMLAH_SUHU] = { 0, 0, 0 };

bool ds18Siap = false;

// Cetak 8 byte address sebagai 16 digit hex ke Serial (untuk dicocokkan dgn
// label fisik). DbgPort tidak punya print(uint8_t, base) jadi diformat dulu.
void sensorPrintAddress(int slot, const uint8_t* a) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X",
           a[0], a[1], a[2], a[3]);
  dbg.print(F("[SUHU] blok "));
  dbg.print(slot);
  dbg.print(F(" addr="));
  dbg.print(buf);
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X",
           a[4], a[5], a[6], a[7]);
  dbg.println(buf);
}

// Salin hasil pindaian bus ke sensorAddress[]. Slot yang tidak terisi
// ditandai address nol + sensorAddressValid = false.
void sensorSimpanAddress(int* jumlahTersimpan) {
  int jumlah = ds18.getDeviceCount();
  int n = 0;
  for (int i = 0; i < jumlah && n < 3; i++) {
    DeviceAddress a;
    if (!ds18.getAddress(a, i)) continue;
    memcpy(sensorAddress[n], a, 8);
    sensorAddressValid[n] = true;
    sensorPrintAddress(n, a);
    n++;
  }
  for (int i = n; i < 3; i++) {   // sisa slot kosong
    memcpy(sensorAddress[i], DS_ADDR_KOSONG, 8);
    sensorAddressValid[i] = false;
  }
  if (jumlahTersimpan) *jumlahTersimpan = n;
  ds18Siap = (n >= 1);
  dbg.print(F("[SUHU] DS18B20 terpakai: "));
  dbg.print(n);
  dbg.print(F("/3"));
  dbg.println(ds18Siap ? F(" slot terisi") : F(" - TIDAK ADA, cek GPIO10"));
}

// Coba pakai address dari NVS. Return true kalau blob ada & addressesnya
// valid, sehingga tidak perlu pindai ulang (pindai bus ~750 ms, dan urutan
// bus bisa berubah kalau sensor dicabut/di-exchange).
bool sensorMuatAddressNVS();   // definisi di storage.h

// Tulis address hasil pindaian ke NVS supaya dipakai pada boot berikutnya.
bool simpanAddressNVS();   // definisi di storage.h

void sensorSuhuInit() {
  analogReadResolution(12);                // ADC MQ-135 12-bit (0..4095)
  ds18.begin();
  ds18.setWaitForConversion(false);       // konversi di latar belakang, UI responsif

  int n = 0;
  bool dariNVS = sensorMuatAddressNVS();
  if (!dariNVS) {
    sensorSimpanAddress(&n);
    simpanAddressNVS();                   // simpan hasil pindaian pertama
  } else {
    for (int i = 0; i < 3; i++) if (sensorAddressValid[i]) n++;
    dbg.print(F("[SUHU] Address dimuat dari NVS, "));
    dbg.print(n);
    dbg.println(F("/3 slot terisi"));
  }

  // Resolusi 12-bit hanya untuk slot yang benar-benar terisi.
  for (int s = 0; s < 3; s++)
    if (sensorAddressValid[s]) ds18.setResolution(sensorAddress[s], 12);
}

// ===== Konversi non-blocking =====
// requestTemperatures() secara default memblokir ~750 ms; selama itu tombol dan
// render UI berhenti, sehingga halaman terasa macet. Dengan
// setWaitForConversion(false), fungsi itu hanya menyalakan bit convert lalu
// langsung kembali: konversi berjalan di latar belakang dan dicek lewat
// isConversionComplete().

void sensorSuhuRequest() {
  if (!ds18Siap) return;
  ds18.requestTemperatures();          // tidak memblokir
}

unsigned long sensorSuhuMs = 0;
const unsigned long INTERVAL_SUHU_MS = 1000;
bool suhuDataBaru = false;

// Panggil tiap loop(): mulai konversi baru lalu tandai data baru saat selesai.
void sensorSuhuUpdate() {
  if (!ds18Siap) return;
  if (!ds18.isConversionComplete()) return;   // konversi lama masih jalan
  unsigned long now = millis();
  if (now - sensorSuhuMs < INTERVAL_SUHU_MS) return;
  sensorSuhuMs = now;
  sensorSuhuRequest();
  suhuDataBaru = true;
}

// true tepat setelah konversi selesai; pemicu redraw UI.
bool sensorSuhuSiapTampil() {
  if (!suhuDataBaru) return false;
  suhuDataBaru = false;
  return true;
}

// Baca suhu (°C) untuk blok idx dari cache terakhir.
// Slot kosong / address tidak pernah dipindai -> DEVICE_DISCONNECTED_C (-127),
// supaya ui.h menampilkan "--.-C".
float sensorSuhuRaw(int idx) {
  if (!ds18Siap) return -127.0f;
  int slot = constrain(kalibSuhuAddr[idx], 1, 3) - 1;
  if (!sensorAddressValid[slot]) return -127.0f;
  return ds18.getTempC(sensorAddress[slot]);
}

// Suhu ternormalisasi = bacaan + offset kalibrasi blok tsb.
float sensorSuhu(int idx) {
  float raw = sensorSuhuRaw(idx);
  if (raw <= -100.0f) return raw;   // error / belum ada sensor
  return raw + suhuOffsetX10[idx] / 10.0f;
}

bool sensorSuhuValid(int idx) { return sensorSuhuRaw(idx) > -100.0f; }

// ================= SENSOR UDARA (MQ-135) =================
// ADC -> tegangan sensor (dibagi divider ratio) ->ppm MQ-135.
int sensorUdaraRaw() {
  return analogRead(MQ135_PIN);
}

// Tegangan pada sensor MQ-135 sebelum divider (V).
float mq135Tegangan() {
  int raw = analogRead(MQ135_PIN);
  float vAdc = (raw / ADC_RES) * VREF;          // tegangan di pin ADC
  return vAdc / DIVIDER_RATIO;                  // naikkan kembali sebelum divider
}

// Konversi tegangan -> ppm mengikuti kurva MQ-135 (datasheet).
float mq135PPM() {
  float vc = mq135Tegangan();
  if (vc <= 0.05f) return 0.0f;                  // avoid divide by ~0
  float rs = ((MQ135_VCC / vc) - 1.0f) * MQ135_RLOAD;
  if (rs <= 0.0f) return 0.0f;
  float ratio = rs / MQ135_RLOAD;
  return 25.7f * pow(ratio, -1.81f) * pow(ratio, 0.13f);
}
