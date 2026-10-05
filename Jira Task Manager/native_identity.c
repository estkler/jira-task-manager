#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincred.h>
#include <wchar.h>
#include "native_identity.h"

static BOOL JoinPath(wchar_t *output,size_t capacity,const wchar_t *left,const wchar_t *right)
{
    int written=swprintf(output,capacity,L"%ls\\%ls",left,right);
    return written>0&&(size_t)written<capacity;
}

static BOOL RemoveLastComponent(wchar_t *path)
{
    wchar_t *back=wcsrchr(path,L'\\'),*forward=wcsrchr(path,L'/');
    wchar_t *separator=back;
    if(!separator||(forward&&forward>separator))separator=forward;
    if(!separator)return FALSE;*separator=0;return TRUE;
}

BOOL TaskManagerMigrateSettingsAtRoot(const wchar_t *local_root,wchar_t *settings_path,size_t capacity)
{
    if(!local_root||!local_root[0]||!settings_path||capacity<MAX_PATH)return FALSE;
    wchar_t new_directory[MAX_PATH],old_directory[MAX_PATH],old_settings[MAX_PATH];
    if(!JoinPath(new_directory,MAX_PATH,local_root,TASK_MANAGER_DATA_DIRECTORY)||
       !JoinPath(settings_path,capacity,new_directory,L"settings.ini")||
       !JoinPath(old_directory,MAX_PATH,local_root,L"fTasks.Native")||
       !JoinPath(old_settings,MAX_PATH,old_directory,L"settings.ini"))return FALSE;
    if(!CreateDirectoryW(new_directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;
    DWORD attributes=GetFileAttributesW(new_directory);
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY))return FALSE;
    if(GetFileAttributesW(settings_path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(old_settings)!=INVALID_FILE_ATTRIBUTES){
        if(!CopyFileW(old_settings,settings_path,TRUE)&&GetLastError()!=ERROR_FILE_EXISTS)return FALSE;
    }
    return TRUE;
}

BOOL TaskManagerDiagnosticsDirectory(const wchar_t *executable_path,wchar_t *directory,size_t capacity)
{
    if(!executable_path||!executable_path[0]||!directory||capacity<MAX_PATH||wcslen(executable_path)>=capacity)return FALSE;
    wcscpy_s(directory,capacity,executable_path);
    if(!RemoveLastComponent(directory))return FALSE;
    const wchar_t *leaf=wcsrchr(directory,L'\\');leaf=leaf?leaf+1:directory;
    if(!wcsicmp(leaf,L"bin")&&!RemoveLastComponent(directory))return FALSE;
    wchar_t result[MAX_PATH];if(!JoinPath(result,MAX_PATH,directory,L"diagnostics"))return FALSE;
    wcscpy_s(directory,capacity,result);return TRUE;
}

static BOOL ShortcutPath(const wchar_t *startup_folder,wchar_t *path,size_t capacity)
{
    return startup_folder&&startup_folder[0]&&JoinPath(path,capacity,startup_folder,TASK_MANAGER_STARTUP_LINK);
}

static BOOL CreateStartupShortcut(const wchar_t *shortcut_path,const wchar_t *executable_path)
{
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    BOOL uninitialize=SUCCEEDED(initialized);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return FALSE;
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;
    HRESULT result=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(result))result=IShellLinkW_SetPath(link,executable_path);
    if(SUCCEEDED(result))result=IShellLinkW_SetArguments(link,L"--startup");
    if(SUCCEEDED(result))result=IShellLinkW_SetDescription(link,L"Jira Task Manager — Jira tasks");
    wchar_t working[MAX_PATH];wcscpy_s(working,MAX_PATH,executable_path);
    if(SUCCEEDED(result)&&RemoveLastComponent(working))result=IShellLinkW_SetWorkingDirectory(link,working);
    if(SUCCEEDED(result))result=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(result))result=IPersistFile_Save(persist,shortcut_path,TRUE);
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);
    if(uninitialize)CoUninitialize();return SUCCEEDED(result);
}

static BOOL LoadStartupTarget(const wchar_t *shortcut_path,wchar_t *target,size_t target_capacity,wchar_t *arguments,size_t argument_capacity)
{
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    BOOL uninitialize=SUCCEEDED(initialized);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return FALSE;
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;WIN32_FIND_DATAW data={0};
    HRESULT result=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(result))result=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(result))result=IPersistFile_Load(persist,shortcut_path,STGM_READ);
    if(SUCCEEDED(result))result=IShellLinkW_GetPath(link,target,(int)target_capacity,&data,SLGP_RAWPATH);
    if(SUCCEEDED(result))result=IShellLinkW_GetArguments(link,arguments,(int)argument_capacity);
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);
    if(uninitialize)CoUninitialize();return SUCCEEDED(result);
}

BOOL TaskManagerSetStartupAt(const wchar_t *startup_folder,const wchar_t *executable_path,BOOL enabled)
{
    wchar_t shortcut[MAX_PATH];if(!ShortcutPath(startup_folder,shortcut,MAX_PATH))return FALSE;
    if(!enabled)return DeleteFileW(shortcut)||GetLastError()==ERROR_FILE_NOT_FOUND;
    return executable_path&&executable_path[0]&&CreateStartupShortcut(shortcut,executable_path)&&TaskManagerStartupEnabledAt(startup_folder,executable_path);
}

BOOL TaskManagerStartupEnabledAt(const wchar_t *startup_folder,const wchar_t *executable_path)
{
    wchar_t shortcut[MAX_PATH],target[MAX_PATH],arguments[64];
    if(!executable_path||!ShortcutPath(startup_folder,shortcut,MAX_PATH)||
       !LoadStartupTarget(shortcut,target,MAX_PATH,arguments,64))return FALSE;
    return !wcsicmp(target,executable_path)&&!wcscmp(arguments,L"--startup");
}

static BOOL StartupContext(wchar_t *startup,wchar_t *executable)
{
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_STARTUP,NULL,SHGFP_TYPE_CURRENT,startup)))return FALSE;
    DWORD length=GetModuleFileNameW(NULL,executable,MAX_PATH);return length>0&&length<MAX_PATH;
}

BOOL TaskManagerSetStartup(BOOL enabled)
{
    wchar_t startup[MAX_PATH],executable[MAX_PATH];return StartupContext(startup,executable)&&TaskManagerSetStartupAt(startup,executable,enabled);
}

BOOL TaskManagerStartupEnabled(void)
{
    wchar_t startup[MAX_PATH],executable[MAX_PATH];return StartupContext(startup,executable)&&TaskManagerStartupEnabledAt(startup,executable);
}

static void SecureFreeCredential(PCREDENTIALW credential)
{
    if(!credential)return;
    if(credential->CredentialBlob&&credential->CredentialBlobSize)SecureZeroMemory(credential->CredentialBlob,credential->CredentialBlobSize);
    CredFree(credential);
}

static BOOL CredentialEquals(PCREDENTIALW left,PCREDENTIALW right)
{
    return left&&right&&left->UserName&&right->UserName&&!wcscmp(left->UserName,right->UserName)&&
        left->CredentialBlobSize==right->CredentialBlobSize&&
        (!left->CredentialBlobSize||!memcmp(left->CredentialBlob,right->CredentialBlob,left->CredentialBlobSize));
}

BOOL TaskManagerMigrateCredentialNames(const wchar_t *new_target,const wchar_t *old_native_target,const wchar_t *old_legacy_target)
{
    if(!new_target||!old_native_target||!old_legacy_target)return FALSE;
    PCREDENTIALW current=NULL,source=NULL,verified=NULL;
    BOOL has_current=CredReadW(new_target,CRED_TYPE_GENERIC,0,&current);
    if(!has_current){
        if(!CredReadW(old_native_target,CRED_TYPE_GENERIC,0,&source))CredReadW(old_legacy_target,CRED_TYPE_GENERIC,0,&source);
        if(source){
            CREDENTIALW destination={0};destination.Type=CRED_TYPE_GENERIC;destination.TargetName=(wchar_t*)new_target;
            destination.UserName=source->UserName;destination.Persist=CRED_PERSIST_LOCAL_MACHINE;
            destination.CredentialBlob=source->CredentialBlob;destination.CredentialBlobSize=source->CredentialBlobSize;
            if(!CredWriteW(&destination,0)||!CredReadW(new_target,CRED_TYPE_GENERIC,0,&verified)||!CredentialEquals(source,verified)){
                SecureFreeCredential(verified);SecureFreeCredential(source);return FALSE;
            }
        }
    }
    SecureFreeCredential(verified);SecureFreeCredential(source);SecureFreeCredential(current);
    LSTATUS first=CredDeleteW(old_native_target,CRED_TYPE_GENERIC,0)?ERROR_SUCCESS:GetLastError();
    LSTATUS second=CredDeleteW(old_legacy_target,CRED_TYPE_GENERIC,0)?ERROR_SUCCESS:GetLastError();
    return (first==ERROR_SUCCESS||first==ERROR_NOT_FOUND)&&(second==ERROR_SUCCESS||second==ERROR_NOT_FOUND);
}

BOOL TaskManagerMigrateCredential(void)
{
    return TaskManagerMigrateCredentialNames(TASK_MANAGER_CREDENTIAL,L"fTasks.Native:Jira:PAT",L"fTasks:Jira:PAT");
}
