/* demo_memory.c - VirtualAlloc, VirtualFree (Linux: mmap, munmap).
 * Shows the difference between reserving address space and committing memory. */
#include "sc_platform.h"
#include "sc_log.h"
#include "sc_mem.h"
#include "sc_time.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    log_open("logs\\demo_memory.log", true);
    printf("=== Memory demo (VirtualAlloc / VirtualFree) ===\n");

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    printf("Page size: %lu bytes, allocation granularity: %lu bytes\n\n",
           (unsigned long)si.dwPageSize, (unsigned long)si.dwAllocationGranularity);

    /* 1. reserve + commit in one call, like an anonymous mmap */
    const size_t size = 1u << 20; /* 1 MiB */
    double t0 = sc_now_us();
    unsigned char *p = mem_alloc(size);
    double t1 = sc_now_us();
    if (!p) return 1;
    memset(p, 0xAB, size);
    int ok = 1;
    for (size_t i = 0; i < size; i += 4096) ok &= (p[i] == 0xAB);
    double t2 = sc_now_us();
    printf("1 MiB reserve+commit: %.1f us, write+verify: %.1f us, content ok: %s\n",
           t1 - t0, t2 - t1, ok ? "YES" : "NO");
    if (!mem_free(p)) return 1;
    double t3 = sc_now_us();
    printf("VirtualFree: %.1f us\n\n", t3 - t2);

    /* 2. reserve a large range, commit only a part of it */
    const size_t big = 64u << 20; /* 64 MiB of address space */
    unsigned char *r = mem_reserve(big);
    if (!r) return 1;
    printf("Reserved 64 MiB of address space (no physical memory used yet).\n");
    printf("Touching it now would crash: it is not committed.\n");
    if (!mem_commit(r, 64 * 1024)) { mem_free(r); return 1; }
    memset(r, 0x5A, 64 * 1024);
    printf("Committed the first 64 KiB and wrote to it: first byte = 0x%02X\n", r[0]);
    ok &= (r[0] == 0x5A);
    if (!mem_free(r)) return 1;

    log_close();
    return ok ? 0 : 1;
}
