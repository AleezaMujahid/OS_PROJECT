/* sc_time.h - timing (QueryPerformanceCounter) and resource usage. */
#ifndef SC_TIME_H
#define SC_TIME_H
#include "sc_platform.h"

/* Monotonic time in microseconds (Linux counterpart: clock_gettime(CLOCK_MONOTONIC)). */
double sc_now_us(void);

typedef struct {
    double user_ms;     /* CPU time in user mode   (GetProcessTimes) */
    double kernel_ms;   /* CPU time in kernel mode (GetProcessTimes) */
    size_t ws_kb;       /* current working set     (GetProcessMemoryInfo) */
    size_t peak_ws_kb;  /* peak working set        (GetProcessMemoryInfo) */
} ResUsage;

bool sc_get_usage(ResUsage *out);

/* One line describing CPU, logical CPUs, RAM and Windows version (for the specs table). */
void sc_sysinfo(char *buf, size_t n);

#endif
