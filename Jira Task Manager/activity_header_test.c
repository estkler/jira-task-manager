#include <windows.h>
#include <assert.h>
static int observed_weight;
static COLORREF observed_color;
static wchar_t observed_text[32];
static RECT observed_alert,observed_label;
static int observed_label_right,observed_alert_height;
static RECT observed_counter;
static SIZE observed_counter_text;
static int observed_counter_font_height,observed_counter_line_height;
static HWND paint_label;
static int WINAPI ObserveText(HDC,LPCWSTR,int,LPRECT,UINT);
#define DrawTextW ObserveText
#include "native.c"
#undef DrawTextW
static int WINAPI ObserveText(HDC dc,LPCWSTR text,int length,LPRECT bounds,UINT flags)
{
    if(text&&(text[0]==L'+'||(text[0]>=L'0'&&text[0]<=L'9'))){
        wcsncpy_s(observed_text,32,text,_TRUNCATE);observed_color=GetTextColor(dc);
        LOGFONTW font;assert(GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font));observed_weight=font.lfWeight;
        if(text[0]==L'+'){observed_alert=*bounds;observed_alert_height=abs(font.lfHeight);}
        else{observed_counter=*bounds;assert(GetTextExtentPoint32W(dc,text,(int)wcslen(text),&observed_counter_text));observed_counter_font_height=abs(font.lfHeight);TEXTMETRICW metric;assert(GetTextMetricsW(dc,&metric));observed_counter_line_height=metric.tmHeight;}
    }else if(text&&text[0]){
        observed_label=*bounds;SIZE size;assert(GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size));
        observed_label_right=bounds->left+size.cx+((flags&DT_CENTER)?(bounds->right-bounds->left-size.cx)/2:0);
    }
    return DrawTextW(dc,text,length,bounds,flags);
}
static void paint(HDC dc,int id,int width,int height)
{
    observed_text[0]=0;observed_weight=0;
    memset(&observed_alert,0,sizeof(observed_alert));memset(&observed_label,0,sizeof(observed_label));
    if(id>=IDC_TAB_ALL&&id<=IDC_TAB_DONE){const wchar_t *names[]={L"Все 39",L"Текущие 9",L"Готово 30"};SetWindowTextW(paint_label,names[id-IDC_TAB_ALL]);}
    DRAWITEMSTRUCT draw={.CtlID=id,.hwndItem=paint_label,.hDC=dc,.rcItem={0,0,width,height}};DrawOwnerButton(&draw);GdiFlush();
}
static void test_no_badge_means_centered_icon(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_tasks=NULL;g_task_count=0;g_hover_button=g_keyboard_button=NULL;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();
        HBITMAP bitmap=CreateCompatibleBitmap(screen,D(36),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
        for(int scope=0;scope<2;scope++){
            g_reported_scope=scope;paint(dc,scope?IDC_TAB_REPORTED:IDC_TAB_ASSIGNED,D(36),D(28));
            assert(!observed_text[0]);RECT ink={D(36),D(28),0,0};int pixels=0;
            for(int y=D(4);y<D(24);y++)for(int x=D(4);x<D(32);x++)if(GetPixel(dc,x,y)==AccentColor()){
                ink.left=min(ink.left,x);ink.right=max(ink.right,x+1);ink.top=min(ink.top,y);ink.bottom=max(ink.bottom,y+1);pixels++;
            }
            assert(pixels>0);assert(abs(ink.left+ink.right-D(36))<=D(2));assert(abs(ink.top+ink.bottom-D(28))<=D(2));
        }
        SelectObject(dc,old);DeleteObject(bitmap);
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_readable_counter(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_dpi=144;g_density=0;g_dark=FALSE;UpdateFonts();RecreateBrushes();
    HBITMAP bitmap=CreateCompatibleBitmap(screen,D(48),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
    TaskItem task={.assigned_to_me=TRUE,.is_new=TRUE,.unread_comment=TRUE};g_tasks=&task;g_task_count=1;
    paint(dc,IDC_TAB_ASSIGNED,D(48),D(28));assert(!wcscmp(observed_text,L"1"));
    assert(observed_weight==FW_NORMAL);assert(observed_counter_font_height>=D(11));
    assert(GetRValue(observed_color)<40&&GetGValue(observed_color)<100&&GetBValue(observed_color)<160);
    assert(observed_counter.top>=0&&observed_counter.bottom<=D(28));
    g_tasks=NULL;g_task_count=0;SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_tab_updates_include_done_and_deduplicate(void)
{
    TaskItem tasks[]={
        {.assigned_to_me=TRUE,.is_new=TRUE,.unread_comment=TRUE,.status=STATUS_TODO},
        {.assigned_to_me=TRUE,.unread_comment=TRUE,.status=STATUS_HOLD},
        {.assigned_to_me=TRUE,.reported_by_me=TRUE,.unread_comment=TRUE,.status=STATUS_DONE},
        {.reported_by_me=TRUE,.is_new=TRUE,.status=STATUS_TODO},
        {.is_new=TRUE,.unread_comment=TRUE,.status=STATUS_DONE},
        {.assigned_to_me=TRUE,.status=STATUS_DONE}};
    g_tasks=tasks;g_task_count=6;g_status_count=STATUS_COUNT;g_status_categories[STATUS_TODO]=g_status_categories[STATUS_HOLD]=0;g_status_categories[STATUS_DONE]=2;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(96),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
    const wchar_t *want[2][3]={{L"+3",L"+2",L"+1"},{L"+2",L"+1",L"+1"}};
    for(int scope=0;scope<2;scope++)for(int selected=0;selected<3;selected++)for(int tab=0;tab<3;tab++){
        g_reported_scope=scope;g_tab=selected;memset(g_filter_masks,0,sizeof(g_filter_masks));
        paint(dc,IDC_TAB_ALL+tab,D(96),D(28));assert(!wcscmp(observed_text,want[scope][tab]));
        assert(observed_alert.left>=observed_label_right&&observed_alert.left-observed_label_right<=D(3));
        assert(observed_alert.top>=0&&observed_alert.top<D(6)&&observed_alert.right<=D(96));
        assert(observed_alert_height>=D(9));
    }
    for(int i=0;i<6;i++)tasks[i].is_new=tasks[i].unread_comment=FALSE;
    for(int tab=0;tab<3;tab++){paint(dc,IDC_TAB_ALL+tab,D(96),D(28));assert(!observed_text[0]);}
    g_tasks=NULL;g_task_count=0;SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_selected_tab_has_only_underline(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_dpi=144;g_density=0;g_tasks=NULL;g_task_count=0;g_hover_button=g_keyboard_button=NULL;
    HBITMAP bitmap=CreateCompatibleBitmap(screen,D(96),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
    for(int dark=0;dark<2;dark++)for(int tab=0;tab<3;tab++){
        g_dark=dark;g_tab=tab;UpdateFonts();RecreateBrushes();paint(dc,IDC_TAB_ALL+tab,D(96),D(28));
        assert(GetPixel(dc,D(3),D(14))==WindowColor());assert(GetPixel(dc,D(48),D(27))==AccentColor());
    }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_inline_counter_has_padding_and_never_covers_icon(void)
{
    g_instance=GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_hover_button=g_keyboard_button=NULL;
    TaskItem tasks[100]={0};for(int i=0;i<100;i++)tasks[i]=(TaskItem){.assigned_to_me=TRUE,.reported_by_me=TRUE,.is_new=TRUE};
    g_tasks=tasks;const int totals[]={7,27,99,100};const wchar_t *want[]={L"7",L"27",L"99",L"99"};
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();
        g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,D(510),D(420),NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();g_dpi=dpi;UpdateFonts();
        HBITMAP bitmap=CreateCompatibleBitmap(screen,D(70),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
        int single_width=0;
        for(int number=0;number<4;number++)for(int scope=0;scope<2;scope++){
            g_task_count=totals[number];g_reported_scope=scope;LayoutControls(D(510),D(420));
            assert(!GetDlgItem(g_window,4990)&&!GetDlgItem(g_window,4991));
            RECT button_bounds;GetWindowRect(scope?g_tab_reported:g_tab_assigned,&button_bounds);
            int width=button_bounds.right-button_bounds.left,height=button_bounds.bottom-button_bounds.top;
            paint(dc,scope?IDC_TAB_REPORTED:IDC_TAB_ASSIGNED,width,height);
            assert(!wcscmp(observed_text,want[number]));
            assert(observed_counter.top>=D(3)&&observed_counter.bottom<=height-D(3));
            assert(observed_counter.bottom-observed_counter.top-observed_counter_line_height>=2);
            assert(width-observed_counter.right>=D(7));
            RECT ink={width,height,0,0};int pixels=0;
            for(int y=D(5);y<height-D(5);y++)for(int x=0;x<observed_counter.left;x++)if(GetPixel(dc,x,y)==AccentColor()){
                ink.left=min(ink.left,x);ink.right=max(ink.right,x+1);ink.top=min(ink.top,y);ink.bottom=max(ink.bottom,y+1);pixels++;
            }
            assert(pixels>0&&abs(ink.right-ink.left-D(14))<=1&&abs(ink.bottom-ink.top-D(14))<=1);
            assert(ink.left>=D(7));assert(observed_counter.left-ink.right>=D(5)-1);
            assert(abs(ink.top+ink.bottom-height)<=D(2));
            assert(abs(observed_counter.top+observed_counter.bottom-height)<=1);
            if(!number)single_width=width;else assert(width>single_width);
        }
        g_task_count=0;LayoutControls(D(510),D(420));
        RECT no_count;GetWindowRect(g_tab_assigned,&no_count);assert(no_count.right-no_count.left==D(36));
        assert(no_count.right-no_count.left<single_width);
        SelectObject(dc,old);DeleteObject(bitmap);
        DestroyWindow(g_window);g_window=NULL;
    }
    g_tasks=NULL;g_task_count=0;DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_inline_toolbar_gives_unused_count_space_to_search(void)
{
    g_dpi=144;g_density=0;g_instance=GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,D(510),D(420),NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();g_dpi=144;UpdateFonts();
    TaskItem tasks[2]={{.assigned_to_me=TRUE},{.reported_by_me=TRUE}};g_tasks=tasks;g_task_count=2;
    HWND tabs[]={g_tab_all,g_tab_current,g_tab_done};RECT baseline[3];int zero_search_width=0;
    for(int density=0;density<2;density++){
        g_density=density;UpdateFonts();
        for(int scope=0;scope<2;scope++)for(int language=0;language<2;language++)for(int activity=0;activity<4;activity++){
            g_reported_scope=scope;g_russian=language;tasks[0].is_new=activity&1;tasks[1].unread_comment=activity&2;UpdateLanguage();LayoutControls(D(510),D(420));
            RECT assigned,reported;GetWindowRect(g_tab_assigned,&assigned);GetWindowRect(g_tab_reported,&reported);
            assert(assigned.right==reported.left&&assigned.top==reported.top);
            int assigned_width=assigned.right-assigned.left,reported_width=reported.right-reported.left;
            if(activity&1)assert(assigned_width>D(36));else assert(assigned_width==D(36));
            if(activity&2)assert(reported_width>D(36));else assert(reported_width==D(36));
            RECT search;GetWindowRect(g_search,&search);int search_width=search.right-search.left;
            if(!activity)zero_search_width=search_width;else assert(search_width<zero_search_width);
            for(int i=0;i<3;i++){RECT rect;GetWindowRect(tabs[i],&rect);assert(rect.top==assigned.top);if(!scope&&!language&&!activity)baseline[i]=rect;else assert(EqualRect(&rect,&baseline[i]));}
            MapWindowPoints(NULL,g_window,(POINT*)&assigned,2);assert(assigned.top==D(10));
        }
    }
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);g_window=NULL;
}
static void test_modes_share_one_frame_even_without_notifications(void)
{
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);g_tasks=NULL;g_task_count=0;g_hover_button=g_keyboard_button=NULL;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();
        HBITMAP bitmap=CreateCompatibleBitmap(screen,D(36),D(28));HGDIOBJ old=SelectObject(dc,bitmap);
        for(int scope=0;scope<2;scope++){
            g_reported_scope=!scope;paint(dc,scope?IDC_TAB_REPORTED:IDC_TAB_ASSIGNED,D(36),D(28));assert(!observed_text[0]);
            assert(GetPixel(dc,scope?D(36)-1:0,D(14))==BorderColor());
            assert(GetPixel(dc,D(18),D(2))==BorderColor());
        }
        SelectObject(dc,old);DeleteObject(bitmap);
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
}
static void test_read_activity_releases_its_slot_immediately(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_density=1;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,510,420,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    wchar_t temporary[MAX_PATH];assert(GetTempPathW(_countof(temporary),temporary));assert(GetTempFileNameW(temporary,L"ach",0,g_settings_path));
    wcscpy_s(g_snapshot.account,_countof(g_snapshot.account),L"offline-header-test");
    TaskItem tasks[]={{.key=L"HEADER-1",.assigned_to_me=TRUE,.is_new=TRUE},{.key=L"HEADER-2",.reported_by_me=TRUE,.unread_comment=TRUE,.latest_comment_id=L"100"}};
    g_tasks=tasks;g_task_count=2;LayoutControls(510,420);
    RECT assigned,reported;GetWindowRect(g_tab_assigned,&assigned);GetWindowRect(g_tab_reported,&reported);
    assert(assigned.right-assigned.left>D(36)&&reported.right-reported.left>D(36));
    MarkTaskSeen(0);assert(!tasks[0].is_new);GetWindowRect(g_tab_assigned,&assigned);assert(assigned.right-assigned.left==D(36));
    MarkCommentSeen(1);assert(!tasks[1].unread_comment);GetWindowRect(g_tab_reported,&reported);assert(reported.right-reported.left==D(36));
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);g_window=NULL;
    assert(DeleteFileW(g_settings_path));g_settings_path[0]=0;g_snapshot.account[0]=0;
}
int main(int argc,char **argv)
{
    paint_label=CreateWindowExW(0,L"BUTTON",L"",WS_POPUP,0,0,160,40,NULL,NULL,GetModuleHandleW(NULL),NULL);assert(paint_label);
    if(argc<2||!strcmp(argv[1],"zero"))test_no_badge_means_centered_icon();
    if(argc<2||!strcmp(argv[1],"counter"))test_readable_counter();
    if(argc<2||!strcmp(argv[1],"tabs"))test_tab_updates_include_done_and_deduplicate();
    if(argc<2||!strcmp(argv[1],"selection"))test_selected_tab_has_only_underline();
    if(argc<2||!strcmp(argv[1],"compact"))test_inline_toolbar_gives_unused_count_space_to_search();
    if(argc<2||!strcmp(argv[1],"pair"))test_inline_counter_has_padding_and_never_covers_icon();
    if(argc<2||!strcmp(argv[1],"inactive"))test_modes_share_one_frame_even_without_notifications();
    if(argc<2||!strcmp(argv[1],"seen"))test_read_activity_releases_its_slot_immediately();
    DestroyWindow(paint_label);
    puts("Preview: joined mode frame, inline counters without overlap, zero has no reserved count slot, unused width returns to search; tab updates unchanged");
}
