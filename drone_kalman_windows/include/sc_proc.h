/* sc_proc.h - Process: CreateProcess, WaitForSingleObject.
 * Windows has no fork(): CreateProcess does fork+exec in one step. */
#ifndef SC_PROC_H
#define SC_PROC_H
#include "sc_platform.h"

/* child_out: write end of a pipe to use as the child's stdout/stderr, or NULL
 * to let the child use our console. On success *proc is a process handle the
 * caller must CloseHandle (the thread handle is closed here). */
bool proc_spawn(const char *cmdline, HANDLE child_out, HANDLE *proc, DWORD *pid);

/* Waits up to ms (INFINITE allowed) and returns the exit code via *exit_code. */
bool proc_wait(HANDLE proc, DWORD ms, DWORD *exit_code);

bool proc_self_path(char *buf, size_t n);   /* GetModuleFileName */

#endif
