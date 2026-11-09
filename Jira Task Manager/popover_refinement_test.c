#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include "native_notes.h"
static int note_save_calls;
static int note_error_dialogs;
static int WINAPI FixtureMessageBox(HWND owner,LPCWSTR text,LPCWSTR caption,UINT type){note_error_dialogs++;return IDOK;}
static BOOL FixtureNoteSave(const wchar_t *path,const wchar_t *account,const wchar_t *key,const wchar_t *text){
    note_save_calls++;return NativeNoteSave(path,account,key,text);
}
static HDC capture_dc;
static int detach_geometry_checks;
static BOOL fixture_cursor_active;
static POINT fixture_cursor;
static BOOL WINAPI FixtureCursor(LPPOINT point){if(fixture_cursor_active){*point=fixture_cursor;return TRUE;}return GetCursorPos(point);}
static BOOL WINAPI FixtureSetWindowPos(HWND,HWND,int,int,int,int,UINT);
static HDC WINAPI FixtureBeginPaint(HWND window,LPPAINTSTRUCT paint){
    if(!capture_dc)return BeginPaint(window,paint);
    ZeroMemory(paint,sizeof(*paint));paint->hdc=capture_dc;GetClientRect(window,&paint->rcPaint);return capture_dc;
}
static BOOL WINAPI FixtureEndPaint(HWND window,const PAINTSTRUCT *paint){return capture_dc?TRUE:EndPaint(window,paint);}
static wchar_t opened_url[1024];static HWND opened_owner;
static BOOL FixtureUrl(wchar_t *url,size_t capacity){wcscpy_s(url,capacity,L"https://jira.example.test/jira/");return TRUE;}
static HINSTANCE WINAPI FixtureOpen(HWND owner,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show){
    (void)parameters;(void)directory;(void)show;assert(!wcscmp(operation,L"open"));opened_owner=owner;wcscpy_s(opened_url,1024,file);return (HINSTANCE)33;
}
static BOOL WINAPI FixtureHide(HWND window,INT show){(void)window;(void)show;return TRUE;}
#define JiraGetUrl FixtureUrl
#define ShellExecuteW FixtureOpen
#define ShowWindow FixtureHide
#define NativeNoteSave FixtureNoteSave
#define MessageBoxW FixtureMessageBox
#define BeginPaint FixtureBeginPaint
#define EndPaint FixtureEndPaint
#define SetWindowPos FixtureSetWindowPos
#define GetCursorPos FixtureCursor
#include "native.c"
#undef ShowWindow
#undef NativeNoteSave
#undef MessageBoxW
#undef BeginPaint
#undef EndPaint
#undef SetWindowPos
#undef GetCursorPos
#include "image_fixture.h"
static BOOL WINAPI FixtureSetWindowPos(HWND window,HWND after,int x,int y,int width,int height,UINT flags){
    BOOL result=SetWindowPos(window,after,x,y,width,height,flags);
    if(window==g_popover.window&&g_popover.detached&&g_popover.transitioning&&!(flags&SWP_NOSIZE)){
        RECT client;GetClientRect(window,&client);HRGN region=CreateRectRgn(0,0,0,0);
        assert(GetWindowRgn(window,region)!=ERROR);
        /* The first detached resize must already have its rectangular region;
           the attached pointer must never survive until the next layout. */
        assert(PtInRegion(region,D(2),client.bottom/2));
        assert(PtInRegion(region,client.right-D(3),client.bottom/2));
        DeleteObject(region);detach_geometry_checks++;
    }
    return result;
}
static RECT relative_bounds(HWND parent,HWND child){RECT rect;assert(GetWindowRect(child,&rect));MapWindowPoints(NULL,parent,(POINT*)&rect,2);return rect;}
static void test_link(void){
    HWND key=GetDlgItem(g_popover.window,1045);assert(key);
    wchar_t label[80];GetWindowTextW(key,label,80);assert(!wcscmp(label,L"PDES-6513"));
    SetWindowTextW(g_popover.input,L"Keep this draft");HWND popup=g_popover.window;
    SendMessageW(popup,WM_COMMAND,MAKEWPARAM(1045,BN_CLICKED),(LPARAM)key);
    assert(!wcscmp(opened_url,L"https://jira.example.test/jira/browse/PDES-6513"));assert(opened_owner==popup);
    GetWindowTextW(g_popover.input,label,80);assert(!wcscmp(label,L"Keep this draft"));assert(!IsWindowVisible(g_window));
}
static void test_unpin(void){
    g_popover.pointer_left=TRUE;LayoutTaskPopoverContents();
    HWND popup=g_popover.window;RECT before,after;GetWindowRect(popup,&before);
    SendMessageW(popup,WM_COMMAND,1043,0);TaskPopover *panel=FindDetachedPopover(1,L"PDES-6513");assert(panel&&panel->pinned);
    GetWindowRect(popup,&after);assert(EqualRect(&before,&after)&&!panel->detached&&"Pin is not an undock command");
    GetWindowRect(popup,&before);SendMessageW(popup,WM_COMMAND,1043,0);assert(!panel->pinned);
    assert(GetWindow(popup,GW_OWNER)==NULL);assert(!IsWindowVisible(g_window));GetWindowRect(popup,&after);assert(EqualRect(&before,&after));
    assert(!SnapDetachedPopover(panel)); /* No snap target while the main list is hidden. */
    assert(AttachTaskPopover(panel,FALSE));assert(GetWindow(popup,GW_OWNER)==g_window);
}
static void test_composer(void){
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int width=280;width<=1000;width+=360){
        g_dpi=dpi;g_russian=ru;UpdateFonts();RefreshTaskPopover();SetWindowPos(g_popover.window,NULL,0,0,D(width),D(360),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutTaskPopoverContents();RECT input=relative_bounds(g_popover.window,g_popover.input),send=relative_bounds(g_popover.window,g_popover.send),feed=relative_bounds(g_popover.window,g_popover.text);
        assert(send.top>=input.top&&send.bottom<=input.bottom);assert(input.right+D(4)<=send.left);
        assert(send.right<=g_popover.body_right-D(17));assert(feed.bottom>=input.top-D(20));
        assert(feed.bottom<input.top&&input.bottom<=D(350));assert(input.right-input.left>=D(100));
    }
}
static void test_media(void){
    g_dpi=96;UpdateFonts();g_popover.loading=FALSE;g_popover.comment_scroll=0;g_popover.comments.count=1;g_popover.comments.items=calloc(1,sizeof(JiraComment));
    JiraComment *comment=g_popover.comments.items;wcscpy_s(comment->author,256,L"Colleague");wcscpy_s(comment->body,2048,L"Comment");
    comment->image_count=1;comment->images=calloc(1,sizeof(JiraCommentImage));size_t length;unsigned char *bytes=ImageFixtureBytes(300,160,&length);
    comment->images[0].image=NativeImageCreate(bytes,length);assert(comment->images[0].image);
    SetWindowPos(g_popover.text,NULL,0,0,D(1000),D(300),SWP_NOZORDER|SWP_NOACTIVATE);HDC dc=GetDC(g_popover.text);
    int top=D(8),image_top=max(top+D(53),top+D(31)+CommentTextHeight(dc,comment,D(1000))+D(8));ReleaseDC(g_popover.text,dc);
    JiraCommentImage *image=NULL;CommentMediaHit(g_popover.text,(POINT){D(55),image_top+D(8)},&image);assert(image==comment->images);
    CommentMediaHit(g_popover.text,(POINT){D(500),image_top+D(8)},&image);assert(!image);
    HDC screen=GetDC(NULL),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(1000),D(300));HGDIOBJ previous=SelectObject(memory,bitmap);
    PaintCommentList(g_popover.text,memory,(RECT){0,0,D(1000),D(300)});GdiFlush();
    assert(GetPixel(memory,D(55),image_top+D(8))!=PopoverColor()); /* Real pixels follow the text's left edge. */
    assert(GetPixel(memory,D(500),image_top+D(8))==PopoverColor());
    NativeImageRelease(comment->images[0].image);bytes=ImageFixtureBytes(20,800,&length);comment->images[0].image=NativeImageCreate(bytes,length);assert(comment->images[0].image);
    wcscpy_s(comment->images[0].filename,256,L"Tall.png");
    PaintCommentList(g_popover.text,memory,(RECT){0,0,D(1000),D(300)});GdiFlush();
    assert(GetPixel(memory,D(55),image_top+D(8))!=PopoverColor());
    /* A tall thumbnail needs a usable caption, without centering its pixels. */
    assert(GetPixel(memory,D(100),image_top+CommentImageHeight(&comment->images[0],D(937))-1)==BorderColor());
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);
    const UINT dimensions[][2]={{16,1600},{800,20},{2,1}};
    /* Empty-body pictures, extreme portraits/panoramas and small images remain
       left aligned and their captions/hits stay bounded as the window grows. */
    comment->body[0]=0;
    for(int shape=0;shape<3;shape++){
        NativeImageRelease(comment->images[0].image);bytes=ImageFixtureBytes(dimensions[shape][0],dimensions[shape][1],&length);
        comment->images[0].image=NativeImageCreate(bytes,length);assert(comment->images[0].image);
        for(int dpi=96;dpi<=192;dpi+=96)for(int width=220;width<=1000;width+=390){
            g_dpi=dpi;UpdateFonts();SetWindowPos(g_popover.text,NULL,0,0,D(width),D(320),SWP_NOZORDER|SWP_NOACTIVATE);
            RECT client;GetClientRect(g_popover.text,&client);screen=GetDC(NULL);memory=CreateCompatibleDC(screen);bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom);previous=SelectObject(memory,bitmap);
            PaintCommentList(g_popover.text,memory,client);GdiFlush();
            assert(GetPixel(memory,D(49)+D(4),D(43)+D(4))!=PopoverColor());
            CommentMediaHit(g_popover.text,(POINT){D(54),D(48)},&image);assert(image==comment->images);
            RECT slot=CommentImageSlotRect(comment->images,D(49),D(43),client.right-D(63));
            assert(slot.right<=client.right-D(14));assert(slot.right-slot.left>=D(96));
            CommentMediaHit(g_popover.text,(POINT){slot.right+D(1),D(48)},&image);assert(!image);
            dc=GetDC(g_popover.text);assert(CommentCardHeight(dc,comment,client.right)>=slot.bottom-D(8)+D(8));ReleaseDC(g_popover.text,dc);
            SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);
        }
    }
    g_dpi=96;UpdateFonts();
}
static void test_glyph(void){
    int geometry_before=detach_geometry_checks;
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_dark=dark;RecreateBrushes();UpdateFonts();LayoutTaskPopoverContents();
        HWND popup=g_popover.window,edit=g_popover.input;assert(popup&&edit);
        SetWindowTextW(edit,L"Retained pin draft");
        HDC screen=GetDC(NULL),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(26),D(26));HGDIOBJ previous=SelectObject(memory,bitmap);
        DRAWITEMSTRUCT draw={.CtlID=1043,.hwndItem=g_popover.pin_button,.hDC=memory,.rcItem={0,0,D(26),D(26)}};
        COLORREF unpinned[3];
        for(int hover=0;hover<3;hover++){
            draw.itemState=hover==2?ODS_SELECTED:0;
            g_hover_button=hover?draw.hwndItem:NULL;SendMessageW(popup,WM_DRAWITEM,1043,(LPARAM)&draw);GdiFlush();
            unpinned[hover]=GetPixel(memory,D(3),D(13));
            assert(unpinned[hover]==PopoverHeaderColor()&&"No background fill at rest, hover or mouse press");
        }
        SendMessageW(popup,WM_COMMAND,1043,0);TaskPopover *panel=FindDetachedPopover(1,L"PDES-6513");assert(panel&&panel->pinned);
        assert(panel->input==edit&&GetWindow(popup,GW_OWNER)==NULL);
        assert(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST);
        for(int hover=0;hover<3;hover++){
            draw.itemState=hover==2?ODS_SELECTED:0;
            g_hover_button=hover?draw.hwndItem:NULL;SendMessageW(popup,WM_DRAWITEM,1043,(LPARAM)&draw);GdiFlush();
            assert(GetPixel(memory,D(3),D(13))==unpinned[hover]&&"Pin state must not add a background tile");
            assert(GetPixel(memory,D(13),D(10))==PopoverHeaderColor()&&"The pin itself must remain an outline");
            assert(GetPixel(memory,D(9),D(8))!=PopoverHeaderColor()&&"Pinned icon must be upright");
        }
        SendMessageW(popup,WM_COMMAND,1043,0);assert(!panel->pinned);
        assert(!(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST));
        g_hover_button=NULL;draw.itemState=0;SendMessageW(popup,WM_DRAWITEM,1043,(LPARAM)&draw);GdiFlush();
        assert(GetPixel(memory,D(3),D(13))==unpinned[0]);
        assert(GetPixel(memory,D(9),D(8))==PopoverHeaderColor()&&"Unpinned icon must be tilted");
        wchar_t retained[80];GetWindowTextW(edit,retained,80);assert(!wcscmp(retained,L"Retained pin draft"));
        assert(AttachTaskPopover(panel,FALSE));
        SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);
    }
    assert(detach_geometry_checks==geometry_before);g_hover_button=NULL;g_dpi=96;g_dark=FALSE;RecreateBrushes();UpdateFonts();
}
static void test_pinned_snap(void){
    if(g_popover.window)CloseTaskPopover();g_dpi=96;UpdateFonts();
    OpenTaskPopover(0,1,(RECT){200,30,215,45});HWND popup=g_popover.window,edit=g_popover.input;
    SetWindowTextW(edit,L"Pinned snap draft");SendMessageW(popup,WM_COMMAND,1043,0);
    TaskPopover *panel=FindDetachedPopover(1,L"PDES-6513");assert(panel&&panel->pinned);
    g_active_popover=panel;assert(DetachTaskPopover()==panel);g_active_popover=&g_attached_popover;
    OpenTaskPopover(0,2,(RECT){200,30,215,45});HWND other=g_popover.window;assert(other&&other!=popup);
    RECT icon,bounds;assert(FindPopoverAnchorRect(panel,&icon));GetWindowRect(popup,&bounds);
    SetWindowPos(popup,NULL,icon.right,(icon.top+icon.bottom)/2-D(48),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    ShowWindow(g_window,SW_SHOWNOACTIVATE);
    LONG_PTR snap_style_before=GetWindowLongPtrW(popup,GWL_EXSTYLE);
    assert(SnapDetachedPopover(panel)&&"Pinned panels must snap back without unpinning");
    assert(!panel->detached&&panel->pinned&&FindDetachedPopover(1,L"PDES-6513")==panel);
    assert(g_attached_popover.window==other&&"Docking a pinned panel must preserve other open editors");
    assert(GetWindow(popup,GW_OWNER)==NULL);
    if(!(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST))fprintf(stderr,"Snap styles before=%llx after=%llx\n",(unsigned long long)snap_style_before,(unsigned long long)GetWindowLongPtrW(popup,GWL_EXSTYLE));
    assert(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST);
    assert(panel->input==edit);wchar_t draft[64];GetWindowTextW(edit,draft,64);assert(!wcscmp(draft,L"Pinned snap draft"));
    ShowWindow(g_window,SW_HIDE);CloseTaskPopover();g_active_popover=panel;CloseTaskPopover();g_active_popover=&g_attached_popover;
    OpenTaskPopover(0,1,(RECT){200,30,215,45});
}
static void test_drag_pointer(void){
    for(int dpi=96;dpi<=192;dpi+=48)for(int side=0;side<2;side++)for(int pinned=0;pinned<2;pinned++){
        if(g_popover.window)CloseTaskPopover();g_dpi=dpi;UpdateFonts();
        OpenTaskPopover(0,1,(RECT){200,30,215,45});assert(g_popover.window);
        g_popover.pointer_left=side;LayoutTaskPopoverContents();
        HWND popup=g_popover.window,edit=g_popover.input;SetWindowTextW(edit,L"Drag draft");
        if(pinned)SendMessageW(popup,WM_COMMAND,1043,0);
        TaskPopover *panel=(TaskPopover*)GetWindowLongPtrW(popup,GWLP_USERDATA);
        RECT before,after,input_before,input_after;GetWindowRect(popup,&before);GetWindowRect(edit,&input_before);
        fixture_cursor_active=TRUE;fixture_cursor=(POINT){before.left+panel->body_left+D(30),before.top+D(18)};
        SendMessageW(popup,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(panel->body_left+D(30),D(18)));
        fixture_cursor.x+=D(2);SendMessageW(popup,WM_MOUSEMOVE,MK_LBUTTON,0);
        assert(!panel->detached&&"A tiny pointer movement is not undocking");
        fixture_cursor.x+=D(30)-D(2);fixture_cursor.y+=D(12);
        SendMessageW(popup,WM_MOUSEMOVE,MK_LBUTTON,0);
        panel=FindDetachedPopover(1,L"PDES-6513");assert(panel&&panel->detached&&panel->pointer_retiring&&panel->pinned==pinned);
        GetWindowRect(popup,&after);GetWindowRect(edit,&input_after);
        RECT expected=input_before;OffsetRect(&expected,D(30),D(12));assert(EqualRect(&expected,&input_after));
        assert(after.right-after.left==before.right-before.left&&"Pointer retraction must not initially resize the panel");
        g_active_popover=panel;ULONGLONG start=panel->pointer_started;
        AdvanceTaskPopoverDetachment(start+60);assert(panel->pointer_extent>0&&panel->pointer_extent<D(16));
        GetWindowRect(edit,&input_after);assert(EqualRect(&expected,&input_after));
        /* Releasing during retraction must keep the independent window and draft. */
        SendMessageW(popup,WM_LBUTTONUP,0,0);assert(panel->pointer_retiring);
        AdvanceTaskPopoverDetachment(start+120);assert(!panel->pointer_retiring&&!panel->pointer_extent);
        GetWindowRect(edit,&input_after);GetWindowRect(popup,&after);
        assert(EqualRect(&expected,&input_after)&&"Final gutter trim must preserve every child screen coordinate");
        assert(after.right-after.left==before.right-before.left-D(16));
        RECT settled=after;SendMessageW(popup,WM_TIMER,3,0);GetWindowRect(popup,&after);assert(EqualRect(&settled,&after));
        wchar_t draft[40];GetWindowTextW(edit,draft,40);assert(!wcscmp(draft,L"Drag draft"));
        fixture_cursor_active=FALSE;CloseTaskPopover();g_active_popover=&g_attached_popover;
    }
    /* Redocking before the animation ends cancels it without adding a second gutter. */
    g_dpi=96;UpdateFonts();OpenTaskPopover(0,1,(RECT){200,30,215,45});
    HWND popup=g_popover.window;RECT before,after;GetWindowRect(popup,&before);
    TaskPopover *panel=BeginTaskPopoverDragDetach();assert(panel&&panel->pointer_retiring);
    assert(AttachTaskPopover(panel,FALSE)&&!g_popover.pointer_retiring&&!g_popover.detached);
    GetWindowRect(popup,&after);assert(before.right-before.left==after.right-after.left);
    SendMessageW(popup,WM_TIMER,3,0);assert(!g_popover.detached);CloseTaskPopover();
    OpenTaskPopover(0,1,(RECT){200,30,215,45});panel=BeginTaskPopoverDragDetach();popup=panel->window;
    g_active_popover=panel;CloseTaskPopover();g_active_popover=&g_attached_popover;
    assert(!IsWindow(popup)&&!FindDetachedPopover(1,L"PDES-6513"));
}
static void test_note_scroll(void){
    CloseTaskPopover();OpenTaskPopover(0,2,(RECT){200,30,215,45});assert(g_popover.input);
    HWND edit=g_popover.input;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<=1;density++){
        g_dpi=dpi;g_density=density;UpdateFonts();
        SetWindowPos(g_popover.window,NULL,0,0,D(366),D(215),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowTextW(edit,L"1. добавить ui от Данилы");LayoutTaskPopoverContents();
        assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
        wchar_t long_note[1800];long_note[0]=0;
        for(int line=0;line<35;line++)wcscat_s(long_note,_countof(long_note),L"Local note line.\r\n");
        SetWindowTextW(edit,long_note);assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);
        SetWindowPos(g_popover.window,NULL,0,0,D(366),D(1100),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutTaskPopoverContents();assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
        SetWindowPos(g_popover.window,NULL,0,0,D(366),D(215),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutTaskPopoverContents();assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);
        SendMessageW(edit,EM_SETSEL,0,-1);SendMessageW(edit,EM_REPLACESEL,TRUE,(LPARAM)L"Short");
        assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));assert(SendMessageW(edit,EM_CANUNDO,0,0));
        DWORD start,end;SendMessageW(edit,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);int saves=note_save_calls;
        for(int pass=0;pass<3;pass++){LayoutTaskPopoverContents();RefreshTaskPopover();}
        DWORD after_start,after_end;SendMessageW(edit,EM_GETSEL,(WPARAM)&after_start,(LPARAM)&after_end);
        assert(start==after_start&&end==after_end);assert(note_save_calls==saves);assert(SendMessageW(edit,EM_CANUNDO,0,0));
        assert(SendMessageW(edit,EM_UNDO,0,0));assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);
        wchar_t restored[1800];GetWindowTextW(edit,restored,_countof(restored));assert(!wcscmp(restored,long_note));
        SendMessageW(edit,EM_LINESCROLL,0,8);int first=(int)SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0);
        LayoutTaskPopoverContents();assert(SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)==first);
        /* Find a real wrapped-text boundary using an independent native edit:
           it fits at full width but overflows if an existing bar steals width. */
        RECT bounds;GetWindowRect(edit,&bounds);
        HWND measure=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL,
            0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,g_window,NULL,g_instance,NULL);assert(measure);
        SendMessageW(measure,WM_SETFONT,SendMessageW(edit,WM_GETFONT,0,0),FALSE);
        HDC dc=GetDC(measure);HGDIOBJ old_font=SelectObject(dc,(HFONT)SendMessageW(edit,WM_GETFONT,0,0));TEXTMETRICW metrics;
        GetTextMetricsW(dc,&metrics);SelectObject(dc,old_font);ReleaseDC(measure,dc);
        wchar_t wrapped[1800];BOOL boundary=FALSE;
        for(int length=1;length<1799&&!boundary;length++){
            wrapped[length-1]=L'W';wrapped[length]=0;ShowScrollBar(measure,SB_VERT,FALSE);SetWindowTextW(measure,wrapped);
            RECT format;SendMessageW(measure,EM_GETRECT,0,(LPARAM)&format);int rows=(format.bottom-format.top)/metrics.tmHeight;
            int full=(int)SendMessageW(measure,EM_GETLINECOUNT,0,0);ShowScrollBar(measure,SB_VERT,TRUE);
            boundary=full<=rows&&SendMessageW(measure,EM_GETLINECOUNT,0,0)>rows;
        }
        assert(boundary);DestroyWindow(measure);
        SendMessageW(edit,EM_SETSEL,0,-1);SendMessageW(edit,EM_REPLACESEL,TRUE,(LPARAM)wrapped);
        assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
        /* A retained bar after a prior resize must not sustain its own overflow. */
        g_popover.updating_scroll=TRUE;ShowScrollBar(edit,SB_VERT,TRUE);g_popover.updating_scroll=FALSE;
        assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);LayoutTaskPopoverContents();
        assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
        assert(SendMessageW(edit,EM_UNDO,0,0));assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);
    }
    SetWindowTextW(edit,L"");LayoutTaskPopoverContents();assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
    TaskPopover *panel=DetachTaskPopover();assert(panel);g_active_popover=panel;
    SetWindowTextW(edit,L"Detached short note");LayoutTaskPopoverContents();assert(!(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL));
    SendMessageW(edit,EM_SETSEL,0,-1);SendMessageW(edit,EM_REPLACESEL,TRUE,(LPARAM)L"One\r\nTwo\r\nThree\r\nFour\r\nFive\r\nSix\r\nSeven\r\nEight\r\nNine\r\nTen");
    assert(GetWindowLongPtrW(edit,GWL_STYLE)&WS_VSCROLL);
    CloseTaskPopover();g_active_popover=&g_attached_popover;g_dpi=96;g_density=1;UpdateFonts();
}
static void test_long_note(void){
    if(g_popover.window)CloseTaskPopover();
    OpenTaskPopover(0,2,(RECT){200,30,215,45});
    size_t length=70000;
    wchar_t *text=calloc(length+1,sizeof(wchar_t)),*loaded=calloc(length+2,sizeof(wchar_t));assert(text&&loaded);
    for(size_t i=0;i<length;i++)text[i]=(i%73==0)?L'Я':L'ф';
    SetWindowTextW(g_popover.input,L"");
    SendMessageW(g_popover.input,EM_REPLACESEL,TRUE,(LPARAM)text);
    assert(GetWindowTextLengthW(g_popover.input)==length);
    assert(!g_popover.note_save_failed&&g_tasks[0].local_note&&!wcscmp(g_tasks[0].local_note,text));
    assert(NativeNoteLoad(g_settings_path,g_snapshot.account,g_tasks[0].key,loaded,length+2)&&!wcscmp(loaded,text));
    CloseTaskPopover();OpenTaskPopover(0,2,(RECT){200,30,215,45});
    assert(GetWindowTextW(g_popover.input,loaded,(int)length+2)==length&&!wcscmp(loaded,text));
    TaskPopover *panel=DetachTaskPopover();assert(panel);g_active_popover=panel;
    SendMessageW(panel->input,EM_SETSEL,length,length);
    SendMessageW(panel->input,EM_REPLACESEL,TRUE,(LPARAM)L"!");
    assert(GetWindowTextLengthW(panel->input)==length+1&&!panel->note_save_failed);
    assert(NativeNoteLoad(g_settings_path,g_snapshot.account,g_tasks[0].key,loaded,length+2)&&loaded[length]==L'!');
    CloseTaskPopover();g_active_popover=&g_attached_popover;
    g_comments_mode=2;RefreshTaskList();BeginNoteEdit(0,0);assert(g_editing_note);
    SendMessageW(g_note_edit,EM_SETSEL,length+1,length+1);
    SendMessageW(g_note_edit,EM_REPLACESEL,TRUE,(LPARAM)L"?");
    assert(GetWindowTextLengthW(g_note_edit)==length+2);
    assert(CommitNoteEdit(FALSE));
    assert(wcslen(g_tasks[0].local_note)==length+1);
    BeginNoteEdit(0,0);SendMessageW(g_note_edit,EM_SETSEL,0,-1);
    SendMessageW(g_note_edit,EM_REPLACESEL,TRUE,(LPARAM)text);assert(CommitNoteEdit(TRUE));
    assert(!wcscmp(g_tasks[0].local_note,text));
    free(g_tasks[0].local_note);g_tasks[0].local_note=NULL;
    g_comments_mode=0;free(text);free(loaded);
}
static void assert_frame_pixels(void){
    RECT client;GetClientRect(g_popover.window,&client);
    HRGN region=CreateRectRgn(0,0,0,0);assert(GetWindowRgn(g_popover.window,region)!=ERROR);
    HDC screen=GetDC(NULL),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom);
    HGDIOBJ previous=SelectObject(memory,bitmap);SelectClipRgn(memory,region);
    capture_dc=memory;SendMessageW(g_popover.window,WM_PAINT,0,0);capture_dc=NULL;GdiFlush();
    int left=g_popover.body_left,right=g_popover.body_right-1,bottom=client.bottom-1,middle=(left+right)/2;
    assert(GetPixel(memory,middle,0)==PopoverBorderColor());
    assert(GetPixel(memory,left,client.bottom/2)==PopoverBorderColor());
    assert(GetPixel(memory,right,client.bottom/2)==PopoverBorderColor());
    assert(GetPixel(memory,middle,bottom)==PopoverBorderColor());
    for(int row=0;row<D(10);row++)for(int edge=0;edge<2;edge++){
        int y=edge?client.bottom-1-row:row;
        int first=left,last=right;
        while(first<right&&!PtInRegion(region,first,y))first++;
        while(last>left&&!PtInRegion(region,last,y))last--;
        assert(GetPixel(memory,first,y)==PopoverBorderColor());
        assert(GetPixel(memory,last,y)==PopoverBorderColor());
    }
    /* The frame encloses the resize grip, including the curved lower corners. */
    for(int side=0;side<2;side++){
        int corner_ink=0;
        for(int y=client.bottom-D(10);y<client.bottom;y++)for(int x=side?right-D(10):left;x<(side?right+1:left+D(11));x++)
            if(PtInRegion(region,x,y)&&GetPixel(memory,x,y)==PopoverBorderColor())corner_ink++;
        assert(corner_ink>=D(8));
    }
    SelectClipRgn(memory,NULL);SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);DeleteObject(region);
}
static void test_locked_note_refresh(void){
    TaskItem *original=g_tasks;
    assert(NativeNoteSave(g_settings_path,g_snapshot.account,original[0].key,L"Retained note"));
    wchar_t pattern[MAX_PATH+64],file[MAX_PATH+64];WIN32_FIND_DATAW found;
    swprintf(pattern,_countof(pattern),L"%ls.note_*.dat",g_settings_path);
    HANDLE search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);FindClose(search);
    wchar_t *slash=wcsrchr(g_settings_path,L'\\');assert(slash);size_t prefix=slash-g_settings_path+1;
    wmemcpy(file,g_settings_path,prefix);wcscpy_s(file+prefix,_countof(file)-prefix,found.cFileName);
    HANDLE locked=CreateFileW(file,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);assert(locked!=INVALID_HANDLE_VALUE);
    g_tasks=calloc(1,sizeof(*g_tasks));assert(g_tasks);*g_tasks=*original;g_tasks[0].local_note=DuplicateWide(L"Retained note");
    JiraSnapshot snapshot={0};snapshot.count=1;snapshot.items=calloc(1,sizeof(*snapshot.items));assert(snapshot.items);
    wcscpy_s(snapshot.items[0].key,80,original[0].key);wcscpy_s(snapshot.items[0].summary,1024,L"Locked note fixture");
    wcscpy_s(snapshot.items[0].status_id,80,L"1");wcscpy_s(snapshot.items[0].status_name,128,L"To Do");snapshot.items[0].assigned_to_me=TRUE;
    assert(InstallSnapshot(&snapshot));
    assert(g_tasks[0].local_note&&!wcscmp(g_tasks[0].local_note,L"Retained note"));
    int errors=note_error_dialogs;OpenTaskPopover(0,2,(RECT){200,30,215,45});
    assert(!g_popover.window&&note_error_dialogs==errors+1);
    CloseHandle(locked);OpenTaskPopover(0,2,(RECT){200,30,215,45});assert(g_popover.window);
    CloseTaskPopover();FreeTaskItems();JiraFreeSnapshot(&g_snapshot);g_tasks=original;g_task_count=1;
}
static void test_frame(void){
    for(int kind=1;kind<=2;kind++){
        if(g_popover.window)CloseTaskPopover();OpenTaskPopover(0,kind,(RECT){200,30,215,45});
        for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int density=0;density<=1;density++)for(int side=0;side<2;side++){
            g_dpi=dpi;g_dark=dark;g_density=density;RecreateBrushes();UpdateFonts();g_popover.pointer_left=side;
            SetWindowPos(g_popover.window,NULL,0,0,D(366),D(275),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            LayoutTaskPopoverContents();assert_frame_pixels();
        }
        TaskPopover *panel=DetachTaskPopover();assert(panel);g_active_popover=panel;
        for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int density=0;density<=1;density++){
            g_dpi=dpi;g_dark=dark;g_density=density;RecreateBrushes();UpdateFonts();
            SetWindowPos(panel->window,NULL,0,0,D(350),D(275),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            LayoutTaskPopoverContents();assert_frame_pixels();
        }
        CloseTaskPopover();g_active_popover=&g_attached_popover;
    }
    g_dpi=96;g_dark=FALSE;g_density=1;RecreateBrushes();UpdateFonts();
}
int main(int argc,char **argv){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_density=1;g_russian=TRUE;RecreateBrushes();UpdateFonts();
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&controls));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"prf",0,g_settings_path));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    TaskItem task={.key=L"PDES-6513",.summary=L"Comment fixture",.status=STATUS_TODO,.assigned_to_me=TRUE};g_tasks=&task;g_task_count=1;g_tab=0;g_jira_busy=TRUE;RefreshTaskList();
    OpenTaskPopover(0,1,(RECT){200,30,215,45});assert(g_popover.window);
    const char *mode=argc>1?argv[1]:"all";
    if(!strcmp(mode,"link")||!strcmp(mode,"all"))test_link();
    if(!strcmp(mode,"composer")||!strcmp(mode,"all"))test_composer();
    if(!strcmp(mode,"media")||!strcmp(mode,"all"))test_media();
    if(!strcmp(mode,"glyph")||!strcmp(mode,"all"))test_glyph();
    if(!strcmp(mode,"pin")||!strcmp(mode,"all"))test_unpin();
    if(!strcmp(mode,"pinned-snap")||!strcmp(mode,"all"))test_pinned_snap();
    if(!strcmp(mode,"drag-pointer")||!strcmp(mode,"all"))test_drag_pointer();
    if(!strcmp(mode,"note-scroll")||!strcmp(mode,"all"))test_note_scroll();
    if(!strcmp(mode,"frame")||!strcmp(mode,"all"))test_frame();
    if(!strcmp(mode,"long-note")||!strcmp(mode,"all"))test_long_note();
    if(!strcmp(mode,"locked-note")||!strcmp(mode,"all"))test_locked_note_refresh();
    if(g_detached_popovers)g_active_popover=g_detached_popovers;CloseTaskPopover();g_active_popover=&g_attached_popover;
    free(task.local_note);g_tasks=NULL;g_task_count=0;g_jira_busy=FALSE;DestroyWindow(g_window);
    wchar_t pattern[MAX_PATH+64],sidecar[MAX_PATH+64];WIN32_FIND_DATAW found;
    swprintf(pattern,_countof(pattern),L"%ls.note_*.dat",g_settings_path);
    HANDLE search=FindFirstFileW(pattern,&found);
    if(search!=INVALID_HANDLE_VALUE){
        wchar_t *slash=wcsrchr(g_settings_path,L'\\');assert(slash);size_t prefix=slash-g_settings_path+1;
        do{wmemcpy(sidecar,g_settings_path,prefix);wcscpy_s(sidecar+prefix,_countof(sidecar)-prefix,found.cFileName);assert(DeleteFileW(sidecar));}while(FindNextFileW(search,&found));
        FindClose(search);
    }
    DeleteFileW(g_settings_path);
    printf("Popover refinement: %s passed\n",mode);return 0;
}
