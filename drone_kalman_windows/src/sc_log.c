#include "sc_platform.h"
#include "sc_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HANDLE   g_file = INVALID_HANDLE_VALUE;
static bool     g_console = true;
static LogLevel g_min = LOG_INFO;
static const char *LVL[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };

static HANDLE open_append(const char *path) {
    return CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

bool log_open(const char *path, bool to_console) {
    g_console = to_console;
    if (!path) return true;
    g_file = open_append(path);
    if (g_file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND) {
        /* create the parent folder (one level) and retry */
        char dir[MAX_PATH];
        snprintf(dir, sizeof dir, "%s", path);
        char *slash = strrchr(dir, '\\');
        if (!slash) slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            CreateDirectoryA(dir, NULL);
            g_file = open_append(path);
        }
    }
    if (g_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "log_open: cannot open %s (error %lu)\n", path, (unsigned long)GetLastError());
        return false;
    }
    return true;
}

void log_close(void) {
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
}

void log_set_level(LogLevel min_level) { g_min = min_level; }

void log_msg(LogLevel lvl, const char *fmt, ...) {
    if (lvl < g_min) return;
    char body[900], line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);

    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = snprintf(line, sizeof line, "%02d:%02d:%02d.%03d [pid %lu] %s %s\r\n",
                     t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                     (unsigned long)GetCurrentProcessId(), LVL[lvl], body);
    if (n < 0) return;
    if ((size_t)n >= sizeof line) {
        n = (int)sizeof line - 1;
        line[n - 2] = '\r';
        line[n - 1] = '\n';
    }
    DWORD w;
    if (g_file != INVALID_HANDLE_VALUE) WriteFile(g_file, line, (DWORD)n, &w, NULL);
    if (g_console) {
        HANDLE e = GetStdHandle(STD_ERROR_HANDLE);
        if (e && e != INVALID_HANDLE_VALUE) WriteFile(e, line, (DWORD)n, &w, NULL);
    }
}

void log_winerr(const char *api, const char *context) {
    DWORD code = GetLastError();
    char msg[256] = "";
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0,
                   msg, sizeof msg, NULL);
    size_t len = strlen(msg);
    while (len && (msg[len - 1] == '\r' || msg[len - 1] == '\n' || msg[len - 1] == ' ')) msg[--len] = '\0';
    log_msg(LOG_ERROR, "%s failed: error %lu (%s) [%s]", api, (unsigned long)code, msg,
            context ? context : "");
}
