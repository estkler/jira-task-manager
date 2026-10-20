/* One owned native image viewer; decoding holds its own ref after the feed closes. */
#define WM_IMAGE_READY (WM_APP+60)
typedef struct ImageViewerState {
    volatile LONG references;
    volatile LONG closed;
    HWND window;NativeImage *image;NativeImageBitmap bitmap;
    BOOL ready,waiting;wchar_t filename[256];
} ImageViewerState;
static HWND g_image_viewer;
static volatile LONG g_image_worker_active;
static DWORD WINAPI ImageViewerWorker(void *parameter);
static void ImageViewerRelease(ImageViewerState *state)
{
    if(!state||InterlockedDecrement(&state->references))return;
    NativeImageBitmapFree(&state->bitmap);NativeImageRelease(state->image);free(state);
}
static void ImageViewerStart(ImageViewerState *state)
{
    if(InterlockedCompareExchange(&g_image_worker_active,1,0)){state->waiting=TRUE;return;}
    state->waiting=FALSE;InterlockedIncrement(&state->references);
    HANDLE thread=CreateThread(NULL,0,ImageViewerWorker,state,0,NULL);
    if(thread)CloseHandle(thread);else{InterlockedExchange(&g_image_worker_active,0);state->ready=TRUE;ImageViewerRelease(state);}
}
static void ImageViewerFinish(ImageViewerState *state)
{
    if(state->window&&IsWindow(state->window)&&GetWindowLongPtrW(state->window,GWLP_USERDATA)==(LONG_PTR)state){
        state->ready=TRUE;InvalidateRect(state->window,NULL,FALSE);
    }
    ImageViewerRelease(state); /* Worker ownership transferred through the main window message. */
    InterlockedExchange(&g_image_worker_active,0);
    if(g_image_viewer&&IsWindow(g_image_viewer)){
        ImageViewerState *latest=(ImageViewerState*)GetWindowLongPtrW(g_image_viewer,GWLP_USERDATA);
        if(latest&&latest->waiting)ImageViewerStart(latest);
    }
}
static DWORD WINAPI ImageViewerWorker(void *parameter)
{
    ImageViewerState *state=parameter;
    if(!InterlockedCompareExchange(&state->closed,0,0))NativeImageDecodeWithCancel(state->image->encoded,state->image->length,1920,1440,&state->bitmap,NULL,NULL,&state->closed);
    if(InterlockedCompareExchange(&g_jira_closing,0,0)||!PostMessageW(g_window,WM_IMAGE_READY,0,(LPARAM)state)){InterlockedExchange(&g_image_worker_active,0);ImageViewerRelease(state);}
    return 0;
}
static LRESULT CALLBACK ImageViewerProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    ImageViewerState *state=(ImageViewerState*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_NCCREATE){state=((CREATESTRUCTW*)lp)->lpCreateParams;InterlockedIncrement(&state->references);state->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)state);return TRUE;}
    if(!state)return DefWindowProcW(window,message,wp,lp);
    switch(message){
    case WM_KEYDOWN:if(wp==VK_ESCAPE){DestroyWindow(window);return 0;}break;
    case WM_SIZE:InvalidateRect(window,NULL,FALSE);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        if(state->waiting&&!InterlockedCompareExchange(&g_image_worker_active,0,0))ImageViewerStart(state);
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);FillRect(dc,&client,g_panel_brush);
        RECT area={D(12),D(12),client.right-D(12),client.bottom-D(44)};
        if(state->ready&&state->bitmap.handle)NativeImageDraw(dc,&state->bitmap,area);
        else{
            HGDIOBJ font=SelectObject(dc,g_font);SetTextColor(dc,MutedColor());SetBkMode(dc,TRANSPARENT);
            DrawTextW(dc,state->ready?(g_russian?L"Не удалось открыть изображение":L"Cannot display this image"):
                (g_russian?L"Открытие изображения…":L"Opening image…"),-1,&area,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,font);
        }
        wchar_t caption[320];swprintf(caption,_countof(caption),L"%ls · %u × %u",state->filename,state->image->width,state->image->height);
        RECT footer={D(12),client.bottom-D(34),client.right-D(12),client.bottom-D(8)};HGDIOBJ font=SelectObject(dc,g_small_font);
        SetTextColor(dc,MutedColor());SetBkMode(dc,TRANSPARENT);DrawTextW(dc,caption,-1,&footer,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,font);
        EndPaint(window,&paint);return 0;
    }
    case WM_NCDESTROY:
        if(g_image_viewer==window)g_image_viewer=NULL;InterlockedExchange(&state->closed,1);state->window=NULL;SetWindowLongPtrW(window,GWLP_USERDATA,0);ImageViewerRelease(state);break;
    }
    return DefWindowProcW(window,message,wp,lp);
}
static HWND OpenCommentImageViewer(NativeImage *image,const wchar_t *filename)
{
    if(!image)return NULL;if(g_image_viewer&&IsWindow(g_image_viewer))DestroyWindow(g_image_viewer);
    ImageViewerState *state=calloc(1,sizeof(*state));if(!state)return NULL;state->references=1;state->image=image;NativeImageRetain(image);
    wcsncpy_s(state->filename,_countof(state->filename),filename&&*filename?filename:L"Image",_TRUNCATE);
    WNDCLASSW cls={0};cls.hInstance=g_instance;cls.lpfnWndProc=ImageViewerProcedure;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);cls.hIcon=LoadAppIcon(FALSE);cls.lpszClassName=L"Task Manager.ImageViewer";RegisterClassW(&cls);
    RECT owner;GetWindowRect(g_window,&owner);MONITORINFO monitor={sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(g_window,MONITOR_DEFAULTTONEAREST),&monitor);
    int width=min(D(840),monitor.rcWork.right-monitor.rcWork.left-D(24)),height=min(D(600),monitor.rcWork.bottom-monitor.rcWork.top-D(24));
    int x=max(monitor.rcWork.left,min((owner.left+owner.right-width)/2,monitor.rcWork.right-width));
    int y=max(monitor.rcWork.top,min((owner.top+owner.bottom-height)/2,monitor.rcWork.bottom-height));
    wchar_t title[320];swprintf(title,_countof(title),L"Jira Task Manager · %ls",state->filename);
    HWND window=CreateWindowExW(0,cls.lpszClassName,title,WS_OVERLAPPEDWINDOW,x,y,width,height,g_window,NULL,g_instance,state);
    if(!window){ImageViewerRelease(state);return NULL;}
    g_image_viewer=window;ApplyWindowCaptionTheme(window);ImageViewerStart(state);
    ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);ImageViewerRelease(state);return window;
}
