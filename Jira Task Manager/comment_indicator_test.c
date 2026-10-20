#include <windows.h>
#include <assert.h>
static ULONGLONG fixture_now=1000;
static BOOL fixture_motion=TRUE;
static ULONGLONG WINAPI FixtureTick(void){return fixture_now;}
static BOOL WINAPI FixtureSettings(UINT action,UINT value,PVOID output,UINT flags)
{
    if(action==SPI_GETCLIENTAREAANIMATION){*(BOOL*)output=fixture_motion;return TRUE;}
    return SystemParametersInfoW(action,value,output,flags);
}
#define GetTickCount64 FixtureTick
#define SystemParametersInfoW FixtureSettings
#include "native.c"
static void test_read_unread_and_filled_tail(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);fixture_motion=FALSE;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++)for(int active=0;active<2;active++){
        g_dpi=dpi;g_density=density;g_dark=dark;
        RECT canvas={0,0,D(32),D(32)},icon={D(8),D(8),D(24),D(24)};
        HBITMAP bitmap=CreateCompatibleBitmap(screen,canvas.right,canvas.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
        COLORREF row=active?ActiveColor():PanelColor();HBRUSH background=CreateSolidBrush(row);
        for(int state=0;state<5;state++){
            BOOL unread=state==1,hovered=state==2,pressed=state==3,open=state==4;
            TaskItem task={.unread_comment=unread,.comment_pulse_started=1000};
            FillRect(dc,&canvas,background);
            DrawCommentIndicator(dc,icon,hovered,pressed,TRUE,unread,open,&task,row);GdiFlush();
            COLORREF expected=state==0?row:unread?(dark?RGB(32,58,82):RGB(229,241,255)):
                hovered?(dark?RGB(47,74,101):RGB(215,235,253)):
                pressed?(dark?RGB(39,125,204):RGB(4,91,172)):(dark?RGB(76,164,245):RGB(11,116,209));
            assert(GetPixel(dc,D(16),D(14))==expected);
            if(dpi>=144)assert(GetPixel(dc,icon.left+D(5),icon.bottom-D(4))==expected);
            /* Presence of unread must not add a floating corner dot or resize the cloud. */
            assert(GetPixel(dc,icon.right-D(1),icon.top-D(2))==row);
        }
        SelectObject(dc,old);DeleteObject(bitmap);DeleteObject(background);
    }
    fixture_motion=TRUE;DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_two_motion_bursts_and_pause(void)
{
    g_dpi=96;g_density=1;g_dark=FALSE;fixture_motion=TRUE;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,64,64);HGDIOBJ old=SelectObject(dc,bitmap);
    RECT canvas={0,0,64,64},icon={24,24,40,40},bounds=icon;InflateRect(&bounds,4,4);
    TaskItem task={.unread_comment=TRUE,.comment_pulse_started=1000};
    const ULONGLONG frames[]={1000,1120,1600,1960,7000,7120,4294969120ULL};
    COLORREF pixels[7][64*64];HBRUSH background=CreateSolidBrush(PanelColor());
    HPEN caller=CreatePen(PS_SOLID,7,RGB(200,0,200));HGDIOBJ old_pen=SelectObject(dc,caller);
    for(int frame=0;frame<7;frame++){
        fixture_now=frames[frame];FillRect(dc,&canvas,background);int mode=GetGraphicsMode(dc);
        DrawCommentIndicator(dc,icon,FALSE,FALSE,TRUE,TRUE,FALSE,&task,PanelColor());GdiFlush();
        assert(GetCurrentObject(dc,OBJ_PEN)==caller&&GetGraphicsMode(dc)==mode);
        for(int y=0;y<64;y++)for(int x=0;x<64;x++){
            pixels[frame][y*64+x]=GetPixel(dc,x,y);POINT point={x,y};
            if(!PtInRect(&bounds,point))assert(pixels[frame][y*64+x]==PanelColor());
        }
    }
    assert(memcmp(pixels[0],pixels[1],sizeof(pixels[0])));
    assert(memcmp(pixels[0],pixels[2],sizeof(pixels[0])));
    assert(!memcmp(pixels[0],pixels[3],sizeof(pixels[0]))); /* Pause after two soft motions. */
    assert(!memcmp(pixels[0],pixels[4],sizeof(pixels[0]))); /* Starts again every six seconds. */
    assert(!memcmp(pixels[1],pixels[5],sizeof(pixels[0])));
    assert(!memcmp(pixels[1],pixels[6],sizeof(pixels[0]))); /* Same cycle after 32-bit uptime wrap. */
    fixture_motion=FALSE;fixture_now=1120;FillRect(dc,&canvas,background);
    DrawCommentIndicator(dc,icon,FALSE,FALSE,TRUE,TRUE,FALSE,&task,PanelColor());GdiFlush();
    for(int y=0;y<64;y++)for(int x=0;x<64;x++)assert(GetPixel(dc,x,y)==pixels[0][y*64+x]);
    fixture_motion=TRUE;task.unread_comment=FALSE;assert(!CommentPulseActive(&task,1120));
    SelectObject(dc,old_pen);DeleteObject(caller);DeleteObject(background);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
int main(void)
{
    test_read_unread_and_filled_tail();test_two_motion_bursts_and_pause();
    puts("Comment notifications: distinct read/unread, complete hover/open/pressed tail, two bounded motions per six seconds and reduced motion passed");
}
