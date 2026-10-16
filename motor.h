#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"  // untuk kalibStepperMl[3]

// ================= DRIVER STEPPER TMC2209 x3 (pompa peristaltik) =================
// Non-blocking: update di loop().
//
// Perubahan dari DRV8825 (v0.6.0):
//   - DRV8825 pakai pin SLP (tidur saat LOW). TMC2209 pakai EN (aktif saat LOW)
//     + PDN (power-down/reset, aktif saat LOW). Perhatikan-polarity-nya dibalik:
//     `digitalWrite(PIN_TMC_EN, LOW)` = driver AKTIF.
//   - EN & PDN di-OR-kan ke 3 driver sekaligus (1 pasang pin).
//   - Mikrostep 1/16 -> 3200 step/rev (`STEP_PER_REV`), bukan 1/36 -> 7200.
//     Angka ini wajib sesuai jumper MS1/MS2 pada driver.
//
// langkahPerMl: jumlah langkah per ml, dihitung dari kalibStepperMl (ml per
// rotasi) sebagai  STEP_PER_REV / ml_per_rev.

const int PIN_STEP[3] = { PIN_STEP1, PIN_STEP2, PIN_STEP3 };
const int PIN_DIR [3] = { PIN_DIR1,  PIN_DIR2,  PIN_DIR3  };

long langkahPerMl[3] = { 1200, 1200, 1200 };  // default, akan diupdate di motorInit()
long  targetLangkah[3] = { 0, 0, 0 };
long  jalanLangkah[3]  = { 0, 0, 0 };
bool  pompaJalan[3]    = { false, false, false };
unsigned long langkahTerakhirUs[3] = { 0, 0, 0 };
// Jeda antar pulsa. 1200 us (~830 Hz) dipilih supaya 3 pompa paralel tidak
// step-loss; DRV8825 lama pakai 400 us dan berisiko kehilangan langkah.
const unsigned int LANGKAH_INTERVAL_US = 1200;

// Aktifkan / matikan driver.
//   sleep = true  -> EN = HIGH (TMC2209 mati, motor bebas, driver's CurrentScale idle)
//   sleep = false -> EN = LOW  (driver aktif, bisa menerima pulsa STEP)
void stepperSleep(bool sleep) {
#if TMC_EN_AKTIF_LOW
  digitalWrite(PIN_TMC_EN, sleep ? HIGH : LOW);
#else
  digitalWrite(PIN_TMC_EN, sleep ? LOW : HIGH);
#endif
}

// Konversi STEP_PER_REV -> step/ml dari kalibrasi (ml per rotasi)
void hitungLangkahPerMl() {
  for (int i = 0; i < 3; i++) {
    float mlPerRev = kalibStepperMl[i];
    if (mlPerRev < 0.1) mlPerRev = 0.1;      // guard div-by-zero
    if (mlPerRev > 10.0) mlPerRev = 10.0;    // batas wajar
    langkahPerMl[i] = (long)((float)STEP_PER_REV / mlPerRev);
    dbg.print(F("[MOTOR] Pompa "));
    dbg.print(i);
    dbg.print(F(": "));
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", mlPerRev);
    dbg.print(buf);
    dbg.print(F(" ml/rev -> "));
    dbg.print(langkahPerMl[i]);
    dbg.println(F(" step/ml"));
  }
}

void motorInit() {
  for (int i = 0; i < 3; i++) {
    pinMode(PIN_STEP[i], OUTPUT);
    pinMode(PIN_DIR[i], OUTPUT);
    digitalWrite(PIN_DIR[i], HIGH);   // arah maju (sedot)
    digitalWrite(PIN_STEP[i], LOW);
  }
  // EN & PDN -> OUTPUT. Mulai dengan driver nonaktif.
  pinMode(PIN_TMC_EN,  OUTPUT);
  pinMode(PIN_TMC_PDN, OUTPUT);
  stepperSleep(true);                 // EN = HIGH -> mati dulu

  // Pulse reset via PDN: LOW (reset) -> delay -> HIGH (lepas reset).
  digitalWrite(PIN_TMC_PDN, LOW);
  delayMicroseconds(TMC_PDN_RESET_US);
  digitalWrite(PIN_TMC_PDN, HIGH);
  delayMicroseconds(TMC_PDN_RESET_US);

  stepperSleep(false);                // EN = LOW -> aktif, siap menerima STEP
  hitungLangkahPerMl();               // hitung langkahPerMl dari kalibrasi NVS
  dbg.print(F("[MOTOR] TMC2209 x3 aktif, STEP_PER_REV="));
  dbg.print(STEP_PER_REV);
  dbg.println(F(" (EN/PDN shared)"));
}

// Mulai memompa idx: 0=HCl, 1=H2O2, 2=H2O. ml dalam ml.
// Definisi sebenarnya ada di bawah, setelah akumulator ml.

bool pompaSelesai(int idx) { return !pompaJalan[idx]; }

// ================= AKUMULATOR ML TERPUMPA (v0.7.2) =================
// Total ml yang benar-benar keluar per bahan selama satu proses.
//
// Kenapa tidak baca `jalanLangkah[]` langsung: `pompaMulai()` me-reset
// `jalanLangkah[idx] = 0`, dan `aksiFase(FASE_RINSING)` memanggil
// `pompaMulai(2, 100)` untuk bilas. Jadi penogonalan ml H2O dari fase DOSING
// akan terhapus begitu fase RINSE dimulai. Nilai `jalanLangkah[]` hanya valid
// sampai pompa itu dijalankan lagi — makanya rekaman harus terjadi di moment
// pompa berhenti, bukan dibaca belakangan.
float totalMlTerpompa[3] = { 0, 0, 0 };

// Rekam ml yang sudah keluar dari pompa `idx`. Aman dipanggil berkali-kali:
// hanya berefek kalau pompa sedang jalan.
void pompaRekamHasil(int idx) {
  if (idx < 0 || idx > 2) return;
  if (!pompaJalan[idx]) return;
  if (langkahPerMl[idx] <= 0) return;
  totalMlTerpompa[idx] += (float)jalanLangkah[idx] / (float)langkahPerMl[idx];
  pompaJalan[idx] = false;            // rekaman sekaligus berarti pompa berhenti
}

float pompaTerpumpa(int idx) {
  if (idx < 0 || idx > 2) return 0;
  return totalMlTerpompa[idx];
}

float pompaTerpumpaTotal() {
  return totalMlTerpompa[0] + totalMlTerpompa[1] + totalMlTerpompa[2];
}

void pompaResetTotal() {
  for (int i = 0; i < 3; i++) totalMlTerpompa[i] = 0;
}

void pompaStopAll() {
  // Rekam dulu: pompa yang sedang jalan sudah memompa sebagian ml, dan cairan
  // itu benar-benar keluar dari tangki walau proses dihentikan mendadak.
  for (int i = 0; i < 3; i++) pompaRekamHasil(i);
}

void pompaMulai(int idx, int ml) {
  if (idx < 0 || idx > 2 || ml <= 0) return;
  // Rekaman HARUS di sini sebelum `jalanLangkah` di-reset. Pompa H2O dipakai
  // ulang untuk bilas di fase RINSE tanpa dihentikan lebih dulu, jadi tanpa
  // baris ini sisa ml H2O dari DOSING hilang begitu pompa dijalankan lagi.
  //
  // Kalibrasi stepper (`.ino` HAL_KALIB_STEPPER) juga memanggil fungsi ini
  // dengan 1 ml untuk ditimbang. Itu ikut terekam, tapi tidak masalah:
  // `prosesMulai()` memanggil `pompaResetTotal()` sehingga angkanya selalu
  // bersih di awal setiap proses.
  pompaRekamHasil(idx);
  stepperSleep(false);                // pastikan driver aktif
  targetLangkah[idx] = (long)ml * langkahPerMl[idx];
  jalanLangkah[idx]  = 0;
  pompaJalan[idx]    = true;
  dbg.print(F("[POMPA] Mulai idx "));
  dbg.print(idx);
  dbg.print(F(", "));
  dbg.print(ml);
  dbg.println(F(" ml"));
}

void motorUpdate() {
  unsigned long now = micros();
  for (int i = 0; i < 3; i++) {
    if (!pompaJalan[i]) continue;
    if (jalanLangkah[i] >= targetLangkah[i]) {
      pompaRekamHasil(i);             // sekaligus set pompaJalan = false
      dbg.print(F("[POMPA] Selesai idx "));
      dbg.print(i);
      dbg.print(F(": "));
      dbg.println(totalMlTerpompa[i]);
      continue;
    }
    if (now - langkahTerakhirUs[i] >= LANGKAH_INTERVAL_US) {
      langkahTerakhirUs[i] = now;
      digitalWrite(PIN_STEP[i], HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_STEP[i], LOW);
      jalanLangkah[i]++;
    }
  }
}

// Semua pompa berhenti + driver dimatikan (EN = HIGH).
// Dipakai saat E-stop / batal / selesai supaya motor tidak panas dan CurrentScale
// tidak menguras catu daya.
void pompaMatikanTotal() {
  pompaStopAll();                     // merekam ml tersisa sebelum driver dimatikan
  stepperSleep(true);
}