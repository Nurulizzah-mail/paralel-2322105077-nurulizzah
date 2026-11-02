# ============================================================================
# Makefile - Simulasi Monte Carlo Estimasi Nilai PI
# Mata Kuliah: Komputer Paralel
# ============================================================================
# Penggunaan (PowerShell):
#   $env:PATH = "C:\Program Files\GNU Octave\Octave-10.1.0\usr\bin;" + `
#               "C:\Program Files\GNU Octave\Octave-10.1.0\mingw64\bin;" + `
#               $env:PATH
#   make all        # Compile main.c -> monte_carlo_pi.exe
#   make clean      # Hapus file biner
# ============================================================================

CC      = gcc
CFLAGS  = -O3 -fopenmp -pthread
LDFLAGS = -lm -fopenmp -pthread
TARGET  = monte_carlo_pi.exe
SRC     = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)
	@echo [OK] Kompilasi berhasil -> $(TARGET)

clean:
	-rm -f $(TARGET)
	@echo [OK] File biner dihapus.

.PHONY: all clean

