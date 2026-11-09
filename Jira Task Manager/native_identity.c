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
    if(SUCCEEDED(result))result=IShellLinkW_SetDescription(link,TASK_MANAGER_TITLE L" — Jira tasks");
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
    if(!executable_path||!executable_path[0])return FALSE;
    if(enabled){
        if(!TaskManagerMigrateShortcutAt(startup_folder,executable_path))return FALSE;
        if(GetFileAttributesW(shortcut)!=INVALID_FILE_ATTRIBUTES){
            wchar_t target[MAX_PATH],arguments[64];
            return LoadStartupTarget(shortcut,target,MAX_PATH,arguments,64)&&!wcsicmp(target,executable_path)&&!wcscmp(arguments,L"--startup");
        }
        return CreateStartupShortcut(shortcut,executable_path)&&TaskManagerStartupEnabledAt(startup_folder,executable_path);
    }
    const wchar_t *names[]={TASK_MANAGER_STARTUP_LINK,TASK_MANAGER_LEGACY_LINK};BOOL ok=TRUE;
    for(int i=0;i<2;i++){
        wchar_t target[MAX_PATH],arguments[64];
        if(!JoinPath(shortcut,MAX_PATH,startup_folder,names[i]))return FALSE;
        if(GetFileAttributesW(shortcut)==INVALID_FILE_ATTRIBUTES)continue;
        if(!LoadStartupTarget(shortcut,target,MAX_PATH,arguments,64)){ok=FALSE;continue;}
        if(!wcsicmp(target,executable_path)&&!wcscmp(arguments,L"--startup")&&!DeleteFileW(shortcut))ok=FALSE;
    }
    return ok;
}

BOOL TaskManagerStartupEnabledAt(const wchar_t *startup_folder,const wchar_t *executable_path)
{
    wchar_t shortcut[MAX_PATH],target[MAX_PATH],arguments[64];
    if(!executable_path||!startup_folder)return FALSE;
    const wchar_t *names[]={TASK_MANAGER_STARTUP_LINK,TASK_MANAGER_LEGACY_LINK};
    for(int i=0;i<2;i++)if(JoinPath(shortcut,MAX_PATH,startup_folder,names[i])&&
       LoadStartupTarget(shortcut,target,MAX_PATH,arguments,64)&&!wcsicmp(target,executable_path)&&!wcscmp(arguments,L"--startup"))return TRUE;
    return FALSE;
}

static BOOL StageShortcutApproval(HKEY approval,BOOL *created)
{
    *created=FALSE;if(!approval)return TRUE;
    BYTE old_value[1024],new_value[1024];DWORD old_type=0,new_type=0,old_size=sizeof(old_value),new_size=sizeof(new_value);
    LSTATUS old_status=RegQueryValueExW(approval,TASK_MANAGER_LEGACY_LINK,NULL,&old_type,old_value,&old_size);
    LSTATUS new_status=RegQueryValueExW(approval,TASK_MANAGER_STARTUP_LINK,NULL,&new_type,new_value,&new_size);
    if(old_status==ERROR_FILE_NOT_FOUND)return new_status==ERROR_FILE_NOT_FOUND;
    if(old_status!=ERROR_SUCCESS||old_type!=REG_BINARY||!old_size)return FALSE;
    if(new_status==ERROR_SUCCESS)return new_type==old_type&&new_size==old_size&&!memcmp(old_value,new_value,old_size);
    if(new_status!=ERROR_FILE_NOT_FOUND)return FALSE;
    if(RegSetValueExW(approval,TASK_MANAGER_STARTUP_LINK,0,old_type,old_value,old_size)!=ERROR_SUCCESS)return FALSE;
    *created=TRUE;return TRUE;
}

BOOL TaskManagerMigrateShortcutWithApprovalAt(const wchar_t *folder,const wchar_t *executable_path,HKEY approval)
{
    wchar_t old_path[MAX_PATH],new_path[MAX_PATH],target[MAX_PATH],arguments[32768];
    if(!folder||!executable_path||!executable_path[0]||
       !JoinPath(old_path,MAX_PATH,folder,TASK_MANAGER_LEGACY_LINK)||!JoinPath(new_path,MAX_PATH,folder,TASK_MANAGER_STARTUP_LINK))return FALSE;
    if(GetFileAttributesW(old_path)==INVALID_FILE_ATTRIBUTES)return GetLastError()==ERROR_FILE_NOT_FOUND||GetLastError()==ERROR_PATH_NOT_FOUND;
    if(!LoadStartupTarget(old_path,target,MAX_PATH,arguments,_countof(arguments)))return FALSE;
    if(wcsicmp(target,executable_path))return TRUE; /* A different installation or application owns this name. */
    if(GetFileAttributesW(new_path)!=INVALID_FILE_ATTRIBUTES)return FALSE;
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);BOOL uninitialize=SUCCEEDED(initialized);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return FALSE;
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;wchar_t temporary[MAX_PATH]=L"";
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(hr))hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist);
    if(SUCCEEDED(hr))hr=IPersistFile_Load(persist,old_path,STGM_READ);
    if(SUCCEEDED(hr))hr=IShellLinkW_SetDescription(link,TASK_MANAGER_TITLE L" — Jira tasks");
    if(SUCCEEDED(hr)&&!GetTempFileNameW(folder,L"jtl",0,temporary))hr=HRESULT_FROM_WIN32(GetLastError());
    if(SUCCEEDED(hr))hr=IPersistFile_Save(persist,temporary,TRUE);
    if(persist)IPersistFile_Release(persist);if(link)IShellLinkW_Release(link);
    /* StartupApproved is keyed by filename. Transfer the exact bytes before
       publishing the new link; unknown/colliding approval leaves the old one. */
    BOOL approval_created=FALSE;
    BOOL ok=SUCCEEDED(hr)&&StageShortcutApproval(approval,&approval_created)&&MoveFileExW(temporary,new_path,MOVEFILE_WRITE_THROUGH);
    if(temporary[0])DeleteFileW(temporary);
    if(ok&&!DeleteFileW(old_path)){DeleteFileW(new_path);ok=FALSE;}
    if(!ok&&approval_created)RegDeleteValueW(approval,TASK_MANAGER_STARTUP_LINK);
    if(uninitialize)CoUninitialize();
    if(ok)SHChangeNotify(SHCNE_RENAMEITEM,SHCNF_PATHW,old_path,new_path);
    return ok;
}

BOOL TaskManagerMigrateShortcutAt(const wchar_t *folder,const wchar_t *executable_path)
{
    return TaskManagerMigrateShortcutWithApprovalAt(folder,executable_path,NULL);
}

BOOL TaskManagerMigrateShellLinks(void)
{
    wchar_t executable[MAX_PATH],folder[MAX_PATH];DWORD length=GetModuleFileNameW(NULL,executable,MAX_PATH);
    if(!length||length>=MAX_PATH)return FALSE;
    const int folders[]={CSIDL_STARTUP,CSIDL_DESKTOPDIRECTORY,CSIDL_PROGRAMS};BOOL ok=TRUE;
    for(int i=0;i<_countof(folders);i++){
        if(FAILED(SHGetFolderPathW(NULL,folders[i],NULL,SHGFP_TYPE_CURRENT,folder))){ok=FALSE;continue;}
        if(folders[i]==CSIDL_STARTUP){
            HKEY approval=NULL;LSTATUS status=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&approval);
            if(status!=ERROR_SUCCESS&&status!=ERROR_FILE_NOT_FOUND){ok=FALSE;continue;}
            if(!TaskManagerMigrateShortcutWithApprovalAt(folder,executable,approval))ok=FALSE;
            if(approval)RegCloseKey(approval);
        }else if(!TaskManagerMigrateShortcutAt(folder,executable))ok=FALSE;
    }
    return ok;
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
