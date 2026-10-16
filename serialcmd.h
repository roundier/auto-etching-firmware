#pragma once
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "dbg.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "process.h"
#include "motor.h"
#include "dcmotor.h"
#include "storage.h"
#include "ui.h"

// ================= KONTROL VIA SERIAL (v0.7.0) =================
// Mengontrol alat dari Serial Monitor tanpa pencet tombol — berguna untuk pengujian
// otomatis dan debug jarak jauh.
//
// Format: awali '>', lalu teks perintah, diakhiri Enter (CR/LF).
//   >help        daftar perintah
//   >status      fase proses, waktu, level 5 tangki, status sensor
//   >start       mulai proses dengan preset terpilih
//   >stop        batalkan proses
//   >preset N    pilih preset N (1..jumlahPreset)
//   >sel <hal>   pindah halaman: menu | preset | cfg | level | kalib
//   >off         matikan semua aktuator + stepper (setara E-stop software)
//   >export      cetak daftar preset sebagai teks (untuk dicadangkan)
//   >import      baca daftar preset dari teks, lalu terapkan
//   >rollback    kembalikan daftar preset sebelum import terakhir
//   >abort       batalkan mode import
//
// NON-BLOCKING: parser membaca SATU karakter per iterasi loop() dan langsung
// memproses begitu baris lengkap. Sengaja TIDAK memakai `Serial.readString()` —
// fungsi itu menunggu Enter sambil memblokir, sehingga seluruh sistem membeku.
//
// Prefix '>' dipakai supaya log biasa dari `dbg.h` tidak ikut diparse.

#define CMD_MAX_LEN 32

char    cmdBuf[CMD_MAX_LEN];
uint8_t cmdLen = 0;

// `namaSensor[]` dideklarasikan di sensor.h; dipakai untuk printsStatus.
extern const char* namaSensor[JUMLAH_SENSOR];

bool cmdCocok(const char* s) {
  return strcasecmp(cmdBuf, s) == 0;
}

// ---------- Pretty print ----------

void cmdPrintHelp() {
  dbg.println(F("=== PERINTAH (awali '>', Enter) ==="));
  dbg.println(F("  >help        daftar perintah ini"));
  dbg.println(F("  >status      fase, waktu, level 5 tangki, status sensor"));
  dbg.println(F("  >start       mulai proses dengan preset terpilih"));
  dbg.println(F("  >stop        batalkan proses"));
  dbg.println(F("  >preset N    pilih preset N (1..jumlahPreset)"));
  dbg.println(F("  >sel <hal>   pindah: menu|preset|cfg|level|kalib"));
  dbg.println(F("  >off         matikan semua aktuator + stepper"));
  dbg.println(F("  >export      cetak daftar preset sebagai teks (salin)"));
  dbg.println(F("  >import      baca & terapkan daftar preset dari teks"));
  dbg.println(F("  >rollback    kembalikan preset sebelum import terakhir"));
  dbg.println(F("  >abort       batalkan mode import"));
}

void cmdPrintStatus() {
  dbg.println(F("=== STATUS ==="));
  dbg.print(F("halaman  : ")); dbg.println(namaHalaman[halaman]);
  dbg.print(F("preset   : ")); dbg.println(daftarPreset[presetTerpilih].nama);

  if (prosesJalan) {
    dbg.print(F("proses   : "));
    dbg.println(namaFase[prosesFase]);
    if (prosesJeda) dbg.println(F("           (JEDA)"));
    dbg.print(F("  waktu  : ")); dbg.print(prosesAkumMs / 1000); dbg.print(F(" dtk"));
    dbg.print(F(" / "));         dbg.println(prosesTotalDetik());
    dbg.print(F("  pompa  : "));
    dbg.print(pompaJalan[0] ? "HCl " : "-- ");
    dbg.print(pompaJalan[1] ? "H2O2 " : "-- ");
    dbg.println(pompaJalan[2] ? "H2O" : "--");
    // Total ml terpompa dipakai untuk memverifikasi akumulator tanpa harus
    // menunggu layar OLED.
    dbg.print(F("  ml     : HCl "));
    dbg.print(pompaTerpumpa(0));
    dbg.print(F("  H2O2 "));
    dbg.print(pompaTerpumpa(1));
    dbg.print(F("  H2O "));
    dbg.println(pompaTerpumpa(2));
  } else {
    dbg.println(F("proses   : tidak berjalan"));
  }

  dbg.println(F("--- level tangki ---"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(F("  "));
    dbg.print(namaSensor[i]);
    while (strlen(namaSensor[i]) + 1 < 8) dbg.print(' ');
    dbg.print(sensorPersen[i]);
    dbg.print(F("% "));
    if (sensorMati[i])           dbg.println(F("[MATI]"));
    else if (sensorAdaCairan(i))  dbg.println(F("[ISI]"));
    else                         dbg.println(F("[KOSONG]"));
  }

  dbg.print(F("jumlah   : "));
  dbg.print(jumlahPreset);
  dbg.println(F(" buah preset"));

  // Fail-safe I2C. OLED dan PCF8574 satu bus, jadi kalau keduanya hilang
  // `i2cGagalBerturut` naik dan seluruh tombol mati. E-stop tetap berfungsi
  // karena pin-nya GPIO langsung. Peringatan TIDAK bisa tampil di OLED (itulah
  // yang hilang), jadi dilaporkan lewat Serial + LED strip.
  dbg.print(F("i2c      : "));
  if (i2cGagalBerturut > 0) {
    dbg.print(F("GAGAL "));
    dbg.print(i2cGagalBerturut);
    dbg.print(F("x berturut, recovery "));
    dbg.print(i2cJumlahRecovery);
    dbg.println(F("x - tombol & OLED kemungkinan mati"));
  } else {
    dbg.println(F("OK"));
  }

  dbg.print(F("wdt      : "));
  dbg.println(wdtAktif ? F("aktif") : F("tidak aktif"));
  dbg.print(F("alarm    : "));
  if (prosesError == ERR_NONE) {
    dbg.println(estopAktif ? F("E-stop terkunci") : F("tidak"));
  } else {
    dbg.println(namaError[prosesError]);
  }
}

// ---------- Backup / restore preset (v0.7.1) ----------
//
// Format blok (teks, human-readable, bisa di-paste ke file .txt):
//
//   #AE1
//   1|Preset 1|100|80|33|17|50|120|60|180|5|3|270
//   2|Preset 2|80|60|40|20|40|100|50|100|8|4|330
//   #OK 7A3F2C91
//
// `#AE1` = header ("A" "E" + versi format). Satu baris per preset:
// indeks | nama | 11 angka int sesuai urutan field di struct Preset.
// `#OK <crc32>` = footer; CRC32 FNV-1a atas seluruh karakter baris data
// (tanpa newline). Tujuannya menangkap paste terpotong / karakter rusak —
// kalau tidak cocok, import dibatalkan dan preset lama tidak disentuh.
//
// Separator '|' aman: nama preset tidak bisa diketik bebas (hanya 4 tombol),
// dan `namaAman()` di bawah tetap menyaring '|' serta karakter kontrol
// kalau suatu saat nama berasal dari sumber lain.
#define FORMAT_AE        "AE1"
#define IMPORT_LINE_MAX  80      // baris data ~50 B, 80 memberi margin
#define PRESET_FIELD_N   13      // 1 indeks + 1 nama + 11 angka

// ---------- Ekspor ----------

// Nama preset -> bentuk aman untuk field teks: '|' dan karakter kontrol (<32)
// diganti spasi, lalu dipotong ke 11 char (room untuk '\0').
void namaAman(const char* asal, char* keluar, size_t n) {
  size_t i = 0;
  for (; asal[i] && i < n - 1; i++) {
    unsigned char c = (unsigned char)asal[i];
    keluar[i] = (c == '|' || c < 32) ? ' ' : (char)c;
  }
  keluar[i] = '\0';
}

// Cetak 8 digit hex uppercase. `DbgPort` tidak punya overload print(v, HEX),
// jadi konversi digitnya dilakukan manual.
void printHex8(uint32_t v) {
  for (int s = 28; s >= 0; s -= 4) {
    uint8_t d = (v >> s) & 0xF;
    dbg.print((char)(d < 10 ? ('0' + d) : ('A' + d - 10)));
  }
}

uint32_t crc32Fnv(unsigned char c, uint32_t h) {
  h ^= c;
  return h * 16777619UL;
}

void presetExport() {
  dbg.println(F("--- MULAI SALIN (sampai #OK) ---"));
  dbg.print(F("#"));
  dbg.println(F(FORMAT_AE));

  uint32_t crc = 2166136261UL;      // seed sama dengan storage.h:hitungCrc()
  for (int i = 0; i < jumlahPreset; i++) {
    Preset& p = daftarPreset[i];
    char nama[12];
    namaAman(p.nama, nama, sizeof(nama));

    char baris[IMPORT_LINE_MAX];
    snprintf(baris, sizeof(baris), "%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",
             i + 1, nama,
             p.pcbPanjang, p.pcbLebar, p.persenHCl, p.persenH2O2, p.persenH2O,
             p.mlHCl, p.mlH2O2, p.mlH2O,
             p.agitasiDetik, p.agitasiSpeed, p.totalDetik);
    dbg.println(baris);

    for (const char* q = baris; *q; q++) crc = crc32Fnv((unsigned char)*q, crc);
  }

  dbg.print(F("#OK "));
  printHex8(crc);
  dbg.println();
  dbg.println(F("--- SELESAI SALIN ---"));
}

// ---------- Impor ----------

bool     impAktif     = false;
bool     impHeaderOK  = false;
bool     impPanjang   = false;   // ada baris yang melebihi IMPORT_LINE_MAX
Preset   impPreset[MAKS_PRESET];
int      impJumlah    = 0;
uint32_t impCrc       = 2166136261UL;
char     impBuf[IMPORT_LINE_MAX];
uint8_t  impLen       = 0;

void impSelesai(const char* pesan) {
  impAktif    = false;
  impHeaderOK = false;
  impPanjang  = false;
  impJumlah   = 0;
  impLen      = 0;
  impCrc      = 2166136261UL;
  dbg.print(F("[IMPORT] Dibatalkan: "));
  dbg.println(pesan);
}

// Pisahkan baris di '|' jadi daftar pointer field. Field yang dimodifikasi
// di tempat (tulisan '\0'), jadi argumen harus writable.
int pecahField(char* s, char** field, int maks) {
  int n = 0;
  field[n++] = s;
  for (char* q = s; *q; q++) {
    if (*q == '|') {
      *q = '\0';
      if (n < maks) field[n++] = q + 1;
      else return -1;              // field berlebih -> format salah
    }
  }
  return n;
}

// Parse satu baris data preset ke impPreset[impJumlah].
// Return false + isi `errAlasan` kalau baris tidak valid.
bool parseBarisPreset(char* baris, char* errAlasan, size_t nErr) {
  char* field[PRESET_FIELD_N];
  int nf = pecahField(baris, field, PRESET_FIELD_N);
  if (nf != PRESET_FIELD_N) {
    snprintf(errAlasan, nErr, "baris punya %d field, harus %d", nf, PRESET_FIELD_N);
    return false;
  }

  // Indeks harus berurutan mulai 1 supaya preset tidak bisa hilang diam-diam
  // (mis. import 1,2,4 lalu ada baris 3 yang terlewat).
  int idx = atoi(field[0]);
  if (idx != impJumlah + 1) {
    snprintf(errAlasan, nErr, "indeks %d tidak berurutan (harusnya %d)",
             idx, impJumlah + 1);
    return false;
  }

  Preset p;
  memset(&p, 0, sizeof(p));
  namaAman(field[1], p.nama, sizeof(p.nama));
  if (p.nama[0] == '\0') {
    snprintf(errAlasan, nErr, "nama preset kosong");
    return false;
  }

  int* target[11] = {
    &p.pcbPanjang, &p.pcbLebar, &p.persenHCl, &p.persenH2O2, &p.persenH2O,
    &p.mlHCl, &p.mlH2O2, &p.mlH2O,
    &p.agitasiDetik, &p.agitasiSpeed, &p.totalDetik
  };
  for (int i = 0; i < 11; i++) {
    char* akhir = nullptr;
    long v = strtol(field[2 + i], &akhir, 10);
    // strtol mengembalikan 0 dengan akhir==awal kalau tidak ada digit, atau
    // dengan karakter sisanya bukan angka. Tolak keduanya supaya data korup tidak
    // diam-diam jadi 0 dan lolos validasiPreset().
    if (akhir == field[2 + i] || *akhir != '\0') {
      snprintf(errAlasan, nErr, "field %d bukan angka: '%s'", i + 3, field[2 + i]);
      return false;
    }
    *target[i] = (int)v;
  }

  if (!validasiPreset(p)) {
    snprintf(errAlasan, nErr, "preset #%d gagal validasi rentang", idx);
    return false;
  }

  // Nama harus unik — UI mengandalkan indeks, tapi preset bernama sama bikin
  // hasil `>export` membingungkan saat dibaca manusia.
  for (int i = 0; i < impJumlah; i++) {
    if (strcasecmp(impPreset[i].nama, p.nama) == 0) {
      snprintf(errAlasan, nErr, "nama '%s' duplikat", p.nama);
      return false;
    }
  }

  impPreset[impJumlah++] = p;
  return true;
}

void presetTerapkan() {
  // Snapshot DULU — kalau `simpanPreset()` nanti gagal, preset lama masih ada
  // di slot "prev" dan bisa dikembalikan dengan >rollback.
  if (!simpanSnapshot()) {
    impSelesai("gagal menyimpan snapshot, batal");
    return;
  }

  jumlahPreset = impJumlah;
  for (int i = 0; i < jumlahPreset; i++) daftarPreset[i] = impPreset[i];

  // presetTerpilih boleh menunjuk indeks yang tidak ada lagi.
  if (presetTerpilih >= jumlahPreset) presetTerpilih = jumlahPreset - 1;
  if (presetTerpilih < 0) presetTerpilih = 0;

  if (simpanPreset()) {
    tampilToast("Preset diimpor");
    dbg.print(F("[IMPORT] OK: "));
    dbg.print(jumlahPreset);
    dbg.println(F(" preset terpasang"));
  } else {
    tampilToast("Gagal simpan");
    dbg.println(F("[IMPORT] Data valid tapi GAGAL ditulis ke NVS"));
  }
}

void presetImportBaris(char* baris) {
  if (impPanjang) { impSelesai("ada baris terlalu panjang (paste terpotong?)"); return; }

  if (baris[0] == '#') {
    // Header
    if (strncmp(baris, "#" FORMAT_AE, 3) == 0) {
      if (baris[3] == '\0') {
        impHeaderOK = true;
        dbg.println(F("[IMPORT] Header OK, kirim baris preset..."));
        return;
      }
      impSelesai("versi format tidak didukung (perlu #AE1)");
      return;
    }

    // Footer "#OK <crc32>"
    unsigned long c = 0;
    if (sscanf(baris, "#OK %lx", &c) != 1) {
      impSelesai("footer rusak, harus \"#OK <crc32>\"");
      return;
    }
    if (!impHeaderOK)           { impSelesai("footer arrived sebelum header #AE1"); return; }
    if (impJumlah < 1)           { impSelesai("tidak ada baris preset"); return; }
    if ((uint32_t)c != impCrc) {
      dbg.print(F("[IMPORT] CRC tidak cocok: terima "));
      printHex8((uint32_t)c);
      dbg.print(F(" <> hitung "));
      printHex8(impCrc);
      dbg.println();
      impSelesai("data rusak / paste terpotong");
      return;
    }

    presetTerapkan();
    return;
  }

  // Baris biasa hanya sah setelah header diterima — supaya log `dbg.h` atau
  // teks acak yang tidak sengaja ikut ter-paste tidak langsung jadi preset.
  if (!impHeaderOK) {
    impSelesai("data sebelum header #AE1 — kirim ulang blok penuh");
    return;
  }
  if (impJumlah >= MAKS_PRESET) { impSelesai("jumlah preset melebihi batas"); return; }

  char err[64];
  if (!parseBarisPreset(baris, err, sizeof(err))) { impSelesai(err); return; }

  for (const char* q = baris; *q; q++) impCrc = crc32Fnv((unsigned char)*q, impCrc);
  dbg.print(F("[IMPORT] "));
  dbg.print(impJumlah);
  dbg.print(F("/"));
  dbg.print(MAKS_PRESET);
  dbg.print(F(" "));
  dbg.println(impPreset[impJumlah - 1].nama);
}

void presetImportMulai() {
  // Mengubah daftar preset di tengah proses akan membuat satu siklus etching
  // mencampur dua preset berbeda. Tolak keras, jangan dipaksa.
  if (prosesJalan) {
    dbg.println(F("[IMPORT] Proses sedang jalan — >stop dulu"));
    return;
  }
  impAktif    = true;
  impHeaderOK = false;
  impPanjang  = false;
  impJumlah   = 0;
  impCrc      = 2166136261UL;
  dbg.println(F("[IMPORT] Paste blok >export (header #AE1 sampai #OK)."));
  dbg.println(F("[IMPORT] Ketik >abort atau perintah lain untuk membatalkan."));
}

void presetRollback() {
  if (prosesJalan) {
    dbg.println(F("[ROLLBACK] Proses sedang jalan — >stop dulu"));
    return;
  }
  Preset tmp[MAKS_PRESET];
  int n = 0;
  if (!muatSnapshot(tmp, &n)) {
    dbg.println(F("[ROLLBACK] Tidak ada snapshot valid"));
    return;
  }
  for (int i = 0; i < n; i++) daftarPreset[i] = tmp[i];
  jumlahPreset = n;
  if (presetTerpilih >= jumlahPreset) presetTerpilih = jumlahPreset - 1;
  if (presetTerpilih < 0) presetTerpilih = 0;

  if (simpanPreset()) {
    tampilToast("Preset dipulihkan");
    dbg.print(F("[ROLLBACK] OK: "));
    dbg.print(n);
    dbg.println(F(" preset dipulihkan"));
  } else {
    tampilToast("Gagal simpan");
    dbg.println(F("[ROLLBACK] GAGAL menulis ke NVS"));
  }
}

// ---------- Eksekusi perintah ----------

void cmdSelHalaman(const char* hal) {
  if      (strcasecmp(hal, "menu")   == 0) pindahHalaman(HAL_MENU_UTAMA);
  else if (strcasecmp(hal, "preset") == 0) pindahHalaman(HAL_PRESET_LIST);
  else if (strcasecmp(hal, "cfg")    == 0) pindahHalaman(HAL_CFG_LIST);
  else if (strcasecmp(hal, "level")  == 0) { levelMasuk(); pindahHalaman(HAL_LEVEL); }
  else if (strcasecmp(hal, "kalib")  == 0) pindahHalaman(HAL_KALIB_MAIN);
  else dbg.println(F("halaman tidak dikenal: menu|preset|cfg|level|kalib"));
}

void cmdJalankan(char* baris) {
  // buang spasi di depan & belakang
  while (*baris == ' ') baris++;
  char* akhir = baris + strlen(baris);
  while (akhir > baris && akhir[-1] == ' ') *--akhir = '\0';
  if (*baris == '\0') return;

  // salin ke cmdBuf supaya bisa dibandingkan case-insensitive
  strncpy(cmdBuf, baris, CMD_MAX_LEN - 1);
  cmdBuf[CMD_MAX_LEN - 1] = '\0';

  // pisahkan argumen pertama (kata kedua)
  char* arg = strchr(cmdBuf, ' ');
  if (arg) *arg++ = '\0';

  if (cmdCocok("help")) { cmdPrintHelp(); return; }
  if (cmdCocok("status")) { cmdPrintStatus(); return; }

  if (cmdCocok("start")) {
    if (prosesJalan) { dbg.println(F("proses sudah berjalan")); return; }
    prosesMulai();
    // `prosesMulai()` menolak kalau alarm aktif / E-stop belum direset — jangan
    // Jangan pindah halaman kalau memang tidak jadi mulai.
    if (!prosesJalan) { dbg.println(F("[CMD] Start ditolak (lihat pesan alarm)")); return; }
    pindahHalaman(HAL_PROSES);
    dbg.print(F("[CMD] Mulai: "));
    dbg.println(daftarPreset[presetTerpilih].nama);
    return;
  }

  if (cmdCocok("stop")) {
    if (!prosesJalan) { dbg.println(F("proses tidak berjalan")); return; }
    prosesBatal();
    dbg.println(F("[CMD] Proses dibatalkan"));
    return;
  }

  if (cmdCocok("preset")) {
    if (!arg) { dbg.println(F("syntax: >preset N")); return; }
    int n = atoi(arg);
    if (n < 1 || n > jumlahPreset) {
      dbg.print(F("[CMD] Preset tidak ada (1.."));
      dbg.print(jumlahPreset);
      dbg.println(F(")"));
      return;
    }
    presetTerpilih = n - 1;
    dbg.print(F("[CMD] Preset: "));
    dbg.println(daftarPreset[presetTerpilih].nama);
    return;
  }

  if (cmdCocok("sel")) {
    if (!arg) { dbg.println(F("syntax: >sel menu|preset|cfg|level|kalib")); return; }
    cmdSelHalaman(arg);
    return;
  }

  if (cmdCocok("off")) {
    semuaAktuatorMati();
    dbg.println(F("[CMD] Semua aktuator dimatikan"));
    return;
  }

  if (cmdCocok("export")) { presetExport(); return; }
  if (cmdCocok("import")) { presetImportMulai(); return; }

  if (cmdCocok("rollback")) { presetRollback(); return; }

  if (cmdCocok("abort")) {
    if (impAktif) impSelesai("dibatalkan perintah >abort");
    else          dbg.println(F("[CMD] Tidak sedang import"));
    return;
  }

  dbg.print(F("[CMD] Perintah tidak dikenal: "));
  dbg.println(cmdBuf);
  dbg.println(F("      ketik >help"));
}

// ---------- Parser non-blocking ----------

// Panggil tiap iterasi loop(). Regenerasi baris selesai (Enter) langsung
// dieksekusi; karakter lain hanya ditumpuk.
void serialCmdUpkeep() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    // Sedang import: semua baris milik import, KECUALI yang diawali '>'.
    // '>' jadi jalan keluar — mengetik perintah apa pun membatalkan import lalu
    // perintah itu dijalankan, jadi tidak ada kondisi "buntu di mode import".
    if (impAktif) {
      if (c == '>') {
        impSelesai("dibatalkan perintah baru");
        cmdLen = 0;                 // karakter sisanya = perintah baru
        continue;
      }
      if (c == '\r' || c == '\n') {
        if (impLen > 0) {
          impBuf[impLen] = '\0';
          presetImportBaris(impBuf);
          impLen = 0;
        }
        continue;
      }
      if (impLen < IMPORT_LINE_MAX - 1) impBuf[impLen++] = c;
      else impPanjang = true;       // penanda: baris berikutnya akan menolak
      continue;
    }

    if (c == '>') {
      cmdLen = 0;                 // awal baris perintah baru
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        cmdJalankan(cmdBuf);
        cmdLen = 0;
      }
      continue;
    }
    if (cmdLen < CMD_MAX_LEN - 1) cmdBuf[cmdLen++] = c;
    // kalau buffer penuh, karakter berikutnya dibuang sampai Enter
  }
}