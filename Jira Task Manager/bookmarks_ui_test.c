#include <windows.h>
#include <assert.h>
static int fixture_errors;
static int WINAPI FixtureMessageBox(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){(void)owner;(void)text;(void)title;(void)flags;fixture_errors++;return IDOK;}
#define MessageBoxW FixtureMessageBox
static void (*fixture_menu_callback)(void);
static UINT fixture_menu_command;
static BOOL WINAPI FixtureTrackPopupMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){
    (void)menu;(void)flags;(void)x;(void)y;(void)reserved;(void)owner;(void)rect;
    if(fixture_menu_callback)fixture_menu_callback();return fixture_menu_command;
}
#define TrackPopupMenu FixtureTrackPopupMenu
static const wchar_t *fixture_url=L"https://jira.example.test/jira/";
static BOOL FixtureUrl(wchar_t *url,size_t capacity){wcscpy_s(url,capacity,fixture_url);return TRUE;}
#define JiraGetUrl FixtureUrl
#include "native.c"
#include "native_bookmarks.h"
/* Uses only its own hidden window: no tray, account, clock or network startup. */
static LRESULT CALLBACK FixtureProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CREATE){g_window=window;CreateControls();return 0;}
    if(message==WM_DESTROY)return 0;
    return WindowProcedure(window,message,wp,lp);
}
static RECT bounds(HWND window){RECT r;GetWindowRect(window,&r);MapWindowPoints(NULL,g_window,(POINT*)&r,2);return r;}
static void click(int id){SendMessageW(g_window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)GetDlgItem(g_window,id));}
static void paint_row(int row,UINT message){RECT r;ListView_GetItemRect(g_list,row,&r,LVIR_BOUNDS);SendMessageW(g_list,message,MK_LBUTTON,MAKELPARAM(D(50),(r.top+r.bottom)/2));}
static int search_layouts,button_text_changes;
static LRESULT CALLBACK CountMessages(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    if(window==g_search&&message==WM_WINDOWPOSCHANGED)search_layouts++;
    if(window==g_bookmark_button&&message==WM_SETTEXT)button_text_changes++;
    return DefSubclassProc(window,message,wp,lp);
}
static void geometry_and_no_flicker(void){
    int widths[]={510,640,740,900,1200},dpis[]={96,144,192};
    for(int lang=0;lang<2;lang++)for(int density=0;density<2;density++)for(int d=0;d<3;d++)for(int w=0;w<5;w++){
        if(g_bookmarks.editing)BookmarkCancel();g_bookmarks.showing=FALSE;
        g_russian=lang;g_density=density;g_dpi=dpis[d];UpdateFonts();UpdateLanguage();
        SetWindowPos(g_window,NULL,0,0,D(widths[w]),D(590),SWP_NOZORDER|SWP_NOACTIVATE);BookmarkRefresh();
        RECT normal=bounds(g_bookmark_button);BookmarkBegin();RECT edit=bounds(g_bookmark_button),caption=bounds(g_bookmark_caption);
        assert(EqualRect(&normal,&edit));
        search_layouts=button_text_changes=0;paint_row(0,WM_LBUTTONDOWN);paint_row(0,WM_LBUTTONUP);
        RECT dirty=bounds(g_bookmark_button),after_caption=bounds(g_bookmark_caption),query=bounds(g_search);
        assert(!search_layouts&&EqualRect(&normal,&dirty)&&EqualRect(&caption,&after_caption));
        if(GetWindowLongPtrW(g_bookmark_caption,GWL_STYLE)&WS_VISIBLE)assert(query.right<=caption.left-D(8)&&caption.right==edit.left&&query.right-query.left>=D(120));
        else {if(query.right>edit.left-D(4))fprintf(stderr,"overlap dpi=%d density=%d lang=%d width=%d input %ld..%ld icon %ld\n",dpis[d],density,lang,widths[w],query.left,query.right,edit.left);assert(query.right<=edit.left-D(4));}
        search_layouts=button_text_changes=0;bookmark_last_row=0;bookmark_drag_value=NativeBookmarksContains(&g_bookmarks,g_tasks[TaskIndexAtRow(0)].key);BookmarkPaintRange(0);
        assert(!search_layouts&&!button_text_changes);
    }
    BookmarkCancel();g_dpi=96;g_density=1;g_russian=TRUE;UpdateFonts();UpdateLanguage();SetWindowPos(g_window,NULL,0,0,900,590,SWP_NOZORDER|SWP_NOACTIVATE);
}
static void edit_action_shapes(void){
    BookmarkBegin();HDC screen=GetDC(g_window),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,D(24),D(24));HGDIOBJ old=SelectObject(memory,bitmap);
    DRAWITEMSTRUCT draw={.CtlID=IDC_BOOKMARKS,.hwndItem=g_bookmark_button,.hDC=memory,.rcItem={0,0,D(24),D(24)}};
    BookmarkDrawButton(&draw);GdiFlush();BOOL clean[24*24];for(int y=0;y<24;y++)for(int x=0;x<24;x++)clean[y*24+x]=GetPixel(memory,x,y)==MutedColor();assert(GetPixel(memory,1,1)==PanelColor());
    assert(NativeBookmarksSet(&g_bookmarks,L"PDES-6507",TRUE));BookmarkPresent();BookmarkDrawButton(&draw);GdiFlush();int difference=0;
    for(int y=0;y<24;y++)for(int x=0;x<24;x++)difference+=clean[y*24+x]!=(GetPixel(memory,x,y)==AccentColor());assert(difference>12);
    wchar_t footer[256];GetWindowTextW(g_updated,footer,256);assert(wcsstr(footer,L"\u2713"));
    assert(NativeBookmarksSet(&g_bookmarks,L"PDES-6507",FALSE));BookmarkDrawButton(&draw);GdiFlush();for(int y=0;y<24;y++)for(int x=0;x<24;x++)assert(clean[y*24+x]==(GetPixel(memory,x,y)==MutedColor()));
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(g_window,screen);BookmarkCancel();
}
static COLORREF gutter_pixel(int row){
    RECT client,cell;GetClientRect(g_list,&client);assert(ListView_GetItemRect(g_list,row,&cell,LVIR_BOUNDS));
    HDC screen=GetDC(g_list),memory=CreateCompatibleDC(screen);assert(memory);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,client.right,max(client.bottom,cell.bottom));assert(bitmap);
    HGDIOBJ old=SelectObject(memory,bitmap);NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=memory;
    draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;DrawListCustom(&draw);GdiFlush();
    COLORREF pixel=GetPixel(memory,D(13),(cell.top+cell.bottom-1)/2);
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(g_list,screen);return pixel;
}
typedef struct BookmarkRowPixels {COLORREF *pixels;int width,height;} BookmarkRowPixels;
static BookmarkRowPixels bookmark_row_pixels(int row){
    RECT client,cell;GetClientRect(g_list,&client);assert(ListView_GetItemRect(g_list,row,&cell,LVIR_BOUNDS));
    int width=client.right,height=max(client.bottom,cell.bottom);void *bits=NULL;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(g_list),memory=CreateCompatibleDC(screen);assert(memory);
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,NULL,0);assert(bitmap&&bits);
    HGDIOBJ old=SelectObject(memory,bitmap);NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=memory;
    draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;DrawListCustom(&draw);GdiFlush();
    BookmarkRowPixels result={.width=ListView_GetColumnWidth(g_list,0)+ListView_GetColumnWidth(g_list,1),.height=cell.bottom-cell.top};
    result.pixels=malloc((size_t)result.width*result.height*sizeof(COLORREF));assert(result.pixels);
    for(int y=0;y<result.height;y++)for(int x=0;x<result.width;x++){
        DWORD pixel=((DWORD*)bits)[(cell.top+y)*width+x];
        result.pixels[y*result.width+x]=RGB((pixel>>16)&255,(pixel>>8)&255,pixel&255);
    }
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(g_list,screen);return result;
}
static void assert_quiet_key_marker(BookmarkRowPixels before,BookmarkRowPixels after){
    assert(before.width==after.width&&before.height==after.height);
    HDC dc=GetDC(g_list);HGDIOBJ font=SelectObject(dc,g_font);SIZE key_size;
    GetTextExtentPoint32W(dc,L"PDES-6507",9,&key_size);SelectObject(dc,font);ReleaseDC(g_list,dc);
    int gutter=ListView_GetColumnWidth(g_list,0),key_end=gutter+D(7)+key_size.cx;
    RECT changed={after.width,after.height,0,0};int count=0;
    for(int y=0;y<after.height;y++)for(int x=0;x<after.width;x++){
        COLORREF old=before.pixels[y*before.width+x],now=after.pixels[y*after.width+x];
        if(old==now)continue;
        assert(x>=key_end+D(3)&&"Bookmarking must preserve the number and the full issue key");
        assert(now!=AccentColor()&&"The passive marker must not look like the blue bookmark-selection mode");
        changed.left=min(changed.left,x);changed.right=max(changed.right,x+1);changed.top=min(changed.top,y);changed.bottom=max(changed.bottom,y+1);count++;
    }
    assert(count>0&&"A saved bookmark must paint a small contour beside its issue key");
    assert(changed.right-changed.left<=D(10)&&changed.bottom-changed.top<=D(13));
    int cx=(changed.left+changed.right)/2,cy=(changed.top+changed.bottom)/2;
    assert(after.pixels[cy*after.width+cx]==before.pixels[cy*before.width+cx]&&"Bookmark contour must remain hollow");
    free(before.pixels);free(after.pixels);
}
static void saved_bookmark_row_markers(void){
    g_bookmarks.showing=FALSE;g_sort_column=-1;g_tab=1;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++)for(int russian=0;russian<2;russian++){
        g_dpi=dpi;g_density=density;g_dark=dark;g_russian=russian;RecreateBrushes();UpdateFonts();
        SetWindowPos(g_window,NULL,0,0,D(900),D(590),SWP_NOZORDER|SWP_NOACTIVATE);BookmarkRefresh();
        RECT before=g_search_frame;int width=ListView_GetColumnWidth(g_list,0);
        BookmarkRowPixels before_pixels=bookmark_row_pixels(0);
        BookmarkToggleTask(L"PDES-6507");
        assert(!g_bookmarks.editing&&!g_bookmarks.showing&&g_bookmarks.saved.count==1);
        assert_quiet_key_marker(before_pixels,bookmark_row_pixels(0));
        wchar_t accessible[64];ListView_GetItemText(g_list,0,0,accessible,64);
        assert(accessible[0]==L'1'&&wcsstr(accessible,g_russian?L"В закладках":L"Bookmarked"));
        assert(gutter_pixel(1)!=AccentColor());
        assert(width==ListView_GetColumnWidth(g_list,0)&&EqualRect(&before,&g_search_frame));
        /* Membership is real persisted state; restarting the model keeps the marker. */
        NativeBookmarksFree(&g_bookmarks);assert(NativeBookmarksLoad(&g_bookmarks,g_settings_path,g_bookmark_server,g_bookmark_account));BookmarkRefresh();
        assert(gutter_pixel(0)!=AccentColor());
        BookmarkActivate();assert(g_bookmarks.showing&&ListView_GetItemCount(g_list)==1&&gutter_pixel(0)!=AccentColor());
        BookmarkActivate();BookmarkBegin();assert(gutter_pixel(0)==AccentColor()&&gutter_pixel(1)!=AccentColor());BookmarkCancel();
        BookmarkToggleTask(L"PDES-6507");assert(g_bookmarks.saved.count==0&&gutter_pixel(0)!=AccentColor());
        g_bookmark_notice_until=0;KillTimer(g_window,785);
    }
    g_dark=FALSE;g_density=1;g_dpi=96;g_russian=TRUE;g_tab=0;RecreateBrushes();UpdateFonts();BookmarkRefresh();
}
static void drag_autoscroll(void){
    TaskItem *previous=g_tasks;int previous_count=g_task_count;
    TaskItem many[80];wchar_t keys[80][80];
    for(int i=0;i<80;i++){swprintf(keys[i],80,L"SCROLL-%d",i);many[i]=(TaskItem){.key=keys[i],.summary=L"Scroll task",.status=STATUS_TODO,.assigned_to_me=TRUE};}
    g_tasks=many;g_task_count=80;g_sort_column=-1;g_bookmarks.showing=FALSE;
    SetWindowPos(g_window,NULL,0,0,900,330,SWP_NOZORDER|SWP_NOACTIVATE);BookmarkBegin();assert(ListView_GetItemCount(g_list)==80);
    paint_row(0,WM_LBUTTONDOWN);RECT client;GetClientRect(g_list,&client);
    SendMessageW(g_list,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(D(50),client.bottom+3));
    for(int i=0;i<8;i++)SendMessageW(g_list,WM_TIMER,780,0);
    assert(ListView_GetTopIndex(g_list)>0&&NativeBookmarksContains(&g_bookmarks,L"SCROLL-0")&&NativeBookmarksCount(&g_bookmarks)>4);
    SendMessageW(g_list,WM_LBUTTONUP,0,0);assert(!bookmark_dragging);BookmarkCancel();
    g_tasks=previous;g_task_count=previous_count;BookmarkRefresh();SetWindowPos(g_window,NULL,0,0,900,590,SWP_NOZORDER|SWP_NOACTIVATE);
}
static void replace_account_while_menu_open(void){
    wcscpy_s(g_snapshot.identity.account_id,256,L"menu-switch-user");RefreshTaskList();BookmarkToggleTask(L"PDES-6166");
}
static void replace_account_back_while_menu_open(void){wcscpy_s(g_snapshot.identity.account_id,256,L"other-user");RefreshTaskList();}
static void preserve_search_during_bookmark_actions(void){
    wchar_t query[256];
    SetWindowTextW(g_search,L"ExpertMotive");assert(ListView_GetItemCount(g_list)==2);
    BookmarkBegin();GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"ExpertMotive")&&ListView_GetItemCount(g_list)==2);
    BookmarkToggleTask(L"PDES-6507");BookmarkToggleTask(L"PDES-6529");
    SetWindowTextW(g_search,L"Serial");BookmarkSave();GetWindowTextW(g_search,query,256);
    assert(!wcscmp(query,L"Serial")&&g_bookmarks.showing&&ListView_GetItemCount(g_list)==0);
    BookmarkActivate();GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial")&&ListView_GetItemCount(g_list)==1);
    BookmarkActivate();GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial")&&ListView_GetItemCount(g_list)==0);
    BookmarkBegin();BookmarkCancel();GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial"));
    BookmarkClear();GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial")&&ListView_GetItemCount(g_list)==1);
    click(IDC_TAB_ALL);GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial"));
    click(IDC_TAB_DONE);GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial"));
    click(IDC_TAB_CURRENT);GetWindowTextW(g_search,query,256);assert(!wcscmp(query,L"Serial")&&ListView_GetItemCount(g_list)==1);
    SetWindowTextW(g_search,L"");
}
static void bookmark_footer_counts(void){
    TaskItem *previous=g_tasks;int previous_count=g_task_count;
    TaskItem tasks[5];wchar_t keys[21][80];
    for(int i=0;i<21;i++)swprintf(keys[i],80,L"FOOTER-%d",i);
    for(int i=0;i<5;i++)tasks[i]=(TaskItem){.key=keys[i],.summary=i<3?L"Match":L"Other",.status=STATUS_TODO,.assigned_to_me=TRUE};
    g_tasks=tasks;g_task_count=5;
    const struct {int total,shown;BOOL russian;const wchar_t *query,*expected;} cases[]={
        {5,5,TRUE,L"",L"Закладки: 5"},
        {5,3,TRUE,L"Match",L"Показано 3 из 5 закладок"},
        {5,0,TRUE,L"Missing",L"Показано 0 из 5 закладок"},
        {1,1,TRUE,L"",L"Закладки: 1"},
        {1,0,TRUE,L"Missing",L"Показано 0 из 1 закладки"},
        {11,3,TRUE,L"Match",L"Показано 3 из 11 закладок"},
        {21,3,TRUE,L"Match",L"Показано 3 из 21 закладки"},
        {5,5,FALSE,L"",L"Bookmarks: 5"},
        {5,3,FALSE,L"Match",L"Showing 3 of 5 bookmarks"},
        {5,0,FALSE,L"Missing",L"Showing 0 of 5 bookmarks"},
        {1,1,FALSE,L"",L"Bookmarks: 1"},
        {1,0,FALSE,L"Missing",L"Showing 0 of 1 bookmark"}
    };
    for(int i=0;i<_countof(cases);i++){
        NativeBookmarksFree(&g_bookmarks);g_bookmark_notice_until=0;g_russian=cases[i].russian;
        assert(NativeBookmarksBegin(&g_bookmarks));
        for(int k=0;k<cases[i].total;k++)assert(NativeBookmarksSet(&g_bookmarks,keys[k],TRUE));
        SetWindowTextW(g_search,cases[i].query);BookmarkSave();
        assert(g_bookmarks.showing&&ListView_GetItemCount(g_list)==cases[i].shown);
        wchar_t footer[256];GetWindowTextW(g_updated,footer,256);
        if(wcscmp(footer,cases[i].expected))fwprintf(stderr,L"Footer case %d: expected [%ls], got [%ls]\n",i,cases[i].expected,footer);
        assert(!wcscmp(footer,cases[i].expected));
    }
    for(int russian=0;russian<2;russian++){
        g_russian=russian;BookmarkBegin();wchar_t footer[256];GetWindowTextW(g_updated,footer,256);
        assert(!wcscmp(footer,russian?L"Выбрано: 1 · Enter — завершить · Esc — отменить":L"Selected: 1 · Enter — finish · Esc — cancel"));
        BookmarkToggleTask(keys[1]);GetWindowTextW(g_updated,footer,256);
        assert(!wcscmp(footer,russian?L"Выбрано: 2 · \u2713 сохранить · Esc — отменить":L"Selected: 2 · \u2713 save · Esc — cancel"));
        BookmarkCancel();BookmarkShowCleared();BookmarkFooterOverride();GetWindowTextW(g_updated,footer,256);
        assert(!wcscmp(footer,russian?L"Закладки удалены · показаны все задачи":L"Bookmarks cleared · showing all tasks"));
        g_bookmark_notice_until=0;g_bookmarks.showing=FALSE;SetWindowTextW(g_updated,L"Normal footer");BookmarkFooterOverride();GetWindowTextW(g_updated,footer,256);assert(!wcscmp(footer,L"Normal footer"));
    }
    BookmarkClear();g_bookmark_notice_until=0;KillTimer(g_window,785);
    g_tasks=previous;g_task_count=previous_count;g_russian=TRUE;SetWindowTextW(g_search,L"");RefreshTaskList();
}
int main(void){
    g_show_all_tab=TRUE; /* This fixture tests bookmark navigation through All as well. */
    g_instance=GetModuleHandleW(NULL);g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    WNDCLASSW cls={.lpfnWndProc=FixtureProcedure,.hInstance=g_instance,.lpszClassName=L"BookmarkUiFixture"};assert(RegisterClassW(&cls));
    HWND window=CreateWindowW(cls.lpszClassName,L"Bookmark fixture",WS_OVERLAPPEDWINDOW,0,0,900,600,NULL,NULL,g_instance,NULL);assert(window);
    assert(GetDlgItem(window,1046)); /* A production bookmark control must exist, not just a demo overlay. */
    wchar_t temp[MAX_PATH],root[MAX_PATH];GetTempPathW(MAX_PATH,temp);assert(GetTempFileNameW(temp,L"bui",0,root));assert(DeleteFileW(root)&&CreateDirectoryW(root,NULL));
    swprintf(g_settings_path,MAX_PATH,L"%ls\\settings.ini",root);
    TaskItem tasks[]={
        {.key=L"PDES-6507",.summary=L"ExpertMotive",.status=STATUS_PROGRESS,.active=TRUE,.assigned_to_me=TRUE},
        {.key=L"PDES-6532",.summary=L"Serial",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"PDES-6529",.summary=L"ExpertMotive04",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"PDES-6166",.summary=L"Other",.status=STATUS_TODO,.assigned_to_me=TRUE}
    };
    g_tasks=tasks;g_task_count=4;wcscpy_s(g_snapshot.account,256,L"fixture-user");wcscpy_s(g_snapshot.identity.account_id,256,L"fixture-user");wcscpy_s(g_snapshot.server,800,L"jira.example.test:443/jira");g_status_count=STATUS_COUNT;g_status_categories[STATUS_PROGRESS]=1;
    RefreshTaskList();assert(ListView_GetItemCount(g_list)==4);
    saved_bookmark_row_markers();
    bookmark_footer_counts();
    preserve_search_during_bookmark_actions();
    RECT icon=bounds(GetDlgItem(window,1046));click(1046);
    assert(ListView_GetItemCount(g_list)==4);RECT now=bounds(GetDlgItem(window,1046));assert(EqualRect(&icon,&now));
    paint_row(0,WM_LBUTTONDOWN);paint_row(2,WM_MOUSEMOVE);paint_row(2,WM_MOUSEMOVE);paint_row(2,WM_LBUTTONUP);
    wchar_t text[256];GetWindowTextW(GetDlgItem(window,1047),text,256);assert(!wcscmp(text,L"Готово"));
    now=bounds(GetDlgItem(window,1046));assert(EqualRect(&icon,&now));
    SetWindowTextW(g_search,L"Other");assert(ListView_GetItemCount(g_list)==1);click(1046);
    assert(ListView_GetItemCount(g_list)==0&&g_bookmarks.saved.count==3); /* Hidden draft choices survive; the query still applies. */
    GetWindowTextW(g_search,text,256);assert(!wcscmp(text,L"Other"));
    click(1046);assert(ListView_GetItemCount(g_list)==1); /* Saved ↔ full preserves query and membership. */
    SetWindowTextW(g_search,L"Serial");assert(ListView_GetItemCount(g_list)==1);SetWindowTextW(g_search,L"");
    /* Removing rows by stable issue key must survive ordering changes. */
    ApplyTaskSort(2);click(1046);assert(ListView_GetItemCount(g_list)==3);click(1046);
    /* Real persisted model can be loaded independently; restart opens the full list. */
    NativeBookmarks saved={0};assert(NativeBookmarksLoad(&saved,g_settings_path,L"jira.example.test:443/jira",L"fixture-user"));
    assert(NativeBookmarksCount(&saved)==3&&!saved.showing&&NativeBookmarksContains(&saved,L"PDES-6532"));NativeBookmarksFree(&saved);
    wcscpy_s(g_snapshot.account,256,L"Renamed display name");RefreshTaskList();assert(NativeBookmarksCount(&g_bookmarks)==3);
    /* Credential URL changes must not move old displayed tasks to a new server's file. */
    fixture_url=L"https://other.example.test/jira/";RefreshTaskList();assert(NativeBookmarksCount(&g_bookmarks)==3);
    assert(!wcscmp(g_bookmark_server,L"jira.example.test:443/jira"));fixture_url=L"https://jira.example.test/jira/";
    /* A different account must never inherit this set. */
    wcscpy_s(g_snapshot.identity.account_id,256,L"other-user");RefreshTaskList();click(1046);
    assert(ListView_GetItemCount(g_list)==4);GetWindowTextW(GetDlgItem(window,1047),text,256);assert(!wcscmp(text,L"Выбор"));
    SetWindowSubclass(g_search,CountMessages,99,0);SetWindowSubclass(g_bookmark_button,CountMessages,99,0);
    geometry_and_no_flicker();
    edit_action_shapes();
    BookmarkBegin();BookmarkToggleTask(L"PDES-6507");
    MSG key={.hwnd=g_search,.message=WM_KEYDOWN,.wParam=VK_ESCAPE};assert(HandleBookmarkKeyMessage(&key)&&!g_bookmarks.editing&&!NativeBookmarksCount(&g_bookmarks));
    BookmarkBegin();BookmarkToggleTask(L"PDES-6507");key.wParam=VK_RETURN;assert(HandleBookmarkKeyMessage(&key)&&g_bookmarks.showing);
    /* Clear feedback appears at the action and expires without a blank reserved slot. */
    BookmarkClear();assert(ListView_GetItemCount(g_list)==4&&!g_bookmarks.showing);GetWindowTextW(g_bookmark_caption,text,256);assert(wcsstr(text,L"Закладки удалены"));
    assert(GetWindowLongPtrW(g_bookmark_caption,GWL_STYLE)&WS_VISIBLE);SendMessageW(g_window,WM_TIMER,785,0);
    assert(!(GetWindowLongPtrW(g_bookmark_caption,GWL_STYLE)&WS_VISIBLE));
    /* Editing cues never replace the column headings, including a language change. */
    BookmarkBegin();g_russian=FALSE;UpdateLanguage();wchar_t heading[80];LVCOLUMNW col={.mask=LVCF_TEXT,.pszText=heading,.cchTextMax=80};
    ListView_GetColumn(g_list,0,&col);assert(!heading[0]);ListView_GetColumn(g_list,2,&col);assert(wcsstr(heading,L"NAME"));BookmarkCancel();
    /* Outside edit, context bookmarking does not acknowledge unrelated activity. */
    tasks[1].is_new=TRUE;tasks[1].unread_comment=TRUE;BookmarkToggleTask(L"PDES-6532");assert(tasks[1].is_new&&tasks[1].unread_comment&&!g_bookmarks.showing);
    /* Popup/settings/note controls must keep their own Enter/Esc behavior. */
    BookmarkBegin();key.hwnd=g_note_edit;assert(!HandleBookmarkKeyMessage(&key));
    HWND other=CreateWindowW(L"EDIT",L"",WS_POPUP,0,0,20,20,NULL,NULL,g_instance,NULL);key.hwnd=other;assert(!HandleBookmarkKeyMessage(&key));DestroyWindow(other);BookmarkCancel();
    /* Context save failures must leave a retryable draft, not discard the requested bookmark. */
    wchar_t settings[MAX_PATH];wcscpy_s(settings,MAX_PATH,g_settings_path);swprintf(g_settings_path,MAX_PATH,L"%ls\\missing\\settings.ini",root);
    int errors=fixture_errors;BookmarkToggleTask(L"PDES-6166");assert(fixture_errors==errors+1&&g_bookmarks.editing&&NativeBookmarksContains(&g_bookmarks,L"PDES-6166"));
    assert(g_bookmarks.saved.count==1);wcscpy_s(g_settings_path,MAX_PATH,settings);BookmarkSave();assert(!g_bookmarks.editing&&g_bookmarks.saved.count==2);
    /* A transient file lock recovers on a later click, without restarting the app. */
    wchar_t locked_pattern[MAX_PATH];swprintf(locked_pattern,MAX_PATH,L"%ls\\bookmarks-*.dat",root);WIN32_FIND_DATAW locked_item;HANDLE scan=FindFirstFileW(locked_pattern,&locked_item),locks[8];int lock_count=0;assert(scan!=INVALID_HANDLE_VALUE);
    do{wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",root,locked_item.cFileName);assert(lock_count<8);locks[lock_count]=CreateFileW(path,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);assert(locks[lock_count++]!=INVALID_HANDLE_VALUE);}while(FindNextFileW(scan,&locked_item));FindClose(scan);
    g_bookmark_server[0]=0;RefreshTaskList();assert(!g_bookmark_ready&&g_bookmark_loading_failed);
    for(int i=0;i<lock_count;i++)CloseHandle(locks[i]);BookmarkActivate();assert(g_bookmark_ready&&g_bookmarks.showing&&g_bookmarks.saved.count==2);
    drag_autoscroll();
    fixture_menu_callback=replace_account_while_menu_open;fixture_menu_command=IDM_BOOKMARK_CLEAR;BookmarkMenu();
    assert(g_bookmarks.saved.count==1&&NativeBookmarksContains(&g_bookmarks,L"PDES-6166"));
    fixture_menu_callback=replace_account_back_while_menu_open;fixture_menu_command=IDM_TASK_BOOKMARK;
    RECT first;ListView_GetItemRect(g_list,0,&first,LVIR_BOUNDS);POINT point={first.left+D(50),(first.top+first.bottom)/2};ClientToScreen(g_list,&point);ShowTaskContextMenu(point);
    assert(g_bookmarks.saved.count==2&&!NativeBookmarksContains(&g_bookmarks,L"PDES-6507"));fixture_menu_callback=NULL;
    g_jira_busy=FALSE;wchar_t reason[256];assert(UpdateInstallAllowed(reason,256));BookmarkBegin();BookmarkToggleTask(L"PDES-6507");
    assert(!UpdateInstallAllowed(reason,256)&&reason[0]);BookmarkCancel();g_jira_busy=TRUE;
    NativeBookmarksFree(&g_bookmarks);
    g_tasks=NULL;g_task_count=0;
    wchar_t pattern[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\bookmarks-*.dat",root);WIN32_FIND_DATAW found;HANDLE find=FindFirstFileW(pattern,&found);
    if(find!=INVALID_HANDLE_VALUE){do{wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",root,found.cFileName);assert(DeleteFileW(path));}while(FindNextFileW(find,&found));FindClose(find);}assert(RemoveDirectoryW(root));
    DestroyWindow(window);puts("Production bookmark UI passed");return 0;
}
