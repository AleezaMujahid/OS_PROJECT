/* sc_log.h - logging and error reporting for every system call.
 * The log file is written with CreateFile/WriteFile (no stdio), one WriteFile
 * per line, opened with FILE_APPEND_DATA so several processes can share it. */
#ifndef SC_LOG_H
#define SC_LOG_H
#include "sc_platform.h"

typedef enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 } LogLevel;

/* path may be NULL (console only). to_console writes to stderr. */
bool log_open(const char *path, bool to_console);
void log_close(void);
void log_set_level(LogLevel min_level);
void log_msg(LogLevel lvl, const char *fmt, ...) SC_PRINTF(2, 3);

/* Logs "<api> failed: error <code> (<system message>) [<context>]" using GetLastError(). */
void log_winerr(const char *api, const char *context);

#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO, __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN, __VA_ARGS__)
#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)

#endif
