/* demo_ipc.c - CreatePipe, CreateFileMapping, MapViewOfFile
 * (Linux: pipe, shmget, shmat).  Run with no arguments. */
#include "sc_platform.h"
#include "sc_ipc.h"
#include "sc_log.h"
#include "sc_proc.h"
#include <stdio.h>
#include <string.h>

/* Child side of part 2: open the shared memory by name, read the parent's text, add a reply. */
static int child_main(const char *name) {
    log_open("logs\\demo_ipc.log", false);
    ShMem m;
    if (!shm_open_existing(&m, name)) return 2;
    char *text = (char *)m.view;
    char reply[256];
    snprintf(reply, sizeof reply, "%s -> child (pid %lu) says: got it!", text, (unsigned long)GetCurrentProcessId());
    snprintf(text, 4096, "%s", reply);
    shm_close(&m);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--child") == 0) return child_main(argv[2]);

    log_open("logs\\demo_ipc.log", true);
    printf("=== IPC demo (CreatePipe / CreateFileMapping / MapViewOfFile) ===\n\n");
    int failed = 0;

    /* ---- Part 1: anonymous pipe, child writes, parent reads ---- */
    printf("Part 1: anonymous pipe\n");
    HANDLE rd, wr, child;
    if (!pipe_make(&rd, &wr)) return 1;
    if (!proc_spawn("cmd.exe /c echo message sent through the pipe", wr, &child, NULL)) return 1;
    CloseHandle(wr); /* parent must close its copy of the write end, or the read never ends */
    char buf[256];
    pipe_read_all(rd, buf, sizeof buf);
    DWORD code = 1;
    proc_wait(child, INFINITE, &code);
    CloseHandle(child);
    CloseHandle(rd);
    printf("  parent received: %s", buf);
    failed |= (code != 0) || (strstr(buf, "message sent") == NULL);

    /* ---- Part 2: named shared memory between two processes ---- */
    printf("\nPart 2: shared memory (file mapping)\n");
    char name[64], self[MAX_PATH], cmd[MAX_PATH + 128];
    snprintf(name, sizeof name, "Local\\demo_ipc_%lu", (unsigned long)GetCurrentProcessId());
    ShMem m;
    if (!shm_create(&m, name, 4096)) return 1;
    snprintf((char *)m.view, 4096, "hello from parent (pid %lu)", (unsigned long)GetCurrentProcessId());
    printf("  parent wrote : %s\n", (char *)m.view);

    if (!proc_self_path(self, sizeof self)) return 1;
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s", self, name);
    if (!proc_spawn(cmd, NULL, &child, NULL)) return 1;
    code = 1;
    proc_wait(child, INFINITE, &code);
    CloseHandle(child);
    printf("  after child  : %s\n", (char *)m.view);
    failed |= (code != 0) || (strstr((char *)m.view, "child") == NULL);
    shm_close(&m);

    printf("\nResult: %s\n", failed ? "FAILED" : "OK");
    log_close();
    return failed;
}
