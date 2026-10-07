/* Owned Win32 settings window. Included after the main UI helpers. */
#define WM_ACCOUNT_CHECKED (WM_APP+40)
#define SETTINGS_WIDTH_DIP 430
#define SETTINGS_HEIGHT_DIP 380
typedef struct AccountCheck {
    HWND window; wchar_t url[768],token[1201],account[256],error[256]; BOOL ok,replace;
} AccountCheck;
static HWND s_window,s_url,s_token,s_interval,s_note;
static HWND s_pages[2][32];static int s_counts[2],s_tab;
static BOOL s_busy,s_saved,s_connected;
static BOOL s_initializing,s_has_account;
static wchar_t s_original_url[768];
static BOOL s_original_startup;
static BOOL UpdateSettingsAccountBusy(void){return s_busy;}
static BOOL UpdateSettingsDirty(void){
    if(!s_window||!IsWindow(s_window))return FALSE;wchar_t url[768],interval[16],tray[16];
    GetWindowTextW(s_url,url,768);GetWindowTextW(s_interval,interval,16);GetWindowTextW(GetDlgItem(s_window,213),tray,16);
    return wcscmp(url,s_original_url)||GetWindowTextLengthW(s_token)>0||JiraParseInterval(interval)!=g_refresh_minutes||JiraParseInterval(tray)!=g_tray_task_count||
        (SendMessageW(GetDlgItem(s_window,214),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_startup;
}
static HFONT s_font,s_small,s_bold,s_title;
static HWND s_hover;
static HANDLE s_thread;
static int S(int value){return MulDiv(value,g_dpi,96);}
static int ClampSettingCoordinate(int value,int minimum,int maximum)
{
    if(maximum<minimum)return minimum;
    return value<minimum?minimum:value>maximum?maximum:value;
}
static POINT SettingsWindowOrigin(RECT owner,RECT work_area,SIZE settings_size)
{
    int gap=S(2),max_x=work_area.right-settings_size.cx,max_y=work_area.bottom-settings_size.cy;
    POINT origin={0};
    origin.y=ClampSettingCoordinate(owner.top,work_area.top,max_y);
    if(owner.right+gap+settings_size.cx<=work_area.right){origin.x=owner.right+gap;return origin;}
    if(owner.left-gap-settings_size.cx>=work_area.left){origin.x=owner.left-gap-settings_size.cx;return origin;}
    origin.x=ClampSettingCoordinate(owner.left,work_area.left,max_x);
    if(owner.bottom+gap+settings_size.cy<=work_area.bottom){origin.y=owner.bottom+gap;return origin;}
    if(owner.top-gap-settings_size.cy>=work_area.top){origin.y=owner.top-gap-settings_size.cy;return origin;}
    origin.x=ClampSettingCoordinate(owner.right+gap,work_area.left,max_x);
    return origin;
}
static BOOL VisibleWindowFrame(HWND window,RECT *frame)
{
    if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_EXTENDED_FRAME_BOUNDS,frame,sizeof(*frame))))return TRUE;
    return GetWindowRect(window,frame);
}
static RECT CurrentSettingsWorkArea(void)
{
    MONITORINFO monitor_info={sizeof(monitor_info)};
    if(!GetMonitorInfoW(MonitorFromWindow(g_window,MONITOR_DEFAULTTONEAREST),&monitor_info))
        SystemParametersInfoW(SPI_GETWORKAREA,0,&monitor_info.rcWork,0);
    return monitor_info.rcWork;
}
static POINT AlignedSettingsOuterOrigin(RECT settings_outer,RECT settings_frame,POINT desired_frame)
{
    POINT outer={settings_outer.left+desired_frame.x-settings_frame.left,
        settings_outer.top+desired_frame.y-settings_frame.top};
    return outer;
}
static POINT PreliminarySettingsOrigin(void)
{
    RECT owner;if(!VisibleWindowFrame(g_window,&owner))SetRectEmpty(&owner);
    SIZE size={S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP)};
    return SettingsWindowOrigin(owner,CurrentSettingsWorkArea(),size);
}
static void PositionSettingsWindow(HWND settings)
{
    RECT owner_frame,settings_frame,settings_outer;
    if(!VisibleWindowFrame(g_window,&owner_frame)||!VisibleWindowFrame(settings,&settings_frame)||!GetWindowRect(settings,&settings_outer))return;
    SIZE frame_size={settings_frame.right-settings_frame.left,settings_frame.bottom-settings_frame.top};
    POINT desired_frame=SettingsWindowOrigin(owner_frame,CurrentSettingsWorkArea(),frame_size);
    POINT outer=AlignedSettingsOuterOrigin(settings_outer,settings_frame,desired_frame);
    SetWindowPos(settings,NULL,outer.x,outer.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}
static HWND SettingsControl(const wchar_t *class_name,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id,int page)
{
    HWND control=CreateWindowExW(0,class_name,text,
        WS_CHILD|WS_VISIBLE|style,S(x),S(y),S(w),S(h),s_window,(HMENU)(INT_PTR)id,g_instance,NULL);
    SendMessageW(control,WM_SETFONT,(WPARAM)s_font,TRUE);
    if(!wcscmp(class_name,L"EDIT"))SetWindowTheme(control,g_dark?L"DarkMode_CFD":L"Explorer",NULL);
    if(page>=0)s_pages[page][s_counts[page]++]=control;
    return control;
}
static BOOL NativeStartupEnabled(void)
{
    return TaskManagerStartupEnabled();
}
static BOOL SetNativeStartup(BOOL enabled)
{
    return TaskManagerSetStartup(enabled);
}
static LRESULT CALLBACK SettingsEditProcedure(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(m==WM_SETFOCUS){LRESULT result=DefSubclassProc(w,m,wp,lp);InvalidateRect(s_window,NULL,FALSE);return result;}
    if(m==WM_KILLFOCUS)InvalidateRect(s_window,NULL,FALSE);
    return DefSubclassProc(w,m,wp,lp);
}
static LRESULT CALLBACK SettingsButtonProcedure(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(m==WM_MOUSEMOVE&&s_hover!=w){s_hover=w;TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,w,0};TrackMouseEvent(&track);InvalidateRect(w,NULL,FALSE);}
    if(m==WM_MOUSELEAVE){s_hover=NULL;InvalidateRect(w,NULL,FALSE);}
    if(m==WM_SETCURSOR){SetCursor(LoadCursorW(NULL,IDC_HAND));return TRUE;}
    return DefSubclassProc(w,m,wp,lp);
}
static void PlaceSettings(int id,int x,int y,int w,int h)
{
    HWND control=GetDlgItem(s_window,id);if(control)SetWindowPos(control,NULL,S(x),S(y),S(w),S(h),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
}
static void LayoutSettingsTabs(void)
{
    if(!s_window||!s_font)return;
    HDC dc=GetDC(s_window);HGDIOBJ old=SelectObject(dc,s_font);int x=14;
    for(int id=200;id<=201;id++){
        wchar_t text[80];SIZE size;GetWindowTextW(GetDlgItem(s_window,id),text,80);
        GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size);
        int width=MulDiv(size.cx,96,g_dpi)+24;PlaceSettings(id,x,14,width,30);x+=width;
    }
    SelectObject(dc,old);ReleaseDC(s_window,dc);
}
static void LayoutSettings(void)
{
    RECT rc;GetClientRect(s_window,&rc);int width=MulDiv(rc.right,96,g_dpi),height=MulDiv(rc.bottom,96,g_dpi);
    int field_width=width-52,preferences=s_connected?170:196;
    PlaceSettings(210,32,81,field_width-12,18);
    PlaceSettings(218,26,110,field_width,18);PlaceSettings(211,32,134,field_width-12,18);
    PlaceSettings(219,26,160,field_width,34);
    PlaceSettings(216,34,121,field_width-90,15);PlaceSettings(217,34,137,field_width-90,18);
    PlaceSettings(215,width-26-62,121,54,28);
    PlaceSettings(221,26,preferences,155,25);PlaceSettings(212,187,preferences+4,36,18);
    PlaceSettings(222,244,preferences,80,25);PlaceSettings(213,width-26-42,preferences+4,36,18);
    PlaceSettings(214,26,preferences+34,field_width,25);PlaceSettings(223,26,preferences+64,field_width,28);
    PlaceSettings(241,14,height-42,180,28);PlaceSettings(IDCANCEL,width-162,height-42,66,28);PlaceSettings(IDOK,width-90,height-42,76,28);
    PlaceSettings(235,26,143,width-52,28);PlaceSettings(242,26,172,260,28);PlaceSettings(234,26,206,220,28);PlaceSettings(236,26,242,width-52,20);PlaceSettings(238,26,266,150,28);
    int connected_ids[]={215,216,217};for(int i=0;i<3;i++)ShowWindow(GetDlgItem(s_window,connected_ids[i]),s_tab==0&&s_connected?SW_SHOW:SW_HIDE);
    int token_ids[]={211,218,219};for(int i=0;i<3;i++)ShowWindow(GetDlgItem(s_window,token_ids[i]),s_tab==0&&!s_connected?SW_SHOW:SW_HIDE);
    LayoutSettingsTabs();
    RedrawWindow(s_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void UpdateSettingsLanguage(void)
{
    if(!s_window||!IsWindow(s_window))return;
    SetWindowTextW(s_window,g_russian?L"Jira Task Manager · Настройки":L"Jira Task Manager · Settings");
    SetWindowTextW(GetDlgItem(s_window,200),L"Jira");
    SetWindowTextW(GetDlgItem(s_window,201),g_russian?L"О приложении":L"About");
    SetWindowTextW(GetDlgItem(s_window,216),g_russian?L"Аккаунт Jira":L"Jira account");
    if(!g_snapshot.account[0])SetWindowTextW(GetDlgItem(s_window,217),g_russian?L"Подключён":L"Connected");
    SetWindowTextW(GetDlgItem(s_window,215),g_russian?L"Выйти":L"Sign out");
    SetWindowTextW(GetDlgItem(s_window,219),g_russian?L"Создать в Jira: Профиль → Personal Access Tokens\r\nСохраняется после проверки · Хранилище Windows":L"Create in Jira: Profile → Personal Access Tokens\r\nSaved after verification · Windows Credential Manager");
    SetWindowTextW(GetDlgItem(s_window,221),g_russian?L"Интервал обновления, мин":L"Update interval, min");
    SetWindowTextW(GetDlgItem(s_window,222),g_russian?L"Задач в трее":L"Tray tasks");
    SetWindowTextW(GetDlgItem(s_window,214),g_russian?L"Запускать Jira Task Manager вместе с Windows":L"Start Jira Task Manager with Windows");
    SetWindowTextW(GetDlgItem(s_window,231),g_russian?L"Приложение для обработки статусов задач Jira.":L"A Jira task status management app.");
    SetWindowTextW(GetDlgItem(s_window,232),g_russian?L"Обновления":L"Updates");
    UpdateUiRender(s_window);
    SetWindowTextW(GetDlgItem(s_window,234),g_russian?L"Открыть папку диагностики":L"Open diagnostics folder");
    SetWindowTextW(GetDlgItem(s_window,236),g_russian?L"Обратная связь":L"Feedback");
    SetWindowTextW(GetDlgItem(s_window,238),g_russian?L"⚡ Быстрая связь":L"⚡ Quick contact");
    SetWindowTextW(GetDlgItem(s_window,IDCANCEL),g_russian?L"Отмена":L"Cancel");
    SetWindowTextW(GetDlgItem(s_window,IDOK),g_russian?L"Сохранить":L"Save");
    LayoutSettings();
}
static void SettingsTab(int page)
{
    s_tab=page;for(int p=0;p<2;p++)for(int i=0;i<s_counts[p];i++)ShowWindow(s_pages[p][i],p==page?SW_SHOW:SW_HIDE);
    for(int id=200;id<202;id++)InvalidateRect(GetDlgItem(s_window,id),NULL,TRUE);
    LayoutSettings();
}
static BOOL SaveNativePreferences(void)
{
    wchar_t interval[16],tray[16];GetWindowTextW(s_interval,interval,16);GetWindowTextW(GetDlgItem(s_window,213),tray,16);
    int minutes=JiraParseInterval(interval),count=JiraParseInterval(tray);
    if(!minutes||count<1||count>30){SetWindowTextW(s_note,g_russian?L"Интервал: 1–1440 минут. Задач в трее: 1–30.":L"Interval: 1–1440 minutes. Tray tasks: 1–30.");return FALSE;}
    BOOL startup=SendMessageW(GetDlgItem(s_window,214),BM_GETCHECK,0,0)==BST_CHECKED;
    if(startup!=NativeStartupEnabled()&&!SetNativeStartup(startup)){SetWindowTextW(s_note,g_russian?L"Не удалось сохранить автозапуск Native.":L"Could not save Native startup preference.");return FALSE;}
    if(!WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",interval,g_settings_path)||!WritePrivateProfileStringW(L"Jira",L"TrayTaskCount",tray,g_settings_path)){
        SetWindowTextW(s_note,g_russian?L"Не удалось сохранить настройки. Проверьте доступ к папке.":L"Could not save settings. Check folder access.");return FALSE;
    }
    g_refresh_minutes=minutes;g_tray_task_count=count;SetTimer(g_window,3,minutes*60000,NULL);return TRUE;
}
static void FreezeSettings(BOOL freeze)
{
    int ids[]={IDOK,210,211,212,213,214,215};for(int i=0;i<7;i++)EnableWindow(GetDlgItem(s_window,ids[i]),!freeze);
}
static DWORD WINAPI CheckAccountWorker(void *arg)
{
    AccountCheck *check=arg;JiraConnection client={0};
    BOOL connected=check->replace?JiraConnectWithToken(&client,check->url,check->token):JiraConnect(&client);
    check->ok=connected&&JiraVerifyAccount(&client,check->account,256);
    if(!check->ok)wcscpy_s(check->error,256,client.error);
    JiraDisconnect(&client);PostMessageW(check->window,WM_ACCOUNT_CHECKED,0,(LPARAM)check);return 0;
}
static void BeginAccountCheck(void)
{
    if(UpdateUiBusy()||g_update_handoff){SetWindowTextW(s_note,g_russian?L"Дождитесь завершения операции обновления приложения.":L"Wait for the application update operation.");return;}
    if(g_jira_busy){SetWindowTextW(s_note,g_russian?L"Дождитесь завершения текущего запроса Jira и нажмите «Сохранить» снова.":L"Wait for the current Jira request, then click Save again.");return;}
    wchar_t interval[16],previous[768];GetWindowTextW(s_interval,interval,16);
    if(!JiraParseInterval(interval)){SetWindowTextW(s_note,g_russian?L"Интервал: целое число от 1 до 1440 минут.":L"Interval must be 1–1440 whole minutes.");return;}
    wchar_t tray[16];GetWindowTextW(GetDlgItem(s_window,213),tray,16);int count=JiraParseInterval(tray);
    if(count<1||count>30){SetWindowTextW(s_note,g_russian?L"Число задач в трее: от 1 до 30.":L"Tray task count must be 1–30.");return;}
    AccountCheck *check=calloc(1,sizeof(*check));if(!check)return;
    check->window=s_window;GetWindowTextW(s_url,check->url,768);GetWindowTextW(s_token,check->token,1201);
    check->replace=check->token[0]!=0;
    JiraConnection parsed={0};BOOL valid=JiraParseUrl(&parsed,check->url);
    if(!valid||(!check->replace&&(!JiraGetUrl(previous,768)||wcscmp(previous,check->url)))){
        SetWindowTextW(s_note,g_russian?L"Укажите HTTPS URL. Для нового адреса нужен токен.":L"Use an HTTPS URL. A new address requires a token.");
        SecureZeroMemory(check,sizeof(*check));free(check);return;
    }
    s_busy=TRUE;g_jira_busy=TRUE;
    FreezeSettings(TRUE);
    SetWindowTextW(s_note,g_russian?L"Проверка аккаунта Jira…":L"Checking Jira account…");
    s_thread=CreateThread(NULL,0,CheckAccountWorker,check,0,NULL);
    if(!s_thread){s_busy=g_jira_busy=FALSE;FreezeSettings(FALSE);SetWindowTextW(s_note,L"Could not start connection check.");SecureZeroMemory(check,sizeof(*check));free(check);}
}
static void ApplySignedOutSettingsUi(const wchar_t *previous_url)
{
    JiraConnection parsed={0};BOOL valid_url=previous_url&&JiraParseUrl(&parsed,previous_url);
    s_initializing=TRUE;s_has_account=s_connected=FALSE;
    if(valid_url){wcscpy_s(s_original_url,768,previous_url);SetWindowTextW(s_url,previous_url);}
    else{s_original_url[0]=0;SetWindowTextW(s_url,L"");}
    SetWindowTextW(s_token,L"");
    SetWindowTextW(s_note,g_russian?L"Вы вышли. Введите токен для подключения.":L"Signed out. Enter a token to connect.");
    s_initializing=FALSE;LayoutSettings();
    RedrawWindow(s_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    SetFocus(s_token);
}
static LRESULT CALLBACK SettingsProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message){
    case WM_CREATE:{
        s_window=window;s_initializing=TRUE;memset(s_counts,0,sizeof(s_counts));s_busy=s_saved=FALSE;s_hover=NULL;s_has_account=s_connected=JiraHasAccount();
        s_font=CreateFontW(-S(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        s_small=CreateFontW(-S(10),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        s_bold=CreateFontW(-S(12),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Semibold");
        s_title=CreateFontW(-S(18),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        HDC measure=GetDC(window);HGDIOBJ old=SelectObject(measure,s_font);int x=14;
        const wchar_t *tabs[]={L"Jira",g_russian?L"О приложении":L"About"};
        for(int i=0;i<2;i++){SIZE size;GetTextExtentPoint32W(measure,tabs[i],(int)wcslen(tabs[i]),&size);int width=MulDiv(size.cx,96,g_dpi)+24;SettingsControl(L"BUTTON",tabs[i],WS_TABSTOP|BS_OWNERDRAW,x,14,width,30,200+i,-1);x+=width;}
        SelectObject(measure,old);ReleaseDC(window,measure);
        SettingsControl(L"STATIC",L"URL",0,26,60,360,18,209,0);
        s_url=SettingsControl(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,30,91,390,26,210,0);
        SendMessageW(s_url,EM_SETLIMITTEXT,512,0);s_original_url[0]=0;if(JiraGetUrl(s_original_url,768))SetWindowTextW(s_url,s_original_url);
        SettingsControl(L"STATIC",L"Personal access token",0,26,114,360,18,218,0);
        s_token=SettingsControl(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD,30,154,390,26,211,0);
        SendMessageW(s_token,EM_SETLIMITTEXT,1200,0);
        SettingsControl(L"STATIC",g_russian?L"Создать в Jira: Профиль → Personal Access Tokens\r\nСохраняется после проверки · Хранилище Windows":L"Create in Jira: Profile → Personal Access Tokens\r\nSaved after verification · Windows Credential Manager",0,26,166,360,48,219,0);
        SettingsControl(L"STATIC",g_russian?L"Аккаунт Jira":L"Jira account",0,34,121,220,15,216,0);
        SettingsControl(L"STATIC",g_snapshot.account[0]?g_snapshot.account:(g_russian?L"Подключён":L"Connected"),0,34,137,220,18,217,0);
        SettingsControl(L"BUTTON",g_russian?L"Выйти":L"Sign out",WS_TABSTOP|BS_OWNERDRAW,326,121,54,28,215,0);
        SettingsControl(L"STATIC",g_russian?L"Интервал обновления, мин":L"Update interval, min",SS_CENTERIMAGE,26,170,155,25,221,0);
        wchar_t interval[16];swprintf(interval,16,L"%d",g_refresh_minutes);
        s_interval=SettingsControl(L"EDIT",interval,WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,325,267,95,26,212,0);SendMessageW(s_interval,EM_SETLIMITTEXT,4,0);
        SettingsControl(L"STATIC",g_russian?L"Задач в трее":L"Tray tasks",SS_CENTERIMAGE,244,170,80,25,222,0);
        swprintf(interval,16,L"%d",g_tray_task_count);SettingsControl(L"EDIT",interval,WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,340,174,36,18,213,0);
        SendMessageW(GetDlgItem(window,213),EM_SETLIMITTEXT,2,0);
        SettingsControl(L"BUTTON",g_russian?L"Запускать Jira Task Manager вместе с Windows":L"Start Jira Task Manager with Windows",WS_TABSTOP|BS_AUTOCHECKBOX,26,209,360,25,214,0);
        s_original_startup=NativeStartupEnabled();SendMessageW(GetDlgItem(window,214),BM_SETCHECK,s_original_startup?BST_CHECKED:BST_UNCHECKED,0);
        s_note=SettingsControl(L"STATIC",L"",0,26,245,360,52,223,0);
        SettingsControl(L"STATIC",L"Jira Task Manager " APP_VERSION,0,26,60,360,28,230,1);
        SettingsControl(L"STATIC",g_russian?L"Приложение для обработки статусов задач Jira.":L"A Jira task status management app.",0,26,90,360,22,231,1);
        SettingsControl(L"STATIC",g_russian?L"Обновления":L"Updates",0,26,121,360,20,232,1);
        SettingsControl(L"STATIC",L"",0,26,143,360,28,235,1);
        SettingsControl(L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,26,172,260,28,242,1);
        SettingsControl(L"BUTTON",g_russian?L"Открыть папку диагностики":L"Open diagnostics folder",WS_TABSTOP|BS_OWNERDRAW,26,176,185,28,234,1);
        SettingsControl(L"STATIC",g_russian?L"Обратная связь":L"Feedback",0,26,218,360,20,236,1);
        SettingsControl(L"BUTTON",g_russian?L"⚡ Быстрая связь":L"⚡ Quick contact",WS_TABSTOP|BS_OWNERDRAW,26,241,136,28,238,1);
        SettingsControl(L"STATIC",L"Jira Task Manager " APP_VERSION,SS_CENTERIMAGE,14,392,180,28,241,-1);
        SettingsControl(L"BUTTON",g_russian?L"Отмена":L"Cancel",WS_TABSTOP|BS_OWNERDRAW,265,392,76,28,IDCANCEL,-1);
        SettingsControl(L"BUTTON",g_russian?L"Сохранить":L"Save",WS_TABSTOP|BS_OWNERDRAW,349,392,85,28,IDOK,-1);
        int small_ids[]={216,219,223,235,241};for(int i=0;i<5;i++)SendMessageW(GetDlgItem(window,small_ids[i]),WM_SETFONT,(WPARAM)s_small,TRUE);
        SendMessageW(GetDlgItem(window,230),WM_SETFONT,(WPARAM)s_title,TRUE);
        SendMessageW(GetDlgItem(window,232),WM_SETFONT,(WPARAM)s_bold,TRUE);SendMessageW(GetDlgItem(window,236),WM_SETFONT,(WPARAM)s_bold,TRUE);
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            wchar_t name[32];GetClassNameW(child,name,32);
            if(!wcscmp(name,L"EDIT"))SetWindowSubclass(child,SettingsEditProcedure,1,0);
            if(!wcscmp(name,L"BUTTON"))SetWindowSubclass(child,SettingsButtonProcedure,1,0);
        }
        ApplyWindowCaptionTheme(window);s_initializing=FALSE;UpdateUiAttach(window);SettingsTab(0);return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==242){if(u_state==UPDATE_UI_AVAILABLE)UpdateUiBeginDownload(window);else if(u_state==UPDATE_UI_READY)UpdateUiBeginInstall(window);else UpdateUiBeginCheck(window);return 0;}
        if(LOWORD(wp)==210&&HIWORD(wp)==EN_CHANGE&&!s_initializing&&!s_busy){wchar_t entered[768];GetWindowTextW(s_url,entered,768);s_connected=s_has_account&&!wcscmp(entered,s_original_url);LayoutSettings();return 0;}
        if(LOWORD(wp)>=200&&LOWORD(wp)<=201){SettingsTab(LOWORD(wp)-200);return 0;}
        if(LOWORD(wp)==IDOK&&!s_busy){BeginAccountCheck();return 0;}
        if(LOWORD(wp)==215&&!s_busy){
            if(UpdateUiBusy()||g_update_handoff)return 0;
            if(g_jira_busy){SetWindowTextW(s_note,g_russian?L"Дождитесь завершения текущего запроса Jira перед выходом.":L"Wait for the current Jira request before signing out.");return 0;}
            wchar_t signed_out_url[768];GetWindowTextW(s_url,signed_out_url,768);
            s_busy=g_jira_busy=TRUE;
            int answer=MessageBoxW(window,g_russian?L"Выйти из Jira в Jira Task Manager?":L"Sign out of Jira in Jira Task Manager?",APP_TITLE,MB_YESNO|MB_ICONQUESTION);
            s_busy=g_jira_busy=FALSE;if(answer!=IDYES)return 0;
            if(!JiraSignOut()){SetWindowTextW(s_note,g_russian?L"Не удалось полностью удалить учётные данные Native. Повторите выход.":L"Could not fully remove Native credentials. Try signing out again.");return 0;}
            s_has_account=s_connected=FALSE;s_saved=FALSE;g_jira_stale=TRUE;g_updated_time=(SYSTEMTIME){0};
            FreeTaskItems();g_status_count=0;JiraFreeSnapshot(&g_snapshot);
            for(int i=0;i<3;i++)g_filter_masks[i]=~0ull;
            wcscpy_s(g_jira_message,256,g_russian?L"Вы вышли из Jira. Подключитесь в настройках.":L"Signed out of Jira. Connect in settings.");RefreshTaskList();UpdateLastUpdated();
            ApplySignedOutSettingsUi(signed_out_url);return 0;
        }
        if(LOWORD(wp)==234){
            wchar_t folder[MAX_PATH],parameters[MAX_PATH+3],explorer[MAX_PATH];
            DWORD executable_length=GetModuleFileNameW(NULL,folder,MAX_PATH);
            BOOL valid=executable_length>0&&executable_length<MAX_PATH&&TaskManagerDiagnosticsDirectory(folder,folder,MAX_PATH);
            int result=valid?SHCreateDirectoryExW(window,folder,NULL):ERROR_BAD_PATHNAME;
            DWORD attributes=valid?GetFileAttributesW(folder):INVALID_FILE_ATTRIBUTES;
            if((result!=ERROR_SUCCESS&&result!=ERROR_ALREADY_EXISTS&&result!=ERROR_FILE_EXISTS)||attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)){
                MessageBoxW(window,g_russian?L"Не удалось создать папку diagnostics приложения.":L"Could not create the application diagnostics folder.",APP_TITLE,MB_OK|MB_ICONERROR);return 0;
            }
            UINT length=GetWindowsDirectoryW(explorer,MAX_PATH);
            if(!length||length>=MAX_PATH||!PathAppendW(explorer,L"explorer.exe"))return 0;
            swprintf(parameters,MAX_PATH+3,L"\"%ls\"",folder);
            if((INT_PTR)ShellExecuteW(window,L"open",explorer,parameters,NULL,SW_SHOWNORMAL)<=32)
                MessageBoxW(window,g_russian?L"Не удалось открыть Проводник Windows.":L"Could not open Windows Explorer.",APP_TITLE,MB_OK|MB_ICONERROR);
            return 0;
        }
        if(LOWORD(wp)==238){ShellExecuteW(window,L"open",L"https://t.me/gooroman",NULL,NULL,SW_SHOWNORMAL);return 0;}
        if(LOWORD(wp)==IDCANCEL){SendMessageW(window,WM_CLOSE,0,0);return 0;}break;
    case WM_ACCOUNT_CHECKED:{
        AccountCheck *check=(AccountCheck*)lp;CloseHandle(s_thread);s_thread=NULL;s_busy=g_jira_busy=FALSE;
        if(check->ok&&check->replace){
            if(!JiraSaveAccount(check->url,check->token)){check->ok=FALSE;wcscpy_s(check->error,256,L"Could not save account in Windows Credential Manager.");}
            else{s_saved=TRUE;g_jira_stale=TRUE;}
        }
        if(check->ok){
            if(SaveNativePreferences())s_saved=TRUE;
            else{check->ok=FALSE;GetWindowTextW(s_note,check->error,256);}
        }
        if(!check->ok){SetWindowTextW(s_note,check->error);SettingsTab(0);FreezeSettings(FALSE);}
        BOOL done=check->ok;SecureZeroMemory(check,sizeof(*check));free(check);
        if(done)DestroyWindow(window);return 0;
    }
    case WM_CLOSE:if(s_busy){MessageBeep(MB_ICONINFORMATION);return 0;}DestroyWindow(window);return 0;
    case WM_SIZE:if(s_url)LayoutSettings();return 0;
    case WM_DESTROY:
        UpdateUiCancel();
        SetWindowTextW(s_token,L"");s_window=NULL;DeleteObject(s_font);DeleteObject(s_small);DeleteObject(s_bold);DeleteObject(s_title);
        if(s_saved&&!InterlockedCompareExchange(&g_jira_closing,0,0)){g_jira_stale=TRUE;PostMessageW(g_window,WM_COMMAND,IDC_REFRESH,0);}
        return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{
        HDC dc=(HDC)wp;int id=GetDlgCtrlID((HWND)lp);BOOL muted=id==209||id==216||id==219||id==223||id==235||id==239||id==241;
        BOOL input=message==WM_CTLCOLOREDIT;SetTextColor(dc,muted?MutedColor():TextColor());SetBkColor(dc,input?PanelColor():WindowColor());return (LRESULT)(input?g_panel_brush:g_window_brush);
    }
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect((HDC)wp,&r,g_window_brush);return 1;}
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);RECT r={S(14),S(44),client.right-S(14),client.bottom-S(52)};HBRUSH b=CreateSolidBrush(g_dark?RGB(65,65,68):RGB(210,210,210));FrameRect(dc,&r,b);
        if(s_tab==0){
            if(s_connected){RECT account={S(26),S(114),client.right-S(26),S(158)};FrameRect(dc,&account,b);}
            int edits[]={210,211,212,213};for(int i=0;i<4;i++){
                HWND edit=GetDlgItem(window,edits[i]);if(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VISIBLE))continue;
                RECT box;GetWindowRect(edit,&box);MapWindowPoints(NULL,window,(POINT*)&box,2);InflateRect(&box,S(6),S(3));
                FillRect(dc,&box,g_panel_brush);HBRUSH edge=CreateSolidBrush(GetFocus()==edit?AccentColor():(g_dark?RGB(85,85,90):RGB(175,175,175)));FrameRect(dc,&box,edge);DeleteObject(edge);
            }
        }DeleteObject(b);EndPaint(window,&paint);return 0;}
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *draw=(DRAWITEMSTRUCT*)lp;RECT r=draw->rcItem;BOOL tab=draw->CtlID>=200&&draw->CtlID<=201;
        COLORREF background=!tab&&(s_hover==draw->hwndItem||(draw->itemState&ODS_SELECTED))?(g_dark?RGB(52,57,64):RGB(231,235,240)):WindowColor();HBRUSH fill=CreateSolidBrush(background);FillRect(draw->hDC,&r,fill);DeleteObject(fill);
        SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,draw->itemState&ODS_DISABLED?MutedColor():TextColor());SelectObject(draw->hDC,s_font);
        wchar_t text[64];GetWindowTextW(draw->hwndItem,text,64);DrawTextW(draw->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(!tab){HBRUSH border=CreateSolidBrush(draw->CtlID==IDOK||s_hover==draw->hwndItem||draw->itemState&ODS_FOCUS?AccentColor():(g_dark?RGB(85,85,90):RGB(175,175,175)));FrameRect(draw->hDC,&r,border);DeleteObject(border);}
        if((int)draw->CtlID-200==s_tab){r.top=r.bottom-S(2);HBRUSH b=CreateSolidBrush(AccentColor());FillRect(draw->hDC,&r,b);DeleteObject(b);}return TRUE;
    }
    }return DefWindowProcW(window,message,wp,lp);
}
static void OpenSettingsWindow(void)
{
    if(s_window&&IsWindow(s_window)){
        ShowWindow(s_window,SW_RESTORE);PositionSettingsWindow(s_window);SetForegroundWindow(s_window);return;
    }
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.lpszClassName=L"Task Manager.Settings";
    wc.hIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
    POINT origin=PreliminarySettingsOrigin();
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,g_russian?L"Jira Task Manager · Настройки":L"Jira Task Manager · Settings",WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
        origin.x,origin.y,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);
    if(!window)return;PositionSettingsWindow(window);ShowWindow(window,SW_SHOW);SetFocus(s_url);
}
