#include <windows.h>
#include <wtsapi32.h>
#include <shellapi.h>
#include <assert.h>
#include <stdio.h>
static int workers;static void *pending;static BOOL locked;
static BOOL FixtureAccount(void){return TRUE;}
static BOOL FixtureUrl(wchar_t *url,size_t n){wcscpy_s(url,n,L"https://fixture.invalid/");return TRUE;}
static HANDLE WINAPI FixtureThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID p,DWORD d,LPDWORD e){workers++;pending=p;return CreateEventW(NULL,TRUE,FALSE,NULL);}
static BOOL WINAPI FixtureTray(DWORD message,PNOTIFYICONDATAW data){return TRUE;}
static BOOL WINAPI FixtureRegister(HWND window,DWORD flags){return TRUE;}
static BOOL WINAPI FixturePresence(HANDLE server,DWORD session,WTS_INFO_CLASS info,LPWSTR *buffer,DWORD *bytes){
    WTSINFOEXW *state=LocalAlloc(LPTR,sizeof(*state));state->Level=1;
    state->Data.WTSInfoExLevel1.SessionFlags=locked?WTS_SESSIONSTATE_LOCK:WTS_SESSIONSTATE_UNLOCK;
    *buffer=(LPWSTR)state;*bytes=sizeof(*state);return TRUE;
}
#define JiraHasAccount FixtureAccount
#define JiraGetUrl FixtureUrl
#define CreateThread FixtureThread
#define Shell_NotifyIconW FixtureTray
#define WTSQuerySessionInformationW FixturePresence
#define WTSFreeMemory LocalFree
#define WTSRegisterSessionNotification FixtureRegister
#include "native.c"
static void pump(void){MSG m;while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){if(m.message==WM_QUIT)continue;TranslateMessage(&m);DispatchMessageW(&m);}}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_show_all_tab=FALSE;
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"slu",0,g_settings_path);RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSW cls={.lpfnWndProc=WindowProcedure,.hInstance=g_instance,.lpszClassName=L"StartupLoadFixture"};assert(RegisterClassW(&cls));
    g_window=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);
    ShowWindow(g_window,SW_SHOWNOACTIVATE);pump();
    assert(workers==1&&g_jira_busy&&"Opening the first window must start one immediate worker without jitter");
    wchar_t text[128];GetWindowTextW(g_tab_current,text,128);assert(!wcscmp(text,L"Текущие"));
    SendMessageW(g_window,WM_SHOWWINDOW,TRUE,0);SendMessageW(g_window,WM_COMMAND,IDC_REFRESH,0);pump();assert(workers==1);
    JiraJob *job=pending;job->success=FALSE;wcscpy_s(job->error,256,L"Fixture network failure");FinishJiraJob(job);pending=NULL;
    assert(!g_jira_busy&&g_jira_message[0]);
    assert(IsWindowVisible(g_initial_retry));
    SendMessageW(g_list,WM_COMMAND,IDC_INITIAL_RETRY,0);pump();assert(workers==2&&!IsWindowVisible(g_initial_retry));
    job=pending;job->success=job->loaded=TRUE;wcscpy_s(job->snapshot.account,256,L"Fixture");FinishJiraJob(job);pending=NULL;
    GetWindowTextW(g_tab_current,text,128);assert(!wcscmp(text,L"Текущие 0")&&!g_jira_stale);
    ShowWindow(g_window,SW_HIDE);g_updated_time=(SYSTEMTIME){0};g_jira_message[0]=0;SetInitialView(INITIAL_WAIT);
    locked=TRUE;g_presence_next_check=0;ShowWindow(g_window,SW_SHOWNOACTIVATE);pump();assert(workers==2&&g_initial_view==INITIAL_WAIT);
    locked=FALSE;g_presence_next_check=0;SendMessageW(g_window,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);AutomaticRefreshTick();assert(workers==3&&g_jira_busy);
    job=pending;job->automatic=job->paused=TRUE;FinishJiraJob(job);assert(g_initial_view==INITIAL_WAIT&&!g_jira_busy);
    AutomaticRefreshTick();assert(workers==4);job=pending;job->success=job->loaded=TRUE;FinishJiraJob(job);pending=NULL;
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_dark=dark;UpdateFonts();RecreateBrushes();LayoutControls(D(510),D(420));
        SetInitialView(INITIAL_LOADING);HDC screen=GetDC(g_list),dc=CreateCompatibleDC(screen);HBITMAP b=CreateCompatibleBitmap(screen,D(480),D(250));HGDIOBJ old=SelectObject(dc,b);
        RECT body={0,0,D(480),D(250)};FillRect(dc,&body,g_panel_brush);DrawInitialView(dc,body);GdiFlush();
        int ink=0;for(int y=D(60);y<D(150);y++)for(int x=D(50);x<D(430);x++)if(GetPixel(dc,x,y)!=PanelColor())ink++;
        assert(ink>0);SelectObject(dc,old);DeleteObject(b);DeleteDC(dc);ReleaseDC(g_list,screen);
    }
    SetInitialView(INITIAL_READY);
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Immediate first visible load, coalescing, failure and real empty result passed");
}
