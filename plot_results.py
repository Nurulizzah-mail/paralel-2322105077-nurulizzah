"""
Script Visualisasi Grafik Benchmark & Komparasi Komputer Paralel
Simulasi Monte Carlo Estimasi Nilai PI
Komparasi: Serial Baseline vs POSIX Threads (Pthreads) vs OpenMP
"""

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

COMP_CSV = "comparison_results.csv"
BENCH_CSV = "benchmark_results.csv"
OUTPUT_IMG = "grafik_analisis_benchmark.png"

def plot_benchmark():
    if os.path.exists(COMP_CSV):
        df = pd.read_csv(COMP_CSV)
        print("[+] Membaca data komparasi dari:", COMP_CSV)
        has_comparison = True
    elif os.path.exists(BENCH_CSV):
        df = pd.read_csv(BENCH_CSV)
        print("[+] Membaca data benchmark OpenMP dari:", BENCH_CSV)
        has_comparison = False
    else:
        print("[!] File CSV tidak ditemukan. Silakan jalankan program C terlebih dahulu.")
        return

    print(df)

    fig, axes = plt.subplots(1, 3, figsize=(18, 5.5))
    total_samples = df['total_samples'].iloc[0]
    fig.suptitle(f"Analisis Kinerja Paralel Monte Carlo Estimasi PI (N = {total_samples:,} Titik)\nSerial Baseline vs Pthreads vs OpenMP", 
                 fontsize=14, fontweight='bold', y=1.02)

    threads = df['threads'].values

    if has_comparison:
        time_pth = df['time_pthreads'].values
        time_omp = df['time_openmp'].values
        time_ser = df['time_serial'].iloc[0]
        spd_pth = df['speedup_pthreads'].values
        spd_omp = df['speedup_openmp'].values
        eff_pth = df['eff_pthreads'].values
        eff_omp = df['eff_openmp'].values

        # 1. Plot Waktu Eksekusi
        axes[0].axhline(y=time_ser, color='#7f8c8d', linestyle=':', linewidth=2, label=f'Baseline Serial ({time_ser:.2f}s)')
        axes[0].plot(threads, time_pth, marker='o', color='#e67e22', linewidth=2.2, markersize=7, label='POSIX Threads (Pthreads)')
        axes[0].plot(threads, time_omp, marker='s', color='#2980b9', linewidth=2.2, markersize=7, label='OpenMP Reduction')
        for t, tp, to in zip(threads, time_pth, time_omp):
            axes[0].annotate(f"{tp:.2f}s", (t, tp), textcoords="offset points", xytext=(-10, 8), ha='center', fontsize=8, color='#d35400', fontweight='bold')
            axes[0].annotate(f"{to:.2f}s", (t, to), textcoords="offset points", xytext=(12, -12), ha='center', fontsize=8, color='#1f618d', fontweight='bold')
        axes[0].set_title("Waktu Eksekusi vs Jumlah Thread", fontweight='bold', fontsize=11)
        axes[0].set_xlabel("Jumlah Thread")
        axes[0].set_ylabel("Waktu Eksekusi (detik)")
        axes[0].set_xticks(threads)
        axes[0].grid(True, linestyle='--', alpha=0.6)
        axes[0].legend(loc='upper right')

        # 2. Plot Speedup
        axes[1].plot(threads, threads, linestyle='--', color='#27ae60', linewidth=2, label='Ideal Linear (100% Speedup)')
        axes[1].plot(threads, spd_pth, marker='o', color='#e67e22', linewidth=2.2, markersize=7, label='Speedup Pthreads')
        axes[1].plot(threads, spd_omp, marker='s', color='#2980b9', linewidth=2.2, markersize=7, label='Speedup OpenMP')
        for t, sp, so in zip(threads, spd_pth, spd_omp):
            axes[1].annotate(f"{sp:.2f}x", (t, sp), textcoords="offset points", xytext=(-10, 8), ha='center', fontsize=8, color='#d35400', fontweight='bold')
            axes[1].annotate(f"{so:.2f}x", (t, so), textcoords="offset points", xytext=(12, -12), ha='center', fontsize=8, color='#1f618d', fontweight='bold')
        axes[1].set_title("Speedup vs Jumlah Thread", fontweight='bold', fontsize=11)
        axes[1].set_xlabel("Jumlah Thread")
        axes[1].set_ylabel("Speedup (kali lipat)")
        axes[1].set_xticks(threads)
        axes[1].grid(True, linestyle='--', alpha=0.6)
        axes[1].legend(loc='upper left')

        # 3. Plot Efisiensi Paralel
        axes[2].axhline(y=100.0, color='#27ae60', linestyle='--', linewidth=1.8, label='Efisiensi Ideal (100%)')
        axes[2].plot(threads, eff_pth, marker='o', color='#e67e22', linewidth=2.2, markersize=7, label='Efisiensi Pthreads')
        axes[2].plot(threads, eff_omp, marker='s', color='#2980b9', linewidth=2.2, markersize=7, label='Efisiensi OpenMP')
        for t, ep, eo in zip(threads, eff_pth, eff_omp):
            axes[2].annotate(f"{ep:.1f}%", (t, ep), textcoords="offset points", xytext=(-10, 8), ha='center', fontsize=8, color='#d35400', fontweight='bold')
            axes[2].annotate(f"{eo:.1f}%", (t, eo), textcoords="offset points", xytext=(12, -12), ha='center', fontsize=8, color='#1f618d', fontweight='bold')
        axes[2].set_title("Efisiensi Paralel vs Jumlah Thread", fontweight='bold', fontsize=11)
        axes[2].set_xlabel("Jumlah Thread")
        axes[2].set_ylabel("Efisiensi (%)")
        axes[2].set_xticks(threads)
        axes[2].set_ylim(0, 115)
        axes[2].grid(True, linestyle='--', alpha=0.6)
        axes[2].legend(loc='lower left')

    plt.tight_layout()
    plt.savefig(OUTPUT_IMG, dpi=300)
    print(f"[+] Grafik visualisasi komparatif berhasil disimpan ke: {OUTPUT_IMG}")

if __name__ == "__main__":
    plot_benchmark()
