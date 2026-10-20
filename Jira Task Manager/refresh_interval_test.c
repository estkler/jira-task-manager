#include "native.c"
#include <assert.h>
/* Only this fixture's hidden windows and temporary profile are used. */
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&controls));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"rfi",0,g_settings_path));
    struct {const wchar_t *stored;int expected;} cases[]={{L"1",15},{L"14",15},{L"15",15},{L"30",30},{L"1440",1440},{L"0",30},{L"1441",30},{L"-1",30}};
    for(int i=0;i<_countof(cases);i++){
        assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",cases[i].stored,g_settings_path));LoadSettings();assert(g_refresh_minutes==cases[i].expected);
    }
    assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",L"30",g_settings_path));LoadSettings();
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"RefreshIntervalFixture"};assert(RegisterClassW(&cls));
    HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
    const wchar_t *invalid[]={L"0",L"1",L"14",L"1441",L"15x",L""};
    for(int ru=0;ru<2;ru++)for(int i=0;i<_countof(invalid);i++){
        g_russian=ru;SetWindowTextW(s_interval,invalid[i]);assert(!SaveNativePreferences());
        assert(g_refresh_minutes==30&&GetPrivateProfileIntW(L"Jira",L"RefreshMinutes",0,g_settings_path)==30);
        wchar_t message[256];GetWindowTextW(s_note,message,256);assert(wcsstr(message,L"15")&&wcsstr(message,L"1440"));
        BeginAccountCheck();assert(!s_busy&&!s_thread);GetWindowTextW(s_note,message,256);assert(wcsstr(message,L"15")&&wcsstr(message,L"1440"));
    }
    /* The shared numeric parser also serves tray counts and avatar days: keep 1 valid there. */
    assert(JiraParseInterval(L"1")==1);SetWindowTextW(GetDlgItem(settings,213),L"1");SetWindowTextW(SettingsItem(259),L"1");
    SetWindowTextW(s_interval,L"15");assert(SaveNativePreferences());assert(g_refresh_minutes==15);LoadSettings();assert(g_refresh_minutes==15&&g_tray_task_count==1);
    SetWindowTextW(s_interval,L"1440");assert(SaveNativePreferences());LoadSettings();assert(g_refresh_minutes==1440);
    DestroyWindow(settings);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));puts("Refresh interval: legacy 1-14 raised to 15, UI rejects subminimum without saving, 15/1440 persist, unrelated numeric preferences unchanged");return 0;
}
