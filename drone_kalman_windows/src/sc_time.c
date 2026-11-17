#include "sc_platform.h"
#include "sc_time.h"
#include "sc_log.h"
#include <psapi.h>
#include <stdio.h>

double sc_now_us(void) {
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e6 / (double)freq.QuadPart;
}

static double ft_ms(FILETIME f) {
    ULARGE_INTEGER u;
    u.LowPart = f.dwLowDateTime;
    u.HighPart = f.dwHighDateTime;
    return (double)u.QuadPart / 10000.0; /* 100 ns units -> ms */
}

bool sc_get_usage(ResUsage *u) {
    FILETIME c, e, k, us;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &us)) {
        log_winerr("GetProcessTimes", "");
        return false;
    }
    u->user_ms = ft_ms(us);
    u->kernel_ms = ft_ms(k);
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
        log_winerr("GetProcessMemoryInfo", "");
        return false;
    }
    u->ws_kb = pmc.WorkingSetSize / 1024;
    u->peak_ws_kb = pmc.PeakWorkingSetSize / 1024;
    return true;
}

static void reg_str(const char *key, const char *value, char *out, size_t n) {
    DWORD sz = (DWORD)n;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, key, value, RRF_RT_REG_SZ, NULL, out, &sz) != ERROR_SUCCESS)
        snprintf(out, n, "unknown");
}

void sc_sysinfo(char *buf, size_t n) {
    char cpu[128], prod[128], disp[64], build[32];
    reg_str("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", cpu, sizeof cpu);
    reg_str("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", prod, sizeof prod);
    reg_str("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion", disp, sizeof disp);
    reg_str("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", build, sizeof build);
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    snprintf(buf, n, "CPU=%s | logical_cpus=%lu | RAM=%llu MB | OS=%s %s (build %s)",
             cpu, (unsigned long)si.dwNumberOfProcessors,
             (unsigned long long)(ms.ullTotalPhys / (1024 * 1024)), prod, disp, build);
}
