# Drone Kalman Filter: Windows part (Person B)

CT-353 Operating Systems CCP. This folder is the **Windows** implementation: 12 Win32 system
calls, a parallel Kalman Filter, a benchmark, and a validation tool.

## 1. Build

You need MinGW-w64 gcc (easiest and smallest: **w64devkit**, about 80 MB, no installer).

```
build.bat
```

All programs appear in `bin\`. (To cross-compile on Linux: `make CC=x86_64-w64-mingw32-gcc`.)

## 2. Run (in this order)

```
bin\demo_fileio.exe          rem CreateFile, WriteFile, ReadFile, CloseHandle
bin\demo_process.exe         rem CreateProcess, WaitForSingleObject
bin\demo_memory.exe          rem VirtualAlloc, VirtualFree
bin\demo_ipc.exe             rem CreatePipe, CreateFileMapping, MapViewOfFile
bin\demo_perms.exe           rem SetFileSecurity (needs an NTFS drive)

bin\gen_test_data.exe 2000 2000 42 data\sensor.bin data\truth.bin
bin\kalman_win.exe single data\sensor.bin data\out_single.bin
bin\kalman_win.exe run    data\sensor.bin data\out_w4.bin 4 --csv results\kalman_scaling.csv
bin\validate.exe compare data\out_single.bin data\out_w4.bin
bin\validate.exe rmse    data\out_w4.bin data\truth.bin data\sensor.bin

bin\bench_win.exe 1000       rem writes results\bench_windows.csv
```

Create the `data` folder first (`mkdir data`). Logs go to `logs\`.

For the scaling chart run `kalman_win run ... 1`, `2`, `4`, `8` with the same `--csv` file.

## 3. The 15 Linux calls and their Windows counterparts

The rubric table has 15 Linux calls but only 12 Windows calls. This is not a mistake: Windows
combines some of them.

| Purpose | Linux | Windows | Module |
|---|---|---|---|
| File I/O | open, read, write, close | CreateFile, ReadFile, WriteFile, CloseHandle | `sc_fileio` |
| Process | fork + exec, wait | CreateProcess (does both), WaitForSingleObject | `sc_proc` |
| Memory | mmap, munmap | VirtualAlloc, VirtualFree | `sc_mem` |
| IPC | pipe, shmget, shmat | CreatePipe, CreateFileMapping, MapViewOfFile | `sc_ipc` |
| Permissions | chmod, chown, umask | SetFileSecurity (DACL = chmod, owner = chown, no umask) | `sc_perm` |

Report points for "OS-specific observations":
- No `fork()` on Windows. A child starts a fresh program, so workers get their data through
  shared memory and a command line, not by copying the parent.
- CreateProcess is far slower than the other calls (milliseconds, not microseconds). This is
  why the parallel Kalman filter only pays off when each worker has enough work.
- `VirtualAlloc` has two steps (reserve address space, then commit memory). `mmap` has one.
- Windows shared memory is a *named object* (`Local\kf_in_<pid>`), found by name. System V
  `shmget` uses a numeric key.
- Windows permissions are ACLs (a list of who may do what), not the 9 rwx bits.
- `ReadFile`/`WriteFile` take a 32-bit size, so big transfers are done in 1 GiB pieces.

## 4. Design (for Modularity and Innovation marks)

- One module per row of the table, each with a header and a `.c` file, plus a logger and a timer.
  Every wrapper checks the result and logs success or `GetLastError()` with the system message.
- `kalman.c` is pure maths with no OS calls, so it can be copied unchanged to Linux.
- Parallelism: one Kalman filter is sequential (step k needs step k-1) and cannot be split.
  The drones are independent, so each worker process filters its own group of drones.
- The sensor file is read into a shared-memory block; workers write into disjoint parts of a
  second shared-memory block; each worker reports back through its own pipe.
- Validation: (1) the parallel output is byte-identical to the single-process output;
  (2) the filtered position error is smaller than the raw sensor error.

## 5. What was tested, and what was not

Built with MinGW-w64 (no warnings) and run under Wine on Linux:
- All demos ran. The 12 benchmark calls all succeeded.
- 500 drones x 2000 steps: single process, 1, 2 and 4 workers all produced byte-identical output.
- Filtered error was about 62% smaller than the raw sensor error.

**Not tested on real Windows yet. Please run everything once on your PC.**
- `demo_perms.exe` could not be checked properly: Wine does not enforce Windows ACLs, so the
  "open for write must be DENIED" step cannot pass there. On a real NTFS drive it should.
- Timings from Wine are meaningless. Use only numbers from your real Windows PC.
- Antivirus (Defender) can make `CreateProcess` and file calls slower and noisier. Close other
  programs while benchmarking, and keep the laptop plugged in.

## 6. Folder map

```
include/   sc_*.h (one per module), kalman.h, drn_format.h
src/       sc_log, sc_time, sc_fileio, sc_mem, sc_ipc, sc_proc, sc_perm, kalman
demos/     one small program per category
app/       kalman_win (single | run | worker), validate, bench_win
tools/     gen_test_data (stand-in until Person C's generator is ready)
```

See `INTERFACE.md` for what must match between Windows, Linux and Person C's generator.
