#include <windows.h>
#include <assert.h>
static BOOL FixtureNoAccount(void){return FALSE;}
static BOOL FixtureNoUrl(wchar_t *url,size_t count){(void)count;url[0]=0;return FALSE;}
#define JiraHasAccount FixtureNoAccount
#define JiraGetUrl FixtureNoUrl
#include "native.c"
static void click_checkbox(HWND settings,int id,BOOL checked){
    HWND checkbox=GetDlgItem(settings,id);if(!checkbox)checkbox=SettingsItem(id);assert(checkbox);
    SendMessageW(checkbox,BM_SETCHECK,checked?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)checkbox);
}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"tcp",0,g_settings_path));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    TaskItem tasks[]={
        {.key=L"CNT-1",.summary=L"New",.status=STATUS_TODO,.assigned_to_me=TRUE,.is_new=TRUE},
        {.key=L"CNT-2",.summary=L"Unread",.status=STATUS_DONE,.assigned_to_me=TRUE,.unread_comment=TRUE,.latest_comment_id=L"100"},
        {.key=L"CNT-3",.summary=L"Read",.status=STATUS_DONE,.assigned_to_me=TRUE}};
    g_tasks=tasks;g_task_count=3;g_status_count=STATUS_COUNT;g_status_categories[STATUS_DONE]=2;g_tab=0;RefreshTaskList();
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.lpszClassName=L"TaskCountPreferencesFixture";assert(RegisterClassW(&wc));
    HWND settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
    assert(GetDlgItem(settings,224));assert(SendMessageW(GetDlgItem(settings,224),BM_GETCHECK,0,0)==BST_CHECKED);
    RECT before,after;GetWindowRect(settings,&before);wchar_t label[80];
    click_checkbox(settings,224,FALSE);GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все"));
    GetWindowTextW(g_tab_current,label,80);assert(!wcscmp(label,L"Текущие"));
    GetWindowTextW(g_tab_done,label,80);assert(!wcscmp(label,L"Готово"));
    assert(TabActivityCount(0)==2&&TabActivityCount(2)==1);assert(UpdateSettingsDirty());
    assert(GetPrivateProfileIntW(L"Interface",L"ShowTabTaskCounts",1,g_settings_path)==1);
    click_checkbox(settings,250,FALSE);assert(TabActivityCount(0)==1&&tasks[0].is_new);
    assert(wcsstr(g_scope_tip[0],L"Новых задач: 0"));
    click_checkbox(settings,251,FALSE);assert(TabActivityCount(0)==0&&tasks[1].unread_comment);
    DestroyWindow(settings);GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все 3"));
    assert(TabActivityCount(0)==2);assert(wcsstr(g_scope_tip[0],L"Новых задач: 1")); /* Cancel rolls back previews and never consumes activity. */
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(360),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,224,FALSE);assert(SaveNativePreferences());
    assert(GetPrivateProfileIntW(L"Interface",L"ShowTabTaskCounts",1,g_settings_path)==0);
    DestroyWindow(settings);LoadSettings();UpdateTabLabels();GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все"));
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(360),g_window,NULL,g_instance,NULL);
    assert(SendMessageW(GetDlgItem(settings,224),BM_GETCHECK,0,0)==BST_UNCHECKED);
    click_checkbox(settings,224,TRUE);GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все 3"));
    assert(SaveSettings()); /* Saving timer/theme while previewing must not commit this checkbox. */
    assert(GetPrivateProfileIntW(L"Interface",L"ShowTabTaskCounts",1,g_settings_path)==0);
    g_russian=FALSE;UpdateLanguage();GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"All 3"));
    GetWindowRect(settings,&after);assert(EqualRect(&before,&after));DestroyWindow(settings);
    GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"All"));
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Tab counts: independent totals, live notification/count preview, Cancel rollback, Save persistence and unchanged settings size passed");return 0;
}
