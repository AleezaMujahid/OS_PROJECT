/* sc_platform.h - must be the FIRST include of every .c file.
 * Windows implementation of the CT-353 CCP (drone tracking + Kalman Filter). */
#ifndef SC_PLATFORM_H
#define SC_PLATFORM_H

#ifndef _WIN32
#error "This folder is the Windows implementation. Build it with MinGW-w64 (native Windows or cross-compiler)."
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 or newer */
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1 /* makes %zu, %llu, snprintf behave like C99 */
#endif

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* printf-style format checking for our logging functions */
#if defined(__GNUC__)
#define SC_PRINTF(a, b) __attribute__((format(gnu_printf, a, b)))
#else
#define SC_PRINTF(a, b)
#endif

#endif /* SC_PLATFORM_H */
