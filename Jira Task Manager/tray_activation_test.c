#include <windows.h>
#include <assert.h>
static HWND tray_test_window;
static int tray_test_restores,tray_test_foregrounds,tray_test_menus,tray_test_menu_result;
static BOOL WINAPI TrayShow(HWND window,int command){
    if(window==tray_test_window&&command==SW_RESTORE){tray_test_restores++;return ShowWindow(window,SW_SHOWNOACTIVATE);}
    if(window==tray_test_window&&command==SW_HIDE)return ShowWindow(window,command);
    if(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)return ShowWindow(window,command);
    wchar_t class_name[64];
    GetClassNameW(window,class_name,_countof(class_name));
    if(!wcscmp(class_name,L"Tasklet.DesktopWidget")){
        if(command==SW_SHOWNOACTIVATE)SetWindowPos(window,NULL,30000,30000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        return ShowWindow(window,command);
    }
    return FALSE;
}
static BOOL WINAPI TrayForeground(HWND window){if(window==tray_test_window)tray_test_foregrounds++;return TRUE;}
static BOOL WINAPI TrayMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){tray_test_menus++;return tray_test_menu_result;}
static BOOL TrayNoAccount(void){return FALSE;}
#define ShowWindow TrayShow
#define SetForegroundWindow TrayForeground
#define TrackPopupMenu TrayMenu
#define JiraHasAccount TrayNoAccount
#include "native.c"
#undef ShowWindow
static LRESULT CALLBACK TrayTestMainSubclass(HWND window,UINT message,WPARAM wp,LPARAM lp,
    UINT_PTR id,DWORD_PTR reference){
    if(message==WM_SIZE||message==WM_SHOWWINDOW)return WindowProcedure(window,message,wp,lp);
    return DefSubclassProc(window,message,wp,lp);
}
static void desktop_widget_contract(void){
    WindowProcedure(g_window,WM_CLOSE,0,0);
    assert(!g_desktop_widget&&!IsWindowVisible(g_window));
    g_desktop_widget_enabled=TRUE;
    TaskItem tasks[]={
        {.key=L"DEMO-17",.summary=L"Reported to another person",.active=TRUE,.reported_by_me=TRUE},
        {.key=L"DEMO-104",.summary=L"Current task",.active=TRUE,.assigned_to_me=TRUE}
    };
    g_tasks=tasks;g_task_count=_countof(tasks);
    assert(SetWindowSubclass(g_window,TrayTestMainSubclass,1,0));
    ShowWindow(g_window,SW_SHOWNOACTIVATE);
    ShowWindow(g_window,SW_SHOWMINNOACTIVE);
    HWND widget=g_desktop_widget;assert(widget&&IsWindowVisible(widget));
    assert(!GetWindow(widget,GW_OWNER)&&(GetWindowLongPtrW(widget,GWL_EXSTYLE)&WS_EX_NOACTIVATE));
    assert(IsIconic(g_window)&&GetForegroundWindow()!=widget);
    HWND task=GetDlgItem(widget,WIDGET_OPEN),hide=GetDlgItem(widget,WIDGET_HIDE);
    wchar_t label[64];GetWindowTextW(task,label,_countof(label));assert(wcsstr(label,L"DEMO-104"));
    wchar_t long_summary[_countof(((JiraItem*)0)->summary)];
    wmemset(long_summary,L'W',_countof(long_summary)-1);long_summary[_countof(long_summary)-1]=0;
    tasks[1].summary=long_summary;RefreshDesktopWidgetTask();
    assert(GetWindowTextLengthW(task)==wcslen(tasks[1].key)+2+wcslen(long_summary));
    RECT task_bounds,timer=DesktopWidgetTimerRect(widget);
    GetWindowRect(task,&task_bounds);MapWindowPoints(NULL,widget,(POINT*)&task_bounds,2);
    assert(task_bounds.right<=timer.left&&"Long titles must not displace the timer");
    HWND clock=GetDlgItem(widget,WIDGET_TIMER);
    assert(clock&&"The widget clock must be an interactive control");
    assert(timer.right-timer.left<WidgetScale(88)&&"Numeric mode must leave less space next to the close button");
    if(CommentAnimationEnabled()){
        assert(g_widget_fade_started);
        g_widget_fade_started=GetTickCount64()-WIDGET_FADE_MS;
        SendMessageW(widget,WM_TIMER,WIDGET_FADE_TIMER,0);
        BYTE alpha;assert(!g_widget_fade_started&&GetLayeredWindowAttributes(widget,NULL,&alpha,NULL)&&alpha==255);
    }
    SendMessageW(clock,WM_RBUTTONUP,0,0);assert(g_text_time&&!g_bar_time);
    GetWindowTextW(clock,label,_countof(label));assert(wcsstr(label,L"ч"));
    SendMessageW(clock,WM_RBUTTONUP,0,0);assert(!g_text_time&&g_bar_time);
    GetWindowTextW(clock,label,_countof(label));assert(wcschr(label,L'%'));
    SendMessageW(clock,WM_RBUTTONUP,0,0);assert(!g_text_time&&!g_bar_time);
    ULONGLONG old_start=g_workday_started;
    tray_test_menu_result=IDM_TIMER_RESET;
    SendMessageW(widget,WM_COMMAND,MAKEWPARAM(WIDGET_TIMER,BN_CLICKED),(LPARAM)clock);
    HWND edit=GetDlgItem(widget,WIDGET_EDIT);
    assert(IsWindowVisible(edit)&&!IsWindowVisible(clock)&&IsIconic(g_window));
    SetWindowTextW(edit,L"00:00");SendMessageW(edit,WM_KEYDOWN,VK_ESCAPE,0);
    assert(g_workday_started==old_start&&IsWindowVisible(clock));
    SendMessageW(widget,WM_COMMAND,MAKEWPARAM(WIDGET_TIMER,BN_CLICKED),(LPARAM)clock);
    SetWindowTextW(edit,L"00:00");SendMessageW(edit,WM_KEYDOWN,VK_RETURN,0);
    assert(g_workday_started!=old_start&&IsWindowVisible(clock));
    wchar_t saved_start[32];
    GetPrivateProfileStringW(L"Timer",L"Started",L"",saved_start,_countof(saved_start),g_settings_path);
    assert(_wcstoui64(saved_start,NULL,10)==g_workday_started);
    tray_test_menu_result=0;
    RECT before,after;GetWindowRect(widget,&before);
    int restore_count=tray_test_restores;
    SendMessageW(task,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(20,14));
    SendMessageW(task,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(60,40));
    GetWindowRect(widget,&after);assert(before.left!=after.left&&before.top!=after.top);
    SendMessageW(task,WM_LBUTTONUP,0,MAKELPARAM(20,14));
    assert(tray_test_restores==restore_count&&IsWindowVisible(widget));
    SendMessageW(task,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(20,14));
    SendMessageW(task,WM_LBUTTONUP,0,MAKELPARAM(-30,-12));
    assert(tray_test_restores==restore_count&&IsWindowVisible(widget));
    wchar_t position[64];
    GetPrivateProfileStringW(L"DesktopWidget",L"Position",L"",position,_countof(position),g_settings_path);
    assert(wcschr(position,L',')&&g_widget_position_saved);
    wcscpy_s(g_timer_numbers,_countof(g_timer_numbers),L"01:01:01");UpdateDesktopWidgetTime();
    GetWindowTextW(clock,label,_countof(label));assert(!wcscmp(label,L"01:01:01"));
    wcscpy_s(g_timer_numbers,_countof(g_timer_numbers),L"01:01:02");UpdateDesktopWidgetTime();
    GetWindowTextW(clock,label,_countof(label));assert(!wcscmp(label,L"01:01:02"));
    tasks[1].active=FALSE;
    RefreshDesktopWidgetTask();GetWindowTextW(task,label,_countof(label));assert(!wcscmp(label,DesktopWidgetEmptyText()));
    SendMessageW(hide,BM_CLICK,0,0);
    assert(!IsWindowVisible(widget)&&IsIconic(g_window));
    WindowProcedure(g_window,WM_SIZE,SIZE_MINIMIZED,0);
    assert(!IsWindowVisible(widget));
    WindowProcedure(g_window,WM_CLOSE,0,0);
    assert(IsWindowVisible(widget)&&!IsWindowVisible(g_window));
    SendMessageW(task,BM_CLICK,0,0);
    assert(!IsWindowVisible(widget)&&IsWindowVisible(g_window));
    WindowProcedure(g_window,WM_CLOSE,0,0);
    assert(IsWindowVisible(widget)&&!IsWindowVisible(g_window));
    SendMessageW(hide,BM_CLICK,0,0);
    WindowProcedure(g_window,WM_CLOSE,0,0);
    assert(!IsWindowVisible(widget));
    WindowProcedure(g_window,WM_TRAYICON,1,WM_LBUTTONUP);
    assert(!IsWindowVisible(widget)&&IsWindowVisible(g_window));
    g_desktop_widget_enabled=FALSE;
    WindowProcedure(g_window,WM_CLOSE,0,0);assert(!IsWindowVisible(widget));
    DestroyWindow(widget);assert(!g_desktop_widget);
    g_tasks=NULL;g_task_count=0;
}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    g_window=tray_test_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"try",0,g_settings_path);
    assert(!IsWindowVisible(g_window));g_jira_busy=TRUE;g_update_notice=TRUE;
    WindowProcedure(g_window,WM_TRAYICON,1,WM_LBUTTONDOWN);assert(!IsWindowVisible(g_window));
    WindowProcedure(g_window,WM_TRAYICON,1,WM_LBUTTONUP);
    assert(IsWindowVisible(g_window)&&tray_test_restores==1&&tray_test_foregrounds==1&&"One completed left click must restore the existing main window");
    assert(g_update_notice&&!s_window&&g_jira_busy&&"A plain tray click must not open update settings or start a Jira request");
    HWND original=g_window;WindowProcedure(g_window,WM_TRAYICON,1,WM_LBUTTONDBLCLK);WindowProcedure(g_window,WM_TRAYICON,1,WM_LBUTTONUP);
    assert(g_window==original&&IsWindowVisible(g_window)&&tray_test_restores==3&&!s_window);
    int restores=tray_test_restores;ShowWindow(g_window,SW_HIDE);
    WindowProcedure(g_window,WM_TRAYICON,1,WM_RBUTTONUP);assert(tray_test_menus==1&&tray_test_restores==restores&&!IsWindowVisible(g_window));
    WindowProcedure(g_window,WM_TRAYICON,1,WM_CONTEXTMENU);assert(tray_test_menus==2&&tray_test_restores==restores);
    WindowProcedure(g_window,WM_TRAYICON,1,NIN_BALLOONUSERCLICK);assert(IsWindowVisible(g_window)&&s_window&&s_tab==1&&!g_update_notice);
    SendMessageW(s_window,WM_COMMAND,IDCANCEL,0);
    desktop_widget_contract();
    DestroyWindow(g_window);DeleteFileW(g_settings_path);
    puts("Tray and widget: activation, update balloon, opt-in, task and clock, dismiss, restore");return 0;
}
