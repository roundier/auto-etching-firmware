# Panduan Repository — Auto Etching Firmware

Panduan ini mencakup dua peran: **Pemilik Repository** dan **Contributor**. Pastikan Anda membagi bagian yang sesuai dengan peran Anda.

---

## Daftar Isi

- [Bagian A: Untuk Pemilik Repository](#bagian-a-untuk-pemilik-repository)
- [Bagian B: Untuk Contributor](#bagian-b-untuk-contributor)
- [Struktur Repository](#struktur-repository)
- [Checklist Sebelum Merge](#checklist-sebelum-merge)

---

## Bagian A: Untuk Pemilik Repository

Pemilik repository memiliki tanggung jawab utama menjaga kualitas dan stabilitas kode utama (branch `main`). Semua perubahan yang masuk ke `main` harus diverifikasi oleh pemilik.

### A1. Setup Repository (Sudah Dilakukan)

Repository sudah dikonfigurasi dengan:

- Remote: `https://github.com/MRaihanP/auto-etching-firmware.git`
- Branch utama: `main`
- Folder project: `/home/raihanP/DevBox/Projects/firmware/arduino/auto-etching-firmware`

### A2. Workflow Pemilik Repository

```
1. Terima Pull Request dari Contributor
2. Review code secara teliti (baca diff, cek logika, cek konflik)
3. Test compile di Arduino IDE / PlatformIO
4. Jika ada masalah → Request changes di PR (beri komentar perbaikan)
5. Jika sudah OK → Approve & Merge PR ke main
6. Update LOG.md dan SUMMARY.md jika ada perubahan signifikan
7. Update version di config.h jika perlu
8. Push ke GitHub
```

### A3. Review Pull Request

Saat menerima Pull Request, pemilik wajib:

- [ ] Membaca deskripsi PR dengan jelas
- [ ] Review setiap file yang diubah (jangan hanya lihat jumlah changed files)
- [ ] Cek tidak ada hardcoded value, debug print, atau kode testing yang tertinggal
- [ ] Cek kompatibilitas dengan struktur folder saat ini
- [ ] Test compile di Arduino IDE / PlatformIO
- [ ] Cek tidak ada file yang salah tempat (mis: .ino di luar root)
- [ ] Beri approve hanya jika semua checklist lolos

### A4. Menggunakan GitHub CLI (Opsional)

Jika `gh` CLI terinstall:

```bash
# Lihat daftar PR yang open
gh pr list

# Review PR tertentu
gh pr review <PR_NUMBER> --approve --comment "LGTM, approved"

# Merge PR
gh pr merge <PR_NUMBER> --squash
```

---

## Bagian B: Untuk Contributor

Contributor adalah pihak yang ingin memperbaiki, mengupdate, atau menambahkan fitur ke repository ini. **DILARANG KERAS push langsung ke branch `main`**.

### B1. Aturan Utama untuk Contributor

> **PENTING**: Semua perubahan HARUS melalui proses Pull Request. Contributor TIDAK BOLEH melakukan push langsung ke `main`. Jika ditemukan pelanggaran, PR akan di-reject.

### B2. Workflow Contributor (Wajib Ikuti)

#### Langkah 1: Fork Repository

```bash
# Fork repo lewat GitHub web UI, lalu clone fork Anda
git clone https://github.com/<USERNAME_ANDA>/auto-etching-firmware.git
cd auto-etching-firmware

# Tambahkan upstream (repo utama)
git remote add upstream https://github.com/MRaihanP/auto-etching-firmware.git

# Fetch latest dari upstream
git fetch upstream

# Sync fork dengan upstream
git checkout main
git merge upstream/main
git push origin main
```

#### Langkah 2: Buat Branch Baru

```bash
# Buat branch baru untuk perubahan Anda
git checkout -b fix/nama-perbaikan
# atau
git checkout -b feature/nama-fitur
```

**Aturan penamaan branch:**
- `fix/` — untuk perbaikan bug
- `feature/` — untuk fitur baru
- `refactor/` — untuk refactoring kode
- `docs/` — untuk perubahan dokumentasi

#### Langkah 3: Lakukan Perubahan

- Edit file yang diperlukan
- Pastikan tidak merusak struktur folder yang ada
- Commit dengan message yang jelas:

```bash
git add .
git commit -m "fix: deskripsi singkat perbaikan"
```

#### Langkah 4: Push Branch ke Fork

```bash
git push origin fix/nama-perbaikan
```

#### Langkah 5: Buat Pull Request

1. Buka GitHub web UI
2. Pilih fork Anda → branch yang baru di-push
3. Klik **"Compare & pull request"**
4. Isi deskripsi PR:

```
## Deskripsi
[Jelaskan perubahan yang dilakukan]

## Jenis Perubahan
- [ ] Bug fix
- [ ] Fitur baru
- [ ] Refactoring
- [ ] Dokumentasi

## Testing
[ ] Sudah test compile di Arduino IDE / PlatformIO
[ ] Sudah test di hardware (jika berlaku)

## Checklist
- [ ] Tidak ada debug print yang tertinggal
- [ ] Tidak ada hardcoded value yang salah
- [ ] Kode mengikuti struktur folder yang ada
```

#### Langkah 6: Tunggu Review Pemilik Repository

- Pemilik repository akan review PR Anda
- Jika ada komentar/request changes → perbaiki dan push lagi ke branch yang sama
- Jika sudah di-approve → pemilik akan merge ke `main`

### B3. Jika Ada Konflik (Conflict)

```bash
# Fetch latest dari upstream
git fetch upstream

# Rebase branch Anda ke upstream/main
git rebase upstream/main

# Selesaikan konflik, lalu:
git add .
git rebase --continue

# Force push ke fork Anda
git push origin fix/nama-perbaikan --force
```

---

## Struktur Repository

```
auto-etching-firmware/
├── auto-etching-firmware.ino   # File utama firmware (setup, loop, navigasi)
├── config.h                    # Pin map & konstanta global
├── data.h                      # Struct, enum halaman, state global
├── dbg.h                       # Debug helper (log Serial)
├── input.h                     # Input handling (6 tombol via PCF8574)
├── sensor.h                    # Sensor level (touch), DS18B20, MQ-135
├── process.h                   # State machine proses etching
├── presetops.h                 # CRUD preset, kalkulasi ml, validasi
├── motor.h                     # Stepper pompa peristaltik (non-blocking)
├── dcmotor.h                   # Motor DC & pompa DC (TB6612FNG)
├── buzzer.h                    # Buzzer non-blocking
├── ui.h                        # OLED UI & seluruh halaman
├── storage.h                   # Penyimpanan NVS (Preferences) + CRC32
├── fontmini.h                  # Font data untuk OLED
├── sensor_extra.h              # ⚠️ kode mati (belum di-#include) — perlu dihapus/aktifkan
├── README.md                   # Project overview
├── GUIDE.md                    # Panduan ini
├── SUMMARY.md                  # Ringkasan fitur & pin map saat ini
├── PROCESS.md                  # Rencana detail proses (5 modul) + pin map TMC2209
├── PROGRESS.md                 # Checklist status implementasi
├── dosing-design.md            # Desain rinci modul dosing
├── LOG.md                      # Changelog / log perubahan
├── etching-otomatis.md         # Plan awal (sebagian superseded)
├── water-level-sensor-plan.md  # Rencana sensor level non-contact
├── Dokumen Perancangan Alat Etching PCB.pdf  # Dokumen perancangan
├── .gitignore                  # File yang di-ignore Git
└── backup/                     # Backup versi-versi sebelumnya
    ├── v0.5.0_2026-10-12/
    ├── v0.5.1_2026-10-12/
    ├── ...
    └── v0.8.2_2026-10-12/
```

> `aktuator.h` **tidak ada** di root — hanya ada di dalam `backup/` sebagai
> sisa versi lama. Wormannya sekarang: `motor.h` (stepper), `dcmotor.h` (DC),
> `buzzer.h`.

---

## Checklist Sebelum Merge (Untuk Pemilik Repository)

Sebuah Pull Request hanya boleh di-merge ke `main` jika:

- [ ] Deskripsi PR jelas dan lengkap
- [ ] Jenis perubahan sudah disebutkan (bug fix / fitur / dll)
- [ ] Sudah di-test compile di Arduino IDE / PlatformIO
- [ ] Tidak ada debug print (`Serial.println`) yang tertinggal
- [ ] Tidak ada file yang salah tempat
- [ ] Kode mengikuti struktur folder yang ada
- [ ] Tidak ada konflik dengan `main`
- [ ] `LOG.md` dan `SUMMARY.md` sudah diupdate jika ada perubahan signifikan
- [ ] Version di `config.h` sudah di-bump jika perlu
- [ ] Pemilik repository sudah menyetujui (approve)

---

## Halaman README

README berisi overview singkat project. Lihat `README.md` untuk detail lebih lanjut.
