/* Owned Win32 settings window. Included after the main UI helpers. */
#include "native_refresh_interval.h"
#define WM_ACCOUNT_CHECKED (WM_APP+40)
#define WM_ADVANCED_REPOSITION (WM_APP+43)
#define SETTINGS_WIDTH_DIP 430
#define SETTINGS_HEIGHT_DIP 417 /* Client height, excluding the native caption/frame. */
typedef struct AccountCheck {
    HWND window; wchar_t url[768],token[1201],account[256],error[256]; BOOL ok,replace;
} AccountCheck;
static HWND s_window,s_url,s_token,s_interval,s_note;
static BOOL s_replacing_token;
static void UpdateSettingsConnectionCaption(void);
static const wchar_t *SettingsRefreshGuidance(void){return g_russian?
    L"Рекомендуем 30 минут. Более частое обновление увеличивает нагрузку на сервер Jira. Уменьшайте интервал только при необходимости.":
    L"30 minutes is recommended. More frequent updates increase the load on the Jira server. Shorten the interval only when necessary.";}
static int SettingsTrayCount(void){return (int)SendMessageW(GetDlgItem(s_window,213),TBM_GETPOS,0,0);}
static void UpdateSettingsTrayReadout(void)
{
    wchar_t value[16];swprintf(value,_countof(value),L"%d",SettingsTrayCount());
    SetWindowTextW(GetDlgItem(s_window,263),value);
    SetWindowTextW(GetDlgItem(s_window,213),g_russian?L"Задач в меню трея":L"Tasks in tray menu");
}
static int SettingsRefreshMinutes(void){return NativeRefreshIntervalNormalize((int)SendMessageW(s_interval,TBM_GETPOS,0,0));}
static void UpdateSettingsRefreshReadout(void)
{
    wchar_t value[32];swprintf(value,_countof(value),g_russian?L"%d мин":L"%d min",SettingsRefreshMinutes());
    SetWindowTextW(GetDlgItem(s_window,226),value);
    /* Native MSAA exposes this name while retaining the trackbar's keyboard/value provider. */
    SetWindowTextW(s_interval,g_russian?L"Интервал обновления, мин":L"Update interval, min");
}
static HWND s_pages[4][32];static int s_counts[4],s_tab;
static HWND s_notifications;static int s_notification_scroll;
static HWND s_advanced_window;static BOOL s_advanced;static UINT s_original_avatar_days;
static UINT s_advanced_dpi;static HFONT s_advanced_font,s_advanced_small;
static BOOL s_advanced_reposition_pending;
static HWND SettingsItem(int id){return GetDlgItem((id>=258&&id<=260)||id==262?s_advanced_window:id>=250&&id<=261&&id!=257?s_notifications:s_window,id);}
static void UpdateSettingsCheckboxThemes(void)
{
    const int ids[]={214,224,225,246,250,251,261,270};
    for(int i=0;i<_countof(ids);i++)SetWindowTheme(SettingsItem(ids[i]),g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
}
static void LayoutNotificationPage(void);
static void UpdateExclusionScrollBar(void);
static BOOL s_busy,s_saved,s_connected;
static BOOL s_reload_tasks;
static const wchar_t *SettingsTokenGuidance(void)
{
    if(s_replacing_token)return g_russian?
        L"Введите новый токен. Он заменит текущий после проверки.\r\nСоздать в Jira: Профиль → Personal Access Tokens":
        L"Enter a new token. It replaces the current one after verification.\r\nCreate in Jira: Profile → Personal Access Tokens";
    return g_russian?L"Создать в Jira: Профиль → Personal Access Tokens\r\nСохраняется после проверки · Хранилище Windows":
        L"Create in Jira: Profile → Personal Access Tokens\r\nSaved after verification · Windows Credential Manager";
}
static void UpdateSettingsConnectionCaption(void)
{
    SetWindowTextW(GetDlgItem(s_window,271),g_russian?L"Подключено":L"Connected");
    SetWindowTextW(GetDlgItem(s_window,274),s_replacing_token?(g_russian?L"Отменить замену":L"Cancel replacement"):(g_russian?L"Заменить токен":L"Replace token"));
    SetWindowTextW(GetDlgItem(s_window,218),s_replacing_token?(g_russian?L"Новый токен":L"New token"):L"Personal access token");
    SetWindowTextW(GetDlgItem(s_window,219),SettingsTokenGuidance());
}
static BOOL s_initializing,s_has_account;
static wchar_t s_original_url[768];
static BOOL s_original_startup;
static BOOL s_original_task_alerts,s_original_comment_alerts,s_original_status_alerts,s_original_tab_counts,s_preferences_saved;
static BOOL s_original_all_tab,s_original_done_tab;
static BOOL s_defaults_staged;
static int s_hidden_tab_fallback=-1;
static void RefreshSettingsPreview(void){
    int previous_tab=g_tab;
    if((!g_show_all_tab&&g_tab==0)||(!g_show_done_tab&&g_tab==2))g_tab=1;
    if(previous_tab!=g_tab)RefreshTaskList();
    UpdateTabLabels();
    HWND controls[]={g_tab_all,g_tab_current,g_tab_done,g_tab_assigned,g_tab_reported,g_list};
    for(int i=0;i<_countof(controls);i++)InvalidateRect(controls[i],NULL,FALSE);
    UpdateTooltips();
    UpdateCommentAttention(GetTickCount64());
}
static void ApplySettingsCheckboxPreview(void){
    if(s_defaults_staged){s_preferences_saved=FALSE;return;}
    g_show_tab_task_counts=SendMessageW(SettingsItem(224),BM_GETCHECK,0,0)==BST_CHECKED;
    g_show_all_tab=SendMessageW(SettingsItem(225),BM_GETCHECK,0,0)==BST_CHECKED;
    g_show_done_tab=SendMessageW(SettingsItem(246),BM_GETCHECK,0,0)==BST_CHECKED;
    if((!g_show_all_tab&&g_tab==0)||(!g_show_done_tab&&g_tab==2))s_hidden_tab_fallback=g_tab;
    g_notifications.tasks=SendMessageW(SettingsItem(250),BM_GETCHECK,0,0)==BST_CHECKED;
    g_notifications.comments=SendMessageW(SettingsItem(251),BM_GETCHECK,0,0)==BST_CHECKED;
    g_notifications.statuses=SendMessageW(SettingsItem(261),BM_GETCHECK,0,0)==BST_CHECKED;
    s_preferences_saved=FALSE;RefreshSettingsPreview();
}
static BOOL UpdateSettingsAccountBusy(void){return s_busy;}
static BOOL UpdateSettingsDirty(void){
    if(!s_window||!IsWindow(s_window))return FALSE;wchar_t url[768];
    GetWindowTextW(s_url,url,768);
    wchar_t exclusions[NATIVE_EXCLUDED_EMAILS_CAPACITY],avatar_days[16];GetWindowTextW(SettingsItem(252),exclusions,_countof(exclusions));GetWindowTextW(SettingsItem(259),avatar_days,_countof(avatar_days));
    return (SendMessageW(SettingsItem(270),BM_GETCHECK,0,0)==BST_CHECKED)!=g_pause_when_locked||
        JiraParseInterval(avatar_days)!=(int)s_original_avatar_days||wcscmp(exclusions,g_notifications.excluded)||
        (SendMessageW(SettingsItem(250),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_task_alerts||
        (SendMessageW(SettingsItem(251),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_comment_alerts||
        (SendMessageW(SettingsItem(261),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_status_alerts||
        (SendMessageW(SettingsItem(224),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_tab_counts||
        (SendMessageW(SettingsItem(225),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_all_tab||
        (SendMessageW(SettingsItem(246),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_done_tab||
        wcscmp(url,s_original_url)||GetWindowTextLengthW(s_token)>0||SettingsRefreshMinutes()!=g_refresh_minutes||SettingsTrayCount()!=g_tray_task_count||
        (SendMessageW(GetDlgItem(s_window,214),BM_GETCHECK,0,0)==BST_CHECKED)!=s_original_startup;
}
static HFONT s_font,s_small,s_bold,s_title,s_link_underline;
static int s_about_divider_y;
static HWND s_hover;
static HANDLE s_thread;
static int S(int value){return MulDiv(value,g_dpi,96);}
#include "native_settings_slider.h"
static RECT SettingsOuterRect(void)
{
    RECT bounds={0,0,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP)};
    if(!AdjustWindowRectExForDpi(&bounds,WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,FALSE,WS_EX_DLGMODALFRAME,g_dpi))
        AdjustWindowRectEx(&bounds,WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,FALSE,WS_EX_DLGMODALFRAME);
    return bounds;
}
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
static RECT SettingsWorkAreaFor(HWND owner)
{
    MONITORINFO monitor_info={sizeof(monitor_info)};
    if(!GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor_info))
        SystemParametersInfoW(SPI_GETWORKAREA,0,&monitor_info.rcWork,0);
    return monitor_info.rcWork;
}
static RECT CurrentSettingsWorkArea(void){return SettingsWorkAreaFor(g_window);}
static POINT AlignedSettingsOuterOrigin(RECT settings_outer,RECT settings_frame,POINT desired_frame)
{
    POINT outer={settings_outer.left+desired_frame.x-settings_frame.left,
        settings_outer.top+desired_frame.y-settings_frame.top};
    return outer;
}
static POINT PreliminarySettingsOrigin(void)
{
    RECT owner;if(!VisibleWindowFrame(g_window,&owner))SetRectEmpty(&owner);
    RECT bounds=SettingsOuterRect();SIZE size={bounds.right-bounds.left,bounds.bottom-bounds.top};
    return SettingsWindowOrigin(owner,CurrentSettingsWorkArea(),size);
}
static void PositionOwnedSettingsWindow(HWND settings,HWND owner)
{
    RECT owner_frame,settings_frame,settings_outer;
    if(!VisibleWindowFrame(owner,&owner_frame)||!VisibleWindowFrame(settings,&settings_frame)||!GetWindowRect(settings,&settings_outer))return;
    SIZE frame_size={settings_frame.right-settings_frame.left,settings_frame.bottom-settings_frame.top};
    POINT desired_frame=SettingsWindowOrigin(owner_frame,SettingsWorkAreaFor(owner),frame_size);
    POINT outer=AlignedSettingsOuterOrigin(settings_outer,settings_frame,desired_frame);
    SetWindowPos(settings,NULL,outer.x,outer.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}
static void PositionSettingsWindow(HWND settings){PositionOwnedSettingsWindow(settings,g_window);}
static HWND SettingsControl(const wchar_t *class_name,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id,int page)
{
    HWND control=CreateWindowExW(0,class_name,text,
        WS_CHILD|WS_VISIBLE|style,S(x),S(y),S(w),S(h),page==2?s_notifications:s_window,(HMENU)(INT_PTR)id,g_instance,NULL);
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
    if(m==WM_MOUSEWHEEL&&GetParent(w)==s_notifications&&!(GetWindowLongPtrW(w,GWL_STYLE)&WS_VSCROLL))return SendMessageW(s_notifications,m,wp,lp);
    if((m==WM_SIZE||m==WM_SETTEXT)&&GetDlgCtrlID(w)==252){LRESULT result=DefSubclassProc(w,m,wp,lp);UpdateExclusionScrollBar();return result;}
    if(m==WM_SETFOCUS){LRESULT result=DefSubclassProc(w,m,wp,lp);InvalidateRect(s_window,NULL,FALSE);return result;}
    if(m==WM_KILLFOCUS)InvalidateRect(s_window,NULL,FALSE);
    return DefSubclassProc(w,m,wp,lp);
}
/* Measure wrapped edit lines at full width before deciding whether a bar is needed.
   Guard reentrant WM_SIZE/EN_UPDATE caused by changing the nonclient scrollbar. */
static void UpdateExclusionScrollBar(void)
{
    static BOOL updating;HWND edit=SettingsItem(252);if(updating||!edit)return;updating=TRUE;
    ShowScrollBar(edit,SB_VERT,FALSE);
    RECT text;SendMessageW(edit,EM_GETRECT,0,(LPARAM)&text);
    HDC dc=GetDC(edit);HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(edit,WM_GETFONT,0,0));TEXTMETRICW metrics;
    GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(edit,dc);
    int visible=max(1,(text.bottom-text.top)/max(1,metrics.tmHeight));
    if(SendMessageW(edit,EM_GETLINECOUNT,0,0)>visible)ShowScrollBar(edit,SB_VERT,TRUE);
    updating=FALSE;
}
static LRESULT CALLBACK SettingsButtonProcedure(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(m==WM_MOUSEWHEEL&&GetParent(w)==s_notifications)return SendMessageW(s_notifications,m,wp,lp);
    if(m==WM_MOUSEMOVE&&s_hover!=w){HWND previous=s_hover;s_hover=w;TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,w,0};TrackMouseEvent(&track);if(previous)InvalidateRect(previous,NULL,FALSE);InvalidateRect(w,NULL,FALSE);}
    if(m==WM_MOUSELEAVE){if(s_hover==w)s_hover=NULL;InvalidateRect(w,NULL,FALSE);}
    BOOL link=GetDlgCtrlID(w)==234||GetDlgCtrlID(w)==238||GetDlgCtrlID(w)==274;
    if(link&&m==WM_LBUTTONDOWN){SetPropW(w,L"Tasklet.Settings.MouseFocus",(HANDLE)1);SendMessageW(GetParent(w),WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);}
    if(link&&m==WM_KEYDOWN){RemovePropW(w,L"Tasklet.Settings.MouseFocus");SendMessageW(GetParent(w),WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);}
    if(link&&(m==WM_KILLFOCUS||m==WM_NCDESTROY))RemovePropW(w,L"Tasklet.Settings.MouseFocus");
    if(m==WM_SETCURSOR){SetCursor(LoadCursorW(NULL,IDC_HAND));return TRUE;}
    return DefSubclassProc(w,m,wp,lp);
}
static int A(int value){return MulDiv(value,s_advanced_dpi,96);}
static void LayoutAdvancedSettings(void)
{
    if(!s_advanced_window)return;
    HFONT font=CreateFontW(-A(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    HFONT small=CreateFontW(-A(10),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    if(!font||!small){if(font)DeleteObject(font);if(small)DeleteObject(small);return;}
    struct {int id,x,y,w,h;BOOL small;} fields[]={{258,20,18,242,24,FALSE},{259,276,18,54,24,FALSE},{260,20,54,310,38,TRUE},{262,20,108,310,34,TRUE}};
    for(int i=0;i<_countof(fields);i++){
        HWND control=GetDlgItem(s_advanced_window,fields[i].id);
        SendMessageW(control,WM_SETFONT,(WPARAM)(fields[i].small?small:font),TRUE);
        SetWindowPos(control,NULL,A(fields[i].x),A(fields[i].y),A(fields[i].w),A(fields[i].h),SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(s_advanced_font)DeleteObject(s_advanced_font);if(s_advanced_small)DeleteObject(s_advanced_small);s_advanced_font=font;s_advanced_small=small;
    /* Use the actual native frame, which can differ from the owner's DPI. */
    RECT outer,client;GetWindowRect(s_advanced_window,&outer);GetClientRect(s_advanced_window,&client);
    SetWindowPos(s_advanced_window,NULL,0,0,A(350)+(outer.right-outer.left-client.right),A(152)+(outer.bottom-outer.top-client.bottom),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
}
static void UpdateAdvancedSettingsTheme(void)
{
    if(!s_advanced_window)return;
    ApplyWindowCaptionTheme(s_advanced_window);SetWindowTheme(SettingsItem(259),g_dark?L"DarkMode_CFD":L"Explorer",NULL);
    RedrawWindow(s_advanced_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static LRESULT CALLBACK AdvancedSettingsProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message){
    case WM_CREATE:{
        s_advanced_window=window;s_advanced_dpi=g_dpi;
        struct {const wchar_t *type,*text;DWORD style;int x,y,w,h,id;BOOL small;} controls[]={
            {L"STATIC",g_russian?L"Кэш аватаров, дней":L"Avatar cache, days",SS_CENTERIMAGE,20,18,242,24,258,FALSE},
            {L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,276,18,54,24,259,FALSE},
            {L"STATIC",g_russian?L"Аватары сохраняются локально. После срока обновляются при открытии комментариев.":L"Avatars are saved locally and refreshed after expiry when comments are opened.",0,20,54,310,38,260,TRUE},
            {L"STATIC",g_russian?L"Изменения сохраняются кнопкой «Сохранить» в основных настройках.":L"Use Save in the main settings window to keep these changes.",0,20,108,310,34,262,TRUE}};
        for(int i=0;i<_countof(controls);i++){
            HWND child=CreateWindowExW(0,controls[i].type,controls[i].text,WS_CHILD|WS_VISIBLE|controls[i].style,
                S(controls[i].x),S(controls[i].y),S(controls[i].w),S(controls[i].h),window,(HMENU)(INT_PTR)controls[i].id,g_instance,NULL);
            if(!child)return -1;
            SendMessageW(child,WM_SETFONT,(WPARAM)(controls[i].small?s_small:s_font),TRUE);
        }
        wchar_t days[16];swprintf(days,16,L"%u",s_original_avatar_days);SetWindowTextW(GetDlgItem(window,259),days);
        SendMessageW(GetDlgItem(window,259),EM_SETLIMITTEXT,3,0);
        SetWindowTheme(GetDlgItem(window,259),g_dark?L"DarkMode_CFD":L"Explorer",NULL);
        SetWindowSubclass(GetDlgItem(window,259),SettingsEditProcedure,1,0);
        ApplyWindowCaptionTheme(window);return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL){SendMessageW(window,WM_CLOSE,0,0);return 0;}
        if(LOWORD(wp)==259&&HIWORD(wp)==EN_CHANGE&&!s_initializing){
            SetWindowTextW(GetDlgItem(window,260),g_russian?L"Аватары сохраняются локально. После срока обновляются при открытии комментариев.":L"Avatars are saved locally and refreshed after expiry when comments are opened.");
        }return 0;
    case WM_CLOSE:
        s_advanced=FALSE;ShowWindow(window,SW_HIDE);
        if(IsWindowVisible(s_window))SetForegroundWindow(s_window);return 0;
    case WM_DPICHANGED:{
        s_advanced_dpi=HIWORD(wp);RECT *suggested=(RECT*)lp;
        SetWindowPos(window,NULL,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutAdvancedSettings();
        /* Placement chosen before the move may no longer fit after scaling. Defer
           until SetWindowPos/WM_DPICHANGED unwind; coalesce recursive requests. */
        if(s_advanced&&!s_advanced_reposition_pending)s_advanced_reposition_pending=PostMessageW(window,WM_ADVANCED_REPOSITION,0,0);
        return 0;
    }
    case WM_ADVANCED_REPOSITION:
        s_advanced_reposition_pending=FALSE;if(s_advanced&&s_window)PositionOwnedSettingsWindow(window,s_window);return 0;
    case WM_DESTROY:
        s_advanced_window=NULL;s_advanced=FALSE;s_advanced_reposition_pending=FALSE;if(s_advanced_font)DeleteObject(s_advanced_font);if(s_advanced_small)DeleteObject(s_advanced_small);
        s_advanced_font=s_advanced_small=NULL;return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:return SendMessageW(s_window,message,wp,lp);
    case WM_ERASEBKGND:{RECT client;GetClientRect(window,&client);FillRect((HDC)wp,&client,g_window_brush);return 1;}
    }return DefWindowProcW(window,message,wp,lp);
}
static BOOL CreateAdvancedSettingsWindow(void)
{
    WNDCLASSW cls={.lpfnWndProc=AdvancedSettingsProcedure,.hInstance=g_instance,.lpszClassName=L"Task Manager.AdvancedSettings",.hCursor=LoadCursorW(NULL,IDC_ARROW)};
    cls.hIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1));RegisterClassW(&cls);
    RECT bounds={0,0,S(350),S(152)};DWORD style=WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;
    if(!AdjustWindowRectExForDpi(&bounds,style,FALSE,WS_EX_DLGMODALFRAME,g_dpi))AdjustWindowRectEx(&bounds,style,FALSE,WS_EX_DLGMODALFRAME);
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,g_russian?APP_TITLE L" · Расширенные настройки":APP_TITLE L" · Advanced settings",
        style,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,s_window,NULL,g_instance,NULL);
    if(!window)return FALSE;LayoutAdvancedSettings();return TRUE;
}
static void OpenAdvancedSettingsWindow(void)
{
    if(!s_advanced_window)return;
    s_advanced=TRUE;PositionOwnedSettingsWindow(s_advanced_window,s_window);
    ShowWindow(s_advanced_window,IsWindowVisible(s_window)?SW_SHOWNORMAL:SW_SHOWNOACTIVATE);
    if(IsWindowVisible(s_window)){SetForegroundWindow(s_advanced_window);SetFocus(SettingsItem(259));}
}
static void PlaceSettings(int id,int x,int y,int w,int h)
{
    HWND control=SettingsItem(id);if(control)SetWindowPos(control,NULL,S(x),S(y),S(w),S(h),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
}
static void LayoutNotificationPage(void)
{
    if(!s_notifications)return;RECT client;GetClientRect(s_notifications,&client);
    int content=S(200),maximum=max(0,content-client.bottom);s_notification_scroll=max(0,min(maximum,s_notification_scroll));
    SCROLLINFO info={sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,content-1,(UINT)client.bottom,s_notification_scroll,0};SetScrollInfo(s_notifications,SB_VERT,&info,TRUE);
    GetClientRect(s_notifications,&client);
    int offset=MulDiv(s_notification_scroll,96,g_dpi),width=MulDiv(client.right,96,g_dpi)-24;
    PlaceSettings(250,12,8-offset,width,26);PlaceSettings(251,12,36-offset,width,26);PlaceSettings(261,12,64-offset,width,26);
    PlaceSettings(256,12,104-offset,width,18);PlaceSettings(252,12,126-offset,width,32);
    PlaceSettings(255,12,164-offset,width,36);
    UpdateExclusionScrollBar();
    RedrawWindow(s_notifications,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static LRESULT CALLBACK NotificationPageProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_VSCROLL){
        SCROLLINFO info={sizeof(info),SIF_ALL};GetScrollInfo(window,SB_VERT,&info);
        int position=s_notification_scroll;
        switch(LOWORD(wp)){
        case SB_LINEUP:position-=S(24);break;case SB_LINEDOWN:position+=S(24);break;
        case SB_PAGEUP:position-=(int)info.nPage;break;case SB_PAGEDOWN:position+=(int)info.nPage;break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:position=info.nTrackPos;break;
        case SB_TOP:position=0;break;case SB_BOTTOM:position=info.nMax;break;
        }
        s_notification_scroll=position;LayoutNotificationPage();return 0;
    }
    if(message==WM_MOUSEWHEEL){s_notification_scroll-=MulDiv(GET_WHEEL_DELTA_WPARAM(wp),S(48),WHEEL_DELTA);LayoutNotificationPage();return 0;}
    if(message==WM_SIZE){LayoutNotificationPage();return 0;}
    if(message==WM_COMMAND||message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_DRAWITEM)
        return SendMessageW(s_window,message,wp,lp);
    if(message==WM_ERASEBKGND){RECT client;GetClientRect(window,&client);FillRect((HDC)wp,&client,g_window_brush);return 1;}
    return DefWindowProcW(window,message,wp,lp);
}
static void LayoutSettingsTabs(void)
{
    if(!s_window||!s_font)return;
    HDC dc=GetDC(s_window);HGDIOBJ old=SelectObject(dc,s_font);int x=S(14);
    const int tabs[]={200,203,202,201};for(int i=0;i<4;i++){int id=tabs[i];
        wchar_t text[80];SIZE size;GetWindowTextW(GetDlgItem(s_window,id),text,80);
        GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size);
        int width=size.cx+S(24);SetWindowPos(GetDlgItem(s_window,id),NULL,x,S(14),width,S(30),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);x+=width;
    }
    SelectObject(dc,old);ReleaseDC(s_window,dc);
}
static int SettingsTextWidthDip(int id,HFONT font)
{
    wchar_t text[256];GetWindowTextW(SettingsItem(id),text,_countof(text));HDC dc=GetDC(s_window);HGDIOBJ old=SelectObject(dc,font);SIZE size={0};
    GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size);SelectObject(dc,old);ReleaseDC(s_window,dc);
    return (size.cx*96+g_dpi-1)/g_dpi;
}
static void LayoutAboutSettings(int width,int height)
{
    int field_width=width-52,title_width=SettingsTextWidthDip(230,s_title);
    wchar_t status[256];GetWindowTextW(SettingsItem(235),status,_countof(status));
    HDC dc=GetDC(s_window);HGDIOBJ old=SelectObject(dc,s_small);RECT measured={0,0,S(field_width),0};
    TEXTMETRICW metrics={0};GetTextMetricsW(dc,&metrics);DrawTextW(dc,status,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(s_window,dc);
    int status_height=max(14,(measured.bottom*96+g_dpi-1)/g_dpi),base_action_y=152+status_height+10;
    int shift=min(0,height-(base_action_y+180)),action_y=base_action_y+shift;
    PlaceSettings(230,26,60+shift,title_width,28);PlaceSettings(268,26+title_width+8,66+shift,field_width-title_width-8,20);
    PlaceSettings(231,26,94+shift,field_width,22);PlaceSettings(232,26,130+shift,field_width,20);
    PlaceSettings(235,26,152+shift,field_width,status_height);
    PlaceSettings(242,26,action_y,min(field_width,SettingsTextWidthDip(242,s_font)+24),28);
    s_about_divider_y=S(action_y+28+24);
    PlaceSettings(236,26,action_y+28+40,field_width,max(18,(metrics.tmHeight*96+g_dpi-1)/g_dpi));
    int contact_width=SettingsTextWidthDip(238,s_font)+4,diagnostics_width=SettingsTextWidthDip(234,s_font)+4;
    PlaceSettings(238,26,action_y+28+64,contact_width,28);
    PlaceSettings(234,26+contact_width+18,action_y+28+64,diagnostics_width,28);
}
static void LayoutSettings(void)
{
    RECT rc;GetClientRect(s_window,&rc);int width=MulDiv(rc.right,96,g_dpi),height=MulDiv(rc.bottom,96,g_dpi);
    int field_width=width-52,preferences=s_connected?190:188;
    PlaceSettings(210,32,81,field_width-12,18);
    int action_width=SettingsTextWidthDip(274,s_font)+4;
    PlaceSettings(218,26,110,field_width-action_width-12,18);PlaceSettings(211,32,134,field_width-12,18);
    PlaceSettings(271,34,158,field_width-action_width-24,20);
    PlaceSettings(274,width-34-action_width,s_replacing_token?110:154,action_width,s_replacing_token?18:26);
    PlaceSettings(219,26,156,field_width,30);
    PlaceSettings(216,34,121,field_width-90,15);PlaceSettings(217,34,137,field_width-90,18);
    PlaceSettings(215,width-26-62,121,54,28);
    PlaceSettings(221,26,preferences,field_width-65,20);PlaceSettings(226,width-86,preferences,60,20);
    PlaceSettings(212,26,preferences+24,field_width,24);
    RECT channel;SendMessageW(s_interval,TBM_GETCHANNELRECT,0,(LPARAM)&channel);
    for(int i=0;i<3;i++){
        int center=S(26)+channel.left+MulDiv(channel.right-channel.left,i,2);
        SetWindowPos(GetDlgItem(s_window,227+i),NULL,center-S(14),S(preferences+49),S(28),S(14),SWP_NOZORDER|SWP_NOACTIVATE);
    }
    HDC hint_dc=GetDC(s_window);HGDIOBJ hint_font=SelectObject(hint_dc,s_small);
    RECT hint={0,0,S(field_width),0};
    DrawTextW(hint_dc,SettingsRefreshGuidance(),-1,&hint,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(hint_dc,hint_font);ReleaseDC(s_window,hint_dc);
    PlaceSettings(269,26,preferences+72,field_width,MulDiv(hint.bottom,96,g_dpi)+2);
    PlaceSettings(270,26,preferences+84+MulDiv(hint.bottom,96,g_dpi),field_width,26);
    PlaceSettings(247,26,60,field_width,16);PlaceSettings(214,26,80,field_width,22);
    PlaceSettings(248,26,120,field_width,16);PlaceSettings(224,26,140,field_width,22);
    PlaceSettings(225,26,164,field_width,22);PlaceSettings(246,26,188,field_width,22);
    PlaceSettings(222,26,232,field_width-65,20);PlaceSettings(263,width-86,232,60,20);
    PlaceSettings(213,26,256,field_width,24);SendMessageW(GetDlgItem(s_window,213),TBM_GETCHANNELRECT,0,(LPARAM)&channel);
    const int positions[]={1,15,30};for(int i=0;i<3;i++){
        int center=S(26)+channel.left+MulDiv(channel.right-channel.left,positions[i]-1,29);
        SetWindowPos(GetDlgItem(s_window,264+i),NULL,center-S(14),S(281),S(28),S(14),SWP_NOZORDER|SWP_NOACTIVATE);
    }
    PlaceSettings(257,26,302,field_width,26);
    PlaceSettings(223,26,height-86,field_width,32);
    PlaceSettings(267,14,height-42,118,28);PlaceSettings(IDCANCEL,width-162,height-42,66,28);PlaceSettings(IDOK,width-90,height-42,76,28);
    if(s_notifications)SetWindowPos(s_notifications,NULL,S(20),S(52),S(width-40),S(max(40,height-146)),SWP_NOZORDER|SWP_NOACTIVATE);
    LayoutNotificationPage();
    LayoutAboutSettings(width,height);
    ShowWindow(SettingsItem(267),s_tab==1?SW_HIDE:SW_SHOW);
    BOOL account=s_tab==0&&s_connected&&!s_replacing_token;
    ShowWindow(SettingsItem(271),account?SW_SHOW:SW_HIDE);
    ShowWindow(SettingsItem(274),s_tab==0&&s_connected?SW_SHOW:SW_HIDE);
    int connected_ids[]={215,216,217};for(int i=0;i<3;i++)ShowWindow(GetDlgItem(s_window,connected_ids[i]),account?SW_SHOW:SW_HIDE);
    int token_ids[]={211,218,219};for(int i=0;i<3;i++)ShowWindow(GetDlgItem(s_window,token_ids[i]),s_tab==0&&(!s_connected||s_replacing_token)?SW_SHOW:SW_HIDE);
    LayoutSettingsTabs();
    RedrawWindow(s_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void RefreshSettingsAccountInfo(void)
{
    if(!s_window||!IsWindow(s_window)||s_busy||!s_connected)return;
    if(g_snapshot.account[0])SetWindowTextW(SettingsItem(217),g_snapshot.account);
    UpdateSettingsConnectionCaption();LayoutSettings();
}
static void UpdateSettingsLanguage(void)
{
    if(!s_window||!IsWindow(s_window))return;
    SetWindowTextW(s_window,g_russian?APP_TITLE L" · Настройки":APP_TITLE L" · Settings");
    SetWindowTextW(GetDlgItem(s_window,200),L"Jira");
    SetWindowTextW(GetDlgItem(s_window,201),g_russian?L"О приложении":L"About");
    SetWindowTextW(GetDlgItem(s_window,202),g_russian?L"Уведомления":L"Notifications");
    SetWindowTextW(GetDlgItem(s_window,203),g_russian?L"Общие":L"General");
    SetWindowTextW(SettingsItem(250),g_russian?L"Уведомлять о новых задачах":L"Notify about new tasks");
    SetWindowTextW(SettingsItem(251),g_russian?L"Уведомлять о новых комментариях":L"Notify about new comments");
    SetWindowTextW(SettingsItem(261),g_russian?L"Уведомлять об изменении статуса менеджером":L"Notify about status changes by a manager");
    SetWindowTextW(SettingsItem(256),g_russian?L"Скрывать комментарии от":L"Hide comments from");
    SetWindowTextW(SettingsItem(255),g_russian?L"Почты через запятую. Комментарии скрыты, без уведомлений. Работает, если Jira передаёт почту автора.":L"Comma-separated emails. Comments are hidden, without alerts. Requires author email from Jira.");
    SetWindowTextW(SettingsItem(257),g_russian?L"Расширенные →":L"Advanced →");
    if(s_advanced_window)SetWindowTextW(s_advanced_window,g_russian?APP_TITLE L" · Расширенные настройки":APP_TITLE L" · Advanced settings");
    SetWindowTextW(SettingsItem(258),g_russian?L"Кэш аватаров, дней":L"Avatar cache, days");
    SetWindowTextW(SettingsItem(260),g_russian?L"Аватары сохраняются локально. После срока обновляются при открытии комментариев.":L"Avatars are saved locally and refreshed after expiry when comments are opened.");
    SetWindowTextW(SettingsItem(262),g_russian?L"Изменения сохраняются кнопкой «Сохранить» в основных настройках.":L"Use Save in the main settings window to keep these changes.");
    SetWindowTextW(GetDlgItem(s_window,216),g_russian?L"Аккаунт Jira":L"Jira account");
    if(!g_snapshot.account[0])SetWindowTextW(GetDlgItem(s_window,217),g_russian?L"Подключён":L"Connected");
    SetWindowTextW(GetDlgItem(s_window,215),g_russian?L"Выйти":L"Sign out");
    SetWindowTextW(GetDlgItem(s_window,219),g_russian?L"Создать в Jira: Профиль → Personal Access Tokens\r\nСохраняется после проверки · Хранилище Windows":L"Create in Jira: Profile → Personal Access Tokens\r\nSaved after verification · Windows Credential Manager");
    SetWindowTextW(GetDlgItem(s_window,221),g_russian?L"Интервал обновления, мин":L"Update interval, min");
    SetWindowTextW(GetDlgItem(s_window,269),SettingsRefreshGuidance());
    UpdateSettingsConnectionCaption();
    SetWindowTextW(GetDlgItem(s_window,270),g_russian?L"Приостанавливать обновления при блокировке компьютера":L"Pause updates when the computer is locked");
    UpdateSettingsRefreshReadout();
    SetWindowTextW(GetDlgItem(s_window,222),g_russian?L"Задач в меню трея":L"Tasks in tray menu");
    UpdateSettingsTrayReadout();
    SetWindowTextW(GetDlgItem(s_window,247),g_russian?L"Запуск":L"Startup");
    SetWindowTextW(GetDlgItem(s_window,248),g_russian?L"Список задач":L"Task list");
    SetWindowTextW(GetDlgItem(s_window,214),g_russian?L"Запускать вместе с Windows":L"Start with Windows");
    SetWindowTextW(SettingsItem(224),g_russian?L"Показывать количество задач во вкладках":L"Show task totals in tabs");
    SetWindowTextW(SettingsItem(225),g_russian?L"Показывать вкладку «Все»":L"Show the All tab");
    SetWindowTextW(SettingsItem(246),g_russian?L"Показывать вкладку «Готово»":L"Show the Done tab");
    SetWindowTextW(GetDlgItem(s_window,231),g_russian?L"Приложение для обработки статусов задач Jira.":L"A Jira task status management app.");
    SetWindowTextW(GetDlgItem(s_window,232),g_russian?L"Обновления":L"Updates");
    UpdateUiRender(s_window);
    SetWindowTextW(GetDlgItem(s_window,234),g_russian?L"Диагностика ↗":L"Diagnostics ↗");
    SetWindowTextW(GetDlgItem(s_window,236),g_russian?L"Помощь и обратная связь":L"Help and feedback");
    SetWindowTextW(GetDlgItem(s_window,238),g_russian?L"Сообщить о проблеме ↗":L"Report a problem ↗");
    SetWindowTextW(SettingsItem(267),g_russian?L"По умолчанию":L"Defaults");
    SetWindowTextW(GetDlgItem(s_window,IDCANCEL),g_russian?L"Отмена":L"Cancel");
    SetWindowTextW(GetDlgItem(s_window,IDOK),g_russian?L"Сохранить":L"Save");
    LayoutSettings();
}
static void SettingsTab(int page)
{
    if(page<0||page>=4)return;
    s_tab=page;for(int p=0;p<4;p++)for(int i=0;i<s_counts[p];i++)ShowWindow(s_pages[p][i],p==page?SW_SHOW:SW_HIDE);
    ShowWindow(s_notifications,page==2?SW_SHOW:SW_HIDE);
    for(int id=200;id<204;id++)InvalidateRect(GetDlgItem(s_window,id),NULL,TRUE);
    LayoutSettings();
}
static void ResetSettingsDraft(void)
{
    SendMessageW(SettingsItem(270),BM_SETCHECK,BST_CHECKED,0);
    BOOL initializing=s_initializing;s_initializing=TRUE;
    SendMessageW(s_interval,TBM_SETPOS,TRUE,30);SendMessageW(SettingsItem(213),TBM_SETPOS,TRUE,20);
    UpdateSettingsRefreshReadout();UpdateSettingsTrayReadout();
    const int enabled[]={224,246,250,251,261},disabled[]={214,225};
    for(int i=0;i<_countof(enabled);i++)SendMessageW(SettingsItem(enabled[i]),BM_SETCHECK,BST_CHECKED,0);
    for(int i=0;i<_countof(disabled);i++)SendMessageW(SettingsItem(disabled[i]),BM_SETCHECK,BST_UNCHECKED,0);
    SetWindowTextW(SettingsItem(252),NATIVE_EXCLUDED_EMAILS_DEFAULT);SetWindowTextW(SettingsItem(259),L"7");
    s_initializing=initializing;s_preferences_saved=FALSE;s_defaults_staged=TRUE;
    UpdateExclusionScrollBar();UpdateSettingsLanguage();
    SetWindowTextW(s_note,g_russian?L"Значения по умолчанию. Нажмите «Сохранить» или «Отмена».":L"Default values staged. Choose Save to apply or Cancel to discard.");
    InvalidateRect(s_window,NULL,FALSE);InvalidateRect(s_notifications,NULL,FALSE);
}
static BOOL SaveNativePreferences(void)
{
    wchar_t interval[16],tray[16],avatar_text[16];GetWindowTextW(SettingsItem(259),avatar_text,16);
    int minutes=SettingsRefreshMinutes(),count=SettingsTrayCount();swprintf(interval,_countof(interval),L"%d",minutes);swprintf(tray,_countof(tray),L"%d",count);
    int avatar_days=JiraParseInterval(avatar_text);
    if(avatar_days<1||avatar_days>365){OpenAdvancedSettingsWindow();SetWindowTextW(SettingsItem(260),g_russian?L"Срок кэша: целое число от 1 до 365 дней.":L"Cache lifetime must be 1–365 whole days.");SetFocus(SettingsItem(259));return FALSE;}
    NativeNotificationPreferences preferences={0};wchar_t exclusions[NATIVE_EXCLUDED_EMAILS_CAPACITY];
    GetWindowTextW(SettingsItem(252),exclusions,_countof(exclusions));
    if(!NativeNormalizeExcludedEmails(exclusions,preferences.excluded,_countof(preferences.excluded))){
        SettingsTab(2);SetWindowTextW(SettingsItem(255),g_russian?L"Проверьте адреса почты. Разделяйте их запятыми.":L"Check email addresses. Separate them with commas.");SetFocus(SettingsItem(252));return FALSE;
    }
    preferences.tasks=SendMessageW(SettingsItem(250),BM_GETCHECK,0,0)==BST_CHECKED;
    preferences.comments=SendMessageW(SettingsItem(251),BM_GETCHECK,0,0)==BST_CHECKED;
    preferences.statuses=SendMessageW(SettingsItem(261),BM_GETCHECK,0,0)==BST_CHECKED;
    if(count<1||count>30){SettingsTab(3);SetWindowTextW(s_note,g_russian?L"Задач в трее: 1–30.":L"Tray tasks: 1–30.");SetFocus(GetDlgItem(s_window,213));return FALSE;}
    BOOL startup=SendMessageW(GetDlgItem(s_window,214),BM_GETCHECK,0,0)==BST_CHECKED;
    if(startup!=NativeStartupEnabled()&&!SetNativeStartup(startup)){SettingsTab(3);SetWindowTextW(s_note,g_russian?L"Не удалось сохранить автозапуск Native.":L"Could not save Native startup preference.");return FALSE;}
    if(!WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",interval,g_settings_path)||!WritePrivateProfileStringW(L"Jira",L"TrayTaskCount",tray,g_settings_path)||
        !NativeRememberRemovedExclusions(g_settings_path,g_notifications.excluded,preferences.excluded)||!NativeNotificationsSave(g_settings_path,&preferences)||!NativeAvatarCacheSaveDays(g_settings_path,(UINT)avatar_days)||
        !NativeAutoRefreshPauseSave(g_settings_path,SendMessageW(SettingsItem(270),BM_GETCHECK,0,0)==BST_CHECKED)||
        !NativeClearLegacyTokenLabels(g_settings_path)||
        !WritePrivateProfileStringW(L"Interface",L"ShowTabTaskCounts",SendMessageW(SettingsItem(224),BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",g_settings_path)||
        !WritePrivateProfileStringW(L"Interface",L"ShowAllTab",SendMessageW(SettingsItem(225),BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",g_settings_path)||
        !WritePrivateProfileStringW(L"Interface",L"ShowDoneTab",SendMessageW(SettingsItem(246),BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0",g_settings_path)){
        SettingsTab(0);SetWindowTextW(s_note,g_russian?L"Не удалось сохранить настройки. Проверьте доступ к папке.":L"Could not save settings. Check folder access.");return FALSE;
    }
    if(wcscmp(g_notifications.excluded,preferences.excluded)&&g_image_viewer&&IsWindow(g_image_viewer))DestroyWindow(g_image_viewer);
    if(wcscmp(g_notifications.excluded,preferences.excluded)){g_snapshot.refresh_cache=FALSE;s_reload_tasks=TRUE;}
    g_notifications=preferences;s_original_avatar_days=(UINT)avatar_days;
    g_show_tab_task_counts=SendMessageW(SettingsItem(224),BM_GETCHECK,0,0)==BST_CHECKED;
    g_show_all_tab=SendMessageW(SettingsItem(225),BM_GETCHECK,0,0)==BST_CHECKED;
    g_show_done_tab=SendMessageW(SettingsItem(246),BM_GETCHECK,0,0)==BST_CHECKED;
    s_original_all_tab=g_show_all_tab;s_original_done_tab=g_show_done_tab;s_hidden_tab_fallback=-1;
    s_original_task_alerts=g_notifications.tasks;s_original_comment_alerts=g_notifications.comments;s_original_status_alerts=g_notifications.statuses;s_original_tab_counts=g_show_tab_task_counts;s_preferences_saved=TRUE;
    for(int i=0;i<g_snapshot.count;i++)if(NativeEmailExcluded(preferences.excluded,g_snapshot.items[i].latest_comment_email)){
        JiraItem *source=&g_snapshot.items[i];int index=TaskByKey(source->key);
        source->latest_comment[0]=source->latest_comment_id[0]=0;
        if(index>=0){g_tasks[index].unread_comment=FALSE;g_tasks[index].comment_pulse_started=0;}
    }
    TaskPopover *previous=g_active_popover;
    if(g_popover.window){JiraFilterComments(&g_popover.comments,preferences.excluded);UpdateCommentScroll();InvalidateRect(g_popover.text,NULL,TRUE);}
    for(TaskPopover *panel=g_detached_popovers;panel;panel=panel->next)if(panel->kind==1){g_active_popover=panel;JiraFilterComments(&panel->comments,preferences.excluded);UpdateCommentScroll();InvalidateRect(panel->text,NULL,TRUE);}
    g_active_popover=previous;
    BOOL pause=SendMessageW(SettingsItem(270),BM_GETCHECK,0,0)==BST_CHECKED;
    BOOL schedule_changed=g_pause_when_locked!=pause||g_refresh_minutes!=minutes;
    g_pause_when_locked=pause;g_refresh_minutes=minutes;g_tray_task_count=count;
    if(schedule_changed){RefreshPresenceChanged();g_auto_was_allowed=NativeAutoRefreshAllowed(g_pause_when_locked,&g_refresh_presence);ResetAutomaticRefresh(FALSE);}
    RefreshTaskList();UpdateTooltips();return TRUE;
}
static void FreezeSettings(BOOL freeze)
{
    int ids[]={IDOK,210,211,212,213,214,215,224,225,246,267,270,274};for(int i=0;i<_countof(ids);i++)EnableWindow(GetDlgItem(s_window,ids[i]),!freeze);
    for(int id=250;id<=252;id++)EnableWindow(SettingsItem(id),!freeze);
    EnableWindow(SettingsItem(261),!freeze);
    EnableWindow(SettingsItem(257),!freeze);EnableWindow(SettingsItem(259),!freeze);
    UpdateSettingsConnectionCaption();
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
    wchar_t previous[768];
    int count=SettingsTrayCount();
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
    s_replacing_token=FALSE;UpdateSettingsConnectionCaption();
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
        s_window=window;s_url=NULL;s_initializing=TRUE;memset(s_counts,0,sizeof(s_counts));s_busy=s_saved=s_replacing_token=s_reload_tasks=FALSE;s_hover=NULL;s_has_account=s_connected=JiraHasAccount();
        s_original_task_alerts=g_notifications.tasks;s_original_comment_alerts=g_notifications.comments;s_original_status_alerts=g_notifications.statuses;s_original_tab_counts=g_show_tab_task_counts;s_preferences_saved=FALSE;
        s_original_all_tab=g_show_all_tab;s_original_done_tab=g_show_done_tab;s_hidden_tab_fallback=-1;s_defaults_staged=FALSE;
        s_font=CreateFontW(-S(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        s_small=CreateFontW(-S(10),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        s_bold=CreateFontW(-S(12),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Semibold");
        s_title=CreateFontW(-S(18),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        LOGFONTW link_font={0};GetObjectW(s_font,sizeof(link_font),&link_font);link_font.lfUnderline=TRUE;s_link_underline=CreateFontIndirectW(&link_font);
        HDC measure=GetDC(window);HGDIOBJ old=SelectObject(measure,s_font);int x=14;
        const wchar_t *tabs[]={L"Jira",g_russian?L"О приложении":L"About",g_russian?L"Уведомления":L"Notifications",g_russian?L"Общие":L"General"};
        const int tab_order[]={0,3,2,1};for(int p=0;p<4;p++){int i=tab_order[p];SIZE size;GetTextExtentPoint32W(measure,tabs[i],(int)wcslen(tabs[i]),&size);int width=MulDiv(size.cx,96,g_dpi)+24;SettingsControl(L"BUTTON",tabs[i],WS_TABSTOP|BS_OWNERDRAW,x,14,width,30,200+i,-1);x+=width;}
        SelectObject(measure,old);ReleaseDC(window,measure);
        WNDCLASSW page_class={0};page_class.lpfnWndProc=NotificationPageProcedure;page_class.hInstance=g_instance;page_class.hCursor=LoadCursorW(NULL,IDC_ARROW);page_class.lpszClassName=L"Task Manager.NotificationSettings";RegisterClassW(&page_class);
        s_notification_scroll=0;s_advanced=FALSE;s_original_avatar_days=NativeAvatarCacheDays(g_settings_path);if(!CreateAdvancedSettingsWindow())return -1;
        s_notifications=CreateWindowExW(WS_EX_CONTROLPARENT,page_class.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN|WS_VSCROLL,S(20),S(52),S(390),S(248),window,NULL,g_instance,NULL);
        SettingsControl(L"BUTTON",g_russian?L"Уведомлять о новых задачах":L"Notify about new tasks",WS_TABSTOP|BS_AUTOCHECKBOX,12,90,340,26,250,2);
        SettingsControl(L"BUTTON",g_russian?L"Уведомлять о новых комментариях":L"Notify about new comments",WS_TABSTOP|BS_AUTOCHECKBOX,12,126,340,26,251,2);
        SettingsControl(L"BUTTON",g_russian?L"Уведомлять об изменении статуса менеджером":L"Notify about status changes by a manager",WS_TABSTOP|BS_AUTOCHECKBOX,12,64,340,26,261,2);
        SendMessageW(SettingsItem(250),BM_SETCHECK,g_notifications.tasks?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(SettingsItem(251),BM_SETCHECK,g_notifications.comments?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(SettingsItem(261),BM_SETCHECK,g_notifications.statuses?BST_CHECKED:BST_UNCHECKED,0);
        SettingsControl(L"STATIC",g_russian?L"Скрывать комментарии от":L"Hide comments from",0,12,172,340,22,256,2);
        SettingsControl(L"EDIT",g_notifications.excluded,WS_TABSTOP|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,12,202,340,70,252,2);
        SendMessageW(SettingsItem(252),EM_SETLIMITTEXT,NATIVE_EXCLUDED_EMAILS_CAPACITY-1,0);
        SettingsControl(L"STATIC",g_russian?L"Почты через запятую. Комментарии скрыты, без уведомлений. Работает, если Jira передаёт почту автора.":L"Comma-separated emails. Comments are hidden, without alerts. Requires author email from Jira.",0,12,286,340,56,255,2);
        SendMessageW(SettingsItem(255),WM_SETFONT,(WPARAM)s_small,TRUE);
        for(int id=250;id<=252;id++)SetWindowSubclass(SettingsItem(id),id==252?SettingsEditProcedure:SettingsButtonProcedure,1,0);
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
        INITCOMMONCONTROLSEX trackbar={sizeof(trackbar),ICC_BAR_CLASSES};InitCommonControlsEx(&trackbar);
        s_interval=SettingsControl(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,26,190,224,24,212,0);
        SendMessageW(s_interval,TBM_SETRANGE,TRUE,MAKELPARAM(15,45));SendMessageW(s_interval,TBM_SETLINESIZE,0,5);SendMessageW(s_interval,TBM_SETPAGESIZE,0,5);
        SendMessageW(s_interval,TBM_SETPOS,TRUE,NativeRefreshIntervalNormalize(g_refresh_minutes));
        SetWindowSubclass(s_interval,SettingsSliderProcedure,1,0);
        SettingsControl(L"STATIC",L"",SS_CENTERIMAGE|SS_RIGHT,258,190,65,24,226,0);
        SettingsControl(L"STATIC",L"15",SS_CENTER,26,213,28,14,227,0);
        SettingsControl(L"STATIC",L"30",SS_CENTER,124,213,28,14,228,0);
        SettingsControl(L"STATIC",L"45",SS_CENTER,222,213,28,14,229,0);
        SettingsControl(L"STATIC",SettingsRefreshGuidance(),0,26,242,378,40,269,0);
        SettingsControl(L"STATIC",L"",0,34,158,100,20,271,0);
        SettingsControl(L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,270,154,130,26,274,0);UpdateSettingsConnectionCaption();
        SettingsControl(L"BUTTON",g_russian?L"Приостанавливать обновления при блокировке компьютера":L"Pause updates when the computer is locked",WS_TABSTOP|BS_AUTOCHECKBOX,26,300,378,26,270,0);
        SendMessageW(SettingsItem(270),BM_SETCHECK,g_pause_when_locked?BST_CHECKED:BST_UNCHECKED,0);
        UpdateSettingsRefreshReadout();
        SettingsControl(L"STATIC",g_russian?L"Запуск":L"Startup",0,26,60,378,16,247,3);
        SettingsControl(L"STATIC",g_russian?L"Список задач":L"Task list",0,26,120,378,16,248,3);
        SettingsControl(L"BUTTON",g_russian?L"Запускать вместе с Windows":L"Start with Windows",WS_TABSTOP|BS_AUTOCHECKBOX,26,80,378,22,214,3);
        s_original_startup=NativeStartupEnabled();SendMessageW(GetDlgItem(window,214),BM_SETCHECK,s_original_startup?BST_CHECKED:BST_UNCHECKED,0);
        SettingsControl(L"BUTTON",g_russian?L"Показывать количество задач во вкладках":L"Show task totals in tabs",WS_TABSTOP|BS_AUTOCHECKBOX,26,140,378,22,224,3);
        SendMessageW(SettingsItem(224),BM_SETCHECK,g_show_tab_task_counts?BST_CHECKED:BST_UNCHECKED,0);
        SettingsControl(L"BUTTON",g_russian?L"Показывать вкладку «Все»":L"Show the All tab",WS_TABSTOP|BS_AUTOCHECKBOX,26,164,378,22,225,3);
        SendMessageW(SettingsItem(225),BM_SETCHECK,g_show_all_tab?BST_CHECKED:BST_UNCHECKED,0);
        SettingsControl(L"BUTTON",g_russian?L"Показывать вкладку «Готово»":L"Show the Done tab",WS_TABSTOP|BS_AUTOCHECKBOX,26,188,378,22,246,3);
        SendMessageW(SettingsItem(246),BM_SETCHECK,g_show_done_tab?BST_CHECKED:BST_UNCHECKED,0);
        SettingsControl(L"STATIC",g_russian?L"Задач в меню трея":L"Tasks in tray menu",SS_CENTERIMAGE,26,232,300,20,222,3);
        HWND tray_slider=SettingsControl(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,26,256,378,24,213,3);
        SendMessageW(tray_slider,TBM_SETRANGE,TRUE,MAKELPARAM(1,30));SendMessageW(tray_slider,TBM_SETLINESIZE,0,1);SendMessageW(tray_slider,TBM_SETPAGESIZE,0,5);
        SendMessageW(tray_slider,TBM_SETPOS,TRUE,max(1,min(30,g_tray_task_count)));
        SetWindowSubclass(tray_slider,SettingsSliderProcedure,1,0);
        SettingsControl(L"STATIC",L"",SS_CENTERIMAGE|SS_RIGHT,344,232,60,20,263,3);
        SettingsControl(L"STATIC",L"1",SS_CENTER,26,281,28,14,264,3);
        SettingsControl(L"STATIC",L"15",SS_CENTER,202,281,28,14,265,3);
        SettingsControl(L"STATIC",L"30",SS_CENTER,378,281,28,14,266,3);UpdateSettingsTrayReadout();
        SettingsControl(L"BUTTON",g_russian?L"Расширенные →":L"Advanced →",WS_TABSTOP|BS_OWNERDRAW,26,302,378,26,257,3);
        s_note=SettingsControl(L"STATIC",L"",0,26,343,378,20,223,-1);
        SettingsControl(L"STATIC",APP_TITLE,0,26,60,116,28,230,1);
        SettingsControl(L"STATIC",APP_VERSION,SS_CENTERIMAGE,150,66,200,20,268,1);
        SettingsControl(L"STATIC",g_russian?L"Приложение для обработки статусов задач Jira.":L"A Jira task status management app.",0,26,90,360,22,231,1);
        SettingsControl(L"STATIC",g_russian?L"Обновления":L"Updates",0,26,121,360,20,232,1);
        SettingsControl(L"STATIC",L"",0,26,143,360,28,235,1);
        SettingsControl(L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,26,172,260,28,242,1);
        SettingsControl(L"STATIC",g_russian?L"Помощь и обратная связь":L"Help and feedback",0,26,250,360,18,236,1);
        SettingsControl(L"BUTTON",g_russian?L"Сообщить о проблеме ↗":L"Report a problem ↗",WS_TABSTOP|BS_OWNERDRAW,26,274,192,28,238,1);
        SettingsControl(L"BUTTON",g_russian?L"Диагностика ↗":L"Diagnostics ↗",WS_TABSTOP|BS_OWNERDRAW,238,274,166,28,234,1);
        SettingsControl(L"BUTTON",g_russian?L"По умолчанию":L"Defaults",WS_TABSTOP|BS_OWNERDRAW,14,392,118,28,267,-1);
        SettingsControl(L"BUTTON",g_russian?L"Отмена":L"Cancel",WS_TABSTOP|BS_OWNERDRAW,265,392,76,28,IDCANCEL,-1);
        SettingsControl(L"BUTTON",g_russian?L"Сохранить":L"Save",WS_TABSTOP|BS_OWNERDRAW,349,392,85,28,IDOK,-1);
        int small_ids[]={216,219,223,227,228,229,235,236,247,248,264,265,266,268,269,271};for(int i=0;i<_countof(small_ids);i++)SendMessageW(GetDlgItem(window,small_ids[i]),WM_SETFONT,(WPARAM)s_small,TRUE);
        SendMessageW(GetDlgItem(window,230),WM_SETFONT,(WPARAM)s_title,TRUE);
        SendMessageW(GetDlgItem(window,232),WM_SETFONT,(WPARAM)s_bold,TRUE);
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            wchar_t name[32];GetClassNameW(child,name,32);
            if(!wcscmp(name,L"EDIT"))SetWindowSubclass(child,SettingsEditProcedure,1,0);
            if(!wcsicmp(name,L"BUTTON"))SetWindowSubclass(child,SettingsButtonProcedure,1,0);
        }
        ApplyWindowCaptionTheme(window);UpdateSettingsCheckboxThemes();s_initializing=FALSE;UpdateUiAttach(window);SettingsTab(0);return 0;
    }
    case WM_HSCROLL:
        if((HWND)lp==s_interval){SendMessageW(s_interval,TBM_SETPOS,TRUE,SettingsRefreshMinutes());UpdateSettingsRefreshReadout();return 0;}
        if((HWND)lp==GetDlgItem(window,213)){UpdateSettingsTrayReadout();return 0;}break;
    case WM_COMMAND:
        if(LOWORD(wp)==267){
            if(s_busy||g_jira_busy||UpdateUiBusy()||g_update_handoff||!IsWindowEnabled(SettingsItem(267))){
                SetWindowTextW(s_note,g_russian?L"Дождитесь завершения текущей операции.":L"Wait for the current operation to finish.");return 0;
            }
            ResetSettingsDraft();return 0;
        }
        if(LOWORD(wp)==252&&(HIWORD(wp)==EN_CHANGE||HIWORD(wp)==EN_UPDATE)){UpdateExclusionScrollBar();return 0;}
        if((LOWORD(wp)==224||LOWORD(wp)==225||LOWORD(wp)==246||LOWORD(wp)==250||LOWORD(wp)==251||LOWORD(wp)==261)&&HIWORD(wp)==BN_CLICKED&&!s_initializing&&!s_busy){ApplySettingsCheckboxPreview();return 0;}
        if(LOWORD(wp)==257&&!s_busy){OpenAdvancedSettingsWindow();return 0;}
        if(LOWORD(wp)==242){if(u_state==UPDATE_UI_AVAILABLE)UpdateUiBeginDownload(window);else if(u_state==UPDATE_UI_READY)UpdateUiBeginInstall(window);else UpdateUiBeginCheck(window);return 0;}
        if(LOWORD(wp)==210&&HIWORD(wp)==EN_CHANGE&&!s_initializing&&!s_busy){
            wchar_t entered[768];GetWindowTextW(s_url,entered,768);BOOL connected=s_has_account&&!wcscmp(entered,s_original_url);
            if(connected&&GetWindowTextLengthW(s_token))s_replacing_token=TRUE;
            s_connected=connected;RefreshSettingsAccountInfo();UpdateSettingsConnectionCaption();LayoutSettings();return 0;
        }
        if(LOWORD(wp)==274&&!s_busy&&s_connected){
            s_replacing_token=!s_replacing_token;
            if(!s_replacing_token)SetWindowTextW(s_token,L"");
            SetWindowTextW(s_note,L"");
            UpdateSettingsConnectionCaption();LayoutSettings();SetFocus(s_replacing_token?s_token:SettingsItem(274));return 0;
        }
        if(LOWORD(wp)>=200&&LOWORD(wp)<=203){SettingsTab(LOWORD(wp)-200);return 0;}
        if(LOWORD(wp)==IDOK&&!s_busy){
            if(s_replacing_token&&!GetWindowTextLengthW(s_token)){
                SettingsTab(0);SetWindowTextW(s_note,g_russian?L"Введите новый токен или отмените замену.":L"Enter a new token or cancel replacement.");SetFocus(s_token);return 0;
            }
            if(UpdateUiBusy()||g_update_handoff){SettingsTab(0);SetWindowTextW(s_note,g_russian?L"Дождитесь завершения обновления приложения.":L"Wait for the application update operation.");return 0;}
            wchar_t url[768];GetWindowTextW(s_url,url,_countof(url));
            if(!GetWindowTextLengthW(s_token)&&!wcscmp(url,s_original_url)){
                if(g_jira_busy){SettingsTab(0);SetWindowTextW(s_note,g_russian?L"Дождитесь текущего запроса Jira.":L"Wait for the current Jira request.");return 0;}
                if(SaveNativePreferences()){s_saved=TRUE;DestroyWindow(window);}return 0;
            }
            BeginAccountCheck();return 0;
        }
        if(LOWORD(wp)==215&&!s_busy){
            if(UpdateUiBusy()||g_update_handoff)return 0;
            if(g_jira_busy){SetWindowTextW(s_note,g_russian?L"Дождитесь завершения текущего запроса Jira перед выходом.":L"Wait for the current Jira request before signing out.");return 0;}
            wchar_t signed_out_url[768];GetWindowTextW(s_url,signed_out_url,768);
            s_busy=g_jira_busy=TRUE;
            int answer=MessageBoxW(window,g_russian?L"Выйти из Jira в " APP_TITLE L"?":L"Sign out of Jira in " APP_TITLE L"?",APP_TITLE,MB_YESNO|MB_ICONQUESTION);
            s_busy=g_jira_busy=FALSE;if(answer!=IDYES)return 0;
            if(!JiraSnapshotCacheRemove(g_settings_path)){
                SetWindowTextW(s_note,g_russian?L"Не удалось удалить локальный кэш задач. Закройте использующие его программы и повторите выход.":L"Could not remove the local task cache. Close programs using it and retry signing out.");return 0;
            }
            if(!JiraSignOut()){SetWindowTextW(s_note,g_russian?L"Не удалось полностью удалить учётные данные Native. Повторите выход.":L"Could not fully remove Native credentials. Try signing out again.");return 0;}
            s_has_account=s_connected=FALSE;s_saved=s_reload_tasks=FALSE;g_jira_stale=TRUE;g_updated_time=(SYSTEMTIME){0};
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
        /* Validate and persist preferences before committing the new credential. */
        if(check->ok&&!SaveNativePreferences()){check->ok=FALSE;GetWindowTextW(s_note,check->error,256);}
        if(check->ok&&check->replace){
            if(!JiraSaveAccount(check->url,check->token)){check->ok=FALSE;wcscpy_s(check->error,256,L"Could not save account in Windows Credential Manager.");}
            else{s_saved=s_reload_tasks=TRUE;g_jira_stale=TRUE;}
        }
        if(check->ok)s_saved=TRUE;
        if(!check->ok){SetWindowTextW(s_note,check->error);SettingsTab(0);FreezeSettings(FALSE);}
        BOOL done=check->ok;SecureZeroMemory(check,sizeof(*check));free(check);
        if(done)DestroyWindow(window);return 0;
    }
    case WM_CLOSE:if(s_busy){MessageBeep(MB_ICONINFORMATION);return 0;}DestroyWindow(window);return 0;
    case WM_SIZE:if(s_url)LayoutSettings();return 0;
    case WM_MOVE:if(s_advanced&&s_advanced_window)PositionOwnedSettingsWindow(s_advanced_window,window);return 0;
    case WM_DESTROY:
        UpdateUiCancel();
        if(s_advanced_window)DestroyWindow(s_advanced_window);
        if(!s_preferences_saved){
            g_notifications.tasks=s_original_task_alerts;g_notifications.comments=s_original_comment_alerts;g_notifications.statuses=s_original_status_alerts;
            g_show_tab_task_counts=s_original_tab_counts;g_show_all_tab=s_original_all_tab;g_show_done_tab=s_original_done_tab;
            if(g_tab==1&&((s_hidden_tab_fallback==0&&s_original_all_tab)||(s_hidden_tab_fallback==2&&s_original_done_tab))){g_tab=s_hidden_tab_fallback;RefreshTaskList();}
            RefreshSettingsPreview();
        }
        SetWindowTextW(s_token,L"");s_window=NULL;s_notifications=NULL;DeleteObject(s_font);DeleteObject(s_small);DeleteObject(s_bold);DeleteObject(s_title);DeleteObject(s_link_underline);s_link_underline=NULL;
        if(s_reload_tasks&&!InterlockedCompareExchange(&g_jira_closing,0,0)){g_jira_stale=TRUE;PostMessageW(g_window,WM_COMMAND,IDC_REFRESH,0);}
        return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{
        HDC dc=(HDC)wp;int id=GetDlgCtrlID((HWND)lp);BOOL muted=id==209||id==216||id==219||id==223||(id>=227&&id<=229)||id==235||id==236||id==239||id==241||id==247||id==248||id==255||id==260||id==262||(id>=264&&id<=266)||id==268||id==269||id==271;
        BOOL input=message==WM_CTLCOLOREDIT;SetTextColor(dc,muted?MutedColor():TextColor());SetBkColor(dc,input?PanelColor():WindowColor());return (LRESULT)(input?g_panel_brush:g_window_brush);
    }
    case WM_ERASEBKGND:{RECT r;GetClientRect(window,&r);FillRect((HDC)wp,&r,g_window_brush);return 1;}
    case WM_PRINTCLIENT:return SendMessageW(window,WM_PAINT,wp,lp);
    case WM_PAINT:{PAINTSTRUCT paint={0};HDC dc=wp?(HDC)wp:BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);RECT r={S(14),S(44),client.right-S(14),client.bottom-S(52)};HBRUSH b=CreateSolidBrush(g_dark?RGB(65,65,68):RGB(210,210,210));FrameRect(dc,&r,b);
        if(s_tab==0){
            if(s_connected&&!s_replacing_token){RECT account={S(26),S(114),client.right-S(26),S(184)};FrameRect(dc,&account,b);}
            int edits[]={210,211};for(int i=0;i<_countof(edits);i++){
                HWND edit=GetDlgItem(window,edits[i]);if(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VISIBLE))continue;
                RECT box;GetWindowRect(edit,&box);MapWindowPoints(NULL,window,(POINT*)&box,2);InflateRect(&box,S(6),S(3));
                FillRect(dc,&box,g_panel_brush);HBRUSH edge=CreateSolidBrush(GetFocus()==edit?AccentColor():(g_dark?RGB(85,85,90):RGB(175,175,175)));FrameRect(dc,&box,edge);DeleteObject(edge);
            }
        }else if(s_tab==3){RECT divider={S(26),S(222),client.right-S(26),S(222)+1};FillRect(dc,&divider,b);}
        else if(s_tab==1){RECT divider={S(26),s_about_divider_y,client.right-S(26),s_about_divider_y+1};FillRect(dc,&divider,b);}
        DeleteObject(b);if(!wp)EndPaint(window,&paint);return 0;}
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *draw=(DRAWITEMSTRUCT*)lp;RECT r=draw->rcItem;BOOL tab=draw->CtlID>=200&&draw->CtlID<=203,link=draw->CtlID==234||draw->CtlID==238||draw->CtlID==274;
        COLORREF background=!tab&&!link&&(s_hover==draw->hwndItem||(draw->itemState&ODS_SELECTED))?(g_dark?RGB(52,57,64):RGB(231,235,240)):WindowColor();HBRUSH fill=CreateSolidBrush(background);FillRect(draw->hDC,&r,fill);DeleteObject(fill);
        BOOL link_emphasis=link&&!(draw->itemState&ODS_DISABLED)&&(s_hover==draw->hwndItem||((draw->itemState&ODS_FOCUS)&&!(draw->itemState&ODS_NOFOCUSRECT)&&!GetPropW(draw->hwndItem,L"Tasklet.Settings.MouseFocus")));
        COLORREF color=draw->itemState&ODS_DISABLED?MutedColor():link?((draw->itemState&ODS_SELECTED)?(g_dark?RGB(150,220,255):RGB(0,85,150)):AccentColor()):TextColor();
        SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,color);HGDIOBJ previous_font=SelectObject(draw->hDC,link_emphasis&&s_link_underline?s_link_underline:s_font);
        wchar_t text[64];GetWindowTextW(draw->hwndItem,text,64);DrawTextW(draw->hDC,text,-1,&r,(link?DT_LEFT:DT_CENTER)|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SelectObject(draw->hDC,previous_font);
        if(!tab&&!link){HBRUSH border=CreateSolidBrush(draw->CtlID==IDOK||draw->CtlID==242||s_hover==draw->hwndItem||draw->itemState&ODS_FOCUS?AccentColor():(g_dark?RGB(85,85,90):RGB(175,175,175)));FrameRect(draw->hDC,&r,border);DeleteObject(border);}
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
    RECT bounds=SettingsOuterRect();
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,g_russian?APP_TITLE L" · Настройки":APP_TITLE L" · Settings",WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
        origin.x,origin.y,bounds.right-bounds.left,bounds.bottom-bounds.top,g_window,NULL,g_instance,NULL);
    if(!window)return;PositionSettingsWindow(window);ShowWindow(window,SW_SHOW);SetFocus(s_url);
}
