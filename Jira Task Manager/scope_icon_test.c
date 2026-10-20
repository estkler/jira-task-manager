#include <windows.h>
#include <assert.h>
#include <math.h>
static BOOL capture_count;
static wchar_t painted_count[32];
static RECT painted_counter_rect;
static int WINAPI CaptureCountText(HDC,LPCWSTR,int,LPRECT,UINT);
#define DrawTextW CaptureCountText
#include "native.c"
#undef DrawTextW
static int WINAPI CaptureCountText(HDC dc,LPCWSTR text,int count,LPRECT rect,UINT flags)
{
    if(capture_count&&text&&text[0]>=L'0'&&text[0]<=L'9'){wcsncpy_s(painted_count,32,text,_TRUNCATE);painted_counter_rect=*rect;}
    return DrawTextW(dc,text,count,rect,flags);
}
static void test_scope_size_and_center(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;
        RECT rect={0,0,D(50),D(28)};
        HBITMAP bitmap=CreateCompatibleBitmap(screen,rect.right,rect.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
        HBRUSH background=CreateSolidBrush(WindowColor());
        for(int scope=0;scope<2;scope++){
            FillRect(dc,&rect,background);DrawScopeIcon(dc,rect,AccentColor(),scope,0,WindowColor());GdiFlush();
            RECT ink={rect.right,rect.bottom,0,0};double mass_x=0,mass_y=0;int mass=0;
            for(int y=0;y<rect.bottom;y++)for(int x=0;x<rect.right;x++)if(GetPixel(dc,x,y)!=WindowColor()){
                ink.left=min(ink.left,x);ink.top=min(ink.top,y);ink.right=max(ink.right,x+1);ink.bottom=max(ink.bottom,y+1);
                mass_x+=x+.5;mass_y+=y+.5;mass++;
            }
            assert(mass>0);
            assert(abs(ink.right-ink.left-D(14))<=1&&abs(ink.bottom-ink.top-D(14))<=1);
            assert(fabs((ink.left+ink.right)/2.0-rect.right/2.0)<=D(1)+.5);
            assert(fabs((ink.top+ink.bottom)/2.0-rect.bottom/2.0)<=D(1)+.5);
            assert(fabs(mass_x/mass-rect.right/2.0)<=D(3)&&fabs(mass_y/mass-rect.bottom/2.0)<=D(3));
            assert(ink.top>=D(3)&&ink.bottom<=rect.bottom-D(3));
        }
        SelectObject(dc,old);DeleteObject(bitmap);DeleteObject(background);
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void paint_scope(HDC dc,int scope,BOOL focused)
{
    DRAWITEMSTRUCT draw={.CtlID=scope?IDC_TAB_REPORTED:IDC_TAB_ASSIGNED,.hwndItem=(HWND)(INT_PTR)2,
        .hDC=dc,.rcItem={0,0,ScopeButtonWidth(dc,scope),D(28)},.itemState=focused?ODS_FOCUS:0};
    painted_count[0]=0;capture_count=TRUE;DrawOwnerButton(&draw);capture_count=FALSE;GdiFlush();
}
static void test_own_shared_activity_and_counter(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_dpi=144;g_density=0;g_dark=FALSE;
    UpdateFonts();RecreateBrushes();g_hover_button=g_keyboard_button=NULL;
    RECT canvas={0,0,D(60),D(28)};HBITMAP bitmap=CreateCompatibleBitmap(screen,canvas.right,canvas.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
    TaskItem tasks[4]={{.assigned_to_me=TRUE},{.reported_by_me=TRUE,.unread_comment=TRUE},
        {.assigned_to_me=TRUE,.reported_by_me=TRUE},{.is_new=TRUE,.unread_comment=TRUE}};
    g_tasks=tasks;g_task_count=4;
    for(int active_scope=0;active_scope<2;active_scope++)for(int activity=0;activity<4;activity++)for(int shared=0;shared<2;shared++){
        g_reported_scope=active_scope;g_tab=2;memset(g_filter_masks,0,sizeof(g_filter_masks));
        tasks[0].is_new=activity&1;tasks[0].unread_comment=activity&2;tasks[2].is_new=tasks[2].unread_comment=shared;
        for(int scope=0;scope<2;scope++){
            paint_scope(dc,scope,FALSE);
            int want=(scope?1:activity!=0)+shared;wchar_t expected[32]=L"";if(want)swprintf(expected,32,L"%d",want);
            assert(!wcscmp(painted_count,expected));
            if(want){
                HGDIOBJ font=SelectObject(dc,g_scope_count_font);SIZE size;
                assert(GetTextExtentPoint32W(dc,painted_count,(int)wcslen(painted_count),&size));SelectObject(dc,font);
                assert(painted_counter_rect.right-painted_counter_rect.left>=size.cx);
                assert(painted_counter_rect.bottom-painted_counter_rect.top>=size.cy);
                assert(painted_counter_rect.right<=ScopeButtonWidth(dc,scope)-D(7));
            }
        }
    }
    tasks[0].is_new=tasks[0].unread_comment=tasks[1].unread_comment=tasks[2].is_new=tasks[2].unread_comment=FALSE;
    paint_scope(dc,0,FALSE);assert(!painted_count[0]);paint_scope(dc,1,FALSE);assert(!painted_count[0]);
    g_tasks=NULL;g_task_count=0;SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_keyboard_focus_and_neutral_selection(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_dpi=96;g_density=1;
    UpdateFonts();HBITMAP bitmap=CreateCompatibleBitmap(screen,60,28);HGDIOBJ old=SelectObject(dc,bitmap);
    g_tasks=NULL;g_task_count=0;g_hover_button=NULL;
    for(int dark=0;dark<2;dark++)for(int scope=0;scope<2;scope++){
        g_dark=dark;g_reported_scope=scope;RecreateBrushes();g_keyboard_button=(HWND)(INT_PTR)2;
        paint_scope(dc,scope,TRUE);int focus_pixels=0;
        for(int y=1;y<6;y++)for(int x=1;x<ScopeButtonWidth(dc,scope)-1;x++)if(GetPixel(dc,x,y)==AccentColor())focus_pixels++;
        assert(focus_pixels>0);
        g_keyboard_button=NULL;paint_scope(dc,scope,FALSE);
        assert(GetPixel(dc,8,6)==PanelColor());
        assert(GetPixel(dc,18,27)!=AccentColor());
    }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_counter_cap_and_text_fit(void)
{
    TaskItem tasks[100]={0};for(int i=0;i<100;i++){tasks[i].assigned_to_me=tasks[i].reported_by_me=TRUE;tasks[i].is_new=tasks[i].unread_comment=TRUE;}
    g_tasks=tasks;g_reported_scope=FALSE;g_hover_button=g_keyboard_button=NULL;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    const int totals[]={9,10,99,100};const wchar_t *expected[]={L"9",L"10",L"99",L"99"};
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++){
        g_dpi=dpi;g_density=density;g_dark=FALSE;UpdateFonts();RecreateBrushes();
        HBITMAP bitmap=CreateCompatibleBitmap(screen,D(60),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
        for(int i=0;i<4;i++){
            g_task_count=totals[i];assert(ScopeActivityCounts(FALSE).updates==totals[i]);
            for(int scope=0;scope<2;scope++){
                paint_scope(dc,scope,FALSE);assert(!wcscmp(painted_count,expected[i]));
                HGDIOBJ font=SelectObject(dc,g_scope_count_font);SIZE size;assert(GetTextExtentPoint32W(dc,painted_count,(int)wcslen(painted_count),&size));
                SelectObject(dc,font);assert(size.cx<=painted_counter_rect.right-painted_counter_rect.left);
                assert(size.cy<=painted_counter_rect.bottom-painted_counter_rect.top);
            }
        }
        SelectObject(dc,old);DeleteObject(bitmap);
    }
    g_tasks=NULL;g_task_count=0;DeleteDC(dc);ReleaseDC(NULL,screen);
}
int main(void)
{
    test_scope_size_and_center();test_own_shared_activity_and_counter();test_counter_cap_and_text_fit();test_keyboard_focus_and_neutral_selection();
    puts("Compact scope controls: common 14-DIP sizes, centering, own/shared counters, zero state and keyboard focus passed");
}
