#include <windows.h>
#include <assert.h>
#include <stdio.h>
static UINT command=2355;static BOOL dimmed_expected;static int warnings;
static void (*during_menu)(void);
static wchar_t capture_directory[MAX_PATH];
static BOOL check_title_font;
static int WINAPI TitleDrawFixture(HDC dc,LPCWSTR text,int count,LPRECT rect,UINT format){
    if(check_title_font&&text&&!wcscmp(text,L"Long-running task")){
        LOGFONTW font;assert(GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font)==sizeof(font));
        assert(font.lfItalic&&"Dimmed task title must render in italic");
    }
    return DrawTextW(dc,text,count,rect,format);
}
static BOOL count_key_measures;static int key_measures;
static BOOL WINAPI KeyMeasureFixture(HDC dc,LPCWSTR text,int count,LPSIZE size){
    if(count_key_measures&&count>2&&text[0]==L'Q'&&text[1]==L'-')key_measures++;
    return GetTextExtentPoint32W(dc,text,count,size);
}
static BOOL WINAPI MenuFixture(HMENU,UINT,int,int,int,HWND,const RECT*);
static int WINAPI ErrorFixture(HWND w,LPCWSTR t,LPCWSTR c,UINT f){warnings++;return IDOK;}
#define TrackPopupMenu MenuFixture
#define MessageBoxW ErrorFixture
#define DrawTextW TitleDrawFixture
#define GetTextExtentPoint32W KeyMeasureFixture
#include "native.c"
#undef MessageBoxW

static BOOL WINAPI MenuFixture(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){
    MENUITEMINFOW info={.cbSize=sizeof(info),.fMask=MIIM_DATA};
    assert(GetMenuItemInfoW(menu,2355,FALSE,&info)&&"Context menu must expose dimming");
    PopupItem *item=(PopupItem*)info.dwItemData;assert(item&&!item->disabled);
    const wchar_t *expected=dimmed_expected?(g_russian?L"Обычный вид":L"Normal appearance"):(g_russian?L"Приглушить":L"Dim");
    assert(!wcscmp(item->text,expected));if(during_menu)during_menu();return command;
}
static POINT PointFor(int row){RECT r;assert(ListView_GetItemRect(g_list,row,&r,LVIR_BOUNDS));POINT p={D(65),(r.top+r.bottom)/2};ClientToScreen(g_list,&p);return p;}
static void SwitchAccount(void){wcscpy_s(g_snapshot.identity.account_id,256,L"another-account");RefreshTaskList();}
static void PaintRows(void){
    check_title_font=TRUE;
    RECT client;GetClientRect(g_list,&client);HDC screen=GetDC(g_list),memory=CreateCompatibleDC(screen);
    HGDIOBJ measure_font=SelectObject(screen,g_font);SIZE key_size;
    assert(GetTextExtentPoint32W(screen,L"TEST-2",6,&key_size));SelectObject(screen,measure_font);
    /* Short keys still leave room for the column caption and sort arrow. */
    assert(ListView_GetColumnWidth(g_list,1)<=key_size.cx+D(34)&&"Task key column must not leave a wide empty gap");
    assert(ListView_GetColumnWidth(g_list,1)>=key_size.cx+D(26));
    HBITMAP bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom);HGDIOBJ old=SelectObject(memory,bitmap);
    for(int row=0;row<2;row++){
        NMLVCUSTOMDRAW draw={0};draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;draw.nmcd.hdc=memory;
        assert(DrawListCustom(&draw)==CDRF_SKIPDEFAULT);GdiFlush();
        RECT r;ListView_GetItemRect(g_list,row,&r,LVIR_BOUNDS);int colored=0,ink=0;
        for(int y=max(0,r.top);y<min(client.bottom,r.bottom-1);y++)for(int x=0;x<client.right;x++){
            COLORREF p=GetPixel(memory,x,y);assert(p!=CLR_INVALID);
            if(GetRValue(p)!=GetGValue(p)||GetGValue(p)!=GetBValue(p))colored++;
            if(p!=PanelColor())ink++;
        }
        assert(ink>20);if(row==1)assert(colored==0);else assert(colored>20);
    }
    if(capture_directory[0]&&g_dpi==96&&g_density==1){
        RECT first,last;ListView_GetItemRect(g_list,0,&first,LVIR_BOUNDS);ListView_GetItemRect(g_list,1,&last,LVIR_BOUNDS);
        int height=last.bottom-first.top;BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=client.right;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        DWORD *pixels=NULL;HBITMAP crop=CreateDIBSection(screen,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);assert(crop);
        HDC target=CreateCompatibleDC(screen);HGDIOBJ prior=SelectObject(target,crop);assert(BitBlt(target,0,0,client.right,height,memory,0,first.top,SRCCOPY));GdiFlush();
        DWORD bytes=client.right*height*4;BITMAPFILEHEADER header={.bfType=0x4d42,.bfSize=sizeof(header)+sizeof(info.bmiHeader)+bytes,.bfOffBits=sizeof(header)+sizeof(info.bmiHeader)};
        wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\native-task-dimming-%ls.bmp",capture_directory,g_dark?L"dark":L"light");
        HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(file!=INVALID_HANDLE_VALUE);DWORD written;
        assert(WriteFile(file,&header,sizeof(header),&written,NULL)&&WriteFile(file,&info.bmiHeader,sizeof(info.bmiHeader),&written,NULL)&&WriteFile(file,pixels,bytes,&written,NULL));CloseHandle(file);
        SelectObject(target,prior);DeleteDC(target);DeleteObject(crop);
    }
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(g_list,screen);
    check_title_font=FALSE;
}
static void TestDimmedOrder(void){
    TaskItem *original=g_tasks;int original_count=g_task_count;
    TaskItem tasks[]={
        {.key=L"TEST-1",.summary=L"Bravo",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"TEST-2",.summary=L"Alpha",.status=STATUS_HOLD,.assigned_to_me=TRUE},
        {.key=L"TEST-3",.summary=L"Charlie",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"TEST-4",.summary=L"Delta",.status=STATUS_HOLD,.assigned_to_me=TRUE}
    };
    g_tasks=tasks;g_task_count=4;g_sort_column=-1;RefreshTaskList();TaskDimmingToggle(L"TEST-4");
    assert(TaskIndexAtRow(0)==0&&TaskIndexAtRow(1)==2&&TaskIndexAtRow(2)==1&&TaskIndexAtRow(3)==3&&"Dimmed tasks must move below all normal tasks");
    const int columns[]={1,2,STATUS_COLUMN};
    for(int c=0;c<_countof(columns);c++)for(int ascending=0;ascending<2;ascending++){
        g_sort_column=columns[c];g_sort_ascending=ascending;SortTaskList();
        assert(!TaskDimmed(&tasks[TaskIndexAtRow(0)])&&!TaskDimmed(&tasks[TaskIndexAtRow(1)]));
        assert(TaskDimmed(&tasks[TaskIndexAtRow(2)])&&TaskDimmed(&tasks[TaskIndexAtRow(3)]));
        const int expected_ascending[]={0,2,1,3},expected_descending[]={2,0,3,1};
        for(int i=0;i<4;i++)assert(TaskIndexAtRow(i)==(ascending?expected_ascending[i]:expected_descending[i]));
    }
    g_sort_column=-1;SortTaskList();
    TaskDimmingToggle(L"TEST-2");assert(TaskIndexAtRow(0)==0&&TaskIndexAtRow(1)==1&&TaskIndexAtRow(2)==2&&TaskIndexAtRow(3)==3);
    TaskDimmingToggle(L"TEST-4");TaskDimmingToggle(L"TEST-2");
    g_tasks=original;g_task_count=original_count;RefreshTaskList();
}
static void TestKeyLayoutCache(void){
    TaskItem *original=g_tasks;int original_count=g_task_count;
    int count=5000;TaskItem *many=calloc(count,sizeof(*many));wchar_t (*keys)[16]=calloc(count,sizeof(*keys));assert(many&&keys);
    for(int i=0;i<count;i++){swprintf(keys[i],16,L"Q-%04d",i);many[i].key=keys[i];many[i].summary=L"Layout fixture";}
    g_tasks=many;g_task_count=count;count_key_measures=TRUE;key_measures=0;
    for(int i=0;i<8;i++)LayoutControls(D(900),D(600));
    assert(key_measures==count&&"Unchanged task keys must be measured once, not for every layout frame");
    count_key_measures=FALSE;g_tasks=original;g_task_count=original_count;LayoutControls(D(900),D(600));free(keys);free(many);
}
int main(int argc,char **argv){
    BOOL font_only=argc==2&&!strcmp(argv[1],"--font-only");
    if(argc==2&&!font_only)assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,capture_directory,MAX_PATH));
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_density=1;g_jira_busy=TRUE;g_tab=1;
    INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&cc));
    wchar_t temp[MAX_PATH],folder[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"dim",0,folder);DeleteFileW(folder);assert(CreateDirectoryW(folder,NULL));
    swprintf(g_settings_path,_countof(g_settings_path),L"%ls\\settings.ini",folder);
    wcscpy_s(g_snapshot.server,_countof(g_snapshot.server),L"https://jira.example.test");
    wcscpy_s(g_snapshot.identity.account_id,256,L"dimming-fixture");
    g_window=CreateWindowW(L"STATIC",L"Dimming fixture",WS_OVERLAPPEDWINDOW,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);
    RecreateBrushes();UpdateFonts();CreateControls();LayoutControls(900,600);
    TaskItem tasks[]={
        {.key=L"TEST-1",.summary=L"First task",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"TEST-2",.summary=L"Long-running task",.status=STATUS_HOLD,.assigned_to_me=TRUE}
    };
    g_tasks=tasks;g_task_count=2;g_status_count=STATUS_COUNT;g_sort_column=-1;RefreshTaskList();
    wcscpy_s(g_status_names[STATUS_TODO],128,L"To Do");wcscpy_s(g_status_names[STATUS_HOLD],128,L"On Hold");
    assert(TaskStatusDisplayText(&tasks[1])[0]);
    ListView_SetItemState(g_list,0,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
    ShowTaskContextMenu(PointFor(1));
    assert(TaskIndexAtRow(0)==0&&TaskIndexAtRow(1)==1);
    assert(TaskDimmed(&tasks[1])&&!TaskDimmed(&tasks[0])&&!tasks[1].active&&tasks[1].status==STATUS_HOLD);
    if(!font_only)TestDimmedOrder();
    TestKeyLayoutCache();
    NativeBookmarks reloaded={0};assert(NativeTaskMarksLoad(&reloaded,g_settings_path,g_snapshot.server,g_snapshot.identity.account_id,TASK_MARK_DIMMED));
    assert(NativeBookmarksCount(&reloaded)==1&&NativeBookmarksContains(&reloaded,L"TEST-2"));NativeBookmarksFree(&reloaded);
    assert(NativeBookmarksBegin(&g_bookmarks)&&NativeBookmarksSet(&g_bookmarks,L"TEST-1",TRUE)&&NativeBookmarksCommit(&g_bookmarks,g_settings_path,g_snapshot.server,g_snapshot.identity.account_id));
    g_bookmarks.showing=FALSE;assert(!NativeBookmarksContains(&g_bookmarks,L"TEST-2")&&TaskDimmed(&tasks[1]));
    dimmed_expected=TRUE;command=0;g_russian=TRUE;ShowTaskContextMenu(PointFor(1));g_russian=FALSE;ShowTaskContextMenu(PointFor(1));
    SetWindowTextW(g_search,L"Long");RefreshTaskList();assert(ListView_GetItemCount(g_list)==1&&TaskIndexAtRow(0)==1&&TaskDimmed(&tasks[1]));
    SetWindowTextW(g_search,L"");RefreshTaskList();
    wchar_t pattern[MAX_PATH],file[MAX_PATH];WIN32_FIND_DATAW found;swprintf(pattern,MAX_PATH,L"%ls\\dimmed-*.dat",folder);
    HANDLE search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);FindClose(search);swprintf(file,MAX_PATH,L"%ls\\%ls",folder,found.cFileName);
    HANDLE locked=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(locked!=INVALID_HANDLE_VALUE);
    command=2355;ShowTaskContextMenu(PointFor(1));assert(warnings==1&&TaskDimmed(&tasks[1]));CloseHandle(locked);
    command=0;during_menu=SwitchAccount;ShowTaskContextMenu(PointFor(1));assert(!TaskDimmed(&tasks[1]));during_menu=NULL;
    wcscpy_s(g_snapshot.identity.account_id,256,L"dimming-fixture");RefreshTaskList();assert(TaskDimmed(&tasks[1]));
    /* A nested identity change must not toggle the new account after menu exit. */
    command=2355;during_menu=SwitchAccount;ShowTaskContextMenu(PointFor(1));during_menu=NULL;assert(!TaskDimmed(&tasks[1]));
    wcscpy_s(g_snapshot.identity.account_id,256,L"dimming-fixture");RefreshTaskList();assert(TaskDimmed(&tasks[1]));
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_dark=dark;g_density=density;RecreateBrushes();UpdateFonts();LayoutControls(D(900),D(600));RefreshTaskList();PaintRows();
    }
    dimmed_expected=TRUE;command=2355;ShowTaskContextMenu(PointFor(1));assert(!TaskDimmed(&tasks[1]));
    dimmed_expected=FALSE;ShowTaskContextMenu(PointFor(1));assert(TaskDimmed(&tasks[1]));
    tasks[1].active=TRUE;tasks[1].status=STATUS_PROGRESS;RefreshTaskList();assert(!TaskDimmed(&tasks[1]));
    tasks[1].active=FALSE;tasks[1].status=STATUS_HOLD;RefreshTaskList();assert(!TaskDimmed(&tasks[1]));
    assert(NativeTaskMarksLoad(&reloaded,g_settings_path,g_snapshot.server,g_snapshot.identity.account_id,TASK_MARK_DIMMED)&&!NativeBookmarksContains(&reloaded,L"TEST-2"));NativeBookmarksFree(&reloaded);
    dimmed_expected=FALSE;ShowTaskContextMenu(PointFor(1));assert(TaskDimmed(&tasks[1]));
    JiraItem item={.key=L"TEST-2",.status_id=L"active",.status_name=L"In Progress",.category=L"indeterminate",.assigned_to_me=TRUE};JiraSnapshot patch={0};
    patch=g_snapshot;patch.items=&item;patch.count=1;assert(InstallIssueStatuses(&patch,L"TEST-2"));assert(!TaskDimmed(&g_tasks[TaskByKey(L"TEST-2")]));
    assert(NativeBookmarksContains(&g_bookmarks,L"TEST-1"));NativeBookmarksFree(&g_dimmed);NativeBookmarksFree(&g_bookmarks);
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);
    swprintf(pattern,MAX_PATH,L"%ls\\*",folder);search=FindFirstFileW(pattern,&found);
    if(search!=INVALID_HANDLE_VALUE){do{if(!(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){swprintf(file,MAX_PATH,L"%ls\\%ls",folder,found.cFileName);assert(DeleteFileW(file));}}while(FindNextFileW(search,&found));FindClose(search);}assert(RemoveDirectoryW(folder));
    printf("Task dimming passed\n");return 0;
}
