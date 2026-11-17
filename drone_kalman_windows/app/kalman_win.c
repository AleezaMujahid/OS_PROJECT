/* kalman_win.c - Kalman Filter for many drones, using Windows system calls.
 *
 *   kalman_win run    <sensor.bin> <out.bin> [workers=4] [--csv file]
 *   kalman_win single <sensor.bin> <out.bin>            (one process, baseline)
 *   kalman_win worker <inMap> <outMap> <first> <last>   (started by "run", not by hand)
 *
 * Design (why this is parallel in a correct way):
 *   One Kalman filter is sequential (step k needs step k-1), so it cannot be split.
 *   But the drones are independent, so each worker process filters its own group.
 *
 *   parent                                   worker i
 *   ------                                   --------
 *   CreateFile + ReadFile: sensor file  -->  [shared memory "in"]  (CreateFileMapping)
 *   CreateFileMapping: "in" and "out"        OpenFileMapping + MapViewOfFile
 *   CreatePipe + CreateProcess x N           filter tracks first..last-1, write to "out"
 *   read worker status from pipes       <--  one status line on stdout (pipe)
 *   WaitForSingleObject x N
 *   CreateFile + WriteFile: output file <--  [shared memory "out"]
 */
#include "sc_platform.h"
#include "drn_format.h"
#include "kalman.h"
#include "sc_fileio.h"
#include "sc_ipc.h"
#include "sc_log.h"
#include "sc_mem.h"
#include "sc_proc.h"
#include "sc_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORKERS 64
#define LOGFILE "logs\\kalman_win.log"

static void usage(void) {
    fprintf(stderr,
            "usage:\n"
            "  kalman_win run    <sensor.bin> <out.bin> [workers=4] [--csv file]\n"
            "  kalman_win single <sensor.bin> <out.bin> [--csv file]\n");
}

/* Appends one row to a CSV file (writes the header if the file is new). */
static void csv_append(const char *path, const char *header, const char *row) {
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { log_winerr("CreateFile(csv)", path); return; }
    bool is_new = (GetLastError() != ERROR_ALREADY_EXISTS);
    if (is_new) fio_write_exact(h, header, strlen(header));
    fio_write_exact(h, row, strlen(row));
    CloseHandle(h);
}

static bool header_ok(const DrnHeader *h, uint64_t file_size) {
    if (memcmp(h->magic, "DRN1", 4) != 0) { LOGE("bad magic: not a DRN1 sensor file"); return false; }
    if (h->n_tracks == 0 || h->n_steps == 0) { LOGE("empty sensor file"); return false; }
    if (drn_in_bytes(h) != file_size) {
        LOGE("file size %llu does not match header (expected %zu)", (unsigned long long)file_size, drn_in_bytes(h));
        return false;
    }
    return true;
}

static void make_out_header(const DrnHeader *in, DrnHeader *out) {
    *out = *in;
    memcpy(out->magic, "KFO1", 4);
}

/* ------------------------------------------------------------------ worker */
static int cmd_worker(const char *in_name, const char *out_name, uint32_t first, uint32_t last) {
    log_open(LOGFILE, false); /* stdout is a pipe to the parent: keep the console quiet */
    ShMem in, out;
    if (!shm_open_existing(&in, in_name)) return 3;
    if (!shm_open_existing(&out, out_name)) return 3;

    const DrnHeader *h = (const DrnHeader *)in.view;
    if (memcmp(h->magic, "DRN1", 4) != 0 || first > last || last > h->n_tracks) {
        LOGE("worker: bad arguments or header (tracks %u..%u)", first, last);
        return 4;
    }
    KfParams p = { h->dt, h->q, h->r };

    double t0 = sc_now_us();
    for (uint32_t t = first; t < last; t++)
        kf_run_track(&p, drn_in_track(in.view, h, t), h->n_steps, drn_out_track(out.view, h, t));
    double ms = (sc_now_us() - t0) / 1000.0;

    ResUsage u;
    sc_get_usage(&u);
    printf("worker pid=%lu tracks=%u..%u compute_ms=%.3f cpu_ms=%.1f peak_ws_kb=%zu\n",
           (unsigned long)GetCurrentProcessId(), first, last - 1, ms, u.user_ms + u.kernel_ms, u.peak_ws_kb);
    fflush(stdout);
    LOGI("worker done: tracks %u..%u in %.3f ms", first, last - 1, ms);

    shm_close(&in);
    shm_close(&out);
    return 0;
}

/* --------------------------------------------------------------------- run */
static int cmd_run(const char *in_path, const char *out_path, int workers, const char *csv) {
    double t_start = sc_now_us();
    char sys[400];
    sc_sysinfo(sys, sizeof sys);
    LOGI("run: %s", sys);

    /* 1. read the sensor file straight into shared memory */
    HANDLE f = fio_open_read(in_path);
    if (f == INVALID_HANDLE_VALUE) return 1;
    uint64_t fsize = 0;
    DrnHeader h;
    if (!fio_get_size(f, &fsize) || !fio_read_exact(f, &h, sizeof h) || !header_ok(&h, fsize)) {
        fio_close(f);
        return 1;
    }
    fio_close(f);
    const size_t in_bytes = drn_in_bytes(&h), out_bytes = drn_out_bytes(&h);

    char in_name[64], out_name[64];
    snprintf(in_name, sizeof in_name, "Local\\kf_in_%lu", (unsigned long)GetCurrentProcessId());
    snprintf(out_name, sizeof out_name, "Local\\kf_out_%lu", (unsigned long)GetCurrentProcessId());
    ShMem in, out;
    if (!shm_create(&in, in_name, in_bytes)) return 1;
    if (!shm_create(&out, out_name, out_bytes)) { shm_close(&in); return 1; }

    f = fio_open_read(in_path);
    if (f == INVALID_HANDLE_VALUE || !fio_read_exact(f, in.view, in_bytes)) return 1;
    fio_close(f);
    DrnHeader oh;
    make_out_header(&h, &oh);
    memcpy(out.view, &oh, sizeof oh);
    double t_read = sc_now_us();

    /* 2. start the workers */
    if (workers < 1) workers = 1;
    if (workers > MAX_WORKERS) workers = MAX_WORKERS;
    if ((uint32_t)workers > h.n_tracks) workers = (int)h.n_tracks;

    char self[MAX_PATH];
    if (!proc_self_path(self, sizeof self)) return 1;

    HANDLE procs[MAX_WORKERS], pipes[MAX_WORKERS];
    uint32_t base = h.n_tracks / workers, extra = h.n_tracks % workers, next = 0;
    for (int i = 0; i < workers; i++) {
        uint32_t cnt = base + ((uint32_t)i < extra ? 1 : 0);
        HANDLE wr;
        if (!pipe_make(&pipes[i], &wr)) return 1;
        char cmd[MAX_PATH + 160];
        snprintf(cmd, sizeof cmd, "\"%s\" worker %s %s %u %u", self, in_name, out_name, next, next + cnt);
        if (!proc_spawn(cmd, wr, &procs[i], NULL)) return 1;
        CloseHandle(wr); /* the parent's copy of the write end must be closed */
        next += cnt;
    }
    double t_spawned = sc_now_us();

    /* 3. collect status lines, then wait for every worker */
    int failed = 0;
    double slowest = 0.0;
    for (int i = 0; i < workers; i++) {
        char line[512];
        pipe_read_all(pipes[i], line, sizeof line);
        CloseHandle(pipes[i]);
        DWORD code = 1;
        if (!proc_wait(procs[i], INFINITE, &code) || code != 0) {
            LOGE("worker %d failed (exit code %lu): %s", i, (unsigned long)code, line);
            failed = 1;
        }
        CloseHandle(procs[i]);
        printf("  %s", line);
        const char *c = strstr(line, "compute_ms=");
        if (c) { double v = atof(c + 11); if (v > slowest) slowest = v; }
    }
    double t_done = sc_now_us();
    if (failed) { shm_close(&in); shm_close(&out); return 2; }

    /* 4. write the output file */
    HANDLE o = fio_open_write(out_path);
    if (o == INVALID_HANDLE_VALUE || !fio_write_exact(o, out.view, out_bytes)) return 1;
    fio_close(o);
    double t_end = sc_now_us();

    shm_close(&in);
    shm_close(&out);

    double read_ms = (t_read - t_start) / 1e3, spawn_ms = (t_spawned - t_read) / 1e3;
    double comp_ms = (t_done - t_spawned) / 1e3, write_ms = (t_end - t_done) / 1e3, total_ms = (t_end - t_start) / 1e3;
    printf("run: %u drones x %u steps, %d worker(s)\n", h.n_tracks, h.n_steps, workers);
    printf("  read file -> shared memory : %9.2f ms\n", read_ms);
    printf("  create worker processes    : %9.2f ms\n", spawn_ms);
    printf("  workers compute + finish   : %9.2f ms   (slowest worker compute: %.2f ms)\n", comp_ms, slowest);
    printf("  write output file          : %9.2f ms\n", write_ms);
    printf("  TOTAL                      : %9.2f ms\n", total_ms);
    LOGI("run finished: workers=%d total_ms=%.2f", workers, total_ms);

    if (csv) {
        char row[256];
        snprintf(row, sizeof row, "windows,run,%d,%u,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", workers, h.n_tracks,
                 h.n_steps, read_ms, spawn_ms, comp_ms, write_ms, total_ms, slowest);
        csv_append(csv, "os,mode,workers,tracks,steps,read_ms,spawn_ms,compute_ms,write_ms,total_ms,slowest_worker_ms\n", row);
    }
    return 0;
}

/* ------------------------------------------------------------------ single */
static int cmd_single(const char *in_path, const char *out_path, const char *csv) {
    double t_start = sc_now_us();
    HANDLE f = fio_open_read(in_path);
    if (f == INVALID_HANDLE_VALUE) return 1;
    uint64_t fsize = 0;
    DrnHeader h;
    if (!fio_get_size(f, &fsize) || !fio_read_exact(f, &h, sizeof h) || !header_ok(&h, fsize)) {
        fio_close(f);
        return 1;
    }
    fio_close(f);

    /* ordinary private memory from VirtualAlloc (not shared) */
    void *in = mem_alloc(drn_in_bytes(&h));
    void *out = mem_alloc(drn_out_bytes(&h));
    if (!in || !out) return 1;
    f = fio_open_read(in_path);
    if (f == INVALID_HANDLE_VALUE || !fio_read_exact(f, in, drn_in_bytes(&h))) return 1;
    fio_close(f);
    DrnHeader oh;
    make_out_header(&h, &oh);
    memcpy(out, &oh, sizeof oh);
    double t_read = sc_now_us();

    KfParams p = { h.dt, h.q, h.r };
    for (uint32_t t = 0; t < h.n_tracks; t++)
        kf_run_track(&p, drn_in_track(in, &h, t), h.n_steps, drn_out_track(out, &h, t));
    double t_comp = sc_now_us();

    HANDLE o = fio_open_write(out_path);
    if (o == INVALID_HANDLE_VALUE || !fio_write_exact(o, out, drn_out_bytes(&h))) return 1;
    fio_close(o);
    double t_end = sc_now_us();
    mem_free(in);
    mem_free(out);

    double read_ms = (t_read - t_start) / 1e3, comp_ms = (t_comp - t_read) / 1e3;
    double write_ms = (t_end - t_comp) / 1e3, total_ms = (t_end - t_start) / 1e3;
    printf("single: %u drones x %u steps, 1 process\n", h.n_tracks, h.n_steps);
    printf("  read file -> memory        : %9.2f ms\n", read_ms);
    printf("  compute                    : %9.2f ms\n", comp_ms);
    printf("  write output file          : %9.2f ms\n", write_ms);
    printf("  TOTAL                      : %9.2f ms\n", total_ms);
    if (csv) {
        char row[256];
        snprintf(row, sizeof row, "windows,single,1,%u,%u,%.3f,0,%.3f,%.3f,%.3f,%.3f\n", h.n_tracks, h.n_steps,
                 read_ms, comp_ms, write_ms, total_ms, comp_ms);
        csv_append(csv, "os,mode,workers,tracks,steps,read_ms,spawn_ms,compute_ms,write_ms,total_ms,slowest_worker_ms\n", row);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--noop") == 0) return 0; /* used by bench_win to time CreateProcess */
    if (argc < 2) { usage(); return 1; }

    const char *csv = NULL;
    for (int i = 2; i + 1 < argc; i++)
        if (strcmp(argv[i], "--csv") == 0) { csv = argv[i + 1]; argc = i; break; }

    if (strcmp(argv[1], "worker") == 0) {
        if (argc != 6) { usage(); return 1; }
        return cmd_worker(argv[2], argv[3], (uint32_t)strtoul(argv[4], NULL, 10), (uint32_t)strtoul(argv[5], NULL, 10));
    }
    log_open(LOGFILE, true);
    int rc;
    if (strcmp(argv[1], "run") == 0 && argc >= 4)
        rc = cmd_run(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 4, csv);
    else if (strcmp(argv[1], "single") == 0 && argc >= 4)
        rc = cmd_single(argv[2], argv[3], csv);
    else { usage(); rc = 1; }
    log_close();
    return rc;
}
