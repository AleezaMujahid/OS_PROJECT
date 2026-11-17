/* sc_ipc.h - IPC: CreatePipe, CreateFileMapping, MapViewOfFile. */
#ifndef SC_IPC_H
#define SC_IPC_H
#include "sc_platform.h"

typedef struct {
    HANDLE h;        /* file-mapping handle */
    void  *view;     /* mapped view         */
    size_t size;     /* 0 when opened by name (whole mapping is mapped) */
    char   name[96];
} ShMem;

/* Pagefile-backed named shared memory (Linux counterpart: shmget + shmat). */
bool shm_create(ShMem *m, const char *name, size_t size);  /* CreateFileMapping + MapViewOfFile */
bool shm_open_existing(ShMem *m, const char *name);        /* OpenFileMapping   + MapViewOfFile */
void shm_close(ShMem *m);                                  /* UnmapViewOfFile + CloseHandle     */

/* Anonymous pipe. Only the write end is inheritable (Linux counterpart: pipe). */
bool   pipe_make(HANDLE *read_end, HANDLE *write_end);     /* CreatePipe */
/* Reads until every writer closed the pipe. Stores at most cap-1 bytes, NUL-terminates.
 * Extra data is drained and dropped so the child never blocks. Returns bytes stored. */
size_t pipe_read_all(HANDLE read_end, char *buf, size_t cap);

#endif
