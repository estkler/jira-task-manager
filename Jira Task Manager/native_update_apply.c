#include "native_update_apply.h"
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
    if(!r||!r->parent_pid||!r->parent_created||!UpdatePathSafe(r->target,FALSE))return NULL;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,r->parent_pid);if(!process)return NULL;
    FILETIME c,e,k,u;wchar_t image[MAX_PATH];DWORD length=MAX_PATH;
    BOOL ok=GetProcessTimes(process,&c,&e,&k,&u)&&(((ULONGLONG)c.dwHighDateTime<<32)|c.dwLowDateTime)==r->parent_created&&
        QueryFullProcessImageNameW(process,0,image,&length)&&!_wcsicmp(image,r->target)&&WaitForSingleObject(process,0)==WAIT_TIMEOUT;
    if(!ok){CloseHandle(process);return NULL;}return process;
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
    if(!ready_event)return UPDATE_INVALID;*ready_event=NULL;if(!stage||!r||!_wcsicmp(stage->executable,r->target))return UPDATE_INVALID;
    HANDLE parent=parent_open(r);if(!parent)return UPDATE_INVALID;CloseHandle(parent);
    wchar_t directory[MAX_PATH];HANDLE dir=stage_lock(stage->executable,directory);if(dir==INVALID_HANDLE_VALUE)return UPDATE_INVALID;
    HANDLE file=CreateFileW(stage->executable,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    UpdateStatus result=UpdateVerifyFile(file,&r->release);if(result!=UPDATE_OK)goto done;
    wchar_t random[33],event_name[80],sha[65],command[2048];if(!nonce(random)){result=UPDATE_IO;goto done;}
    swprintf(event_name,_countof(event_name),L"Local\\JiraTaskUpdate-%ls",random);HANDLE event=CreateEventW(NULL,TRUE,FALSE,event_name);
    if(!event||GetLastError()==ERROR_ALREADY_EXISTS){if(event)CloseHandle(event);result=UPDATE_IO;goto done;}
    for(int i=0;i<32;i++)swprintf(sha+2*i,3,L"%02x",r->release.sha256[i]);
    /* All strings are canonical paths/event names: Windows forbids embedded
       quotes in paths, and neither quoted argument ends with a backslash. */
    int count=swprintf(command,_countof(command),L"\"%ls\" --apply-update %lu %llu \"%ls\" %lu.%lu.%lu %lu %ls \"%ls\"",stage->executable,r->parent_pid,r->parent_created,r->target,r->release.version.major,r->release.version.minor,r->release.version.patch,r->release.size,sha,event_name);
    STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;
    if(count<0||!CreateProcessW(stage->executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&si,&pi)){CloseHandle(event);result=UPDATE_IO;goto done;}
    CloseHandle(pi.hThread);if(helper_process)*helper_process=pi.hProcess;else CloseHandle(pi.hProcess);*ready_event=event;result=UPDATE_OK;
done:if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);CloseHandle(dir);return result;
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
static BOOL same_file(HANDLE a,HANDLE b){BY_HANDLE_FILE_INFORMATION x,y;return GetFileInformationByHandle(a,&x)&&GetFileInformationByHandle(b,&y)&&x.dwVolumeSerialNumber==y.dwVolumeSerialNumber&&x.nFileIndexHigh==y.nFileIndexHigh&&x.nFileIndexLow==y.nFileIndexLow;}
static BOOL copy_locked(HANDLE source,HANDLE target){
    BYTE buffer[16384];DWORD got,written;LARGE_INTEGER zero={0};if(!SetFilePointerEx(source,zero,NULL,FILE_BEGIN))return FALSE;
    while(ReadFile(source,buffer,sizeof(buffer),&got,NULL)){if(!got)return FlushFileBuffers(target);if(!WriteFile(target,buffer,got,&written,NULL)||written!=got)return FALSE;}return FALSE;
}
static void result_log(const wchar_t *directory,const wchar_t *random,UpdateStatus status){
    wchar_t path[MAX_PATH];if(swprintf(path,MAX_PATH,L"%ls\\Jira-update-%ls.log",directory,random)<0||!UpdatePathSafe(directory,TRUE))return;
    HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(f==INVALID_HANDLE_VALUE)return;
    char text[80];int count=snprintf(text,sizeof(text),"Native update result: %u\r\n",(unsigned)status);DWORD written;WriteFile(f,text,count,&written,NULL);FlushFileBuffers(f);CloseHandle(f);
}
int UpdateRunHelperUsing(int argc,wchar_t **argv,const UpdateApplyOps *ops){
    UpdateStatus result=UPDATE_INVALID;UpdateApplyRequest r={0};ULONGLONG n;wchar_t source[MAX_PATH],source_dir[MAX_PATH],target_dir[MAX_PATH],random[33]={0},candidate[MAX_PATH]={0},backup[MAX_PATH]={0};
    HANDLE source_lock=INVALID_HANDLE_VALUE,target_lock=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE,old=INVALID_HANDLE_VALUE,parent=NULL,event=NULL,new_file=INVALID_HANDLE_VALUE;BOOL backed_up=FALSE,installed=FALSE,candidate_owned=FALSE;
    if(argc!=9||!argv||wcscmp(argv[1],L"--apply-update")||!number(argv[2],MAXDWORD,&n)||!n)return result;r.parent_pid=(DWORD)n;
    if(!number(argv[3],~(ULONGLONG)0,&r.parent_created)||!r.parent_created||wcslen(argv[4])>=MAX_PATH||!dirname(argv[4],target_dir))return result;
    wcscpy_s(r.target,MAX_PATH,argv[4]);if(!UpdateParseVersion(argv[5],&r.release.version)||!number(argv[6],UPDATE_EXE_LIMIT,&n)||!n||!digest(argv[7],r.release.sha256))return result;r.release.size=(DWORD)n;
    const wchar_t event_prefix[]=L"Local\\JiraTaskUpdate-";size_t prefix_length=_countof(event_prefix)-1;
    if(wcslen(argv[8])!=prefix_length+32||wcsncmp(argv[8],event_prefix,prefix_length))return result;
    for(size_t i=prefix_length;i<prefix_length+32;i++)if(!((argv[8][i]>=L'0'&&argv[8][i]<=L'9')||(argv[8][i]>=L'a'&&argv[8][i]<=L'f')))return result;
    DWORD length=GetModuleFileNameW(NULL,source,MAX_PATH);if(!length||length>=MAX_PATH||!_wcsicmp(source,r.target)||!(ops&&ops->nonce?ops->nonce(random):nonce(random)))return result;
    if(!UpdatePathSafe(target_dir,TRUE))return result;
    target_lock=CreateFileW(target_dir,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(target_lock==INVALID_HANDLE_VALUE)goto done;
    parent=parent_open(&r);if(!parent)goto done;
    source_lock=stage_lock(source,source_dir);if(source_lock==INVALID_HANDLE_VALUE)goto done;
    file=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(UpdateVerifyFile(file,&r.release)!=UPDATE_OK)goto done;
    old=CreateFileW(r.target,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    UpdateRelease old_release;if(old==INVALID_HANDLE_VALUE){result=UPDATE_BUSY;goto done;}
    BY_HANDLE_FILE_INFORMATION old_info;
    if(UpdateInspectFile(old,&old_release)!=UPDATE_OK||UpdateCompareVersion(r.release.version,old_release.version)<=0||same_file(file,old)||!GetFileInformationByHandle(old,&old_info)||old_info.nNumberOfLinks!=1)goto done;
    event=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[8]);if(!event||!SetEvent(event))goto done;
    if(WaitForSingleObject(parent,ops?ops->parent_timeout_ms:60000)!=WAIT_OBJECT_0){result=UPDATE_BUSY;goto done;}
    HANDLE check=CreateFileW(r.target,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    BOOL unchanged=check!=INVALID_HANDLE_VALUE&&same_file(old,check)&&UpdatePathSafe(r.target,FALSE)&&UpdatePathSafe(target_dir,TRUE);if(check!=INVALID_HANDLE_VALUE)CloseHandle(check);if(!unchanged)goto done;
    if(swprintf(candidate,MAX_PATH,L"%ls\\Jira-update-%ls.candidate.exe",target_dir,random)<0||swprintf(backup,MAX_PATH,L"%ls\\Jira-update-%ls.backup.exe",target_dir,random)<0){result=UPDATE_IO;goto done;}
    new_file=CreateFileW(candidate,GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(new_file==INVALID_HANDLE_VALUE){result=UPDATE_IO;goto done;}
    candidate_owned=TRUE;
    if(!copy_locked(file,new_file)||UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){result=UPDATE_IO;goto done;}
    /* Drop this handle's write access before launching: the read-only handle
       below denies any other writer while allowing our exact renames. */
    CloseHandle(new_file);new_file=CreateFileW(candidate,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(new_file==INVALID_HANDLE_VALUE||UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK){result=UPDATE_INVALID;goto done;}
    BOOL (*start)(const wchar_t*,const wchar_t*)=ops&&ops->start?ops->start:start_default;
    if(!handle_path_equals(old,r.target)||!move_file(ops,old,r.target,backup)){result=UPDATE_IO;goto done;}backed_up=TRUE;
    if(!move_file(ops,new_file,candidate,r.target)){result=UPDATE_IO;goto restore;}installed=TRUE;
    CloseHandle(new_file);new_file=CreateFileW(r.target,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(new_file==INVALID_HANDLE_VALUE||!UpdatePathSafe(r.target,FALSE)||UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK||!start(r.target,target_dir)){result=UPDATE_IO;goto restore;}
    result=UPDATE_OK;goto done;
restore:
    if(installed){
        if(new_file!=INVALID_HANDLE_VALUE)CloseHandle(new_file);
        new_file=CreateFileW(r.target,GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(new_file==INVALID_HANDLE_VALUE||UpdateVerifyFile(new_file,&r.release)!=UPDATE_OK||!move_file(ops,new_file,r.target,candidate))goto done;
    }
    if(move_file(ops,old,backup,r.target)){backed_up=FALSE;installed=FALSE;}
done:
    if(candidate_owned&&new_file!=INVALID_HANDLE_VALUE&&handle_path_equals(new_file,candidate)){FILE_DISPOSITION_INFO disposition={TRUE};SetFileInformationByHandle(new_file,FileDispositionInfo,&disposition,sizeof(disposition));}
    if(new_file!=INVALID_HANDLE_VALUE)CloseHandle(new_file);if(old!=INVALID_HANDLE_VALUE)CloseHandle(old);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    /* Backup is never deleted. If restoration fails it remains recoverable. */
    (void)backed_up;if(random[0])result_log(target_dir,random,result);
    if(event)CloseHandle(event);if(parent)CloseHandle(parent);if(source_lock!=INVALID_HANDLE_VALUE)CloseHandle(source_lock);if(target_lock!=INVALID_HANDLE_VALUE)CloseHandle(target_lock);return result;
}
int UpdateRunHelper(int argc,wchar_t **argv){return UpdateRunHelperUsing(argc,argv,NULL);}
