/* sc_mem.h - Memory: VirtualAlloc, VirtualFree. */
#ifndef SC_MEM_H
#define SC_MEM_H
#include "sc_platform.h"

void *mem_alloc(size_t n);            /* VirtualAlloc(MEM_RESERVE|MEM_COMMIT) */
void *mem_reserve(size_t n);          /* VirtualAlloc(MEM_RESERVE) only        */
bool  mem_commit(void *p, size_t n);  /* VirtualAlloc(MEM_COMMIT) on reserved  */
bool  mem_free(void *p);              /* VirtualFree(MEM_RELEASE)              */

#endif
