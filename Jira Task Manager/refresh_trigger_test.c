#include <windows.h>
#include <assert.h>
#include <stdio.h>
static int refresh_posts,workers,signouts;static void *trigger_job;
static BOOL TriggerSignOut(void){signouts++;return TRUE;}
static int WINAPI TriggerConfirm(HWND w,LPCWSTR t,LPCWSTR c,UINT f){return IDYES;}
static BOOL TriggerAccount(void){return FALSE;}
static BOOL TriggerUrl(wchar_t *url,size_t count){wcscpy_s(url,count,L"https://fixture.invalid/");return TRUE;}
static BOOL TriggerStartup(void){return FALSE;}
static BOOL TriggerSetStartup(BOOL enabled){assert(!enabled);return TRUE;}
static BOOL TriggerShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
static BOOL WINAPI TriggerPost(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_COMMAND&&LOWORD(wp)==1009){refresh_posts++;return TRUE;}return PostMessageW(window,message,wp,lp);
}
static HANDLE WINAPI TriggerThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID arg,DWORD d,LPDWORD e){workers++;trigger_job=arg;return CreateEventW(NULL,TRUE,FALSE,NULL);}
#define JiraHasAccount TriggerAccount
#define JiraSignOut TriggerSignOut
#define MessageBoxW TriggerConfirm
#define JiraGetUrl TriggerUrl
#define TaskManagerStartupEnabled TriggerStartup
#define TaskManagerSetStartup TriggerSetStartup
#define ShowWindow TriggerShow
#define PostMessageW TriggerPost
#define CreateThread TriggerThread
#include "native.c"
static HWND create_settings(void){RECT r=SettingsOuterRect();return CreateWindowW(L"RefreshTriggerSettings",L"",WS_CAPTION|WS_SYSMENU,30000,30000,r.right-r.left,r.bottom-r.top,g_window,NULL,g_instance,NULL);}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    wchar_t dir[MAX_PATH];GetTempPathW(MAX_PATH,dir);GetTempFileNameW(dir,L"rtr",0,g_settings_path);
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"RefreshTriggerSettings"};RegisterClassW(&cls);
    NativeNotificationsLoad(g_settings_path,&g_notifications);g_jira_stale=g_jira_busy=FALSE;g_presence_next_check=~0ULL;
    g_refresh_presence=(NativeRefreshPresence){0};g_auto_was_allowed=TRUE;g_refresh_minutes=30;
    ULONGLONG due=GetTickCount64()+1800000;g_auto_refresh_due=due;
    HWND settings=create_settings();assert(settings);SendMessageW(SettingsItem(213),TBM_SETPOS,TRUE,17);
    SendMessageW(settings,WM_COMMAND,IDOK,0);assert(!IsWindow(settings));
    assert(refresh_posts==0&&workers==0&&!g_jira_stale&&"Saving tray/interface preferences must not refresh Jira");
    assert(g_auto_was_allowed&&g_auto_refresh_due==due&&"UI-only preferences must not rearm an immediate automatic refresh");
    settings=create_settings();SendMessageW(s_interval,TBM_SETPOS,TRUE,25);SendMessageW(settings,WM_COMMAND,IDOK,0);
    assert(!refresh_posts&&!workers&&g_refresh_minutes==25&&g_auto_was_allowed);
    assert(g_auto_refresh_due>=GetTickCount64()+24*60000&&"An interval change schedules the next normal interval, not a near-immediate poll");
    settings=create_settings();SetWindowTextW(SettingsItem(252),L"new-filter@example.test");g_snapshot.refresh_cache=TRUE;
    SendMessageW(settings,WM_COMMAND,IDOK,0);assert(refresh_posts==1&&!workers&&!g_snapshot.refresh_cache&&"Changed comment filters still require one refresh");
    refresh_posts=0;g_jira_stale=FALSE;settings=create_settings();SendMessageW(settings,WM_COMMAND,IDCANCEL,0);assert(!refresh_posts&&!workers);
    g_comments_mode=0;ApplyCommentsMode(1);ApplyCommentsMode(2);ApplyCommentsMode(0);
    assert(!workers&&!refresh_posts&&"Changing comment display uses the existing snapshot");
    g_auto_was_allowed=FALSE;g_auto_refresh_due=GetTickCount64()+5000;WindowProcedure(g_window,WM_COMMAND,IDC_REFRESH,0);
    assert(workers==1&&trigger_job&&g_auto_refresh_due>=GetTickCount64()+24*60000&&"Manual refresh must reset the next automatic interval");
    due=g_auto_refresh_due;AutomaticRefreshTick();
    assert(g_auto_was_allowed&&g_auto_refresh_due==due&&"Manual refresh just after startup/unlock must not be followed by another near-immediate poll");
    JiraJob *job=trigger_job;JiraFreeSnapshot(&job->baseline);free(job->status_keys);free(job->status_writes.items);free(job);CloseHandle(g_jira_thread);g_jira_thread=NULL;g_jira_busy=FALSE;
    JiraConnection cache={0};assert(JiraConfigureSnapshotCache(&cache,g_settings_path));
    HANDLE cache_file=CreateFileW(cache.snapshot_cache_path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);assert(cache_file!=INVALID_HANDLE_VALUE);CloseHandle(cache_file);
    assert(SetFileAttributesW(cache.snapshot_cache_path,FILE_ATTRIBUTE_READONLY));settings=create_settings();s_has_account=s_connected=TRUE;
    SendMessageW(settings,WM_COMMAND,215,0);
    assert(!signouts&&s_has_account&&"Failed cache cleanup must be visible and retryable before reporting sign-out");
    wchar_t message[256];GetWindowTextW(s_note,message,256);assert(message[0]);
    assert(SetFileAttributesW(cache.snapshot_cache_path,FILE_ATTRIBUTE_NORMAL));SendMessageW(settings,WM_COMMAND,215,0);
    assert(signouts==1&&!s_has_account&&GetFileAttributesW(cache.snapshot_cache_path)==INVALID_FILE_ATTRIBUTES);
    SendMessageW(settings,WM_COMMAND,IDCANCEL,0);
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("UI-only settings/display cause no Jira reads; filter changes refresh once; manual refresh resets interval");
}
