#include "sc_platform.h"
#include "sc_ipc.h"
#include "sc_log.h"
#include <stdio.h>
#include <string.h>

static bool map_view(ShMem *m) {
    m->view = MapViewOfFile(m->h, FILE_MAP_ALL_ACCESS, 0, 0, 0); /* 0 = map the whole mapping */
    if (!m->view) {
        log_winerr("MapViewOfFile", m->name);
        CloseHandle(m->h);
        m->h = NULL;
        return false;
    }
    return true;
}

bool shm_create(ShMem *m, const char *name, size_t size) {
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    uint64_t s = (uint64_t)size;
    m->h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                              (DWORD)(s >> 32), (DWORD)(s & 0xFFFFFFFFu), name);
    if (!m->h) {
        log_winerr("CreateFileMapping", name);
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) LOGW("file mapping %s already existed", name);
    if (!map_view(m)) return false;
    m->size = size;
    LOGI("CreateFileMapping + MapViewOfFile ok: %s, %zu bytes", name, size);
    return true;
}

bool shm_open_existing(ShMem *m, const char *name) {
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    m->h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!m->h) {
        log_winerr("OpenFileMapping", name);
        return false;
    }
    if (!map_view(m)) return false;
    LOGI("OpenFileMapping + MapViewOfFile ok: %s", name);
    return true;
}

void shm_close(ShMem *m) {
    if (m->view && !UnmapViewOfFile(m->view)) log_winerr("UnmapViewOfFile", m->name);
    if (m->h && !CloseHandle(m->h)) log_winerr("CloseHandle", m->name);
    m->view = NULL;
    m->h = NULL;
}

bool pipe_make(HANDLE *rd, HANDLE *wr) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE; /* both ends inheritable at first ... */
    if (!CreatePipe(rd, wr, &sa, 0)) {
        log_winerr("CreatePipe", "");
        return false;
    }
    /* ... then make the read end private to the parent */
    if (!SetHandleInformation(*rd, HANDLE_FLAG_INHERIT, 0)) {
        log_winerr("SetHandleInformation", "read end");
        CloseHandle(*rd);
        CloseHandle(*wr);
        return false;
    }
    LOGI("CreatePipe ok");
    return true;
}

size_t pipe_read_all(HANDLE rd, char *buf, size_t cap) {
    size_t used = 0;
    char sink[256];
    for (;;) {
        DWORD got = 0;
        bool store = (used + 1 < cap);
        char *dst = store ? buf + used : sink;
        DWORD want = store ? (DWORD)(cap - 1 - used) : (DWORD)sizeof sink;
        if (!ReadFile(rd, dst, want, &got, NULL)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) log_winerr("ReadFile(pipe)", "");
            break; /* ERROR_BROKEN_PIPE = all writers closed = normal end */
        }
        if (got == 0) break;
        if (store) used += got;
    }
    if (cap) buf[used] = '\0';
    return used;
}
