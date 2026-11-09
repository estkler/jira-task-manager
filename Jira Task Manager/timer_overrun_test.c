#include <windows.h>
#include <assert.h>
#include <stdio.h>
static ULONGLONG fixture_now;
static SYSTEMTIME fixture_local;
static void WINAPI FixtureClock(LPFILETIME value){ULARGE_INTEGER n;n.QuadPart=fixture_now;value->dwLowDateTime=n.LowPart;value->dwHighDateTime=n.HighPart;}
static void WINAPI FixtureLocal(LPSYSTEMTIME value){*value=fixture_local;}
static int WINAPI CaptureTimerText(HDC,LPCWSTR,int,LPRECT,UINT);
static BOOL WINAPI CaptureTimerEmoji(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
#define GetSystemTimeAsFileTime FixtureClock
#define GetLocalTime FixtureLocal
#define DrawTextW CaptureTimerText
#define GdiAlphaBlend CaptureTimerEmoji
#include "native.c"
#undef DrawTextW
#undef GdiAlphaBlend
static BOOL capturing;
static struct {wchar_t text[128];COLORREF color;LONG weight,height;RECT rect;} painted[4];
static int painted_count,emoji_count;
static int WINAPI CaptureTimerText(HDC dc,LPCWSTR text,int length,LPRECT rect,UINT format)
{
    if(capturing){
        assert(painted_count<4);int i=painted_count++;
        wcsncpy_s(painted[i].text,_countof(painted[i].text),text,length<0?_TRUNCATE:(size_t)length);
        LOGFONTW font;GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font);
        painted[i].weight=font.lfWeight;painted[i].height=font.lfHeight;painted[i].color=GetTextColor(dc);painted[i].rect=*rect;
    }
    return DrawTextW(dc,text,length,rect,format);
}
static BOOL WINAPI CaptureTimerEmoji(HDC dest,int x,int y,int w,int h,HDC source,int sx,int sy,int sw,int sh,BLENDFUNCTION blend)
{
    if(capturing){emoji_count++;assert(w==D(16)&&h==D(16)&&blend.AlphaFormat==AC_SRC_ALPHA);}
    return GdiAlphaBlend(dest,x,y,w,h,source,sx,sy,sw,sh,blend);
}
static void paint_timer(void)
{
    RECT r;GetClientRect(g_timer,&r);HDC screen=GetDC(g_timer),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,max(1,r.right),max(1,r.bottom));HGDIOBJ old=SelectObject(dc,bitmap);
    DRAWITEMSTRUCT draw={0};draw.CtlID=IDC_TIMER;draw.hwndItem=g_timer;draw.hDC=dc;draw.rcItem=r;
    painted_count=emoji_count=0;capturing=TRUE;DrawOwnerButton(&draw);capturing=FALSE;
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(g_timer,screen);
}
static RECT bounds(HWND control){RECT r;GetWindowRect(control,&r);MapWindowPoints(NULL,g_window,(POINT*)&r,2);return r;}
static void elapsed_text(int elapsed,const wchar_t *number)
{
    g_workday_started=fixture_now-(ULONGLONG)elapsed*10000000ULL;UpdateTimer();
    wchar_t actual[128],expected[128];GetWindowTextW(g_timer,actual,128);
    swprintf(expected,128,L"%ls%ls",number,elapsed>=9*3600?(g_russian?L" · Пора домой \xD83E\xDD17":L" · Time to go home \xD83E\xDD17"):L"");
    assert(!wcscmp(actual,expected));
}
int main(void)
{
    /* Frozen local18:00 supports9h+1min without crossing a day; no live settings. */
    fixture_local=(SYSTEMTIME){.wYear=2026,.wMonth=10,.wDay=21,.wHour=18};FILETIME local,utc;
    assert(SystemTimeToFileTime(&fixture_local,&local)&&LocalFileTimeToFileTime(&local,&utc));ULARGE_INTEGER n={.LowPart=utc.dwLowDateTime,.HighPart=utc.dwHighDateTime};fixture_now=n.QuadPart;
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"tov",0,g_settings_path));
    g_instance=GetModuleHandleW(NULL);INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_WIN95_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int text=0;text<2;text++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_russian=ru;g_text_time=text;g_dark=dark;UpdateFonts();RecreateBrushes();
        int widths[]={300,478,740};for(int w=0;w<_countof(widths);w++){
            int width=widths[w];
            SetWindowPos(g_window,NULL,0,0,D(width),D(550),SWP_NOMOVE|SWP_NOZORDER);g_footer_right=0;
            elapsed_text(9*3600-1,text?(ru?L"8ч 59м 59с":L"8h 59m 59s"):L"08:59:59");LayoutFooterControls(D(width),D(550));
            RECT before=bounds(g_timer);paint_timer();assert(painted_count==1&&emoji_count==0);LONG normal_height=painted[0].height;
            HDC measure=GetDC(g_window);HGDIOBJ saved_font=SelectObject(measure,g_timer_font);SIZE ordinary_size;
            GetTextExtentPoint32W(measure,g_timer_numbers,(int)wcslen(g_timer_numbers),&ordinary_size);
            SelectObject(measure,saved_font);ReleaseDC(g_window,measure);
            assert(before.right-before.left==ordinary_size.cx+D(4)); /* No unused home-caption reservation before nine hours. */
            RECT overtime={0};
            struct {int elapsed;const wchar_t *digital,*en,*ru;} cases[]={
                {9*3600,L"09:00:00",L"9h 0m 0s",L"9ч 0м 0с"},
                {9*3600+1,L"09:00:01",L"9h 0m 1s",L"9ч 0м 1с"},
                {9*3600+60,L"09:01:00",L"9h 1m 0s",L"9ч 1м 0с"}};
            for(int i=0;i<_countof(cases);i++){
                const wchar_t *number=text?(ru?cases[i].ru:cases[i].en):cases[i].digital;elapsed_text(cases[i].elapsed,number);
                if(i==0){RECT automatic=bounds(g_timer);assert(automatic.right>before.right);} /* The threshold tick, without resizing, reveals the caption. */
                LayoutFooterControls(D(width),D(550));
                RECT actual=bounds(g_timer);if(i==0)overtime=actual;else assert(EqualRect(&overtime,&actual));
                HWND buttons[]={g_refresh,g_density_button,g_language,g_theme,g_settings};RECT previous=actual;
                for(int b=0;b<5;b++){RECT current=bounds(buttons[b]);assert(previous.right<=current.left);previous=current;}
                g_hover_button=g_timer;paint_timer();g_hover_button=NULL;
                assert(painted_count>=2&&!wcscmp(painted[0].text,number));
                assert(painted[0].weight>=FW_BOLD&&painted[0].height==normal_height);
                COLORREF red=painted[0].color;assert(GetRValue(red)>GetGValue(red)*1.5&&GetRValue(red)>GetBValue(red)*1.5);
                assert(painted[1].color==TextColor()&&painted[1].weight<FW_BOLD);
                if(width==740)assert(emoji_count==1); /* Exact embedded artwork is drawn when it fits. */
            }
            g_workday_started=0;UpdateTimer();LayoutFooterControls(D(width),D(550));paint_timer();assert(painted_count==1&&painted[0].color==TextColor());
            elapsed_text(3600,text?(ru?L"1ч 0м 0с":L"1h 0m 0s"):L"01:00:00");paint_timer();assert(painted_count==1);
        }
    }
    HBITMAP cached=g_timer_emoji.bitmap;assert(cached);DWORD gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int tick=0;tick<100;tick++){
        g_workday_started=fixture_now-(9ULL*3600+tick)*10000000;UpdateTimer();paint_timer();assert(g_timer_emoji.bitmap==cached);
    }
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==gdi);
    g_workday_started=fixture_now-24ULL*3600*10000000;UpdateTimer();assert(!g_workday_started);
    WindowProcedure(g_window,WM_DESTROY,0,0);assert(!g_timer_emoji.bitmap);DestroyWindow(g_window);
    assert(DeleteFileW(g_settings_path));puts("Timer overrun: continuous seconds, threshold styling, localized formats, reset/day rollover, stable bounded footer, cached transparent emoji and shutdown release across DPI/themes");return 0;
}
