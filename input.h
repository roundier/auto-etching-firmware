#pragma once
#include <Wire.h>
#include "config.h"
#include "dbg.h"

// Diisi di inputInit() mengikuti define P_xxx, jadi aman kalau pin ditukar
const char* namaTombol[JUMLAH_TOMBOL];

bool stateStabil[JUMLAH_TOMBOL];
bool stateTerbaca[JUMLAH_TOMBOL];
unsigned long waktuBerubah[JUMLAH_TOMBOL];
int tombolAktif = TIDAK_ADA;

// ================= RECOVERY BUS I2C (v0.7.0) =================
// Kalau PCF8574 (expander tombol) hiccup — kabel longgar, konektor korosi, atau
// slave menahan SDA — `Wire.requestFrom()` akan mengembalikan 0 byte setiap
// kali. Akibatnya semua tombol mati dan alat tidak bisa dikontrol sama sekali.
//
// Pemulihan standar: `Wire.end()` (tutup bus, lepaskan pin dari driver),
// jeda sesaat supaya slave melepaskan SDA, lalu `Wire.begin()` lagi. Kalau
// slave sedang menahan SDA, jeda ini memungkinkannya melepas.
//
// Debounce-nya: harus kegagalan BERUNTUN, bukan per kejadian, supaya noise
// sesaat tidak memicu reset bus.
#define I2C_GAGAL_MAX       5      // kegagalan beruntun sebelum recovery
#define I2C_RECOVERY_TUNDA_MS 10   // jeda saat re-init bus (ms)

uint8_t i2cGagalBerturut = 0;
bool    i2cRecovered    = false;   // true kalau recovery terakhir BERHASIL
uint8_t i2cJumlahRecovery = 0;     // total recovery sepanjang boot

bool bacaPCF8574(uint8_t &data) {
  Wire.requestFrom(PCF8574_ADDR, 1);
  if (Wire.available()) {
    data = Wire.read();
    i2cGagalBerturut = 0;    // sukses → reset penghitung
    return true;
  }
  if (i2cGagalBerturut < 255) i2cGagalBerturut++;
  return false;
}

bool tulisPCF8574(uint8_t data) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

// Reset bus I2C + daftar ulang PCF8574. Return true kalau slave terdeteksi lagi.
bool i2cRecovery() {
  dbg.println(F("[I2C] Recovery: reset bus"));
  Wire.end();
  delay(I2C_RECOVERY_TUNDA_MS);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);
  delay(I2C_RECOVERY_TUNDA_MS);

  uint8_t data = 0xFF;
  i2cRecovered = bacaPCF8574(data);
  if (i2cRecovered) {
    i2cGagalBerturut = 0;
    i2cJumlahRecovery++;
    dbg.println(F("[I2C] Bus pulih, PCF8574 terdeteksi"));
    return true;
  }
  // Jangan reset penghitung — supaya recovery dicoba lagi di iterasi berikutnya
  // alih-alih menunggu 5 kegagalan baru.
  dbg.println(F("[I2C] GAGAL pulih - cek kabel SDA/SCL & power PCF8574"));
  return false;
}

// Panggil tiap iterasi loop(). Cheap: hanya cek satu counter saat tombol dibaca.
void i2cUpkeep() {
  if (i2cGagalBerturut >= I2C_GAGAL_MAX) {
    i2cRecovery();
  }
}

// Inisialisasi state tombol dari kondisi pin saat ini
void inputInit() {
  namaTombol[P_ATAS]  = "ATAS";
  namaTombol[P_BAWAH] = "BAWAH";
  namaTombol[P_KIRI]  = "KIRI";
  namaTombol[P_KANAN] = "KANAN";
  namaTombol[P_ENTER] = "ENTER";
  namaTombol[P_BACK]  = "BACK";
  uint8_t data = 0xFF;
  bacaPCF8574(data);
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    bool tertekan = !bitRead(data, i);
    stateStabil[i]  = tertekan;
    stateTerbaca[i] = tertekan;
    waktuBerubah[i] = millis();
  }
}

// Baca tombol + debounce + blocking (tombol pertama mengunci).
// Return index tombol yang baru saja valid ditekan, atau -1.
int bacaTombolBaru() {
  uint8_t data;
  if (!bacaPCF8574(data)) return TIDAK_ADA;

  int hasil = TIDAK_ADA;
  unsigned long sekarang = millis();

  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    bool mentah = !bitRead(data, i);   // active LOW

    if (mentah != stateTerbaca[i]) {
      waktuBerubah[i] = sekarang;
      stateTerbaca[i] = mentah;
    }

    if ((sekarang - waktuBerubah[i]) > DEBOUNCE_MS && mentah != stateStabil[i]) {
      stateStabil[i] = mentah;
      if (stateStabil[i]) {
        if (tombolAktif == TIDAK_ADA) { tombolAktif = i; hasil = i; }
      } else {
        if (tombolAktif == i) tombolAktif = TIDAK_ADA;
      }
    }
  }
  return hasil;
}
