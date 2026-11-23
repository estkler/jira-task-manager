/* Read-only native card; opening acknowledges only the copied transition. */
static JiraSnapshot g_status_mark_scope;
static RECT StatusMarkCloseRect(HWND window){RECT r;GetClientRect(window,&r);return (RECT){r.right-D(32),D(3),r.right-D(4),D(31)};}
static void DrawStatusMarkDetail(HWND window,HDC dc)
{
    RECT rect;GetClientRect(window,&rect);FillRect(dc,&rect,g_popover_brush);SetBkMode(dc,TRANSPARENT);
    RECT header={1,1,rect.right-1,D(34)};HBRUSH brush=CreateSolidBrush(PopoverHeaderColor());FillRect(dc,&header,brush);DeleteObject(brush);
    brush=CreateSolidBrush(AccentColor());RECT accent={D(12),D(11),D(15),D(24)};FillRect(dc,&accent,brush);DeleteObject(brush);
    HGDIOBJ font=SelectObject(dc,g_bold_font);SetTextColor(dc,TextColor());
    RECT title={D(22),D(3),rect.right-D(35),D(31)};DrawTextW(dc,g_russian?L"Статус изменён":L"Status changed",-1,&title,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    SelectObject(dc,g_small_font);SetTextColor(dc,MutedColor());RECT key={D(13),D(43),rect.right-D(13),D(61)};
    DrawTextW(dc,g_status_mark_shown.key,-1,&key,DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(dc,g_font);SetTextColor(dc,TextColor());wchar_t text[272];swprintf(text,272,L"%ls → %ls",g_status_mark_shown.previous,g_status_mark_shown.current);
    RECT body={D(13),D(65),rect.right-D(13),rect.bottom-D(12)};DrawTextW(dc,text,-1,&body,DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,font);
    RECT close=StatusMarkCloseRect(window);POINT point;GetCursorPos(&point);ScreenToClient(window,&point);
    if(PtInRect(&close,point)){brush=CreateSolidBrush(g_dark?RGB(48,75,103):RGB(213,231,249));FillRect(dc,&close,brush);DeleteObject(brush);}
    int x=(close.left+close.right)/2,y=(close.top+close.bottom)/2;HPEN pen=CreatePen(PS_SOLID,max(1,D(1)),MutedColor());HGDIOBJ old=SelectObject(dc,pen);
    MoveToEx(dc,x-D(4),y-D(4),NULL);LineTo(dc,x+D(5),y+D(5));MoveToEx(dc,x-D(4),y+D(4),NULL);LineTo(dc,x+D(5),y-D(5));SelectObject(dc,old);DeleteObject(pen);
    HRGN frame=CreateRoundRectRgn(0,0,rect.right+1,rect.bottom+1,D(10),D(10));brush=CreateSolidBrush(PopoverBorderColor());FrameRgn(dc,frame,brush,1,1);DeleteObject(brush);DeleteObject(frame);
}
static void CloseStatusMark(void){HWND window=g_status_mark_window;g_status_mark_window=NULL;if(window&&IsWindow(window))DestroyWindow(window);}
static LRESULT CALLBACK StatusMarkProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_CLOSE||(message==WM_ACTIVATE&&LOWORD(wp)==WA_INACTIVE)||(message==WM_KEYDOWN&&wp==VK_ESCAPE)){CloseStatusMark();return 0;}
    if(message==WM_LBUTTONUP){RECT r=StatusMarkCloseRect(window);POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(PtInRect(&r,p)){CloseStatusMark();return 0;}}
    if(message==WM_MOUSEMOVE){TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);RECT r=StatusMarkCloseRect(window);InvalidateRect(window,&r,FALSE);}
    if(message==WM_MOUSELEAVE){RECT r=StatusMarkCloseRect(window);InvalidateRect(window,&r,FALSE);}
    if(message==WM_PAINT){PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);DrawStatusMarkDetail(window,dc);EndPaint(window,&paint);return 0;}
    if(message==WM_NCDESTROY&&g_status_mark_window==window)g_status_mark_window=NULL;
    return DefWindowProcW(window,message,wp,lp);
}
static void OpenStatusMark(int row)
{
    int task=TaskIndexAtRow(row);NativeStatusMark *mark=task>=0&&task<g_task_count?StatusMarkForTask(&g_tasks[task]):NULL;RECT dot;
    if(!mark||!StatusMarkRectForRow(row,&dot))return;
    NativeStatusMark shown=*mark;CloseStatusMark();g_status_mark_shown=shown;
    g_status_mark_scope=(JiraSnapshot){.identity=g_snapshot.identity};wcscpy_s(g_status_mark_scope.server,800,g_snapshot.server);
    WNDCLASSW cls={.lpfnWndProc=StatusMarkProcedure,.hInstance=g_instance,.hCursor=LoadCursorW(NULL,IDC_ARROW),.lpszClassName=L"Tasklet.StatusChangeDetail"};RegisterClassW(&cls);
    MapWindowPoints(g_list,NULL,(POINT*)&dot,2);MONITORINFO monitor={.cbSize=sizeof(monitor)};GetMonitorInfoW(MonitorFromRect(&dot,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT bounds;GetWindowRect(g_window,&bounds);if(!IntersectRect(&bounds,&bounds,&monitor.rcWork))bounds=monitor.rcWork;
    int width=min(D(270),bounds.right-bounds.left-D(12));if(width<D(180)){bounds=monitor.rcWork;width=min(D(270),bounds.right-bounds.left-D(12));}
    HDC dc=GetDC(g_list);HGDIOBJ font=SelectObject(dc,g_font);wchar_t text[272];swprintf(text,272,L"%ls → %ls",shown.previous,shown.current);
    RECT body={0,0,width-D(26),0};DrawTextW(dc,text,-1,&body,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,font);ReleaseDC(g_list,dc);
    int height=max(D(98),body.bottom+D(79)),x=max(bounds.left+D(6),min(dot.left,bounds.right-width-D(6)));
    int y=dot.bottom+D(4);if(y+height>bounds.bottom)y=dot.top-height-D(4);y=max(monitor.rcWork.top,min(y,monitor.rcWork.bottom-height));
    g_status_mark_window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"",WS_POPUP,x,y,width,height,g_window,NULL,g_instance,NULL);
    if(!g_status_mark_window)return;
    SetWindowRgn(g_status_mark_window,CreateRoundRectRgn(0,0,width+1,height+1,D(10),D(10)),FALSE);
    ShowWindow(g_status_mark_window,SW_SHOWNORMAL);SetFocus(g_status_mark_window);
    if(NativeStatusSameScope(&g_status_mark_scope,&g_status_marks.scope)&&NativeStatusSameScope(&g_status_mark_scope,&g_snapshot)&&
       !NativeStatusMarksAcknowledge(&g_status_marks,g_settings_path,&shown)){
        CloseStatusMark();MessageBoxW(g_window,g_russian?L"Не удалось сохранить просмотр изменения. Отметка пока останется.":L"Could not save acknowledgement. The mark will remain.",APP_TITLE,MB_OK|MB_ICONWARNING);
    }
    InvalidateRect(g_list,NULL,FALSE);
}
