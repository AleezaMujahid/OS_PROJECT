/* sc_perm.h - Permissions: SetFileSecurity.
 *
 * Linux has three calls here, Windows has one:
 *   chmod() -> SetFileSecurity(DACL_SECURITY_INFORMATION)  : who may read/write
 *   chown() -> SetFileSecurity(OWNER_SECURITY_INFORMATION) : who owns the file
 *   umask() -> no equivalent: new files get the DACL inherited from the parent
 *              folder (or the SECURITY_ATTRIBUTES passed to CreateFile).
 * NTFS only. FAT/exFAT drives have no ACLs and SetFileSecurity fails there. */
#ifndef SC_PERM_H
#define SC_PERM_H
#include "sc_platform.h"

/* A prepared security descriptor with one ACE for "Everyone".
 * Do not copy or move it after perm_prepare(): the descriptor points into itself. */
typedef struct {
    SECURITY_DESCRIPTOR sd;
    BYTE  sid[SECURITY_MAX_SID_SIZE];
    DWORD acl[32];
} PermSD;

bool perm_prepare(PermSD *p, DWORD access_mask);   /* builds the SD (no file touched) */

bool perm_set_readonly(const char *path);          /* chmod 444 analogue */
bool perm_set_full(const char *path);              /* chmod 666 analogue */
bool perm_set_owner_to_self(const char *path);     /* chown analogue     */
bool perm_dump_sddl(const char *path, char *out, size_t n); /* GetFileSecurity, as text */

#endif
