#pragma once
#include <Arduino.h>
#include "data.h"
#include "motor.h"
#include "dcmotor.h"
#include "sensor.h"   // sensorMati[] - deteksi sensor mati saat proses berjalan
#include "buzzer.h"
#include "led_strip.h"

// ================= STATE MACHINE PROSES (v0.7.4 - file A) =================
// Logika proses berjalan di prosesUpdate() (dipanggil tiap loop), TERPISAH
// dari render. Tiap transisi fase juga mengubah mode LED strip.
//
// PERUBAHAN BESAR v0.7.4 (Bab 3, 4, 5):
//   - Dulu perpindahan fase memakai FRAKSI WAKTU dari `totalDetik`
//     (`faseDariFrac()`): 10% dosing, 75% etching, 88% draining. Akibatnya
//     pompa bisa masih berjalan saat fase sudah ETCHING, atau selesai jauh
//     sebelum fase berikutnya.
//   - Sekarang SETIAP FASE PADA DASARNYA BERBASIS KONDISI:
//       DOSING  -> selesai saat pompa benar-benar selesai (bukan timer)
//       ETCHING -> selesai saat budget waktu etching habis
//       DRAINING/RINSING -> selesai setelah durasi tetap (T_*), bukan fraksi
//     Durasi total dihitung dari rencana (budget) tiap fase, jadi progress bar
//     jujur dengan apa yang benar-benar dijalankan.
//
// Sub-fase (CEK / PRIME / DOSIS / ANTI-DRIP / BILAS AIR / SABUN / DRAIN
// AKHIR) ditangani variabel `prosesTahap`, bukan enum baru — supaya `ui.h`
// (file B) yang mengindeks `namaFase[]` dan array 6 elemen tetap utuh.

enum FaseProses {
  FASE_IDLE,
  FASE_DOSING,
  FASE_ETCHING,
  FASE_DRAINING,
  FASE_RINSING,
  FASE_SELESAI
};
const char* namaFase[] = {
  "IDLE", "DOSING", "ETCHING", "DRAINING", "RINSING", "SELESAI"
};

// ================= KONSTANTA PROSES (file A) =================
// Sengaja di `#ifndef` semua: kalau B memutuskan memindahkannya ke `config.h`
// (pemilik file config.h), versi config.h otomatis menang tanpa bentrok.
#ifndef TOLERANSI_STOK
#define TOLERANSI_STOK        10      // ml toleransi sensor capacitive
#endif
#ifndef VOL_TANGKI_HCL
#define VOL_TANGKI_HCL     1000
#endif
#ifndef VOL_TANGKI_H2O2
#define VOL_TANGKI_H2O2    1000
#endif
#ifndef VOL_TANGKI_H2O
#define VOL_TANGKI_H2O     2000
#endif
#ifndef VOL_TANGKI_SABUN
#define VOL_TANGKI_SABUN   1000
#endif
#ifndef VOL_TANGKI_LIMBAH
#define VOL_TANGKI_LIMBAH  5000
#endif
#ifndef DOSING_MARGIN_MS
#define DOSING_MARGIN_MS  15000UL    // margin watchdog di atas estimasi
#endif
#ifndef DOSING_TIMEOUT_MAKS_MS
#define DOSING_TIMEOUT_MAKS_MS 300000UL  // plafon keras 5 menit
#endif
#ifndef T_DRAINING_MS
#define T_DRAINING_MS    45000UL
#endif
#ifndef T_RINSE_AIR_MS
#define T_RINSE_AIR_MS   30000UL
#endif
#ifndef T_RINSE_SABUN_MS
#define T_RINSE_SABUN_MS 10000UL
#endif
#ifndef T_DRAIN_AKHIR_MS
#define T_DRAIN_AKHIR_MS 15000UL
#endif
#ifndef ML_BILASAN
#define ML_BILASAN          200      // ml air bilasan (pompa stepper)
#endif
#ifndef ML_BILASAN_SABUN
#define ML_BILASAN_SABUN     50      // ml air sabun (pompa DC, durasi-based)
#endif
#ifndef LOKASI_LIMBAH_MAKS
#define LOKASI_LIMBAH_MAKS   95      // % -> guard overfill
#endif
#ifndef SPEED_AGITASI_BILAS
#define SPEED_AGITASI_BILAS   2
#endif
#ifndef SUHU_MAKS_KOMPONEN
#define SUHU_MAKS_KOMPONEN  70.0f     // °C -> turunkan speed agitasi
#endif
#ifndef SUHU_MAKS_STOP
#define SUHU_MAKS_STOP      85.0f     // °C -> gagal
#endif
#ifndef GAS_AMANG_PPM
#define GAS_AMANG_PPM        400      // MQ-135 -> badge peringatan (bukan stop)
#endif
#ifndef AGITASI_SIKLUS_MS
#define AGITASI_SIKLUS_MS  1000UL    // waktu ON/OFF minimum satu siklus agitasi
#endif

// ================= SUB-FASE (TAHAP) =================
enum TahapProses {
  TAHAP_CEK = 0,       // pre-flight: sensor, stok, pompa terkalibrasi
  TAHAP_PRIME,         // isi selang (hanya pompa yang dipakai)
  TAHAP_DOSIS,         // pompa paralel bekerja
  TAHAP_ANTIDRIP,      // balik DIR, tekan tekanan sisa
  TAHAP_SIKLUS,        // etching: agitasi ON/OFF + proteksi
  TAHAP_DRAIN,         // buang etchant
  TAHAP_BILAS_AIR,     // bilas air (stepper)
  TAHAP_BILAS_SABUN,   // bilas air sabun (pompa DC) + fan
  TAHAP_DRAIN_AKHIR    // buang air bilasan
};
const char* namaTahap[] = {
  "CEK STOK", "PRIME", "DOSIS", "ANTI-DRIP", "SIKLUS AGITASI",
  "BUANG", "BILAS AIR", "BILAS SABUN", "BUANG AKHIR"
};

unsigned char prosesTahap = TAHAP_CEK;
unsigned long prosesTahapMs = 0;   // millis() saat sub-fase terakhir berubah
const char* prosesTahapNama() { return namaTahap[prosesTahap]; }

// Badge peringatan untuk UI (MQ-135 / suhu). Nilai ini dibaca `ui.h` kalau B
// sudah menambahkan badge-nya; sampai itu hanya tampil di Serial.
bool badgeWarning = false;
char badgeTeks[18] = "-";

// Volume tangki sumber untuk cek ketersediaan (ml).
int volumeTangki(int idx) {
  switch (idx) {
    case 0:  return VOL_TANGKI_HCL;
    case 1:  return VOL_TANGKI_H2O2;
    case 2:  return VOL_TANGKI_H2O;
    case 3:  return VOL_TANGKI_SABUN;
    default: return VOL_TANGKI_LIMBAH;
  }
}

// ================= STATE PROSES =================
bool prosesJalan = false;          // true = proses aktif (termasuk saat jeda)
bool prosesJeda  = false;
bool dialogBatal = false;          // dialog konfirmasi batal sedang tampil
unsigned long prosesAkumMs      = 0;   // waktu proses berjalan (tanpa jeda)
unsigned long prosesWaktuMs     = 0;   // sama dengan prosesAkumMs; dipakai layar selesai
unsigned long prosesTerakhirMs  = 0;
unsigned long prosesMasukFaseMs = 0;   // prosesWaktuMs saat fase terakhir berubah
int  prosesDetikTerakhir        = -1;
int  prosesSiklus               = 0;   // siklus agitasi ke berapa
FaseProses prosesFase            = FASE_IDLE;

// Latch E-stop. Sekali aktif TIDAK otomatis lepas saat pin kembali HIGH.
bool estopAktif = false;

// Budget nyata tiap fase (ms) - hasil hitungan, bukan tebakan preset.
unsigned long budgetFaseMs[6]      = { 0, 0, 0, 0, 0, 0 };
unsigned long budgetSelesaiMs      = 0; // akumulasi fase yang sudah selesai

// Deteksi pompa macet / jekot (lihat catatan di prosesUpdate).
#define MACET_MS 3000UL
long langkahMacetAcuan[3] = { 0, 0, 0 };
unsigned long langkahMacetMs[3] = { 0, 0, 0 };

int prosesTotalDetik() { return daftarPreset[presetTerpilih].totalDetik; }
bool prosesSelesai() { return prosesFase == FASE_SELESAI; }

// ================= RENCANA / BUDGET WAKTU =================
// Estimasi durasi satu pompa dari jumlah langkah yang harus dijalankan.
unsigned long estimasiPompaMs(int idx) {
  if (idx < 0 || idx > 2) return 0;
  return ((unsigned long)targetLangkah[idx] * (unsigned long)LANGKAH_INTERVAL_US) / 1000UL;
}

// Perkiraan durasi fase dosing: pompa TERLAMBAT + prime + anti-drip.
// Dipakai sebagai budget progress bar.
unsigned long estimasiDosisMs() {
  unsigned long t = 0;
  for (int i = 0; i < 3; i++) {
    unsigned long e = estimasiPompaMs(i);
    if (e > t) t = e;
  }
  // + prime & anti-drip untuk 3 pompa (paralel -> cukup sekali hitung)
  t += (3UL * (PRIME_STEPS + ANTI_DRIP_STEPS) * LANGKAH_INTERVAL_US) / 1000UL;
  if (t < 1000UL) t = 1000UL;
  return t;
}

// Watchdog dosing TIDAK boleh konstanta mati: dengan 1200 us/langkah, dosis
// 300 ml butuh ratusan detik, jadi batas tetap 90 s akan membunuh proses yang
// sah (dosing-design.md §2.6). Hitung dari laju nyata + margin, dengan plafon.
unsigned long dosingWatchdogMs() {
  unsigned long wd = estimasiDosisMs() * 2UL + DOSING_MARGIN_MS;
  return (wd > DOSING_TIMEOUT_MAKS_MS) ? DOSING_TIMEOUT_MAKS_MS : wd;
}

// Hitung ulang budget semua fase. Dipanggil saat proses mulai.
void hitungRencana() {
  Preset &p = daftarPreset[presetTerpilih];

  budgetFaseMs[FASE_DOSING]   = estimasiDosisMs();
  budgetFaseMs[FASE_DRAINING] = T_DRAINING_MS;
  budgetFaseMs[FASE_RINSING]  = T_RINSE_AIR_MS + T_RINSE_SABUN_MS + T_DRAIN_AKHIR_MS;
  budgetFaseMs[FASE_IDLE]     = 0;
  budgetFaseMs[FASE_SELESAI]  = 0;

  // Budget etching: preset punya dua angka relevant — `agitasiDetik` (durasi
  // agitasi) dan `totalDetik` (durasi proses yang diinginkan operator). Di v0.7.3
  // etching memakai 65% dari totalDetik. Supaya intentions tetap terjaga:
  //   budget = max(agitasiDetik, totalDetik - (dosing + drain + rinse))
  // sehingga tidak pernah negatif dan tidak pernah lebih kecil dari durasi
  // agitasi yang diminta. Log di Serial supaya mudah dikalibrasi di hardware.
  unsigned long overhead = budgetFaseMs[FASE_DOSING]
                         + budgetFaseMs[FASE_DRAINING]
                         + budgetFaseMs[FASE_RINSING];
  unsigned long dariTotal = ((unsigned long)p.totalDetik * 1000UL > overhead)
                          ? ((unsigned long)p.totalDetik * 1000UL - overhead) : 0UL;
  unsigned long dariAgitasi = (unsigned long)p.agitasiDetik * 1000UL;
  budgetFaseMs[FASE_ETCHING] = (dariTotal > dariAgitasi) ? dariTotal : dariAgitasi;
  if (budgetFaseMs[FASE_ETCHING] < 1000UL) budgetFaseMs[FASE_ETCHING] = 1000UL;
  budgetSelesaiMs = 0;

  dbg.print(F("[RENCANA] dosing="));
  dbg.print(budgetFaseMs[FASE_DOSING] / 1000);
  dbg.print(F("s (wd "));
  dbg.print(dosingWatchdogMs() / 1000);
  dbg.print(F("s) etching="));
  dbg.print(budgetFaseMs[FASE_ETCHING] / 1000);
  dbg.print(F("s drain="));
  dbg.print(budgetFaseMs[FASE_DRAINING] / 1000);
  dbg.print(F("s rinse="));
  dbg.println(budgetFaseMs[FASE_RINSING] / 1000);
}

// Batas timeout per fase (ms). 0 = tidak dibatasi.
unsigned long batasTimeoutFase() {
  switch (prosesFase) {
    case FASE_DOSING:   return dosingWatchdogMs();
    case FASE_ETCHING:  return budgetFaseMs[FASE_ETCHING] * 2UL + DOSING_MARGIN_MS;
    case FASE_DRAINING: return T_DRAINING_MS * 2UL;
    case FASE_RINSING:  return (T_RINSE_AIR_MS + T_RINSE_SABUN_MS + T_DRAIN_AKHIR_MS) * 2UL;
    default: return 0;
  }
}

// ================= PROGRESS (dipakai ui.h) =================
// Fraksi keseluruhan = (budget fase yang sudah selesai + waktu fase berjalan) /
// total budget. Nie "jujur" dengan mesin, berbeda dengan fraksi waktu preset.
float prosesFraksiTotal() {
  unsigned long total = budgetSelesaiMs + budgetFaseMs[prosesFase];
  if (total == 0) return 0;
  unsigned long sudah = budgetSelesaiMs + (prosesWaktuMs - prosesMasukFaseMs);
  if (sudah > total) sudah = total;
  return (float)sudah / (float)total;
}

// Progres 0..1 DI DALAM fase yang sedang berjalan.
float prosesFraksiFase() {
  unsigned long b = budgetFaseMs[prosesFase];
  if (b == 0) return 1.0f;
  unsigned long t = prosesWaktuMs - prosesMasukFaseMs;
  if (t > b) t = b;
  return (float)t / (float)b;
}

// ================= SEMUA AKTUATOR MATI =================
void semuaAktuatorMati() {
  agitasiSet(0);
  agitasiRampBerhenti();
  pompaLimbahSet(false);
  pompaSabunSet(false);
  fanSet(false);
  pompaMatikanTotal();
}

// Aksi hardware saat memasuki sebuah fase.
void aksiFase(FaseProses f) {
  Preset &p = daftarPreset[presetTerpilih];
  switch (f) {
    case FASE_DOSING:
      ledProses();
      dcmotorStopAll();
      // Pompa TIDAK langsung dinyalakan: tahap CEK & PRIME mendahului.
      break;
    case FASE_ETCHING:
      ledProses();
      pompaStopAll();
      fanSet(true);                       // exhaust: asam + panas (Bab 4)
      agitasiSetRamp(p.agitasiSpeed);     // naik bertahap, bukan "shock"
      break;
    case FASE_DRAINING:
      ledProses();
      agitasiSet(0);
      fanSet(false);
      pompaLimbahSet(true);
      break;
    case FASE_RINSING:
      ledProses();
      pompaLimbahSet(false);
      // Hanya mulai bila belum jalan: `aksiFase()` juga dipanggil saat RESUME
      // dari jeda. Tanpa guard ini setiap resume memanggil pompaMulai() lagi
      // dan air bilasan ter-dose dua kali.
      if (!pompaJalan[2]) pompaMulai(2, ML_BILASAN);
      agitasiSetRamp(SPEED_AGITASI_BILAS);
      break;
    case FASE_SELESAI:
      semuaAktuatorMati();
      break;
    default: break;
  }
}

// Pindah fase: akumulasikan budget fase yang baru selesai.
void faseLanjut(FaseProses f) {
  budgetSelesaiMs += budgetFaseMs[prosesFase];
  prosesFase = f;
  prosesMasukFaseMs = prosesWaktuMs;
  prosesSiklus = 0;
  dbg.print(F("[PROSES] Fase: "));
  dbg.println(namaFase[f]);
  aksiFase(f);
  if (f == FASE_SELESAI) {
    buzzerSelesai();
    ledSelesai();
    dialogBatal = false;
  }
  perluGambar = true;
}

void prosesMulai() {
  if (prosesError != ERR_NONE || estopAktif) {
    dbg.println(F("[PROSES] Ditolak: alarm/E-stop belum direset"));
    return;
  }
  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  prosesAkumMs = 0;
  prosesWaktuMs = 0;
  badgeWarning = false;
  snprintf(badgeTeks, sizeof(badgeTeks), "-");
  pompaResetTotal();

  prosesFase = FASE_IDLE;
  prosesTahap = TAHAP_CEK;
  prosesMasukFaseMs = 0;

  // Siapkan target DOSE (pompa BELUM dinyalakan — harus lewat cek stok &
  // prime dulu). Tujuannya hanya supaya budget durasi bisa dihitung di awal.
  Preset &p = daftarPreset[presetTerpilih];
  pompaSiapkan(0, p.mlHCl);
  pompaSiapkan(1, p.mlH2O2);
  pompaSiapkan(2, p.mlH2O);

  hitungRencana();

  for (int i = 0; i < 3; i++) { langkahMacetAcuan[i] = 0; langkahMacetMs[i] = millis(); }
  prosesTerakhirMs = millis();
  prosesDetikTerakhir = -1;

  faseLanjut(FASE_DOSING);
  dbg.print(F("[PROSES] Mulai: "));
  dbg.print(p.nama);
  dbg.print(F(" ("));
  dbg.print(p.mlHCl); dbg.print(F("/"));
  dbg.print(p.mlH2O2); dbg.print(F("/"));
  dbg.print(p.mlH2O); dbg.println(F(" ml)"));
  buzzerMulai();
  perluGambar = true;
}

void prosesToggleJeda() {
  if (!prosesJalan || prosesSelesai()) return;
  prosesJeda = !prosesJeda;
  dbg.println(prosesJeda ? F("[PROSES] Jeda") : F("[PROSES] Lanjut"));
  if (prosesJeda) {
    // Jeda mematikan output, bukan hanya timer. Pompa yang sedang jalan
    // TIDAK direkam (pompaStopAll tidak dipanggil di sini): isinya akan
    // direkam begitu pompa benar-benar berhenti, supaya resume melanjutkan
    // dari langkah tersisa - bukan mengulang dosis.
    agitasiSet(0);
    agitasiRampBerhenti();
    pompaLimbahSet(false);
    pompaSabunSet(false);
    fanSet(false);
    pompaJedaTahanPompa = true;   // motorUpdate() menahan pompa
    ledJeda();
  } else {
    pompaJedaTahanPompa = false;
    aksiFase(prosesFase);
    ledProses();
  }
  perluGambar = true;
}

void prosesBatal() {
  dbg.print(F("[PROSES] Dibatalkan pada detik ke-"));
  dbg.println(prosesAkumMs / 1000);
  buzzerBatal();
  prosesJalan = false;
  prosesJeda = false;
  dialogBatal = false;
  prosesFase = FASE_IDLE;
  pompaJedaTahanPompa = false;
  semuaAktuatorMati();
  ledError();
  perluGambar = true;
}

// ================= FAIL-SAFE & ALARM =================
// SATU-SATUNYA jalan keluar kondisi abnormal.
void prosesGagal(KodeError kode) {
  if (prosesError != ERR_NONE) return;
  prosesError = kode;
  prosesJalan = false;
  prosesJeda  = false;
  dialogBatal = false;
  prosesFase  = FASE_IDLE;
  pompaJedaTahanPompa = false;
  semuaAktuatorMati();
  buzzerAlarm();
  ledError();
  dbg.print(F("[ALARM] "));
  dbg.print(namaError[kode]);
  dbg.print(F(" pada detik ke-"));
  dbg.println(prosesAkumMs / 1000);
  pindahHalaman(HAL_ALARM);
  perluGambar = true;
}

void prosesPulihkan() {
  prosesError = ERR_NONE;
  estopAktif  = false;
  buzzerAlarmStop();
  buzzerSelesai();
  ledIdle();
  pindahHalaman(HAL_MENU_UTAMA);
  perluGambar = true;
}

void prosesKeluarSelesai() {
  prosesJalan = false;
  prosesFase = FASE_IDLE;
  prosesError = ERR_NONE;
  ledIdle();
  perluGambar = true;
}

// ================= DOSING: CEK STOK =================
// Pre-flight. Dijalankan sebagai TAHAP_CEK di dalam FASE_DOSING.
//Sensor mati / tangki belum terkalibrasi / cairan kurang -> berhenti SEBELUM
// pompa menyala, supaya tidak ada(Setengah) dosis lalu gagal.
bool dosingCekStok(bool sudahPrime) {
  Preset &p = daftarPreset[presetTerpilih];
  int target[3] = { p.mlHCl, p.mlH2O2, p.mlH2O };

  // 1. Sensor mati?
  for (int i = 0; i < 3; i++) {
    if (sensorMati[i]) {
      dbg.print(F("[CEK] Sensor ")); dbg.print(i); dbg.println(F(" mati"));
      prosesGagal(ERR_SENSOR);
      return false;
    }
  }

  // 2. Pompa terkalibrasi? Kalau belum, dosis akan meleset jauh.
  if (!pompaTerkalibrasi()) {
    dbg.println(F("[CEK] PERINGATAN: pompa belum dikalibrasi (default 1.0 ml/rotasi)"));
    snprintf(badgeTeks, sizeof(badgeTeks), "KALIB?");
  }

  // 3. Stok cukup?
  for (int i = 0; i < 3; i++) {
    int persen = constrain(sensorPersen[i], 0, 100);
    int tersedia = (persen * volumeTangki(i)) / 100;
    if (target[i] > tersedia + TOLERANSI_STOK) {
      dbg.print(F("[CEK] Tangki "));
      dbg.print(i);
      dbg.print(F(" kurang: butuh "));
      dbg.print(target[i]);
      dbg.print(F(" ml, ada "));
      dbg.print(tersedia);
      dbg.print(F(" ml (")); dbg.print(persen); dbg.println(F("%)"));
      prosesGagal(ERR_LEVEL_KURANG);
      return false;
    }
  }

  // 4. Cairan benar-benar terdeteksi (sensor ADA, bukan sekadar persen).
  if (sudahPrime) {
    for (int i = 0; i < 3; i++) {
      if (target[i] > 0 && !sensorAdaCairan(i)) {
        dbg.print(F("[CEK] Tangki "));
        dbg.print(i);
        dbg.println(F(" terdeteksi kosong"));
        prosesGagal(ERR_LEVEL_KURANG);
        return false;
      }
    }
  }
  return true;
}

// ================= UPDATE PROSES =================
void prosesUpdate() {
  if (!prosesJalan) return;

  unsigned long now = millis();
  if (!prosesJeda && !prosesSelesai()) {
    unsigned long d = now - prosesTerakhirMs;
    prosesWaktuMs += d;
    prosesAkumMs  += d;
  }
  prosesTerakhirMs = now;

  // Ramp agitasi selalu jalan (memakai millis(), jadi aman saat jeda).
  agitasiUpdate();

  int d = prosesAkumMs / 1000;
  if (d != prosesDetikTerakhir) {
    prosesDetikTerakhir = d;
    perluGambar = true;
  }

  // ---- Pemeriksaan fail-safe (hanya saat aktif & tidak dijeda) ----------
  if (prosesJeda || prosesSelesai()) return;

  // 1. Pompa macet / jekot: pompaJalan tapi langkah beku.
  for (int i = 0; i < 3; i++) {
    if (!pompaJalan[i]) continue;
    if (jalanLangkah[i] != langkahMacetAcuan[i]) {
      langkahMacetAcuan[i] = jalanLangkah[i];
      langkahMacetMs[i] = now;
      continue;
    }
    if (now - langkahMacetMs[i] > MACET_MS) {
      dbg.print(F("[ALARM] Pompa ")); dbg.print(i);
      dbg.println(F(" macet (langkah beku)"));
      prosesGagal(ERR_STEP_MACET);
      return;
    }
  }

  // 2. Sensor mati saat proses jalan.
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (sensorMati[i]) {
      dbg.print(F("[ALARM] Sensor ")); dbg.print(i);
      dbg.println(F(" mati"));
      prosesGagal(ERR_SENSOR);
      return;
    }
  }

  // 3. Timeout per fase.
  unsigned long maks = batasTimeoutFase();
  if (maks > 0 && (prosesWaktuMs - prosesMasukFaseMs) > maks) {
    dbg.print(F("[ALARM] Timeout fase "));
    dbg.println(namaFase[prosesFase]);
    prosesGagal(ERR_TIMEOUT);
    return;
  }

  // ================= TRANSISI SUB-FASE BERBASIS KONDISI =================
  switch (prosesFase) {

    // ---------- DOSING: CEK -> PRIME -> DOSIS -> ANTI-DRIP ----------
    case FASE_DOSING: {
      unsigned long sejak = now - prosesTahapMs;

      // Sumber habis SAAT pompa jalan: langsung berhenti, jangan lanjut.
      if (prosesTahap == TAHAP_DOSIS) {
        for (int i = 0; i < 3; i++) {
          if (pompaJalan[i] && !sensorAdaCairan(i)) {
            dbg.print(F("[ALARM] Sumber tangki ")); dbg.print(i);
            dbg.println(F(" habis saat dosing"));
            prosesGagal(ERR_LEVEL_KURANG);
            return;
          }
        }
      }

      switch (prosesTahap) {
        case TAHAP_CEK: {
          // Hold 1 detik supaya operator sempat membaca "CEK STOK".
          if (!dosingCekStok(false)) return;
          if (sejak < 1000UL) break;
          Preset &p = daftarPreset[presetTerpilih];
          int pakai[3] = { p.mlHCl > 0, p.mlH2O2 > 0, p.mlH2O > 0 };
          pompaPrimeMulai(pakai, 3);
          prosesTahap = TAHAP_PRIME;
          prosesTahapMs = now;
          break;
        }
        case TAHAP_PRIME:
          if (!pompaSemuaSelesai()) break;
          if (!dosingCekStok(true)) return;      // cek ulang setelah prime
          prosesTahap = TAHAP_DOSIS;
          prosesTahapMs = now;
          // BARU di sini pompa dinyalakan — setelah stok aman & selang terisi.
          stepperSleep(false);
          pompaMulai(0, daftarPreset[presetTerpilih].mlHCl);
          pompaMulai(1, daftarPreset[presetTerpilih].mlH2O2);
          pompaMulai(2, daftarPreset[presetTerpilih].mlH2O);
          dbg.println(F("[DOSING] Pompa paralel jalan"));
          break;
        case TAHAP_DOSIS:
          // Inti perubahan v0.7.4: fase ini selesai KARENA pompa selesai,
          // bukan karena waktu. Dulu pompa bisa masih jalan saat fase sudah
          // berganti ke ETCHING.
          if (!pompaSemuaSelesai()) break;
          pompaAntiDripMulai();
          prosesTahap = TAHAP_ANTIDRIP;
          prosesTahapMs = now;
          break;
        case TAHAP_ANTIDRIP:
          if (!pompaSemuaSelesai()) break;
          pompaAntiDripSelesai();
          faseLanjut(FASE_ETCHING);
          prosesTahap = TAHAP_SIKLUS;
          prosesTahapMs = now;
          break;
        default: break;
      }
      break;
    }

    // ---------- ETCHING: siklus agitasi + proteksi ----------
    case FASE_ETCHING: {
      unsigned long sejak = prosesWaktuMs - prosesMasukFaseMs;
      unsigned long siklusMs = max((unsigned long)daftarPreset[presetTerpilih].agitasiDetik * 1000UL, AGITASI_SIKLUS_MS);
      unsigned long posMs = sejak % (2UL * siklusMs);
      bool mauAduk = (posMs < siklusMs);

      if (mauAduk != agitasiNyala()) {
        if (mauAduk) { agitasiSetRamp(daftarPreset[presetTerpilih].agitasiSpeed); prosesSiklus++; }
        else agitasiSet(0);
      }

      // Proteksi suhu: >70 turunkan speed, >85 gagal.
      if (sensorSuhuValid(1)) {
        float t = sensorSuhu(1);
        if (t > SUHU_MAKS_STOP) {
          dbg.print(F("[ALARM] Suhu komponen "));
          dbg.print(t); dbg.println(F(" °C"));
          prosesGagal(ERR_SUHU);
          return;
        }
        if (t > SUHU_MAKS_KOMPONEN) {
          snprintf(badgeTeks, sizeof(badgeTeks), "PANAS");
          badgeWarning = true;
        }
      }

      // Proteksi gas: hanya peringatan di UI, belum auto-stop.
      if (mq135PPM() > GAS_AMANG_PPM) {
        snprintf(badgeTeks, sizeof(badgeTeks), "GAS!");
        badgeWarning = true;
      }

      if (sejak >= budgetFaseMs[FASE_ETCHING]) {
        agitasiSet(0);
        faseLanjut(FASE_DRAINING);
        prosesTahap = TAHAP_DRAIN;
        prosesTahapMs = now;
      }
      break;
    }

    // ---------- DRAINING: durasi tetap + guard overfill ----------
    case FASE_DRAINING: {
      unsigned long sejak = prosesWaktuMs - prosesMasukFaseMs;

      // Guard overfill: tangki limbah hampir penuh -> stop + alarm.
      if (sensorPersen[4] > LOKASI_LIMBAH_MAKS) {
        dbg.print(F("[ALARM] Limbah "));
        dbg.print(sensorPersen[4]);
        dbg.println(F("% — overfill"));
        prosesGagal(ERR_OVERFLOW);
        return;
      }

      if (sejak >= T_DRAINING_MS) {
        pompaLimbahSet(false);
        faseLanjut(FASE_RINSING);
        prosesTahap = TAHAP_BILAS_AIR;
        prosesTahapMs = now;
      }
      break;
    }

    // ---------- RINSING: air -> sabun -> drain akhir ----------
    case FASE_RINSING: {
      unsigned long sejak = now - prosesTahapMs;

      switch (prosesTahap) {
        case TAHAP_BILAS_AIR:
          // Selesai bila pompa air selesai ATAU lewat batas waktu.
          if (pompaSelesai(2) || sejak >= T_RINSE_AIR_MS) {
            agitasiSet(0);
            pompaSabunSet(true);
            fanSet(true);                    // busa + asam sisa
            prosesTahap = TAHAP_BILAS_SABUN;
            prosesTahapMs = now;
          }
          break;
        case TAHAP_BILAS_SABUN:
          if (sejak >= T_RINSE_SABUN_MS) {
            pompaSabunSet(false);
            pompaLimbahSet(true);
            prosesTahap = TAHAP_DRAIN_AKHIR;
            prosesTahapMs = now;
          }
          break;
        case TAHAP_DRAIN_AKHIR:
          if (sensorPersen[4] > LOKASI_LIMBAH_MAKS) {
            dbg.println(F("[ALARM] Limbah penuh saat drain akhir"));
            prosesGagal(ERR_OVERFLOW);
            return;
          }
          if (sejak >= T_DRAIN_AKHIR_MS) {
            pompaLimbahSet(false);
            faseLanjut(FASE_SELESAI);
          }
          break;
        default: break;
      }
      break;
    }

    default: break;
  }
}