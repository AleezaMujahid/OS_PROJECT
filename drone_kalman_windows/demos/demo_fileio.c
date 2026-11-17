/* demo_fileio.c - CreateFile, WriteFile, ReadFile, CloseHandle (Linux: open, write, read, close). */
#include "sc_platform.h"
#include "sc_fileio.h"
#include "sc_log.h"
#include "sc_time.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    log_open("logs\\demo_fileio.log", true);
    const char *path = "demo_fileio.txt";
    const char msg[] = "Hello from the Windows file I/O demo\r\n";
    char back[128] = {0};
    double t[6];

    printf("=== File I/O demo (CreateFile / WriteFile / ReadFile / CloseHandle) ===\n");

    t[0] = sc_now_us();
    HANDLE h = fio_open_write(path);                        /* CreateFile */
    t[1] = sc_now_us();
    if (h == INVALID_HANDLE_VALUE) return 1;
    bool ok = fio_write_exact(h, msg, sizeof msg - 1);      /* WriteFile  */
    t[2] = sc_now_us();
    ok = fio_close(h) && ok;                                /* CloseHandle */
    t[3] = sc_now_us();
    if (!ok) return 1;

    h = fio_open_read(path);                                /* CreateFile */
    if (h == INVALID_HANDLE_VALUE) return 1;
    uint64_t size = 0;
    ok = fio_get_size(h, &size) && size < sizeof back;
    t[4] = sc_now_us();
    ok = ok && fio_read_exact(h, back, (size_t)size);       /* ReadFile   */
    t[5] = sc_now_us();
    fio_close(h);
    if (!ok) return 1;

    printf("Wrote %zu bytes, read back %llu bytes: %s", sizeof msg - 1, (unsigned long long)size, back);
    printf("Same content: %s\n", strcmp(msg, back) == 0 ? "YES" : "NO");
    printf("\nTiming of one call (includes logging, so only indicative; use bench_win.exe for real numbers):\n");
    printf("  CreateFile  : %8.1f us\n  WriteFile   : %8.1f us\n  CloseHandle : %8.1f us\n  ReadFile    : %8.1f us\n",
           t[1] - t[0], t[2] - t[1], t[3] - t[2], t[5] - t[4]);

    if (!DeleteFileA(path)) log_winerr("DeleteFile", path);
    log_close();
    return strcmp(msg, back) == 0 ? 0 : 1;
}
