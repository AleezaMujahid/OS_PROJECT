# Shared contract (give this to Person A and Person C)

The Windows and Linux programs must agree on these things, or the results cannot be compared.

## 1. Data file format (`include/drn_format.h`)
Little-endian, IEEE double.

| File | Magic | Content after the 40-byte header |
|---|---|---|
| sensor file | `DRN1` | `double[n_tracks][n_steps][2]`  (x, y in metres) |
| ground truth | `DRN1` | same layout as the sensor file |
| filter output | `KFO1` | `double[n_tracks][n_steps][4]`  (x, y, vx, vy) |

Header (40 bytes): `char magic[4]; uint32 n_tracks; uint32 n_steps; uint32 reserved=0; double dt; double q; double r;`

Person C's generator must write this format (the stand-in `tools/gen_test_data.c` shows how).
Suggested values: `dt = 0.1 s`, sensor sigma 3 m so `r = 9`, and `q = 10` (tested; see README).

## 2. Kalman Filter (`include/kalman.h`, `src/kalman.c`)
2D constant-velocity model, state `[x, y, vx, vy]`, Joseph-form covariance update.
`kalman.c`, `kalman.h` and `drn_format.h` contain **no OS calls**. Person A can copy them to
Linux unchanged. Then Linux and Windows outputs can be compared too (they should match; tiny
differences around 1e-12 are acceptable if compilers differ).

## 3. Parallel layout
Drones are split into contiguous groups, one group per worker process. Sizes differ by at most one drone.

## 4. Benchmark CSV (one file per OS, same columns)
```
os,call,category,iterations,min_us,median_us,p95_us,mean_us,max_us,cpu_ms,ws_delta_kb,peak_ws_kb
```
`os` is `windows` or `linux`. Times are in microseconds, 1000 iterations, 10% warm-up first.
Linux `fork`/`exec`/`wait` map to the single Windows row `CreateProcess`/`WaitForSingleObject`;
`munmap` pairs with `VirtualFree`; `chmod`/`chown`/`umask` all pair with `SetFileSecurity`.

Scaling CSV (`kalman_win ... --csv`):
```
os,mode,workers,tracks,steps,read_ms,spawn_ms,compute_ms,write_ms,total_ms,slowest_worker_ms
```

## 5. Command line
```
kalman_win single <sensor.bin> <out.bin>
kalman_win run    <sensor.bin> <out.bin> [workers] [--csv file]
validate compare <a.bin> <b.bin>
validate rmse    <out.bin> <truth.bin> <sensor.bin>
```
