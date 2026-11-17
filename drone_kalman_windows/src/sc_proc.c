#include "sc_platform.h"
#include "sc_proc.h"
#include "sc_log.h"
#include <string.h>

bool proc_spawn(const char *cmdline, HANDLE child_out, HANDLE *proc, DWORD *pid) {
    char cmd[2048]; /* CreateProcess may modify the command-line buffer */
    if (strlen(cmdline) >= sizeof cmd) {
        LOGE("CreateProcess: command line too long");
        return false;
    }
    strcpy(cmd, cmdline);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    if (child_out) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = NULL;
        si.hStdOutput = child_out;
        si.hStdError = child_out;
    }
    if (!CreateProcessA(NULL, cmd, NULL, NULL, child_out ? TRUE : FALSE, 0, NULL, NULL, &si, &pi)) {
        log_winerr("CreateProcess", cmdline);
        return false;
    }
    CloseHandle(pi.hThread);
    *proc = pi.hProcess;
    if (pid) *pid = pi.dwProcessId;
    LOGI("CreateProcess ok: child pid %lu", (unsigned long)pi.dwProcessId);
    return true;
}

bool proc_wait(HANDLE proc, DWORD ms, DWORD *exit_code) {
    DWORD r = WaitForSingleObject(proc, ms);
    if (r == WAIT_FAILED) {
        log_winerr("WaitForSingleObject", "");
        return false;
    }
    if (r == WAIT_TIMEOUT) {
        LOGW("WaitForSingleObject timed out after %lu ms", (unsigned long)ms);
        return false;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(proc, &code)) {
        log_winerr("GetExitCodeProcess", "");
        return false;
    }
    if (exit_code) *exit_code = code;
    LOGI("WaitForSingleObject ok: process exited with code %lu", (unsigned long)code);
    return true;
}

bool proc_self_path(char *buf, size_t n) {
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)n);
    if (len == 0 || len >= n) {
        log_winerr("GetModuleFileName", "");
        return false;
    }
    return true;
}
