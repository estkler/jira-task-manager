#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include "native_images.h"
static HANDLE viewer_decode_entered,viewer_decode_continue;static volatile LONG viewer_decode_calls;
static BOOL BlockViewerDecode(const unsigned char *bytes,size_t length,UINT width,UINT height,NativeImageBitmap *bitmap,UINT *source_width,UINT *source_height,const volatile LONG *cancelled)
{
    if(viewer_decode_continue){InterlockedIncrement(&viewer_decode_calls);SetEvent(viewer_decode_entered);assert(WaitForSingleObject(viewer_decode_continue,3000)==WAIT_OBJECT_0);}
    return NativeImageDecodeWithCancel(bytes,length,width,height,bitmap,source_width,source_height,cancelled);
}
static wchar_t opened_profile[1024];
static BOOL WINAPI HideViewerForTest(HWND window,INT show)
{
    wchar_t name[80];GetClassNameW(window,name,_countof(name));return ShowWindow(window,!wcscmp(name,L"Task Manager.ImageViewer")?SW_HIDE:show);
}
static HINSTANCE WINAPI CaptureProfile(HWND window,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show)
{
    (void)window;(void)parameters;(void)directory;(void)show;assert(!wcscmp(operation,L"open"));wcscpy_s(opened_profile,_countof(opened_profile),file);return (HINSTANCE)33;
}
#define ShellExecuteW CaptureProfile
#define ShowWindow HideViewerForTest
#define NativeImageDecodeWithCancel BlockViewerDecode
#include "native.c"
#include "image_fixture.h"
static void FinishViewerFixture(void)
{
    for(int i=0;i<500;i++){
        MSG message;if(PeekMessageW(&message,g_window,WM_IMAGE_READY,WM_IMAGE_READY,PM_REMOVE)){ImageViewerFinish((ImageViewerState*)message.lParam);return;}Sleep(5);
    }
    assert(!"Image decode worker did not finish");
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    g_popover.window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,D(430),D(360),g_window,NULL,g_instance,NULL);
    g_popover.text=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,D(400),D(240),g_popover.window,NULL,g_instance,NULL);
    g_popover.kind=1;g_popover.loading=FALSE;g_popover.comments.count=1;g_popover.comments.items=calloc(1,sizeof(JiraComment));
    JiraComment *comment=g_popover.comments.items;wcscpy_s(comment->author,256,L"Colleague");wcscpy_s(comment->body,2048,L"Image below");
    wcscpy_s(comment->profile_url,1024,L"https://jira.example.test/secure/ViewProfile.jspa?name=colleague");
    HDC dc=GetDC(g_popover.text);int text_only=CommentCardHeight(dc,comment,D(400));
    comment->image_count=1;comment->images=calloc(1,sizeof(JiraCommentImage));size_t length;unsigned char *bytes=ImageFixtureBytes(300,160,&length);
    comment->images[0].image=NativeImageCreate(bytes,length);assert(comment->images[0].image);
    assert(CommentCardHeight(dc,comment,D(400))>text_only+D(100));
    ReleaseDC(g_popover.text,dc);
    CommentListProcedureInner(g_popover.text,WM_LBUTTONDOWN,0,MAKELPARAM(D(55),D(20)));
    assert(!wcscmp(opened_profile,L"https://jira.example.test/secure/ViewProfile.jspa?name=colleague"));
    /* An old response for the same key cannot replace a reopened pane. */
    g_popover.generation=2;wcscpy_s(g_popover.key,80,L"IMG-1");JiraComment *original=g_popover.comments.items;
    JiraJob *old=calloc(1,sizeof(*old));old->action=4;old->loaded=old->success=TRUE;old->popover_generation=1;wcscpy_s(old->key,80,L"IMG-1");
    old->comments.count=1;old->comments.items=calloc(1,sizeof(JiraComment));old->comments.items[0].image_count=1;old->comments.items[0].images=calloc(1,sizeof(JiraCommentImage));
    bytes=ImageFixtureBytes(100,80,&length);old->comments.items[0].images[0].image=NativeImageCreate(bytes,length);assert(old->comments.items[0].images[0].image);
    FinishJiraJob(old);assert(g_popover.comments.items==original);
    HWND viewer=OpenCommentImageViewer(comment->images[0].image,L"Fixture.png");assert(viewer);FinishViewerFixture();
    ImageViewerState *state=(ImageViewerState*)GetWindowLongPtrW(viewer,GWLP_USERDATA);assert(state&&state->ready&&state->bitmap.handle);
    CloseTaskPopover();assert(NativeImageMemoryBytes()>0);DestroyWindow(viewer);assert(NativeImageMemoryBytes()==0);
    /* Closing the viewer before its worker returns still releases every asset. */
    bytes=ImageFixtureBytes(300,160,&length);NativeImage *temporary=NativeImageCreate(bytes,length);assert(temporary);
    viewer=OpenCommentImageViewer(temporary,L"Early close.png");assert(viewer);NativeImageRelease(temporary);DestroyWindow(viewer);FinishViewerFixture();assert(NativeImageMemoryBytes()==0);
    viewer_decode_entered=CreateEventW(NULL,TRUE,FALSE,NULL);viewer_decode_continue=CreateEventW(NULL,TRUE,FALSE,NULL);assert(viewer_decode_entered&&viewer_decode_continue);
    bytes=ImageFixtureBytes(300,160,&length);temporary=NativeImageCreate(bytes,length);assert(temporary);
    viewer=OpenCommentImageViewer(temporary,L"Blocked decode.png");assert(viewer);assert(WaitForSingleObject(viewer_decode_entered,1000)==WAIT_OBJECT_0);
    for(int i=0;i<16;i++){viewer=OpenCommentImageViewer(temporary,L"Latest viewer.png");assert(viewer);}
    Sleep(30);assert(InterlockedCompareExchange(&viewer_decode_calls,0,0)==1); /* Coalesce obsolete requests; never one thread per click. */
    SetEvent(viewer_decode_continue);FinishViewerFixture();FinishViewerFixture();assert(viewer_decode_calls==2);
    NativeImageRelease(temporary);DestroyWindow(viewer);assert(NativeImageMemoryBytes()==0);CloseHandle(viewer_decode_entered);CloseHandle(viewer_decode_continue);viewer_decode_continue=NULL;
    DestroyWindow(g_window);
    puts("Comment media UI: image-aware layout, author activation, owned image cleanup passed");
}
