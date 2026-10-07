/* Isolated UI verification harness. Uses synthetic data and never connects to Jira. */
#define wWinMain ProductEntryPoint
#include "native.c"
#undef wWinMain

static BOOL preview_resize_check;
static BOOL preview_comment_style;
static int preview_frame,preview_unpainted,preview_first_bad=-1,preview_first_part=-1;
static RECT preview_resize_base;

static void StartPreviewResize(int kind)
{
    if(g_attached_popover.window)CloseTaskPopover();
    RECT anchor;
    if(kind==1)CommentIndicatorRectForRow(0,&anchor);else NoteIndicatorRectForRow(0,&anchor);
    OpenTaskPopover(0,kind,anchor);
    /* Settle the initial editor text paint before measuring resize events. */
    RedrawWindow(g_popover.window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    GetWindowRect(g_popover.window,&preview_resize_base);
    g_popover.resizing=TRUE;
}

static void PopulatePreviewComments(void)
{
    if(!g_attached_popover.window||g_attached_popover.kind!=1||!g_attached_popover.loading)return;
    TaskPopover *previous=g_active_popover;g_active_popover=&g_attached_popover;
    KillTimer(g_popover.window,2);g_popover.loading=FALSE;
    g_popover.comments.count=preview_comment_style?3:8;
    g_popover.comments.items=calloc(g_popover.comments.count,sizeof(JiraComment));
    for(int i=0;i<g_popover.comments.count&&g_popover.comments.items;i++){
        JiraComment *comment=&g_popover.comments.items[i];
        wcscpy_s(comment->author,_countof(comment->author),i%2?L"Roman Gulyaev":L"Maria Petrova");
        wcscpy_s(comment->identity.key,256,i%2?L"preview-self":L"preview-colleague");
        swprintf(comment->created,_countof(comment->created),L"2026-10-05T10:%02d:00.000+0300",i);
        wcscpy_s(comment->body,_countof(comment->body),preview_comment_style?
            (i==0?L"Посмотри, пожалуйста, обновлённый вариант. Добавила замечания к задаче.":
             i==1?L"Спасибо! Посмотрел замечания, сегодня внесу изменения и пришлю новый вариант.":
             L"Отлично, буду ждать. Если возникнут вопросы — напиши здесь."):
            L"Preview comment: checking text alignment, wrapping and scrolling while this window is dragged and resized.");
    }
    RefreshTaskPopover();EnableWindow(g_popover.send,FALSE);
    g_active_popover=previous;
}

static LRESULT CALLBACK PreviewProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_CREATE){g_window=window;CreateControls();return 0;}
    if(message==WM_TIMER&&wp==4){
        if(preview_frame==300){StartPreviewResize(2);}
        if(g_popover.window){
            int phase=preview_frame%100,delta=phase<50?phase:100-phase;
            g_popover.resize_initial=preview_resize_base;
            g_popover.resize_initial.right+=D(delta*2);
            g_popover.resize_initial.bottom+=D(delta);
            GetCursorPos(&g_popover.resize_start);
            SendMessageW(g_popover.window,WM_MOUSEMOVE,MK_LBUTTON,0);
            HWND parts[]={g_popover.window,g_popover.text,g_popover.input,g_popover.send};
            for(int i=0;i<4;i++)if(parts[i]&&GetUpdateRect(parts[i],NULL,FALSE)){
                preview_unpainted++;
                if(preview_first_bad<0){preview_first_bad=preview_frame;preview_first_part=i;}
            }
        }
        if(++preview_frame==600){
            KillTimer(window,4);
            if(g_popover.window){g_popover.resizing=FALSE;UpdateTaskPopoverRegion();}
            FILE *report=NULL;
            if(!_wfopen_s(&report,L"diagnostics\\resize-frame-check.txt",L"w")){
                fprintf(report,"Resize steps: %d\nUnpainted surfaces after a step: %d\n",preview_frame,preview_unpainted);
                fprintf(report,"First incomplete step: %d, surface: %d\n",preview_first_bad,preview_first_part);
                fclose(report);
            }
        }
        return 0;
    }
    if(message==WM_CLOSE){g_jira_busy=FALSE;g_allow_close=TRUE;}
    LRESULT result=WindowProcedure(window,message,wp,lp);
    if(message==WM_NOTIFY)PopulatePreviewComments();
    return result;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show)
{
    (void)previous;preview_resize_check=wcsstr(command_line,L"--resize-check")!=NULL;
    preview_comment_style=wcsstr(command_line,L"--comment-style")!=NULL;
#ifdef COMMENT_STYLE_PREVIEW
    preview_comment_style=TRUE;
#endif
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_instance=instance;g_dpi=GetDpiForSystem();g_russian=preview_comment_style;g_jira_busy=TRUE;
    g_dark=wcsstr(command_line,L"--dark")!=NULL;
#ifdef COMMENT_STYLE_DARK
    g_dark=TRUE;
#endif
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);
    GetTempFileNameW(temp,L"tmu",0,g_settings_path);
    wcscpy_s(g_snapshot.account,_countof(g_snapshot.account),L"UI verification");
    wcscpy_s(g_snapshot.identity.key,256,L"preview-self");
    RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSEXW cls={sizeof(cls)};cls.hInstance=instance;cls.lpfnWndProc=PreviewProcedure;
    cls.hIcon=LoadAppIcon(FALSE);cls.hIconSm=LoadAppIcon(TRUE);cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    cls.lpszClassName=L"Task Manager.UI.Preview";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Task Manager — UI check",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        100,100,MulDiv(610,g_dpi,96),MulDiv(460,g_dpi,96),NULL,NULL,instance,NULL);
#ifdef SCOPE_ICON_PREVIEW
    SetWindowTextW(window,L"Jira Task Manager — Scope icon preview (offline)");
#endif
    g_status_count=3;
    wcscpy_s(g_status_names[0],128,L"To Do");wcscpy_s(g_status_names[1],128,L"In Progress");wcscpy_s(g_status_names[2],128,L"Done");
    g_status_categories[0]=0;g_status_categories[1]=1;g_status_categories[2]=2;
    g_task_count=3;g_tasks=calloc(3,sizeof(TaskItem));
    g_tasks[0]=(TaskItem){.key=L"DEMO-1",.summary=L"Short new task",.status=0,.local_note=DuplicateWide(L"Preview local note\r\nThis is synthetic data.\r\nScroll and resize the note window."),.is_new=TRUE,.assigned_to_me=TRUE,.reported_by_me=TRUE,.latest_comment_id=L"100"};
    g_tasks[1]=(TaskItem){.key=L"DEMO-2",.summary=L"A very long task title that needs room for NEW, Jira comments and notes together",.status=1,.local_note=DuplicateWide(L"Another isolated preview note."),.is_new=TRUE,.assigned_to_me=TRUE,.latest_comment_id=L"101",.unread_comment=TRUE};
    g_tasks[2]=(TaskItem){.key=L"DEMO-3",.summary=L"Completed preview task",.status=2,.assigned_to_me=TRUE};
#ifdef SCOPE_ICON_PREVIEW
    g_tasks[2].assigned_to_me=FALSE;g_tasks[2].reported_by_me=TRUE;
    g_tasks[2].unread_comment=TRUE;g_tasks[2].latest_comment_id=L"102";
#endif
    g_workday_started=CurrentFileTime()-45ULL*60*10000000;g_tab=0;RefreshTaskList();UpdateTimer();
    ShowWindow(window,show);UpdateWindow(window);
    if(preview_resize_check||preview_comment_style){StartPreviewResize(1);PopulatePreviewComments();}
    if(preview_comment_style){g_popover.resizing=FALSE;UpdateTaskPopoverRegion();}
    if(preview_resize_check)SetTimer(window,4,50,NULL);
    MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    DeleteFileW(g_settings_path);return (int)message.wParam;
}
