#include <windows.h>
#include <assert.h>
#include "native.c"
static void filter_snapshot(const char *id,const char *email,const char *created)
{
    char json[2200];snprintf(json,sizeof(json),"{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"FILTER-1\",\"fields\":{\"summary\":\"Offline filter\",\"status\":{\"id\":\"1\",\"name\":\"Done\",\"statusCategory\":{\"key\":\"done\"}},\"comment\":{\"comments\":[{\"id\":\"%s\",\"created\":\"%s\",\"body\":\"Visible colleague comment\",\"author\":{\"emailAddress\":\"%s\",\"key\":\"colleague\"}}]}}}]}",id,created,email);
    JiraSnapshot snapshot={0};int total,start;assert(JiraParsePageFiltered(json,strlen(json),&snapshot,&total,&start,g_notifications.excluded));
    wcscpy_s(snapshot.account,256,L"Filter fixture");snapshot.items[0].assigned_to_me=TRUE;assert(InstallSnapshot(&snapshot));
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"ntf",0,g_settings_path));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.lpszClassName=L"NotificationSettingsFixture";assert(RegisterClassW(&wc));
    HWND settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
    assert(GetDlgItem(settings,202)); /* Separate settings page, not more main-toolbar controls. */
    SettingsTab(2);
    assert(SettingsItem(261)); /* Independent status preference, not tied to task/comment indicators. */
    wchar_t status_label[128];GetWindowTextW(SettingsItem(261),status_label,128);
    assert(!wcscmp(status_label,L"Уведомлять об изменении статуса менеджером"));
    assert(SendMessageW(SettingsItem(261),BM_GETCHECK,0,0)==BST_CHECKED);
    RECT status_bounds;GetClientRect(SettingsItem(261),&status_bounds);
    HDC measure=GetDC(SettingsItem(261));HGDIOBJ saved=SelectObject(measure,(HFONT)SendMessageW(SettingsItem(261),WM_GETFONT,0,0));
    SIZE status_text;assert(GetTextExtentPoint32W(measure,status_label,(int)wcslen(status_label),&status_text));
    assert(status_text.cx+S(22)<=status_bounds.right);SelectObject(measure,saved);ReleaseDC(SettingsItem(261),measure);
    SendMessageW(SettingsItem(261),BM_SETCHECK,BST_UNCHECKED,0);
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(261,BN_CLICKED),(LPARAM)SettingsItem(261));
    assert(!g_notifications.statuses&&UpdateSettingsDirty());
    DestroyWindow(settings);assert(g_notifications.statuses); /* Cancel rolls back the immediate preview. */
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);SettingsTab(2);
    SendMessageW(SettingsItem(261),BM_SETCHECK,BST_UNCHECKED,0);
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(261,BN_CLICKED),(LPARAM)SettingsItem(261));assert(!g_notifications.statuses);
    assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL)); /* At normal size everything fits. */
    SetWindowPos(settings,NULL,0,0,S(430),S(260),SWP_NOMOVE|SWP_NOZORDER);LayoutSettings();
    RECT footer_before,footer_after,viewport;GetWindowRect(GetDlgItem(settings,IDOK),&footer_before);GetClientRect(s_notifications,&viewport);
    SCROLLINFO scroll={sizeof(scroll),SIF_ALL};assert(GetScrollInfo(s_notifications,SB_VERT,&scroll));assert(scroll.nMax>(int)scroll.nPage);
    SendMessageW(s_notifications,WM_VSCROLL,SB_BOTTOM,0);assert(s_notification_scroll>0);
    RECT hint;GetWindowRect(SettingsItem(255),&hint);MapWindowPoints(NULL,s_notifications,(POINT*)&hint,2);assert(hint.bottom<=viewport.bottom);
    GetWindowRect(GetDlgItem(settings,IDOK),&footer_after);assert(EqualRect(&footer_before,&footer_after));
    SendMessageW(s_notifications,WM_VSCROLL,SB_TOP,0);assert(s_notification_scroll==0);
    SetWindowPos(settings,NULL,0,0,S(430),S(360),SWP_NOMOVE|SWP_NOZORDER);LayoutSettings();
    wchar_t text[4096];GetWindowTextW(SettingsItem(252),text,4096);assert(!wcscmp(text,L"no-mail@kama.gs"));
    assert(SendMessageW(SettingsItem(250),BM_GETCHECK,0,0)==BST_CHECKED);
    assert(SendMessageW(SettingsItem(251),BM_GETCHECK,0,0)==BST_CHECKED);
    SendMessageW(SettingsItem(250),BM_SETCHECK,BST_UNCHECKED,0);
    SendMessageW(SettingsItem(251),BM_SETCHECK,BST_UNCHECKED,0);
    SetWindowTextW(SettingsItem(252),L"robot@example.test, no-mail@kama.gs");assert(UpdateSettingsDirty());
    assert(SaveNativePreferences());
    LoadSettings();assert(!g_notifications.statuses);
    TaskItem fixture[2]={0};fixture[0].assigned_to_me=TRUE;fixture[0].is_new=TRUE;fixture[1].assigned_to_me=TRUE;fixture[1].unread_comment=TRUE;
    g_tasks=fixture;g_task_count=2;assert(ScopeActivityCounts(FALSE).updates==0);assert(TabActivityCount(0)==0);
    LoadSettings();assert(ScopeActivityCounts(FALSE).updates==0);
    SetWindowTextW(SettingsItem(252),L"broken-email");assert(!SaveNativePreferences());
    wchar_t stored[2048];GetPrivateProfileStringW(L"Notifications",L"ExcludedEmails",L"",stored,_countof(stored),g_settings_path);
    assert(!wcscmp(stored,L"robot@example.test, no-mail@kama.gs"));
    SetWindowTextW(SettingsItem(252),L"");assert(SaveNativePreferences());
    DestroyWindow(settings);settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);
    GetWindowTextW(SettingsItem(252),text,4096);assert(!text[0]); /* Explicitly clearing exclusions survives reopen. */
    SendMessageW(SettingsItem(250),BM_SETCHECK,BST_CHECKED,0);assert(SaveNativePreferences());
    assert(ScopeActivityCounts(FALSE).updates==1&&!TaskCommentActivity(&fixture[1]));
    SendMessageW(SettingsItem(250),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(SettingsItem(251),BM_SETCHECK,BST_CHECKED,0);assert(SaveNativePreferences());
    assert(ScopeActivityCounts(FALSE).updates==1&&!TaskNewActivity(&fixture[0]));
    g_tasks=NULL;g_task_count=0;
    filter_snapshot("10","person@example.test","2001-01-01T10:00:00.000+0300");
    filter_snapshot("11","person@example.test","2001-01-02T10:00:00.000+0300");assert(g_tasks[0].unread_comment&&ScopeActivityCounts(FALSE).updates==1);
    SetWindowTextW(SettingsItem(252),L"person@example.test");assert(SaveNativePreferences());
    assert(ScopeActivityCounts(FALSE).updates==0&&!g_tasks[0].latest_comment_id[0]&&!g_tasks[0].latest_comment[0]);
    SetWindowTextW(SettingsItem(252),L"");assert(SaveNativePreferences());LoadSettings();
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);
    filter_snapshot("12","person@example.test","2001-01-03T10:00:00.000+0300");assert(!g_tasks[0].unread_comment); /* Old revealed history, including restart. */
    filter_snapshot("13","person@example.test","2030-01-01T10:00:00.000Z");assert(g_tasks[0].unread_comment);
    SetWindowTextW(SettingsItem(252),L"robot@example.test");assert(SaveNativePreferences());
    filter_snapshot("14","person@example.test","2030-01-02T10:00:00.000Z");assert(g_tasks[0].unread_comment); /* Unrelated real arrivals cannot be swallowed by filter changes. */
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);DestroyWindow(settings);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Notification preferences: separate page, defaults, persistence, empty exclusions and disabled in-app counters passed");
}
