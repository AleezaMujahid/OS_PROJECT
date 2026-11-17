#include "sc_platform.h"
#include "sc_perm.h"
#include "sc_log.h"
#include <sddl.h>
#include <stdio.h>
#include <string.h>

bool perm_prepare(PermSD *p, DWORD mask) {
    memset(p, 0, sizeof *p);
    DWORD sidsz = sizeof p->sid;
    if (!CreateWellKnownSid(WinWorldSid, NULL, p->sid, &sidsz)) {
        log_winerr("CreateWellKnownSid", "Everyone");
        return false;
    }
    PACL acl = (PACL)p->acl;
    if (!InitializeAcl(acl, sizeof p->acl, ACL_REVISION)) {
        log_winerr("InitializeAcl", "");
        return false;
    }
    if (!AddAccessAllowedAce(acl, ACL_REVISION, mask, p->sid)) {
        log_winerr("AddAccessAllowedAce", "");
        return false;
    }
    if (!InitializeSecurityDescriptor(&p->sd, SECURITY_DESCRIPTOR_REVISION)) {
        log_winerr("InitializeSecurityDescriptor", "");
        return false;
    }
    if (!SetSecurityDescriptorDacl(&p->sd, TRUE, acl, FALSE)) {
        log_winerr("SetSecurityDescriptorDacl", "");
        return false;
    }
    return true;
}

static bool set_dacl(const char *path, DWORD mask, const char *what) {
    static PermSD p; /* static: the descriptor points into itself */
    if (!perm_prepare(&p, mask)) return false;
    if (!SetFileSecurityA(path, DACL_SECURITY_INFORMATION, &p.sd)) {
        log_winerr("SetFileSecurity(DACL)", path);
        return false;
    }
    LOGI("SetFileSecurity ok: %s -> %s", path, what);
    return true;
}

bool perm_set_readonly(const char *path) {
    return set_dacl(path, FILE_GENERIC_READ, "Everyone: read only");
}

bool perm_set_full(const char *path) {
    return set_dacl(path, FILE_ALL_ACCESS, "Everyone: full control");
}

bool perm_set_owner_to_self(const char *path) {
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        log_winerr("OpenProcessToken", "");
        return false;
    }
    union { TOKEN_USER tu; BYTE raw[256]; } u;
    DWORD need = 0;
    BOOL ok = GetTokenInformation(tok, TokenUser, &u, sizeof u, &need);
    CloseHandle(tok);
    if (!ok) {
        log_winerr("GetTokenInformation", "TokenUser");
        return false;
    }
    SECURITY_DESCRIPTOR sd;
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorOwner(&sd, u.tu.User.Sid, FALSE)) {
        log_winerr("SetSecurityDescriptorOwner", "");
        return false;
    }
    if (!SetFileSecurityA(path, OWNER_SECURITY_INFORMATION, &sd)) {
        log_winerr("SetFileSecurity(owner)", path);
        return false;
    }
    LOGI("SetFileSecurity ok: %s -> owner = current user", path);
    return true;
}

bool perm_dump_sddl(const char *path, char *out, size_t n) {
    DWORD buf[512]; /* self-relative security descriptor, DWORD aligned */
    DWORD need = 0;
    const DWORD info = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    if (!GetFileSecurityA(path, info, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need)) {
        log_winerr("GetFileSecurity", path);
        return false;
    }
    LPSTR text = NULL;
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorA((PSECURITY_DESCRIPTOR)buf, SDDL_REVISION_1,
                                                              info, &text, NULL)) {
        log_winerr("ConvertSecurityDescriptorToStringSecurityDescriptor", path);
        return false;
    }
    snprintf(out, n, "%s", text);
    LocalFree(text);
    return true;
}
