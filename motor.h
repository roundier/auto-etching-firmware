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

// ================= KONSTANTA KHUSUS DOSING (file A) =================
// Dipakai dengan `#ifndef` supaya bila B nanti memindahkannya ke `config.h`
// (pemilik file B), versi config.h yang menang tanpa konflik define.
#ifndef PRIME_STEPS
#define PRIME_STEPS       200      // langkah prime selang sebelum dosing
#endif
#ifndef ANTI_DRIP_STEPS
#define ANTI_DRIP_STEPS    50      // langkah balik DIR setelah pompa selesai
#endif
#ifndef DOSE_MIN_ML
#define DOSE_MIN_ML         1      // guard volume minimum per pompa
#endif
#ifndef DOSE_MAX_ML
#define DOSE_MAX_ML       500      // guard volume maksimum per pompa
#endif

// Mode tiap pompa. Rekaman ml HARUS hanya terjadi untuk dosis: prime dan
// anti-drip memang menggerakkan rotor, tapi airnya bukan bagian resep.
#define POMPA_MODE_DOSIS     0
#define POMPA_MODE_PRIME     1
#define POMPA_MODE_ANTIDRIP  2
uint8_t pompaMode[3] = { POMPA_MODE_DOSIS, POMPA_MODE_DOSIS, POMPA_MODE_DOSIS };

// Dideklarasikan DI SINI (bukan di process.h) karena dipakai oleh motorUpdate()
// yang dideklarasikan lebih dulu dalam urutan include. Saat jeda, pompa
// ditahan di posisi sekarang: tidak maju, tapi juga tidak direkam sebagai ml
// terpompa — sehingga resume melanjutkan dari langkah tersisa, bukan mengulang.
bool pompaJedaTahanPompa = false;

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
// hanya berefek kalau pompa sedang jalan, dan TIDAK merekam untuk mode
// prime/anti-drip (airnya bukan bagian resep).
void pompaRekamHasil(int idx) {
  if (idx < 0 || idx > 2) return;
  if (!pompaJalan[idx]) return;
  if (pompaMode[idx] == POMPA_MODE_DOSIS && langkahPerMl[idx] > 0)
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
  if (idx < 0 || idx > 2) return;
  if (ml <= 0) return;                       // ml = 0 -> dilewati, bukan macet
  // Guard volume: mencegah satu preset rusak meledakkan pompa ribuan langkah.
  if (ml < DOSE_MIN_ML) ml = DOSE_MIN_ML;
  if (ml > DOSE_MAX_ML) {
    dbg.print(F("[POMPA] idx "));
    dbg.print(idx);
    dbg.print(F(" ml="));
    dbg.print(ml);
    dbg.println(F(" > DOSE_MAX, dibatasi"));
    ml = DOSE_MAX_ML;
  }
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
  pompaMode[idx]     = POMPA_MODE_DOSIS;
  targetLangkah[idx] = (long)ml * langkahPerMl[idx];
  jalanLangkah[idx]  = 0;
  pompaJalan[idx]    = true;
  dbg.print(F("[POMPA] Mulai idx "));
  dbg.print(idx);
  dbg.print(F(", "));
  dbg.print(ml);
  dbg.println(F(" ml"));
}

// SIAPKAN target tanpa menyalakan pompa.
//
// Dipakai saat proses baru mulai: `pompaMulai()` sekaligus menyalakan pompa,
// padahal proses baru akan melakukan cek stok + prime lebih dulu. Dose baru
// boleh jalan di tahap DOSIS. Fungsi ini hanya menghitung `targetLangkah`
// supaya budget durasi bisa dihitung sejak awal.
void pompaSiapkan(int idx, int ml) {
  if (idx < 0 || idx > 2) return;
  if (ml < 0) ml = 0;
  if (ml > DOSE_MAX_ML) ml = DOSE_MAX_ML;
  pompaMode[idx]     = POMPA_MODE_DOSIS;
  targetLangkah[idx] = (ml <= 0) ? 0 : (long)ml * langkahPerMl[idx];
  jalanLangkah[idx]  = 0;
  pompaJalan[idx]    = false;
}

void motorUpdate() {
  // Jeda / latch E-stop: pompa berhenti di posisi, target & jalanLangkah tetap
  // utuh sehingga resume melanjutkan (bukan reset atau double-dosis).
  if (pompaJedaTahanPompa) return;

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
  antiDripAktif = false;
  stepperSleep(true);
}

// ================= API PROGRESS & PRIME / ANTI-DRIP (file A) =================
// Dipakai process.h untuk menyelesaikan fase dosing berdasarkan KONDISI
// (pompa selesai) alih-alih fraksi waktu.

// true bila ketiga pompa tidak sedang berjalan.
bool pompaSemuaSelesai() {
  for (int i = 0; i < 3; i++) if (pompaJalan[i]) return false;
  return true;
}

// Progres 0..100 persen per pompa (dipakai layer serial & log).
uint8_t pompaProgres(int idx) {
  if (idx < 0 || idx > 2) return 0;
  if (targetLangkah[idx] <= 0) return 100;
  long v = (jalanLangkah[idx] * 100L) / targetLangkah[idx];
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  return (uint8_t)v;
}

//.ml target fase ini (dosis terakhir yang diminta), untuk tampilan.
int pompaTargetMl(int idx) {
  if (idx < 0 || idx > 2) return 0;
  if (langkahPerMl[idx] <= 0) return 0;
  return (int)((targetLangkah[idx] + langkahPerMl[idx] / 2) / langkahPerMl[idx]);
}

// True bila kalibrasi ml/rotasi sudah diisi user. B belum menyimpan flag
// eksplisit (butir kontrak B9), jadi dipakai pendekatan lain: nilai default NVS
// 1.0 ml/rotasi dianggap "belum dikalibrasi".
bool pompaTerkalibrasi() {
  for (int i = 0; i < 3; i++) {
    if (kalibStepperMl[i] >= 0.99f && kalibStepperMl[i] <= 1.01f) return false;
    if (langkahPerMl[i] <= 0) return false;
  }
  return true;
}

// ---- PRIME -------------------------------------------------------------
// Pompa peristaltik yang mulai dengan selang kosong hanya menghisap udara:
// 300 ml bisa keluar jadi 250 ml, atau tidak keluar sama sekali, tanpa error
// apa pun. Prime mengisi selang dulu dengan PRIME_STEPS langkah per pompa.
//
// CATATAN AKURASI: air prime ikut masuk ke tangki proses (volume kecil), jadi
// setelah priming volume tangki proses sedikit lebih banyak dari resep. Ini
// diterima karena jauh lebih kecil daripada dosing tanpa prime.

// Prime hanya pompa yang memang akan dipakai (idx yang ml > 0) supaya
// selang bahan yang tidak dipakai tidak ikut basah.
void pompaPrimeMulai(const int* pakai, int jumlah) {
  stepperSleep(false);
  int n = 0;
  for (int i = 0; i < 3 && n < jumlah; i++) {
    if (!pakai[i]) continue;
    if (pompaJalan[i]) continue;
    pompaMode[i]     = POMPA_MODE_PRIME;
    targetLangkah[i] = PRIME_STEPS;
    jalanLangkah[i]  = 0;
    pompaJalan[i]    = true;
    n++;
  }
  dbg.print(F("[PRIME] "));
  dbg.print(n);
  dbg.print(F(" pompa x "));
  dbg.print(PRIME_STEPS);
  dbg.println(F(" langkah"));
}

// ---- ANTI-DRIP ---------------------------------------------------------
// Setelah pompa berhenti, tekanan sisa di dalam selang mendorong cairan
// menetes ke tangki proses -> over-dosis kecil tapi nyata. Solusi: balik DIR
// sebentar supaya tekanan hilang, lalu kembalikan DIR ke arah sedot.
bool antiDripAktif = false;

void pompaAntiDripMulai() {
  antiDripAktif = true;
  stepperSleep(false);
  for (int i = 0; i < 3; i++) digitalWrite(PIN_DIR[i], LOW);   // arah balik
  int n = 0;
  for (int i = 0; i < 3; i++) {
    if (targetLangkah[i] <= 0) continue;      // pompa tidak dipakai fase ini
    pompaMode[i]     = POMPA_MODE_ANTIDRIP;
    targetLangkah[i] = ANTI_DRIP_STEPS;
    jalanLangkah[i]  = 0;
    pompaJalan[i]    = true;
    n++;
  }
  dbg.print(F("[ANTIDRIP] "));
  dbg.print(n);
  dbg.print(F(" pompa x "));
  dbg.print(ANTI_DRIP_STEPS);
  dbg.println(F(" langkah"));
}

// Dipanggil sekali setelah semua pompa anti-drip selesai: kembalikan DIR.
void pompaAntiDripSelesai() {
  for (int i = 0; i < 3; i++) digitalWrite(PIN_DIR[i], HIGH);  // arah sedot
  antiDripAktif = false;
  dbg.println(F("[ANTIDRIP] Selesai, DIR dikembalikan"));
}