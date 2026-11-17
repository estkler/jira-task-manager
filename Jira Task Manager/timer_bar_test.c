#define main LegacyTimerInputRegressionUnused
#include "timer_row_input_test.c"
#undef main
static void tooltip_live(void)
{
    g_russian=TRUE;g_bar_time=TRUE;g_text_time=FALSE;
    g_workday_started=input_now-(3ULL*3600+12*60+46)*10000000;UpdateTimer();
    NMTTDISPINFOW tip={.hdr={.hwndFrom=g_tooltips,.idFrom=(UINT_PTR)g_timer,.code=TTN_GETDISPINFOW}};
    SendMessageW(g_window,WM_NOTIFY,0,(LPARAM)&tip);
    assert(tip.lpszText&&wcsstr(tip.lpszText,L"35% рабочего дня")&&!wcsstr(tip.lpszText,L"03:12:46"));
    g_bar_time=FALSE;UpdateTimer();
    TOOLINFOW tool={.cbSize=sizeof(tool)-sizeof(void*),.hwnd=g_window,.uId=(UINT_PTR)g_timer,
        .uFlags=TTF_IDISHWND|TTF_TRACK|TTF_ABSOLUTE,.lpszText=LPSTR_TEXTCALLBACKW};
    SendMessageW(g_tooltips,TTM_DELTOOLW,0,(LPARAM)&tool);assert(SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&tool));
    ShowWindow(g_window,SW_SHOWNOACTIVATE);SendMessageW(g_tooltips,TTM_TRACKACTIVATE,TRUE,(LPARAM)&tool);
    wchar_t text[256];GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"03:12:46"));
    input_now+=10000000;SendMessageW(g_window,WM_TIMER,1,0);
    GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"03:12:47")&&"The visible tooltip must tick without moving the pointer");
    g_text_time=TRUE;UpdateTimer();input_now+=10000000;SendMessageW(g_window,WM_TIMER,1,0);
    GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"3ч 12м 48с"));
    g_bar_time=TRUE;g_text_time=FALSE;g_russian=FALSE;UpdateTimer();
    GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"35% of the workday")&&!wcsstr(text,L"Elapsed"));
    input_now+=200ULL*10000000;SendMessageW(g_window,WM_TIMER,1,0);
    GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"36% of the workday")&&IsWindowVisible(g_tooltips));
    g_workday_started=input_now-10ULL*3600*10000000;UpdateTimer();
    GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"100% of the workday"));
    g_workday_started=0;UpdateTimer();GetWindowTextW(g_tooltips,text,_countof(text));assert(wcsstr(text,L"0% of the workday"));
    SendMessageW(g_tooltips,TTM_TRACKACTIVATE,FALSE,(LPARAM)&tool);ShowWindow(g_window,SW_HIDE);UpdateTooltips();
}
static void bar_pixels(int elapsed)
{
    g_workday_started=input_now-(ULONGLONG)elapsed*10000000ULL;g_bar_time=TRUE;g_text_time=FALSE;UpdateTimer();
    assert(g_timer_bar_amount==(elapsed>=9*3600?1000:MulDiv(elapsed,1000,9*3600)));
    HDC screen=GetDC(g_window),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(330),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
    RECT rect={0,0,D(330),D(28)};FillRect(dc,&rect,g_window_brush);DrawTimerCaption(dc,rect,TextColor());
    const wchar_t *sample=g_russian?L"0ч 00м 00с":L"0h 00m 00s";
    HGDIOBJ font=SelectObject(dc,g_timer_font);SIZE digits;GetTextExtentPoint32W(dc,sample,(int)wcslen(sample),&digits);SelectObject(dc,font);
    COLORREF left=GetPixel(dc,digits.cx/4,D(14)),right=GetPixel(dc,digits.cx*3/4,D(14));
    if(elapsed<9*3600)assert(GetPixel(dc,digits.cx+D(3),D(14))==WindowColor()&&"The bar must occupy the width of the text clock");
    assert(elapsed==4*3600+1800?left!=right:left==right);
    COLORREF track=g_dark?RGB(52,58,67):RGB(224,229,235);
    COLORREF fill=elapsed>=9*3600?(g_dark?RGB(115,63,70):RGB(245,183,188)):(g_dark?RGB(51,92,125):RGB(182,217,245));
    assert(left==(elapsed?fill:track)&&"Use the approved soft fill rather than the saturated accent");
    int ink=0;
    for(int y=D(10);y<D(18);y++)for(int x=digits.cx/2-D(10);x<digits.cx/2+D(10);x++){
        COLORREF pixel=GetPixel(dc,x,y);if(pixel!=WindowColor()&&pixel!=track&&pixel!=fill)ink++;
    }
    assert(ink>0&&"The percentage must be visible in the center of the bar");
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(g_window,screen);
    wchar_t accessible[128];GetWindowTextW(g_timer,accessible,128);assert(wcsstr(accessible,elapsed==10*3600?L"10:00:00":elapsed==9*3600?L"09:00:00":elapsed?L"04:30:00":L"00:00:00"));
}
int main(void)
{
    FILETIME local,utc;assert(SystemTimeToFileTime(&input_local,&local)&&LocalFileTimeToFileTime(&local,&utc));input_now=((ULONGLONG)utc.dwHighDateTime<<32)|utc.dwLowDateTime;
    g_instance=GetModuleHandleW(NULL);g_dpi=96;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    wchar_t dir[MAX_PATH];GetTempPathW(MAX_PATH,dir);GetTempFileNameW(dir,L"tbr",0,g_settings_path);
    WNDCLASSW cls={.lpfnWndProc=InputProcedure,.hInstance=g_instance,.lpszClassName=L"TimerBarFixture"};RegisterClassW(&cls);
    g_window=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);
    g_jira_busy=g_jira_stale=FALSE;g_updated_time=(SYSTEMTIME){.wYear=2026,.wMonth=10,.wDay=29,.wHour=16,.wMinute=20,.wSecond=30};
    g_workday_started=input_now-3600ULL*10000000;ULONGLONG start=g_workday_started;
    g_text_time=g_bar_time=FALSE;ToggleTimerFormat();assert(g_text_time&&!g_bar_time);ToggleTimerFormat();assert(!g_text_time&&g_bar_time);
    assert(GetPrivateProfileIntW(L"Timer",L"Format",-1,g_settings_path)==2);g_bar_time=FALSE;LoadSettings();assert(g_bar_time&&!g_text_time&&g_workday_started==start);
    ToggleTimerFormat();assert(!g_bar_time&&!g_text_time&&g_workday_started==start);
    WritePrivateProfileStringW(L"Timer",L"Format",NULL,g_settings_path);WritePrivateProfileStringW(L"Timer",L"Text",L"1",g_settings_path);LoadSettings();assert(g_text_time&&!g_bar_time);
    WritePrivateProfileStringW(L"Timer",L"Format",L"99",g_settings_path);LoadSettings();assert(g_text_time&&!g_bar_time);
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;UpdateFonts();RecreateBrushes();SetWindowPos(g_window,NULL,0,0,D(510),D(550),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        const int elapsed[]={0,4*3600+1800,9*3600,10*3600};for(int i=0;i<4;i++)bar_pixels(elapsed[i]);
        g_workday_started=input_now-3600ULL*10000000;g_text_time=g_bar_time=FALSE;UpdateTimer();g_footer_right=0;LayoutFooterControls(D(510),D(550));RECT original;GetWindowRect(g_updated,&original);ULONGLONG pixels=StampPixels();
        wchar_t before[128],after[128];GetWindowTextW(g_updated,before,128);
        for(int i=0;i<3;i++){ToggleTimerFormat();RECT current;GetWindowRect(g_updated,&current);GetWindowTextW(g_updated,after,128);assert(EqualRect(&original,&current)&&!wcscmp(before,after)&&StampPixels()==pixels);}
    }
    tooltip_live();
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Workday bar: saved modes, native pixels, stable footer and live percentage/time tooltip passed");
}
