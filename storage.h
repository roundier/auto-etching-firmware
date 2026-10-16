#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "sensor_extra.h"   // dsOffsetX10[], mq135Baseline, mq135Margin

// Semua preset disimpan sebagai SATU blob (satu key) + versi + checksum.
#define FORMAT_VERSI 1

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
  Preset   p[MAKS_PRESET];
  uint32_t crc;
};

Preferences prefs;

uint32_t hitungCrc(const uint8_t* d, size_t n) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619UL; }
  return h;
}

bool simpanPreset() {
  DataSimpan d;
  memset(&d, 0, sizeof(d));
  d.versi  = FORMAT_VERSI;
  d.jumlah = jumlahPreset;
  for (int i = 0; i < jumlahPreset; i++) d.p[i] = daftarPreset[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("cfg", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.print(F("[STORE] Simpan "));
  dbg.print(jumlahPreset);
  dbg.println(ok ? F(" preset: OK") : F(" preset: GAGAL"));
  return ok;
}

void muatPreset() {
  DataSimpan d;
  bool ada = false, valid = false;

  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength("cfg");
    ada = (len > 0);
    if (len == sizeof(d)) {
      prefs.getBytes("cfg", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
              (d.jumlah <= MAKS_PRESET);
      for (uint32_t i = 0; valid && i < d.jumlah; i++)
        if (!validasiPreset(d.p[i])) valid = false;
    }
    prefs.end();
  }

  if (valid) {
    jumlahPreset = d.jumlah;
    for (int i = 0; i < jumlahPreset; i++) daftarPreset[i] = d.p[i];
    dbg.print(F("[STORE] Dimuat "));
    dbg.print(jumlahPreset);
    dbg.println(F(" preset"));
  } else {
    resetKeDefault();
    dbg.println(ada ? F("[STORE] Data korup/versi beda -> default")
                       : F("[STORE] Data belum ada -> default"));
    simpanPreset();
  }
}

// ================= SNAPSHOT / ROLLBACK (v0.7.1) =================
// Slot cadangan berisi daftar preset SEBELUM import Serial terakhir. Satu slot
// saja (bukan arsip) karena tujuannya cuma "batalkan import yang barusan salah",
// bukan menyimpan histori. Format & checksum identik dengan key "cfg", jadi
// bisa dipakai ulang struktur `DataSimpan` di atas.
#define NVS_KEY_PREV "prev"

bool simpanSnapshot() {
  DataSimpan d;
  memset(&d, 0, sizeof(d));
  d.versi  = FORMAT_VERSI;
  d.jumlah = jumlahPreset;
  for (int i = 0; i < jumlahPreset; i++) d.p[i] = daftarPreset[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes(NVS_KEY_PREV, &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.print(F("[STORE] Snapshot "));
  dbg.print(jumlahPreset);
  dbg.println(ok ? F(" preset disimpan") : F(" preset GAGAL disimpan"));
  return ok;
}

// Muat snapshot ke slot "prev". Return true kalau slot ada & valid.
// TIDAK menyentuh daftarPreset — pemanggil yang memutuskan mau applying atau tidak.
bool muatSnapshot(Preset* keluar, int* jumlahKeluar) {
  DataSimpan d;
  bool valid = false;

  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength(NVS_KEY_PREV);
    if (len == sizeof(d)) {
      prefs.getBytes(NVS_KEY_PREV, &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
              (d.jumlah >= 1) && (d.jumlah <= MAKS_PRESET);
      for (uint32_t i = 0; valid && i < d.jumlah; i++)
        if (!validasiPreset(d.p[i])) valid = false;
      if (valid) {
        *jumlahKeluar = d.jumlah;
        for (uint32_t i = 0; i < d.jumlah; i++) keluar[i] = d.p[i];
      }
    }
    prefs.end();
  }
  return valid;
}

// ================= KALIBRASI SENSOR LEVEL =================
#define FORMAT_VERSI_KALIB 1

struct DataKalib {
  uint32_t versi;
  int32_t  kosong[JUMLAH_SENSOR];
  int32_t  penuh[JUMLAH_SENSOR];
  uint32_t crc;
};

bool simpanKalib() {
  DataKalib d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    d.kosong[i] = sensorKalibKosong[i];
    d.penuh[i]  = sensorKalibPenuh[i];
  }
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi: OK") : F("[STORE] Simpan kalibrasi: GAGAL"));
  return ok;
}

void muatKalib() {
  DataKalib d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal") == sizeof(d)) {
      prefs.getBytes("kal", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SENSOR; i++)
        if (d.kosong[i] <= 0 || d.penuh[i] <= 0 || d.penuh[i] <= d.kosong[i]) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    sensorKalibKosong[i] = valid ? d.kosong[i] : 113000;
    sensorKalibPenuh[i]  = valid ? d.penuh[i]  : 156000;
  }
  dbg.println(valid ? F("[STORE] Kalibrasi dimuat") : F("[STORE] Kalibrasi belum ada (default)"));
}

// ================= KALIBRASI SENSOR SUHU & UDARA =================
// Blob TERPISAH dari kalibrasi level ("kal"): kalau struct ini ikut berubah,
// kalibrasi 0%/100% yang sudah disimpan di "kal" tidak jadi hilang.
// v1 = dsOffsetX10[] + mq135Baseline/Margin (design lama, 3-bus)
// v2 = addrSensor[] + suhuOffsetX10[] + mq135Normal/mq135Kotor (design 3-sensor-1-bus)
#define FORMAT_VERSI_KALIB2 2

struct DataKalib2 {
  uint32_t versi;
  int16_t  addrSensor[JUMLAH_SUHU];    // DeviceAddress 1..3 per blok
  int16_t  suhuOffsetX10[JUMLAH_SUHU]; // x10 degC per blok (untuk-aligned dengan float runtime)
  int16_t  mq135Normal;                // acuan udara bersih (ppm)
  int16_t  mq135Kotor;                 // acuan udara kotor (ppm)
  uint32_t crc;                        // harus field terakhir
};

bool simpanKalib2() {
  DataKalib2 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB2;
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    d.addrSensor[i]    = (int16_t)kalibSuhuAddr[i];
    d.suhuOffsetX10[i] = (int16_t)suhuOffsetX10[i];
  }
  d.mq135Normal = constrain(mq135Normal, 0, 32767);
  d.mq135Kotor  = constrain(mq135Kotor,  0, 32767);
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal2", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi suhu/udara: OK")
                 : F("[STORE] Simpan kalibrasi suhu/udara: GAGAL"));
  return ok;
}

void muatKalib2() {
  DataKalib2 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal2") == sizeof(d)) {
      prefs.getBytes("kal2", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB2) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SUHU; i++) {
        if (d.addrSensor[i]    < 1 || d.addrSensor[i]    > 3) valid = false;
        if (d.suhuOffsetX10[i] < -200 || d.suhuOffsetX10[i] > 200) valid = false;
      }
      if (d.mq135Normal < 0 || d.mq135Normal > 32767) valid = false;
      if (d.mq135Kotor  < 0 || d.mq135Kotor  > 32767) valid = false;
    }
    prefs.end();
  }
  if (valid) {
    for (int i = 0; i < JUMLAH_SUHU; i++) {
      kalibSuhuAddr[i]    = d.addrSensor[i];
      suhuOffsetX10[i]    = (float)d.suhuOffsetX10[i];
    }
    mq135Normal = d.mq135Normal;
    mq135Kotor  = d.mq135Kotor;
  }
  dbg.println(valid ? F("[STORE] Kalibrasi suhu/udara dimuat")
                    : F("[STORE] Kalibrasi suhu/udara belum ada (default)"));
}

// ================= KALIBRASI STEPPER (ml per rotasi) =================
// Blob TERPISAH: "kal3" - simpan kalibStepperMl[3] agar langkahPerMl otomatis terhitung
#define FORMAT_VERSI_KALIB3 1

struct DataKalib3 {
  uint32_t versi;
  float    stepperMl[3];   // ml per rotasi untuk HCl, H2O2, Air
  uint32_t crc;
};

bool simpanKalibStepper() {
  DataKalib3 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB3;
  for (int i = 0; i < 3; i++) d.stepperMl[i] = kalibStepperMl[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal3", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi stepper: OK")
                 : F("[STORE] Simpan kalibrasi stepper: GAGAL"));
  return ok;
}

void muatKalibStepper() {
  DataKalib3 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal3") == sizeof(d)) {
      prefs.getBytes("kal3", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB3) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < 3; i++)
        if (d.stepperMl[i] < 0.1 || d.stepperMl[i] > 10.0) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < 3; i++) {
    kalibStepperMl[i] = valid ? d.stepperMl[i] : 1.0;  // default 1.0 ml/rotasi
  }
  dbg.println(valid ? F("[STORE] Kalibrasi stepper dimuat")
                    : F("[STORE] Kalibrasi stepper belum ada (default)"));
}

// ================= ADDRESS DS18B20 (v0.6.3) =================
// Blob TERPISAH: "kal5". Alasan pisah dari "kal2": `DataKalib2` disimpan lewat
// Preferences sebagai struct dengan CRC; address hasil pindaian bus TIDAK boleh
// ikut hilang kalau kalibrasi suhu ditulis ulang, dan sebaliknya. Memisahkan
// juga menjaga `FORMAT_VERSI_KALIB2` tetap 2 sehingga blob "kal2" versi lama
// yang sudah tersimpan di alat tidak ditolak.
#define FORMAT_VERSI_KALIB5 1

struct DataKalib5 {
  uint32_t versi;
  uint8_t  addr[3][8];   // DeviceAddress tiap slot, 0 = slot kosong
  uint8_t  valid;         // bitmask: bit s = 1 kalau slot s terisi
  uint32_t crc;
};

bool simpanAddressNVS() {
  DataKalib5 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB5;
  for (int s = 0; s < 3; s++) {
    memcpy(d.addr[s], sensorAddress[s], 8);
    if (sensorAddressValid[s]) d.valid |= (1 << s);
  }
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal5", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan address DS18B20: OK")
                 : F("[STORE] Simpan address DS18B20: GAGAL"));
  return ok;
}

// Return true kalau blob ada, CRC cocok, dan addressesnya terlihat plausibel
// (family code DS18B20 = 0x28). Kalau tidak, caller memindai ulang bus.
bool sensorMuatAddressNVS() {
  DataKalib5 d;
  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal5") == sizeof(d)) {
      prefs.getBytes("kal5", &d, sizeof(d));
      ok = (d.versi == FORMAT_VERSI_KALIB5) &&
           (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
    }
    prefs.end();
  }
  if (!ok) { dbg.println(F("[STORE] Address DS18B20 belum ada -> pindai bus")); return false; }

  bool ada = false;
  for (int s = 0; s < 3; s++) {
    bool v = (d.valid & (1 << s)) != 0;
    // Family code 0x28 wajib: menolak blob yang isinya acak / versi lama.
    if (v && d.addr[s][0] != 0x28) { ok = false; break; }
    memcpy(sensorAddress[s], d.addr[s], 8);
    sensorAddressValid[s] = v;
    if (v) ada = true;
  }
  if (!ok || !ada) {
    for (int s = 0; s < 3; s++) {
      memcpy(sensorAddress[s], DS_ADDR_KOSONG, 8);
      sensorAddressValid[s] = false;
    }
    return false;
  }
  return true;
}

// ================= KALIBRASI DC: DUTY PWM + ML HASIL TEST =================
// Blob TERPISAH: "kal4" — nilai yang diedit di halaman Kalibrasi Motor DC
// (pompa Sabun/Limbah) sekaligus Kalibrasi Agitasi & Fan. Sebelumnya semua
// nilai ini volatile: hilang tiap reboot, sehingga duty PWM motor agitasi,
// pompa abnormally, dan exhaust fan selalu kembali ke default 128 / 200.
//
// Layout array `kalibDuty[7]` (lihat `data.h`):
//   [0] pompa Sabun   [1] pompa Limbah
//   [2..4] agitasi speed 1..3   [5] cadangan   [6] exhaust fan
// `kalibMl[2]` = ml hasil uji pompa Sabun / Limbah (float di RAM, x10 di NVS).
//
// Blob terpisah dari "kal"/"kal2"/"kal3" supaya perubahan struct di sini tidak
// membuat kalibrasi lain hilang.
#define FORMAT_VERSI_KALIB4 1
#define JUMLAH_KALIB_DUTY   7    // harus sama dengan ukuran kalibDuty[] di data.h
#define JUMLAH_KALIB_ML     2    // harus sama dengan ukuran kalibMl[] di data.h

struct DataKalib4 {
  uint32_t versi;
  int16_t  duty[JUMLAH_KALIB_DUTY];   // PWM 0..255
  int16_t  mlX10[JUMLAH_KALIB_ML];     // ml x10 (1 desimal), 0 = belum diuji
  uint32_t crc;                        // harus field terakhir
};

// Default kalau blob belum ada / rusak. 128 = ~50% duty, nilai tengah yang
// selalu membuat motor berputar tanpa kepanasan.
const int16_t KALIB4_DEFAULT_DUTY = 128;

bool simpanKalib4() {
  DataKalib4 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB4;
  for (int i = 0; i < JUMLAH_KALIB_DUTY; i++)
    d.duty[i] = (int16_t)constrain(kalibDuty[i], 0, 255);
  for (int i = 0; i < JUMLAH_KALIB_ML; i++)
    d.mlX10[i] = (int16_t)constrain((long)lroundf(kalibMl[i] * 10.0f), 0, 32767);
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal4", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi DC: OK")
                 : F("[STORE] Simpan kalibrasi DC: GAGAL"));
  return ok;
}

void muatKalib4() {
  DataKalib4 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal4") == sizeof(d)) {
      prefs.getBytes("kal4", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB4) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_KALIB_DUTY; i++)
        if (d.duty[i] < 0 || d.duty[i] > 255) valid = false;
      for (int i = 0; valid && i < JUMLAH_KALIB_ML; i++)
        if (d.mlX10[i] < 0) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < JUMLAH_KALIB_DUTY; i++)
    kalibDuty[i] = valid ? d.duty[i] : KALIB4_DEFAULT_DUTY;
  for (int i = 0; i < JUMLAH_KALIB_ML; i++)
    kalibMl[i] = valid ? (d.mlX10[i] / 10.0f) : 0.0f;
  dbg.println(valid ? F("[STORE] Kalibrasi DC dimuat")
                    : F("[STORE] Kalibrasi DC belum ada (default)"));
}
