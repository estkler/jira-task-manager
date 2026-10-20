#include <windows.h>
#include <shellapi.h>
#include <assert.h>
static wchar_t opened_url[1024];static HWND opened_owner;
static BOOL FixtureUrl(wchar_t *url,size_t capacity){wcscpy_s(url,capacity,L"https://jira.example.test/jira/");return TRUE;}
static HINSTANCE WINAPI FixtureOpen(HWND owner,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show){
    (void)parameters;(void)directory;(void)show;assert(!wcscmp(operation,L"open"));opened_owner=owner;wcscpy_s(opened_url,1024,file);return (HINSTANCE)33;
}
static BOOL WINAPI FixtureHide(HWND window,INT show){(void)window;(void)show;return TRUE;}
#define JiraGetUrl FixtureUrl
#define ShellExecuteW FixtureOpen
#define ShowWindow FixtureHide
#include "native.c"
#undef ShowWindow
#include "image_fixture.h"
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
    HWND popup=g_popover.window;RECT before,after;GetWindowRect(popup,&before);
    SendMessageW(popup,WM_COMMAND,1043,0);TaskPopover *panel=FindDetachedPopover(1,L"PDES-6513");assert(panel&&panel->pinned);
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
    g_dpi=96;g_popover.pinned=TRUE;g_hover_button=NULL;
    HDC screen=GetDC(NULL),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(26),D(26));HGDIOBJ previous=SelectObject(memory,bitmap);
    DRAWITEMSTRUCT draw={.CtlID=1043,.hwndItem=g_popover.pin_button,.hDC=memory,.rcItem={0,0,D(26),D(26)}};DrawPopoverHeaderButton(&draw);GdiFlush();
    assert(GetPixel(memory,D(1),D(1))==PopoverHeaderColor()); /* Pinned glyph, not a permanently filled tile. */
    int ink=0;for(int y=0;y<D(26);y++)for(int x=0;x<D(26);x++)if(GetPixel(memory,x,y)==AccentColor())ink++;
    assert(ink>10&&ink<=45);
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(NULL,screen);g_popover.pinned=FALSE;
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
    if(g_detached_popovers)g_active_popover=g_detached_popovers;CloseTaskPopover();g_active_popover=&g_attached_popover;
    g_tasks=NULL;g_task_count=0;g_jira_busy=FALSE;DestroyWindow(g_window);DeleteFileW(g_settings_path);
    printf("Popover refinement: %s passed\n",mode);return 0;
}
