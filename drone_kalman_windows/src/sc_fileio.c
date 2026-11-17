#include "sc_platform.h"
#include "sc_fileio.h"
#include "sc_log.h"

HANDLE fio_open_read(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) log_winerr("CreateFile(read)", path);
    else LOGI("CreateFile(read) ok: %s", path);
    return h;
}

HANDLE fio_open_write(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) log_winerr("CreateFile(write)", path);
    else LOGI("CreateFile(write) ok: %s", path);
    return h;
}

bool fio_get_size(HANDLE h, uint64_t *size) {
    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li)) {
        log_winerr("GetFileSizeEx", "");
        return false;
    }
    *size = (uint64_t)li.QuadPart;
    return true;
}

#define CHUNK ((size_t)1 << 30) /* ReadFile/WriteFile take a DWORD: do big transfers in pieces */

bool fio_read_exact(HANDLE h, void *buf, size_t n) {
    char *p = (char *)buf;
    size_t left = n;
    while (left) {
        DWORD want = (DWORD)(left > CHUNK ? CHUNK : left), got = 0;
        if (!ReadFile(h, p, want, &got, NULL)) {
            log_winerr("ReadFile", "");
            return false;
        }
        if (got == 0) {
            LOGE("ReadFile: unexpected end of file, %zu bytes missing", left);
            return false;
        }
        p += got;
        left -= got;
    }
    LOGI("ReadFile ok: %zu bytes", n);
    return true;
}

bool fio_write_exact(HANDLE h, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t left = n;
    while (left) {
        DWORD want = (DWORD)(left > CHUNK ? CHUNK : left), put = 0;
        if (!WriteFile(h, p, want, &put, NULL)) {
            log_winerr("WriteFile", "");
            return false;
        }
        p += put;
        left -= put;
    }
    LOGI("WriteFile ok: %zu bytes", n);
    return true;
}

bool fio_close(HANDLE h) {
    if (!CloseHandle(h)) {
        log_winerr("CloseHandle", "");
        return false;
    }
    LOGI("CloseHandle ok");
    return true;
}
