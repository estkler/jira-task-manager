#ifndef TASKLET_TOKEN_INFO_H
#define TASKLET_TOKEN_INFO_H
#include <windows.h>
#include <wchar.h>
/* Retire obsolete local display metadata without reading any credentials. */
static BOOL NativeClearLegacyTokenLabels(const wchar_t *path)
{
    return path&&*path&&WritePrivateProfileStringW(L"Jira",L"TokenLabel",NULL,path)&&
        WritePrivateProfileStringW(L"Jira",L"TokenLabelIdentity",NULL,path);
}
#endif
