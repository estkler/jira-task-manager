#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
static int token_saved;
static BOOL TokenFixtureAccount(void){return TRUE;}
static BOOL TokenFixtureUrl(wchar_t *url,size_t capacity){wcscpy_s(url,capacity,L"https://jira.example.test/");return TRUE;}
static BOOL TokenFixtureStartup(void){return FALSE;}
static BOOL TokenFixtureSetStartup(BOOL enabled){assert(!enabled);return TRUE;}
static BOOL TokenFixtureSaveAccount(const wchar_t *url,const wchar_t *token){assert(!wcscmp(token,L"fixture-new-token-DCBA"));token_saved++;return TRUE;}
static BOOL TokenFixtureShow(HWND w,int command){return (GetWindowLongPtrW(w,GWL_STYLE)&WS_CHILD)?ShowWindow(w,command):FALSE;}
#define JiraHasAccount TokenFixtureAccount
#define JiraGetUrl TokenFixtureUrl
#define TaskManagerStartupEnabled TokenFixtureStartup
#define TaskManagerSetStartup TokenFixtureSetStartup
#define JiraSaveAccount TokenFixtureSaveAccount
#define ShowWindow TokenFixtureShow
#include "native.c"
static BOOL shown(HWND w){return w&&(GetWindowLongPtrW(w,GWL_STYLE)&WS_VISIBLE);}
static RECT relative(HWND child,HWND parent){RECT r;GetWindowRect(child,&r);MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    wchar_t directory[MAX_PATH];GetTempPathW(MAX_PATH,directory);GetTempFileNameW(directory,L"tui",0,g_settings_path);
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"TokenInfoUiFixture"};RegisterClassW(&cls);
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;RecreateBrushes();g_snapshot.account[0]=0;
        RECT outer=SettingsOuterRect();HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_CAPTION|WS_SYSMENU,30000,30000,outer.right-outer.left,outer.bottom-outer.top,g_window,NULL,g_instance,NULL);assert(settings);
        assert(!SettingsItem(272)&&!SettingsItem(273)&&"No token label controls may remain");
        assert(!SettingsItem(241)&&"The duplicated version must be absent from the settings footer");
        wchar_t caption[256];GetWindowTextW(SettingsItem(271),caption,256);assert(!wcscmp(caption,ru?L"Подключено":L"Connected"));
        GetWindowTextW(SettingsItem(274),caption,256);assert(!wcscmp(caption,ru?L"Заменить токен":L"Replace token"));
        assert(shown(SettingsItem(271))&&shown(SettingsItem(274))&&!shown(s_token));
        wcscpy_s(g_snapshot.account,256,L"Fixture account");RefreshSettingsAccountInfo();GetWindowTextW(SettingsItem(217),caption,256);assert(!wcscmp(caption,L"Fixture account"));
        RECT status=relative(SettingsItem(271),settings),action=relative(SettingsItem(274),settings),client;GetClientRect(settings,&client);assert(status.right<=action.left);
        assert(action.right<=client.right-S(34)&&"Replace link must stay inside the account frame");
        SendMessageW(settings,WM_COMMAND,274,0);assert(shown(s_token)&&!shown(SettingsItem(271))&&!shown(SettingsItem(215)));
        assert(!GetWindowTextLengthW(s_note)&&"Replacement guidance belongs beside the token field, not in the bottom status area");
        GetWindowTextW(SettingsItem(219),caption,256);assert(wcsstr(caption,ru?L"после проверки":L"after verification"));
        RECT edit_frame=relative(s_token,settings),cancel_action=relative(SettingsItem(274),settings),overlap;
        InflateRect(&edit_frame,S(6),S(3));assert(!IntersectRect(&overlap,&edit_frame,&cancel_action)&&"Cancel replacement must never erase the token field outline");
        RECT guidance=relative(SettingsItem(219),settings),interval_label=relative(SettingsItem(221),settings);
        assert(guidance.top>=edit_frame.bottom&&guidance.bottom<=interval_label.top);
        HDC measure=GetDC(settings);HGDIOBJ font=SelectObject(measure,(HFONT)SendMessageW(SettingsItem(219),WM_GETFONT,0,0));
        RECT measured={0,0,guidance.right-guidance.left,0};DrawTextW(measure,caption,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
        assert(measured.bottom<=guidance.bottom-guidance.top&&"Replacement guidance must fit completely beside its field");SelectObject(measure,font);ReleaseDC(settings,measure);
        assert((GetWindowLongPtrW(s_token,GWL_STYLE)&ES_PASSWORD)&&GetWindowTextLengthW(s_token)==0);
        SetWindowTextW(s_token,L"fixture-new-token-DCBA");assert(UpdateSettingsDirty()&&token_saved==0);
        SendMessageW(settings,WM_COMMAND,274,0);assert(!shown(s_token)&&!GetWindowTextLengthW(s_token)&&token_saved==0&&!UpdateSettingsDirty());
        SendMessageW(settings,WM_COMMAND,274,0);SendMessageW(settings,WM_COMMAND,IDOK,0);assert(IsWindow(settings)&&!s_busy&&GetWindowTextLengthW(s_note)>0);
        SetWindowTextW(s_token,L"fixture-new-token-DCBA");AccountCheck *failed=calloc(1,sizeof(*failed));failed->replace=TRUE;wcscpy_s(failed->error,256,L"Fixture verification failed");
        SendMessageW(settings,WM_ACCOUNT_CHECKED,0,(LPARAM)failed);assert(IsWindow(settings)&&shown(s_token)&&GetWindowTextLengthW(s_token)>0&&token_saved==0);
        SendMessageW(settings,WM_COMMAND,IDCANCEL,0);assert(!IsWindow(settings)&&token_saved==0);
    }
    g_dpi=96;g_russian=TRUE;RECT outer=SettingsOuterRect();HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_CAPTION|WS_SYSMENU,30000,30000,outer.right-outer.left,outer.bottom-outer.top,g_window,NULL,g_instance,NULL);
    SetWindowTextW(s_url,L"https://other.example.test/");assert(shown(s_token)&&!shown(SettingsItem(271)));
    SetWindowTextW(s_token,L"fixture-new-token-DCBA");SetWindowTextW(s_url,L"https://jira.example.test/");assert(shown(s_token)&&"Never hide an entered replacement token when returning to the original URL");
    SetWindowTextW(SettingsItem(252),L"not-an-email");
    AccountCheck *invalid=calloc(1,sizeof(*invalid));invalid->ok=invalid->replace=TRUE;wcscpy_s(invalid->url,768,L"https://jira.example.test/");wcscpy_s(invalid->token,1201,L"fixture-new-token-DCBA");
    SendMessageW(settings,WM_ACCOUNT_CHECKED,0,(LPARAM)invalid);assert(IsWindow(settings)&&token_saved==0&&"A failed preference save must not replace the current credential");
    SetWindowTextW(SettingsItem(252),L"");
    AccountCheck *replacement=calloc(1,sizeof(*replacement));replacement->ok=replacement->replace=TRUE;wcscpy_s(replacement->url,768,L"https://jira.example.test/");wcscpy_s(replacement->token,1201,L"fixture-new-token-DCBA");
    SendMessageW(settings,WM_ACCOUNT_CHECKED,0,(LPARAM)replacement);assert(token_saved==1&&!IsWindow(settings));
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Private token UI: no hints/labels, connected state, masked draft, cancel/failure/success, URL changes and RU/EN/themes/DPI passed");
}
