/* Breaks caught: replacing without identity/version checks, losing old bytes on
   rename failure, updating while the exact parent is alive, missing restart. */
#include "native_update.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include <winioctl.h>
static UpdateRelease old_release,new_release;
static const wchar_t *old_fixture,*new_fixture;
static void hash(const wchar_t *s,BYTE *out){for(int i=0;i<32;i++){unsigned n;assert(swscanf(s+2*i,L"%2x",&n)==1);out[i]=(BYTE)n;}}
static void verify(const wchar_t *path,const UpdateRelease *r){HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);assert(f!=INVALID_HANDLE_VALUE);assert(UpdateVerifyFile(f,r)==UPDATE_OK);CloseHandle(f);}
static void cleanup(UpdateStage *s){
    wchar_t pattern[MAX_PATH],path[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\*",s->directory);WIN32_FIND_DATAW data;HANDLE find=FindFirstFileW(pattern,&data);
    if(find!=INVALID_HANDLE_VALUE){do{if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){swprintf(path,MAX_PATH,L"%ls\\%ls",s->directory,data.cFileName);DeleteFileW(path);}}while(FindNextFileW(find,&data));FindClose(find);}
    UpdateStageCleanup(s);
}
static void integration(BOOL rollback,BOOL timeout,BOOL locked,int collision,BOOL restart_failed,BOOL same_version){
    UpdateStage source,target;assert(UpdateStageCreate(&source)==UPDATE_OK);assert(UpdateStageCreate(&target)==UPDATE_OK);
    assert(CopyFileW(new_fixture,source.executable,TRUE));wchar_t installed[MAX_PATH];swprintf(installed,MAX_PATH,L"%ls\\Задача менеджер.exe",target.directory);assert(CopyFileW(same_version?new_fixture:old_fixture,installed,TRUE));
    wchar_t foreign[MAX_PATH]={0};if(collision){swprintf(foreign,MAX_PATH,L"%ls\\Jira-update-00000000000000000000000000000000.%ls.exe",target.directory,collision==1?L"candidate":L"backup");HANDLE f=CreateFileW(foreign,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD written;assert(WriteFile(f,"sentinel",8,&written,NULL)&&written==8);CloseHandle(f);}
    SetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_RENAME",rollback?L"1":NULL);SetEnvironmentVariableW(L"JTM_FIXTURE_SHORT_WAIT",timeout?L"1":NULL);
    SetEnvironmentVariableW(L"JTM_FIXTURE_FIXED_NONCE",collision?L"1":NULL);SetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_START",restart_failed?L"1":NULL);
    HANDLE lock=locked?CreateFileW(installed,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL):INVALID_HANDLE_VALUE;
    wchar_t command[2048];swprintf(command,_countof(command),L"\"%ls\" --fixture-parent \"%ls\" %u",installed,source.executable,timeout?750:0);
    STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;assert(CreateProcessW(installed,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,target.directory,&si,&pi));CloseHandle(pi.hThread);
    SetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_RENAME",NULL);SetEnvironmentVariableW(L"JTM_FIXTURE_SHORT_WAIT",NULL);
    SetEnvironmentVariableW(L"JTM_FIXTURE_FIXED_NONCE",NULL);SetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_START",NULL);
    assert(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0);CloseHandle(pi.hProcess);if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    wchar_t log_pattern[MAX_PATH];swprintf(log_pattern,MAX_PATH,L"%ls\\Jira-update-*.log",target.directory);
    WIN32_FIND_DATAW data;BOOL logged=FALSE;for(int i=0;i<100;i++){HANDLE f=FindFirstFileW(log_pattern,&data);if(f!=INVALID_HANDLE_VALUE){FindClose(f);logged=TRUE;break;}Sleep(50);}assert(logged);
    verify(installed,same_version?&new_release:rollback||timeout||locked||collision||restart_failed?&old_release:&new_release);
    if(collision){HANDLE f=CreateFileW(foreign,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(f!=INVALID_HANDLE_VALUE);char bytes[9]={0};DWORD got;assert(ReadFile(f,bytes,8,&got,NULL)&&got==8&&!memcmp(bytes,"sentinel",8));CloseHandle(f);}
    if(!rollback&&!timeout&&!locked&&!collision&&!restart_failed&&!same_version){
        wchar_t pattern[MAX_PATH],backup[MAX_PATH],marker[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\Jira-update-*.backup.exe",target.directory);
        HANDLE f=FindFirstFileW(pattern,&data);assert(f!=INVALID_HANDLE_VALUE);swprintf(backup,MAX_PATH,L"%ls\\%ls",target.directory,data.cFileName);verify(backup,&old_release);assert(!FindNextFileW(f,&data));FindClose(f);
        swprintf(marker,MAX_PATH,L"%ls\\started-30",target.directory);BOOL started=FALSE;for(int i=0;i<100;i++){if(GetFileAttributesW(marker)!=INVALID_FILE_ATTRIBUTES){started=TRUE;break;}Sleep(50);}assert(started);
    }
    Sleep(100);cleanup(&source);cleanup(&target);
}
static void test_apply_identity(void){
    UpdateStage s;assert(UpdateStageCreate(&s)==UPDATE_OK);assert(CopyFileW(new_fixture,s.executable,TRUE));UpdateApplyRequest r={0};r.parent_pid=GetCurrentProcessId();FILETIME c,e,k,u;assert(GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u));r.parent_created=((ULONGLONG)c.dwHighDateTime<<32)|c.dwLowDateTime;GetModuleFileNameW(NULL,r.target,MAX_PATH);r.release=new_release;HANDLE ready=NULL;
    r.parent_created++;assert(UpdateLaunchHelper(&s,&r,&ready)==UPDATE_INVALID&&!ready);r.parent_created--;r.parent_pid=0xffffffff;assert(UpdateLaunchHelper(&s,&r,&ready)==UPDATE_INVALID&&!ready);
    r.parent_pid=GetCurrentProcessId();wcscpy_s(r.target,MAX_PATH,s.executable);assert(UpdateLaunchHelper(&s,&r,&ready)==UPDATE_INVALID&&!ready);
    wcscpy_s(r.target,MAX_PATH,L"C:\\not-an-installation\\missing.exe");assert(UpdateLaunchHelper(&s,&r,&ready)==UPDATE_INVALID&&!ready);
    GetModuleFileNameW(NULL,r.target,MAX_PATH);assert(DeleteFileW(s.executable));assert(UpdateLaunchHelper(&s,&r,&ready)!=UPDATE_OK&&!ready);
    UpdateStage b;assert(UpdateStageCreate(&b)==UPDATE_OK);assert(CopyFileW(new_fixture,b.executable,TRUE));
    struct {DWORD tag;WORD length,reserved,sub_offset,sub_length,print_offset,print_length;wchar_t path[2*MAX_PATH+8];} mount={0};
    mount.tag=IO_REPARSE_TAG_MOUNT_POINT;swprintf(mount.path,_countof(mount.path),L"\\??\\%ls",b.directory);mount.sub_length=(WORD)(wcslen(mount.path)*2);mount.print_offset=mount.sub_length+2;wcscpy_s((wchar_t*)((BYTE*)mount.path+mount.print_offset),MAX_PATH,b.directory);mount.print_length=(WORD)(wcslen(b.directory)*2);mount.length=8+mount.print_offset+mount.print_length+2;
    HANDLE junction=CreateFileW(s.directory,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);assert(junction!=INVALID_HANDLE_VALUE);DWORD got;assert(DeviceIoControl(junction,FSCTL_SET_REPARSE_POINT,&mount,8+mount.length,NULL,0,&got,NULL));CloseHandle(junction);
    assert(UpdateLaunchHelper(&s,&r,&ready)==UPDATE_INVALID&&!ready);verify(b.executable,&new_release);
    CloseHandle(s.directory_lock);s.directory_lock=INVALID_HANDLE_VALUE;assert(RemoveDirectoryW(s.directory));UpdateStageCleanup(&s);cleanup(&b);
    cleanup(&s);
}
static void test_apply_success(void){integration(FALSE,FALSE,FALSE,0,FALSE,FALSE);}
static void test_apply_rollback(void){integration(TRUE,FALSE,FALSE,0,FALSE,FALSE);integration(FALSE,FALSE,FALSE,0,TRUE,FALSE);}
static void test_apply_timeout(void){integration(FALSE,TRUE,FALSE,0,FALSE,FALSE);}
int wmain(int argc,wchar_t **argv){
    assert(argc==7);old_fixture=argv[1];new_fixture=argv[2];old_release.version=(UpdateVersion){0,9,29};new_release.version=(UpdateVersion){0,9,30};old_release.size=wcstoul(argv[5],NULL,10);new_release.size=wcstoul(argv[6],NULL,10);hash(argv[3],old_release.sha256);hash(argv[4],new_release.sha256);
    test_apply_identity();test_apply_success();test_apply_rollback();test_apply_timeout();integration(FALSE,FALSE,TRUE,0,FALSE,FALSE);integration(FALSE,FALSE,FALSE,1,FALSE,FALSE);integration(FALSE,FALSE,FALSE,2,FALSE,FALSE);integration(FALSE,FALSE,FALSE,0,FALSE,TRUE);
    puts("Update apply identity, success, rollback, timeout and locked-target tests passed");return 0;
}
