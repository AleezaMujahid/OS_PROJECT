/* sc_fileio.h - File I/O: CreateFile, ReadFile, WriteFile, CloseHandle. */
#ifndef SC_FILEIO_H
#define SC_FILEIO_H
#include "sc_platform.h"

HANDLE fio_open_read(const char *path);             /* CreateFile, OPEN_EXISTING */
HANDLE fio_open_write(const char *path);            /* CreateFile, CREATE_ALWAYS */
bool   fio_get_size(HANDLE h, uint64_t *size);      /* GetFileSizeEx (helper)    */
bool   fio_read_exact(HANDLE h, void *buf, size_t n);        /* ReadFile loop  */
bool   fio_write_exact(HANDLE h, const void *buf, size_t n); /* WriteFile loop */
bool   fio_close(HANDLE h);                         /* CloseHandle               */

#endif
