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
    assert(!g_show_all_tab);LoadSettings();assert(!g_show_all_tab&&g_tab==1);
    assert(WritePrivateProfileStringW(L"Interface",L"ShowAllTab",L"1",g_settings_path));
    LoadSettings();assert(g_show_all_tab); /* An explicit existing choice wins over the new default. */
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    TaskItem tasks[]={
        {.key=L"CNT-1",.summary=L"New",.status=STATUS_TODO,.assigned_to_me=TRUE,.is_new=TRUE},
        {.key=L"CNT-2",.summary=L"Unread",.status=STATUS_DONE,.assigned_to_me=TRUE,.unread_comment=TRUE,.latest_comment_id=L"100"},
        {.key=L"CNT-3",.summary=L"Read",.status=STATUS_DONE,.assigned_to_me=TRUE}};
    g_tasks=tasks;g_task_count=3;g_status_count=STATUS_COUNT;g_status_categories[STATUS_DONE]=2;g_tab=0;RefreshTaskList();
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.lpszClassName=L"TaskCountPreferencesFixture";assert(RegisterClassW(&wc));
    HWND settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
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
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,224,FALSE);assert(SaveNativePreferences());
    assert(GetPrivateProfileIntW(L"Interface",L"ShowTabTaskCounts",1,g_settings_path)==0);
    DestroyWindow(settings);LoadSettings();UpdateTabLabels();GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все"));
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    assert(SendMessageW(GetDlgItem(settings,224),BM_GETCHECK,0,0)==BST_UNCHECKED);
    click_checkbox(settings,224,TRUE);GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"Все 3"));
    assert(SaveSettings()); /* Saving timer/theme while previewing must not commit this checkbox. */
    assert(GetPrivateProfileIntW(L"Interface",L"ShowTabTaskCounts",1,g_settings_path)==0);
    g_russian=FALSE;UpdateLanguage();GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"All 3"));
    GetWindowRect(settings,&after);assert(EqualRect(&before,&after));DestroyWindow(settings);
    GetWindowTextW(g_tab_all,label,80);assert(!wcscmp(label,L"All"));
    /* Hiding All must remove the actual tab and give its slot to search, not
       merely blank its label. Cancel must undo the live preview. */
    g_tab=0;SetWindowTextW(g_search,L"CNT");RefreshTaskList();assert(ListView_GetItemCount(g_list)==3);
    RECT search_before=g_search_frame,all_slot;GetWindowRect(g_tab_all,&all_slot);
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    assert(GetDlgItem(settings,225));assert(SendMessageW(GetDlgItem(settings,225),BM_GETCHECK,0,0)==BST_CHECKED);
    click_checkbox(settings,225,FALSE);
    assert(!(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE));assert(g_tab==1);
    assert(ListView_GetItemCount(g_list)==1);GetWindowTextW(g_search,label,80);assert(!wcscmp(label,L"CNT"));
    assert(g_search_frame.right-g_search_frame.left==search_before.right-search_before.left+all_slot.right-all_slot.left);
    assert(UpdateSettingsDirty());assert(GetPrivateProfileIntW(L"Interface",L"ShowAllTab",1,g_settings_path)==1);
    WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_ALL,BN_CLICKED),(LPARAM)g_tab_all);assert(g_tab==1);
    SelectTaskScope(TRUE);assert(g_tab==1);SelectTaskScope(FALSE);assert(g_tab==1);
    DestroyWindow(settings);assert(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE);assert(g_tab==0);
    assert(ListView_GetItemCount(g_list)==3);assert(EqualRect(&g_search_frame,&search_before));
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,225,FALSE);assert(SaveNativePreferences());DestroyWindow(settings);
    assert(GetPrivateProfileIntW(L"Interface",L"ShowAllTab",1,g_settings_path)==0);
    g_tab=0;LoadSettings();UpdateTabLabels();assert(g_tab==1);assert(!(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE));
    g_russian=FALSE;UpdateLanguage();
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    assert(SendMessageW(GetDlgItem(settings,225),BM_GETCHECK,0,0)==BST_UNCHECKED);
    GetWindowTextW(GetDlgItem(settings,225),label,80);assert(wcsstr(label,L"All"));
    click_checkbox(settings,225,TRUE);assert(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE);
    assert(SaveSettings());assert(GetPrivateProfileIntW(L"Interface",L"ShowAllTab",1,g_settings_path)==0);
    g_russian=TRUE;UpdateLanguage();GetWindowTextW(GetDlgItem(settings,225),label,80);assert(wcsstr(label,L"Все"));
    DestroyWindow(settings);assert(!(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE));
    /* Neither the Done filter nor the query is replaced when All is hidden. */
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,225,TRUE);assert(SaveNativePreferences());DestroyWindow(settings);
    g_tab=2;RefreshTaskList();assert(ListView_GetItemCount(g_list)==2);
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,225,FALSE);assert(g_tab==2&&ListView_GetItemCount(g_list)==2);
    DestroyWindow(settings);assert(g_tab==2);GetWindowTextW(g_search,label,80);assert(!wcscmp(label,L"CNT"));
    /* Done is independently visible by default, with the same live-preview,
       fallback, reclaimed search width and transactional Save/Cancel contract. */
    g_tab=2;RefreshTaskList();assert(ListView_GetItemCount(g_list)==2);
    RECT done_slot;GetWindowRect(g_tab_done,&done_slot);search_before=g_search_frame;
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    assert(GetDlgItem(settings,246)&&SendMessageW(GetDlgItem(settings,246),BM_GETCHECK,0,0)==BST_CHECKED);
    click_checkbox(settings,246,FALSE);assert(!(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE)&&g_tab==1&&ListView_GetItemCount(g_list)==1);
    assert(g_search_frame.right-g_search_frame.left==search_before.right-search_before.left+done_slot.right-done_slot.left);
    assert(UpdateSettingsDirty()&&GetPrivateProfileIntW(L"Interface",L"ShowDoneTab",1,g_settings_path)==1);
    WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_DONE,BN_CLICKED),(LPARAM)g_tab_done);assert(g_tab==1);
    SelectTaskScope(TRUE);assert(g_tab==1);SelectTaskScope(FALSE);assert(g_tab==1);
    DestroyWindow(settings);assert(g_tab==2&&(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE)&&EqualRect(&g_search_frame,&search_before));
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,246,FALSE);assert(SaveNativePreferences());DestroyWindow(settings);
    assert(GetPrivateProfileIntW(L"Interface",L"ShowDoneTab",1,g_settings_path)==0);
    g_tab=2;LoadSettings();RefreshTaskList();assert(g_tab==1&&!(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE));
    g_russian=FALSE;
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    assert(SendMessageW(GetDlgItem(settings,246),BM_GETCHECK,0,0)==BST_UNCHECKED);
    GetWindowTextW(GetDlgItem(settings,246),label,80);assert(wcsstr(label,L"Done"));
    click_checkbox(settings,246,TRUE);assert(SaveSettings()&&GetPrivateProfileIntW(L"Interface",L"ShowDoneTab",1,g_settings_path)==0);
    DestroyWindow(settings);assert(!(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE));
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    click_checkbox(settings,225,FALSE);assert(!(GetWindowLongPtrW(g_tab_all,GWL_STYLE)&WS_VISIBLE)&&!(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE));
    assert((GetWindowLongPtrW(g_tab_current,GWL_STYLE)&WS_VISIBLE)&&g_tab==1);
    click_checkbox(settings,246,TRUE);assert(SaveNativePreferences());DestroyWindow(settings);
    assert(WritePrivateProfileStringW(L"Interface",L"ShowDoneTab",NULL,g_settings_path));LoadSettings();RefreshTaskList();
    assert(GetWindowLongPtrW(g_tab_done,GWL_STYLE)&WS_VISIBLE); /* Missing value defaults to on. */
    GetWindowTextW(g_search,label,80);assert(!wcscmp(label,L"CNT"));
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int connected=0;connected<2;connected++){
        g_dpi=dpi;g_russian=ru;UpdateFonts();
        settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
        s_connected=connected;LayoutSettings();RECT check,done_check,note,save;
        GetWindowRect(GetDlgItem(settings,225),&check);GetWindowRect(GetDlgItem(settings,246),&done_check);GetWindowRect(s_note,&note);GetWindowRect(GetDlgItem(settings,IDOK),&save);
        assert(check.bottom<=done_check.top&&done_check.bottom<=note.top&&note.bottom<=save.top);
        DestroyWindow(settings);
    }
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Tab preferences: independent totals, All visibility, reclaimed search width, preserved filters/query, Save/Cancel, localization and DPI layout passed");return 0;
}
