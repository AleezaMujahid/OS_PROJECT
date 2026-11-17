/* demo_process.c - CreateProcess, WaitForSingleObject (Linux: fork, exec, wait).
 * Windows has no fork(): CreateProcess creates the process AND loads the program. */
#include "sc_platform.h"
#include "sc_log.h"
#include "sc_proc.h"
#include "sc_time.h"
#include <stdio.h>

int main(void) {
    log_open("logs\\demo_process.log", true);
    printf("=== Process demo (CreateProcess / WaitForSingleObject) ===\n");
    printf("Parent pid: %lu\n", (unsigned long)GetCurrentProcessId());
    printf("Note: Windows has no fork(); CreateProcess = fork() + exec() in one call.\n\n");

    HANDLE child = NULL;
    DWORD pid = 0, code = 0;

    double t0 = sc_now_us();
    if (!proc_spawn("cmd.exe /c \"echo Hello from the child process & exit 7\"", NULL, &child, &pid)) return 1;
    double t1 = sc_now_us();
    printf("Child pid : %lu (created in %.1f us)\n", (unsigned long)pid, t1 - t0);

    if (!proc_wait(child, INFINITE, &code)) return 1;       /* blocks until the child exits */
    double t2 = sc_now_us();
    printf("Child exit code: %lu (expected 7), waited %.1f us\n", (unsigned long)code, t2 - t1);

    CloseHandle(child);
    log_close();
    return code == 7 ? 0 : 1;
}
