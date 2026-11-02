@echo off
setlocal enabledelayedexpansion

title Web Dashboard Monte Carlo PI - OpenMP Parallel Computing

:: Tambahkan MinGW ke PATH sesi ini
set "PATH=C:\Program Files\GNU Octave\Octave-10.1.0\mingw64\bin;%PATH%"

echo =====================================================================
echo    MEMULAI WEB DASHBOARD MONTE CARLO ESTIMASI PI (OPENMP + FLASK)
echo =====================================================================

:: Pastikan file eksekusi C sudah terkompilasi
if not exist "monte_carlo_pi.exe" (
    echo [*] Mengompilasi main.c dengan OpenMP dan optimasi -O3...
    gcc -O3 -fopenmp main.c -o monte_carlo_pi.exe -lm
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Gagal mengompilasi main.c!
        pause
        exit /b 1
    )
    echo [OK] Kompilasi berhasil!
)

echo [*] Membuka browser ke http://localhost:5000...
start "" http://localhost:5000

echo [*] Menjalankan server Python Flask...
python server.py

pause
