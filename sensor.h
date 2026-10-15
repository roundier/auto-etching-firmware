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

// Filter state
int  filterBuf[JUMLAH_SENSOR][FILTER_WINDOW];
int  filterIdx[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
long filterSum[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
bool filterInit[JUMLAH_SENSOR] = { false, false, false, false, false };

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    pinMode(sensorPin[i], INPUT);
    sensorRaw[i]       = 0;
    sensorPersen[i]    = 0;
    sensorKalibKosong[i] = 113000;
    sensorKalibPenuh[i]  = 156000;
    filterInit[i]      = false;
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
}

void sensorSetKosong(int i) {
  sensorKalibKosong[i] = sensorRaw[i];
}

void sensorSetPenuh(int i) {
  sensorKalibPenuh[i] = sensorRaw[i];
}

void levelMasuk() {
  levelScroll = 0;
}

// ================= SENSOR SUHU (DS18B20 x3, 1-Wire) =================
// Satu bus 1-Wire di PIN_DS18B20, tiga sensor dibedakan lewat DeviceAddress.
// Blok "Suhu Motor / Komponen / Chamber" masing-masing menunjuk ke salah satu
// address lewat kalibSuhuAddr[] (1..3) -> sensorAddress[alamat-1].
// Offset per blok (x10 °C) dihimpun dari kalibrasi KIRI/KANAN.

OneWire ds18Bus(PIN_DS18B20);
DallasTemperature ds18(&ds18Bus);

DeviceAddress sensorAddress[3] = {
  { 0x28, 0xB8, 0xEF, 0x87, 0x3A, 0x25, 0x02, 0x64 },
  { 0x28, 0x80, 0x77, 0x48, 0x3A, 0x25, 0x02, 0x02 },
  { 0x28, 0x8E, 0xFD, 0x23, 0x39, 0x25, 0x02, 0x87 }
};

// address terpilih per blok (1..3, index 0 = sensorAddress[0])
int kalibSuhuAddr[JUMLAH_SUHU] = { 1, 2, 3 };

// offset kalibrasi per blok (x10 °C)
float suhuOffsetX10[JUMLAH_SUHU] = { 0, 0, 0 };

bool ds18Siap = false;

void sensorSuhuInit() {
  analogReadResolution(12);                // ADC MQ-135 12-bit (0..4095)
  ds18.begin();
  // Resolusi 12-bit per sensor (mengikuti sketch referensi DS18B20_3pcs).
  for (int s = 0; s < 3; s++) ds18.setResolution(sensorAddress[s], 12);
  ds18.setWaitForConversion(false);       // konversi di latar belakang, UI tetap responsif
  int jumlah = ds18.getDeviceCount();
  ds18Siap = (jumlah >= 1);
  dbg.print(F("[SUHU] DS18B20 ditemukan: "));
  dbg.print(jumlah);
  dbg.println(ds18Siap ? F(" sensor") : F(" (tidak ada)"));
  // Cocokkan address hasil scan dengan address yang diharapkan (urutan).
  for (int s = 0; s < 3; s++) {
    bool ketemu = false;
    for (int i = 0; i < jumlah; i++) {
      DeviceAddress a;
      if (!ds18.getAddress(a, i)) continue;
      if (memcmp(a, sensorAddress[s], 8) == 0) { ketemu = true; break; }
    }
    dbg.print(F("[SUHU]  sensor"));
    dbg.print(s + 1);
    dbg.println(ketemu ? F(" OK") : F(" TIDAK DITEMUKAN"));
  }
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
float sensorSuhuRaw(int idx) {
  if (!ds18Siap) return -127.0f;
  int slot = constrain(kalibSuhuAddr[idx], 1, 3) - 1;
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
