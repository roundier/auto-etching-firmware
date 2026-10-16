#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "dbg.h"

// ================= WATCHDOG HARDWARE (ESP32-S3) =================
// Jaring pengaman: kalau firmware hang (bug, I2C macet, infinite loop) dan
// `loop()` tidak lagi jalan, Task WDT memaksa CPU reset setelah
// `WDT_TIMEOUT_MS`. Tanpa ini, alat hanya bisa dipulihkan dengan mematikan
// listrik secara manual.
//
// Syarat supaya tidak salah-trigger:
//   - timeout harus jauh lebih besar dari siklus loop terburuk. `loop()`
//     memanggil `oled.display()` yang memblokir I2C ~20-30 ms, dan
//     `ds18.requestTemperatures()` non-blocking. Tidak ada `delay()` panjang di
//     jalur proses sejak v0.6.1. Jadi 10 detik sangat lega.
//   - `watchdogFeed()` dipanggil di `loop()`, bukan hanya di `setup()`.
//
// Catatan: Arduino core ESP32-S3 sudah mendaftarkan idle task ke Task WDT saat
// boot. `watchdogInit()` di sini memakai `reconfigure()` kalau sudah terdaftar
// (mengubah timeout yang ada), alih-alih membuat konfigurasi kedua yang
// akan memicu error ESP_ERR_INVALID_STATE.

#define WDT_TIMEOUT_MS   10000U   // 10 detik

// true kalau Task WDT aktif & di-reset tiap loop
bool wdtAktif = false;

// Reset ulang tiap 1 detik. `loop()` dipanggil jauh lebih cepat, jadi
// mengreset tiap iterasi hanya menambah overhead tanpa manfaat.
unsigned long wdtMsTerakhir = 0;

void watchdogInit() {
  esp_task_wdt_config_t cfg = {
    .timeout_ms     = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,      // 0 = semua core (ESP32-S3 bisa idle di core mana pun)
    .trigger_panic  = true     // reset paksa saat timeout
  };

  // `init()` akan kena ESP_ERR_INVALID_STATE kalau core Arduino sudah
  // mendaftarkan task ini saat boot. Dalam kasus itu cukup `reconfigure()`
  // timeout yang sudah ada — tidak perlu konfigurasi kedua.
  esp_err_t err = esp_task_wdt_init(&cfg);
  if (err == ESP_OK) {
    err = esp_task_wdt_add(NULL);
    if (err == ESP_OK) {
      wdtAktif = true;
      dbg.print(F("[WDT] Aktif, timeout "));
      dbg.print(WDT_TIMEOUT_MS / 1000);
      dbg.println(F(" detik"));
      return;
    }
  } else if (err == ESP_ERR_INVALID_STATE) {
    if (esp_task_wdt_reconfigure(&cfg) == ESP_OK) {
      wdtAktif = true;
      dbg.print(F("[WDT] Timeout diset ulang: "));
      dbg.print(WDT_TIMEOUT_MS / 1000);
      dbg.println(F(" detik"));
      return;
    }
  }

  // Tidak fatal — alat tetap jalan normal, hanya tanpa jaring pengaman.
  // Sengaja tidak panic supaya debugging di lapangan tidak jadi sulit.
  dbg.println(F("[WDT] GAGAL aktif — lanjut tanpa watchdog"));
  wdtAktif = false;
}

// Panggil tiap iterasi loop(). Aman dipanggil sangat sering.
void watchdogFeed() {
  if (!wdtAktif) return;
  unsigned long now = millis();
  if ((long)(now - wdtMsTerakhir) < 1000) return;
  wdtMsTerakhir = now;
  esp_task_wdt_reset();
}

// Matikan watchdog sementara (untuk operasi yang memang boleh lama, mis. hapus
// flash). Selalu nyalakan lagi setelahnya dengan watchdogNyalakan().
void watchdogMatikan() {
  if (!wdtAktif) return;
  esp_task_wdt_delete(NULL);
  wdtAktif = false;
  dbg.println(F("[WDT] Dimatikan sementara"));
}

void watchdogNyalakan() {
  if (wdtAktif) return;
  esp_task_wdt_config_t cfg = {
    .timeout_ms     = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  if (esp_task_wdt_init(&cfg) == ESP_OK && esp_task_wdt_add(NULL) == ESP_OK) {
    wdtAktif = true;
    wdtMsTerakhir = millis();
    dbg.println(F("[WDT] Aktif kembali"));
  }
}