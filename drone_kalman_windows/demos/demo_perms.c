/* demo_perms.c - SetFileSecurity (Linux: chmod, chown, umask). NTFS drives only. */
#include "sc_platform.h"
#include "sc_log.h"
#include "sc_perm.h"
#include <stdio.h>
#include <string.h>

static bool try_open_for_write(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("  open for write: DENIED (error %lu)\n", (unsigned long)GetLastError());
        return false;
    }
    CloseHandle(h);
    printf("  open for write: allowed\n");
    return true;
}

int main(void) {
    log_open("logs\\demo_perms.log", true);
    const char *path = "demo_perms.txt";
    char sddl[1024];
    int failed = 0;

    printf("=== Permissions demo (SetFileSecurity) ===\n");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { log_winerr("CreateFile", path); return 1; }
    CloseHandle(h);

    if (perm_dump_sddl(path, sddl, sizeof sddl)) printf("Start      : %s\n", sddl);

    printf("\n1. chmod 444 analogue: DACL = Everyone read-only\n");
    if (!perm_set_readonly(path)) {
        printf("  SetFileSecurity failed. Is this an NTFS drive? (FAT/exFAT have no ACLs)\n");
        DeleteFileA(path);
        return 1;
    }
    if (perm_dump_sddl(path, sddl, sizeof sddl)) printf("  now        : %s\n", sddl);
    if (try_open_for_write(path)) { /* must be denied on NTFS */
        printf("  Expected DENIED on a real NTFS drive. Check that this folder is on NTFS, not FAT/exFAT or a Wine/network drive.\n");
        failed = 1;
    }

    printf("\n2. chmod 666 analogue: DACL = Everyone full control\n");
    if (!perm_set_full(path)) failed = 1;
    failed |= !try_open_for_write(path);

    printf("\n3. chown analogue: owner = current user\n");
    if (!perm_set_owner_to_self(path)) {
        printf("  could not change owner (needs the right to set this owner); see logs\\demo_perms.log\n");
    } else if (perm_dump_sddl(path, sddl, sizeof sddl)) {
        printf("  now        : %s\n", sddl);
    }

    printf("\n4. umask analogue: none. New files inherit the DACL of their folder.\n");

    if (!DeleteFileA(path)) log_winerr("DeleteFile", path);
    printf("\nResult: %s\n", failed ? "FAILED" : "OK");
    log_close();
    return failed;
}
