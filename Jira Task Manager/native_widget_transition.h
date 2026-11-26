/* Temporary, click-through image of our own window. No desktop capture. */
enum { WIDGET_TRANSITION_TIMER = 2, WIDGET_TRANSITION_MS = 290, WIDGET_REVEAL_MS = 175,
    WIDGET_CONTENT_FADE_START_MS = 35, WIDGET_CONTENT_FADE_END_MS = 160 };

static struct {
    HWND window,system_transition_owner;
    HDC image_dc,frame_dc;
    HBITMAP image,frame;
    RECT from,to;
    POINT crop;
    ULONGLONG started;
} g_widget_transition;

static void CancelDesktopWidgetTransition(void)
{
    if(g_widget_transition.system_transition_owner){
        BOOL disable=FALSE;
        DwmSetWindowAttribute(g_widget_transition.system_transition_owner,DWMWA_TRANSITIONS_FORCEDISABLED,&disable,sizeof(disable));
    }
    if(g_desktop_widget)KillTimer(g_desktop_widget,WIDGET_TRANSITION_TIMER);
    if(g_widget_transition.window)DestroyWindow(g_widget_transition.window);
    if(g_widget_transition.image_dc)DeleteDC(g_widget_transition.image_dc);
    if(g_widget_transition.frame_dc)DeleteDC(g_widget_transition.frame_dc);
    if(g_widget_transition.image)DeleteObject(g_widget_transition.image);
    if(g_widget_transition.frame)DeleteObject(g_widget_transition.frame);
    memset(&g_widget_transition,0,sizeof(g_widget_transition));
}

static BOOL CaptureDesktopWidgetTransition(HWND main_window)
{
    if(!g_desktop_widget_enabled||g_allow_close||g_update_handoff||
       !IsWindowVisible(main_window)||!IsWindowEnabled(main_window)||IsIconic(main_window)||
       !CommentAnimationEnabled())return FALSE;
    HWND foreground=GetForegroundWindow();
    if(foreground&&GetAncestor(foreground,GA_ROOTOWNER)!=main_window)return FALSE;
    CancelDesktopWidgetTransition();
    RECT outer;
    if(!GetWindowRect(main_window,&outer))return FALSE;
    int width=outer.right-outer.left,height=outer.bottom-outer.top;
    if(width<=0||height<=0||width>4096||height>4096||(ULONGLONG)width*height>9000000)return FALSE;

    HDC screen=GetDC(NULL);
    if(!screen)return FALSE;
    g_widget_transition.image_dc=CreateCompatibleDC(screen);
    g_widget_transition.image=CreateCompatibleBitmap(screen,width,height);
    ReleaseDC(NULL,screen);
    if(g_widget_transition.image_dc&&g_widget_transition.image){
        HDC dc=g_widget_transition.image_dc;
        if(SelectObject(dc,g_widget_transition.image)){
            FillRect(dc,&(RECT){0,0,width,height},g_window_brush);
            SendMessageW(main_window,WM_PRINT,(WPARAM)dc,PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
            RECT frame,crop;
            if(SUCCEEDED(DwmGetWindowAttribute(main_window,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame)))&&
               IntersectRect(&crop,&frame,&outer))g_widget_transition.from=crop;
            else g_widget_transition.from=outer;
            g_widget_transition.crop=(POINT){g_widget_transition.from.left-outer.left,
                g_widget_transition.from.top-outer.top};
            return TRUE;
        }
    }
    CancelDesktopWidgetTransition();
    return FALSE;
}

static LONG InterpolateWidgetEdge(LONG start,LONG end,double amount)
{
    return (LONG)lround(start+(end-start)*amount);
}

static RECT DesktopWidgetTransitionBounds(RECT from,RECT to,unsigned elapsed)
{
    double fraction=(double)min(elapsed,WIDGET_TRANSITION_MS)/WIDGET_TRANSITION_MS;
    double left=1.0-fraction,amount=1.0-left*left*left;
    LONG x=InterpolateWidgetEdge(from.left,to.left,amount),y=InterpolateWidgetEdge(from.top,to.top,amount);
    LONG width=InterpolateWidgetEdge(from.right-from.left,to.right-to.left,amount);
    LONG height=InterpolateWidgetEdge(from.bottom-from.top,to.bottom-to.top,amount);
    return (RECT){x,y,x+width,y+height};
}

static BOOL DrawDesktopWidgetTransition(unsigned elapsed)
{
    RECT from=g_widget_transition.from;
    RECT bounds=DesktopWidgetTransitionBounds(from,g_widget_transition.to,elapsed);
    SIZE size={bounds.right-bounds.left,bounds.bottom-bounds.top};
    HDC frame=g_widget_transition.frame_dc;
    HDC image=g_widget_transition.image_dc;
    POINT crop=g_widget_transition.crop;
    int width=from.right-from.left,height=from.bottom-from.top;
    if(elapsed<=WIDGET_CONTENT_FADE_START_MS){
        if(!StretchBlt(frame,0,0,size.cx,size.cy,image,crop.x,crop.y,width,height,SRCCOPY))return FALSE;
    }else{
        FillRect(frame,&(RECT){0,0,size.cx,size.cy},g_window_brush);
        if(elapsed<WIDGET_CONTENT_FADE_END_MS){
            BYTE content_alpha=(BYTE)(255*(WIDGET_CONTENT_FADE_END_MS-elapsed)/
                (WIDGET_CONTENT_FADE_END_MS-WIDGET_CONTENT_FADE_START_MS));
            BLENDFUNCTION content={AC_SRC_OVER,0,content_alpha,0};
            if(!GdiAlphaBlend(frame,0,0,size.cx,size.cy,image,crop.x,crop.y,width,height,content))return FALSE;
        }
    }

    unsigned faded=max(elapsed,WIDGET_REVEAL_MS)-WIDGET_REVEAL_MS;
    BLENDFUNCTION blend={AC_SRC_OVER,0,(BYTE)(255-255*faded/(WIDGET_TRANSITION_MS-WIDGET_REVEAL_MS)),0};
    POINT top_left={bounds.left,bounds.top},origin={0,0};
    return UpdateLayeredWindow(g_widget_transition.window,NULL,&top_left,&size,frame,&origin,0,&blend,ULW_ALPHA);
}

static BOOL StartDesktopWidgetTransition(void)
{
    if(!g_widget_transition.image)return FALSE;
    RECT from=g_widget_transition.from,to;
    if(!GetWindowRect(g_desktop_widget,&to)||to.right<=to.left||to.bottom<=to.top)goto fail;
    g_widget_transition.to=to;
    HDC screen=GetDC(NULL);
    if(!screen)goto fail;
    g_widget_transition.frame_dc=CreateCompatibleDC(screen);
    g_widget_transition.frame=CreateCompatibleBitmap(screen,
        max(from.right-from.left,to.right-to.left),max(from.bottom-from.top,to.bottom-to.top));
    ReleaseDC(NULL,screen);
    if(!g_widget_transition.frame_dc||!g_widget_transition.frame||
       !SelectObject(g_widget_transition.frame_dc,g_widget_transition.frame))goto fail;
    /* Fast shrinking; the nearly opaque first frames preserve the contents. */
    SetStretchBltMode(g_widget_transition.frame_dc,COLORONCOLOR);

    WNDCLASSW registration={.lpfnWndProc=DefWindowProcW,.hInstance=g_instance,.lpszClassName=L"Tasklet.WidgetTransition"};
    RegisterClassW(&registration);
    g_widget_transition.window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT,
        registration.lpszClassName,L"",WS_POPUP,from.left,from.top,0,0,NULL,NULL,g_instance,NULL);
    if(!g_widget_transition.window||!DrawDesktopWidgetTransition(0)||
       !SetTimer(g_desktop_widget,WIDGET_TRANSITION_TIMER,15,NULL))goto fail;
    g_widget_transition.started=GetTickCount64();
    if(!SetWindowPos(g_widget_transition.window,HWND_TOPMOST,0,0,0,0,
        SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW))goto fail;
    return TRUE;

fail:
    CancelDesktopWidgetTransition();
    return FALSE;
}

static void CompleteDesktopWidgetTransition(void)
{
    if(!g_widget_transition.window)return;
    CancelDesktopWidgetTransition();
    SetLayeredWindowAttributes(g_desktop_widget,0,255,LWA_ALPHA);
}

static void DesktopWidgetTransitionTick(ULONGLONG now)
{
    if(!g_widget_transition.window)return;
    unsigned elapsed=(unsigned)min(now-g_widget_transition.started,(ULONGLONG)WIDGET_TRANSITION_MS);
    if(elapsed==WIDGET_TRANSITION_MS||!DrawDesktopWidgetTransition(elapsed)){
        CompleteDesktopWidgetTransition();
    }else if(elapsed>=WIDGET_REVEAL_MS){
        BYTE alpha=(BYTE)(255*(elapsed-WIDGET_REVEAL_MS)/(WIDGET_TRANSITION_MS-WIDGET_REVEAL_MS));
        SetLayeredWindowAttributes(g_desktop_widget,0,alpha,LWA_ALPHA);
    }
}
