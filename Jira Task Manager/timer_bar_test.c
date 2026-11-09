#define main LegacyTimerInputRegressionUnused
#include "timer_row_input_test.c"
#undef main
static void bar_pixels(int elapsed)
{
    g_workday_started=input_now-(ULONGLONG)elapsed*10000000ULL;g_bar_time=TRUE;g_text_time=FALSE;UpdateTimer();
    assert(g_timer_bar_amount==(elapsed>=9*3600?1000:MulDiv(elapsed,1000,9*3600)));
    HDC screen=GetDC(g_window),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(330),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
    RECT rect={0,0,D(330),D(28)};FillRect(dc,&rect,g_window_brush);DrawTimerCaption(dc,rect,TextColor());
    COLORREF left=GetPixel(dc,D(25),D(14)),right=GetPixel(dc,D(85),D(14));
    assert(elapsed==4*3600+1800?left!=right:left==right);
    if(elapsed>=9*3600)assert(left==(g_dark?RGB(255,111,116):RGB(190,30,40)));
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
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Workday bar: three saved modes, legacy migration, nine-hour clamp with continuous elapsed time, themed native pixels and stable Updated footer passed");
}
