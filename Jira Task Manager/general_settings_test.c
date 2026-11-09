#include <windows.h>
#include <assert.h>
#include <stddef.h>
#include <shellapi.h>
static NOTIFYICONDATAW general_tray_notice;
static BOOL WINAPI GeneralCaptureTray(DWORD message,PNOTIFYICONDATAW notice){
    if(message==NIM_ADD)general_tray_notice=*notice;return TRUE;
}
static BOOL GeneralNoAccount(void){return FALSE;}
static BOOL GeneralNoUrl(wchar_t *url,size_t count){if(count)url[0]=0;return FALSE;}
static BOOL GeneralHiddenShow(HWND window,int command){
    return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;
}
#define JiraHasAccount GeneralNoAccount
#define JiraGetUrl GeneralNoUrl
#define ShowWindow GeneralHiddenShow
#define Shell_NotifyIconW GeneralCaptureTray
#include "native.c"
static BOOL visible(HWND window){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_VISIBLE)!=0;}
static RECT relative(HWND window,HWND parent){RECT r;assert(GetWindowRect(window,&r));MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;}
static void named(HWND window,const wchar_t *expected){wchar_t caption[256];GetWindowTextW(window,caption,256);assert(!wcscmp(caption,expected)&&"Visible app name must stay Jira Tasklet on every surface and language switch");}
static void readable(HWND window){
    assert(window);wchar_t text[128];GetWindowTextW(window,text,128);RECT client;GetClientRect(window,&client);
    HDC dc=GetDC(window);HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(window,WM_GETFONT,0,0));SIZE size;
    assert(GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size)&&size.cx<=client.right&&size.cy<=client.bottom);
    SelectObject(dc,old);ReleaseDC(window,dc);
}
static void layout_contract(HWND settings){
    /* This hidden fixture creates a blank-caption window directly, bypassing OpenSettingsWindow. */
    UpdateSettingsLanguage();
    named(settings,g_russian?L"Jira Tasklet · Настройки":L"Jira Tasklet · Settings");
    named(SettingsItem(230),L"Jira Tasklet");named(SettingsItem(268),APP_VERSION);assert(!SettingsItem(241));
    g_reported_scope=FALSE;UpdateScopeLabels();named(g_window,g_russian?L"Jira Tasklet — Входящие":L"Jira Tasklet — Incoming");
    g_reported_scope=TRUE;UpdateScopeLabels();named(g_window,g_russian?L"Jira Tasklet — Исходящие":L"Jira Tasklet — Outgoing");g_reported_scope=FALSE;
    const int tabs[]={200,203,202,201};RECT previous={0};
    for(int i=0;i<4;i++){
        HWND tab=GetDlgItem(settings,tabs[i]);assert(tab&&"General needs its own real native tab");readable(tab);
        RECT bounds=relative(tab,settings);
        if(bounds.left<previous.right||bounds.right>S(SETTINGS_WIDTH_DIP)-S(14))fprintf(stderr,"Tab bounds id=%d dpi=%u ru=%d: %ld..%ld previous=%ld limit=%d\n",tabs[i],g_dpi,g_russian,bounds.left,bounds.right,previous.right,S(SETTINGS_WIDTH_DIP)-S(14));
        assert(bounds.left>=previous.right&&bounds.right<=S(SETTINGS_WIDTH_DIP)-S(14));previous=bounds;
    }
    SettingsTab(0);assert(visible(s_interval)&&!visible(SettingsItem(214))&&!visible(SettingsItem(213))&&!visible(SettingsItem(257)));
    for(int connected=0;connected<2;connected++){
        s_connected=connected;LayoutSettings();
        RECT refresh=relative(s_interval,settings),value=relative(SettingsItem(226),settings),label=relative(SettingsItem(221),settings);
        assert(label.right<=value.left&&value.bottom<=refresh.top&&refresh.left==S(26)&&refresh.right==S(SETTINGS_WIDTH_DIP-26));
    }
    SettingsTab(3);assert(!visible(s_interval)&&visible(SettingsItem(214))&&visible(SettingsItem(213))&&visible(SettingsItem(257))&&!visible(s_notifications));
    const int tab_sequence[]={201,214,224,225,246,213,257,267,IDCANCEL,IDOK};
    for(int i=1;i<_countof(tab_sequence);i++){
        HWND next=GetNextDlgTabItem(settings,SettingsItem(tab_sequence[i-1]),FALSE);
        if(next!=SettingsItem(tab_sequence[i]))fprintf(stderr,"Keyboard order from %d: got %d, expected %d\n",tab_sequence[i-1],GetDlgCtrlID(next),tab_sequence[i]);
        assert(next==SettingsItem(tab_sequence[i])&&"General keyboard navigation must follow its visible top-to-bottom order");
    }
    int ids[]={247,214,248,224,225,246,222,213,257};RECT prior={0};
    for(int i=0;i<_countof(ids);i++){
        HWND item=SettingsItem(ids[i]);assert(item&&visible(item));RECT bounds=relative(item,settings);
        assert(bounds.left>=S(26)&&bounds.right<=S(SETTINGS_WIDTH_DIP-26)&&bounds.top>=prior.bottom);prior=bounds;
    }
    RECT note=relative(s_note,settings),save=relative(SettingsItem(IDOK),settings);assert(prior.bottom<=note.top&&note.bottom<save.top);
    assert(visible(s_note)&&!(GetWindowLongPtrW(settings,GWL_STYLE)&(WS_VSCROLL|WS_HSCROLL)));
    HDC dc=GetDC(s_note);HGDIOBJ font=SelectObject(dc,(HFONT)SendMessageW(s_note,WM_GETFONT,0,0));
    RECT message={0,0,note.right-note.left,0};const wchar_t *error=g_russian?
        L"Не удалось проверить аккаунт Jira. Проверьте подключение к сети и адрес сервера, затем повторите сохранение настроек.":
        L"Could not verify the Jira account. Check your network connection and the server address, then try saving the settings again.";
    DrawTextW(dc,error,-1,&message,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    assert(message.bottom<=note.bottom-note.top&&"Shared validation messages must remain readable on every page");
    SelectObject(dc,font);ReleaseDC(s_note,dc);
    const int captions[]={247,248,222,263,264,265,266,257};for(int i=0;i<_countof(captions);i++)readable(SettingsItem(captions[i]));
    SettingsTab(2);assert(visible(s_notifications)&&visible(SettingsItem(250))&&!visible(SettingsItem(257))&&!visible(SettingsItem(214)));
    assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL));
    RECT notification_bounds=relative(s_notifications,settings);
    assert(notification_bounds.bottom<=note.top&&"Notification page must not cover the shared validation message");
    SettingsTab(1);assert(visible(SettingsItem(230))&&visible(SettingsItem(242))&&!visible(SettingsItem(257)));
    /* Existing updater navigation depends on About retaining internal index 1. */
    SettingsTab(3);RECT before,after;GetWindowRect(settings,&before);
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(257,BN_CLICKED),(LPARAM)SettingsItem(257));
    assert(s_advanced&&GetWindow(s_advanced_window,GW_OWNER)==settings&&GetAncestor(SettingsItem(259),GA_ROOT)==s_advanced_window);
    named(s_advanced_window,g_russian?L"Jira Tasklet · Расширенные настройки":L"Jira Tasklet · Advanced settings");
    g_russian=!g_russian;UpdateSettingsLanguage();
    named(settings,g_russian?L"Jira Tasklet · Настройки":L"Jira Tasklet · Settings");
    named(s_advanced_window,g_russian?L"Jira Tasklet · Расширенные настройки":L"Jira Tasklet · Advanced settings");
    g_russian=!g_russian;UpdateSettingsLanguage();
    GetWindowRect(settings,&after);assert(EqualRect(&before,&after));
    SetWindowTextW(SettingsItem(259),L"14");SendMessageW(s_advanced_window,WM_CLOSE,0,0);
    assert(!s_advanced&&NativeAvatarCacheDays(g_settings_path)==7);
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(257,BN_CLICKED),(LPARAM)SettingsItem(257));
    wchar_t text[80];GetWindowTextW(SettingsItem(259),text,80);assert(s_advanced&&!wcscmp(text,L"14"));
    SetWindowTextW(SettingsItem(259),L"7");SendMessageW(s_advanced_window,WM_CLOSE,0,0);
}
static void tray_contract(HWND settings){
    HWND slider=SettingsItem(213);wchar_t class_name[80];GetClassNameW(slider,class_name,80);
    assert(!wcscmp(class_name,TRACKBAR_CLASSW)&&"Tray count must be bounded by a native slider, not free text");
    assert(SendMessageW(slider,TBM_GETRANGEMIN,0,0)==1&&SendMessageW(slider,TBM_GETRANGEMAX,0,0)==30&&SendMessageW(slider,TBM_GETPOS,0,0)==20);
    SendMessageW(slider,WM_KEYDOWN,VK_HOME,0);
    for(int value=1;value<=30;value++){
        if(value>1)SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);
        wchar_t text[32],expected[32];swprintf(expected,32,L"%d",value);GetWindowTextW(SettingsItem(263),text,32);
        assert(SendMessageW(slider,TBM_GETPOS,0,0)==value&&!wcscmp(text,expected));
        assert(g_tray_task_count==20&&GetPrivateProfileIntW(L"Jira",L"TrayTaskCount",0,g_settings_path)==20);
        assert(UpdateSettingsDirty()==(value!=20));
    }
    SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);assert(SendMessageW(slider,TBM_GETPOS,0,0)==30);
    SendMessageW(slider,WM_KEYDOWN,VK_HOME,0);SendMessageW(slider,WM_KEYDOWN,VK_LEFT,0);assert(SendMessageW(slider,TBM_GETPOS,0,0)==1);
    SendMessageW(slider,TBM_SETPOS,TRUE,99);assert(SendMessageW(slider,TBM_GETPOS,0,0)==30);
    SendMessageW(slider,TBM_SETPOS,TRUE,-5);assert(SendMessageW(slider,TBM_GETPOS,0,0)==1);
    FreezeSettings(TRUE);assert(!IsWindowEnabled(slider)&&!IsWindowEnabled(SettingsItem(257))&&!IsWindowEnabled(SettingsItem(224))&&!IsWindowEnabled(SettingsItem(259)));
    FreezeSettings(FALSE);assert(IsWindowEnabled(slider)&&IsWindowEnabled(SettingsItem(257))&&IsWindowEnabled(SettingsItem(224)));
    SendMessageW(slider,TBM_SETPOS,TRUE,17);SendMessageW(settings,WM_HSCROLL,TB_THUMBTRACK,(LPARAM)slider);
    SettingsTab(2);SettingsTab(0);SettingsTab(3);assert(SendMessageW(slider,TBM_GETPOS,0,0)==17&&UpdateSettingsDirty());
    assert(SaveNativePreferences()&&g_tray_task_count==17&&GetPrivateProfileIntW(L"Jira",L"TrayTaskCount",0,g_settings_path)==17);
    SendMessageW(slider,TBM_SETPOS,TRUE,23);HWND owned=s_advanced_window;DestroyWindow(settings);assert(!IsWindow(owned));
    LoadSettings();assert(g_tray_task_count==17);
}
int main(int argc,char **argv){
    g_instance=GetModuleHandleW(NULL);g_jira_busy=TRUE;g_dpi=96;g_russian=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&controls));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"gsp",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    AddTrayIcon();assert(!wcscmp(general_tray_notice.szTip,L"Jira Tasklet")&&general_tray_notice.hWnd==g_window);
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"GeneralSettingsFixture"};assert(RegisterClassW(&cls));
    for(int dpi=96;dpi<=192;dpi+=48)for(int language=0;language<2;language++)for(int dark=0;dark<2;dark++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_russian=language;g_dark=dark;g_density=density;RecreateBrushes();
        assert(WritePrivateProfileStringW(L"Jira",L"TrayTaskCount",L"20",g_settings_path));LoadSettings();
        HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
        if(argc>1&&!strcmp(argv[1],"slider"))tray_contract(settings);
        else {layout_contract(settings);tray_contract(settings);}
        settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
        assert(SendMessageW(SettingsItem(213),TBM_GETPOS,0,0)==17);DestroyWindow(settings);
    }
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("General settings: four native pages, owned Advanced window, tray slider endpoints/all integers, drafts, persistence, busy guards and RU/EN/DPI/themes passed");return 0;
}
