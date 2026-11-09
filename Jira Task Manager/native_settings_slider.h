/* Shared pointer/paint behavior, retaining the native trackbar class and key provider. */
static HWND s_slider_drag,s_slider_hot;
static int s_slider_grab_offset;
static RECT SettingsSliderChannel(HWND window)
{
    RECT track;SendMessageW(window,TBM_GETCHANNELRECT,0,(LPARAM)&track);return track;
}
static int SettingsSliderCenterX(HWND window,RECT track)
{
    int low=(int)SendMessageW(window,TBM_GETRANGEMIN,0,0),high=(int)SendMessageW(window,TBM_GETRANGEMAX,0,0);
    int position=(int)SendMessageW(window,TBM_GETPOS,0,0);
    return track.left+(high>low?MulDiv(track.right-track.left,position-low,high-low):0);
}
static void SettingsSliderNotify(HWND window,UINT action)
{
    int position=(int)SendMessageW(window,TBM_GETPOS,0,0);
    SendMessageW(GetParent(window),WM_HSCROLL,MAKEWPARAM(action,position),(LPARAM)window);
}
static void SettingsSliderPoint(HWND window,int x)
{
    RECT track=SettingsSliderChannel(window);int span=track.right-track.left;
    int low=(int)SendMessageW(window,TBM_GETRANGEMIN,0,0),high=(int)SendMessageW(window,TBM_GETRANGEMAX,0,0);
    int step=GetDlgCtrlID(window)==212?5:1,steps=(high-low)/step;
    if(span<=0||steps<=0)return;
    x=max(track.left,min(track.right,x));
    int position=low+(int)(((LONGLONG)(x-track.left)*steps+span/2)/span)*step;
    int previous=(int)SendMessageW(window,TBM_GETPOS,0,0);
    SendMessageW(window,TBM_SETPOS,TRUE,position);SettingsSliderNotify(window,TB_THUMBTRACK);
    if(previous!=position)NotifyWinEvent(EVENT_OBJECT_VALUECHANGE,window,OBJID_CLIENT,CHILDID_SELF);
}
static void SettingsSliderStop(HWND window)
{
    if(s_slider_drag==window){s_slider_drag=NULL;s_slider_grab_offset=0;if(GetCapture()==window)ReleaseCapture();}
    InvalidateRect(window,NULL,FALSE);
}
static void SettingsSliderRound(HDC dc,RECT r,int radius,COLORREF color)
{
    HBRUSH brush=CreateSolidBrush(color);HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(brush);
}
static void PaintSettingsSliderContent(HWND window,HDC dc)
{
    RECT client;GetClientRect(window,&client);HBRUSH background=CreateSolidBrush(WindowColor());FillRect(dc,&client,background);DeleteObject(background);
    RECT track=SettingsSliderChannel(window);if(track.right<=track.left)return;
    int y=(track.top+track.bottom)/2,x=SettingsSliderCenterX(window,track),half=max(1,S(2)),radius=max(3,S(5));
    BOOL enabled=IsWindowEnabled(window),active=enabled&&(s_slider_hot==window||s_slider_drag==window);
    COLORREF rail=g_dark?RGB(76,80,86):RGB(210,215,222),accent=enabled?AccentColor():MutedColor();
    RECT bar={track.left,y-half,track.right,y+half};SettingsSliderRound(dc,bar,2*half,rail);
    if(x>track.left){bar.right=x;SettingsSliderRound(dc,bar,2*half,accent);}
    if(active){RECT halo={x-radius-S(3),y-radius-S(3),x+radius+S(3)+1,y+radius+S(3)+1};SettingsSliderRound(dc,halo,2*(radius+S(3))+1,g_dark?RGB(42,65,81):RGB(220,237,252));}
    if(enabled&&GetFocus()==window&&!(SendMessageW(window,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
        int outer=radius+S(4);RECT ring={x-outer,y-outer,x+outer+1,y+outer+1};SettingsSliderRound(dc,ring,2*outer+1,accent);
        InflateRect(&ring,-max(1,S(1)),-max(1,S(1)));SettingsSliderRound(dc,ring,2*outer+1,WindowColor());
    }
    RECT thumb={x-radius,y-radius,x+radius+1,y+radius+1};SettingsSliderRound(dc,thumb,2*radius+1,accent);
    {
        int low=(int)SendMessageW(window,TBM_GETRANGEMIN,0,0),high=(int)SendMessageW(window,TBM_GETRANGEMAX,0,0);
        int steps=(high-low)/(GetDlgCtrlID(window)==212?5:1);
        if(steps<=0)return;
        int tick_y=y+radius+S(5),tick_width=max(1,S(1));
        HBRUSH ticks=CreateSolidBrush(enabled?MutedColor():rail);
        for(int i=0;i<=steps;i++){
            int tick_x=track.left+MulDiv(track.right-track.left,i,steps);
            RECT mark={tick_x-tick_width/2,tick_y,tick_x-tick_width/2+tick_width,tick_y+S(3)};
            FillRect(dc,&mark,ticks);
        }
        DeleteObject(ticks);
    }
}
static void PaintSettingsSlider(HWND window,HDC dc)
{
    RECT client;GetClientRect(window,&client);int width=client.right,height=client.bottom;
    if(width<=0||height<=0)return;
    HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=memory?CreateCompatibleBitmap(dc,width,height):NULL;
    if(!memory||!bitmap){if(memory)DeleteDC(memory);PaintSettingsSliderContent(window,dc);return;}
    HGDIOBJ previous=SelectObject(memory,bitmap);
    if(!previous||previous==HGDI_ERROR){DeleteObject(bitmap);DeleteDC(memory);PaintSettingsSliderContent(window,dc);return;}
    PaintSettingsSliderContent(window,memory);
    BitBlt(dc,0,0,width,height,memory,0,0,SRCCOPY);
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);
}
static LRESULT CALLBACK SettingsSliderProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)data;
    switch(message){
    case TBM_SETPOS:case WM_KEYDOWN:case WM_MOUSEWHEEL:{
        /* Never expose the native partial repaint before our complete frame. */
        LRESULT result=DefSubclassProc(window,message,message==TBM_SETPOS?FALSE:wp,lp);
        /* Native damage covers the stock thumb, not our halo/selected rail. */
        if(message!=TBM_SETPOS||wp)InvalidateRect(window,NULL,FALSE);
        return result;
    }
    case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:{
        if(!IsWindowEnabled(window))return 0;
        SetFocus(window);RECT track=SettingsSliderChannel(window);int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        int center=SettingsSliderCenterX(window,track),radius=max(3,S(5));
        s_slider_grab_offset=abs(x-center)<=radius&&abs(y-(track.top+track.bottom)/2)<=radius?x-center:0;
        s_slider_drag=window;SetCapture(window);SettingsSliderPoint(window,x-s_slider_grab_offset);InvalidateRect(window,NULL,FALSE);return 0;
    }
    case WM_MOUSEMOVE:
        if(s_slider_drag==window){
            if(!IsWindowEnabled(window)||GetCapture()!=window||!(wp&MK_LBUTTON)){SettingsSliderStop(window);return 0;}
            SettingsSliderPoint(window,(short)LOWORD(lp)-s_slider_grab_offset);return 0;
        }
        if(s_slider_hot!=window){s_slider_hot=window;TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);InvalidateRect(window,NULL,FALSE);}return 0;
    case WM_LBUTTONUP:
        if(s_slider_drag==window){if(IsWindowEnabled(window)&&GetCapture()==window)SettingsSliderPoint(window,(short)LOWORD(lp)-s_slider_grab_offset);SettingsSliderStop(window);SettingsSliderNotify(window,TB_THUMBPOSITION);SettingsSliderNotify(window,TB_ENDTRACK);}return 0;
    case WM_MOUSELEAVE:if(s_slider_hot==window)s_slider_hot=NULL;InvalidateRect(window,NULL,FALSE);return 0;
    case WM_CANCELMODE:SettingsSliderStop(window);return 0;
    case WM_CAPTURECHANGED:if(s_slider_drag==window){s_slider_drag=NULL;s_slider_grab_offset=0;InvalidateRect(window,NULL,FALSE);}break;
    case WM_KILLFOCUS:SettingsSliderStop(window);break;
    case WM_SHOWWINDOW:if(!wp)SettingsSliderStop(window);break;
    case WM_ENABLE:if(!wp)SettingsSliderStop(window);InvalidateRect(window,NULL,FALSE);break;
    case WM_SIZE:SettingsSliderStop(window);break;
    case WM_SETFOCUS:case WM_THEMECHANGED:case WM_UPDATEUISTATE:InvalidateRect(window,NULL,FALSE);break;
    case WM_ERASEBKGND:return 1;
    case WM_PRINTCLIENT:PaintSettingsSlider(window,(HDC)wp);return 0;
    case WM_PAINT:
        if(wp)PaintSettingsSlider(window,(HDC)wp);
        else{PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);PaintSettingsSlider(window,dc);EndPaint(window,&paint);}return 0;
    case WM_NCDESTROY:
        SettingsSliderStop(window);if(s_slider_hot==window)s_slider_hot=NULL;RemoveWindowSubclass(window,SettingsSliderProcedure,id);break;
    }
    return DefSubclassProc(window,message,wp,lp);
}
