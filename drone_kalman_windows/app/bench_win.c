/* bench_win.c - times each of the 12 Windows system calls, one call at a time.
 *
 *   bench_win [iterations=1000] [csv=results\bench_windows.csv]
 *
 * For every call: warm-up runs, then N timed runs. Only the call itself is inside the
 * timer; setup and clean-up (e.g. closing a handle we just opened) are outside it.
 * Reports min / median / p95 / mean / max in microseconds, plus CPU time and working-set
 * change over the whole loop. The Linux benchmark writes the SAME csv columns. */
#include "sc_platform.h"
#include "sc_fileio.h"
#include "sc_log.h"
#include "sc_mem.h"
#include "sc_perm.h"
#include "sc_proc.h"
#include "sc_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_RD  "bench_read.bin"
#define FILE_WR  "bench_write.bin"
#define FILE_SEC "bench_sec.bin"
#define BLOCK    4096
#define ONE_MIB  (1u << 20)

static struct {
    HANDLE h, h2;
    void *p;
    char buf[BLOCK];
} G;
static PROCESS_INFORMATION G_PI;
static STARTUPINFOA G_SI;
static char G_CMD[MAX_PATH + 32], SELF_CMD[MAX_PATH + 32];
static PermSD G_SD;

/* ---------------------------------------------------------------- helpers */
static bool make_file(const char *path, size_t n) {
    HANDLE h = fio_open_write(path);
    if (h == INVALID_HANDLE_VALUE) return false;
    char *b = (char *)calloc(1, n);
    bool ok = b && fio_write_exact(h, b, n);
    free(b);
    return fio_close(h) && ok;
}
static HANDLE open_rd(void) {
    return CreateFileA(FILE_RD, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* ------------------------------------------------------------ file I/O */
static bool op_createfile(void) { G.h = open_rd(); return G.h != INVALID_HANDLE_VALUE; }
static void cl_closeh(void) { CloseHandle(G.h); }

static bool init_read(void) { G.h = open_rd(); return G.h != INVALID_HANDLE_VALUE; }
static void fini_h(void) { CloseHandle(G.h); }
static bool pre_rewind(void) { return SetFilePointer(G.h, 0, NULL, FILE_BEGIN) == 0; }
static bool op_readfile(void) { DWORD n = 0; return ReadFile(G.h, G.buf, BLOCK, &n, NULL) && n == BLOCK; }

static bool init_write(void) {
    G.h = CreateFileA(FILE_WR, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return G.h != INVALID_HANDLE_VALUE;
}
static bool op_writefile(void) { DWORD n = 0; return WriteFile(G.h, G.buf, BLOCK, &n, NULL) && n == BLOCK; }

static bool pre_open(void) { G.h = open_rd(); return G.h != INVALID_HANDLE_VALUE; }
static bool op_closehandle(void) { return CloseHandle(G.h) != 0; }

/* ------------------------------------------------------------- process */
static bool pre_cmd(void) {
    memset(&G_SI, 0, sizeof G_SI);
    G_SI.cb = sizeof G_SI;
    strcpy(G_CMD, SELF_CMD);
    return true;
}
static bool op_createprocess(void) { return CreateProcessA(NULL, G_CMD, NULL, NULL, FALSE, 0, NULL, NULL, &G_SI, &G_PI) != 0; }
static void cl_process(void) {
    WaitForSingleObject(G_PI.hProcess, INFINITE);
    CloseHandle(G_PI.hProcess);
    CloseHandle(G_PI.hThread);
}

static bool init_wait(void) { /* one child that has already exited: its handle is signalled */
    pre_cmd();
    if (!CreateProcessA(NULL, G_CMD, NULL, NULL, FALSE, 0, NULL, NULL, &G_SI, &G_PI)) return false;
    CloseHandle(G_PI.hThread);
    return WaitForSingleObject(G_PI.hProcess, INFINITE) == WAIT_OBJECT_0;
}
static bool op_wait(void) { return WaitForSingleObject(G_PI.hProcess, 0) == WAIT_OBJECT_0; }
static void fini_wait(void) { CloseHandle(G_PI.hProcess); }

/* -------------------------------------------------------------- memory */
static bool op_valloc(void) { G.p = VirtualAlloc(NULL, ONE_MIB, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); return G.p != NULL; }
static void cl_vfree(void) { VirtualFree(G.p, 0, MEM_RELEASE); }
static bool pre_valloc(void) { return op_valloc(); }
static bool op_vfree(void) { return VirtualFree(G.p, 0, MEM_RELEASE) != 0; }

/* ----------------------------------------------------------------- IPC */
static bool op_createpipe(void) { return CreatePipe(&G.h, &G.h2, NULL, 0) != 0; }
static void cl_pipe(void) { CloseHandle(G.h); CloseHandle(G.h2); }

static bool op_createmap(void) {
    G.h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, ONE_MIB, NULL);
    return G.h != NULL;
}
static bool init_map(void) {
    G.h2 = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, ONE_MIB, NULL);
    return G.h2 != NULL;
}
static bool op_mapview(void) { G.p = MapViewOfFile(G.h2, FILE_MAP_ALL_ACCESS, 0, 0, 0); return G.p != NULL; }
static void cl_unmap(void) { UnmapViewOfFile(G.p); }
static void fini_map(void) { CloseHandle(G.h2); }

/* --------------------------------------------------------- permissions */
static bool init_sec(void) { return make_file(FILE_SEC, 16) && perm_prepare(&G_SD, FILE_ALL_ACCESS); }
static bool op_setsec(void) { return SetFileSecurityA(FILE_SEC, DACL_SECURITY_INFORMATION, &G_SD.sd) != 0; }
static void fini_sec(void) { DeleteFileA(FILE_SEC); }

/* --------------------------------------------------------------- table */
typedef struct {
    const char *name, *category;
    int max_iters;
    bool (*init)(void);
    void (*fini)(void);
    bool (*prepare)(void);
    bool (*op)(void);
    void (*cleanup)(void);
} Bench;

static const Bench BENCHES[] = {
    { "CreateFile",          "file_io", 1 << 30, NULL,       NULL,      NULL,        op_createfile,    cl_closeh },
    { "ReadFile",            "file_io", 1 << 30, init_read,  fini_h,    pre_rewind,  op_readfile,      NULL },
    { "WriteFile",           "file_io", 1 << 30, init_write, fini_h,    pre_rewind,  op_writefile,     NULL },
    { "CloseHandle",         "file_io", 1 << 30, NULL,       NULL,      pre_open,    op_closehandle,   NULL },
    { "CreateProcess",       "process", 200,     NULL,       NULL,      pre_cmd,     op_createprocess, cl_process },
    { "WaitForSingleObject", "process", 1 << 30, init_wait,  fini_wait, NULL,        op_wait,          NULL },
    { "VirtualAlloc",        "memory",  1 << 30, NULL,       NULL,      NULL,        op_valloc,        cl_vfree },
    { "VirtualFree",         "memory",  1 << 30, NULL,       NULL,      pre_valloc,  op_vfree,         NULL },
    { "CreatePipe",          "ipc",     1 << 30, NULL,       NULL,      NULL,        op_createpipe,    cl_pipe },
    { "CreateFileMapping",   "ipc",     1 << 30, NULL,       NULL,      NULL,        op_createmap,     cl_closeh },
    { "MapViewOfFile",       "ipc",     1 << 30, init_map,   fini_map,  NULL,        op_mapview,       cl_unmap },
    { "SetFileSecurity",     "perms",   1 << 30, init_sec,   fini_sec,  NULL,        op_setsec,        NULL },
};
#define NB ((int)(sizeof BENCHES / sizeof BENCHES[0]))

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

typedef struct {
    int n;
    double min, median, p95, mean, max, cpu_ms;
    long long ws_delta_kb;
    size_t peak_ws_kb;
} Result;

static bool run_bench(const Bench *b, int iters, double *s, Result *r) {
    if (b->init && !b->init()) {
        log_winerr(b->name, "setup");
        return false;
    }
    int n = iters < b->max_iters ? iters : b->max_iters;
    int warm = n / 10;
    if (warm < 5) warm = 5;
    if (warm > 100) warm = 100;
    bool ok = true;

    for (int i = 0; i < warm && ok; i++) { /* warm-up, not timed */
        if (b->prepare && !b->prepare()) { log_winerr(b->name, "warm-up prepare"); ok = false; break; }
        if (!b->op()) { log_winerr(b->name, "warm-up"); ok = false; break; }
        if (b->cleanup) b->cleanup();
    }

    ResUsage u0, u1;
    sc_get_usage(&u0);
    for (int i = 0; i < n && ok; i++) {
        if (b->prepare && !b->prepare()) { log_winerr(b->name, "prepare"); ok = false; break; }
        double t0 = sc_now_us();
        bool good = b->op();
        double t1 = sc_now_us();
        if (!good) { log_winerr(b->name, "timed call"); ok = false; break; }
        s[i] = t1 - t0;
        if (b->cleanup) b->cleanup();
    }
    sc_get_usage(&u1);
    if (b->fini) b->fini();
    if (!ok) return false;

    qsort(s, (size_t)n, sizeof(double), cmp_d);
    double sum = 0;
    for (int i = 0; i < n; i++) sum += s[i];
    r->n = n;
    r->min = s[0];
    r->median = (n % 2) ? s[n / 2] : 0.5 * (s[n / 2 - 1] + s[n / 2]);
    r->p95 = s[(size_t)(0.95 * (n - 1) + 0.5)];
    r->mean = sum / n;
    r->max = s[n - 1];
    r->cpu_ms = (u1.user_ms + u1.kernel_ms) - (u0.user_ms + u0.kernel_ms);
    r->ws_delta_kb = (long long)u1.ws_kb - (long long)u0.ws_kb;
    r->peak_ws_kb = u1.peak_ws_kb;
    return true;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--noop") == 0) return 0; /* the child that CreateProcess starts */

    int iters = argc > 1 ? atoi(argv[1]) : 1000;
    const char *csv_path = argc > 2 ? argv[2] : "results\\bench_windows.csv";
    if (iters < 10) iters = 10;

    log_open("logs\\bench_win.log", true);
    log_set_level(LOG_WARN); /* the benchmark calls the API directly; only problems are logged */

    char self[MAX_PATH], sys[400];
    if (!proc_self_path(self, sizeof self)) return 1;
    snprintf(SELF_CMD, sizeof SELF_CMD, "\"%s\" --noop", self);
    sc_sysinfo(sys, sizeof sys);
    printf("Machine: %s\nIterations per call: %d (CreateProcess: at most 200)\n\n", sys, iters);

    CreateDirectoryA("results", NULL);
    if (!make_file(FILE_RD, 64 * 1024)) return 1;
    double *samples = (double *)mem_alloc(sizeof(double) * (size_t)iters);
    if (!samples) return 1;

    char csv[8192];
    int len = snprintf(csv, sizeof csv,
                       "os,call,category,iterations,min_us,median_us,p95_us,mean_us,max_us,cpu_ms,ws_delta_kb,peak_ws_kb\r\n");
    printf("%-20s %7s %10s %10s %10s %10s %10s %8s\n", "call", "n", "min(us)", "median", "p95", "mean", "max", "cpu(ms)");

    int failures = 0;
    for (int i = 0; i < NB; i++) {
        Result r;
        if (!run_bench(&BENCHES[i], iters, samples, &r)) {
            printf("%-20s FAILED (see logs\\bench_win.log)\n", BENCHES[i].name);
            failures++;
            continue;
        }
        printf("%-20s %7d %10.3f %10.3f %10.3f %10.3f %10.3f %8.1f\n", BENCHES[i].name, r.n, r.min, r.median, r.p95,
               r.mean, r.max, r.cpu_ms);
        len += snprintf(csv + len, sizeof csv - (size_t)len, "windows,%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%lld,%zu\r\n",
                        BENCHES[i].name, BENCHES[i].category, r.n, r.min, r.median, r.p95, r.mean, r.max, r.cpu_ms,
                        r.ws_delta_kb, r.peak_ws_kb);
    }

    HANDLE o = fio_open_write(csv_path);
    if (o != INVALID_HANDLE_VALUE) {
        fio_write_exact(o, csv, (size_t)len);
        fio_close(o);
        printf("\nResults written to %s\n", csv_path);
    } else {
        failures++;
    }
    DeleteFileA(FILE_RD);
    DeleteFileA(FILE_WR);
    mem_free(samples);
    log_close();
    return failures ? 1 : 0;
}
