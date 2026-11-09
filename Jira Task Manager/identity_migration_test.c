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

/* Real Shell links pin ownership, preserved launch behavior, and collisions. */
extern BOOL TaskManagerMigrateShortcutAt(const wchar_t *folder,const wchar_t *executable_path);
extern BOOL TaskManagerMigrateShortcutWithApprovalAt(const wchar_t *folder,const wchar_t *executable_path,HKEY approval);
static BOOL FixtureShortcut(const wchar_t *path,const wchar_t *target,const wchar_t *arguments)
{
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(hr))hr=IShellLinkW_SetPath(link,target);
    if(SUCCEEDED(hr))hr=IShellLinkW_SetArguments(link,arguments);
    if(SUCCEEDED(hr))hr=IShellLinkW_SetWorkingDirectory(link,L"C:\\Windows");
    if(SUCCEEDED(hr))hr=IShellLinkW_SetDescription(link,L"Jira Task Manager — Jira tasks");
    if(SUCCEEDED(hr))hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(hr))hr=IPersistFile_Save(persist,path,TRUE);
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);return SUCCEEDED(hr);
}
static void ShortcutMigration(const wchar_t *folder,const wchar_t *executable)
{
    wchar_t old_link[MAX_PATH],new_link[MAX_PATH],target[MAX_PATH],arguments[64];
    swprintf(old_link,MAX_PATH,L"%ls\\Jira Task Manager.lnk",folder);
    swprintf(new_link,MAX_PATH,L"%ls\\Jira Tasklet.lnk",folder);
    Expect(TaskManagerMigrateShortcutAt(folder,executable),"absent shortcut migration succeeds without enabling startup");
    Expect(GetFileAttributesW(new_link)==INVALID_FILE_ATTRIBUTES,"absent shortcuts stay absent");
    Expect(FixtureShortcut(old_link,executable,L"--startup"),"legacy owned startup link exists");
    Expect(TaskManagerStartupEnabledAt(folder,executable),"legacy startup is recognized before migration");
    Expect(TaskManagerMigrateShortcutAt(folder,executable),"owned legacy link migrates");
    Expect(GetFileAttributesW(old_link)==INVALID_FILE_ATTRIBUTES,"old visible shortcut name disappears");
    Expect(ReadShortcut(new_link,target,MAX_PATH,arguments,64)&&!wcsicmp(target,executable)&&!wcscmp(arguments,L"--startup"),"migration preserves target and startup argument");
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;wchar_t description[128],working[MAX_PATH];
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(hr))hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(hr))hr=IPersistFile_Load(persist,new_link,STGM_READ);
    if(SUCCEEDED(hr))hr=IShellLinkW_GetDescription(link,description,128);
    if(SUCCEEDED(hr))hr=IShellLinkW_GetWorkingDirectory(link,working,MAX_PATH);
    Expect(SUCCEEDED(hr)&&!wcscmp(description,L"Jira Tasklet — Jira tasks")&&!wcsicmp(working,L"C:\\Windows"),"migration updates display description and preserves working directory");
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);
    Expect(TaskManagerMigrateShortcutAt(folder,executable),"migration is idempotent");
    Expect(TaskManagerSetStartupAt(folder,executable,FALSE),"migrated startup disables");
    Expect(FixtureShortcut(old_link,executable,L"--startup"),"legacy startup recreated");
    Expect(TaskManagerSetStartupAt(folder,executable,FALSE)&&GetFileAttributesW(old_link)==INVALID_FILE_ATTRIBUTES,"disable removes legacy owned startup too");
    Expect(FixtureShortcut(old_link,L"C:\\Windows\\notepad.exe",L""),"foreign legacy link exists");
    Expect(TaskManagerMigrateShortcutAt(folder,executable),"foreign old name is left alone");
    Expect(ReadShortcut(old_link,target,MAX_PATH,arguments,64)&&!wcsicmp(target,L"C:\\Windows\\notepad.exe"),"foreign shortcut remains intact");
    DeleteFileW(old_link);
    Expect(FixtureShortcut(old_link,executable,L"--custom preserve"),"ordinary desktop link fixture");
    Expect(FixtureShortcut(new_link,L"C:\\Windows\\notepad.exe",L""),"new name collision exists");
    Expect(!TaskManagerMigrateShortcutAt(folder,executable),"name collision is reported without overwriting");
    Expect(ReadShortcut(old_link,target,MAX_PATH,arguments,64)&&!wcsicmp(target,executable)&&!wcscmp(arguments,L"--custom preserve"),"collision preserves old launch behavior");
    Expect(ReadShortcut(new_link,target,MAX_PATH,arguments,64)&&!wcsicmp(target,L"C:\\Windows\\notepad.exe"),"collision preserves foreign destination");
    DeleteFileW(new_link);
    Expect(TaskManagerMigrateShortcutAt(folder,executable),"desktop shortcut migrates after collision is removed");
    Expect(ReadShortcut(new_link,target,MAX_PATH,arguments,64)&&!wcscmp(arguments,L"--custom preserve"),"non-startup arguments survive migration");
    Expect(!TaskManagerStartupEnabledAt(folder,executable),"ordinary link does not enable startup");
    DeleteFileW(new_link);
}
static void StartupApprovalMigration(const wchar_t *folder,const wchar_t *executable)
{
    wchar_t key_name[128],old_link[MAX_PATH],new_link[MAX_PATH];
    swprintf(key_name,128,L"Software\\JiraTaskletIdentityFixture-%lu",GetCurrentProcessId());
    HKEY key=NULL;Expect(RegCreateKeyExW(HKEY_CURRENT_USER,key_name,0,NULL,0,KEY_ALL_ACCESS,NULL,&key,NULL)==ERROR_SUCCESS,"isolated approval registry fixture created");
    if(!key)return;
    swprintf(old_link,MAX_PATH,L"%ls\\Jira Task Manager.lnk",folder);swprintf(new_link,MAX_PATH,L"%ls\\Jira Tasklet.lnk",folder);
    const BYTE disabled[12]={3,0,0,0,1,2,3,4,5,6,7,8},enabled[12]={2};BYTE actual[12];DWORD type,size;
    Expect(FixtureShortcut(old_link,executable,L"--startup"),"Windows-disabled startup fixture link created");
    Expect(RegSetValueExW(key,L"Jira Task Manager.lnk",0,REG_BINARY,disabled,12)==ERROR_SUCCESS,"Windows-disabled state written");
    Expect(TaskManagerMigrateShortcutWithApprovalAt(folder,executable,key),"disabled startup link migrates with approval");
    size=12;Expect(RegQueryValueExW(key,L"Jira Tasklet.lnk",NULL,&type,actual,&size)==ERROR_SUCCESS&&type==REG_BINARY&&size==12&&!memcmp(actual,disabled,12),"exact disabled state including timestamp survives new filename");
    DeleteFileW(new_link);RegDeleteValueW(key,L"Jira Tasklet.lnk");
    Expect(FixtureShortcut(old_link,executable,L"--startup"),"approval collision fixture link created");
    Expect(RegSetValueExW(key,L"Jira Tasklet.lnk",0,REG_BINARY,enabled,12)==ERROR_SUCCESS,"conflicting new-name approval exists");
    Expect(!TaskManagerMigrateShortcutWithApprovalAt(folder,executable,key),"conflicting approval leaves legacy link untouched");
    size=12;Expect(GetFileAttributesW(old_link)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(new_link)==INVALID_FILE_ATTRIBUTES&&RegQueryValueExW(key,L"Jira Tasklet.lnk",NULL,&type,actual,&size)==ERROR_SUCCESS&&!memcmp(actual,enabled,12),"approval collision preserves both original launch state and foreign value");
    RegDeleteValueW(key,L"Jira Task Manager.lnk");
    Expect(!TaskManagerMigrateShortcutWithApprovalAt(folder,executable,key),"unknown old state never adopts a stale new-name approval");
    RegDeleteValueW(key,L"Jira Tasklet.lnk");
    Expect(TaskManagerMigrateShortcutWithApprovalAt(folder,executable,key),"no approval entries allows default-state migration");
    size=12;Expect(RegQueryValueExW(key,L"Jira Tasklet.lnk",NULL,&type,actual,&size)==ERROR_FILE_NOT_FOUND,"no approval entries invent no enabled state");
    DeleteFileW(new_link);RegCloseKey(key);Expect(RegDeleteKeyW(HKEY_CURRENT_USER,key_name)==ERROR_SUCCESS,"isolated registry fixture removed");
}

int main(void)
{
    Expect(!wcscmp(TASK_MANAGER_EXE_NAME,L"Jira Task Manager.exe"),"display rename preserves executable identity");
    Expect(!wcscmp(TASK_MANAGER_DATA_DIRECTORY,L"Task Manager"),"display rename preserves profile folder");
    Expect(!wcscmp(TASK_MANAGER_STARTUP_LINK,L"Jira Tasklet.lnk"),"Windows sees the current startup shortcut name");
    Expect(!wcscmp(TASK_MANAGER_CREDENTIAL,L"Task Manager:Jira:PAT"),"display rename preserves existing Jira token target");
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
    swprintf(link_path,MAX_PATH,L"%ls\\Jira Tasklet.lnk",startup);
    Expect(ReadShortcut(link_path,link_target,MAX_PATH,link_arguments,64),"startup shortcut can be loaded by Windows Shell");
    Expect(!wcsicmp(link_target,executable),"startup shortcut targets renamed Jira Task Manager executable");
    Expect(!wcscmp(link_arguments,L"--startup"),"startup shortcut uses the startup argument");
    Expect(TaskManagerSetStartupAt(startup,executable,FALSE),"startup shortcut can be disabled");
    Expect(GetFileAttributesW(link_path)==INVALID_FILE_ATTRIBUTES,"disabling startup removes the shortcut");
    ShortcutMigration(startup,executable);
    StartupApprovalMigration(startup,executable);

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
