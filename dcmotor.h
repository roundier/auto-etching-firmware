#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"   // kalibDuty[] — hasil kalibrasi PWM per motor

// ================= DRIVER TB6612FNG x2 (1 arah) =================
// INA->3.3V, INB->GND, jadi arah tetap; kecepatan via PWM pada PWx.
// Driver 1: PWA=agitasi (15), PWB=pompa sabun (16)
// Driver 2: PWA=pompa limbah (17), PWB=exhaust fan (18)
//
// v0.6.6: PWM tidak lagi konstanta. Semua motor memakai duty hasil kalibrasi
// (`kalibDuty[]`, disimpan di NVS blob "kal4"). Sebelumnya `kalibDuty[]` hanya
// dibaca untuk tampilan — nilai yang dikalibrasi lewat menu tidak berefek sama
// sekali ke motor.
//
// Layout `kalibDuty[7]` (lihat `data.h`):
//   [0] pompa Sabun   [1] pompa Limbah   [2..4] agitasi speed 1..3
//   [5] tidak dipakai  [6] exhaust fan
//
// Default semua slot 128 (~50% duty) sampai kalibrasi diisi — motor tetap
// berputar normal, tidak diam total.

// ================= SHADOW STATE AKTUATOR (v0.7.2) =================
// Semua motor DC ditulis langsung ke register PWM lewat `analogWrite()`, jadi
// tidak ada cara membaca kembali "apakah agitasi sedang jalan" tanpa Shadow
// state. Variabel di bawah dicatat di setiap setter — satu-satunya cara yang
// sinkron dengan apa yang benar-benar ditulis ke pin.
int  agitasiDutyAktif = 0;   // PWM terakhir di PIN_PWA_D1 (0 = mati)
bool pompaSabunOn      = false;
bool pompaLimbahOn     = false;
bool fanOn             = false;

// ---- State ramp-up agitasi (v0.7.4, file A) ----
// WAJIB dideklarasikan di sini, SEBELUM setter di bawah memakainya: C++
// mensyaratkan deklarasi mendahului pemakaian, dan `agitasiSet()`,
// `agitasiSetDuty()`, serta `dcmotorStopAll()` semuanya menyentuh variabel ini.
// Fungsi-fungsinya ada di bagian bawah file ("RAMP-UP AGITASI").
#ifndef RAMP_AGITASI_MS
#define RAMP_AGITASI_MS 1000UL
#endif
int           agitasiDutyTarget   = 0;   // duty tujuan (0..255)
int           agitasiDutysekarang = 0;   // duty nyata terakhir
unsigned long agitasiRampMsSisa   = 0;

void dcmotorInit() {
  pinMode(PIN_PWA_D1, OUTPUT);
  pinMode(PIN_PWB_D1, OUTPUT);
  pinMode(PIN_PWA_D2, OUTPUT);
  pinMode(PIN_PWB_D2, OUTPUT);
  analogWrite(PIN_PWA_D1, 0);
  analogWrite(PIN_PWB_D1, 0);
  analogWrite(PIN_PWA_D2, 0);
  analogWrite(PIN_PWB_D2, 0);
  agitasiDutyAktif = 0;
  pompaSabunOn  = false;
  pompaLimbahOn = false;
  fanOn         = false;
}

// Ambil duty terkalibrasi sebuah motor (indeks ke kalibDuty[]).
static inline int dutyKalib(int idx) {
  return constrain(kalibDuty[idx], 0, 255);
}

// Speed agitasi 1-5 -> PWM terkalibrasi. Hanya 3 titik kalibrasi (speed 1..3)
// sementara preset punya 5 tingkat, jadi di-interpolasi linier:
//   speed 1 -> slot 2,  speed 2 -> antara 2-3,  speed 3 -> slot 3,
//   speed 4 -> antara 3-4,  speed 5 -> slot 4.
// Dengan begitu semua hasil kalibrasi terpakai dan tidak ada tingkat yang loncat.
int agitasiPwm(int speed) {
  speed = constrain(speed, 1, 5);
  // posisi 0..2 dalamsteps 0.5
  int posX2 = (speed - 1) * 2;          // 0,2,4,6,8
  int lo = 2 + (posX2 >> 1);            // 2,3,3,4,4
  int hi = (lo < 4) ? (lo + 1) : 4;
  int sisa = posX2 & 1;                 // 0 = tepat di titik, 1 = di tengah
  if (sisa == 0 || lo == hi) return dutyKalib(lo);
  return (dutyKalib(lo) * (2 - sisa) + dutyKalib(hi) * sisa) / 2;
}

void agitasiSet(int speed) {
  int pwm = (speed <= 0) ? 0 : agitasiPwm(speed);
  analogWrite(PIN_PWA_D1, pwm);
  agitasiDutyAktif = pwm;
  agitasiDutysekarang = pwm;      // setelan instan ikut mereset state ramp
  agitasiDutyTarget   = pwm;
}

// Duty langsung 0..255 (dipakai halaman Kalibrasi Agitasi + test manual)
void agitasiSetDuty(int duty) {
  int pwm = constrain(duty, 0, 255);
  analogWrite(PIN_PWA_D1, pwm);
  agitasiDutyAktif = pwm;
  agitasiDutysekarang = pwm;
  agitasiDutyTarget   = pwm;
}

void pompaSabunSet(bool on) {
  analogWrite(PIN_PWB_D1, on ? dutyKalib(0) : 0);
  pompaSabunOn = on;
}

void pompaLimbahSet(bool on) {
  analogWrite(PIN_PWA_D2, on ? dutyKalib(1) : 0);
  pompaLimbahOn = on;
}

void fanSet(bool on) {
  analogWrite(PIN_PWB_D2, on ? dutyKalib(6) : 0);
  fanOn = on;
}

// PWM langsung 0..255 (dipakai halaman Kalibrasi Exhaust Fan)
void fanSetAt(int pwm) {
  int v = constrain(pwm, 0, 255);
  analogWrite(PIN_PWB_D2, v);
  fanOn = (v > 0);
}

void dcmotorStopAll() {
  analogWrite(PIN_PWA_D1, 0);
  analogWrite(PIN_PWB_D1, 0);
  analogWrite(PIN_PWA_D2, 0);
  analogWrite(PIN_PWB_D2, 0);
  agitasiDutyAktif = 0;
  pompaSabunOn  = false;
  pompaLimbahOn = false;
  fanOn         = false;
  agitasiDutyTarget  = 0;
  agitasiRampMsSisa  = 0;
  agitasiDutysekarang = 0;
}

// ================= RAMP-UP AGITASI (file A) =================
// Motor DC yang dinyalakan mendadak dari duty 0 ke penuh undergo "shock":
// arus awal besar, agitasi bisa tersendat, MOSFET cepat panas. Karena itu
// proses memanggil `agitasiSetRamp()` (bukan `agitasiSet()`) sehingga duty
// naik bertahap.
//
// `agitasiUpdate()` sengaja dipanggil dari `prosesUpdate()` dan BUKAN dari
// loop() .ino — supaya file milik B tidak perlu disentuh sama sekali.
//
// Catatan: variabel `agitasiDutyTarget`, `agitasiDutysekarang`,
// `agitasiRampMsSisa`, dan `RAMP_AGITASI_MS` TIDAK dideklarasikan di sini tapi
// di blok "Shadow state aktuator" paling atas file — supaya setter di atas
// yang memakainya compiles.

void agitasiSetRamp(int speed) {
  agitasiDutyTarget = (speed <= 0) ? 0 : agitasiPwm(speed);
  agitasiRampMsSisa = RAMP_AGITASI_MS;
}

void agitasiRampBerhenti() {
  agitasiDutyTarget   = 0;
  agitasiRampMsSisa   = 0;
  agitasiDutysekarang = 0;
}

bool agitasiRampSelesai() { return agitasiDutysekarang == agitasiDutyTarget; }

// Dipanggil tiap loop dari prosesUpdate(). Duty bergerak ~8 langkah per ms
// menuju target sehingga total naik ≈ RAMP_AGITASI_MS.
void agitasiUpdate() {
  if (agitasiDutysekarang == agitasiDutyTarget) return;
  if (agitasiDutyTarget > agitasiDutysekarang) {
    int v = agitasiDutysekarang + max(1, agitasiDutyTarget / 64);
    agitasiDutysekarang = (v > agitasiDutyTarget) ? agitasiDutyTarget : v;
  } else {
    int v = agitasiDutysekarang - max(1, agitasiDutyTarget / 32);
    agitasiDutysekarang = (v < agitasiDutyTarget) ? agitasiDutyTarget : v;
  }
  analogWrite(PIN_PWA_D1, agitasiDutysekarang);
  agitasiDutyAktif = agitasiDutysekarang;
}

bool agitasiNyala() { return agitasiDutysekarang > 0; }
