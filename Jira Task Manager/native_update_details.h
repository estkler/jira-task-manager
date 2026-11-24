/* Lightweight owned update window: release text and byte-based progress.
   No additional network calls and no extension of the helper wire protocol. */
static UINT u_detail_dpi;
static int u_detail_scale,u_detail_dark=-1,u_detail_ru=-1;
static HFONT u_detail_font,u_detail_small,u_detail_bold;
static int UD(int value){return MulDiv(value,(int)u_detail_dpi*u_detail_scale,9600);}
static HWND u_detail_hover;
static void UpdateDetailsFonts(void){
    HFONT font=CreateFontW(-UD(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    HFONT small=CreateFontW(-UD(11),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    HFONT bold=CreateFontW(-UD(13),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Semibold");
    if(!font||!small||!bold){if(font)DeleteObject(font);if(small)DeleteObject(small);if(bold)DeleteObject(bold);return;}
    for(HWND child=GetWindow(u_details,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){int id=GetDlgCtrlID(child);SendMessageW(child,WM_SETFONT,(WPARAM)(id==400?bold:id==403?small:font),TRUE);}
    if(u_detail_font)DeleteObject(u_detail_font);if(u_detail_small)DeleteObject(u_detail_small);if(u_detail_bold)DeleteObject(u_detail_bold);u_detail_font=font;u_detail_small=small;u_detail_bold=bold;
}
static int u_detail_percent=-1,u_detail_state=-1;
static ULONGLONG u_detail_asset;
static void UpdateDetailsText(int id,const wchar_t *text){HWND item=GetDlgItem(u_details,id);int n=GetWindowTextLengthW(item);size_t length=wcslen(text);wchar_t *old=malloc(((size_t)n+1)*sizeof(*old));if(!old)return;GetWindowTextW(item,old,n+1);if((size_t)n!=length||wcscmp(old,text))SetWindowTextW(item,text);free(old);}
static void UpdateDetailsNotes(void){
    wchar_t text[UPDATE_NOTES_CAP*2];size_t n=0;BOOL line=TRUE;const wchar_t *p=u_notes;
    while(*p&&n<_countof(text)-3){
        if(line){while(*p==L'#')p++;if(*p==L' ')p++;if((p[0]==L'-'||p[0]==L'*')&&p[1]==L' '){text[n++]=L'•';text[n++]=L' ';p+=2;}line=FALSE;}
        if(!*p)break;if(*p==L'\r'){p++;continue;}if(*p==L'\n'){text[n++]=L'\r';line=TRUE;}
        text[n++]=*p++;
    }text[n]=0;UpdateDetailsText(402,n?text:(g_russian?L"Описание изменений не приложено к этой версии.":L"No release notes were provided for this version."));u_detail_asset=u_release.asset_id;
}
static int UpdateDetailsPercent(void){if(u_work&&u_work->action==1&&u_work->release.size){LONG bytes=InterlockedCompareExchange(&u_work->received,0,0);return (int)min(100ULL,(ULONGLONG)max(0,bytes)*100/u_work->release.size);}return u_state==UPDATE_UI_READY||u_state==UPDATE_UI_APPLYING?100:0;}
static RECT UpdateDetailsBar(HWND window){RECT r;GetClientRect(window,&r);return (RECT){UD(20),r.bottom-UD(78),r.right-UD(20),r.bottom-UD(70)};}
static void UpdateDetailsLayout(HWND window){
    RECT r;GetClientRect(window,&r);int w=r.right,h=r.bottom;
    SetWindowPos(GetDlgItem(window,400),NULL,UD(20),UD(17),w-UD(40),UD(26),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(window,401),NULL,UD(20),UD(53),w-UD(40),UD(22),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(window,402),NULL,UD(24),UD(82),w-UD(48),max(UD(30),h-UD(208)),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(window,403),NULL,UD(20),h-UD(111),w-UD(40),UD(28),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(window,IDCANCEL),NULL,w-UD(250),h-UD(48),UD(100),UD(28),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(window,IDOK),NULL,w-UD(140),h-UD(48),UD(120),UD(28),SWP_NOZORDER|SWP_NOACTIVATE);
}
static void UpdateDetailsDraw(HWND window,HDC dc){
    RECT r;GetClientRect(window,&r);FillRect(dc,&r,g_window_brush);
    RECT notes={UD(20),UD(78),r.right-UD(20),r.bottom-UD(122)};FillRect(dc,&notes,g_panel_brush);HBRUSH border=CreateSolidBrush(BorderColor());FrameRect(dc,&notes,border);DeleteObject(border);
    if(u_state==UPDATE_UI_DOWNLOADING||u_state==UPDATE_UI_READY||u_state==UPDATE_UI_APPLYING){
        RECT bar=UpdateDetailsBar(window);int saved=SaveDC(dc);HRGN region=CreateRoundRectRgn(bar.left,bar.top,bar.right+1,bar.bottom+1,UD(8),UD(8));ExtSelectClipRgn(dc,region,RGN_AND);
        HBRUSH track=CreateSolidBrush(g_dark?RGB(51,57,65):RGB(224,229,235));FillRect(dc,&bar,track);DeleteObject(track);
        RECT fill=bar;fill.right=fill.left+MulDiv(fill.right-fill.left,UpdateDetailsPercent(),100);HBRUSH blue=CreateSolidBrush(AccentColor());FillRect(dc,&fill,blue);DeleteObject(blue);RestoreDC(dc,saved);DeleteObject(region);
    }
}
static void UpdateDetailsActions(void){
    HWND action=GetDlgItem(u_details,IDOK),cancel=GetDlgItem(u_details,IDCANCEL);
    BOOL applying=u_state==UPDATE_UI_APPLYING;
    BOOL action_ready=u_state==UPDATE_UI_AVAILABLE||u_state==UPDATE_UI_READY||u_state==UPDATE_UI_ERROR;
    BOOL action_enabled=action_ready&&!u_work&&!UpdateUiBusy()&&!UpdateSettingsAccountBusy();
    HWND focus=GetFocus();

    const wchar_t *label=g_russian?L"Скачать":L"Download";
    if(u_state==UPDATE_UI_READY||applying)label=g_russian?L"Установить":L"Install";
    if(u_state==UPDATE_UI_ERROR||u_state==UPDATE_UI_CHECKING)label=g_russian?L"Повторить":L"Retry";
    UpdateDetailsText(IDOK,label);
    UpdateDetailsText(IDCANCEL,u_state==UPDATE_UI_DOWNLOADING?(g_russian?L"Отмена":L"Cancel"):(g_russian?L"Закрыть":L"Close"));

    if(!action_enabled&&u_detail_hover==action)u_detail_hover=NULL;
    if(applying&&u_detail_hover==cancel)u_detail_hover=NULL;
    EnableWindow(action,action_enabled);
    EnableWindow(cancel,!applying);
    if((focus==action&&!action_enabled)||(focus==cancel&&applying))
        SetFocus(applying?GetDlgItem(u_details,402):cancel);

    if((int)u_state!=u_detail_state){
        EnableMenuItem(GetSystemMenu(u_details,FALSE),SC_CLOSE,MF_BYCOMMAND|(applying?MF_GRAYED:MF_ENABLED));
        DrawMenuBar(u_details);
    }
}
static void UpdateDetailsRender(void){
    if(!u_details||!IsWindow(u_details))return;wchar_t title[128],text[256];
    if(u_detail_dark!=(int)g_dark){u_detail_dark=(int)g_dark;SetWindowTheme(GetDlgItem(u_details,402),g_dark?L"DarkMode_Explorer":L"Explorer",NULL);ApplyWindowCaptionTheme(u_details);RedrawWindow(u_details,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);}
    if(u_detail_ru!=(int)g_russian){u_detail_ru=(int)g_russian;UpdateDetailsNotes();}
    swprintf(title,128,g_russian?L"Версия %lu.%lu.%lu":L"Version %lu.%lu.%lu",u_release.version.major,u_release.version.minor,u_release.version.patch);UpdateDetailsText(400,title);
    UpdateDetailsText(401,g_russian?L"Что нового":L"What's new");if(u_detail_asset!=u_release.asset_id)UpdateDetailsNotes();
    int percent=UpdateDetailsPercent();BOOL downloading=u_state==UPDATE_UI_DOWNLOADING;
    if(downloading&&u_work){if(InterlockedCompareExchange(&u_work->verifying,0,0))wcscpy_s(text,256,g_russian?L"Проверяем файл…":L"Verifying the file…");else swprintf(text,256,g_russian?L"Скачивание обновления · %d%%":L"Downloading update · %d%%",percent);}
    else if(u_state==UPDATE_UI_AVAILABLE)wcscpy_s(text,256,g_russian?L"Скачивание начнётся по нажатию «Скачать».":L"Click Download when you're ready.");
    else if(u_state==UPDATE_UI_READY)wcscpy_s(text,256,u_reason[0]?u_reason:(g_russian?L"Файл проверен. Можно установить обновление.":L"File verified. Ready to install."));
    else GetWindowTextW(GetDlgItem(u_settings,235),text,256);
    UpdateDetailsText(403,text);
    UpdateDetailsActions();
    if(percent!=u_detail_percent||(int)u_state!=u_detail_state){RECT bar=UpdateDetailsBar(u_details);InvalidateRect(u_details,&bar,FALSE);u_detail_percent=percent;u_detail_state=(int)u_state;}
}
static void UpdateDetailsClose(void){HWND window=u_details;u_details=NULL;u_detail_hover=NULL;if(window&&IsWindow(window))DestroyWindow(window);}
static void UpdateDetailsDismiss(void){
    if(u_state==UPDATE_UI_APPLYING)return;
    if(u_work&&u_work->action==1){++u_generation;InterlockedExchange(&u_work->cancel.cancelled,1);UpdateWorkRelease(u_work);u_work=NULL;u_state=UPDATE_UI_AVAILABLE;u_reason[0]=0;}
    UpdateDetailsClose();UpdateUiRender(u_settings);
}
static LRESULT CALLBACK UpdateDetailButton(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    if(message==WM_MOUSEMOVE){u_detail_hover=window;TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);InvalidateRect(window,NULL,FALSE);}
    if(message==WM_MOUSELEAVE){if(u_detail_hover==window)u_detail_hover=NULL;InvalidateRect(window,NULL,FALSE);}
    return DefSubclassProc(window,message,wp,lp);
}
static LRESULT CALLBACK UpdateDetailsProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CLOSE||(message==WM_KEYDOWN&&wp==VK_ESCAPE)){UpdateDetailsDismiss();return 0;}
    if(message==WM_COMMAND){int id=LOWORD(wp);if(id==IDCANCEL){UpdateDetailsDismiss();return 0;}if(id==IDOK){if(u_state==UPDATE_UI_READY)UpdateUiBeginInstall(u_settings);else if(u_state==UPDATE_UI_AVAILABLE)UpdateUiBeginDownload(u_settings);else if(u_state==UPDATE_UI_ERROR){if(u_failed_action==1&&u_release.asset_id){u_state=UPDATE_UI_AVAILABLE;UpdateUiBeginDownload(u_settings);}else UpdateUiBeginCheck(u_settings);}return 0;}}
    if(message==WM_TIMER){UpdateDetailsRender();return 0;}
    if(message==WM_DPICHANGED){u_detail_dpi=HIWORD(wp);UpdateDetailsFonts();RECT *r=(RECT*)lp;SetWindowPos(window,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(window,NULL,FALSE);return 0;}
    if(message==WM_SIZE){UpdateDetailsLayout(window);return 0;}
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT){HDC dc=(HDC)wp;int id=GetDlgCtrlID((HWND)lp);SetTextColor(dc,id==403?MutedColor():TextColor());SetBkColor(dc,id==402?PanelColor():WindowColor());return (LRESULT)(id==402?g_panel_brush:g_window_brush);}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT||message==WM_PRINTCLIENT){PAINTSTRUCT paint;HDC dc=message==WM_PRINTCLIENT?(HDC)wp:BeginPaint(window,&paint);UpdateDetailsDraw(window,dc);if(message==WM_PAINT)EndPaint(window,&paint);return 0;}
    if(message==WM_DRAWITEM){DRAWITEMSTRUCT *draw=(DRAWITEMSTRUCT*)lp;RECT r=draw->rcItem;BOOL enabled=!(draw->itemState&ODS_DISABLED),hover=enabled&&u_detail_hover==draw->hwndItem;HBRUSH brush=CreateSolidBrush(enabled&&(hover||(draw->itemState&ODS_SELECTED))?(g_dark?RGB(52,57,64):RGB(231,235,240)):WindowColor());FillRect(draw->hDC,&r,brush);DeleteObject(brush);
        SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,draw->itemState&ODS_DISABLED?MutedColor():TextColor());HGDIOBJ font=SelectObject(draw->hDC,u_detail_font?u_detail_font:g_font);wchar_t text[80];GetWindowTextW(draw->hwndItem,text,80);DrawTextW(draw->hDC,text,-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);SelectObject(draw->hDC,font);
        brush=CreateSolidBrush(hover?AccentColor():BorderColor());FrameRect(draw->hDC,&r,brush);DeleteObject(brush);if(enabled&&(draw->itemState&ODS_FOCUS)&&!(draw->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-UD(3),-UD(3));DrawFocusRect(draw->hDC,&r);}return TRUE;
    }
    if(message==WM_DESTROY){KillTimer(window,1);if(u_detail_font)DeleteObject(u_detail_font);if(u_detail_small)DeleteObject(u_detail_small);if(u_detail_bold)DeleteObject(u_detail_bold);u_detail_font=u_detail_small=u_detail_bold=NULL;return 0;}
    if(message==WM_NCDESTROY&&u_details==window)u_details=NULL;
    return DefWindowProcW(window,message,wp,lp);
}
static void UpdateDetailsOpen(HWND settings){
    if(u_shutdown||u_handoff||!settings||!IsWindow(settings))return;if(u_details){ShowWindow(u_details,SW_RESTORE);SetForegroundWindow(u_details);return;}
    u_detail_dpi=g_dpi;u_detail_scale=g_density==1?100:90;u_detail_dark=u_detail_ru=-1;u_settings=settings;WNDCLASSW wc={.lpfnWndProc=UpdateDetailsProcedure,.hInstance=g_instance,.hCursor=LoadCursorW(NULL,IDC_ARROW),.hIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1)),.lpszClassName=L"Tasklet.UpdateDetails"};RegisterClassW(&wc);
    RECT parent;GetWindowRect(settings,&parent);MONITORINFO monitor={.cbSize=sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(settings,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT size={0,0,UD(420),UD(370)};DWORD style=WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&size,style,FALSE,WS_EX_DLGMODALFRAME,g_dpi);
    int w=min(size.right-size.left,monitor.rcWork.right-monitor.rcWork.left-UD(16)),h=min(size.bottom-size.top,monitor.rcWork.bottom-monitor.rcWork.top-UD(16));
    int x=max(monitor.rcWork.left,min((parent.left+parent.right-w)/2,monitor.rcWork.right-w)),y=max(monitor.rcWork.top,min((parent.top+parent.bottom-h)/2,monitor.rcWork.bottom-h));
    u_details=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,g_russian?APP_TITLE L" · Обновление":APP_TITLE L" · Update",style,x,y,w,h,settings,NULL,g_instance,NULL);if(!u_details)return;
    struct {const wchar_t *cls;int id;DWORD style;} items[]={{L"STATIC",400,0},{L"STATIC",401,0},{L"EDIT",402,ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_TABSTOP},{L"STATIC",403,0},{L"BUTTON",IDCANCEL,BS_OWNERDRAW|WS_TABSTOP},{L"BUTTON",IDOK,BS_OWNERDRAW|WS_TABSTOP}};
    for(int i=0;i<_countof(items);i++){HWND child=CreateWindowW(items[i].cls,L"",WS_CHILD|WS_VISIBLE|items[i].style,0,0,1,1,u_details,(HMENU)(INT_PTR)items[i].id,g_instance,NULL);SendMessageW(child,WM_SETFONT,(WPARAM)(items[i].id==400?g_bold_font:items[i].id==403?g_small_font:g_font),FALSE);if(items[i].id==IDOK||items[i].id==IDCANCEL)SetWindowSubclass(child,UpdateDetailButton,1,0);if(items[i].id==402)SetWindowTheme(child,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);}
    UpdateDetailsFonts();u_detail_asset=0;u_detail_percent=-1;u_detail_state=-1;UpdateDetailsLayout(u_details);UpdateDetailsNotes();UpdateDetailsRender();ApplyWindowCaptionTheme(u_details);SetTimer(u_details,1,120,NULL);
    SendMessageW(u_details,WM_UPDATEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);ShowWindow(u_details,SW_SHOW);SetFocus(GetDlgItem(u_details,IDOK));
}
