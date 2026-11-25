#include "native_update_apply.h"
#include "native_identity.h"
#include <bcrypt.h>
#include <aclapi.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

static BOOL nonce(wchar_t out[33]){
    BYTE bytes[16];if(BCryptGenRandom(NULL,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return FALSE;
    for(int i=0;i<16;i++)swprintf(out+i*2,3,L"%02x",bytes[i]);return TRUE;
}
static BOOL dirname(const wchar_t *path,wchar_t out[MAX_PATH]){
    if(!path||wcslen(path)>=MAX_PATH||wcslen(path)<4||path[1]!=L':'||path[2]!=L'\\'||wcschr(path+2,L':')||wcschr(path,L'"'))return FALSE;
    wcscpy_s(out,MAX_PATH,path);wchar_t *last=wcsrchr(out,L'\\');if(!last||!last[1])return FALSE;if(last==out+2)last[1]=0;else *last=0;return TRUE;
}
static BOOL number(const wchar_t *text,ULONGLONG maximum,ULONGLONG *out){
    if(!text||!*text)return FALSE;ULONGLONG n=0;for(;*text;text++){if(*text<L'0'||*text>L'9'||n>(maximum-(*text-L'0'))/10)return FALSE;n=n*10+(*text-L'0');}*out=n;return TRUE;
}
static BOOL digest(const wchar_t *text,BYTE out[32]){
    if(wcslen(text)!=64)return FALSE;for(int i=0;i<32;i++){int n=0;for(int j=0;j<2;j++){wchar_t c=text[2*i+j];int v=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;if(v<0)return FALSE;n=n*16+v;}out[i]=(BYTE)n;}return TRUE;
}
static HANDLE parent_open(const UpdateApplyRequest *r){
    if(!r||!r->parent_pid||!r->parent_created||!UpdatePathSafe(r->target,FALSE)){SetLastError(ERROR_INVALID_DATA);return NULL;}
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,r->parent_pid);if(!process)return NULL;
    FILETIME c,e,k,u;wchar_t image[MAX_PATH];DWORD length=MAX_PATH;
    DWORD error=ERROR_INVALID_DATA;
    if(!GetProcessTimes(process,&c,&e,&k,&u)||!QueryFullProcessImageNameW(process,0,image,&length))error=GetLastError();
    else if((((ULONGLONG)c.dwHighDateTime<<32)|c.dwLowDateTime)==r->parent_created&&
        !_wcsicmp(image,r->target)&&WaitForSingleObject(process,0)==WAIT_TIMEOUT)return process;
    CloseHandle(process);SetLastError(error);return NULL;
}
static BOOL private_directory(HANDLE directory){
    PSECURITY_DESCRIPTOR sd=NULL;PACL acl=NULL;BOOL ok=FALSE;HANDLE token=NULL;TOKEN_USER *user=NULL;DWORD needed=0;
    BYTE system[SECURITY_MAX_SID_SIZE];DWORD system_size=sizeof(system);
    if(GetSecurityInfo(directory,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&acl,NULL,&sd)!=ERROR_SUCCESS||!acl||acl->AceCount!=2)goto done;
    SECURITY_DESCRIPTOR_CONTROL control;DWORD revision;if(!GetSecurityDescriptorControl(sd,&control,&revision)||!(control&SE_DACL_PROTECTED))goto done;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)||!CreateWellKnownSid(WinLocalSystemSid,NULL,system,&system_size))goto done;
    GetTokenInformation(token,TokenUser,NULL,0,&needed);user=malloc(needed);if(!user||!GetTokenInformation(token,TokenUser,user,needed,&needed))goto done;
    BOOL has_user=FALSE,has_system=FALSE;
    for(DWORD i=0;i<2;i++){ACCESS_ALLOWED_ACE *ace=NULL;if(!GetAce(acl,i,(void**)&ace)||ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE||ace->Header.AceFlags||ace->Mask!=FILE_ALL_ACCESS)goto done;
        if(EqualSid(&ace->SidStart,user->User.Sid)&&!has_user)has_user=TRUE;else if(EqualSid(&ace->SidStart,system)&&!has_system)has_system=TRUE;else goto done;}
    ok=has_user&&has_system;
done:if(sd)LocalFree(sd);if(token)CloseHandle(token);free(user);return ok;
}
static HANDLE stage_lock(const wchar_t *source,wchar_t directory[MAX_PATH]){
    if(!dirname(source,directory)||!UpdatePathSafe(source,FALSE)||!UpdatePathSafe(directory,TRUE))return INVALID_HANDLE_VALUE;
    const wchar_t *name=wcsrchr(directory,L'\\');if(!name)return INVALID_HANDLE_VALUE;name++;
    if(wcslen(name)!=47||wcsncmp(name,L"JiraTaskUpdate-",15))return INVALID_HANDLE_VALUE;
    for(int i=15;i<47;i++)if(!((name[i]>=L'0'&&name[i]<=L'9')||(name[i]>=L'a'&&name[i]<=L'f')))return INVALID_HANDLE_VALUE;
    if(wcscmp(wcsrchr(source,L'\\')+1,L"Jira.Task.Manager.exe"))return INVALID_HANDLE_VALUE;
    wchar_t temp[MAX_PATH],resolved[MAX_PATH+8];DWORD n=GetTempPathW(MAX_PATH,temp);if(!n||n>=MAX_PATH)return INVALID_HANDLE_VALUE;
    HANDLE p=CreateFileW(temp,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(p==INVALID_HANDLE_VALUE)return p;n=GetFinalPathNameByHandleW(p,resolved,_countof(resolved),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);CloseHandle(p);
    if(!n||n>=_countof(resolved)||wcsncmp(resolved,L"\\\\?\\",4))return INVALID_HANDLE_VALUE;
    size_t end=wcslen(resolved+4);if(end>3&&resolved[end+3]==L'\\')resolved[end+3]=0;
    wchar_t actual_parent[MAX_PATH];wcscpy_s(actual_parent,MAX_PATH,directory);wchar_t *last=wcsrchr(actual_parent,L'\\');if(last==actual_parent+2)last[1]=0;else *last=0;
    if(_wcsicmp(actual_parent,resolved+4))return INVALID_HANDLE_VALUE;
    HANDLE lock=CreateFileW(directory,GENERIC_READ|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(lock!=INVALID_HANDLE_VALUE&&(!private_directory(lock)||!UpdatePathSafe(directory,TRUE))){CloseHandle(lock);return INVALID_HANDLE_VALUE;}return lock;
}
UpdateStatus UpdateLaunchHelperTracked(const UpdateStage *stage,const UpdateApplyRequest *r,HANDLE *ready_event,HANDLE *helper_process){
    if(helper_process)*helper_process=NULL;
    if(!ready_event){SetLastError(ERROR_INVALID_PARAMETER);return UPDATE_INVALID;}
    *ready_event=NULL;if(!stage||!r||!_wcsicmp(stage->executable,r->target)){SetLastError(ERROR_INVALID_PARAMETER);return UPDATE_INVALID;}
    HANDLE parent=parent_open(r);if(!parent)return UPDATE_INVALID;CloseHandle(parent);
    wchar_t directory[MAX_PATH];HANDLE dir=stage_lock(stage->executable,directory);if(dir==INVALID_HANDLE_VALUE){SetLastError(ERROR_INVALID_DATA);return UPDATE_INVALID;}
    HANDLE file=CreateFileW(stage->executable,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    DWORD error=file==INVALID_HANDLE_VALUE?GetLastError():ERROR_INVALID_DATA;
    UpdateStatus result=UpdateVerifyFile(file,&r->release);if(result!=UPDATE_OK)goto done;
    wchar_t random[33],event_name[80],sha[65],command[2048];if(!nonce(random)){error=ERROR_GEN_FAILURE;result=UPDATE_IO;goto done;}
    swprintf(event_name,_countof(event_name),L"Local\\JiraTaskUpdate-%ls",random);HANDLE event=CreateEventW(NULL,TRUE,FALSE,event_name);
    error=GetLastError();
    if(!event||error==ERROR_ALREADY_EXISTS){if(event)CloseHandle(event);result=UPDATE_IO;goto done;}
    for(int i=0;i<32;i++)swprintf(sha+2*i,3,L"%02x",r->release.sha256[i]);
    /* All strings are canonical paths/event names: Windows forbids embedded
       quotes in paths, and neither quoted argument ends with a backslash. */
    int count=swprintf(command,_countof(command),L"\"%ls\" --apply-update %lu %llu \"%ls\" %lu.%lu.%lu %lu %ls \"%ls\"",stage->executable,r->parent_pid,r->parent_created,r->target,r->release.version.major,r->release.version.minor,r->release.version.patch,r->release.size,sha,event_name);
    STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;
    if(count<0||!CreateProcessW(stage->executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&si,&pi)){
        error=count<0?ERROR_INSUFFICIENT_BUFFER:GetLastError();CloseHandle(event);result=UPDATE_IO;goto done;
    }
    CloseHandle(pi.hThread);if(helper_process)*helper_process=pi.hProcess;else CloseHandle(pi.hProcess);*ready_event=event;result=UPDATE_OK;
done:if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);CloseHandle(dir);SetLastError(result==UPDATE_OK?ERROR_SUCCESS:error);return result;
}
UpdateStatus UpdateLaunchHelper(const UpdateStage *stage,const UpdateApplyRequest *r,HANDLE *ready_event){return UpdateLaunchHelperTracked(stage,r,ready_event,NULL);}
static BOOL move_file(const UpdateApplyOps *ops,HANDLE file,const wchar_t *from,const wchar_t *to){
    if(ops&&ops->move)return ops->move(from,to);
    DWORD bytes=(DWORD)(wcslen(to)*sizeof(wchar_t)),size=(DWORD)offsetof(FILE_RENAME_INFO,FileName)+bytes;
    FILE_RENAME_INFO *rename=calloc(1,size);if(!rename)return FALSE;rename->ReplaceIfExists=FALSE;rename->FileNameLength=bytes;memcpy(rename->FileName,to,bytes);
    BOOL ok=SetFileInformationByHandle(file,FileRenameInfo,rename,size);free(rename);return ok;
}
static BOOL handle_path_equals(HANDLE file,const wchar_t *path){
    wchar_t actual[MAX_PATH+8];DWORD n=GetFinalPathNameByHandleW(file,actual,_countof(actual),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    return n&&n<_countof(actual)&&!wcsncmp(actual,L"\\\\?\\",4)&&!_wcsicmp(actual+4,path);
}
static BOOL start_default(const wchar_t *target,const wchar_t *directory){
    wchar_t command[MAX_PATH+3];if(swprintf(command,_countof(command),L"\"%ls\"",target)<0)return FALSE;
    STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;if(!CreateProcessW(target,command,NULL,NULL,FALSE,0,NULL,directory,&si,&pi))return FALSE;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return TRUE;
}
static HANDLE open_installed_for_update(const wchar_t *path,DWORD *sharing_retries)
{
    /* Antivirus and indexers can briefly open the running image without
       FILE_SHARE_DELETE. Retry only sharing/locking errors before handoff. */
    ULONGLONG deadline=GetTickCount64()+1200;
    for(;;){
        HANDLE file=CreateFileW(path,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,
            OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(file!=INVALID_HANDLE_VALUE)return file;
        DWORD error=GetLastError();
        if((error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION)||GetTickCount64()>=deadline){
            SetLastError(error);return INVALID_HANDLE_VALUE;
        }
        ++*sharing_retries;
        Sleep(80);
    }
}
static BOOL same_file(HANDLE a,HANDLE b){BY_HANDLE_FILE_INFORMATION x,y;return GetFileInformationByHandle(a,&x)&&GetFileInformationByHandle(b,&y)&&x.dwVolumeSerialNumber==y.dwVolumeSerialNumber&&x.nFileIndexHigh==y.nFileIndexHigh&&x.nFileIndexLow==y.nFileIndexLow;}
static BOOL copy_locked(HANDLE source,HANDLE target){
    BYTE buffer[16384];DWORD got,written;LARGE_INTEGER zero={0};if(!SetFilePointerEx(source,zero,NULL,FILE_BEGIN))return FALSE;
    while(ReadFile(source,buffer,sizeof(buffer),&got,NULL)){if(!got)return FlushFileBuffers(target);if(!WriteFile(target,buffer,got,&written,NULL)||written!=got)return FALSE;}return FALSE;
}
static HANDLE open_result_log(const wchar_t *directory,const wchar_t *random){
    wchar_t path[MAX_PATH];
    if(!UpdatePathSafe(directory,TRUE)||swprintf(path,MAX_PATH,L"%ls\\Jira-update-%ls.log",directory,random)<0)
        return INVALID_HANDLE_VALUE;
    return CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ, NULL,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
}
static void result_log(const UpdateApplyRequest *request,const wchar_t *directory,const wchar_t *random,
    UpdateStatus status,const char *step,const UpdateApplyFailure *failure,DWORD sharing_retries){
    wchar_t diagnostics[MAX_PATH];
    HANDLE file=TaskManagerDiagnosticsDirectory(request->target,diagnostics,MAX_PATH)?
        open_result_log(diagnostics,random):INVALID_HANDLE_VALUE;
    if(file==INVALID_HANDLE_VALUE)file=open_result_log(directory,random);
    if(file==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME now;GetLocalTime(&now);
    char text[384];DWORD written;
    int count=snprintf(text,sizeof(text),
        "Native update result: %u\r\n"
        "%04u-%02u-%02u %02u:%02u:%02u | parent_pid=%lu | version=%lu.%lu.%lu | step=%s | win32=%lu | rollback_win32=%lu | sharing_retries=%lu\r\n",
        (unsigned)status,now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
        request->parent_pid,request->release.version.major,request->release.version.minor,
        request->release.version.patch,step,failure->windows_error,failure->rollback_error,sharing_retries);
    if(count>0&&count<(int)sizeof(text))WriteFile(file,text,(DWORD)count,&written,NULL);
    FlushFileBuffers(file);CloseHandle(file);
}
BOOL UpdateFormatApplyFailure(const UpdateApplyFailure *f,BOOL ru,wchar_t *text,size_t capacity){
    if(!f||!text||!capacity||f->phase<UPDATE_APPLY_CHECK_TARGET||f->phase>UPDATE_APPLY_START)return FALSE;
    static const wchar_t *en[]={L"Check installed executable",L"Create new executable",L"Copy new executable",L"Verify new executable",L"Preserve previous version",L"Replace executable",L"Start updated application"};
    static const wchar_t *russian[]={L"Проверка установленного приложения",L"Создание нового файла",L"Копирование нового файла",L"Проверка нового файла",L"Сохранение предыдущей версии",L"Замена приложения",L"Запуск обновлённого приложения"};
    wchar_t detail[512]={0},recovery[1100];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,f->windows_error,0,detail,_countof(detail),NULL);
    if(f->original_available){
        if(swprintf(recovery,_countof(recovery),ru?L"Предыдущая версия сохранена по адресу:\n%ls\n\nОткройте её вручную. Обновление можно повторить позже.":L"The previous version is available at:\n%ls\n\nOpen it manually. You can retry the update later.",f->target)<0)return FALSE;
    }else if(f->backup[0]){
        if(swprintf(recovery,_countof(recovery),ru?L"Автоматическое восстановление не завершено (ошибка Windows: %lu).\nПредыдущая версия сохранена в резервной копии:\n%ls\n\nВосстановите этот файл по адресу:\n%ls\nНе удаляйте резервную копию.":L"Automatic recovery did not complete (Windows error: %lu).\nThe previous version is preserved in this backup:\n%ls\n\nRestore this file to:\n%ls\nDo not delete the backup.",f->rollback_error,f->backup,f->target)<0)return FALSE;
    }else if(swprintf(recovery,_countof(recovery),ru?L"Установленный файл не был изменён обновлением.\nПроверьте приложение по адресу:\n%ls":L"The updater did not change the installed file.\nCheck the application at:\n%ls",f->target)<0)return FALSE;
    return swprintf(text,capacity,ru?L"Не удалось обновить " TASK_MANAGER_TITLE L".\n\nОперация: %ls\nОшибка Windows: %lu\n%ls\n%ls":L"Could not update " TASK_MANAGER_TITLE L".\n\nOperation: %ls\nWindows error: %lu\n%ls\n%ls",ru?russian[f->phase]:en[f->phase],f->windows_error,detail,recovery)>=0;
}
static void report_failure(const UpdateApplyFailure *failure){
    wchar_t text[2048];BOOL ru=PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN;
    if(!UpdateFormatApplyFailure(failure,ru,text,_countof(text)))wcscpy_s(text,_countof(text),TASK_MANAGER_TITLE L" update failed. Keep the installation backup and try again later.");
    MessageBoxW(NULL,text,TASK_MANAGER_TITLE,MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
}
int UpdateRunHelperUsing(int argc,wchar_t **argv,const UpdateApplyOps *ops){
    UpdateStatus result=UPDATE_INVALID;UpdateApplyRequest r={0};ULONGLONG n;wchar_t source[MAX_PATH],source_dir[MAX_PATH],target_dir[MAX_PATH],random[33]={0},candidate[MAX_PATH]={0},backup[MAX_PATH]={0};
    HANDLE source_lock=INVALID_HANDLE_VALUE,target_lock=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE,old=INVALID_HANDLE_VALUE,parent=NULL,event=NULL,new_file=INVALID_HANDLE_VALUE;BOOL backed_up=FALSE,installed=FALSE,candidate_owned=FALSE,parent_exited=FALSE;
    UpdateApplyFailure failure={.phase=UPDATE_APPLY_CHECK_TARGET};
    const char *step="check_target_directory";
    DWORD sharing_retries=0;
    if(argc!=9||!argv||wcscmp(argv[1],L"--apply-update")||!number(argv[2],MAXDWORD,&n)||!n)return result;r.parent_pid=(DWORD)n;
    if(!number(argv[3],~(ULONGLONG)0,&r.parent_created)||!r.parent_created||wcslen(argv[4])>=MAX_PATH||!dirname(argv[4],target_dir))return result;
    wcscpy_s(r.target,MAX_PATH,argv[4]);if(!UpdateParseVersion(argv[5],&r.release.version)||!number(argv[6],UPDATE_EXE_LIMIT,&n)||!n||!digest(argv[7],r.release.sha256))return result;r.release.size=(DWORD)n;
    const wchar_t event_prefix[]=L"Local\\JiraTaskUpdate-";size_t prefix_length=_countof(event_prefix)-1;
    if(wcslen(argv[8])!=prefix_length+32||wcsncmp(argv[8],event_prefix,prefix_length))return result;
    for(size_t i=prefix_length;i<prefix_length+32;i++)if(!((argv[8][i]>=L'0'&&argv[8][i]<=L'9')||(argv[8][i]>=L'a'&&argv[8][i]<=L'f')))return result;
    DWORD length=GetModuleFileNameW(NULL,source,MAX_PATH);if(!length||length>=MAX_PATH||!_wcsicmp(source,r.target)||!(ops&&ops->nonce?ops->nonce(random):nonce(random)))return result;
    if(!UpdatePathSafe(target_dir,TRUE))return result;
    target_lock=CreateFileW(target_dir,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(target_lock==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();goto done;}
    step="check_running_app";parent=parent_open(&r);if(!parent){failure.windows_error=GetLastError();goto done;}
    step="check_staging_directory";source_lock=stage_lock(source,source_dir);if(source_lock==INVALID_HANDLE_VALUE)goto done;
    step="open_staged_exe";file=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();goto done;}
    step="verify_staged_exe";if(UpdateVerifyFile(file,&r.release)!=UPDATE_OK)goto done;
    step="open_installed_exe";
    old=open_installed_for_update(r.target,&sharing_retries);
    UpdateRelease old_release;if(old==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();result=UPDATE_BUSY;goto done;}
    BY_HANDLE_FILE_INFORMATION old_info;
    step="verify_installed_exe";if(UpdateInspectFile(old,&old_release)!=UPDATE_OK)goto done;
    step="check_version";if(UpdateCompareVersion(r.release.version,old_release.version)<=0)goto done;
    step="check_file_identity";if(same_file(file,old))goto done;
    step="read_installed_file_info";if(!GetFileInformationByHandle(old,&old_info)){failure.windows_error=GetLastError();goto done;}
    step="check_installed_links";if(old_info.nNumberOfLinks!=1)goto done;
    step="prepare_target_names";
    int candidate_length=swprintf(candidate,MAX_PATH,L"%ls\\Jira-update-%ls.candidate.exe",target_dir,random);
    int backup_length=swprintf(backup,MAX_PATH,L"%ls\\Jira-update-%ls.backup.exe",target_dir,random);
    if(candidate_length<0||candidate_length>=MAX_PATH||backup_length<0||backup_length>=MAX_PATH){
        failure.windows_error=ERROR_FILENAME_EXCED_RANGE;result=UPDATE_IO;goto done;
    }
    step="open_ready_event";event=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[8]);if(!event){failure.windows_error=GetLastError();goto done;}
    step="signal_ready";if(!SetEvent(event)){failure.windows_error=GetLastError();goto done;}
    step="wait_parent_exit";
    DWORD wait=WaitForSingleObject(parent,ops?ops->parent_timeout_ms:60000);
    if(wait!=WAIT_OBJECT_0){failure.windows_error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();result=UPDATE_BUSY;goto done;}
    parent_exited=TRUE;wcscpy_s(failure.target,MAX_PATH,r.target);
    step="recheck_installed_exe";
    HANDLE check=CreateFileW(r.target,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(check==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();goto done;}
    BOOL unchanged=same_file(old,check)&&UpdatePathSafe(r.target,FALSE)&&UpdatePathSafe(target_dir,TRUE);
    CloseHandle(check);if(!unchanged)goto done;
    failure.original_available=TRUE;failure.phase=UPDATE_APPLY_CREATE;step="create_candidate";
    new_file=CreateFileW(candidate,GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(new_file==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();result=UPDATE_IO;goto done;}
    candidate_owned=TRUE;
    failure.phase=UPDATE_APPLY_COPY;step="copy_candidate";
    if(!(ops&&ops->copy?ops->copy(file,new_file):copy_locked(file,new_file))){failure.windows_error=GetLastError();result=UPDATE_IO;goto done;}
    failure.phase=UPDATE_APPLY_VERIFY;step="verify_candidate";
    if(UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){failure.windows_error=ERROR_INVALID_DATA;result=UPDATE_INVALID;goto done;}
    /* Drop this handle's write access before launching: the read-only handle
       below denies any other writer while allowing our exact renames. */
    CloseHandle(new_file);new_file=CreateFileW(candidate,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(new_file==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();result=UPDATE_IO;goto done;}
    if(UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){failure.windows_error=ERROR_INVALID_DATA;result=UPDATE_INVALID;goto done;}
    BOOL (*start)(const wchar_t*,const wchar_t*)=ops&&ops->start?ops->start:start_default;
    failure.phase=UPDATE_APPLY_BACKUP;step="backup_exe";
    if(!handle_path_equals(old,r.target)){failure.windows_error=ERROR_INVALID_DATA;result=UPDATE_INVALID;goto done;}
    if(!move_file(ops,old,r.target,backup)){failure.windows_error=GetLastError();result=UPDATE_IO;goto done;}backed_up=TRUE;failure.original_available=FALSE;
    failure.phase=UPDATE_APPLY_REPLACE;step="replace_exe";
    if(!move_file(ops,new_file,candidate,r.target)){failure.windows_error=GetLastError();result=UPDATE_IO;goto restore;}installed=TRUE;
    CloseHandle(new_file);new_file=CreateFileW(r.target,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    failure.phase=UPDATE_APPLY_VERIFY;step="verify_replacement";
    if(new_file==INVALID_HANDLE_VALUE){failure.windows_error=GetLastError();result=UPDATE_IO;goto restore;}
    if(!UpdatePathSafe(r.target,FALSE)||UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){failure.windows_error=ERROR_INVALID_DATA;result=UPDATE_INVALID;goto restore;}
    failure.phase=UPDATE_APPLY_START;step="start_updated_app";
    if(!start(r.target,target_dir)){failure.windows_error=GetLastError();result=UPDATE_IO;goto restore;}
    result=UPDATE_OK;step="complete";goto done;
restore:
    if(installed){
        if(new_file!=INVALID_HANDLE_VALUE)CloseHandle(new_file);
        new_file=CreateFileW(r.target,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(new_file==INVALID_HANDLE_VALUE){failure.rollback_error=GetLastError();goto done;}
        if(UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){failure.rollback_error=ERROR_INVALID_DATA;goto done;}
        if(!move_file(ops,new_file,r.target,candidate)){failure.rollback_error=GetLastError();goto done;}
    }
    if(move_file(ops,old,backup,r.target)){backed_up=FALSE;installed=FALSE;failure.original_available=TRUE;}else failure.rollback_error=GetLastError();
done:
    if(candidate_owned&&new_file!=INVALID_HANDLE_VALUE&&handle_path_equals(new_file,candidate)){FILE_DISPOSITION_INFO disposition={TRUE};SetFileInformationByHandle(new_file,FileDispositionInfo,&disposition,sizeof(disposition));}
    if(new_file!=INVALID_HANDLE_VALUE)CloseHandle(new_file);if(old!=INVALID_HANDLE_VALUE)CloseHandle(old);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    /* Backup is never deleted. If restoration fails it remains recoverable. */
    if(backed_up)wcscpy_s(failure.backup,MAX_PATH,backup);
    if(event)CloseHandle(event);if(parent)CloseHandle(parent);if(source_lock!=INVALID_HANDLE_VALUE)CloseHandle(source_lock);if(target_lock!=INVALID_HANDLE_VALUE)CloseHandle(target_lock);
    if(result!=UPDATE_OK&&!failure.windows_error)failure.windows_error=result==UPDATE_INVALID?ERROR_INVALID_DATA:ERROR_GEN_FAILURE;
    if(random[0])result_log(&r,target_dir,random,result,step,&failure,sharing_retries);
    if(parent_exited&&result!=UPDATE_OK)(ops&&ops->report?ops->report:report_failure)(&failure);
    return result;
}
int UpdateRunHelper(int argc,wchar_t **argv){return UpdateRunHelperUsing(argc,argv,NULL);}
