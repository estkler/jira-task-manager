#include <windows.h>
#include <assert.h>
static BOOL fixture_visible,fixture_iconic;
static ULONGLONG fixture_now=1000;
static int fixture_icon_invalidations;
static BOOL WINAPI FixtureInvalidate(HWND,const RECT *,BOOL);
static BOOL WINAPI FixtureVisible(HWND);
static BOOL WINAPI FixtureIconic(HWND);
static ULONGLONG WINAPI FixtureTick(void){return fixture_now;}
static BOOL WINAPI HiddenShow(HWND window,int command){
    if(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)return ShowWindow(window,command);
    return FALSE;
}
#define ShowWindow HiddenShow
#define IsWindowVisible FixtureVisible
#define IsIconic FixtureIconic
#define GetTickCount64 FixtureTick
#define InvalidateRect FixtureInvalidate
#define main unused_settings_main
#include "settings_test.c"
#undef main
#undef IsWindowVisible
#undef IsIconic
#undef InvalidateRect
static BOOL WINAPI FixtureInvalidate(HWND window,const RECT *rect,BOOL erase){
    if(window==g_list&&rect&&!erase)fixture_icon_invalidations++;
    return InvalidateRect(window,rect,erase);
}
static BOOL WINAPI FixtureVisible(HWND window){return window==g_window||window==g_list?fixture_visible:IsWindowVisible(window);}
static BOOL WINAPI FixtureIconic(HWND window){return window==g_window?fixture_iconic:IsIconic(window);}
static void tick(ULONGLONG now){fixture_now=now;WindowProcedure(g_window,WM_TIMER,1,0);}
int main(void)
{
    /* Only visibility/time are simulated. Real hidden controls, list filtering and
       native WM_TIMER handlers are used; no cursor, account or server interaction. */
    g_instance=GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);
    CreateControls();g_dpi=96;g_density=1;UpdateFonts();
    TaskItem tasks[60]={0};
    for(int i=0;i<60;i++){
        tasks[i].key=L"FIXTURE-1";tasks[i].summary=L"Unread fixture";tasks[i].latest_comment_id=L"100";
        tasks[i].assigned_to_me=TRUE;tasks[i].unread_comment=TRUE;
    }
    tasks[1].assigned_to_me=FALSE;tasks[1].reported_by_me=TRUE;
    g_tasks=tasks;g_task_count=60;g_tab=0;g_reported_scope=FALSE;
    g_jira_busy=FALSE;update_fail_thread=TRUE;fixture_thread_starts=0;
    RefreshTaskList();
    tick(1000);assert(!tasks[0].comment_pulse_started); /* Hidden: no animation. */
    fixture_visible=TRUE;tick(1000);assert(tasks[0].comment_pulse_started==1000);
    assert(!tasks[1].comment_pulse_started&&!tasks[59].comment_pulse_started); /* Other scope/offscreen. */
    tick(2600);assert(g_comment_attention_animating);
    tick(15999);assert(tasks[0].comment_pulse_started==1000&&g_comment_attention_animating);
    tick(16000);assert(tasks[0].comment_pulse_started==1000&&g_comment_attention_animating);
    fixture_iconic=TRUE;tick(31000);assert(tasks[0].comment_pulse_started==1000&&!g_comment_attention_animating);
    fixture_iconic=FALSE;tick(32000);assert(tasks[0].comment_pulse_started==32000);
    fixture_now=32400;assert(fabs(GetCommentMotion(&tasks[0],fixture_now).angle)>0);
    fixture_icon_invalidations=0;
    fixture_client_animation=FALSE;
    assert(GetCommentMotion(&tasks[0],fixture_now).angle==0); /* Incidental paint obeys preference immediately. */
    SendMessageW(g_list,WM_TIMER,7,0);
    assert(!g_comment_attention_animating&&!g_comment_attention_visible);
    assert(fixture_icon_invalidations>0); /* Request base repaint even though real fixture is hidden. */
    assert(GetCommentMotion(&tasks[0],32500).angle==0);
    fixture_client_animation=FALSE;tick(47000);assert(tasks[0].comment_pulse_started==32000);
    fixture_client_animation=TRUE;tick(48000);assert(tasks[0].comment_pulse_started==48000);
    fixture_visible=FALSE;fixture_now=48500;SendMessageW(g_list,WM_TIMER,7,0);
    assert(tasks[0].comment_pulse_started==48000);
    fixture_visible=TRUE;tick(49000);assert(tasks[0].comment_pulse_started==49000);
    tasks[0].unread_comment=FALSE;tick(64000);assert(tasks[0].comment_pulse_started==49000);
    SetWindowTextW(g_search,L"no matching title");RefreshTaskList();tick(80000);
    assert(tasks[2].comment_pulse_started==49000&&!g_comment_attention_animating); /* Last visibility resume; filtered rows do not animate. */
    SetWindowTextW(g_search,L"");SelectTaskScope(TRUE);tick(81000);assert(tasks[1].comment_pulse_started==80000);
    assert(!g_jira_thread&&!g_jira_busy&&!fixture_thread_starts); /* No worker attempt, even when idle. */
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);
    puts("Visible unread comments keep a repeating motion cycle; hidden/minimized/reduced-motion/filtered/offscreen/read states do not animate or poll Jira");
    return 0;
}
