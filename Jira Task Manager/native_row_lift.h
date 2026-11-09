/* Visual-only interpolation of a confirmed start. Jira data and native list
   ordering are already authoritative; this bounded layer owns pixels and keys,
   never pointers into a replaceable task array. */
typedef struct RowLiftPosition {wchar_t key[80];RECT bounds;} RowLiftPosition;
typedef struct RowLiftPending {
    RowLiftPosition *rows;int count,total,top;RECT body;wchar_t key[80];
    UINT dpi;BOOL dark;int density;
} RowLiftPending;
typedef struct RowLiftTile {HBITMAP bitmap;int from,to,height;BOOL target;} RowLiftTile;
static struct {
    HWND canvas,actor;HBITMAP buffer;RowLiftTile *tiles;int count,target,width,height;
    RECT body;UINT dpi;BOOL dark;int density;ULONGLONG started;ProgressFeedback wave;
} g_row_lift;
static UINT g_row_lift_discard_up;
static BOOL RowLiftInput(UINT message,LPARAM parameter)
{
    UINT bit=message==WM_LBUTTONUP?MK_LBUTTON:message==WM_RBUTTONUP?MK_RBUTTON:0;
    if(bit&&(g_row_lift_discard_up&bit)){
        g_row_lift_discard_up&=~bit;if(GetCapture()==g_list)ReleaseCapture();return TRUE;
    }
    /* A lost capture may leave the orphaned mouse-up pending. Keep that guard,
       but never suppress the independent keyboard context-menu gesture. */
    if(message==WM_CONTEXTMENU&&parameter!=(LPARAM)-1&&(g_row_lift_discard_up&MK_RBUTTON)&&GetCapture()==g_list)return TRUE;
    if(message==WM_LBUTTONDOWN)g_row_lift_discard_up&=~MK_LBUTTON;
    if(message==WM_RBUTTONDOWN)g_row_lift_discard_up&=~MK_RBUTTON;
    return FALSE;
}
static void RowLiftCopy(HDC dc,HBITMAP bitmap,int x,int y,int width,int height)
{
    HDC source=CreateCompatibleDC(dc);if(!source)return;
    HGDIOBJ old=SelectObject(source,bitmap);BitBlt(dc,x,y,width,height,source,0,0,SRCCOPY);
    SelectObject(source,old);DeleteDC(source);
}
static double RowLiftPhase(ULONGLONG now)
{
    double phase=now>=g_row_lift.started?min(1.0,(double)(now-g_row_lift.started)/350.0):0;
    return phase*phase*(3-2*phase);
}
static int RowLiftY(const RowLiftTile *tile,double phase)
{return tile->from+(int)((tile->to-tile->from)*phase);}
static BOOL RowLiftBody(RECT *body)
{
    RECT header;if(!g_list||!GetClientRect(g_list,body)||!GetWindowRect(ListView_GetHeader(g_list),&header))return FALSE;
    MapWindowPoints(NULL,g_list,(POINT*)&header,2);body->top=max(0,header.bottom);
    return body->right>0&&body->bottom>body->top;
}
static LRESULT CALLBACK RowLiftProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_ERASEBKGND)return 1;
    /* Consume the dismissing click: forwarding it to the newly sorted list
       could activate a different issue beneath a still-moving visual row. */
    if(message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK||message==WM_RBUTTONDOWN){
        g_row_lift_discard_up|=message==WM_RBUTTONDOWN?MK_RBUTTON:MK_LBUTTON;
        SetCapture(g_list);RowLiftClear(FALSE);return 0;
    }
    if(message==WM_CANCELMODE){RowLiftClear(FALSE);return 0;}
    if(message==WM_MOUSEWHEEL){RowLiftClear(FALSE);return SendMessageW(g_list,message,wp,lp);}
    if(message==WM_PAINT){
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
        if(GetDlgCtrlID(window)==3051){
            if(g_row_lift.tiles&&g_row_lift.target>=0&&g_row_lift.target<g_row_lift.count){
                RowLiftTile *tile=&g_row_lift.tiles[g_row_lift.target];RowLiftCopy(dc,tile->bitmap,0,0,g_row_lift.width,tile->height);
            }
        }else if(g_row_lift.buffer){
            HDC memory=CreateCompatibleDC(dc);if(memory){
                HGDIOBJ old=SelectObject(memory,g_row_lift.buffer);RECT rect={0,0,g_row_lift.width,g_row_lift.height};FillRect(memory,&rect,g_panel_brush);
                double phase=RowLiftPhase(GetTickCount64());
                for(int i=0;i<g_row_lift.count;i++)if(!g_row_lift.tiles[i].target){
                    RowLiftTile *tile=&g_row_lift.tiles[i];RowLiftCopy(memory,tile->bitmap,0,RowLiftY(tile,phase),g_row_lift.width,tile->height);
                }
                BitBlt(dc,0,0,g_row_lift.width,g_row_lift.height,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteDC(memory);
            }
        }
        EndPaint(window,&paint);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
static void RowLiftClear(BOOL play_wave)
{
    if(!g_row_lift.tiles&&!g_row_lift.canvas&&!g_row_lift.wave.key[0])return;
    if(g_window)KillTimer(g_window,946);
    if(g_row_lift.canvas)DestroyWindow(g_row_lift.canvas);
    for(int i=0;i<g_row_lift.count;i++)if(g_row_lift.tiles[i].bitmap)DeleteObject(g_row_lift.tiles[i].bitmap);
    free(g_row_lift.tiles);if(g_row_lift.buffer)DeleteObject(g_row_lift.buffer);
    ProgressFeedback wave=g_row_lift.wave;memset(&g_row_lift,0,sizeof(g_row_lift));
    if(play_wave&&wave.key[0]&&CommentAnimationEnabled()){
        ProgressFeedbackClear();g_progress_feedback=wave;g_progress_feedback.started=GetTickCount64();
        if(ProgressFeedbackValid())SetTimer(g_window,945,16,NULL);else ProgressFeedbackClear();
    }else free(wave.previous);
    if(g_list)InvalidateRect(g_list,NULL,FALSE);
}
static RowLiftPending RowLiftPrepare(const wchar_t *key)
{
    RowLiftPending pending={0};
    if(!key[0]||!IsWindowVisible(g_window)||IsIconic(g_window)||!CommentAnimationEnabled()||!RowLiftBody(&pending.body))return pending;
    pending.total=ListView_GetItemCount(g_list);pending.top=ListView_GetTopIndex(g_list);
    int capacity=min(pending.total-pending.top,ListView_GetCountPerPage(g_list)+2);
    if(capacity<=0||capacity>128)return (RowLiftPending){0};
    pending.rows=calloc((size_t)capacity,sizeof(*pending.rows));if(!pending.rows)return (RowLiftPending){0};
    BOOL found=FALSE;
    for(int row=pending.top;row<pending.total&&pending.count<capacity;row++){
        RECT bounds;LVITEMW item={.mask=LVIF_PARAM,.iItem=row};
        if(!ListView_GetItem(g_list,&item)||item.lParam<0||item.lParam>=g_task_count||!ListView_GetItemRect(g_list,row,&bounds,LVIR_BOUNDS))break;
        if(bounds.top>=pending.body.bottom)break;if(bounds.bottom<=pending.body.top)continue;
        RowLiftPosition *position=&pending.rows[pending.count++];position->bounds=bounds;wcscpy_s(position->key,80,g_tasks[item.lParam].key);
        if(!wcscmp(key,position->key)&&bounds.top>=pending.body.top&&bounds.bottom<=pending.body.bottom)found=TRUE;
    }
    if(!found){free(pending.rows);return (RowLiftPending){0};}
    wcscpy_s(pending.key,80,key);pending.dpi=g_dpi;pending.dark=g_dark;pending.density=g_density;return pending;
}
static void RowLiftBegin(RowLiftPending *pending)
{
    RECT body;BOOL valid=pending->rows&&g_progress_feedback.key[0]&&!wcscmp(pending->key,g_progress_feedback.key)&&
        CommentAnimationEnabled()&&IsWindowVisible(g_window)&&!IsIconic(g_window)&&RowLiftBody(&body)&&EqualRect(&body,&pending->body)&&
        pending->total==ListView_GetItemCount(g_list)&&pending->top==ListView_GetTopIndex(g_list)&&
        pending->dpi==g_dpi&&pending->dark==g_dark&&pending->density==g_density;
    if(!valid){free(pending->rows);pending->rows=NULL;return;}
    /* Cap visible GDI pixel storage (including the reusable backing buffer).
       Large or changing viewports safely retain the ordinary amber reaction. */
    ULONGLONG pixels=(ULONGLONG)body.right*(body.bottom-body.top);
    if(pixels>6ULL*1024*1024){free(pending->rows);pending->rows=NULL;return;}
    RowLiftClear(FALSE);g_row_lift.tiles=calloc((size_t)pending->count,sizeof(*g_row_lift.tiles));
    if(!g_row_lift.tiles){free(pending->rows);pending->rows=NULL;return;}
    g_row_lift.body=body;g_row_lift.width=body.right;g_row_lift.height=body.bottom-body.top;g_row_lift.target=-1;
    g_row_lift.dpi=g_dpi;g_row_lift.dark=g_dark;g_row_lift.density=g_density;
    HDC target=GetDC(g_list);BOOL moved=FALSE;ULONGLONG tile_pixels=0;
    for(int n=0;n<pending->count;n++){
        int row=pending->top+n;LVITEMW item={.mask=LVIF_PARAM,.iItem=row};RECT bounds;
        if(!ListView_GetItem(g_list,&item)||item.lParam<0||item.lParam>=g_task_count||!ListView_GetItemRect(g_list,row,&bounds,LVIR_BOUNDS)){valid=FALSE;break;}
        const wchar_t *key=g_tasks[item.lParam].key;int old=-1;
        for(int i=0;i<pending->count;i++)if(!wcscmp(key,pending->rows[i].key)){old=i;break;}
        if(old<0){valid=FALSE;break;}
        RowLiftTile *tile=&g_row_lift.tiles[n];tile->height=bounds.bottom-bounds.top;tile->from=pending->rows[old].bounds.top-body.top;tile->to=bounds.top-body.top;
        if(tile->height<=0||tile->height!=pending->rows[old].bounds.bottom-pending->rows[old].bounds.top){valid=FALSE;break;}
        tile_pixels+=(ULONGLONG)g_row_lift.width*tile->height;if(tile_pixels+pixels>6ULL*1024*1024){valid=FALSE;break;}
        tile->target=!wcscmp(key,pending->key);
        if(tile->target){g_row_lift.target=n;moved=tile->to<tile->from&&bounds.top>=body.top&&bounds.bottom<=body.bottom;}
        HDC memory=CreateCompatibleDC(target);tile->bitmap=CreateCompatibleBitmap(target,g_row_lift.width,tile->height);g_row_lift.count=n+1;
        if(!memory||!tile->bitmap){if(memory)DeleteDC(memory);valid=FALSE;break;}
        HGDIOBJ old_bitmap=SelectObject(memory,tile->bitmap);SetViewportOrgEx(memory,0,-bounds.top,NULL);
        NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=memory;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;DrawListCustom(&draw);
        SelectObject(memory,old_bitmap);DeleteDC(memory);
    }
    if(valid&&moved)g_row_lift.buffer=CreateCompatibleBitmap(target,g_row_lift.width,g_row_lift.height);
    ReleaseDC(g_list,target);free(pending->rows);pending->rows=NULL;
    if(!valid||!moved||!g_row_lift.buffer){RowLiftClear(FALSE);return;}
    WNDCLASSW cls={0};cls.lpfnWndProc=RowLiftProcedure;cls.hInstance=g_instance;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);cls.lpszClassName=L"TaskManager.ConfirmedRowLift";RegisterClassW(&cls);
    g_row_lift.started=GetTickCount64();
    g_row_lift.canvas=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,body.top,g_row_lift.width,g_row_lift.height,g_list,(HMENU)3050,g_instance,NULL);
    RowLiftTile *tile=&g_row_lift.tiles[g_row_lift.target];
    if(g_row_lift.canvas)g_row_lift.actor=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,0,tile->from,g_row_lift.width,tile->height,g_row_lift.canvas,(HMENU)3051,g_instance,NULL);
    if(!g_row_lift.canvas||!g_row_lift.actor){RowLiftClear(FALSE);return;}
    g_row_lift.wave=g_progress_feedback;g_progress_feedback=(ProgressFeedback){0};KillTimer(g_window,945);
    SetWindowPos(g_row_lift.canvas,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(!SetTimer(g_window,946,16,NULL)){RowLiftClear(TRUE);return;}
    UpdateWindow(g_row_lift.canvas);UpdateWindow(g_row_lift.actor);
}
static void RowLiftTick(ULONGLONG now)
{
    if(!g_row_lift.canvas)return;
    RECT body;int target=TaskByKey(g_row_lift.wave.key);
    BOOL valid=target>=0&&g_tasks[target].active&&NativeStatusSameScope(&g_row_lift.wave.scope,&g_snapshot)&&
        IsWindowVisible(g_window)&&!IsIconic(g_window)&&RowLiftBody(&body)&&EqualRect(&body,&g_row_lift.body)&&
        g_row_lift.dpi==g_dpi&&g_row_lift.dark==g_dark&&g_row_lift.density==g_density;
    if(!valid||!CommentAnimationEnabled()){RowLiftClear(FALSE);return;}
    if(now-g_row_lift.started>=350){RowLiftClear(TRUE);return;}
    RowLiftTile *tile=&g_row_lift.tiles[g_row_lift.target];
    SetWindowPos(g_row_lift.actor,NULL,0,RowLiftY(tile,RowLiftPhase(now)),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    InvalidateRect(g_row_lift.canvas,NULL,FALSE);UpdateWindow(g_row_lift.canvas);UpdateWindow(g_row_lift.actor);
}
