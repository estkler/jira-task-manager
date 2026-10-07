#include "native.c"
#include <assert.h>

/* Real GDI output: the cross-mode cue must look informational, not amber/error-like.
   Losing the badge, changing it back to amber, or drawing the same mode twice fails. */
static void test_scope_painter(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int selected=0;selected<2;selected++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_dark=dark;g_density=density;
        RECT rect={0,0,D(28),D(28)}; /* The actual toolbar control, not a roomy icon sample. */
        COLORREF foreground=selected?AccentColor():MutedColor();
        HBITMAP bitmap=CreateCompatibleBitmap(screen,rect.right,rect.bottom);
        HGDIOBJ previous=SelectObject(dc,bitmap);
        COLORREF modes[2][72*72]={0};
        for(int reported=0;reported<2;reported++){
            HBRUSH background=CreateSolidBrush(WindowColor());
            FillRect(dc,&rect,background);
            DrawScopeIcon(dc,rect,foreground,reported,TRUE);GdiFlush();
            int blue=0,warm=0;
            for(int y=D(2);y<D(9);y++)for(int x=D(19);x<D(26);x++){
                COLORREF color=GetPixel(dc,x,y);
                if(GetBValue(color)>GetRValue(color)+30)blue++;
                if(GetRValue(color)>GetBValue(color)+30)warm++;
            }
            assert(blue>0);assert(warm==0);
            for(int y=0;y<rect.bottom;y++)for(int x=0;x<rect.right;x++)
                modes[reported][y*72+x]=GetPixel(dc,x,y);
            FillRect(dc,&rect,background);
            DrawScopeIcon(dc,rect,foreground,reported,FALSE);GdiFlush();
            /* Reserve a clear corner around the dot; no icon edge may run underneath it. */
            for(int y=D(1);y<D(10);y++)for(int x=D(18);x<D(27);x++){
                if(GetPixel(dc,x,y)!=WindowColor())
                    fprintf(stderr,"Badge corner overlap: DPI=%d density=%d reported=%d pixel=%d,%d\n",dpi,density,reported,x,y);
                assert(GetPixel(dc,x,y)==WindowColor());
            }
            DeleteObject(background);
        }
        int different=0;
        for(int y=D(10);y<D(27);y++)for(int x=D(3);x<D(23);x++)
            different+=modes[0][y*72+x]!=modes[1][y*72+x];
        assert(different>D(10));
        SelectObject(dc,previous);DeleteObject(bitmap);
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
}

static void test_other_mode_activity(void)
{
    TaskItem tasks[4]={0};g_tasks=tasks;g_task_count=4;
    tasks[0].assigned_to_me=TRUE;tasks[0].is_new=TRUE;
    tasks[1].reported_by_me=TRUE;tasks[1].unread_comment=TRUE;
    tasks[2].reported_by_me=TRUE;tasks[2].is_new=TRUE;tasks[2].unread_comment=TRUE;
    tasks[3].assigned_to_me=tasks[3].reported_by_me=TRUE;tasks[3].unread_comment=TRUE;
    g_reported_scope=FALSE;assert(OtherScopeAttentionCount()==2);
    g_reported_scope=TRUE;assert(OtherScopeAttentionCount()==1);
    tasks[0].is_new=FALSE;tasks[0].unread_comment=TRUE;
    assert(OtherScopeAttentionCount()==1);
    tasks[0].unread_comment=FALSE;assert(OtherScopeAttentionCount()==0);
    g_reported_scope=FALSE;assert(OtherScopeAttentionCount()==2);
    tasks[1].unread_comment=FALSE;tasks[2].unread_comment=tasks[2].is_new=FALSE;
    assert(OtherScopeAttentionCount()==0);
    g_tasks=NULL;g_task_count=0;
}

int main(void)
{
    test_scope_painter();test_other_mode_activity();
    puts("Scope icons: distinct modes, blue activity badge, both themes, 96/144/192 DPI; tasks/comments in both scopes passed");
}
