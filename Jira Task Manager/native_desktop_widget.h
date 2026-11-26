/* Compact, non-activating companion to the main window. Included by native.c. */
static HWND g_desktop_widget,g_widget_tooltip;
static BOOL g_desktop_widget_main_shown;
static UINT g_desktop_widget_dpi = 96;
static HFONT g_desktop_widget_font;
static BOOL g_widget_position_saved,g_widget_editing,g_widget_moved;
static POINT g_widget_position,g_widget_press,g_widget_offset;
static HWND g_widget_drag_source;
static ULONGLONG g_widget_fade_started;

enum { WIDGET_OPEN = 1, WIDGET_HIDE, WIDGET_TIMER, WIDGET_EDIT, WIDGET_HEIGHT = 38,
    WIDGET_FADE_TIMER = 1, WIDGET_FADE_MS = 150,
    WIDGET_CLOSE_SPACE = 27, WIDGET_IDLE_OPEN_SPACE = 32, WIDGET_TIMER_PADDING = 7 };

#include "native_widget_transition.h"

static int WidgetScale(int value)
{
    return MulDiv(value,g_desktop_widget_dpi,96);
}

static COLORREF WidgetBackground(void)
{
    return g_dark?RGB(35,39,45):RGB(248,250,252);
}

static COLORREF WidgetBorder(void)
{
    return g_dark?RGB(108,119,134):RGB(126,137,151);
}

static const TaskItem *DesktopWidgetActiveTask(void)
{
    for(int i=0;i<g_task_count;i++){
        if(g_tasks[i].assigned_to_me&&TaskHasActiveAccent(&g_tasks[i]))return &g_tasks[i];
    }
    return NULL;
}

static int DesktopWidgetTimerWidth(HWND window)
{
    const wchar_t *sample=g_russian?L"88ч 88м 88с":L"88h 88m 88s";
    SIZE text={0};
    HDC dc=GetDC(window);
    HGDIOBJ font=SelectObject(dc,g_desktop_widget_font?g_desktop_widget_font:g_font);
    GetTextExtentPoint32W(dc,sample,(int)wcslen(sample),&text);
    SelectObject(dc,font);ReleaseDC(window,dc);
    return text.cx+2*WidgetScale(WIDGET_TIMER_PADDING);
}

static RECT DesktopWidgetTimerRect(HWND window)
{
    RECT bounds;
    GetClientRect(window,&bounds);
    bounds.right-=WidgetScale(WIDGET_CLOSE_SPACE);
    bounds.left=max(0,bounds.right-DesktopWidgetTimerWidth(window));
    bounds.top=WidgetScale(3);
    bounds.bottom-=WidgetScale(3);
    return bounds;
}

static void LayoutDesktopWidget(HWND window)
{
    RECT client,timer=DesktopWidgetTimerRect(window);
    GetClientRect(window,&client);
    int inset=WidgetScale(3),height=timer.bottom-timer.top;
    const UINT flags=SWP_NOACTIVATE|SWP_NOZORDER;
    SetWindowPos(GetDlgItem(window,WIDGET_OPEN),NULL,inset,inset,max(1,timer.left-inset),height,flags);
    SetWindowPos(GetDlgItem(window,WIDGET_TIMER),NULL,timer.left,timer.top,timer.right-timer.left,height,flags);
    SetWindowPos(GetDlgItem(window,WIDGET_EDIT),NULL,timer.left+WidgetScale(3),WidgetScale(7),
        timer.right-timer.left-WidgetScale(6),WidgetScale(24),flags);
    SetWindowPos(GetDlgItem(window,WIDGET_HIDE),NULL,client.right-WidgetScale(26),inset,WidgetScale(23),height,flags);
}

static void UpdateDesktopWidgetTime(void)
{
    HWND widget=g_desktop_widget;
    if(!widget||!IsWindowVisible(widget))return;
    HWND clock=GetDlgItem(widget,WIDGET_TIMER);
    wchar_t time[80],previous[80];
    if(g_bar_time)swprintf(time,_countof(time),g_russian?L"%d%% рабочего дня":L"%d%% of the workday",g_timer_bar_amount/10);
    else wcscpy_s(time,_countof(time),g_timer_numbers);
    GetWindowTextW(clock,previous,_countof(previous));
    if(wcscmp(previous,time))SetWindowTextW(clock,time);
    InvalidateRect(clock,NULL,FALSE);
    if(g_widget_tooltip&&IsWindowVisible(g_widget_tooltip))SendMessageW(g_widget_tooltip,TTM_UPDATE,0,0);
}

static void EndWidgetStartEdit(void)
{
    if(!g_widget_editing)return;
    g_widget_editing=FALSE;
    ShowWindow(GetDlgItem(g_desktop_widget,WIDGET_EDIT),SW_HIDE);
    ShowWindow(GetDlgItem(g_desktop_widget,WIDGET_TIMER),SW_SHOWNA);
    UpdateDesktopWidgetTime();
}

static void HideDesktopWidget(void)
{
    CancelDesktopWidgetTransition();
    if(!g_desktop_widget)return;
    EndWidgetStartEdit();
    KillTimer(g_desktop_widget,WIDGET_FADE_TIMER);
    g_widget_fade_started=0;
    if(g_widget_tooltip)SendMessageW(g_widget_tooltip,TTM_POP,0,0);
    if(g_widget_drag_source&&GetCapture()==g_widget_drag_source)ReleaseCapture();
    ShowWindow(g_desktop_widget,SW_HIDE);
}

static int DesktopWidgetWidth(HWND window,int work_width)
{
    int preferred=DesktopWidgetActiveTask()?WidgetScale(398):
        WidgetScale(WIDGET_IDLE_OPEN_SPACE)+DesktopWidgetTimerWidth(window)+WidgetScale(WIDGET_CLOSE_SPACE);
    return min(preferred,work_width-WidgetScale(10)*2);
}

static void PositionDesktopWidget(HWND window,HMONITOR monitor,UINT dpi,const POINT *origin)
{
    MONITORINFO info={sizeof(info)};
    if(!GetMonitorInfoW(monitor,&info))return;
    int work_width=info.rcWork.right-info.rcWork.left;
    /* Keep enough room for the task even on narrow, highly scaled displays. */
    dpi=min(dpi,(UINT)max(1,MulDiv(work_width,96,300)));
    BOOL dpi_changed=dpi!=g_desktop_widget_dpi;
    g_desktop_widget_dpi=dpi;
    if(dpi_changed||!g_desktop_widget_font){
        if(g_desktop_widget_font)DeleteObject(g_desktop_widget_font);
        g_desktop_widget_font=CreateFontW(-WidgetScale(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    }
    int inset=WidgetScale(10),height=WidgetScale(WIDGET_HEIGHT);
    int width=DesktopWidgetWidth(window,work_width);
    int x=info.rcWork.right-inset-width,y=info.rcWork.bottom-inset-height;
    if(origin){
        x=max(info.rcWork.left,min(info.rcWork.right-width,origin->x));
        y=max(info.rcWork.top,min(info.rcWork.bottom-height,origin->y));
    }
    SetWindowPos(window,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);
    RECT client;
    GetClientRect(window,&client);
    width=client.right;height=client.bottom; /* A nested WM_DPICHANGED may have resized it again. */
    SendMessageW(GetDlgItem(window,WIDGET_EDIT),WM_SETFONT,(WPARAM)g_desktop_widget_font,TRUE);
    LayoutDesktopWidget(window);
    HRGN corners=CreateRoundRectRgn(0,0,width+1,height+1,WidgetScale(16),WidgetScale(16));
    if(corners&&!SetWindowRgn(window,corners,TRUE))DeleteObject(corners);
}

static void RefreshDesktopWidgetLayout(HWND window)
{
    if(g_widget_drag_source)return;
    HMONITOR monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info={sizeof(info)};
    RECT bounds;
    if(GetMonitorInfoW(monitor,&info)&&GetClientRect(window,&bounds)&&
       bounds.right!=DesktopWidgetWidth(window,info.rcWork.right-info.rcWork.left)){
        CompleteDesktopWidgetTransition();
        PositionDesktopWidget(window,monitor,GetDpiForWindow(window),g_widget_position_saved?&g_widget_position:NULL);
        InvalidateRect(window,NULL,TRUE);
    }else LayoutDesktopWidget(window);
}

static void RefreshDesktopWidgetTask(void)
{
    if(!g_desktop_widget||!IsWindowVisible(g_desktop_widget))return;
    HWND button=GetDlgItem(g_desktop_widget,WIDGET_OPEN);
    const TaskItem *task=DesktopWidgetActiveTask();
    wchar_t label[_countof(((JiraItem*)0)->key)+_countof(((JiraItem*)0)->summary)+1];
    if(task)swprintf(label,_countof(label),L"%ls  %ls",task->key,task->summary);
    else wcscpy_s(label,_countof(label),g_russian?L"Открыть Jira Tasklet":L"Open Jira Tasklet");
    SetWindowTextW(button,label);
    RefreshDesktopWidgetLayout(g_desktop_widget);
    InvalidateRect(button,NULL,FALSE);
    InvalidateRect(GetDlgItem(g_desktop_widget,WIDGET_TIMER),NULL,FALSE);
}

static void PaintWidgetOpen(HDC dc,RECT bounds)
{
    int icon_size=WidgetScale(16);
    DrawIconEx(dc,bounds.left+WidgetScale(6),(bounds.top+bounds.bottom-icon_size)/2,
        LoadAppIcon(TRUE),icon_size,icon_size,0,NULL,DI_NORMAL);
    const TaskItem *task=DesktopWidgetActiveTask();
    if(!task)return;
    bounds.left+=WidgetScale(30);
    bounds.right-=WidgetScale(4);
    const UINT text_flags=DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
    SIZE key={0};
    GetTextExtentPoint32W(dc,task->key,(int)wcslen(task->key),&key);
    RECT key_bounds=bounds;
    key_bounds.right=min(bounds.right,bounds.left+min(key.cx,WidgetScale(108)));
    SetTextColor(dc,AccentColor());
    DrawTextW(dc,task->key,-1,&key_bounds,text_flags);
    bounds.left=key_bounds.right+WidgetScale(9);
    SetTextColor(dc,TextColor());
    if(bounds.left<bounds.right)DrawTextW(dc,task->summary,-1,&bounds,text_flags);
}

static COLORREF WidgetHoverColor(void)
{
    return g_dark?RGB(52,59,68):RGB(232,237,243);
}

static void PaintWidgetTime(HDC dc,RECT bounds,BOOL highlighted);

static void PaintWidgetButton(const DRAWITEMSTRUCT *draw)
{
    HDC dc=draw->hDC;
    RECT bounds=draw->rcItem;
    HDC buffer=NULL;HBITMAP bitmap=NULL;HGDIOBJ old_bitmap=NULL;
    if(draw->CtlID==WIDGET_TIMER&&bounds.right>0&&bounds.bottom>0){
        buffer=CreateCompatibleDC(dc);
        if(buffer)bitmap=CreateCompatibleBitmap(dc,bounds.right,bounds.bottom);
        if(bitmap){old_bitmap=SelectObject(buffer,bitmap);dc=buffer;}
    }
    POINT cursor;
    BOOL hover=GetCursorPos(&cursor)&&ScreenToClient(draw->hwndItem,&cursor)&&PtInRect(&bounds,cursor);
    BOOL highlighted=hover||(draw->itemState&ODS_SELECTED);
    HBRUSH base=CreateSolidBrush(WidgetBackground());
    FillRect(dc,&bounds,base);DeleteObject(base);
    if(highlighted&&!(draw->CtlID==WIDGET_TIMER&&g_bar_time)){
        HBRUSH fill=CreateSolidBrush(WidgetHoverColor());
        HGDIOBJ old_brush=SelectObject(dc,fill),old_pen=SelectObject(dc,GetStockObject(NULL_PEN));
        RoundRect(dc,0,0,bounds.right,bounds.bottom,WidgetScale(12),WidgetScale(12));
        SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(fill);
    }
    if(draw->CtlID==WIDGET_OPEN){
        SetBkMode(dc,TRANSPARENT);
        HGDIOBJ font=SelectObject(dc,g_desktop_widget_font?g_desktop_widget_font:g_font);
        PaintWidgetOpen(dc,bounds);
        SelectObject(dc,font);
    }else if(draw->CtlID==WIDGET_TIMER){
        PaintWidgetTime(dc,bounds,highlighted);
    }else{
        int cx=bounds.right/2,cy=bounds.bottom/2,arm=WidgetScale(4);
        HPEN pen=CreatePen(PS_SOLID,max(1,WidgetScale(1)),MutedColor());
        HGDIOBJ old=SelectObject(dc,pen);
        MoveToEx(dc,cx-arm,cy-arm,NULL);LineTo(dc,cx+arm+1,cy+arm+1);
        MoveToEx(dc,cx+arm,cy-arm,NULL);LineTo(dc,cx-arm-1,cy+arm+1);
        SelectObject(dc,old);DeleteObject(pen);
    }
    if(bitmap){
        BitBlt(draw->hDC,0,0,bounds.right,bounds.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,old_bitmap);DeleteObject(bitmap);
    }
    if(buffer)DeleteDC(buffer);
}

static void PaintWidgetTime(HDC dc,RECT bounds,BOOL highlighted)
{
    HGDIOBJ font=SelectObject(dc,g_desktop_widget_font?g_desktop_widget_font:g_font);
    if(g_bar_time){
        DrawWorkdayProgressSurface(dc,bounds,WidgetScale(12),
            highlighted?WidgetHoverColor():WorkdayProgressTrackColor());
        SelectObject(dc,font);
        return;
    }
    int middle=(bounds.top+bounds.bottom)/2;
    HBRUSH separator=CreateSolidBrush(g_dark?RGB(66,72,82):RGB(210,218,228));
    RECT line={bounds.left,middle-WidgetScale(9),bounds.left+1,middle+WidgetScale(9)};
    FillRect(dc,&line,separator);DeleteObject(separator);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,g_timer_overrun?(g_dark?RGB(255,111,116):RGB(190,30,40)):TextColor());
    bounds.left+=WidgetScale(WIDGET_TIMER_PADDING);bounds.right-=WidgetScale(WIDGET_TIMER_PADDING);
    DrawTextW(dc,g_timer_numbers,-1,&bounds,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,font);
}

static void RememberDesktopWidgetPosition(HWND widget)
{
    RECT bounds;
    if(!GetWindowRect(widget,&bounds))return;
    POINT origin={bounds.left,bounds.top};
    PositionDesktopWidget(widget,MonitorFromWindow(widget,MONITOR_DEFAULTTONEAREST),GetDpiForWindow(widget),&origin);
    GetWindowRect(widget,&bounds);
    g_widget_position=(POINT){bounds.left,bounds.top};g_widget_position_saved=TRUE;
    wchar_t position[64];
    swprintf(position,_countof(position),L"%ld,%ld",g_widget_position.x,g_widget_position.y);
    WritePrivateProfileStringW(L"DesktopWidget",L"Position",position,g_settings_path);
}

static void MoveWidgetWithPointer(HWND source,LPARAM lp)
{
    POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
    ClientToScreen(source,&point);
    if(!g_widget_moved&&abs(point.x-g_widget_press.x)<WidgetScale(4)&&abs(point.y-g_widget_press.y)<WidgetScale(4))return;
    g_widget_moved=TRUE;
    SetCursor(LoadCursorW(NULL,IDC_SIZEALL));
    SetWindowPos(g_desktop_widget,NULL,point.x-g_widget_offset.x,point.y-g_widget_offset.y,0,0,
        SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}

static BOOL HandleWidgetDrag(HWND source,UINT message,LPARAM lp)
{
    HWND widget=g_desktop_widget;
    switch(message){
    case WM_LBUTTONDOWN:{
        CompleteDesktopWidgetTransition();
        RECT bounds;GetWindowRect(widget,&bounds);
        POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        ClientToScreen(source,&point);
        g_widget_drag_source=source;g_widget_press=point;g_widget_moved=FALSE;
        g_widget_offset=(POINT){point.x-bounds.left,point.y-bounds.top};
        SetForegroundWindow(widget);SetCapture(source);
        return TRUE;
    }
    case WM_MOUSEMOVE:
        if(g_widget_drag_source==source&&GetCapture()==source){
            MoveWidgetWithPointer(source,lp);
            return TRUE;
        }
        break;
    case WM_LBUTTONUP:
        if(g_widget_drag_source==source){
            MoveWidgetWithPointer(source,lp);
            BOOL moved=g_widget_moved;
            g_widget_drag_source=NULL;g_widget_moved=FALSE;
            if(GetCapture()==source)ReleaseCapture();
            if(moved)RememberDesktopWidgetPosition(widget);
            else if(source!=widget){
                RECT client;GetClientRect(source,&client);
                if(PtInRect(&client,(POINT){GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}))
                    SendMessageW(widget,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(source),BN_CLICKED),(LPARAM)source);
            }
            return TRUE;
        }
        break;
    case WM_CANCELMODE:
        if(g_widget_drag_source==source&&GetCapture()==source)ReleaseCapture();
        break;
    case WM_CAPTURECHANGED:
        if(g_widget_drag_source==source){
            BOOL moved=g_widget_moved;
            g_widget_drag_source=NULL;g_widget_moved=FALSE;
            if(moved)RememberDesktopWidgetPosition(widget);
        }
        break;
    }
    return FALSE;
}

static void ShowWidgetTimerMenu(HWND widget)
{
    if(g_update_handoff)return;
    if(g_widget_tooltip)SendMessageW(g_widget_tooltip,TTM_POP,0,0);
    RECT bounds;GetWindowRect(GetDlgItem(widget,WIDGET_TIMER),&bounds);
    MONITORINFO monitor={sizeof(monitor)};
    BOOL below=GetMonitorInfoW(MonitorFromWindow(widget,MONITOR_DEFAULTTONEAREST),&monitor)&&
        monitor.rcWork.bottom-bounds.bottom>=WidgetScale(64);
    SetForegroundWindow(widget);
    int command=TrackWorkdayMenu(widget,(POINT){bounds.left,below?bounds.bottom:bounds.top},below?TPM_TOPALIGN:TPM_BOTTOMALIGN);
    PostMessageW(widget,WM_NULL,0,0);
    if(command==IDM_TIMER_RESET&&IsWindowVisible(widget)){
        HWND edit=GetDlgItem(widget,WIDGET_EDIT);
        PrefillStartEdit(edit);g_widget_editing=TRUE;
        ShowWindow(GetDlgItem(widget,WIDGET_TIMER),SW_HIDE);
        ShowWindow(edit,SW_SHOWNA);
        SetFocus(edit);
        if(GetFocus()==edit)SendMessageW(edit,EM_SETSEL,0,-1);
        else EndWidgetStartEdit();
    }else if(command==IDM_TIMER_PORTAL)OpenPortal();
}

static LRESULT CALLBACK WidgetStartEditProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,
    UINT_PTR subclass,DWORD_PTR reference)
{
    if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
    if(message==WM_KILLFOCUS)EndWidgetStartEdit();
    if(message==WM_KEYDOWN&&wp==VK_ESCAPE){EndWidgetStartEdit();return 0;}
    if(message==WM_KEYDOWN&&wp==VK_RETURN){
        if(SaveWorkdayStartFromEdit(window)){EndWidgetStartEdit();UpdateTimer();}
        return 0;
    }
    if(message==WM_CHAR&&(wp==VK_RETURN||wp==VK_ESCAPE))return 0;
    return DefSubclassProc(window,message,wp,lp);
}

static LRESULT CALLBACK DesktopWidgetButtonProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,
    UINT_PTR subclass,DWORD_PTR reference)
{
    int control=GetDlgCtrlID(window);
    if(message==WM_LBUTTONDOWN||message==WM_RBUTTONDOWN)CompleteDesktopWidgetTransition();
    BOOL draggable=control==WIDGET_OPEN||control==WIDGET_TIMER;
    if(draggable&&HandleWidgetDrag(window,message,lp))return 0;
    if(draggable&&message==BM_CLICK){
        SendMessageW(GetParent(window),WM_COMMAND,MAKEWPARAM(control,BN_CLICKED),(LPARAM)window);
        return 0;
    }
    if(draggable&&(message==WM_RBUTTONUP||message==WM_CONTEXTMENU)){
        if(message==WM_RBUTTONUP||(LPARAM)-1==lp){
            ToggleTimerFormat();
        }
        return 0;
    }
    switch(message){
    case WM_SETCURSOR:SetCursor(LoadCursorW(NULL,g_widget_moved?IDC_SIZEALL:IDC_HAND));return TRUE;
    case WM_MOUSEMOVE:{
        TRACKMOUSEEVENT tracking={sizeof(tracking),TME_QUERY,window,0};
        if(!TrackMouseEvent(&tracking)||!(tracking.dwFlags&TME_LEAVE)){
            tracking.dwFlags=TME_LEAVE;tracking.hwndTrack=window;
            TrackMouseEvent(&tracking);InvalidateRect(window,NULL,FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE:InvalidateRect(window,NULL,FALSE);break;
    }
    return DefSubclassProc(window,message,wp,lp);
}

static LRESULT CALLBACK DesktopWidgetProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(HandleWidgetDrag(window,message,lp))return 0;
    switch(message){
    case WM_CREATE:{
        HWND open=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            0,0,0,0,window,(HMENU)WIDGET_OPEN,g_instance,NULL);
        HWND hide=CreateWindowW(L"BUTTON",g_russian?L"Скрыть виджет":L"Hide widget",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            0,0,0,0,window,(HMENU)WIDGET_HIDE,g_instance,NULL);
        HWND clock=CreateWindowW(L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            0,0,0,0,window,(HMENU)WIDGET_TIMER,g_instance,NULL);
        HWND edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_BORDER|ES_CENTER|ES_AUTOHSCROLL,
            0,0,0,0,window,(HMENU)WIDGET_EDIT,g_instance,NULL);
        if(!open||!hide||!clock||!edit)return -1;
        SetWindowSubclass(open,DesktopWidgetButtonProcedure,1,0);
        SetWindowSubclass(hide,DesktopWidgetButtonProcedure,1,0);
        SetWindowSubclass(clock,DesktopWidgetButtonProcedure,1,0);
        SetWindowSubclass(edit,WidgetStartEditProcedure,1,0);
        SendMessageW(edit,EM_SETLIMITTEXT,8,0);
        HWND tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_NOPREFIX,
            0,0,0,0,window,NULL,g_instance,NULL);
        if(tip){
            g_widget_tooltip=tip;
            TOOLINFOW tool={.cbSize=sizeof(tool)-sizeof(void*),.uFlags=TTF_IDISHWND|TTF_SUBCLASS,
                .hwnd=window,.uId=(UINT_PTR)clock,.lpszText=LPSTR_TEXTCALLBACKW};
            SendMessageW(tip,TTM_SETMAXTIPWIDTH,0,WidgetScale(320));
            SendMessageW(tip,TTM_ADDTOOLW,0,(LPARAM)&tool);
        }
        return 0;
    }
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT paint;
        HDC dc=BeginPaint(window,&paint);
        RECT bounds;
        GetClientRect(window,&bounds);
        HBRUSH fill=CreateSolidBrush(WidgetBackground());
        HPEN outline=CreatePen(PS_SOLID,1,WidgetBorder());
        HGDIOBJ brush=SelectObject(dc,fill),pen=SelectObject(dc,outline);
        RoundRect(dc,0,0,bounds.right,bounds.bottom,WidgetScale(16),WidgetScale(16));
        SelectObject(dc,pen);SelectObject(dc,brush);DeleteObject(outline);DeleteObject(fill);
        EndPaint(window,&paint);return 0;
    }
    case WM_TIMER:
        if(wp==WIDGET_TRANSITION_TIMER)DesktopWidgetTransitionTick(GetTickCount64());
        else if(wp==WIDGET_FADE_TIMER&&g_widget_fade_started){
            ULONGLONG elapsed=min(GetTickCount64()-g_widget_fade_started,(ULONGLONG)WIDGET_FADE_MS);
            SetLayeredWindowAttributes(window,0,(BYTE)(35+220*elapsed/WIDGET_FADE_MS),LWA_ALPHA);
            if(elapsed==WIDGET_FADE_MS){KillTimer(window,WIDGET_FADE_TIMER);g_widget_fade_started=0;}
        }
        return 0;
    case WM_DRAWITEM:{
        const DRAWITEMSTRUCT *draw=(const DRAWITEMSTRUCT*)lp;
        if(draw->CtlType==ODT_MENU)DrawNativeMenuItem(draw);
        else PaintWidgetButton(draw);
        return TRUE;
    }
    case WM_MEASUREITEM:
        if(MeasureNativeMenuItem(window,(MEASUREITEMSTRUCT*)lp))return TRUE;
        break;
    case WM_NOTIFY:
        if(((NMHDR*)lp)->code==TTN_GETDISPINFOW){
            ((NMTTDISPINFOW*)lp)->lpszText=(wchar_t*)TimerTooltipText();
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp,TextColor());SetBkColor((HDC)wp,PanelColor());
        return (LRESULT)g_panel_brush;
    case WM_COMMAND:
        if(HIWORD(wp)==BN_CLICKED){
            if(LOWORD(wp)==WIDGET_HIDE)HideDesktopWidget();
            if(LOWORD(wp)==WIDGET_TIMER)ShowWidgetTimerMenu(window);
            if(LOWORD(wp)==WIDGET_OPEN){
                HideDesktopWidget();
                ShowWindow(g_window,SW_RESTORE);SetForegroundWindow(g_window);
            }
        }
        return 0;
    case WM_CLOSE:HideDesktopWidget();return 0;
    case WM_DPICHANGED:{
        CompleteDesktopWidgetTransition();
        if(g_widget_drag_source&&g_widget_moved)return 0;
        RECT *suggested=(RECT*)lp;
        POINT origin={suggested->left,suggested->top};
        PositionDesktopWidget(window,MonitorFromRect(suggested,MONITOR_DEFAULTTONEAREST),HIWORD(wp),&origin);
        return 0;
    }
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        CompleteDesktopWidgetTransition();
        if(IsWindowVisible(window)&&!g_widget_drag_source)
            PositionDesktopWidget(window,MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),GetDpiForWindow(window),
                g_widget_position_saved?&g_widget_position:NULL);
        return 0;
    case WM_DESTROY:
        CancelDesktopWidgetTransition();
        if(g_desktop_widget_font)DeleteObject(g_desktop_widget_font);
        g_desktop_widget_font=NULL;
        g_widget_tooltip=NULL;
        g_desktop_widget=NULL;
        return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

static void ShowDesktopWidget(void)
{
    if(!g_desktop_widget_enabled||g_update_handoff||g_allow_close){
        CancelDesktopWidgetTransition();return;
    }
    if(g_widget_transition.window&&IsWindowVisible(g_desktop_widget))return;
    if(!g_desktop_widget){
        WNDCLASSW cls={.style=CS_DROPSHADOW,.lpfnWndProc=DesktopWidgetProcedure,.hInstance=g_instance,
            .hCursor=LoadCursorW(NULL,IDC_ARROW),.lpszClassName=L"Tasklet.DesktopWidget"};
        RegisterClassW(&cls);
        g_desktop_widget=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_LAYERED,
            cls.lpszClassName,APP_TITLE L" · Widget",WS_POPUP|WS_CLIPCHILDREN,
            0,0,0,0,NULL,NULL,g_instance,NULL);
        if(!g_desktop_widget){CancelDesktopWidgetTransition();return;}
        wchar_t position[64],extra;
        GetPrivateProfileStringW(L"DesktopWidget",L"Position",L"",position,_countof(position),g_settings_path);
        g_widget_position_saved=swscanf(position,L"%ld,%ld%lc",&g_widget_position.x,&g_widget_position.y,&extra)==2;
    }
    HMONITOR monitor=g_widget_position_saved?MonitorFromPoint(g_widget_position,MONITOR_DEFAULTTONEAREST):
        MonitorFromWindow(g_window,MONITOR_DEFAULTTONEAREST);
    UINT dpi=MonitorFromWindow(g_desktop_widget,MONITOR_DEFAULTTONEAREST)==monitor?
        GetDpiForWindow(g_desktop_widget):g_dpi;
    PositionDesktopWidget(g_desktop_widget,monitor,dpi,g_widget_position_saved?&g_widget_position:NULL);
    SetWindowTextW(GetDlgItem(g_desktop_widget,WIDGET_HIDE),g_russian?L"Скрыть виджет":L"Hide widget");
    KillTimer(g_desktop_widget,WIDGET_FADE_TIMER);
    g_widget_fade_started=0;
    BOOL animate=CommentAnimationEnabled();
    if(!animate)CancelDesktopWidgetTransition();
    SetLayeredWindowAttributes(g_desktop_widget,0,g_widget_transition.image?0:animate?35:255,LWA_ALPHA);
    ShowWindow(g_desktop_widget,SW_SHOWNOACTIVATE);
    RefreshDesktopWidgetTask();
    UpdateTimer();
    InvalidateRect(g_desktop_widget,NULL,FALSE);
    if(StartDesktopWidgetTransition())return;
    BOOL fade=animate&&SetTimer(g_desktop_widget,WIDGET_FADE_TIMER,15,NULL);
    g_widget_fade_started=fade?GetTickCount64():0;
    SetLayeredWindowAttributes(g_desktop_widget,0,fade?35:255,LWA_ALPHA);
}
