#include <windows.h>
#include <assert.h>
#include <stdio.h>
static ULONGLONG input_now;
static SYSTEMTIME input_local={.wYear=2026,.wMonth=10,.wDay=29,.wHour=18};
static void WINAPI InputClock(LPFILETIME value){ULARGE_INTEGER n;n.QuadPart=input_now;value->dwLowDateTime=n.LowPart;value->dwHighDateTime=n.HighPart;}
static void WINAPI InputLocal(LPSYSTEMTIME value){*value=input_local;}
static BOOL InputNoAccount(void){return FALSE;}
static BOOL InputNoUrl(wchar_t *url,size_t count){if(count)url[0]=0;return FALSE;}
static BOOL WINAPI InputMenu(HMENU,UINT,int,int,int,HWND,const RECT*);
static int menu_count;
static int stamp_moves,stamp_texts;static BOOL stamp_copies;
static BOOL stamp_probe;
static HWND stamp_window;static BOOL stamp_invalidated;
static BOOL WINAPI InputInvalidate(HWND window,const RECT *rect,BOOL erase){
    if(stamp_probe&&window==stamp_window&&!rect)stamp_invalidated=TRUE;
    return InvalidateRect(window,rect,erase);
}
#define GetSystemTimeAsFileTime InputClock
#define GetLocalTime InputLocal
#define JiraHasAccount InputNoAccount
#define JiraGetUrl InputNoUrl
#define TrackPopupMenu InputMenu
#define InvalidateRect InputInvalidate
#include "native.c"
static BOOL WINAPI InputMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){
    (void)x;(void)y;(void)reserved;(void)rect;assert(owner==g_window&&(flags&TPM_RETURNCMD));menu_count++;
    assert(GetMenuItemCount(menu)==2&&"Left-click timer menu must contain only start-time editing and work-time analysis");
    const UINT ids[]={2202,2203};
    for(int i=0;i<2;i++){
        MENUITEMINFOW item={.cbSize=sizeof(item),.fMask=MIIM_DATA};assert(GetMenuItemID(menu,i)==ids[i]&&GetMenuItemInfoW(menu,i,TRUE,&item));
        PopupItem *data=(PopupItem*)item.dwItemData;assert(data&&data->plain);
        const wchar_t *label=i?(g_russian?L"Анализ рабочего времени ↗":L"Work time analysis ↗"):(g_russian?L"Изменить начало дня…":L"Change start time…");
        assert(!wcscmp(data->text,label));
    }return 0; /* Cancel at the external popup boundary; never open a browser or edit real data. */
}
static LRESULT CALLBACK InputProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CREATE){g_window=window;CreateControls();return 0;}
    if(message==WM_DESTROY)return 0;
    return WindowProcedure(window,message,wp,lp);
}
static LRESULT CALLBACK StampProbe(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    if(stamp_probe&&message==WM_WINDOWPOSCHANGING){WINDOWPOS *position=(WINDOWPOS*)lp;stamp_moves++;if(!(position->flags&SWP_NOCOPYBITS))stamp_copies=TRUE;}
    if(stamp_probe&&message==WM_SETTEXT)stamp_texts++;
    return DefSubclassProc(window,message,wp,lp);
}
static ULONGLONG StampPixels(void){
    RECT rect;GetClientRect(g_updated,&rect);HDC dc=CreateCompatibleDC(NULL);void *pixels;BITMAPINFO info={0};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=rect.right;info.bmiHeader.biHeight=-rect.bottom;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);
    HGDIOBJ old=SelectObject(dc,bitmap);size_t bytes=(size_t)rect.right*rect.bottom*4;memset(pixels,0x7f,bytes);
    DRAWITEMSTRUCT draw={.CtlID=IDC_UPDATED,.hwndItem=g_updated,.hDC=dc,.rcItem=rect};DrawUpdatedLabel(&draw);GdiFlush();
    ULONGLONG hash=1469598103934665603ULL;for(size_t i=0;i<bytes;i++)hash=(hash^((BYTE*)pixels)[i])*1099511628211ULL;
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return hash;
}
static void footer_stability(void){
    g_jira_busy=g_jira_stale=g_status_sync_warning=FALSE;g_updated_time=(SYSTEMTIME){.wYear=2026,.wMonth=11,.wDay=5,.wHour=15,.wMinute=48,.wSecond=18};
    SYSTEMTIME updated=g_updated_time;g_workday_started=input_now-3723ULL*10000000;ULONGLONG start=g_workday_started;
    SetWindowSubclass(g_updated,StampProbe,99,0);
    stamp_window=g_updated;
    WNDCLASSW settings_class={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"FooterSettingsFixture"};RegisterClassW(&settings_class);
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_russian=ru;g_dark=dark;UpdateFonts();RecreateBrushes();
        SetWindowPos(g_window,NULL,0,0,D(510),D(550),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);g_footer_right=0;
        g_text_time=g_bar_time=FALSE;UpdateTimer();LayoutFooterControls(D(510),D(550));RECT baseline;GetWindowRect(g_updated,&baseline);
        wchar_t caption[128],actual[128];GetWindowTextW(g_updated,caption,128);
        ULONGLONG pixels=StampPixels();
        for(int mode=0;mode<6;mode++){
            g_text_time=mode%3==1;g_bar_time=mode%3==2;UpdateTimer();stamp_moves=stamp_texts=0;stamp_copies=stamp_invalidated=FALSE;stamp_probe=TRUE;
            LayoutFooterControls(D(510),D(550));stamp_probe=FALSE;
            RECT after;GetWindowRect(g_updated,&after);GetWindowTextW(g_updated,actual,128);
            assert(EqualRect(&baseline,&after)&&"Timer display mode must not move or resize the last-updated region");
            assert(!wcscmp(caption,actual)&&"Timer display mode must not change the last-updated caption");
            assert(StampPixels()==pixels&&"Timestamp rendering must remain pixel-identical across timer formats");
            assert(stamp_moves>0&&!stamp_copies&&"Owner-drawn footer moves must discard copied pixels");
            assert(stamp_invalidated&&"The entire timestamp must be invalidated after layout, even when its offscreen parent is hidden");
            stamp_texts=0;stamp_probe=TRUE;UpdateLastUpdated();stamp_probe=FALSE;
            assert(stamp_texts<=1&&"A refresh-label update must publish only its final fitted caption");
        }
        RECT outer=SettingsOuterRect();HWND settings=CreateWindowW(settings_class.lpszClassName,L"",WS_CAPTION|WS_SYSMENU,30000,30000,outer.right-outer.left,outer.bottom-outer.top,g_window,NULL,g_instance,NULL);assert(settings);
        SendMessageW(settings,WM_COMMAND,IDCANCEL,0);assert(!IsWindow(settings));
        RECT after;GetWindowRect(g_updated,&after);GetWindowTextW(g_updated,actual,128);
        assert(EqualRect(&baseline,&after)&&!wcscmp(caption,actual)&&"Closing unchanged Settings must not alter the timestamp");
        assert(StampPixels()==pixels&&"Settings Cancel must not change timestamp pixels");
        assert(g_workday_started==start&&!memcmp(&updated,&g_updated_time,sizeof(updated)));
    }
    g_russian=TRUE;g_status_sync_warning=TRUE;UpdateLastUpdated();wchar_t state[128];GetWindowTextW(g_updated,state,128);assert(wcsstr(state,L"статусы не подтверждены"));
    g_status_sync_warning=FALSE;g_completion_notice_until=GetTickCount64()+10000;wcscpy_s(g_completion_notice,128,L"Fixture completion notice");UpdateLastUpdated();GetWindowTextW(g_updated,state,128);assert(!wcscmp(state,L"Fixture completion notice"));
    g_completion_notice_until=0;g_jira_stale=TRUE;UpdateLastUpdated();GetWindowTextW(g_updated,state,128);assert(wcsstr(state,L"ошибка"));
    g_jira_stale=FALSE;RemoveWindowSubclass(g_updated,StampProbe,99);g_jira_busy=TRUE;UpdateLastUpdated();GetWindowTextW(g_updated,state,128);assert(wcsstr(state,L"загрузка"));
    puts("Last-updated geometry, caption and pixels stay independent of timer modes and Settings Cancel; Jira state priority preserved");
}
static void timer_input(void){
    for(int language=0;language<2;language++){
        g_russian=language;g_text_time=g_bar_time=FALSE;g_workday_started=input_now-(ULONGLONG)3723*10000000;ULONGLONG start=g_workday_started;UpdateTimer();
        wchar_t text[128];GetWindowTextW(g_timer,text,128);assert(!wcscmp(text,L"01:02:03"));
        int menus=menu_count;SendMessageW(g_timer,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(3,3));SendMessageW(g_timer,WM_RBUTTONUP,0,MAKELPARAM(3,3));
        assert(g_text_time&&"A real right-click on the timer must directly toggle its format");
        assert(menu_count==menus&&g_workday_started==start&&GetPrivateProfileIntW(L"Timer",L"Text",0,g_settings_path)==1);
        GetWindowTextW(g_timer,text,128);assert(!wcscmp(text,language?L"1ч 2м 3с":L"1h 2m 3s"));
        g_text_time=FALSE;LoadSettings();assert(g_text_time&&g_workday_started==start);
        SendMessageW(g_timer,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(3,3));SendMessageW(g_timer,WM_RBUTTONUP,0,MAKELPARAM(3,3));
        assert(!g_text_time&&g_bar_time&&g_workday_started==start&&GetPrivateProfileIntW(L"Timer",L"Text",1,g_settings_path)==0);
        GetWindowTextW(g_timer,text,128);assert(!wcscmp(text,L"01:02:03"));
        SendMessageW(g_timer,WM_CONTEXTMENU,(WPARAM)g_timer,MAKELPARAM(-1,-1));assert(!g_text_time&&!g_bar_time&&"Keyboard context gesture cycles bar back to numeric");
        SendMessageW(g_timer,WM_CONTEXTMENU,(WPARAM)g_timer,MAKELPARAM(-1,-1));assert(g_text_time);
        SendMessageW(g_language,WM_RBUTTONUP,0,MAKELPARAM(3,3));assert(g_text_time);
        menus=menu_count;SendMessageW(g_window,WM_COMMAND,MAKEWPARAM(IDC_TIMER,BN_CLICKED),(LPARAM)g_timer);
        assert(menu_count==menus+1&&g_text_time&&g_workday_started==start);
        g_workday_started=0;SendMessageW(g_timer,WM_RBUTTONUP,0,MAKELPARAM(3,3));assert(!g_workday_started);
    }
}
static void footer_timestamp_fit(void)
{
    g_jira_busy=g_jira_stale=FALSE;g_updated_time=(SYSTEMTIME){.wYear=2026,.wMonth=11,.wDay=5,.wHour=15,.wMinute=48,.wSecond=18};
    SYSTEMTIME before=g_updated_time;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int ru=0;ru<2;ru++){
        g_dpi=dpi;g_density=density;g_russian=ru;UpdateFonts();
        for(int width=300;width<=650;width++){
            SetWindowPos(g_window,NULL,0,0,D(width),D(550),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            g_workday_started=input_now-3723ULL*10000000;g_text_time=g_bar_time=FALSE;UpdateTimer();
            for(int mode=0;mode<2;mode++){
                /* Input persistence is covered above; sweep geometry without thousands of INI writes. */
                g_text_time=mode;UpdateTimer();g_footer_right=0;LayoutFooterControls(D(width),D(550));
                RECT control;GetClientRect(g_updated,&control);RECT actual=UpdatedTextRect(control);
                HDC dc=GetDC(g_window);HGDIOBJ old=SelectObject(dc,g_small_font);SIZE size,minimum;
                wchar_t text[128];GetWindowTextW(g_updated,text,128);GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size);
                GetTextExtentPoint32W(dc,L"15:48:18",8,&minimum);SelectObject(dc,old);ReleaseDC(g_window,dc);
                if(actual.right-actual.left>=minimum.cx){
                    if(size.cx>actual.right-actual.left)fprintf(stderr,"Timestamp clipped: dpi=%d density=%d ru=%d width=%d text=%ls measured=%ld actual=%ld\n",dpi,density,ru,width,text,size.cx,actual.right-actual.left);
                    assert(size.cx<=actual.right-actual.left&&"Format switching must keep all timestamp digits inside the actual padded draw rectangle");
                    assert(wcsstr(text,L"15:48:18"));
                }
                assert(!memcmp(&before,&g_updated_time,sizeof(before))&&"Changing timer format must not change the last refresh time");
            }
        }
    }
    g_jira_busy=TRUE;puts("Timer format preserves full timestamp at all footer width boundaries");
}
static void move_at(int x,int y){SendMessageW(g_list,WM_MOUSEMOVE,0,MAKELPARAM(x,y));}
static void row_input(void){
    TaskItem tasks[]={
        {.key=L"HOVER-1",.summary=L"First",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"HOVER-2",.summary=L"Second",.status=STATUS_HOLD,.assigned_to_me=TRUE}
    };
    g_tasks=tasks;g_task_count=2;g_tab=1;g_sort_column=-1;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();SetWindowPos(g_window,NULL,30000,30000,D(740),D(550),SWP_NOZORDER|SWP_NOACTIVATE);
        LayoutControls(D(740),D(550));RefreshTaskList();
        int widths[]={26,100,160,20,120};for(int col=0;col<5;col++)ListView_SetColumnWidth(g_list,col,D(widths[col]));
        RECT client;GetClientRect(g_list,&client);RECT last={0};
        for(int row=0;row<2;row++){
            RECT bounds;assert(ListView_GetItemRect(g_list,row,&bounds,LVIR_BOUNDS));assert(bounds.right<client.right-D(30));int y=(bounds.top+bounds.bottom)/2;
            move_at(client.right-1,y);assert(g_hover_row==row&&"Free space beyond the last column must hover the same full row");
            assert(!g_hover_status&&!g_hover_key&&!g_hover_note&&!g_hover_comment);
            HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=client.right;info.bmiHeader.biHeight=-client.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void *pixels;
            HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
            NMLVCUSTOMDRAW draw={0};draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;draw.nmcd.hdc=dc;assert(DrawListCustom(&draw)==CDRF_SKIPDEFAULT);
            assert(GetPixel(dc,client.right-1,y)==(dark?RGB(33,37,43):RGB(247,249,251))&&"Hover background must actually reach the right edge");
            SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
            RECT pill;assert(StatusPillRectForRow(row,&pill));move_at((pill.left+pill.right)/2,y);assert(g_hover_row==row&&g_hover_status);
            move_at(client.right-1,y);assert(g_hover_row==row&&!g_hover_status);
            move_at(client.right,y);assert(g_hover_row==-1);move_at(-1,y);assert(g_hover_row==-1);last=bounds;
        }
        move_at(client.right-1,last.bottom+D(4));assert(g_hover_row==-1);
        RECT first;assert(ListView_GetItemRect(g_list,0,&first,LVIR_BOUNDS));
        move_at(client.right-1,first.top>0?first.top-1:-1);assert(g_hover_row==-1);
        move_at(client.right-1,(last.top+last.bottom)/2);SendMessageW(g_list,WM_MOUSELEAVE,0,0);assert(g_hover_row==-1);
    }g_tasks=NULL;g_task_count=0;
}
int main(int argc,char **argv){
    FILETIME local,utc;assert(SystemTimeToFileTime(&input_local,&local)&&LocalFileTimeToFileTime(&local,&utc));ULARGE_INTEGER value={.LowPart=utc.dwLowDateTime,.HighPart=utc.dwHighDateTime};input_now=value.QuadPart;
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&controls));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp)&&GetTempFileNameW(temp,L"tri",0,g_settings_path));
    WNDCLASSW cls={.lpfnWndProc=InputProcedure,.hInstance=g_instance,.lpszClassName=L"TimerRowInputFixture"};assert(RegisterClassW(&cls));
    HWND window=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(window);
    if(argc<2||!strcmp(argv[1],"timer")){timer_input();footer_stability();footer_timestamp_fit();}
    if(argc<2||!strcmp(argv[1],"row"))row_input();
    DestroyWindow(window);assert(DeleteFileW(g_settings_path));puts("Timer direct right-click and full-width row hover passed");return 0;
}
