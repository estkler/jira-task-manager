#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "native_update.h"
static ULONGLONG auto_now=10000;static void *auto_job;static int auto_workers,auto_notices,auto_checks;
static UpdateStatus auto_status=UPDATE_OK;static int auto_patch=1;
static BOOL auto_notice_ok=TRUE;
static ULONGLONG WINAPI AutoClock(void){return auto_now;}
static BOOL AutoAccount(void){return FALSE;}
static BOOL AutoUrl(wchar_t *url,size_t n){if(n)url[0]=0;return FALSE;}
static BOOL WINAPI AutoShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
static BOOL WINAPI AutoForeground(HWND window){return TRUE;}
static HANDLE WINAPI AutoThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID value,DWORD d,LPDWORD e)
{assert(!auto_job);auto_job=value;auto_workers++;return CreateEventW(NULL,TRUE,FALSE,NULL);}
static BOOL WINAPI AutoNotice(DWORD message,PNOTIFYICONDATAW notice);
static UpdateStatus AutoCheck(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out)
{auto_checks++;*out=(UpdateRelease){.version={current.major,current.minor,current.patch+(DWORD)auto_patch},.asset_id=42,.size=100};return auto_status;}
#define GetTickCount64 AutoClock
#define CreateThread AutoThread
#define Shell_NotifyIconW AutoNotice
#define UpdateCheck AutoCheck
#define JiraHasAccount AutoAccount
#define JiraGetUrl AutoUrl
#define ShowWindow AutoShow
#define SetForegroundWindow AutoForeground
#include "native.c"
static BOOL WINAPI AutoNotice(DWORD message,PNOTIFYICONDATAW notice)
{if(message==NIM_MODIFY&&(notice->uFlags&NIF_INFO)){if(!auto_notice_ok)return FALSE;if(wcsstr(notice->szInfoTitle,L"Jira Tasklet"))auto_notices++;assert(notice->dwInfoFlags&NIIF_NOSOUND);}return TRUE;}
static void complete_auto(void)
{UpdateWork *job=auto_job;auto_job=NULL;assert(job&&job->action==0&&"Background checks must never download or install");JiraSnapshot untouched=g_snapshot;UpdateUiWorker(job);MSG result;assert(PeekMessageW(&result,g_window,WM_UPDATE_RESULT,WM_UPDATE_RESULT,PM_REMOVE));assert(UpdateUiHandleMessage(result.message,result.wParam,result.lParam));assert(!memcmp(&untouched,&g_snapshot,sizeof(untouched)));}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    wchar_t dir[MAX_PATH];GetTempPathW(MAX_PATH,dir);GetTempFileNameW(dir,L"aut",0,g_settings_path);
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();g_refresh_presence=(NativeRefreshPresence){0};
    UpdateUiAutoInit();UpdateUiAutoTick();assert(!auto_workers);auto_now+=30000;
    g_refresh_presence.locked=TRUE;UpdateUiAutoTick();assert(!auto_workers);g_refresh_presence.locked=FALSE;
    g_refresh_presence.suspended=TRUE;UpdateUiAutoTick();assert(!auto_workers);g_refresh_presence.suspended=FALSE;
    g_jira_busy=TRUE;UpdateUiAutoTick();assert(!auto_workers);g_jira_busy=FALSE;
    HWND settings=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,500,400,g_window,NULL,g_instance,NULL);
    CreateWindowW(L"BUTTON",L"",WS_CHILD,0,0,200,30,settings,(HMENU)242,g_instance,NULL);UpdateUiAttach(settings);
    wchar_t label[80];GetWindowTextW(GetDlgItem(settings,242),label,80);assert(!wcscmp(label,L"Обновить"));
    UpdateUiAutoTick();assert(auto_workers==1&&u_work&&u_work->background&&!UpdateUiBusy());
    UpdateUiCancel();DestroyWindow(settings);assert(u_work&&"Closing Settings must retain background discovery");complete_auto();
    assert(u_state==UPDATE_UI_AVAILABLE&&auto_notices==1&&g_update_notice&&auto_checks==1);
    WindowProcedure(g_window,WM_TRAYICON,0,NIN_BALLOONUSERCLICK);assert(s_window&&s_tab==1&&!g_update_notice);SendMessageW(s_window,WM_COMMAND,IDCANCEL,0);
    g_update_notice=TRUE;JiraItem notice_task={0};wcscpy_s(notice_task.key,80,L"FIXTURE-1");ShowJiraActivityNotification(1,&notice_task,0,NULL,NULL,NULL);assert(!g_update_notice&&"Jira balloons retain their own click behavior");
    UpdateUiAutoTick();assert(auto_workers==1);auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();assert(auto_workers==2);complete_auto();assert(auto_notices==1);
    /* Read suppression from the profile even after the controller is reinitialized. */
    UpdateUiAutoInit();auto_now+=30000;UpdateUiAutoTick();complete_auto();assert(auto_notices==1);
    auto_patch=2;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(auto_notices==2);
    auto_status=UPDATE_NETWORK;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(u_state==UPDATE_UI_AVAILABLE&&auto_notices==2);
    int before=auto_workers;auto_now+=3599000;UpdateUiAutoTick();assert(auto_workers==before);auto_now+=1000;UpdateUiAutoTick();assert(auto_workers==before+1);complete_auto();
    /* A current version gets no notice, and language changes keep the compact action. */
    auto_status=UPDATE_NONE;auto_patch=0;auto_now+=3600000;UpdateUiAutoTick();complete_auto();assert(u_state==UPDATE_UI_CURRENT&&auto_notices==2);
    auto_status=UPDATE_NETWORK;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(u_state==UPDATE_UI_CURRENT&&"Offline check must not offer reinstalling a release already current");
    settings=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,500,400,g_window,NULL,g_instance,NULL);CreateWindowW(L"BUTTON",L"",WS_CHILD,0,0,200,30,settings,(HMENU)242,g_instance,NULL);g_russian=FALSE;UpdateUiAttach(settings);GetWindowTextW(GetDlgItem(settings,242),label,80);assert(!wcscmp(label,L"Update"));
    auto_status=UPDATE_OK;auto_patch=3;auto_notice_ok=FALSE;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(auto_notices==2);
    auto_notice_ok=TRUE;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(auto_notices==3&&"Failed notice delivery must not suppress the next successful notice");
    assert(SetFileAttributesW(g_settings_path,FILE_ATTRIBUTE_READONLY));auto_patch=4;auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(auto_notices==4);
    assert(SetFileAttributesW(g_settings_path,FILE_ATTRIBUTE_NORMAL));auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();complete_auto();assert(auto_notices==4&&"Notification persistence failure must not repeat the same notice within this process");
    auto_now+=24ULL*60*60*1000;UpdateUiAutoTick();assert(auto_job);UpdateWork *pending=auto_job;auto_job=NULL;UpdateUiShutdown();assert(!u_work);UpdateUiWorker(pending);assert(!PeekMessageW(&(MSG){0},g_window,WM_UPDATE_RESULT,WM_UPDATE_RESULT,PM_REMOVE));
    DestroyWindow(settings);DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Automatic updates: delayed startup/daily checks, presence/busy deferral, quiet retry, Settings lifetime, persistent single-version notice, manual-only download/install and shutdown passed");
}
