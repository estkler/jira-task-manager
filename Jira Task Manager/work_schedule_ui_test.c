#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <wtsapi32.h>
static int presence_query_mode;static BOOL presence_registration_ready=TRUE;
static BOOL WINAPI PresenceQuery(HANDLE server,DWORD session,WTS_INFO_CLASS info,LPWSTR *buffer,DWORD *bytes)
{
    if(!presence_query_mode)return WTSQuerySessionInformationW(server,session,info,buffer,bytes);
    *buffer=NULL;*bytes=0;if(presence_query_mode==1)return FALSE;
    WTSINFOEXW *state=LocalAlloc(LPTR,sizeof(*state));assert(state);state->Level=1;
    state->Data.WTSInfoExLevel1.SessionFlags=presence_query_mode==2?WTS_SESSIONSTATE_UNLOCK:presence_query_mode==3?WTS_SESSIONSTATE_LOCK:WTS_SESSIONSTATE_UNKNOWN;
    *buffer=(LPWSTR)state;*bytes=sizeof(*state);return TRUE;
}
static void WINAPI PresenceFree(PVOID buffer){if(presence_query_mode)LocalFree(buffer);else WTSFreeMemory(buffer);}
static BOOL WINAPI PresenceRegister(HWND window,DWORD flags){if(!presence_registration_ready){SetLastError(RPC_S_INVALID_BINDING);return FALSE;}return TRUE;}
#define WTSQuerySessionInformationW PresenceQuery
#define WTSFreeMemory PresenceFree
#define WTSRegisterSessionNotification PresenceRegister
static BOOL NoScheduleAccount(void){return FALSE;}
static BOOL NoScheduleUrl(wchar_t *url,size_t n){if(n)url[0]=0;return FALSE;}
static BOOL NoScheduleStartup(void){return FALSE;}
static BOOL NoScheduleStartupWrite(BOOL enabled){assert(!enabled);return TRUE;}
#define TaskManagerStartupEnabled NoScheduleStartup
#define TaskManagerSetStartup NoScheduleStartupWrite
static BOOL HiddenScheduleShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
#define ShowWindow HiddenScheduleShow
#define JiraHasAccount NoScheduleAccount
#define JiraGetUrl NoScheduleUrl
static void *presence_job;
static HANDLE WINAPI PresenceThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID argument,DWORD d,LPDWORD e)
{presence_job=argument;return CreateEventW(NULL,TRUE,FALSE,NULL);}
#define CreateThread PresenceThread
#include "native.c"
static void CaptureSchedule(HWND window,const wchar_t *path)
{
    RECT bounds;GetClientRect(window,&bounds);HDC target=GetDC(window),dc=CreateCompatibleDC(target);
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bounds.right;info.bmiHeader.biHeight=-bounds.bottom;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(target,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);
    HGDIOBJ old=SelectObject(dc,bitmap);SendMessageW(window,WM_ERASEBKGND,(WPARAM)dc,0);SendMessageW(window,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))if(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE){
        RECT r;GetWindowRect(child,&r);MapWindowPoints(NULL,window,(POINT*)&r,2);int saved=SaveDC(dc);
        SetViewportOrgEx(dc,r.left,r.top,NULL);IntersectClipRect(dc,0,0,r.right-r.left,r.bottom-r.top);
        SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);
    }
    BITMAPFILEHEADER file={.bfType=0x4d42,.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)};
    DWORD size=bounds.right*bounds.bottom*4;file.bfSize=file.bfOffBits+size;
    HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(output!=INVALID_HANDLE_VALUE);DWORD wrote;
    assert(WriteFile(output,&file,sizeof(file),&wrote,NULL)&&WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&wrote,NULL)&&WriteFile(output,pixels,size,&wrote,NULL));CloseHandle(output);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,target);
}
int main(int argc,char **argv)
{
    g_instance=GetModuleHandleW(NULL);INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    g_dpi=96;g_russian=TRUE;g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"tws",0,g_settings_path);
    UpdateFonts();CreateControls();
    g_presence_next_check=~0ULL;
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"WorkScheduleFixture"};RegisterClassW(&cls);
    RECT outer=SettingsOuterRect();
    HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,30000,30000,outer.right-outer.left,outer.bottom-outer.top,g_window,NULL,g_instance,NULL);assert(settings);
    HWND pause=GetDlgItem(settings,270);assert(pause);
    assert((GetWindowLongPtrW(pause,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX);
    assert(SendMessageW(pause,BM_GETCHECK,0,0)==BST_CHECKED&&g_pause_when_locked);
    if(argc>1){
        SetWindowTextW(s_url,L"https://jira.example.test/");s_connected=TRUE;SetWindowTextW(SettingsItem(217),L"Demo account");
        UpdateSettingsConnectionCaption();LayoutSettings();
        RecreateBrushes();CaptureSchedule(settings,L"bin/refresh-settings-light.bmp");
        g_dark=TRUE;RecreateBrushes();UpdateSettingsCheckboxThemes();CaptureSchedule(settings,L"bin/refresh-settings-dark.bmp");
        g_dark=FALSE;RecreateBrushes();UpdateSettingsCheckboxThemes();
        SendMessageW(settings,WM_COMMAND,274,0);assert(s_replacing_token);
        s_hover=SettingsItem(274);CaptureSchedule(settings,L"bin/token-replacement-light.bmp");
        g_dark=TRUE;RecreateBrushes();UpdateSettingsCheckboxThemes();CaptureSchedule(settings,L"bin/token-replacement-dark.bmp");
        g_dark=FALSE;s_hover=NULL;RecreateBrushes();UpdateSettingsCheckboxThemes();SendMessageW(settings,WM_COMMAND,274,0);assert(!s_replacing_token);
    }
    SendMessageW(pause,BM_SETCHECK,BST_UNCHECKED,0);assert(g_pause_when_locked&&UpdateSettingsDirty());
    assert(SaveNativePreferences()&&!g_pause_when_locked&&!NativeAutoRefreshPauseLoad(g_settings_path));
    ResetSettingsDraft();assert(SendMessageW(pause,BM_GETCHECK,0,0)==BST_CHECKED&&!g_pause_when_locked);
    g_snapshot.refresh_cache=TRUE;SetWindowTextW(SettingsItem(252),L"fixture@example.org");
    assert(SaveNativePreferences()&&g_pause_when_locked&&!g_snapshot.refresh_cache);
    g_auto_was_allowed=TRUE;g_auto_refresh_due=0;g_jira_stale=FALSE;
    WindowProcedure(g_window,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);
    assert(g_refresh_presence.locked&&InterlockedCompareExchange(&g_auto_load_cancelled,0,0));
    AutomaticRefreshTick();assert(!g_auto_was_allowed&&!g_jira_busy&&!presence_job);
    /* Manual refresh bypasses the presence gate; transport is never executed. */
    assert(StartJiraJob(0,NULL,NULL,(POINT){0}));JiraJob *manual=presence_job;presence_job=NULL;assert(!manual->automatic);
    manual->paused=TRUE;FinishJiraJob(manual);
    WindowProcedure(g_window,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
    assert(!g_refresh_presence.locked&&InterlockedCompareExchange(&g_auto_load_cancelled,0,0));
    AutomaticRefreshTick();assert(!g_jira_busy&&g_auto_was_allowed&&g_auto_refresh_due>GetTickCount64());
    g_auto_refresh_due=0;AutomaticRefreshTick();assert(g_jira_busy&&presence_job);
    JiraJob *automatic=presence_job;presence_job=NULL;assert(automatic->automatic&&!InterlockedCompareExchange(&g_auto_load_cancelled,0,0));
    WindowProcedure(g_window,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);
    assert(InterlockedCompareExchange(&g_auto_load_cancelled,0,0));
    g_jira_stale=FALSE;wcscpy_s(g_jira_message,256,L"retained");automatic->paused=TRUE;FinishJiraJob(automatic);
    assert(!g_jira_stale&&!wcscmp(g_jira_message,L"retained"));
    WindowProcedure(g_window,WM_POWERBROADCAST,PBT_APMSUSPEND,0);assert(g_refresh_presence.suspended);
    g_pause_when_locked=FALSE;AutomaticRefreshTick();assert(!g_jira_busy);
    g_refresh_presence.suspended=FALSE;presence_query_mode=1;presence_registration_ready=FALSE;g_session_notifications_registered=FALSE;g_presence_next_check=0;
    RefreshWindowsPresence();assert(g_refresh_presence.locked&&!g_session_notifications_registered&&g_presence_next_check>GetTickCount64());
    presence_query_mode=2;presence_registration_ready=TRUE;g_presence_next_check=0;
    RefreshWindowsPresence();assert(!g_refresh_presence.locked&&g_session_notifications_registered);
    presence_query_mode=4;g_presence_next_check=0;RefreshWindowsPresence();assert(g_refresh_presence.locked&&g_presence_next_check<=GetTickCount64()+5000);
    presence_query_mode=3;g_presence_next_check=0;RefreshWindowsPresence();assert(g_refresh_presence.locked);
    DestroyWindow(settings);DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Lock pause checkbox, staged preferences, manual bypass and automatic cancellation passed");
}
