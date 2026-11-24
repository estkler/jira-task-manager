#include <windows.h>
#include <assert.h>
static HWND tray_test_window;
static int tray_test_restores,tray_test_foregrounds,tray_test_menus;
static BOOL WINAPI TrayShow(HWND window,int command){
    if(window==tray_test_window&&command==SW_RESTORE){tray_test_restores++;return ShowWindow(window,SW_SHOWNOACTIVATE);}
    if(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)return ShowWindow(window,command);
    return FALSE;
}
static BOOL WINAPI TrayForeground(HWND window){if(window==tray_test_window)tray_test_foregrounds++;return TRUE;}
static BOOL WINAPI TrayMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){tray_test_menus++;return 0;}
static BOOL TrayNoAccount(void){return FALSE;}
#define ShowWindow TrayShow
#define SetForegroundWindow TrayForeground
#define TrackPopupMenu TrayMenu
#define JiraHasAccount TrayNoAccount
#include "native.c"
#undef ShowWindow
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
    SendMessageW(s_window,WM_COMMAND,IDCANCEL,0);DestroyWindow(g_window);DeleteFileW(g_settings_path);
    puts("Tray activation: one click restores, double click stays open, right click retains menu, update balloon retains its action");return 0;
}
