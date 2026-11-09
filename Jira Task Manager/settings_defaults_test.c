#include <windows.h>
#include <assert.h>
#include <stddef.h>
static BOOL defaults_startup=TRUE;
static BOOL DefaultsStartupEnabled(void){return defaults_startup;}
static BOOL DefaultsSetStartup(BOOL enabled){defaults_startup=enabled;return TRUE;}
static BOOL DefaultsNoAccount(void){return FALSE;}
static BOOL DefaultsUrl(wchar_t *url,size_t count){wcscpy_s(url,count,L"https://jira.example.test/");return TRUE;}
static BOOL DefaultsHiddenShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
#define TaskManagerStartupEnabled DefaultsStartupEnabled
#define TaskManagerSetStartup DefaultsSetStartup
#define JiraHasAccount DefaultsNoAccount
#define JiraGetUrl DefaultsUrl
#define ShowWindow DefaultsHiddenShow
#include "native.c"
static void text_is(HWND control,const wchar_t *expected){wchar_t text[2100];GetWindowTextW(control,text,_countof(text));assert(!wcscmp(text,expected));}
static void click_reset(HWND settings){SendMessageW(settings,WM_COMMAND,MAKEWPARAM(267,BN_CLICKED),(LPARAM)GetDlgItem(settings,267));}
static DWORD read_profile(BYTE *bytes,DWORD capacity){HANDLE file=CreateFileW(g_settings_path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);DWORD size=0;assert(ReadFile(file,bytes,capacity,&size,NULL));CloseHandle(file);assert(size<capacity);return size;}
static void seed_preferences(void){
    assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",L"45",g_settings_path));
    assert(WritePrivateProfileStringW(L"Jira",L"TrayTaskCount",L"29",g_settings_path));
    assert(WritePrivateProfileStringW(L"Interface",L"ShowTabTaskCounts",L"0",g_settings_path));
    assert(WritePrivateProfileStringW(L"Interface",L"ShowAllTab",L"1",g_settings_path));
    assert(WritePrivateProfileStringW(L"Interface",L"ShowDoneTab",L"0",g_settings_path));
    assert(WritePrivateProfileStringW(L"Timer",L"Started",L"123456789",g_settings_path));
    assert(WritePrivateProfileStringW(L"Protected",L"Sentinel",L"preserve data",g_settings_path));
    NativeNotificationPreferences preferences={FALSE,FALSE,L"someone@example.test",FALSE};
    assert(NativeNotificationsSave(g_settings_path,&preferences)&&NativeAvatarCacheSaveDays(g_settings_path,42));
    g_refresh_minutes=45;g_tray_task_count=29;g_show_tab_task_counts=FALSE;g_show_all_tab=TRUE;g_show_done_tab=FALSE;g_notifications=preferences;
    defaults_startup=TRUE;g_workday_started=123456789;g_tab=1;
}
static void globals_unchanged(int ru,int dark,int density){
    assert(g_refresh_minutes==45&&g_tray_task_count==29&&!g_show_tab_task_counts&&g_show_all_tab&&!g_show_done_tab);
    assert(!g_notifications.tasks&&!g_notifications.comments&&!g_notifications.statuses&&!wcscmp(g_notifications.excluded,L"someone@example.test"));
    assert(defaults_startup&&g_workday_started==123456789&&g_russian==ru&&g_dark==dark&&g_density==density);
}
static void defaults_staged(void){
    assert(SendMessageW(SettingsItem(212),TBM_GETPOS,0,0)==30&&SendMessageW(SettingsItem(213),TBM_GETPOS,0,0)==20);
    text_is(SettingsItem(226),g_russian?L"30 мин":L"30 min");text_is(SettingsItem(263),L"20");
    const int on[]={224,246,250,251,261},off[]={214,225};
    for(int i=0;i<_countof(on);i++)assert(SendMessageW(SettingsItem(on[i]),BM_GETCHECK,0,0)==BST_CHECKED);
    for(int i=0;i<_countof(off);i++)assert(SendMessageW(SettingsItem(off[i]),BM_GETCHECK,0,0)==BST_UNCHECKED);
    text_is(SettingsItem(252),L"no-mail@kama.gs");text_is(SettingsItem(259),L"7");
}
static void button_layout(HWND settings){
    HWND button=GetDlgItem(settings,267);assert(button&&"Settings need a shared Defaults footer button");
    text_is(button,g_russian?L"По умолчанию":L"Defaults");wchar_t name[32];GetClassNameW(button,name,32);assert(!wcscmp(name,L"Button"));
    RECT bounds,cancel;GetWindowRect(button,&bounds);GetWindowRect(SettingsItem(IDCANCEL),&cancel);
    assert(!SettingsItem(241)&&bounds.right<=cancel.left);
    HDC dc=GetDC(button);HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(button,WM_GETFONT,0,0));SIZE size;wchar_t caption[64];GetWindowTextW(button,caption,64);GetTextExtentPoint32W(dc,caption,(int)wcslen(caption),&size);
    assert(size.cx+S(12)<=bounds.right-bounds.left&&size.cy<=bounds.bottom-bounds.top);SelectObject(dc,old);ReleaseDC(button,dc);
    for(int tab=0;tab<4;tab++){SettingsTab(tab);assert(((GetWindowLongPtrW(button,GWL_STYLE)&WS_VISIBLE)!=0)==(tab!=1));}
    g_russian=!g_russian;UpdateSettingsLanguage();text_is(button,g_russian?L"По умолчанию":L"Defaults");g_russian=!g_russian;UpdateSettingsLanguage();
}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temporary[MAX_PATH];GetTempPathW(MAX_PATH,temporary);assert(GetTempFileNameW(temporary,L"def",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"SettingsDefaultsFixture"};assert(RegisterClassW(&cls));
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;g_density=density;RecreateBrushes();seed_preferences();
        BYTE before[20000],after[20000];DWORD before_size=read_profile(before,sizeof(before));
        HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);button_layout(settings);
        SetWindowTextW(s_url,L"https://draft.example.test/");SetWindowTextW(s_token,L"local-test-token-draft");
        FreezeSettings(TRUE);assert(!IsWindowEnabled(SettingsItem(267)));click_reset(settings);assert(SettingsRefreshMinutes()==45);FreezeSettings(FALSE);
        g_jira_busy=TRUE;click_reset(settings);assert(SettingsRefreshMinutes()==45);g_jira_busy=FALSE;
        g_update_handoff=TRUE;click_reset(settings);assert(SettingsRefreshMinutes()==45);g_update_handoff=FALSE;
        click_reset(settings);defaults_staged();assert(UpdateSettingsDirty());globals_unchanged(ru,dark,density);
        SendMessageW(SettingsItem(224),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(settings,WM_COMMAND,MAKEWPARAM(224,BN_CLICKED),(LPARAM)SettingsItem(224));
        globals_unchanged(ru,dark,density); /* Editing a reset draft must not apply other defaults as a live preview. */
        click_reset(settings);defaults_staged();
        text_is(s_url,L"https://draft.example.test/");text_is(s_token,L"local-test-token-draft");
        assert(read_profile(after,sizeof(after))==before_size&&!memcmp(before,after,before_size));
        click_reset(settings);defaults_staged();
        SendMessageW(settings,WM_COMMAND,257,0);assert(s_advanced);text_is(SettingsItem(259),L"7");SetWindowTextW(SettingsItem(259),L"14");SendMessageW(s_advanced_window,WM_CLOSE,0,0);click_reset(settings);text_is(SettingsItem(259),L"7");
        SendMessageW(settings,WM_COMMAND,IDCANCEL,0);assert(!IsWindow(settings));globals_unchanged(ru,dark,density);
        assert(read_profile(after,sizeof(after))==before_size&&!memcmp(before,after,before_size));
        settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
        assert(SettingsRefreshMinutes()==45&&SettingsTrayCount()==29);text_is(SettingsItem(259),L"42");click_reset(settings);defaults_staged();
        SendMessageW(settings,WM_COMMAND,IDOK,0);assert(!IsWindow(settings));
        assert(g_refresh_minutes==30&&g_tray_task_count==20&&g_show_tab_task_counts&&!g_show_all_tab&&g_show_done_tab&&!defaults_startup);
        assert(g_notifications.tasks&&g_notifications.comments&&g_notifications.statuses&&!wcscmp(g_notifications.excluded,L"no-mail@kama.gs"));
        assert(GetPrivateProfileIntW(L"Jira",L"RefreshMinutes",0,g_settings_path)==30&&GetPrivateProfileIntW(L"Jira",L"TrayTaskCount",0,g_settings_path)==20&&NativeAvatarCacheDays(g_settings_path)==7);
        wchar_t sentinel[64];GetPrivateProfileStringW(L"Protected",L"Sentinel",L"",sentinel,64,g_settings_path);assert(!wcscmp(sentinel,L"preserve data"));
        assert(GetPrivateProfileIntW(L"Timer",L"Started",0,g_settings_path)==123456789&&g_workday_started==123456789&&g_russian==ru&&g_dark==dark&&g_density==density);
        g_jira_busy=TRUE;
    }
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));puts("Settings defaults: reversible draft, exact defaults, busy guards, shared Advanced, Save/Cancel, protected connection/data/timer/theme/language and RU/EN/DPI/density passed");return 0;
}
