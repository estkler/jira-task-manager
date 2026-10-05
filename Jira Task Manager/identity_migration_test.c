#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <wincred.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "native_identity.h"

static int failures;

static void Expect(BOOL condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static BOOL WriteBytes(const wchar_t *path, const char *contents)
{
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    DWORD length=(DWORD)strlen(contents),written=0;
    BOOL ok=WriteFile(file,contents,length,&written,NULL)&&written==length;
    CloseHandle(file);return ok;
}

static char *ReadBytes(const wchar_t *path)
{
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return NULL;
    DWORD size=GetFileSize(file,NULL),read=0;
    char *bytes=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size+1);
    BOOL ok=bytes&&ReadFile(file,bytes,size,&read,NULL);
    CloseHandle(file);if(!ok){if(bytes)HeapFree(GetProcessHeap(),0,bytes);return NULL;}
    bytes[read]=0;return bytes;
}

static BOOL WriteCredential(const wchar_t *target,const wchar_t *url,const wchar_t *token)
{
    CREDENTIALW credential={0};credential.Type=CRED_TYPE_GENERIC;credential.TargetName=(wchar_t*)target;
    credential.UserName=(wchar_t*)url;credential.Persist=CRED_PERSIST_SESSION;
    credential.CredentialBlob=(LPBYTE)token;credential.CredentialBlobSize=(DWORD)(wcslen(token)*sizeof(wchar_t));
    return CredWriteW(&credential,0);
}

static BOOL CredentialMatches(const wchar_t *target,const wchar_t *url,const wchar_t *token)
{
    PCREDENTIALW credential=NULL;if(!CredReadW(target,CRED_TYPE_GENERIC,0,&credential))return FALSE;
    size_t token_bytes=wcslen(token)*sizeof(wchar_t);
    BOOL ok=credential->UserName&&!wcscmp(credential->UserName,url)&&credential->CredentialBlobSize==token_bytes&&
        !memcmp(credential->CredentialBlob,token,token_bytes);
    if(credential->CredentialBlob)SecureZeroMemory(credential->CredentialBlob,credential->CredentialBlobSize);
    CredFree(credential);return ok;
}

static BOOL ReadShortcut(const wchar_t *path,wchar_t *target,size_t target_capacity,wchar_t *arguments,size_t argument_capacity)
{
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;WIN32_FIND_DATAW data={0};
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(hr))hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(hr))hr=IPersistFile_Load(persist,path,STGM_READ);
    if(SUCCEEDED(hr))hr=IShellLinkW_GetPath(link,target,(int)target_capacity,&data,SLGP_RAWPATH);
    if(SUCCEEDED(hr))hr=IShellLinkW_GetArguments(link,arguments,(int)argument_capacity);
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);return SUCCEEDED(hr);
}

int main(void)
{
    HRESULT com=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    Expect(SUCCEEDED(com),"COM initializes for real shortcut verification");
    wchar_t temp[MAX_PATH],root[MAX_PATH],old_directory[MAX_PATH],old_settings[MAX_PATH];
    GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"tmi",0,root);DeleteFileW(root);
    Expect(CreateDirectoryW(root,NULL),"temporary migration root is created");
    swprintf(old_directory,MAX_PATH,L"%ls\\fTasks.Native",root);CreateDirectoryW(old_directory,NULL);
    swprintf(old_settings,MAX_PATH,L"%ls\\settings.ini",old_directory);
    Expect(WriteBytes(old_settings,"[Timer]\r\nText=1\r\n"),"old settings fixture is written");

    wchar_t new_settings[MAX_PATH];
    Expect(TaskManagerMigrateSettingsAtRoot(root,new_settings,MAX_PATH),"settings migration succeeds");
    wchar_t expected_settings[MAX_PATH];swprintf(expected_settings,MAX_PATH,L"%ls\\Task Manager\\settings.ini",root);
    Expect(!wcscmp(new_settings,expected_settings),"new settings path uses Task Manager");
    char *settings=ReadBytes(new_settings);
    Expect(settings&&!strcmp(settings,"[Timer]\r\nText=1\r\n"),"settings contents are preserved");
    if(settings)HeapFree(GetProcessHeap(),0,settings);
    Expect(GetFileAttributesW(old_settings)!=INVALID_FILE_ATTRIBUTES,"safe migration retains old settings until deployment cleanup");
    Expect(WriteBytes(new_settings,"new-wins"),"new settings can be changed");
    Expect(TaskManagerMigrateSettingsAtRoot(root,new_settings,MAX_PATH),"repeated migration succeeds");
    settings=ReadBytes(new_settings);Expect(settings&&!strcmp(settings,"new-wins"),"existing new settings are never overwritten");
    if(settings)HeapFree(GetProcessHeap(),0,settings);

    wchar_t executable[MAX_PATH],diagnostics[MAX_PATH],expected_diagnostics[MAX_PATH];
    swprintf(executable,MAX_PATH,L"%ls\\Task Manager\\bin\\Jira Task Manager.exe",root);
    swprintf(expected_diagnostics,MAX_PATH,L"%ls\\Task Manager\\diagnostics",root);
    Expect(TaskManagerDiagnosticsDirectory(executable,diagnostics,MAX_PATH),"diagnostics path is derived");
    Expect(!wcscmp(diagnostics,expected_diagnostics),"bin deployment stores diagnostics in the app root");
    wcscpy_s(diagnostics,MAX_PATH,executable);
    Expect(TaskManagerDiagnosticsDirectory(diagnostics,diagnostics,MAX_PATH),"diagnostics path supports in-place conversion used by settings");
    Expect(!wcscmp(diagnostics,expected_diagnostics),"in-place diagnostics conversion remains correct");

    wchar_t startup[MAX_PATH];swprintf(startup,MAX_PATH,L"%ls\\Startup",root);CreateDirectoryW(startup,NULL);
    Expect(TaskManagerSetStartupAt(startup,executable,TRUE),"startup shortcut is created");
    Expect(TaskManagerStartupEnabledAt(startup,executable),"created startup shortcut is recognized");
    wchar_t link_path[MAX_PATH],link_target[MAX_PATH],link_arguments[64];
    swprintf(link_path,MAX_PATH,L"%ls\\Jira Task Manager.lnk",startup);
    Expect(ReadShortcut(link_path,link_target,MAX_PATH,link_arguments,64),"startup shortcut can be loaded by Windows Shell");
    Expect(!wcsicmp(link_target,executable),"startup shortcut targets renamed Jira Task Manager executable");
    Expect(!wcscmp(link_arguments,L"--startup"),"startup shortcut uses the startup argument");
    Expect(TaskManagerSetStartupAt(startup,executable,FALSE),"startup shortcut can be disabled");
    Expect(GetFileAttributesW(link_path)==INVALID_FILE_ATTRIBUTES,"disabling startup removes the shortcut");

    wchar_t new_credential[128],old_native[128],old_legacy[128];DWORD pid=GetCurrentProcessId();
    swprintf(new_credential,128,L"Task Manager Test %lu",pid);
    swprintf(old_native,128,L"fTasks.Native Test %lu",pid);
    swprintf(old_legacy,128,L"fTasks Legacy Test %lu",pid);
    CredDeleteW(new_credential,CRED_TYPE_GENERIC,0);CredDeleteW(old_native,CRED_TYPE_GENERIC,0);CredDeleteW(old_legacy,CRED_TYPE_GENERIC,0);
    Expect(WriteCredential(old_native,L"https://native.example",L"native-token"),"native credential fixture is written");
    Expect(WriteCredential(old_legacy,L"https://legacy.example",L"legacy-token"),"legacy credential fixture is written");
    Expect(TaskManagerMigrateCredentialNames(new_credential,old_native,old_legacy),"credential migration succeeds");
    Expect(CredentialMatches(new_credential,L"https://native.example",L"native-token"),"native credential wins and secret is preserved");
    Expect(!CredReadW(old_native,CRED_TYPE_GENERIC,0,(PCREDENTIALW*)&settings),"old native credential is removed after verified copy");
    Expect(!CredReadW(old_legacy,CRED_TYPE_GENERIC,0,(PCREDENTIALW*)&settings),"old legacy credential is removed after verified copy");
    CredDeleteW(new_credential,CRED_TYPE_GENERIC,0);

    DeleteFileW(new_settings);DeleteFileW(old_settings);RemoveDirectoryW(startup);
    wchar_t new_directory[MAX_PATH];swprintf(new_directory,MAX_PATH,L"%ls\\Task Manager",root);
    RemoveDirectoryW(new_directory);RemoveDirectoryW(old_directory);RemoveDirectoryW(root);
    if(SUCCEEDED(com))CoUninitialize();
    if(failures)return 1;puts("identity_migration_test: PASS");return 0;
}
