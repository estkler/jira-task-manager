#include <windows.h>
#include <assert.h>
BOOL FixtureNoAccount(void){return FALSE;}
BOOL FixtureNoUrl(wchar_t *url,size_t count){(void)count;url[0]=0;return FALSE;}
#define JiraHasAccount FixtureNoAccount
#define JiraGetUrl FixtureNoUrl
#include "native.c"
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temporary[MAX_PATH];assert(GetTempPathW(MAX_PATH,temporary));assert(GetTempFileNameW(temporary,L"avp",0,g_settings_path));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.lpszClassName=L"AvatarSettingsFixture";assert(RegisterClassW(&wc));
    HWND settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);SettingsTab(2);
    /* Advanced is an owned top-level window, never an inline scroll disclosure. */
    assert(SettingsItem(257)&&SettingsItem(259));
    HWND advanced=GetAncestor(SettingsItem(259),GA_ROOT);assert(advanced!=settings);
    assert(GetWindow(advanced,GW_OWNER)==settings);
    assert(!(GetWindowLongPtrW(advanced,GWL_STYLE)&WS_VISIBLE));
    assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL));
    wchar_t text[80];GetWindowTextW(SettingsItem(259),text,80);assert(!wcscmp(text,L"7"));
    RECT original;GetWindowRect(settings,&original);
    SendMessageW(s_notifications,WM_COMMAND,257,0);
    assert(GetWindowLongPtrW(advanced,GWL_STYLE)&WS_VISIBLE);
    assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL));
    RECT input,viewport;GetWindowRect(SettingsItem(259),&input);MapWindowPoints(NULL,advanced,(POINT*)&input,2);GetClientRect(advanced,&viewport);assert(input.top>=0&&input.bottom<=viewport.bottom);
    SetWindowTextW(SettingsItem(259),L"14");assert(UpdateSettingsDirty());
    SendMessageW(advanced,WM_CLOSE,0,0);assert(!(GetWindowLongPtrW(advanced,GWL_STYLE)&WS_VISIBLE));
    assert(NativeAvatarCacheDays(g_settings_path)==7); /* Close keeps a draft, not a disk write. */
    SendMessageW(s_notifications,WM_COMMAND,257,0);GetWindowTextW(SettingsItem(259),text,80);assert(!wcscmp(text,L"14"));
    assert(SaveNativePreferences());
    assert(NativeAvatarCacheDays(g_settings_path)==14);
    SetWindowTextW(SettingsItem(259),L"0");assert(!SaveNativePreferences());assert(NativeAvatarCacheDays(g_settings_path)==14);
    SetWindowTextW(SettingsItem(259),L"366");assert(!SaveNativePreferences());assert(NativeAvatarCacheDays(g_settings_path)==14);
    SetWindowTextW(SettingsItem(259),L"14");SendMessageW(advanced,WM_CLOSE,0,0);
    assert(!(GetWindowLongPtrW(advanced,GWL_STYLE)&WS_VISIBLE));assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL));
    RECT after;GetWindowRect(settings,&after);assert(EqualRect(&original,&after));
    DestroyWindow(settings);assert(!IsWindow(advanced));settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
    GetWindowTextW(SettingsItem(259),text,80);assert(!wcscmp(text,L"14"));
    SetWindowTextW(SettingsItem(259),L"21");DestroyWindow(settings);assert(NativeAvatarCacheDays(g_settings_path)==14);
    settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
    GetWindowTextW(SettingsItem(259),text,80);assert(!wcscmp(text,L"14")); /* Main Cancel discards child edits. */
    advanced=GetAncestor(SettingsItem(259),GA_ROOT);RECT work=SettingsWorkAreaFor(settings);
    SetWindowPos(settings,NULL,max(work.left,work.right-S(830)),work.top,S(430),S(360),SWP_NOZORDER|SWP_NOACTIVATE);
    SendMessageW(s_notifications,WM_COMMAND,257,0);RECT suggested;GetWindowRect(advanced,&suggested);
    SendMessageW(advanced,WM_DPICHANGED,MAKEWPARAM(192,192),(LPARAM)&suggested);
    LOGFONTW scaled_font;assert(GetObjectW((HFONT)SendMessageW(SettingsItem(259),WM_GETFONT,0,0),sizeof(scaled_font),&scaled_font));assert(scaled_font.lfHeight==-24);
    MSG pending;int deferred=0;while(PeekMessageW(&pending,advanced,0,0,PM_REMOVE)){assert(deferred++<100);DispatchMessageW(&pending);}
    RECT final_frame;assert(VisibleWindowFrame(advanced,&final_frame));assert(final_frame.left>=work.left&&final_frame.right<=work.right&&final_frame.top>=work.top&&final_frame.bottom<=work.bottom);
    GetWindowTextW(SettingsItem(259),text,80);assert(!wcscmp(text,L"14"));
    SendMessageW(advanced,WM_DPICHANGED,MAKEWPARAM(96,96),(LPARAM)&suggested);
    g_russian=FALSE;UpdateSettingsLanguage();GetWindowTextW(SettingsItem(257),text,80);assert(wcsstr(text,L"Advanced"));
    g_russian=TRUE;UpdateSettingsLanguage();GetWindowTextW(SettingsItem(257),text,80);assert(wcsstr(text,L"Расширенные"));
    DestroyWindow(settings);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Avatar settings: owned Advanced window, preserved drafts, shared Save/Cancel, validation and unchanged parent layout passed");return 0;
}
