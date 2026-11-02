/**
 * ============================================================================
 * TUGAS UAS KOMPUTER PARALEL - OPSI C
 * ============================================================================
 * Topik       : Simulasi Monte Carlo Estimasi Nilai PI (10^9 Sampel)
 * Metode      : Pemrograman Paralel Komparasi POSIX Threads (Pthreads) vs OpenMP
 * Fitur Utama :
 *   1. Algoritma Serial Baseline Murni (Single Thread)
 *   2. Paralelisasi Low-Level Manual dengan POSIX Threads (Pthreads)
 *   3. Paralelisasi Directive-Based dengan OpenMP (Reduction Bebas Race Condition)
 *   4. Demonstrasi Khusus: Perbandingan DENGAN vs TANPA Reduction (Race Condition)
 *   5. Generator Angka Acak Modern (Xorshift128+) Independen Per-Thread (Thread-Safe)
 *   6. Suite Komparasi Lengkap (Serial vs Pthreads vs OpenMP: 1, 2, 4, 8 Thread)
 *   7. Analisis Speedup, Efisiensi Paralel, Amdahl's Law, dan Kompleksitas Kode (LOC)
 *   8. Ekspor Hasil Komprehensif ke CSV untuk Visualisasi Python & Web Dashboard
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <omp.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

// ============================================================================
// RANDOM NUMBER GENERATOR (RNG) MODERN: Xorshift128+
// ============================================================================
// Alasan Penggunaan:
// 1. Thread-Safe: Setiap thread mengalokasikan struct state sendiri (tanpa lock/mutex).
// 2. Kualitas Statistik Tinggi: Memiliki periode 2^128 - 1 dan lolos uji BigCrush.
// 3. Jauh lebih cepat dari rand() dan memiliki resolusi floating point 53-bit.
// ============================================================================

typedef struct {
    uint64_t s[2];
} xor128_state;

// Inisialisasi seed menggunakan SplitMix64 agar antar-thread tidak ada korelasi
static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline void xor128_init(xor128_state *state, uint64_t master_seed, int thread_id) {
    // Seed unik per thread yang di-garam (salted) dengan thread_id
    uint64_t sm = master_seed ^ (0x517cc1b727220a95ULL * (uint64_t)(thread_id + 1));
    state->s[0] = splitmix64(&sm);
    state->s[1] = splitmix64(&sm);
}

static inline uint64_t xor128_next(xor128_state *state) {
    uint64_t s1 = state->s[0];
    const uint64_t s0 = state->s[1];
    state->s[0] = s0;
    s1 ^= s1 << 23;
    state->s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return state->s[1] + s0;
}

// Menghasilkan double seragam pada rentang [0.0, 1.0) dengan presisi 53-bit
static inline double xor128_next_double(xor128_state *state) {
    return (xor128_next(state) >> 11) * (1.0 / (double)(1ULL << 53));
}

// ============================================================================
// 1. ALGORITMA SERIAL BASELINE (SINGLE THREAD)
// ============================================================================
// Digunakan sebagai baseline acuan murni untuk perhitungan Speedup dan Efisiensi.
// Tidak ada overhead thread creation, context switching, maupun sinkronisasi.
// ============================================================================
uint64_t run_monte_carlo_serial(uint64_t total_samples, uint64_t base_seed) {
    xor128_state rng;
    xor128_init(&rng, base_seed, 0);
    uint64_t total_inside = 0;

    for (uint64_t i = 0; i < total_samples; i++) {
        double x = xor128_next_double(&rng);
        double y = xor128_next_double(&rng);
        if (x * x + y * y <= 1.0) {
            total_inside++;
        }
    }

    return total_inside;
}

// ============================================================================
// 2. PARALELISASI LOW-LEVEL: POSIX THREADS (PTHREADS)
// ============================================================================
// Karakteristik:
// - Pembagian Workload: Statis eksplisit (total_samples dibagi rata ke num_threads,
//   sisa modulus dialokasikan ke thread terakhir).
// - Mekanisme Sinkronisasi:
//   Setiap thread menulis hasil penghitungan ke field privat 'inside_count' pada
//   struct-nya masing-masing (tanpa mutex / lock contention, bebas false sharing).
//   Setelah semua thread selesai melalui pthread_join(), thread master
//   mengakumulasi hasil parsial secara sekuensial.
// ============================================================================

typedef struct {
    int thread_id;
    uint64_t samples_to_process;
    uint64_t base_seed;
    uint64_t inside_count;
} PthreadWorkerArg;

void* monte_carlo_pthread_worker(void *arg) {
    PthreadWorkerArg *data = (PthreadWorkerArg*)arg;
    xor128_state rng;
    xor128_init(&rng, data->base_seed, data->thread_id);

    uint64_t local_inside = 0;
    uint64_t count = data->samples_to_process;

    for (uint64_t i = 0; i < count; i++) {
        double x = xor128_next_double(&rng);
        double y = xor128_next_double(&rng);
        if (x * x + y * y <= 1.0) {
            local_inside++;
        }
    }

    data->inside_count = local_inside;
    return NULL;
}

uint64_t run_monte_carlo_pthreads(uint64_t total_samples, int num_threads, uint64_t base_seed) {
    if (num_threads <= 0) num_threads = 1;

    pthread_t *threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    PthreadWorkerArg *args = (PthreadWorkerArg*)malloc(num_threads * sizeof(PthreadWorkerArg));

    uint64_t chunk = total_samples / num_threads;
    uint64_t remainder = total_samples % num_threads;

    // 1. Spawning / Creation Threads
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].base_seed = base_seed;
        args[i].samples_to_process = chunk + (i == num_threads - 1 ? remainder : 0);
        args[i].inside_count = 0;

        pthread_create(&threads[i], NULL, monte_carlo_pthread_worker, (void*)&args[i]);
    }

    // 2. Joining & Sinkronisasi Pengumpulan Hasil
    uint64_t total_inside = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_inside += args[i].inside_count;
    }

    free(threads);
    free(args);

    return total_inside;
}

// ============================================================================
// 3. PARALELISASI DIRECTIVE-BASED: OPENMP REDUCTION (AMAN & STANDAR)
// ============================================================================
// Karakteristik:
// - Pragma: #pragma omp parallel num_threads(num_threads) reduction(+:total_inside)
// - Scope Variabel:
//   • shared : total_inside (direduksi secara atomik oleh runtime OpenMP di barrier)
//   • private: tid, rng, loop variable i, x, y
// - Loop Scheduling: schedule(static) pembagian iterasi otomatis seimbang.
// ============================================================================
uint64_t run_monte_carlo_reduction(uint64_t total_samples, int num_threads, uint64_t base_seed) {
    if (num_threads <= 0) num_threads = 1;

    uint64_t total_inside = 0;

    #pragma omp parallel num_threads(num_threads) reduction(+:total_inside)
    {
        int tid = omp_get_thread_num();
        xor128_state rng;
        xor128_init(&rng, base_seed, tid);

        #pragma omp for schedule(static)
        for (uint64_t i = 0; i < total_samples; i++) {
            double x = xor128_next_double(&rng);
            double y = xor128_next_double(&rng);
            if (x * x + y * y <= 1.0) {
                total_inside++;
            }
        }
    }

    return total_inside;
}

// ============================================================================
// 4. SIMULASI MONTE CARLO TANPA REDUCTION (DEMO RACE CONDITION)
// Hanya untuk pembuktian akademik bahwa tanpa reduction hasilnya rusak/korup.
// ============================================================================
uint64_t run_monte_carlo_unsafe(uint64_t total_samples, int num_threads, uint64_t base_seed) {
    volatile uint64_t unsafe_inside = 0;

    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        xor128_state rng;
        xor128_init(&rng, base_seed, tid);

        #pragma omp for schedule(static)
        for (uint64_t i = 0; i < total_samples; i++) {
            double x = xor128_next_double(&rng);
            double y = xor128_next_double(&rng);
            if (x * x + y * y <= 1.0) {
                // RACE CONDITION: banyak thread menulis variabel yang sama serentak!
                unsafe_inside++;
            }
        }
    }

    return unsafe_inside;
}

// ============================================================================
// STRUKTUR DATA HASIL BENCHMARK & KOMPARASI
// ============================================================================
typedef struct {
    int threads;
    uint64_t total_samples;
    uint64_t inside_count;
    double estimated_pi;
    double absolute_error;
    double execution_time;
    double speedup;
    double efficiency;
} BenchmarkResult;

typedef struct {
    int threads;
    uint64_t total_samples;
    double time_serial;
    double time_pthreads;
    double time_openmp;
    double speedup_pthreads;
    double speedup_openmp;
    double eff_pthreads;
    double eff_openmp;
    double pi_pthreads;
    double pi_openmp;
    double err_pthreads;
    double err_openmp;
} ComparisonResult;

// ============================================================================
// FUNGSI TAMPILAN BANNER & TABEL
// ============================================================================
void print_header() {
    printf("\n");
    printf("================================================================================\n");
    printf("        SIMULASI ESTIMASI NILAI PI DENGAN METODE MONTE CARLO                    \n");
    printf("              KOMPARASI MULTITHREADING: PTHREADS VS OPENMP                      \n");
    printf("                   UAS KOMPUTER PARALEL - OPSI C (1.5x)                         \n");
    printf("================================================================================\n");
    printf(" Target Sampel      : 1,000,000,000 Titik (10^9)\n");
    printf(" Arsitektur CPU     : Maksimal %d Logical Cores Terdeteksi\n", omp_get_max_threads());
    printf(" Model RNG          : Xorshift128+ Modern (Thread-Safe, Private State)\n");
    printf(" Nilai Baku PI      : %.16f\n", M_PI);
    printf(" Paradigma Diuji    : Serial Baseline, POSIX Threads (Pthreads), OpenMP\n");
    printf("================================================================================\n\n");
}

void print_table(BenchmarkResult results[], int count) {
    printf("\n===============================================================================================================\n");
    printf("                                  HASIL EVALUASI BENCHMARK OPENMP                                               \n");
    printf("===============================================================================================================\n");
    printf("%-8s | %-15s | %-15s | %-15s | %-12s | %-9s | %-10s\n",
           "Threads", "Titik Dihitung", "Titik di Lingk.", "Estimasi PI", "Error Mutlak", "Waktu (s)", "Speedup");
    printf("---------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-8d | %-15llu | %-15llu | %-15.10f | %-12.8e | %-9.4f | %-9.2fx\n",
               results[i].threads,
               (unsigned long long)results[i].total_samples,
               (unsigned long long)results[i].inside_count,
               results[i].estimated_pi,
               results[i].absolute_error,
               results[i].execution_time,
               results[i].speedup);
    }
    printf("===============================================================================================================\n");
}

void print_comparison_table(ComparisonResult results[], int count) {
    printf("\n==============================================================================================================================\n");
    printf("                         TABEL KOMPARASI LENGKAP: SERIAL vs PTHREADS vs OPENMP                                        \n");
    printf("==============================================================================================================================\n");
    printf("%-7s | %-12s | %-12s | %-10s | %-10s | %-10s | %-10s | %-15s\n",
           "Threads", "Waktu Pth(s)", "Waktu Omp(s)", "Spd Pth", "Spd Omp", "Eff Pth(%)", "Eff Omp(%)", "Est PI (Omp)");
    printf("------------------------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-7d | %-12.4f | %-12.4f | %-9.2fx | %-9.2fx | %-9.1f%% | %-9.1f%% | %-15.10f\n",
               results[i].threads,
               results[i].time_pthreads,
               results[i].time_openmp,
               results[i].speedup_pthreads,
               results[i].speedup_openmp,
               results[i].eff_pthreads,
               results[i].eff_openmp,
               results[i].pi_openmp);
    }
    printf("==============================================================================================================================\n");
}

// Simpan hasil ke file CSV agar bisa dibuat grafik dengan Python
void save_to_csv(const char *filename, BenchmarkResult results[], int count) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[!] Peringatan: Gagal menyimpan data ke %s\n", filename);
        return;
    }

    fprintf(fp, "threads,total_samples,inside_count,estimated_pi,absolute_error,execution_time,speedup,efficiency\n");
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%d,%llu,%llu,%.15f,%.15e,%.6f,%.4f,%.2f\n",
                results[i].threads,
                (unsigned long long)results[i].total_samples,
                (unsigned long long)results[i].inside_count,
                results[i].estimated_pi,
                results[i].absolute_error,
                results[i].execution_time,
                results[i].speedup,
                results[i].efficiency);
    }
    fclose(fp);
    printf("[+] Data benchmark OpenMP berhasil disimpan ke: %s\n", filename);
}

void save_comparison_to_csv(const char *filename, ComparisonResult results[], int count) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[!] Peringatan: Gagal menyimpan data ke %s\n", filename);
        return;
    }

    fprintf(fp, "threads,total_samples,time_serial,time_pthreads,time_openmp,speedup_pthreads,speedup_openmp,eff_pthreads,eff_openmp,pi_pthreads,pi_openmp,err_pthreads,err_openmp\n");
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%d,%llu,%.6f,%.6f,%.6f,%.4f,%.4f,%.2f,%.2f,%.12f,%.12f,%.12e,%.12e\n",
                results[i].threads,
                (unsigned long long)results[i].total_samples,
                results[i].time_serial,
                results[i].time_pthreads,
                results[i].time_openmp,
                results[i].speedup_pthreads,
                results[i].speedup_openmp,
                results[i].eff_pthreads,
                results[i].eff_openmp,
                results[i].pi_pthreads,
                results[i].pi_openmp,
                results[i].err_pthreads,
                results[i].err_openmp);
    }
    fclose(fp);
    printf("[+] Data komparasi (Pthreads vs OpenMP) berhasil disimpan ke: %s\n", filename);
}

// ============================================================================
// MODE EKSEKUSI SUITE
// ============================================================================

void run_suite_benchmark(uint64_t total_samples) {
    int thread_configs[] = {1, 2, 4, 8};
    int num_configs = sizeof(thread_configs) / sizeof(thread_configs[0]);
    int max_avail = omp_get_max_threads();
    if (max_avail > 8) {
        thread_configs[3] = max_avail;
    }

    BenchmarkResult results[10];
    double baseline_time = 0.0;
    uint64_t base_seed = (uint64_t)time(NULL);

    printf("[*] Memulai Benchmark OpenMP dengan %llu Sampel (10^9)...\n", (unsigned long long)total_samples);
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_configs; i++) {
        int t = thread_configs[i];
        printf("--> Menjalankan OpenMP dengan %2d Thread ... ", t);
        fflush(stdout);

        double t_start = omp_get_wtime();
        uint64_t inside = run_monte_carlo_reduction(total_samples, t, base_seed);
        double t_end = omp_get_wtime();
        double elapsed = t_end - t_start;

        if (i == 0) {
            baseline_time = elapsed; // Waktu sekuensial (1 Thread)
        }

        double pi_est = 4.0 * ((double)inside / (double)total_samples);
        double err = fabs(pi_est - M_PI);
        double speedup = (elapsed > 0 ? baseline_time / elapsed : 1.0);
        double efficiency = (speedup / (double)t) * 100.0;

        results[i].threads = t;
        results[i].total_samples = total_samples;
        results[i].inside_count = inside;
        results[i].estimated_pi = pi_est;
        results[i].absolute_error = err;
        results[i].execution_time = elapsed;
        results[i].speedup = speedup;
        results[i].efficiency = efficiency;

        printf("Selesai! [%.3f detik | Speedup: %.2fx]\n", elapsed, speedup);
    }

    print_table(results, num_configs);
    save_to_csv("benchmark_results.csv", results, num_configs);
}

void run_suite_comparison(uint64_t total_samples) {
    int thread_configs[] = {1, 2, 4, 8};
    int num_configs = sizeof(thread_configs) / sizeof(thread_configs[0]);
    int max_avail = omp_get_max_threads();
    if (max_avail > 8) {
        thread_configs[3] = max_avail;
    }

    ComparisonResult results[10];
    BenchmarkResult omp_results[10];
    uint64_t base_seed = (uint64_t)time(NULL);

    printf("\n[*] Mengukur Waktu Baseline Serial Murni (1 Core, 0 Overhead)...\n");
    double t_ser_start = omp_get_wtime();
    uint64_t inside_serial = run_monte_carlo_serial(total_samples, base_seed);
    double t_ser_end = omp_get_wtime();
    double time_serial = t_ser_end - t_ser_start;
    double pi_serial = 4.0 * ((double)inside_serial / (double)total_samples);
    printf("    -> Baseline Serial Selesai: %.4f detik | PI: %.10f\n\n", time_serial, pi_serial);

    printf("[*] Memulai Komparasi Multithreading Pthreads vs OpenMP (%llu Sampel)...\n", (unsigned long long)total_samples);
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_configs; i++) {
        int t = thread_configs[i];
        printf("--> [%2d Thread] Menjalankan Pthreads ... ", t);
        fflush(stdout);

        // Uji Pthreads
        double t_pth_start = omp_get_wtime();
        uint64_t inside_pth = run_monte_carlo_pthreads(total_samples, t, base_seed);
        double t_pth_end = omp_get_wtime();
        double elapsed_pth = t_pth_end - t_pth_start;
        double pi_pth = 4.0 * ((double)inside_pth / (double)total_samples);
        double err_pth = fabs(pi_pth - M_PI);
        double spd_pth = (elapsed_pth > 0 ? time_serial / elapsed_pth : 1.0);
        double eff_pth = (spd_pth / (double)t) * 100.0;

        printf("Selesai (%.3fs) | OpenMP ... ", elapsed_pth);
        fflush(stdout);

        // Uji OpenMP
        double t_omp_start = omp_get_wtime();
        uint64_t inside_omp = run_monte_carlo_reduction(total_samples, t, base_seed);
        double t_omp_end = omp_get_wtime();
        double elapsed_omp = t_omp_end - t_omp_start;
        double pi_omp = 4.0 * ((double)inside_omp / (double)total_samples);
        double err_omp = fabs(pi_omp - M_PI);
        double spd_omp = (elapsed_omp > 0 ? time_serial / elapsed_omp : 1.0);
        double eff_omp = (spd_omp / (double)t) * 100.0;

        printf("Selesai (%.3fs)\n", elapsed_omp);

        // Simpan ke struct
        results[i].threads = t;
        results[i].total_samples = total_samples;
        results[i].time_serial = time_serial;
        results[i].time_pthreads = elapsed_pth;
        results[i].time_openmp = elapsed_omp;
        results[i].speedup_pthreads = spd_pth;
        results[i].speedup_openmp = spd_omp;
        results[i].eff_pthreads = eff_pth;
        results[i].eff_openmp = eff_omp;
        results[i].pi_pthreads = pi_pth;
        results[i].pi_openmp = pi_omp;
        results[i].err_pthreads = err_pth;
        results[i].err_openmp = err_omp;

        omp_results[i].threads = t;
        omp_results[i].total_samples = total_samples;
        omp_results[i].inside_count = inside_omp;
        omp_results[i].estimated_pi = pi_omp;
        omp_results[i].absolute_error = err_omp;
        omp_results[i].execution_time = elapsed_omp;
        omp_results[i].speedup = spd_omp;
        omp_results[i].efficiency = eff_omp;
    }

    print_comparison_table(results, num_configs);
    save_comparison_to_csv("comparison_results.csv", results, num_configs);
    save_to_csv("benchmark_results.csv", omp_results, num_configs);

    printf("\n[RANGKUMAN ANALISIS KOMPARATIF]:\n");
    printf("1. Kompleksitas Kode (LOC): OpenMP (~3 baris pragma) jauh lebih ringkas dibanding Pthreads (~40 baris kode).\n");
    printf("2. Performa Waktu Eksekusi: Keduanya bersaing ketat. OpenMP runtime memiliki overhead minimal saat thread barrier.\n");
    printf("3. Trade-off: Pthreads memberi kendali penuh alokasi memori thread, sedangkan OpenMP unggul dalam produktivitas.\n");
}

void demo_race_condition(uint64_t samples) {
    printf("\n================================================================================\n");
    printf("            DEMONSTRASI TANTANGAN KHUSUS: RACE CONDITION vs REDUCTION           \n");
    printf("================================================================================\n");
    printf("Sampel Uji: %llu titik dengan 4 Thread\n\n", (unsigned long long)samples);

    uint64_t seed = 987654321ULL;

    printf("[1] Menjalankan DENGAN Reduction (Aman)...\n");
    double t1_start = omp_get_wtime();
    uint64_t safe_count = run_monte_carlo_reduction(samples, 4, seed);
    double t1_end = omp_get_wtime();
    double pi_safe = 4.0 * (double)safe_count / (double)samples;

    printf("    -> Hasil Hitung Titik : %llu\n", (unsigned long long)safe_count);
    printf("    -> Estimasi Nilai PI  : %.10f (Error: %.8f)\n", pi_safe, fabs(pi_safe - M_PI));
    printf("    -> Waktu Eksekusi     : %.4f detik\n\n", t1_end - t1_start);

    printf("[2] Menjalankan TANPA Reduction (Race Condition Terjadi!)...\n");
    double t2_start = omp_get_wtime();
    uint64_t unsafe_count = run_monte_carlo_unsafe(samples, 4, seed);
    double t2_end = omp_get_wtime();
    double pi_unsafe = 4.0 * (double)unsafe_count / (double)samples;

    printf("    -> Hasil Hitung Titik : %llu\n", (unsigned long long)unsafe_count);
    printf("    -> Estimasi Nilai PI  : %.10f (Error: %.8f)\n", pi_unsafe, fabs(pi_unsafe - M_PI));
    printf("    -> Waktu Eksekusi     : %.4f detik\n\n", t2_end - t2_start);

    printf("[!] KESIMPULAN PERBANDINGAN:\n");
    if (safe_count != unsafe_count) {
        printf("    Terbukti terjadi DATA LOSS / CORRUPTION sebesar %llu titik pada mode tanpa reduction!\n",
               (unsigned long long)(safe_count > unsafe_count ? safe_count - unsafe_count : unsafe_count - safe_count));
        printf("    Hal ini membuktikan kewajiban penggunaan OpenMP reduction pada variabel akumulator.\n");
    } else {
        printf("    Tidak tampak collision pada skala kecil, namun tetap unsafe secara arsitektur paralel.\n");
    }
    printf("================================================================================\n\n");
}

void run_custom_interactive() {
    int mode = 1; // 1: OpenMP, 2: Pthreads, 3: Serial
    int threads = 4;
    uint64_t samples = 1000000000ULL;
    char buffer[128];

    printf("\nPILIH METODE KOMPUTASI:\n");
    printf("  [1] OpenMP (Default)\n");
    printf("  [2] POSIX Threads (Pthreads)\n");
    printf("  [3] Serial Baseline (Single Thread)\n");
    printf("Pilihan [1-3, Default 1]: ");
    if (fgets(buffer, sizeof(buffer), stdin) && buffer[0] != '\n') {
        int m = atoi(buffer);
        if (m >= 1 && m <= 3) mode = m;
    }

    if (mode != 3) {
        printf("Masukkan Jumlah Thread (1, 2, 4, 8) [Default: 4]: ");
        if (fgets(buffer, sizeof(buffer), stdin) && buffer[0] != '\n') {
            int t = atoi(buffer);
            if (t > 0) threads = t;
        }
    } else {
        threads = 1;
    }

    printf("Masukkan Jumlah Sampel Titik [Default: 1000000000]: ");
    if (fgets(buffer, sizeof(buffer), stdin) && buffer[0] != '\n') {
        uint64_t s = strtoull(buffer, NULL, 10);
        if (s > 0) samples = s;
    }

    printf("\n[*] Menjalankan simulasi (%llu titik, %d thread, Mode: %s)...\n",
           (unsigned long long)samples, threads,
           (mode == 1 ? "OpenMP" : (mode == 2 ? "Pthreads" : "Serial")));

    double t_start = omp_get_wtime();
    uint64_t inside = 0;
    if (mode == 1) {
        inside = run_monte_carlo_reduction(samples, threads, (uint64_t)time(NULL));
    } else if (mode == 2) {
        inside = run_monte_carlo_pthreads(samples, threads, (uint64_t)time(NULL));
    } else {
        inside = run_monte_carlo_serial(samples, (uint64_t)time(NULL));
    }
    double t_end = omp_get_wtime();
    double elapsed = t_end - t_start;

    double pi_est = 4.0 * (double)inside / (double)samples;
    double error = fabs(pi_est - M_PI);

    printf("\n================================================================================\n");
    printf("                                HASIL SIMULASI                                  \n");
    printf("================================================================================\n");
    printf(" Model Paralelisasi      : %s\n", (mode == 1 ? "OpenMP" : (mode == 2 ? "Pthreads" : "Serial")));
    printf(" Jumlah Thread Digunakan : %d\n", threads);
    printf(" Total Titik Sampel      : %llu\n", (unsigned long long)samples);
    printf(" Titik di Dalam Lingkaran: %llu\n", (unsigned long long)inside);
    printf(" Estimasi Nilai PI       : %.12f\n", pi_est);
    printf(" Nilai Baku M_PI         : %.12f\n", M_PI);
    printf(" Selisih Error Mutlak    : %.12f\n", error);
    printf(" Rasio Akurasi           : %.6f%%\n", (1.0 - (error / M_PI)) * 100.0);
    printf(" Waktu Eksekusi Total    : %.4f detik\n", elapsed);
    printf(" Throughput Komputasi    : %.2f Juta Titik/detik\n", (elapsed > 0 ? ((double)samples / elapsed) / 1000000.0 : 0.0));
    printf("================================================================================\n");
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--json-run") == 0) {
            int t = (argc > 2) ? atoi(argv[2]) : 4;
            uint64_t s = (argc > 3) ? strtoull(argv[3], NULL, 10) : 1000000000ULL;
            const char *method = (argc > 4) ? argv[4] : "openmp";

            double t_start = omp_get_wtime();
            uint64_t inside = 0;
            if (strcmp(method, "pthread") == 0) {
                inside = run_monte_carlo_pthreads(s, t, (uint64_t)time(NULL));
            } else if (strcmp(method, "serial") == 0) {
                t = 1;
                inside = run_monte_carlo_serial(s, (uint64_t)time(NULL));
            } else {
                inside = run_monte_carlo_reduction(s, t, (uint64_t)time(NULL));
            }
            double t_end = omp_get_wtime();
            double elapsed = t_end - t_start;
            double pi_est = 4.0 * (double)inside / (double)s;
            double err = fabs(pi_est - M_PI);

            printf("{\"success\":true,\"method\":\"%s\",\"threads\":%d,\"total_samples\":%llu,\"inside_count\":%llu,\"estimated_pi\":%.12f,\"actual_pi\":%.12f,\"absolute_error\":%.12e,\"execution_time\":%.6f,\"throughput\":%.2f}\n",
                   method, t, (unsigned long long)s, (unsigned long long)inside, pi_est, M_PI, err, elapsed, (elapsed > 0 ? ((double)s / elapsed) / 1e6 : 0.0));
            return 0;
        } else if (strcmp(argv[1], "--json-benchmark") == 0) {
            uint64_t total_samples = (argc > 2) ? strtoull(argv[2], NULL, 10) : 1000000000ULL;
            int thread_configs[] = {1, 2, 4, 8};
            int num_configs = sizeof(thread_configs) / sizeof(thread_configs[0]);
            int max_avail = omp_get_max_threads();
            if (max_avail > 8) thread_configs[3] = max_avail;
            
            double baseline_time = 0.0;
            uint64_t base_seed = (uint64_t)time(NULL);

            printf("{\"success\":true,\"total_samples\":%llu,\"results\":[", (unsigned long long)total_samples);
            for (int i = 0; i < num_configs; i++) {
                int t = thread_configs[i];
                double t_start = omp_get_wtime();
                uint64_t inside = run_monte_carlo_reduction(total_samples, t, base_seed);
                double t_end = omp_get_wtime();
                double elapsed = t_end - t_start;
                if (i == 0) baseline_time = elapsed;
                double pi_est = 4.0 * ((double)inside / (double)total_samples);
                double err = fabs(pi_est - M_PI);
                double speedup = (elapsed > 0 ? baseline_time / elapsed : 1.0);
                double efficiency = (speedup / (double)t) * 100.0;

                if (i > 0) printf(",");
                printf("{\"threads\":%d,\"inside_count\":%llu,\"estimated_pi\":%.12f,\"absolute_error\":%.12e,\"execution_time\":%.6f,\"speedup\":%.4f,\"efficiency\":%.2f}",
                       t, (unsigned long long)inside, pi_est, err, elapsed, speedup, efficiency);
            }
            printf("]}\n");
            return 0;
        } else if (strcmp(argv[1], "--json-compare") == 0) {
            uint64_t total_samples = (argc > 2) ? strtoull(argv[2], NULL, 10) : 1000000000ULL;
            int thread_configs[] = {1, 2, 4, 8};
            int num_configs = sizeof(thread_configs) / sizeof(thread_configs[0]);
            int max_avail = omp_get_max_threads();
            if (max_avail > 8) thread_configs[3] = max_avail;

            uint64_t base_seed = (uint64_t)time(NULL);
            double t_s_start = omp_get_wtime();
            run_monte_carlo_serial(total_samples, base_seed);
            double t_s_end = omp_get_wtime();
            double time_serial = t_s_end - t_s_start;

            printf("{\"success\":true,\"total_samples\":%llu,\"time_serial\":%.6f,\"comparison\":[", (unsigned long long)total_samples, time_serial);
            for (int i = 0; i < num_configs; i++) {
                int t = thread_configs[i];
                // Pthreads
                double t_p1 = omp_get_wtime();
                uint64_t in_pth = run_monte_carlo_pthreads(total_samples, t, base_seed);
                double t_p2 = omp_get_wtime();
                double elapsed_pth = t_p2 - t_p1;

                // OpenMP
                double t_o1 = omp_get_wtime();
                uint64_t in_omp = run_monte_carlo_reduction(total_samples, t, base_seed);
                double t_o2 = omp_get_wtime();
                double elapsed_omp = t_o2 - t_o1;

                double spd_pth = (elapsed_pth > 0 ? time_serial / elapsed_pth : 1.0);
                double spd_omp = (elapsed_omp > 0 ? time_serial / elapsed_omp : 1.0);
                double eff_pth = (spd_pth / (double)t) * 100.0;
                double eff_omp = (spd_omp / (double)t) * 100.0;
                double pi_pth = 4.0 * ((double)in_pth / (double)total_samples);
                double pi_omp = 4.0 * ((double)in_omp / (double)total_samples);

                if (i > 0) printf(",");
                printf("{\"threads\":%d,\"time_pthreads\":%.6f,\"time_openmp\":%.6f,\"speedup_pthreads\":%.4f,\"speedup_openmp\":%.4f,\"eff_pthreads\":%.2f,\"eff_openmp\":%.2f,\"pi_pthreads\":%.10f,\"pi_openmp\":%.10f}",
                       t, elapsed_pth, elapsed_omp, spd_pth, spd_omp, eff_pth, eff_omp, pi_pth, pi_omp);
            }
            printf("]}\n");
            return 0;
        } else if (strcmp(argv[1], "--json-race") == 0) {
            uint64_t samples = (argc > 2) ? strtoull(argv[2], NULL, 10) : 20000000ULL;
            uint64_t seed = 987654321ULL;
            
            double t1_start = omp_get_wtime();
            uint64_t safe_count = run_monte_carlo_reduction(samples, 4, seed);
            double t1_end = omp_get_wtime();
            double pi_safe = 4.0 * (double)safe_count / (double)samples;

            double t2_start = omp_get_wtime();
            uint64_t unsafe_count = run_monte_carlo_unsafe(samples, 4, seed);
            double t2_end = omp_get_wtime();
            double pi_unsafe = 4.0 * (double)unsafe_count / (double)samples;

            uint64_t lost_points = (safe_count > unsafe_count) ? (safe_count - unsafe_count) : (unsafe_count - safe_count);

            printf("{\"success\":true,\"samples\":%llu,\"threads\":4,\"safe\":{\"inside\":%llu,\"pi\":%.10f,\"error\":%.10e,\"time\":%.6f},\"unsafe\":{\"inside\":%llu,\"pi\":%.10f,\"error\":%.10e,\"time\":%.6f},\"lost_points\":%llu,\"race_occurred\":%s}\n",
                   (unsigned long long)samples,
                   (unsigned long long)safe_count, pi_safe, fabs(pi_safe - M_PI), t1_end - t1_start,
                   (unsigned long long)unsafe_count, pi_unsafe, fabs(pi_unsafe - M_PI), t2_end - t2_start,
                   (unsigned long long)lost_points,
                   (lost_points > 0 ? "true" : "false"));
            return 0;
        } else if (strcmp(argv[1], "--compare") == 0) {
            uint64_t samples = 1000000000ULL;
            if (argc > 2) {
                samples = strtoull(argv[2], NULL, 10);
            }
            print_header();
            run_suite_comparison(samples);
            return 0;
        } else if (strcmp(argv[1], "--benchmark") == 0) {
            uint64_t samples = 1000000000ULL;
            if (argc > 2) {
                samples = strtoull(argv[2], NULL, 10);
            }
            print_header();
            run_suite_benchmark(samples);
            return 0;
        } else if (strcmp(argv[1], "--demo-race") == 0) {
            print_header();
            demo_race_condition(20000000ULL);
            return 0;
        } else if (strcmp(argv[1], "--run") == 0) {
            int t = (argc > 2) ? atoi(argv[2]) : 4;
            uint64_t s = (argc > 3) ? strtoull(argv[3], NULL, 10) : 1000000000ULL;
            print_header();
            double t_start = omp_get_wtime();
            uint64_t inside = run_monte_carlo_reduction(s, t, (uint64_t)time(NULL));
            double t_end = omp_get_wtime();
            double pi_est = 4.0 * (double)inside / (double)s;
            printf("Mode: OpenMP | Threads: %d | Samples: %llu | Inside: %llu | PI: %.10f | Waktu: %.4f s\n",
                   t, (unsigned long long)s, (unsigned long long)inside, pi_est, t_end - t_start);
            return 0;
        } else if (strcmp(argv[1], "--pthread-run") == 0) {
            int t = (argc > 2) ? atoi(argv[2]) : 4;
            uint64_t s = (argc > 3) ? strtoull(argv[3], NULL, 10) : 1000000000ULL;
            print_header();
            double t_start = omp_get_wtime();
            uint64_t inside = run_monte_carlo_pthreads(s, t, (uint64_t)time(NULL));
            double t_end = omp_get_wtime();
            double pi_est = 4.0 * (double)inside / (double)s;
            printf("Mode: Pthreads | Threads: %d | Samples: %llu | Inside: %llu | PI: %.10f | Waktu: %.4f s\n",
                   t, (unsigned long long)s, (unsigned long long)inside, pi_est, t_end - t_start);
            return 0;
        }
    }

    // Menu Interaktif jika dijalankan tanpa argumen
    print_header();
    while (true) {
        printf("PILIHAN MENU:\n");
        printf("  [1] Jalankan Benchmark KOMPARASI LENGKAP: Serial vs Pthreads vs OpenMP (10^9 Sampel)\n");
        printf("  [2] Jalankan Benchmark Standar OpenMP (1, 2, 4, 8 Thread)\n");
        printf("  [3] Simulasi Kustom (Pilih Model, Thread, dan Jumlah Sampel)\n");
        printf("  [4] Demonstrasi Bahaya Race Condition (Dengan vs Tanpa Reduction)\n");
        printf("  [5] Keluar Program\n");
        printf("Masukkan Pilihan [1-5]: ");

        char input[32];
        if (!fgets(input, sizeof(input), stdin)) break;
        int choice = atoi(input);

        if (choice == 1) {
            run_suite_comparison(1000000000ULL);
            break;
        } else if (choice == 2) {
            run_suite_benchmark(1000000000ULL);
            break;
        } else if (choice == 3) {
            run_custom_interactive();
            break;
        } else if (choice == 4) {
            demo_race_condition(50000000ULL);
            printf("\nTekan Enter untuk kembali ke menu...");
            getchar();
            print_header();
        } else if (choice == 5) {
            printf("\nProgram selesai. Terima kasih!\n");
            break;
        } else {
            printf("\n[!] Pilihan tidak valid. Silakan coba lagi.\n\n");
        }
    }

    return 0;
}
