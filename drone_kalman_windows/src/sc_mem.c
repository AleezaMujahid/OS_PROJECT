#include "sc_platform.h"
#include "sc_mem.h"
#include "sc_log.h"

void *mem_alloc(size_t n) {
    void *p = VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) {
        log_winerr("VirtualAlloc", "commit");
        return NULL;
    }
    LOGI("VirtualAlloc ok: %zu bytes at %p", n, p);
    return p;
}

void *mem_reserve(size_t n) {
    void *p = VirtualAlloc(NULL, n, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) {
        log_winerr("VirtualAlloc", "reserve");
        return NULL;
    }
    LOGI("VirtualAlloc(reserve) ok: %zu bytes at %p", n, p);
    return p;
}

bool mem_commit(void *p, size_t n) {
    if (!VirtualAlloc(p, n, MEM_COMMIT, PAGE_READWRITE)) {
        log_winerr("VirtualAlloc", "commit part of reserved range");
        return false;
    }
    LOGI("VirtualAlloc(commit) ok: %zu bytes at %p", n, p);
    return true;
}

bool mem_free(void *p) {
    if (!p) return true;
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
        log_winerr("VirtualFree", "");
        return false;
    }
    LOGI("VirtualFree ok: %p", p);
    return true;
}
