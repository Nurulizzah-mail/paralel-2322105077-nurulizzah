# Simulasi Monte Carlo Estimasi Nilai $\pi$ (10⁹ Sampel)
**Mata Kuliah:** Komputer Paralel  
**Topik:** Opsi C - Monte Carlo Simulasi Nilai $\pi$ (Difficulty: 1.5x)  
**Teknologi:** C (C99), POSIX Threads (Pthreads), OpenMP, Xorshift128+ PRNG, Python 3 (Matplotlib)

---

## 1. Deskripsi Proyek
Proyek ini mengimplementasikan simulasi Monte Carlo untuk mengestimasi nilai konstanta matematika $\pi$ menggunakan **1.000.000.000 (1 Miliar)** titik sampel acak pada kuadran kartesius $[0, 1] \times [0, 1]$.

Perhitungan dilakukan secara komparatif memanfaatkan multi-core CPU dengan **Baseline Serial**, **POSIX Threads (Pthreads)**, dan **OpenMP**, memastikan bebas dari *race condition* melalui mekanisme *accumulator per-thread* dan *reduction*, serta menggunakan *Random Number Generator* (RNG) modern independen pada tiap thread.

---

## 2. Dasar Teori & Formulasi Matematika

1. Diberikan bujur sangkar satuan dengan panjang sisi $s = 1$, maka luas bujur sangkar adalah:
   $$L_{\text{persegi}} = 1 \times 1 = 1$$
2. Di dalam bujur sangkar tersebut terdapat seperempat lingkaran dengan radius $r = 1$, luasnya adalah:
   $$L_{\text{lingkaran}} = \frac{1}{4} \pi r^2 = \frac{\pi}{4}$$
3. Jika ditaburkan $N$ titik acak seragam $(x, y)$ di mana $x \in [0, 1]$ dan $y \in [0, 1]$:
   * Suatu titik berada di dalam lingkaran jika memenuhi persamaan:
     $$x^2 + y^2 \le 1$$
   * Peluang titik jatuh di dalam lingkaran adalah rasio luas:
     $$\frac{N_{\text{inside}}}{N_{\text{total}}} \approx \frac{L_{\text{lingkaran}}}{L_{\text{persegi}}} = \frac{\pi / 4}{1} = \frac{\pi}{4}$$
   * Maka estimasi nilai $\pi$ diperoleh dengan:
     $$\pi \approx 4 \times \frac{N_{\text{inside}}}{N_{\text{total}}}$$

---

## 3. Tantangan Khusus & Solusi Arsitektur

### A. Race Condition pada Accumulator $\rightarrow$ Wajib Reduction
* **Masalah:** Jika banyak thread secara bersamaan mencoba melakukan inkremen pada variabel global (`total_inside++`), terjadi tabrakan pembacaan dan penulisan memori (*race condition*), yang berakibat pada *data loss* signifikan.
* **Solusi:** Menggunakan klausa `#pragma omp parallel for reduction(+:total_inside)`. OpenMP mengalokasikan akumulator lokal privat di register tiap core, lalu menggabungkannya secara atomik di akhir loop.
* **Pembuktian:** Disediakan fitur `--demo-race` yang membuktikan bahwa tanpa *reduction*, estimasi $\pi$ meleset drastis (hilang jutaan titik data).

### B. Kualitas & Kecepatan RNG Per-Thread
* **Masalah:** Fungsi standar `rand()` di C tidak *thread-safe* atau menggunakan *internal lock/mutex* global. Jika dibagi ke 8 thread, terjadi *lock contention* yang membuat program sangat lambat, serta periode `rand()` hanya $2^{15}-1 = 32.767$ (sangat tidak memadai untuk $10^9$ sampel).
* **Solusi:** Menggunakan algoritma **Xorshift128+** dengan inisialisasi **SplitMix64**.
  * Setiap thread memiliki *state RNG* privat `xor128_state` di memori thread masing-masing (tanpa *sharing*, tanpa *lock*).
  * Periode sangat besar: $2^{128}-1$.
  * Presisi mantissa 53-bit seragam $[0.0, 1.0)$.
  * Sangat cepat (~2 siklus instruksi CPU per angka acak).

### C. Analisis Trade-off: Thread vs Speedup vs Akurasi
1. **Speedup & Skalabilitas (Hukum Amdahl):**
   * Peningkatan thread dari 1 ke 2 memberikan *speedup* hampir linier ($\approx 1.98\times$).
   * Pada 4 dan 8 thread, *speedup* meningkat signifikan (mencapai $\approx 3.89\times$ pada 8 thread), memangkas waktu dari 3.87 detik menjadi 0.99 detik untuk 1 Miliar titik.
2. **Akurasi Statistik:**
   * Akurasi Monte Carlo secara teoretis berbanding lurus dengan $\mathcal{O}(1/\sqrt{N})$.
   * Pada $N = 10^9$, nilai estimasi mencapai akurasi hingga 4–5 digit desimal ($\pi \approx 3.1415...$, galat $\approx 3.5 \times 10^{-5}$).
   * Peningkatan jumlah thread **tidak mengurangi akurasi statistik**, karena setiap thread diberikan *seed stream* independen yang tidak saling berkorelasi (*uncorrelated independent pseudo-random streams*).

---

## 4. Struktur File Proyek

```
Uas Komputer Pararel/
├── main.c                          # Kode sumber C (OpenMP + Xorshift128+ RNG + JSON API)
├── server.py                       # Backend server Python Flask penghubung C & Web
├── templates/
│   └── index.html                  # Frontend SPA Dashboard Interaktif
├── static/
│   ├── style.css                   # Desain modern Cyber Dark Glassmorphism
│   └── app.js                      # Controller interaktif & Chart.js visualizer
├── run.bat                         # Launcher utama (Pilih Web Dashboard atau CLI)
├── run_web.bat                     # Launcher langsung ke Web Dashboard
├── monte_carlo_pi.exe              # File biner C hasil kompilasi
├── plot_results.py                 # Script Python untuk membuat grafik benchmark statis
├── benchmark_results.csv           # Data hasil pengujian performa (CSV)
├── grafik_analisis_benchmark.png   # Hasil grafik visualisasi
└── README.md                       # Laporan & dokumentasi teknis lengkap
```

---

## 5. Cara Menjalankan Aplikasi

### Cara 1: Web Dashboard Interaktif (Paling Direkomendasikan)
1. Cukup klik dua kali **`run_web.bat`** (atau jalankan `run.bat` dan pilih opsi 1).
2. Browser akan terbuka otomatis ke alamat: **`http://localhost:5000`**
3. Di dalam web dashboard, Anda dapat:
   - **Live Simulator:** Mengatur jumlah thread (1-16) dan sampel (hingga 1 Miliar), melihat visualisasi sebaran titik di canvas kuadran secara real-time, dan memantau throughput komputasi.
   - **Benchmark 10⁹ Titik:** Menjalankan benchmark otomatis 1, 2, 4, dan 8 core secara langsung, melihat grafik Speedup vs Amdahl dan grafik Waktu Eksekusi, serta mengekspor data ke CSV.
   - **Lab Race Condition:** Menguji pembuktian tabrakan memori secara nyata (*Dengan vs Tanpa Reduction*).
   - **Teori & Arsitektur:** Membaca penjelasan matematis dan arsitektur OpenMP.

### Cara 2: Menjalankan Mode Terminal / CLI
Jalankan file biner secara langsung:
```bash
# 1. Menjalankan menu interaktif di terminal
.\monte_carlo_pi.exe

# 2. Menjalankan benchmark otomatis 1 Miliar sampel (1, 2, 4, 8 Thread)
.\monte_carlo_pi.exe --benchmark 1000000000

# 3. Menjalankan uji perbandingan Race Condition
.\monte_carlo_pi.exe --demo-race
```

### Cara 3: Membuat Grafik Analisis (Python)
Setelah menjalankan benchmark dan file `benchmark_results.csv` terbuat:
```bash
python plot_results.py
```
Grafik akan disimpan otomatis ke file `grafik_analisis_benchmark.png`.

---

## 6. Hasil Pengujian Nyata (1 Miliar Sampel)

Spesifikasi Mesin Uji: 8 Logical Cores CPU, Windows 64-bit, GCC 14.2.0 OpenMP.

| Threads | Titik Dihitung ($N$) | Titik dlm Lingkaran ($N_{\text{in}}$) | Estimasi Nilai $\pi$ | Error Mutlak | Waktu (s) | Speedup | Efisiensi |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | 1.000.000.000 | 785.406.850 | **3.1416274000** | $3.47 \times 10^{-5}$ | 3.8680 s | 1.00x | 100.0% |
| **2** | 1.000.000.000 | 785.416.573 | **3.1416662920** | $7.36 \times 10^{-5}$ | 1.9510 s | 1.98x | 99.1% |
| **4** | 1.000.000.000 | 785.413.371 | **3.1416534840** | $6.08 \times 10^{-5}$ | 1.4600 s | 2.65x | 66.2% |
| **8** | 1.000.000.000 | 785.389.258 | **3.1415570320** | $3.56 \times 10^{-5}$ | 0.9950 s | 3.89x | 48.6% |

---

## 7. Bukti Uji Tantangan Khusus: Race Condition vs Reduction

Hasil pengujian eksekusi 20.000.000 titik dengan 4 thread:
* **DENGAN Reduction:**
  * Titik terhitung: **15.706.717** titik
  * Estimasi $\pi$: **3.1413434000** (Error: 0.00024925)
* **TANPA Reduction (Race Condition Terjadi):**
  * Titik terhitung: **6.156.959** titik
  * Estimasi $\pi$: **1.2313918000** (Error: 1.91020085 $\rightarrow$ **RUSAK!**)
  * Terjadi kehilangan data (**Data Loss**) sebanyak **9.549.758 titik** akibat penulisan konkuren ke memori tanpa proteksi.
"# pararel-2322105077-nurulizzah" 
