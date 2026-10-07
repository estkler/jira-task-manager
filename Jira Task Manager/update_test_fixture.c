#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include "native_update.h"
#include "native_update_apply.h"
static BOOL fixture_move(const wchar_t *from,const wchar_t *to){
    static int calls;wchar_t flag[8];
    if(GetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_RENAME",flag,8)&&++calls==2){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return MoveFileExW(from,to,MOVEFILE_WRITE_THROUGH);
}
static BOOL fixture_nonce(wchar_t out[33]){wcscpy_s(out,33,L"00000000000000000000000000000000");return TRUE;}
static BOOL fixture_start_failed(const wchar_t *target,const wchar_t *directory){(void)target;(void)directory;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
static void marker(const wchar_t *name){
    wchar_t path[MAX_PATH];GetModuleFileNameW(NULL,path,MAX_PATH);wchar_t *last=wcsrchr(path,L'\\');if(!last)return;
    last[1]=0;wcscat_s(path,MAX_PATH,name);HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show){
    (void)instance;(void)previous;(void)command;(void)show;int argc;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);int result=0;
    if(argc>1&&!wcscmp(argv[1],L"--apply-update")){
        wchar_t flag[8];UpdateApplyOps ops={GetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_RENAME",flag,8)?fixture_move:NULL,GetEnvironmentVariableW(L"JTM_FIXTURE_FAIL_START",flag,8)?fixture_start_failed:NULL,GetEnvironmentVariableW(L"JTM_FIXTURE_SHORT_WAIT",flag,8)?125:60000,GetEnvironmentVariableW(L"JTM_FIXTURE_FIXED_NONCE",flag,8)?fixture_nonce:NULL};
        result=UpdateRunHelperUsing(argc,argv,&ops);
    }else if(argc==4&&!wcscmp(argv[1],L"--fixture-parent")){
        UpdateStage stage={0};wcscpy_s(stage.executable,MAX_PATH,argv[2]);wcscpy_s(stage.directory,MAX_PATH,argv[2]);*wcsrchr(stage.directory,L'\\')=0;
        stage.directory_lock=CreateFileW(stage.directory,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
        UpdateApplyRequest request={0};request.parent_pid=GetCurrentProcessId();FILETIME created,exit,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user);
        request.parent_created=((ULONGLONG)created.dwHighDateTime<<32)|created.dwLowDateTime;GetModuleFileNameW(NULL,request.target,MAX_PATH);
        HANDLE f=CreateFileW(stage.executable,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);result=UpdateInspectFile(f,&request.release);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
        HANDLE ready=NULL;if(result==UPDATE_OK)result=UpdateLaunchHelper(&stage,&request,&ready);
        if(result==UPDATE_OK&&WaitForSingleObject(ready,5000)==WAIT_OBJECT_0){marker(L"parent-ready");Sleep(wcstoul(argv[3],NULL,10));}
        else result=20;
        if(ready)CloseHandle(ready);if(stage.directory_lock!=INVALID_HANDLE_VALUE)CloseHandle(stage.directory_lock);
    }else marker(L"started-30");
    LocalFree(argv);return result;
}
