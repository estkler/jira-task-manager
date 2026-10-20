#include <windows.h>
/* Exercise production creation/layout without displaying a test window on the desktop. */
static BOOL WINAPI HiddenFixtureShow(HWND window,int command){
    if(GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)return ShowWindow(window,command);
    return FALSE;
}
#define ShowWindow HiddenFixtureShow
static int WINAPI CaptureHeaderText(HDC,LPCWSTR,int,LPRECT,UINT);
#define DrawTextW CaptureHeaderText
static BOOL WINAPI CaptureHeaderBlit(HDC,int,int,int,int,HDC,int,int,DWORD);
#define BitBlt CaptureHeaderBlit
#define main existing_settings_tests
#include "settings_test.c"
#undef main
#undef DrawTextW
#undef BitBlt
static BOOL capture_header;
static const char *header_snapshot_prefix;
static BOOL header_snapshot_pending;
static wchar_t painted_header[1024];
static RECT painted_title_rect;
static UINT painted_title_format;
/* Optional screenshots of the actual production WM_PAINT surface, not a mockup.
   Copy to an unselected bitmap before encoding; no desktop capture or account access. */
static BOOL WINAPI CaptureHeaderBlit(HDC target,int x,int y,int width,int height,HDC source,int sx,int sy,DWORD rop)
{
    if(capture_header&&header_snapshot_pending&&header_snapshot_prefix){
        HDC copy=CreateCompatibleDC(source);assert(copy);
        HBITMAP bitmap=CreateCompatibleBitmap(source,width,height);assert(bitmap);
        HGDIOBJ previous=SelectObject(copy,bitmap);assert(BitBlt(copy,0,0,width,height,source,sx,sy,SRCCOPY));
        SelectObject(copy,previous);
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        DWORD size=(DWORD)width*height*4;BYTE *pixels=malloc(size);assert(pixels);
        assert(GetDIBits(copy,bitmap,0,height,pixels,&info,DIB_RGB_COLORS)==height);
        BITMAPFILEHEADER file={0};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+size;
        char path[MAX_PATH];snprintf(path,sizeof(path),"%s-%u-%s.bmp",header_snapshot_prefix,g_dpi,g_russian?"ru":"en");
        FILE *output=NULL;assert(!fopen_s(&output,path,"wb"));
        assert(fwrite(&file,sizeof(file),1,output)==1&&fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,output)==1);
        assert(fwrite(pixels,size,1,output)==1&&!fclose(output));
        free(pixels);DeleteObject(bitmap);DeleteDC(copy);header_snapshot_pending=FALSE;
    }
    return BitBlt(target,x,y,width,height,source,sx,sy,rop);
}
static int WINAPI CaptureHeaderText(HDC dc,LPCWSTR text,int count,LPRECT rect,UINT format)
{
    if(capture_header&&rect->top==D(3)&&(format&DT_SINGLELINE)&&!(format&DT_RIGHT)){
            wcsncpy_s(painted_header,_countof(painted_header),text,_TRUNCATE);
            painted_title_rect=*rect;painted_title_format=format;
    }
    return DrawTextW(dc,text,count,rect,format);
}
static BOOL CALLBACK FindOwnedTooltip(HWND window,LPARAM data)
{
    HWND *windows=(HWND*)data;wchar_t name[128];GetClassNameW(window,name,_countof(name));
    if(GetWindow(window,GW_OWNER)==windows[0]&&!wcscmp(name,TOOLTIPS_CLASSW))windows[1]=window;
    return TRUE;
}
static void assert_comment_header(HWND panel,const wchar_t *expected)
{
    painted_header[0]=0;capture_header=TRUE;
    InvalidateRect(panel,NULL,FALSE);SendMessageW(panel,WM_PAINT,0,0);capture_header=FALSE;
    /* Catch a generic title or text painting over the independently reserved issue key. */
    assert(!wcscmp(painted_header,expected));
    assert((painted_title_format&DT_END_ELLIPSIS)&&(painted_title_format&DT_NOPREFIX));
    HWND key=GetDlgItem(panel,IDC_POPOVER_KEY);assert(key);RECT key_rect;assert(GetWindowRect(key,&key_rect));MapWindowPoints(NULL,panel,(POINT*)&key_rect,2);
    assert(painted_title_rect.right<=key_rect.left);wchar_t key_label[80];GetWindowTextW(key,key_label,80);assert(!wcscmp(key_label,L"TEST-42"));
    wchar_t caption[1200];GetWindowTextW(panel,caption,_countof(caption));
    assert(wcsstr(caption,expected)&&wcsstr(caption,L"TEST-42"));
    HWND windows[2]={panel,NULL};EnumThreadWindows(GetCurrentThreadId(),FindOwnedTooltip,(LPARAM)windows);
    assert(windows[1]);
    assert((HFONT)SendMessageW(windows[1],WM_GETFONT,0,0)==g_small_font);
    wchar_t tooltip[1024]={0};TOOLINFOW tool={0};tool.cbSize=sizeof(tool)-sizeof(void*);
    tool.hwnd=panel;tool.uId=1;tool.lpszText=tooltip;
    SendMessageW(windows[1],TTM_GETTEXTW,_countof(tooltip),(LPARAM)&tool);
    assert(!wcscmp(tooltip,expected));
    tool.lpszText=NULL;assert(SendMessageW(windows[1],TTM_GETTOOLINFOW,0,(LPARAM)&tool));
    assert(EqualRect(&tool.rect,&painted_title_rect));
}
static void comment_header_regression(void)
{
    /* Exercise actual hidden panel creation, refresh, docking and tooltip callback.
       Never start a Jira worker or display a window on the user's desktop. */
    TaskItem task={0};wchar_t summary[1024]=L"A task & its comment history";
    task.key=L"TEST-42";task.summary=summary;task.status=STATUS_TODO;
    task.assigned_to_me=task.reported_by_me=TRUE;
    g_tasks=&task;g_task_count=1;g_tab=0;g_jira_busy=TRUE;
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++){
        g_dpi=dpi;g_russian=ru;UpdateFonts();RecreateBrushes();
        LayoutControls(MulDiv(740,dpi,96),MulDiv(550,dpi,96));RefreshTaskList();
        RECT anchor;assert(PopoverFallbackAnchorForRow(0,&anchor));OpenTaskPopover(0,1,anchor);
        assert(g_popover.window&&!IsWindowVisible(g_popover.window));
        assert_comment_header(g_popover.window,L"A task & its comment history");
        SetWindowTextW(g_popover.input,L"Unsent draft");
        TaskPopover *detached=DetachTaskPopover();assert(detached);
        assert_comment_header(detached->window,L"A task & its comment history");
        /* Replacing shared fonts must not leave this owned tooltip on a deleted HFONT. */
        UpdateFonts();assert_comment_header(detached->window,L"A task & its comment history");
        TaskPopover *previous=g_active_popover;g_active_popover=detached;
        wcscpy_s(summary,_countof(summary),L"Renamed task with a very long title that must be ellipsized without hiding its issue key or close control");
        RefreshTaskPopover();assert_comment_header(detached->window,summary);
        SetWindowPos(detached->window,NULL,0,0,D(280),D(280),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        header_snapshot_pending=header_snapshot_prefix!=NULL;
        assert_comment_header(detached->window,summary);
        g_tasks=NULL;g_task_count=0;wcscpy_s(summary,_countof(summary),L"Changed backing memory");
        RefreshTaskPopover();assert_comment_header(detached->window,
            L"Renamed task with a very long title that must be ellipsized without hiding its issue key or close control");
        g_tasks=&task;g_task_count=1;wcscpy_s(summary,_countof(summary),L"A task & its comment history");
        g_active_popover=previous;assert(AttachTaskPopover(detached,FALSE));RefreshTaskPopover();
        assert_comment_header(g_popover.window,summary);
        wchar_t draft[80];GetWindowTextW(g_popover.input,draft,_countof(draft));assert(!wcscmp(draft,L"Unsent draft"));
        HWND windows[2]={g_popover.window,NULL};EnumThreadWindows(GetCurrentThreadId(),FindOwnedTooltip,(LPARAM)windows);
        CloseTaskPopover();assert(!IsWindow(windows[1]));
        /* Missing/empty summary still identifies the issue, rather than a blank header. */
        summary[0]=0;OpenTaskPopover(0,1,anchor);assert_comment_header(g_popover.window,L"TEST-42");CloseTaskPopover();
        wcscpy_s(summary,_countof(summary),L"A task & its comment history");
        OpenTaskPopover(0,2,anchor);painted_header[0]=0;capture_header=TRUE;
        SendMessageW(g_popover.window,WM_PAINT,0,0);capture_header=FALSE;
        assert(!wcscmp(painted_header,ru?L"Заметка":L"Note"));CloseTaskPopover();
    }
    g_jira_busy=FALSE;g_tasks=NULL;g_task_count=0;
    puts("Comment headers use the task title, preserve the issue key/draft, refresh safely, and expose the full title in docked/detached tooltips");
}
static JiraJob *offline_header_refresh(const wchar_t *summary)
{
    JiraJob *job=calloc(1,sizeof(*job));assert(job);job->loaded=job->success=TRUE;
    if(summary){
        job->snapshot.items=calloc(1,sizeof(JiraItem));assert(job->snapshot.items);job->snapshot.count=1;
        JiraItem *item=job->snapshot.items;
        wcscpy_s(item->key,_countof(item->key),L"TEST-42");
        wcscpy_s(item->summary,_countof(item->summary),summary);
        wcscpy_s(item->status_id,_countof(item->status_id),L"1");
        wcscpy_s(item->status_name,_countof(item->status_name),L"To Do");
        wcscpy_s(item->category,_countof(item->category),L"new");
        item->assigned_to_me=item->reported_by_me=TRUE;
    }
    return job;
}
static void snapshot_comment_header_regression(void)
{
    /* Catch missing production refresh wiring: calling the title helper alone
       cannot prove that ordinary Jira results update an already detached panel. */
    assert(!g_settings_path[0]);g_tab=0;g_dpi=96;UpdateFonts();
    LayoutControls(740,550);FinishJiraJob(offline_header_refresh(L"Before refresh"));
    RECT anchor;assert(PopoverFallbackAnchorForRow(0,&anchor));g_jira_busy=TRUE;
    OpenTaskPopover(0,1,anchor);SetWindowTextW(g_popover.input,L"Draft survives polling");
    TaskPopover *detached=DetachTaskPopover();assert(detached);
    FinishJiraJob(offline_header_refresh(L"After ordinary Jira refresh"));
    assert_comment_header(detached->window,L"After ordinary Jira refresh");
    FinishJiraJob(offline_header_refresh(NULL));
    assert_comment_header(detached->window,L"After ordinary Jira refresh");
    wchar_t draft[80];GetWindowTextW(detached->input,draft,_countof(draft));assert(!wcscmp(draft,L"Draft survives polling"));
    g_active_popover=detached;CloseTaskPopover();g_active_popover=&g_attached_popover;
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);
    puts("Detached comment titles follow actual Jira result installation without losing drafts when a task disappears");
}

static void unread_comment_activation_regression(void)
{
    /* Synthetic messages go only to a hidden fixture: never move the real cursor.
       Persist read state in an isolated temporary file, not the installed profile. */
    assert(!g_settings_path[0]);
    wchar_t temporary[MAX_PATH];assert(GetTempPathW(_countof(temporary),temporary));
    assert(GetTempFileNameW(temporary,L"jci",0,g_settings_path));
    TaskItem task={0};task.key=L"TEST-COMMENT";task.summary=L"Comment activation fixture";
    task.latest_comment_id=L"100";task.latest_comment=L"Unread example";
    task.assigned_to_me=TRUE;task.unread_comment=TRUE;task.comment_pulse_started=1000;
    g_tasks=&task;g_task_count=1;g_reported_scope=FALSE;g_tab=0;g_dpi=96;
    wcscpy_s(g_snapshot.account,_countof(g_snapshot.account),L"offline-fixture");
    UpdateFonts();LayoutControls(740,550);RefreshTaskList();
    RECT icon;assert(CommentIndicatorRectForRow(0,&icon));
    LPARAM point=MAKELPARAM((icon.left+icon.right)/2,(icon.top+icon.bottom)/2);
    SendMessageW(g_list,WM_MOUSEMOVE,0,point);
    assert(g_hover_comment&&task.unread_comment&&task.comment_pulse_started==1000);
    g_jira_busy=TRUE;NMITEMACTIVATE click={0};click.hdr.hwndFrom=g_list;
    click.hdr.idFrom=IDC_TASKS;click.hdr.code=NM_CLICK;click.iItem=0;click.iSubItem=2;
    click.ptAction=(POINT){(icon.left+icon.right)/2,(icon.top+icon.bottom)/2};
    WindowProcedure(g_window,WM_NOTIFY,0,(LPARAM)&click);
    assert(g_popover.window&&!IsWindowVisible(g_popover.window)&&task.unread_comment);
    JiraJob *failed=calloc(1,sizeof(*failed));assert(failed);failed->action=4;
    wcscpy_s(failed->key,_countof(failed->key),task.key);FinishJiraJob(failed);
    assert(task.unread_comment); /* Failed loading must not discard unread activity. */
    JiraJob *loaded=calloc(1,sizeof(*loaded));assert(loaded);
    loaded->action=4;loaded->success=loaded->loaded=TRUE;
    wcscpy_s(loaded->key,_countof(loaded->key),task.key);
    loaded->comments.items=calloc(1,sizeof(JiraComment));assert(loaded->comments.items);
    loaded->comments.count=1;
    wcscpy_s(loaded->comments.items[0].id,_countof(loaded->comments.items[0].id),L"100");
    wcscpy_s(loaded->comments.items[0].body,_countof(loaded->comments.items[0].body),L"Unread example");
    FinishJiraJob(loaded);
    assert(!task.unread_comment&&!task.comment_pulse_started&&!ScopeActivity(FALSE));
    BOOL unread=TRUE;assert(NativeCommentObserve(g_settings_path,g_snapshot.account,task.key,L"100",FALSE,&unread)&&!unread);
    CloseTaskPopover();g_jira_busy=FALSE;g_tasks=NULL;g_task_count=0;
    assert(DeleteFileW(g_settings_path));g_settings_path[0]=0;g_snapshot.account[0]=0;
    puts("Hover preserves unread state; icon activation plus successful loading clears it, failed loading preserves it");
}

static void toolbar_regression(void)
{
    HWND incoming=GetDlgItem(g_window,1019);assert(incoming);
    HWND controls[]={incoming,g_tab_reported,g_tab_all,g_tab_current,g_tab_done,g_search};
    TaskItem task={0};g_tasks=&task;g_task_count=1;
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int width=478;width<=740;width+=131){
        g_dpi=dpi;g_density=density;UpdateFonts();
        assert((HFONT)SendMessageW(g_density_button,WM_GETFONT,0,0)==g_font);
        RECT baseline[6],search_frame;
        for(int variant=0;variant<16;variant++){
            g_russian=variant&1;g_reported_scope=variant&2;
            task.assigned_to_me=g_reported_scope!=0;task.reported_by_me=!g_reported_scope;
            task.is_new=variant&4;task.unread_comment=variant&8;
            SetWindowTextW(g_tab_all,g_russian?L"Все 38":L"All 10");
            SetWindowTextW(g_tab_current,g_russian?L"Текущие 9":L"Current 0");
            SetWindowTextW(g_tab_done,g_russian?L"Готово 29":L"Done 10");
            int client_width=MulDiv(width,dpi,96);
            LayoutControls(client_width,MulDiv(550,dpi,96));
            RECT in,out;GetWindowRect(incoming,&in);GetWindowRect(g_tab_reported,&out);
            MapWindowPoints(NULL,g_window,(POINT*)&in,2);MapWindowPoints(NULL,g_window,(POINT*)&out,2);
            assert(g_search_frame.left==D(10)&&in.top==D(10)&&in.bottom==g_search_frame.bottom);
            assert(in.left>g_search_frame.right&&in.right==out.left&&out.top==in.top&&out.bottom==in.bottom);
            assert(g_search_frame.top==D(10)); /* No notification-only row above the toolbar. */
            RECT last_tab;GetWindowRect(g_tab_done,&last_tab);MapWindowPoints(NULL,g_window,(POINT*)&last_tab,2);
            assert(last_tab.right<=client_width-D(10));
            if(!variant){for(int i=0;i<6;i++)GetWindowRect(controls[i],&baseline[i]);search_frame=g_search_frame;}
            else{
                for(int i=2;i<5;i++){RECT actual;GetWindowRect(controls[i],&actual);assert(EqualRect(&actual,&baseline[i]));}
                assert(g_search_frame.left==search_frame.left&&g_search_frame.top==search_frame.top&&g_search_frame.bottom==search_frame.bottom);
                assert(g_search_frame.right<=search_frame.right);
            }
        }
        /* Counts and translated result labels cannot move the text entry either. */
        SetWindowTextW(g_search,L"test");RECT query_bounds;
        g_russian=FALSE;LayoutControls(MulDiv(width,dpi,96),MulDiv(550,dpi,96));GetWindowRect(g_search,&query_bounds);
        g_russian=TRUE;LayoutControls(MulDiv(width,dpi,96),MulDiv(550,dpi,96));RECT actual;GetWindowRect(g_search,&actual);assert(EqualRect(&actual,&query_bounds));
        SetWindowTextW(g_search,L"");
    }
    g_tasks=NULL;g_task_count=0;
    puts("Toolbar stays one row; tabs keep their anchors and unused notification space returns to search across languages, scopes, widths and DPI");
}
static void scope_selection_regression(void)
{
    /* Exercise the real command handler, with no Jira worker or persisted data. */
    g_tab=1;g_reported_scope=FALSE;
    for(int ru=0;ru<2;ru++){
        g_russian=ru;UpdateLanguage();
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_REPORTED,BN_CLICKED),0);
        assert(g_reported_scope&&g_tab==1);
        wchar_t caption[160];GetWindowTextW(g_window,caption,_countof(caption));
        assert(wcsstr(caption,ru?L"Исходящие":L"Outgoing"));
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_DONE,BN_CLICKED),0);
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_REPORTED,BN_CLICKED),0);
        assert(g_reported_scope&&g_tab==2); /* Selected mode is not a toggle. */
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(1019,BN_CLICKED),0);
        assert(!g_reported_scope&&g_tab==2);
        GetWindowTextW(g_window,caption,_countof(caption));assert(wcsstr(caption,ru?L"Входящие":L"Incoming"));
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(1019,BN_CLICKED),0);assert(!g_reported_scope&&g_tab==2);
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_ALL,BN_CLICKED),0);
        SelectTaskScope(TRUE);assert(g_tab==0);SelectTaskScope(FALSE);assert(g_tab==0);
        /* Switching language updates the caption but not selection. */
        g_russian=!ru;UpdateLanguage();GetWindowTextW(g_window,caption,_countof(caption));
        assert(wcsstr(caption,ru?L"Incoming":L"Входящие"));
        assert(g_tab==0);
        WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_CURRENT,BN_CLICKED),0);
    }
    puts("Both mode commands select their own scope, retain tabs and update the localized caption");
}
/* Each permanently visible button describes its own scope, regardless of selection. */
static void scope_tooltip_regression(void)
{
    TaskItem tasks[2]={0};g_tasks=tasks;g_task_count=2;
    for(int russian=0;russian<2;russian++)for(int reported=0;reported<2;reported++)for(int activity=0;activity<4;activity++){
        g_russian=russian;g_reported_scope=reported;
        tasks[0].assigned_to_me=reported;tasks[0].reported_by_me=!reported;
        tasks[0].is_new=activity&1;tasks[0].unread_comment=activity&2;
        tasks[1].assigned_to_me=tasks[1].reported_by_me=TRUE;
        UpdateTooltips();
        for(int button_scope=0;button_scope<2;button_scope++){
            HWND button=GetDlgItem(g_window,button_scope?IDC_TAB_REPORTED:1019);assert(button);
            wchar_t actual[256]={0};TOOLINFOW tip={0};tip.cbSize=sizeof(tip)-sizeof(void*);tip.hwnd=g_window;tip.uId=(UINT_PTR)button;tip.lpszText=actual;
            SendMessageW(g_tooltips,TTM_GETTEXTW,_countof(actual),(LPARAM)&tip);
            const wchar_t *name=russian?(button_scope?L"Исходящие задачи":L"Входящие задачи"):(button_scope?L"Outgoing tasks":L"Incoming tasks");
            int signals=button_scope==!reported?activity:0;
            wchar_t expected[256];swprintf(expected,256,russian?
                L"%ls\nЗадач с обновлениями: %d\nНовых задач: %d\nЗадач с непрочитанными комментариями: %d":
                L"%ls\nTasks with updates: %d\nNew tasks: %d\nTasks with unread comments: %d",
                name,signals!=0,(signals&1)!=0,(signals&2)!=0);
            assert(!wcscmp(actual,expected));
        }
    }
    TaskItem many[100]={0};for(int i=0;i<100;i++){many[i].assigned_to_me=many[i].reported_by_me=TRUE;many[i].is_new=many[i].unread_comment=TRUE;}
    g_tasks=many;g_task_count=100;g_russian=FALSE;UpdateTooltips();
    for(int scope=0;scope<2;scope++){
        wchar_t actual[256]={0},expected[256];TOOLINFOW tip={0};tip.cbSize=sizeof(tip)-sizeof(void*);tip.hwnd=g_window;
        tip.uId=(UINT_PTR)GetDlgItem(g_window,scope?IDC_TAB_REPORTED:1019);tip.lpszText=actual;
        SendMessageW(g_tooltips,TTM_GETTEXTW,_countof(actual),(LPARAM)&tip);
        swprintf(expected,256,L"%ls\nTasks with updates: 100\nNew tasks: 100\nTasks with unread comments: 100",scope?L"Outgoing tasks":L"Incoming tasks");
        assert(!wcscmp(actual,expected));
    }
    g_tasks=NULL;g_task_count=0;
    puts("Both scope tooltips identify their own mode and exact uncapped event counts in RU/EN");
}
static void about_regression(BOOL check_alignment)
{
    for(int dpi=96;dpi<=192;dpi+=48){
        g_dpi=dpi;OpenSettingsWindow();assert(s_window);SettingsTab(1);
        for(int language=0;language<2;language++){
            g_russian=language;UpdateSettingsLanguage();
            RECT client;GetClientRect(s_window,&client);
            RECT first={0};int ids[]={242,234,238};
            for(int i=0;i<3;i++){
                RECT actual;GetWindowRect(GetDlgItem(s_window,ids[i]),&actual);MapWindowPoints(NULL,s_window,(POINT*)&actual,2);
                assert(actual.left>=S(26)&&actual.right<=client.right-S(26));
                assert(actual.bottom<=client.bottom-S(52)-S(8));
                if(!i)first=actual;else if(check_alignment)assert(actual.left==first.left&&actual.right==first.right);
            }
        }
        DestroyWindow(s_window);
    }
    puts("About actions are aligned and inside the real captioned window's panel");
}
int main(int argc,char **argv)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_instance=GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    if(argc<2||!strcmp(argv[1],"tooltip"))scope_tooltip_regression();
    if(argc<2||!strcmp(argv[1],"toolbar")){toolbar_regression();scope_selection_regression();}
    if(argc<2||!strcmp(argv[1],"about"))about_regression(TRUE);
    if(argc>2&&!strcmp(argv[1],"comment-header"))header_snapshot_prefix=argv[2];
    if(argc<2||!strcmp(argv[1],"comment-header"))comment_header_regression();
    if(argc<2||!strcmp(argv[1],"comment-header"))snapshot_comment_header_regression();
    if(argc<2||!strcmp(argv[1],"comment-activation"))unread_comment_activation_regression();
    if(argc>1&&!strcmp(argv[1],"about-bounds"))about_regression(FALSE);
    DestroyWindow(g_window);return 0;
}
