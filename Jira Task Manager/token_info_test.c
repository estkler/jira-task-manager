#include "native_token_info.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    wchar_t dir[MAX_PATH],path[MAX_PATH],text[96];GetTempPathW(MAX_PATH,dir);GetTempFileNameW(dir,L"tki",0,path);
    WritePrivateProfileStringW(L"Jira",L"TokenLabel",L"Old local label",path);
    WritePrivateProfileStringW(L"Jira",L"TokenLabelIdentity",L"Old fingerprint",path);
    WritePrivateProfileStringW(L"Jira",L"Url",L"https://jira.example.test/",path);
    WritePrivateProfileStringW(L"Timer",L"Started",L"123456789",path);
    WritePrivateProfileStringW(L"Interface",L"DueDateBar",L"1",path);
    assert(NativeClearLegacyTokenLabels(path));
    GetPrivateProfileStringW(L"Jira",L"TokenLabel",L"missing",text,96,path);assert(!wcscmp(text,L"missing"));
    GetPrivateProfileStringW(L"Jira",L"TokenLabelIdentity",L"missing",text,96,path);assert(!wcscmp(text,L"missing"));
    GetPrivateProfileStringW(L"Jira",L"Url",L"",text,96,path);assert(!wcscmp(text,L"https://jira.example.test/"));
    assert(GetPrivateProfileIntW(L"Timer",L"Started",0,path)==123456789&&GetPrivateProfileIntW(L"Interface",L"DueDateBar",0,path)==1);
    assert(NativeClearLegacyTokenLabels(path));DeleteFileW(path);puts("Legacy token label cleanup preserves connection, timer and interface settings");
}
