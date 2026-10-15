/*
  Menu OLED 6 Tombol - v0.5.0
  Perubahan dari v0.4.1:
    - sensor.h: sensor level sekarang baca GPIO langsung (ganti MPR121 I2C)
    - motor.h (baru): driver stepper DRV8825 x3, non-blocking, ml -> langkah
    - dcmotor.h (baru): PWM TB6612FNG x2 (agitasi 15, sabun 16, limbah 17, fan 18)
    - process.h: fase DOSING/ETCHING/DRAINING/RINSING kini menggerakkan aktuator
    - E-stop (GPIO8): hentikan semua aktuator & batalkan proses
  Perubahan dari v0.3:
    - Tombol KIRI/KANAN ditukar (config.h)
    - sensor.h: baca 6 sensor level cairan (MPR121), rata-rata + histeresis
    - Menu "Level Cairan": nilai, ambang, status ISI/KSG per sensor;
      ENTER = set ambang dari nilai sekarang (tangki KOSONG), KIRI/KANAN = fine tune,
      BACK = simpan/buang ambang (NVS)
  Catatan v0.3:
    - storage.h: preset disimpan di NVS (Preferences) + versi + checksum;
      data korup/versi beda -> otomatis kembali ke default
    - presetops.h: tambah / duplikat / hapus / edit preset + validasi
    - Menu "Konfigurasi Preset": List -> Aksi (Edit/Duplikat/Hapus) -> Edit
      Edit: ATAS/BAWAH pilih field, KIRI/KANAN ubah nilai, ENTER simpan
      %H2O dan semua ml dihitung otomatis dari %HCl, %H2O2, dan Volume
  Library: Wire, Adafruit_GFX, Adafruit_SSD1306, Preferences (ESP32)
*/

#include <Wire.h>
#include "dbg.h"
#include "config.h"
#include "input.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "storage.h"
#include "buzzer.h"
#include "process.h"
#include "ui.h"

unsigned long waktuRenderTerakhir = 0;

#ifdef DEBUG_RENDER
unsigned long dbgWaktu = 0;
int dbgJumlahRender = 0;
#endif

// ================= HELPER KONFIGURASI =================
void simpanDanToast(const char* ok) {
  tampilToast(simpanPreset() ? ok : "Gagal simpan!");
}

void editGeserField(int delta) {
  editField = constrain(editField + delta, 0, JUMLAH_FIELD - 1);
  if (editField < editScroll) editScroll = editField;
  if (editField >= editScroll + EDIT_VISIBLE) editScroll = editField - EDIT_VISIBLE + 1;
}

void mulaiEdit() {
  presetEdit = daftarPreset[cfgIdx];
  editBerubah = false;
  editField = 0;
  editScroll = 0;
  dbg.print(F("[CFG] Edit: "));
  dbg.println(presetEdit.nama);
  pindahHalaman(HAL_CFG_EDIT);
}

void simpanEdit() {
  daftarPreset[cfgIdx] = presetEdit;
  editBerubah = false;
}

// ================= NAVIGASI =================
void tanganiTombol(int t) {
  dbg.print(F("[TOMBOL] "));
  dbg.println(namaTombol[t]);
  buzzerKlik();                 // feedback bunyi saat tombol ditekan
  perluGambar = true;

  switch (halaman) {

    case HAL_MENU_UTAMA:
      if (t == P_ATAS)  listGeser(L_UTAMA, -1, JUMLAH_MENU);
      if (t == P_BAWAH) listGeser(L_UTAMA, +1, JUMLAH_MENU);
      if (t == P_ENTER) {
        if (lsSel[L_UTAMA] == 0) {
          listReset(L_PRESET);
          pindahHalaman(HAL_PRESET_LIST);
        } else if (lsSel[L_UTAMA] == 1) {
          listReset(L_CFG);
          pindahHalaman(HAL_CFG_LIST);
        } else if (lsSel[L_UTAMA] == 2) {
          levelMasuk();
          pindahHalaman(HAL_LEVEL);
        } else {
          listReset(L_AKSI);
          pindahHalaman(HAL_KALIB_MAIN);
        }
      }
      break;

    case HAL_PRESET_LIST:
      if (jumlahPreset > 0) {
        if (t == P_ATAS)  listGeser(L_PRESET, -1, jumlahPreset);
        if (t == P_BAWAH) listGeser(L_PRESET, +1, jumlahPreset);
        if (t == P_ENTER) {
          presetTerpilih = lsSel[L_PRESET];
          presetDetailScroll = 0;
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    case HAL_PRESET_DETAIL: {
      int maxScroll = max(0, DETAIL_CONTENT_H - (SCREEN_HEIGHT - HEADER_H));
      if (t == P_ATAS)  presetDetailScroll = max(0, presetDetailScroll - DETAIL_SCROLL_STEP);
      if (t == P_BAWAH) presetDetailScroll = min(maxScroll, presetDetailScroll + DETAIL_SCROLL_STEP);
      if (t == P_ENTER) { prosesMulai(); pindahHalaman(HAL_PROSES); }
      if (t == P_BACK)  pindahHalaman(HAL_PRESET_LIST);
      break;
    }

    case HAL_PROSES:
      if (dialogBatal) {
        if (t == P_ENTER) {
          prosesBatal();
          pindahHalaman(HAL_PRESET_DETAIL);
        } else if (t == P_BACK) {
          dialogBatal = false;
          dbg.println(F("[PROSES] Batal dibatalkan (lanjut)"));
        }
      } else if (prosesSelesai()) {
        if (t == P_ENTER || t == P_BACK) {
          prosesKeluarSelesai();
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else {
        if (t == P_ENTER) prosesToggleJeda();
        if (t == P_BACK) {
          dialogBatal = true;
          dbg.println(F("[PROSES] Konfirmasi batal ditampilkan"));
        }
      }
      break;

    case HAL_PLACEHOLDER:
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Level Cairan ----------
    case HAL_LEVEL:
      if (t == P_ATAS) {
        levelScroll = max(0, levelScroll - LEVEL_SCROLL_STEP);
        dbg.println(F("[LEVEL] Scroll up"));
      }
      if (t == P_BAWAH) {
        levelScroll += LEVEL_SCROLL_STEP;
        dbg.println(F("[LEVEL] Scroll down"));
      }
      if (t == P_BACK) {
        levelScroll = 0;
        pindahHalaman(HAL_MENU_UTAMA);
      }
      break;

    // ---------- Kalibrasi Main ----------
    case HAL_KALIB_MAIN:
      if (t == P_ATAS)  listGeser(L_AKSI, -1, JUMLAH_MENU_KALIB);
      if (t == P_BAWAH) listGeser(L_AKSI, +1, JUMLAH_MENU_KALIB);
      if (t == P_ENTER) {
        int a = lsSel[L_AKSI];
        if (a == 0) {
          // Motor
          listReset(L_AKSI);
          kalibMotorIdx = 0;
          pindahHalaman(HAL_KALIB_MOTOR_LIST);
        } else if (a == 1) {
          // Sensor Level
          kalibSensorIdx = 0;
          kalibSetStep = 0;
          pindahHalaman(HAL_KALIB_LEVEL);
        } else if (a == 2) {
          // Sensor Suhu
          kalibSuhuIdx = 0;
          kalibSuhuScroll = 0;
          kalibSuhuField = 0;
          pindahHalaman(HAL_KALIB_SUHU);
        } else if (a == 3) {
          // Sensor Udara (MQ-135)
          kalibUdaraSel = 0;
          pindahHalaman(HAL_KALIB_UDARA);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Kalibrasi Motor: Pilih Motor ----------
    case HAL_KALIB_MOTOR_LIST:
      if (t == P_ATAS)  listGeser(L_AKSI, -1, JUMLAH_MENU_MOTOR);
      if (t == P_BAWAH) listGeser(L_AKSI, +1, JUMLAH_MENU_MOTOR);
      if (t == P_ENTER) {
        kalibMotorIdx = lsSel[L_AKSI];
        kalibStepperCursor = 0;
        kalibDCCursor = 0;
        if (kalibMotorIdx <= 2) {
          // Pompa HCl, H2O2, Air (stepper)
          pindahHalaman(HAL_KALIB_STEPPER);
        } else if (kalibMotorIdx <= 4) {
          // Pompa Sabun, Limbah (DC)
          kalibDCScroll = 0;
          pindahHalaman(HAL_KALIB_DC);
        } else if (kalibMotorIdx == 5) {
          // Motor Agitasi
          pindahHalaman(HAL_KALIB_AGITASI);
        } else {
          // Kipas Exhaust
          pindahHalaman(HAL_KALIB_FAN);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_KALIB_MAIN);
      break;

    // ---------- Kalibrasi Stepper (Pompa HCl/H2O2/Air) ----------
    case HAL_KALIB_STEPPER:
      if (t == P_BACK) { pindahHalaman(HAL_KALIB_MOTOR_LIST); break; }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibStepperCursor = 1 - kalibStepperCursor;
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        if (kalibStepperCursor == 1) {
          float delta = (t == P_KANAN) ? 0.1 : -0.1;
          kalibStepperMl[kalibMotorIdx] += delta;
          if (kalibStepperMl[kalibMotorIdx] < 0.1) kalibStepperMl[kalibMotorIdx] = 0.1;
          if (kalibStepperMl[kalibMotorIdx] > 10.0) kalibStepperMl[kalibMotorIdx] = 10.0;
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibStepperCursor == 0) {
          pompaMulai(kalibMotorIdx, 1);
          tampilToast("Running...");
        }
      }
      break;

    // ---------- Kalibrasi DC (Pompa Sabun/Limbah) ----------
    case HAL_KALIB_DC:
      if (t == P_BACK) { kalibDCScroll = 0; pindahHalaman(HAL_KALIB_MOTOR_LIST); break; }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibDCCursor = (kalibDCCursor + 1) % 3;
        // Auto-scroll: pastikan baris terpilih sepenuhnya terlihat
        {
          static const int rowTop[3]  = { 0, 16, 28 };
          static const int rowH[3]    = { 12, 10, 10 };
          int maxScroll = max(0, KALIB_DC_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          if (rowTop[kalibDCCursor] - kalibDCScroll + rowH[kalibDCCursor] > visibleH)
            kalibDCScroll = rowTop[kalibDCCursor] + rowH[kalibDCCursor] - visibleH;
          if (rowTop[kalibDCCursor] - kalibDCScroll < 0)
            kalibDCScroll = rowTop[kalibDCCursor];
          if (kalibDCScroll > maxScroll) kalibDCScroll = maxScroll;
          if (kalibDCScroll < 0) kalibDCScroll = 0;
        }
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int idx = kalibMotorIdx - 3;
        if (kalibDCCursor == 1) {
          int delta = (t == P_KANAN) ? 5 : -5;
          kalibDuty[idx] += delta;
          if (kalibDuty[idx] < 0) kalibDuty[idx] = 0;
          if (kalibDuty[idx] > 255) kalibDuty[idx] = 255;
        } else if (kalibDCCursor == 2) {
          int delta = (t == P_KANAN) ? 1 : -1;
          kalibMl[idx] += delta;
          if (kalibMl[idx] < 0) kalibMl[idx] = 0;
          if (kalibMl[idx] > 999) kalibMl[idx] = 999;
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibDCCursor == 0) {
          if (kalibMotorIdx == 3) pompaSabunSet(true);
          else pompaLimbahSet(true);
          kalibMotorRunning = true;
          kalibMotorStartMs = millis();
          tampilToast("Running...");
        }
      }
      break;

    // ---------- Kalibrasi Agitasi ----------
    case HAL_KALIB_AGITASI:
      if (t == P_BACK) { pindahHalaman(HAL_KALIB_MOTOR_LIST); break; }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibDCCursor = (kalibDCCursor + 1) % 4;
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int delta = (t == P_KANAN) ? 5 : -5;
        if (kalibDCCursor < 3) {
          kalibDuty[kalibDCCursor + 2] = constrain(kalibDuty[kalibDCCursor + 2] + delta, 0, 255);
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibDCCursor == 3) {
          // Test all speeds
          for (int s = 1; s <= 3; s++) {
            agitasiSet(s);
            delay(1000);
          }
          agitasiSet(0);
          tampilToast("Done");
        } else {
          agitasiSet(kalibDCCursor + 1);
          delay(2000);
          agitasiSet(0);
          tampilToast("Done");
        }
      }
      break;

    // ---------- Kalibrasi Fan ----------
    case HAL_KALIB_FAN:
      if (t == P_BACK) { pindahHalaman(HAL_KALIB_MOTOR_LIST); break; }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibDCCursor = (kalibDCCursor + 1) % 2;
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        if (kalibDCCursor == 0) {
          int delta = (t == P_KANAN) ? 5 : -5;
          kalibDuty[6] = constrain(kalibDuty[6] + delta, 0, 255);
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibDCCursor == 1) {
          fanSet(true);
          delay(2000);
          fanSet(false);
          tampilToast("Done");
        }
      }
      break;

// ---------- Kalibrasi Sensor Suhu ----------
    case HAL_KALIB_SUHU:
      if (t == P_BACK) { kalibSuhuScroll = 0; pindahHalaman(HAL_KALIB_MAIN); break; }
      if (t == P_ATAS)  kalibSuhuIdx = max(0, kalibSuhuIdx - 1);
      if (t == P_BAWAH) kalibSuhuIdx = min(JUMLAH_SUHU - 1, kalibSuhuIdx + 1);
      if (t == P_ENTER) kalibSuhuField = 1 - kalibSuhuField;   // pindah field aktif
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (kalibSuhuField == 0) {
          // Field addr: pilih sensor 1..3
          kalibSuhuAddr[kalibSuhuIdx] = constrain(kalibSuhuAddr[kalibSuhuIdx] + arah, 1, 3);
          dbg.print(F("[KALIB-SUHU] "));
          dbg.print(namaSensorSuhu[kalibSuhuIdx]);
          dbg.print(F(" addr="));
          dbg.println(kalibSuhuAddr[kalibSuhuIdx]);
        } else {
          // Field suhu: atur offset kalibrasi ±0.1 °C
          suhuOffsetX10[kalibSuhuIdx] += arah * 1.0f;
          if (suhuOffsetX10[kalibSuhuIdx] >  200.0f) suhuOffsetX10[kalibSuhuIdx] =  200.0f;
          if (suhuOffsetX10[kalibSuhuIdx] < -200.0f) suhuOffsetX10[kalibSuhuIdx] = -200.0f;
          dbg.print(F("[KALIB-SUHU] "));
          dbg.print(namaSensorSuhu[kalibSuhuIdx]);
          dbg.print(F(" offset="));
          dbg.println(suhuOffsetX10[kalibSuhuIdx] / 10.0f);
        }
        break;
      }
      // Auto-scroll: pastikan blok terpilih terlihat
      {
        int maxScroll = max(0, SUHU_CONTENT_H - (AREA_BAWAH - HEADER_H - 3));
        int visibleH  = AREA_BAWAH - (HEADER_H + 3);
        if (SUHU_ROW_TOP[kalibSuhuIdx] - kalibSuhuScroll + SUHU_ROW_H > visibleH)
          kalibSuhuScroll = SUHU_ROW_TOP[kalibSuhuIdx] + SUHU_ROW_H - visibleH;
        if (SUHU_ROW_TOP[kalibSuhuIdx] - kalibSuhuScroll < 0)
          kalibSuhuScroll = SUHU_ROW_TOP[kalibSuhuIdx];
        if (kalibSuhuScroll > maxScroll) kalibSuhuScroll = maxScroll;
        if (kalibSuhuScroll < 0) kalibSuhuScroll = 0;
      }
      break;

    // ---------- Kalibrasi Sensor Udara (MQ-135) ----------
    case HAL_KALIB_UDARA:
      if (t == P_BACK) { pindahHalaman(HAL_KALIB_MAIN); break; }
      if (t == P_ATAS || t == P_BAWAH) kalibUdaraSel = 1 - kalibUdaraSel;
      if (t == P_ENTER) {
        // Rekam bacaan saat ini (ppm) sebagai acuan baris terpilih
        mq135Bacaan = sensorUdaraRaw();
        int ppm = (int)mq135PPM();
        if (kalibUdaraSel == 0) {
          mq135Normal = ppm;
          tampilToast("Normal direkam");
        } else {
          mq135Kotor = ppm;
          tampilToast("Kotor direkam");
        }
        dbg.print(F("[KALIB-MQ135] "));
        dbg.print(kalibUdaraSel == 0 ? F("normal") : F("kotor"));
        dbg.print(F(" adc="));
        dbg.print(mq135Bacaan);
        dbg.print(F(" ppm="));
        dbg.println(ppm);
      }
      break;

    // ---------- Kalibrasi Sensor Level: Pilih Cairan ----------
    case HAL_KALIB_LEVEL: {
      const int ROW_H = 10;
      const int VISIBLE = (AREA_BAWAH - HEADER_H) / ROW_H;
      int maxScroll = max(0, JUMLAH_SENSOR - VISIBLE);
      if (t == P_ATAS) {
        kalibSensorIdx = max(0, kalibSensorIdx - 1);
        if (kalibSensorIdx < kalibLevelScroll) kalibLevelScroll = kalibSensorIdx;
      }
      if (t == P_BAWAH) {
        kalibSensorIdx = min(JUMLAH_SENSOR - 1, kalibSensorIdx + 1);
        if (kalibSensorIdx >= kalibLevelScroll + VISIBLE) kalibLevelScroll = kalibSensorIdx - VISIBLE + 1;
      }
      if (t == P_ENTER) {
        kalibSetStep = 0;
        pindahHalaman(HAL_KALIB_LEVEL_SET);
      }
      if (t == P_BACK) {
        kalibLevelScroll = 0;
        pindahHalaman(HAL_KALIB_MAIN);
      }
      break;
    }

    // ---------- Kalibrasi Sensor Level: Set 0% / 100% ----------
    case HAL_KALIB_LEVEL_SET:
      if (t == P_BACK) {
        pindahHalaman(HAL_KALIB_LEVEL);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibSetStep = 1 - kalibSetStep;
        break;
      }
      if (t == P_ENTER) {
        if (kalibSetStep == 0) {
          sensorSetKosong(kalibSensorIdx);
          dbg.print(F("[KALIB] 0% tersimpan: "));
          dbg.println(sensorKalibKosong[kalibSensorIdx]);
          tampilToast("0% tersimpan");
        } else {
          sensorSetPenuh(kalibSensorIdx);
          simpanKalib();
          dbg.print(F("[KALIB] 100% tersimpan: "));
          dbg.println(sensorKalibPenuh[kalibSensorIdx]);
          tampilToast("100% tersimpan");
          pindahHalaman(HAL_KALIB_LEVEL);
        }
      }
      break;

    // ---------- Konfigurasi: daftar preset + "+ Tambah" ----------
    case HAL_CFG_LIST: {
      int n = jumlahItemCfg();
      if (t == P_ATAS)  listGeser(L_CFG, -1, n);
      if (t == P_BAWAH) listGeser(L_CFG, +1, n);
      if (t == P_ENTER) {
        int idx = lsSel[L_CFG];
        if (idx < jumlahPreset) {
          cfgIdx = idx;
          listReset(L_AKSI);
          pindahHalaman(HAL_CFG_AKSI);
        } else if (tambahPreset()) {
          dbg.print(F("[CFG] Tambah: "));
          dbg.println(daftarPreset[jumlahPreset - 1].nama);
          listSet(L_CFG, jumlahPreset - 1);
          simpanDanToast("Preset ditambah");
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;
    }

    // ---------- Konfigurasi: Edit / Duplikat / Hapus ----------
    case HAL_CFG_AKSI:
      if (dialogCfg == DLG_HAPUS) {
        if (t == P_ENTER) {
          dbg.print(F("[CFG] Hapus: "));
          dbg.println(daftarPreset[cfgIdx].nama);
          hapusPreset(cfgIdx);
          dialogCfg = DLG_NONE;
          listSet(L_CFG, min(cfgIdx, jumlahItemCfg() - 1));
          pindahHalaman(HAL_CFG_LIST);
          simpanDanToast("Preset dihapus");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
        }
      } else {
        if (t == P_ATAS)  listGeser(L_AKSI, -1, 3);
        if (t == P_BAWAH) listGeser(L_AKSI, +1, 3);
        if (t == P_ENTER) {
          int a = lsSel[L_AKSI];
          if (a == 0) {
            mulaiEdit();
          } else if (a == 1) {
            if (duplikatPreset(cfgIdx)) {
              dbg.print(F("[CFG] Duplikat -> "));
              dbg.println(daftarPreset[jumlahPreset - 1].nama);
              listSet(L_CFG, jumlahPreset - 1);
              pindahHalaman(HAL_CFG_LIST);
              simpanDanToast("Preset diduplikat");
            } else {
              tampilToast("Penuh (maks 10)");
            }
          } else {
            dialogCfg = DLG_HAPUS;
          }
        }
        if (t == P_BACK) pindahHalaman(HAL_CFG_LIST);
      }
      break;

    // ---------- Konfigurasi: Edit field ----------
    case HAL_CFG_EDIT:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          simpanEdit();
          dialogCfg = DLG_NONE;
          pindahHalaman(HAL_CFG_AKSI);
          simpanDanToast("Tersimpan");
        } else if (t == P_BACK) {
          dbg.println(F("[CFG] Perubahan dibuang"));
          dialogCfg = DLG_NONE;
          editBerubah = false;
          pindahHalaman(HAL_CFG_AKSI);
        }
      } else {
        if (t == P_ATAS)  editGeserField(-1);
        if (t == P_BAWAH) editGeserField(+1);
        if (t == P_KIRI || t == P_KANAN) {
          Preset sebelum = presetEdit;
          ubahField(presetEdit, editField, (t == P_KANAN) ? +1 : -1);
          if (memcmp(&sebelum, &presetEdit, sizeof(Preset)) != 0) editBerubah = true;
        }
        if (t == P_ENTER) {
          simpanEdit();
          simpanDanToast("Tersimpan");
        }
        if (t == P_BACK) {
          if (editBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_CFG_AKSI);
        }
      }
      break;
  }
}

// ================= SETUP =================
void setup() {
  dbgInit();
  delay(300);
  dbg.print(F("[BOOT] Menu_OLED_6Button "));
  dbg.println(F(FW_VERSI));

  muatPreset();
  muatKalib();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);

  if (!tulisPCF8574(0xFF))
    dbg.println(F("!! PCF8574 tidak terdeteksi. Cek wiring/alamat I2C."));
  inputInit();

  if (!sensorInit())
    dbg.println(F("!! Sensor level tidak siap. Cek wiring GPIO."));
  sensorSuhuInit();                      // scan address DS18B20 di GPIO10

  motorInit();
  dcmotorInit();
  buzzerInit();
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_MQ135, INPUT);                 // sensor udara MQ-135
  pinMode(PIN_DS18B20, INPUT);               // sensor suhu (1-Wire, placeholder)

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    dbg.println(F("!! OLED tidak terdeteksi di 0x3C. Cek wiring/alamat I2C."));
  } else {
    oled.setRotation(0);
    gambarLayar();
    perluGambar = false;
  }

  // Port USB baru terhubung setelah host membuka monitor; cetak ulang banner
  // agar pesan boot tidak hilang.
  delay(2000);
  dbgInit();
  dbg.print(F("[BOOT] Menu_OLED_6Button "));
  dbg.println(F(FW_VERSI));

  // Nada boot diputar di akhir setup(): kalau dipanggil lebih awal, melodi
  // akan tertahan oleh delay() di atas (buzzerUpdate baru jalan di loop).
  buzzerBoot();
}

// ================= LOOP =================
void loop() {
  int t = bacaTombolBaru();
  if (t != TIDAK_ADA) tanganiTombol(t);

  prosesUpdate();                            // logika proses, tidak tergantung render
  motorUpdate();                             // stepper non-blocking
  sensorUpdate();                            // baca sensor level cairan
  buzzerUpdate();                            // pola nada non-blocking

  // Konversi DS18B20 non-blocking + redraw kalibrasi saat data baru tiba
  if (halaman == HAL_KALIB_SUHU) {
    sensorSuhuUpdate();
    if (sensorSuhuSiapTampil()) perluGambar = true;
  }

  // Bacaan real-time untuk halaman kalibrasi MQ-135
  if (halaman == HAL_KALIB_UDARA) {
    static unsigned long udaraMs = 0;
    unsigned long now = millis();
    if (now - udaraMs >= 100) {
      udaraMs = now;
      mq135Bacaan = sensorUdaraRaw();
      perluGambar = true;
    }
  }

  // E-stop: hentikan semua aktuator & batalkan proses
  if (digitalRead(PIN_ESTOP) == LOW) {
    if (prosesJalan) {
      prosesBatal();
      tampilToast("E-STOP!");
    }
    agitasiSet(0);
    pompaLimbahSet(false);
    pompaSabunSet(false);
    fanSet(false);
    pompaStopAll();
  }

  bool animasiBerjalan = false;
  for (int L = 0; L < JUMLAH_LIST; L++) {
    if (lsAnimasi[L]) animasiBerjalan = true;   // termasuk frame terakhir animasi
    listUpdateAnimasi(L);
  }
  if (animasiBerjalan) perluGambar = true;

  if (toastAktif && (long)(millis() - toastSampai) >= 0) {
    toastAktif = false;
    perluGambar = true;
  }

  unsigned long sekarang = millis();
  if (perluGambar && (sekarang - waktuRenderTerakhir >= INTERVAL_RENDER_MS)) {
    perluGambar = false;                     // sebelum gambar, agar tombol saat display() tidak hilang
    gambarLayar();
    waktuRenderTerakhir = sekarang;
#ifdef DEBUG_RENDER
    dbgJumlahRender++;
#endif
#ifdef DEBUG_LAMBAT_RENDER_MS
    delay(DEBUG_LAMBAT_RENDER_MS);
#endif

    // Baca ulang tombol setelah oled.display() (bagian paling lama memblokir I2C)
    int t2 = bacaTombolBaru();
    if (t2 != TIDAK_ADA) tanganiTombol(t2);
  }

#ifdef DEBUG_RENDER
  if (sekarang - dbgWaktu >= 1000) {
    dbg.print(F("[DBG] render/detik: "));
    dbg.println(dbgJumlahRender);
    dbgJumlahRender = 0;
    dbgWaktu = sekarang;
  }
#endif

  delay(2);
}
