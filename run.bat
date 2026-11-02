@echo off
setlocal enabledelayedexpansion

title UAS Komputer Paralel - Monte Carlo Pi Simulation

:: Tambahkan MinGW GCC ke PATH sesi ini
set "PATH=C:\Program Files\GNU Octave\Octave-10.1.0\mingw64\bin;%PATH%"

echo =====================================================================
echo    UAS KOMPUTER PARALEL - SIMULASI MONTE CARLO ESTIMASI NILAI PI
echo =====================================================================

where gcc >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC tidak ditemukan di sistem.
    pause
    exit /b 1
)

:: Pastikan biner C terkompilasi
if not exist "monte_carlo_pi.exe" (
    echo [*] Mengompilasi main.c dengan OpenMP dan optimasi -O3...
    gcc -O3 -fopenmp -pthread main.c -o monte_carlo_pi.exe -lm
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Kompilasi gagal! Periksa error di atas.
        pause
        exit /b 1
    )
    echo [OK] Kompilasi Berhasil -> monte_carlo_pi.exe
)

echo.
echo PILIH MODE APLIKASI:
echo   [1] Jalankan WEB DASHBOARD INTERAKTIF (Buka di Browser)
echo   [2] Jalankan Mode KONSOL TERMINAL (CLI)
echo.
set /p MODE="Masukkan Pilihan [1 atau 2, default 1]: "

if "%MODE%"=="2" (
    echo.
    echo [*] Menjalankan mode konsol...
    monte_carlo_pi.exe %*
    pause
    exit /b 0
)

echo.
echo [*] Membuka browser ke http://localhost:5000...
start "" http://localhost:5000
echo [*] Menjalankan server Web Flask...
python server.py
pause
