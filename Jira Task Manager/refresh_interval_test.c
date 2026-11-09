#include <windows.h>
#include <stddef.h>
static BOOL RefreshNoAccount(void){return FALSE;}
static BOOL RefreshNoUrl(wchar_t *url,size_t count){if(count)url[0]=0;return FALSE;}
#define JiraHasAccount RefreshNoAccount
#define JiraGetUrl RefreshNoUrl
#include "native.c"
#include <assert.h>
/* Only hidden fixture windows and a temporary profile are used. Never connect. */
static void stored_intervals(void)
{
    /* Catch discarded legacy values, missed upper clamping, and rounding down. */
    struct {const wchar_t *stored;int expected;} cases[]={
        {NULL,30},{L"1",15},{L"14",15},{L"15",15},{L"17",15},{L"18",20},
        {L"22",20},{L"23",25},{L"30",30},{L"42",40},{L"43",45},{L"45",45},
        {L"46",45},{L"1440",45},{L"1441",45},{L"0",15},{L"-1",15},
        {L"2147483647",45},{L"-2147483648",15}};
    for(int i=0;i<_countof(cases);i++){
        assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",cases[i].stored,g_settings_path));
        LoadSettings();
        if(g_refresh_minutes!=cases[i].expected){fprintf(stderr,"Stored case %d: expected %d, got %d\n",i,cases[i].expected,g_refresh_minutes);abort();}
    }
    puts("Refresh load: missing defaults to 30; saved integers clamp and round to 15..45 in steps of 5");
}
static RECT child_rect(HWND child,HWND parent)
{
    RECT r;assert(GetWindowRect(child,&r));MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;
}
static void readable_caption(HWND control)
{
    wchar_t text[128];GetWindowTextW(control,text,_countof(text));
    RECT bounds;GetClientRect(control,&bounds);HDC dc=GetDC(control);
    HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(control,WM_GETFONT,0,0));SIZE size;
    assert(GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size));
    assert(size.cx<=bounds.right&&size.cy<=bounds.bottom);
    SelectObject(dc,old);ReleaseDC(control,dc);
}
static void slider_preferences(void)
{
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"RefreshIntervalFixture"};assert(RegisterClassW(&cls));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;g_density=density;g_refresh_minutes=30;RecreateBrushes();
        assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",L"30",g_settings_path));
        HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
        wchar_t name[64];GetClassNameW(s_interval,name,64);
        assert(!wcscmp(name,TRACKBAR_CLASSW)); /* Catch reintroduction of free text. */
        assert(GetWindowLongPtrW(s_interval,GWL_STYLE)&WS_TABSTOP);
        assert(SendMessageW(s_interval,TBM_GETRANGEMIN,0,0)==15&&SendMessageW(s_interval,TBM_GETRANGEMAX,0,0)==45);
        assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==30);
        int caption_ids[]={221,222,226,227,228,229};
        for(int i=0;i<_countof(caption_ids);i++)readable_caption(GetDlgItem(settings,caption_ids[i]));
        for(int connected=0;connected<2;connected++){
            s_connected=connected;LayoutSettings();
            RECT slider=child_rect(s_interval,settings),value=child_rect(GetDlgItem(settings,226),settings);
            RECT note=child_rect(s_note,settings),footer=child_rect(GetDlgItem(settings,IDOK),settings);
            RECT client;GetClientRect(settings,&client);assert(note.bottom<=client.bottom-S(52));
            assert(value.bottom<=slider.top&&value.right<=slider.right);
            assert(slider.right-slider.left==S(SETTINGS_WIDTH_DIP-52)&&note.bottom<footer.top-S(4));
            int ids[]={227,228,229};const wchar_t *labels[]={L"15",L"30",L"45"};
            for(int i=0;i<3;i++){
                HWND mark=GetDlgItem(settings,ids[i]);assert(mark);GetWindowTextW(mark,name,64);assert(!wcscmp(name,labels[i]));
                RECT r=child_rect(mark,settings);assert(r.top>=slider.bottom&&r.bottom<=note.top);
            }
        }
        /* Native keys select the same seven values as dragging. */
        SendMessageW(s_interval,WM_KEYDOWN,VK_HOME,0);assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==15);
        for(int position=0;position<7;position++){
            if(position)SendMessageW(s_interval,WM_KEYDOWN,VK_RIGHT,0);
            int minutes=15+5*position;wchar_t expected[32];swprintf(expected,32,ru?L"%d мин":L"%d min",minutes);
            GetWindowTextW(GetDlgItem(settings,226),name,64);assert(!wcscmp(name,expected));
            readable_caption(GetDlgItem(settings,226));
            assert(g_refresh_minutes==30); /* A draft must not change the active timer. */
            assert(UpdateSettingsDirty()==(minutes!=30));
        }
        SendMessageW(s_interval,TBM_SETPOS,TRUE,24);SendMessageW(settings,WM_HSCROLL,TB_THUMBTRACK,(LPARAM)s_interval);
        assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==25);GetWindowTextW(GetDlgItem(settings,226),name,64);assert(wcsstr(name,L"25"));
        SendMessageW(s_interval,WM_KEYDOWN,VK_END,0);assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==45);
        SendMessageW(s_interval,WM_KEYDOWN,VK_PRIOR,0);assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==40);
        SendMessageW(s_interval,WM_KEYDOWN,VK_NEXT,0);assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==45);
        g_russian=!ru;UpdateSettingsLanguage();GetWindowTextW(GetDlgItem(settings,226),name,64);assert(!wcscmp(name,ru?L"45 min":L"45 мин"));
        g_russian=ru;UpdateSettingsLanguage();
        FreezeSettings(TRUE);assert(!IsWindowEnabled(s_interval));FreezeSettings(FALSE);assert(IsWindowEnabled(s_interval));
        SendMessageW(GetDlgItem(settings,213),TBM_SETPOS,TRUE,1);SetWindowTextW(SettingsItem(259),L"1");
        for(int position=0;position<7;position++){
            SendMessageW(s_interval,TBM_SETPOS,TRUE,15+position*5);assert(SaveNativePreferences());
            int minutes=15+position*5;assert(g_refresh_minutes==minutes);
            LoadSettings();assert(g_refresh_minutes==minutes&&g_tray_task_count==1);
        }
        SendMessageW(s_interval,TBM_SETPOS,TRUE,15);DestroyWindow(settings);
        LoadSettings();assert(g_refresh_minutes==45); /* Closing drops the unsaved 15. */
    }
    /* A reopened window must show the saved choice, rather than the default. */
    HWND reopened=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(reopened);
    assert(SendMessageW(s_interval,TBM_GETPOS,0,0)==45);DestroyWindow(reopened);
    DestroyWindow(g_window);
    puts("Refresh slider: seven keyboard values, localized live readout, saved/reloaded selection, canceled draft, layouts/themes/DPI/densities and unrelated numeric preferences");
}
int main(int argc,char **argv)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&controls));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"rfi",0,g_settings_path));
    if(argc<2||!strcmp(argv[1],"load"))stored_intervals();
    if(argc<2||!strcmp(argv[1],"ui"))slider_preferences();
    assert(DeleteFileW(g_settings_path));return 0;
}
