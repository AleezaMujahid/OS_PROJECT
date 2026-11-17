@echo off
rem Builds everything with MinGW-w64 gcc (w64devkit or MSYS2 mingw64). Run from this folder.
setlocal
where gcc >nul 2>nul || (echo gcc not found. Install w64devkit or MinGW-w64 and add it to PATH. & exit /b 1)
if not exist bin mkdir bin
if not exist logs mkdir logs
if not exist results mkdir results

set CF=-std=gnu11 -O2 -Wall -Wextra -Iinclude
set LIBS=-ladvapi32 -lpsapi
set SRC=src\sc_log.c src\sc_time.c src\sc_fileio.c src\sc_mem.c src\sc_ipc.c src\sc_proc.c src\sc_perm.c src\kalman.c

for %%A in (kalman_win validate bench_win) do (
  echo Building %%A
  gcc %CF% app\%%A.c %SRC% -o bin\%%A.exe %LIBS% || exit /b 1
)
for %%D in (demo_fileio demo_process demo_memory demo_ipc demo_perms) do (
  echo Building %%D
  gcc %CF% demos\%%D.c %SRC% -o bin\%%D.exe %LIBS% || exit /b 1
)
echo Building gen_test_data
gcc %CF% tools\gen_test_data.c -o bin\gen_test_data.exe -lm || exit /b 1

echo.
echo Build OK. Programs are in the bin folder.
