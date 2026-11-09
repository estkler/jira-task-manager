#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <wchar.h>

/* Only the external clipboard boundary is replaced: production menu routing,
   URL construction, allocation, UTF-16 data and cleanup all run here. */
enum { FAIL_NONE, FAIL_ALLOC, FAIL_LOCK, FAIL_OPEN, FAIL_EMPTY, FAIL_SET };
static int failure,allocations,opens,closes,empties,writes,warnings;
static BOOL clipboard_open;
static HGLOBAL clipboard_data;
static wchar_t copied[4096];
static HGLOBAL WINAPI FixtureGlobalAlloc(UINT flags,SIZE_T bytes){
    assert(flags==GMEM_MOVEABLE);
    if(failure==FAIL_ALLOC)return NULL;
    HGLOBAL result=GlobalAlloc(flags,bytes);if(result)allocations++;return result;
}
static LPVOID WINAPI FixtureGlobalLock(HGLOBAL memory){return failure==FAIL_LOCK?NULL:GlobalLock(memory);}
static HGLOBAL WINAPI FixtureGlobalFree(HGLOBAL memory){
    HGLOBAL result=GlobalFree(memory);if(!result)allocations--;return result;
}
static BOOL WINAPI FixtureOpenClipboard(HWND owner){
    assert(owner);opens++;if(failure==FAIL_OPEN)return FALSE;clipboard_open=TRUE;return TRUE;
}
static BOOL WINAPI FixtureCloseClipboard(void){assert(clipboard_open);clipboard_open=FALSE;closes++;return TRUE;}
static BOOL WINAPI FixtureEmptyClipboard(void){
    assert(clipboard_open);empties++;return failure!=FAIL_EMPTY;
}
static HANDLE WINAPI FixtureSetClipboardData(UINT format,HANDLE memory){
    assert(clipboard_open&&format==CF_UNICODETEXT);writes++;
    if(failure==FAIL_SET)return NULL;
    const wchar_t *text=GlobalLock(memory);assert(text);
    assert(GlobalSize(memory)>=(wcslen(text)+1)*sizeof(wchar_t));
    wcscpy_s(copied,_countof(copied),text);GlobalUnlock(memory);
    clipboard_data=memory;return memory;
}
static int WINAPI FixtureMessageBox(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){
    assert(owner&&text&&text[0]&&title&&flags);warnings++;return IDOK;
}
static BOOL WINAPI FixtureForeground(HWND owner){(void)owner;return TRUE;}
static const wchar_t *fixture_url=L"https://jira.example.test/jira/";
static BOOL FixtureUrl(wchar_t *url,size_t capacity){
    if(!fixture_url){url[0]=0;return FALSE;}return wcscpy_s(url,capacity,fixture_url)==0;
}
static UINT menu_command;
static BOOL expect_link_enabled=TRUE;
static int menu_count;
static void (*during_menu)(void);
static BOOL WINAPI FixtureTrackPopupMenu(HMENU,UINT,int,int,int,HWND,const RECT*);
#define GlobalAlloc FixtureGlobalAlloc
#define GlobalLock FixtureGlobalLock
#define GlobalFree FixtureGlobalFree
#define OpenClipboard FixtureOpenClipboard
#define CloseClipboard FixtureCloseClipboard
#define EmptyClipboard FixtureEmptyClipboard
#define SetClipboardData FixtureSetClipboardData
#define MessageBoxW FixtureMessageBox
#define SetForegroundWindow FixtureForeground
#define JiraGetUrl FixtureUrl
#define TrackPopupMenu FixtureTrackPopupMenu
#include "native.c"

static BOOL WINAPI FixtureTrackPopupMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){
    (void)x;(void)y;(void)reserved;(void)rect;assert(owner==g_window&&(flags&TPM_RETURNCMD));menu_count++;
    const UINT commands[]={2353,2354};
    const wchar_t *labels[2][2]={{L"Copy task title",L"Copy task link"},{L"Копировать название",L"Копировать ссылку"}};
    for(int i=0;i<2;i++){
        MENUITEMINFOW info={.cbSize=sizeof(info),.fMask=MIIM_DATA|MIIM_STATE};
        assert(GetMenuItemInfoW(menu,commands[i],FALSE,&info)&&"Task menu must expose both copy actions");
        PopupItem *item=(PopupItem*)info.dwItemData;
        assert(item&&item->plain&&!wcscmp(item->text,labels[g_russian!=FALSE][i]));
        if(i==1)assert(item->disabled==!expect_link_enabled);
    }
    if(during_menu)during_menu();return menu_command;
}
static LRESULT CALLBACK FixtureProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CREATE){g_window=window;CreateControls();return 0;}
    if(message==WM_DESTROY)return 0;
    return WindowProcedure(window,message,wp,lp);
}
static void ResetClipboardFixture(void){
    if(clipboard_data){FixtureGlobalFree(clipboard_data);clipboard_data=NULL;}
    assert(allocations==0&&!clipboard_open);copied[0]=0;
    opens=closes=empties=writes=warnings=0;failure=FAIL_NONE;
}
static POINT RowPoint(int row){
    RECT rect;assert(ListView_GetItemRect(g_list,row,&rect,LVIR_BOUNDS));
    POINT point={rect.left+D(50),(rect.top+rect.bottom)/2};ClientToScreen(g_list,&point);return point;
}
static void CopyAt(UINT command,POINT point){menu_command=command;ShowTaskContextMenu(point);assert(g_context_task==-1);}
static TaskItem replacement={.key=L"OTHER-99",.summary=L"Replacement task",.status=STATUS_TODO,.assigned_to_me=TRUE};
static wchar_t *old_summary;
static void ReplaceTasksDuringMenu(void){
    free(old_summary);old_summary=NULL;g_tasks=&replacement;g_task_count=1;
    fixture_url=L"https://other.example.test/";RefreshTaskList();
}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&controls));
    WNDCLASSW cls={.lpfnWndProc=FixtureProcedure,.hInstance=g_instance,.lpszClassName=L"TaskCopyMenuFixture"};assert(RegisterClassW(&cls));
    HWND window=CreateWindowW(cls.lpszClassName,L"Task copy fixture",WS_OVERLAPPEDWINDOW,0,0,900,600,NULL,NULL,g_instance,NULL);assert(window);
    TaskItem tasks[]={
        {.key=L"TEST-1",.summary=L"Previously selected",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"TEST-2",.summary=L"Полное название задачи \U0001F680",.status=STATUS_TODO,.assigned_to_me=TRUE,.is_new=TRUE,.unread_comment=TRUE}
    };
    g_tasks=tasks;g_task_count=2;g_status_count=STATUS_COUNT;g_sort_column=-1;RefreshTaskList();
    assert(TaskIndexAtRow(0)==0&&TaskIndexAtRow(1)==1);
    ListView_SetItemState(g_list,0,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
    POINT clicked=RowPoint(1);
    /* Catches routing copy to the previously selected row, lossy title copying,
       or acknowledging task/comment activity as a side effect. */
    for(int language=0;language<2;language++){
        g_russian=language;ResetClipboardFixture();CopyAt(2353,clicked);
        assert(!wcscmp(copied,L"Полное название задачи \U0001F680"));
        assert(opens==1&&closes==1&&writes==1&&!warnings);
        assert(tasks[1].is_new&&tasks[1].unread_comment);
        ResetClipboardFixture();CopyAt(2354,clicked);
        assert(!wcscmp(copied,L"https://jira.example.test/jira/browse/TEST-2"));
        assert(tasks[1].is_new&&tasks[1].unread_comment);
    }
    ResetClipboardFixture();CopyAt(2353,(POINT){-1,-1});assert(!wcscmp(copied,L"Previously selected"));
    /* Catches display ellipsis or a fixed short clipboard buffer. */
    wchar_t long_title[1024];for(int i=0;i<1023;i++)long_title[i]=L'Ж';long_title[1023]=0;
    tasks[1].summary=long_title;ResetClipboardFixture();CopyAt(2353,clicked);assert(!wcscmp(copied,long_title));
    tasks[1].summary=L"Title";
    const wchar_t *bases[]={L"https://jira.example.test",L"https://jira.example.test/context///",L"https://jira.example.test:8443/jira/"};
    const wchar_t *urls[]={L"https://jira.example.test/browse/TEST-2",L"https://jira.example.test/context/browse/TEST-2",L"https://jira.example.test:8443/jira/browse/TEST-2"};
    for(int i=0;i<3;i++){fixture_url=bases[i];ResetClipboardFixture();CopyAt(2354,clicked);assert(!wcscmp(copied,urls[i]));}
    /* Failed preparation/open must not empty the existing user's clipboard;
       failed publication must release memory and close the clipboard. */
    fixture_url=L"https://jira.example.test/jira/";
    for(int fail=FAIL_ALLOC;fail<=FAIL_SET;fail++){
        ResetClipboardFixture();failure=fail;CopyAt(2353,clicked);
        assert(!copied[0]&&!clipboard_data&&allocations==0&&!clipboard_open&&warnings==1);
        assert(closes==(opens&&fail!=FAIL_OPEN));
        if(fail<=FAIL_OPEN)assert(!empties&&!writes);
        if(fail==FAIL_EMPTY)assert(!writes);
    }
    ResetClipboardFixture();fixture_url=NULL;expect_link_enabled=FALSE;CopyAt(0,clicked);assert(!opens);
    CopyAt(2353,clicked);assert(!wcscmp(copied,L"Title"));
    ResetClipboardFixture();CopyAt(2354,clicked);assert(!opens&&!copied[0]);
    fixture_url=L"https://jira.example.test/jira/";expect_link_enabled=TRUE;
    ResetClipboardFixture();CopyAt(0,clicked);assert(!opens&&!warnings);
    int menus=menu_count;POINT blank={-10000,-10000};CopyAt(2353,blank);assert(menu_count==menus&&!opens);
    /* Catches a use-after-free / changing the target URL during the nested
       popup loop: clipboard values belong to the task whose menu was opened. */
    for(int action=2353;action<=2354;action++){
        ResetClipboardFixture();old_summary=DuplicateWide(L"Original title");tasks[1].summary=old_summary;
        g_tasks=tasks;g_task_count=2;fixture_url=L"https://jira.example.test/jira/";RefreshTaskList();clicked=RowPoint(1);
        during_menu=ReplaceTasksDuringMenu;CopyAt(action,clicked);during_menu=NULL;
        assert(!wcscmp(copied,action==2353?L"Original title":L"https://jira.example.test/jira/browse/TEST-2"));
    }
    ResetClipboardFixture();g_tasks=NULL;g_task_count=0;DestroyWindow(window);
    puts("Task copy context menu passed (clipboard isolated)");return 0;
}
