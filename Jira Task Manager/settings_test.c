#include <windows.h>
#include <shellapi.h>
static wchar_t opened_file[32768],opened_parameters[32768];
static HINSTANCE WINAPI CaptureShellOpen(HWND w,LPCWSTR op,LPCWSTR file,LPCWSTR params,LPCWSTR dir,INT show){
    (void)w;(void)op;(void)dir;(void)show;
    wcscpy_s(opened_file,32768,file?file:L"");wcscpy_s(opened_parameters,32768,params?params:L"");return (HINSTANCE)33;
}
BOOL CaptureJiraGetUrl(wchar_t *url,size_t capacity){return wcscpy_s(url,capacity,L"https://jira.example.test/jira/")==0;}
#define ShellExecuteW CaptureShellOpen
#define JiraGetUrl CaptureJiraGetUrl
static BOOL WINAPI CaptureTaskMenu(HMENU,UINT,int,int,int,HWND,const RECT*);
#define TrackPopupMenu CaptureTaskMenu
#include "native.c"
#include <assert.h>
static wchar_t captured_comment_action[256];
static BOOL WINAPI CaptureTaskMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){
    (void)flags;(void)x;(void)y;(void)reserved;(void)owner;(void)rect;
    MENUITEMINFOW info={0};info.cbSize=sizeof(info);info.fMask=MIIM_DATA;
    assert(GetMenuItemInfoW(menu,IDM_TASK_COMMENTS,FALSE,&info));
    wcscpy_s(captured_comment_action,256,((PopupItem*)info.dwItemData)->text);
    return 0;
}
static void AssertStatusFilterCluster(void){
    HWND header=ListView_GetHeader(g_list);RECT cell,button;
    assert(Header_GetItemRect(header,STATUS_COLUMN,&cell));GetWindowRect(g_filter,&button);
    MapWindowPoints(NULL,header,(POINT*)&button,2);
    wchar_t label[80];HDITEMW item={0};item.mask=HDI_TEXT;item.pszText=label;item.cchTextMax=80;
    assert(Header_GetItem(header,STATUS_COLUMN,&item));
    HDC dc=GetDC(header);HGDIOBJ old=SelectObject(dc,g_small_font);SIZE text;
    GetTextExtentPoint32W(dc,label,(int)wcslen(label),&text);SelectObject(dc,old);ReleaseDC(header,dc);
    int group_width=text.cx+D(5)+button.right-button.left;
    int expected_left=cell.left+(cell.right-cell.left-group_width)/2+text.cx+D(5);
    assert(abs(button.left-expected_left)<=1);
    assert(button.left>=cell.left&&button.right<=cell.right);
}
static void CALLBACK CloseModalRegression(HWND window,UINT message,UINT_PTR timer,DWORD tick) {
    (void)message;(void)tick;KillTimer(window,timer);
    if(s_window)SendMessageW(s_window,WM_CLOSE,0,0);
}
static int resize_nc_paints;
static LRESULT CALLBACK TraceResizeFramePaint(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    (void)id;(void)data;if(message==WM_NCPAINT)resize_nc_paints++;
    return DefSubclassProc(window,message,wp,lp);
}
static COLORREF TransitionStarCenter(TransitionInputDialog *dialog,int score){
    HWND button=GetDlgItem(dialog->window,IDC_TRANSITION_RATING+score);RECT bounds;GetClientRect(button,&bounds);
    HDC target=GetDC(button),dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,bounds.right,bounds.bottom);
    HGDIOBJ old=SelectObject(dc,bitmap);DRAWITEMSTRUCT draw={0};draw.CtlID=IDC_TRANSITION_RATING+score;
    draw.hwndItem=button;draw.hDC=dc;draw.rcItem=bounds;DrawTransitionButton(dialog,&draw);
    COLORREF pixel=GetPixel(dc,bounds.right/2,bounds.bottom/2);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(button,target);return pixel;
}
int main(void) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    OpenPortal();
    assert(!wcscmp(opened_file,L"https://portal.kama.gs/company/building_access.php"));
    wchar_t issue_url[1024];assert(BuildTaskUrl(L"https://jira.example.test/jira/",L"PDES-6271",issue_url,_countof(issue_url)));
    assert(!wcscmp(issue_url,L"https://jira.example.test/jira/browse/PDES-6271"));
    assert(OpenTaskInJira(L"PDES-6271"));assert(!wcscmp(opened_file,issue_url));
    JiraConnection c={0};
    assert(JiraParseUrl(&c,L"https://jira.example.test/jira/"));
    assert(!wcscmp(c.host,L"jira.example.test")&&!wcscmp(c.prefix,L"/jira"));
    assert(!JiraParseUrl(&c,L"http://jira.example.test"));
    assert(!JiraParseUrl(&c,L"https://user:pass@jira.example.test"));
    assert(!JiraParseUrl(&c,L"https://jira.example.test/?token=secret"));
    assert(JiraParseInterval(L"15")==15);
    assert(JiraParseInterval(L"0")==0&&JiraParseInterval(L"1441")==0&&JiraParseInterval(L"15x")==0);
    g_instance=GetModuleHandleW(NULL);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);
    assert(g_window);CreateControls();g_dpi=144;
    assert(!(GetWindowLongPtrW(g_list,GWL_STYLE)&LVS_NOSORTHEADER));
    TOOLINFOW list_tip={0};list_tip.cbSize=sizeof(list_tip)-sizeof(void*);list_tip.hwnd=g_window;list_tip.uId=(UINT_PTR)g_list;
    assert(SendMessageW(g_tooltips,TTM_GETTOOLINFOW,0,(LPARAM)&list_tip));
    for(int mode=0;mode<2;mode++) {
        g_density=mode;UpdateFonts();
        assert((HFONT)SendMessageW(g_updated,WM_GETFONT,0,0)==g_small_font);
        assert((GetWindowLongPtrW(g_updated,GWL_STYLE)&SS_TYPEMASK)==SS_OWNERDRAW);
        g_russian=TRUE;g_text_time=TRUE;g_workday_started=CurrentFileTime()-3600ULL*10000000ULL;UpdateTimer();
        wchar_t timer_text[96];GetWindowTextW(g_timer,timer_text,96);
        assert(wcsstr(timer_text,L"1ч 0м ")!=NULL);
        g_jira_stale=FALSE;g_updated_time.wHour=16;g_updated_time.wMinute=35;g_updated_time.wSecond=46;
        UpdateLastUpdated();LayoutControls(510,550);
        RECT search_position,reporter_position,filter_position,all_tab_position;
        GetWindowRect(g_search,&search_position);GetWindowRect(g_tab_reported,&reporter_position);
        GetWindowRect(g_filter,&filter_position);GetWindowRect(g_tab_all,&all_tab_position);
        RECT header_position;GetWindowRect(ListView_GetHeader(g_list),&header_position);
        assert(GetParent(g_filter)==ListView_GetHeader(g_list));
        assert(search_position.right<reporter_position.left&&reporter_position.right<all_tab_position.left);
        assert(filter_position.left>=header_position.left&&filter_position.right<=header_position.right);
        assert(filter_position.top>=header_position.top&&filter_position.bottom<=header_position.bottom);
        for(int language=0;language<2;language++){
            g_russian=language;g_sort_column=1;UpdateColumnLabels();AssertStatusFilterCluster();
            ApplyTaskSort(STATUS_COLUMN);AssertStatusFilterCluster();
            ApplyTaskSort(STATUS_COLUMN);AssertStatusFilterCluster();
        }
        g_russian=TRUE;g_sort_column=-1;g_sort_ascending=TRUE;UpdateColumnLabels();
        int key_width=ListView_GetColumnWidth(g_list,1),status_width=ListView_GetColumnWidth(g_list,STATUS_COLUMN);
        ListView_SetColumnWidth(g_list,1,key_width+D(15));AssertStatusFilterCluster();
        ListView_SetColumnWidth(g_list,STATUS_COLUMN,status_width+D(20));AssertStatusFilterCluster();
        ListView_SetColumnWidth(g_list,1,key_width);ListView_SetColumnWidth(g_list,STATUS_COLUMN,status_width);
        wchar_t text[128];GetWindowTextW(g_updated,text,128);
        HDC dc=GetDC(g_window);SelectObject(dc,g_small_font);SIZE size;
        GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size);ReleaseDC(g_window,dc);
        RECT rect;GetClientRect(g_updated,&rect);
        assert(rect.right>=size.cx);
        assert(wcsstr(text,L"16:35:46")!=NULL);
        LayoutControls(MulDiv(510,(int)g_dpi,96),550);
        assert(g_search_frame.right-g_search_frame.left>=D(150));
    }
    LayoutControls(510,550);
    int name_without_scroll=ListView_GetColumnWidth(g_list,2);
    for(int i=0;i<60;i++){
        LVITEMW row={0};row.mask=LVIF_TEXT;row.iItem=i;row.pszText=L"Task";
        assert(ListView_InsertItem(g_list,&row)>=0);
    }
    LayoutControls(510,550);
    int name_with_scroll=ListView_GetColumnWidth(g_list,2);
    assert(name_with_scroll==name_without_scroll);
    ListView_DeleteAllItems(g_list);
    assert(!(GetWindowLongPtrW(g_comments,GWL_STYLE)&WS_VISIBLE));
    RECT refresh_position,settings_position,language_position,theme_position;
    GetWindowRect(g_refresh,&refresh_position);GetWindowRect(g_settings,&settings_position);
    GetWindowRect(g_language,&language_position);GetWindowRect(g_theme,&theme_position);
    assert(refresh_position.left<settings_position.left);
    assert(language_position.right<theme_position.left);
    RECT timer_anchor_rect={123,40,210,68},visible_frame={104,20,700,500};
    POINT timer_anchor=TimerMenuAnchor(timer_anchor_rect,visible_frame);
    assert(timer_anchor.x==104&&timer_anchor.y==68);
    SIZE settings_size={S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP)};
    RECT work_area={0,0,1920,1080},owner_right={100,100,700,650};
    POINT settings_origin=SettingsWindowOrigin(owner_right,work_area,settings_size);
    assert(settings_origin.x==owner_right.right+S(2)&&settings_origin.y==owner_right.top);
    RECT owner_left={1200,100,1800,650};settings_origin=SettingsWindowOrigin(owner_left,work_area,settings_size);
    assert(settings_origin.x==owner_left.left-S(2)-settings_size.cx&&settings_origin.y==owner_left.top);
    RECT narrow_work={0,0,1200,1080},owner_below={250,100,950,400};
    settings_origin=SettingsWindowOrigin(owner_below,narrow_work,settings_size);
    assert(settings_origin.x==owner_below.left&&settings_origin.y==owner_below.bottom+S(2));
    RECT settings_outer={800,80,1450,650},settings_frame={810,90,1440,640};
    POINT desired_frame={703,100};
    POINT aligned_outer=AlignedSettingsOuterOrigin(settings_outer,settings_frame,desired_frame);
    assert(aligned_outer.x==693&&aligned_outer.y==90);
    HWND original_owner=g_window;
    HWND frame_owner=CreateWindowExW(0,L"STATIC",L"Frame owner",WS_OVERLAPPEDWINDOW,100,100,740,550,NULL,NULL,g_instance,NULL);
    HWND frame_settings=CreateWindowExW(WS_EX_DLGMODALFRAME,L"STATIC",L"Frame settings",WS_CAPTION|WS_SYSMENU,
        900,100,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),frame_owner,NULL,g_instance,NULL);
    assert(frame_owner&&frame_settings);
    ShowWindow(frame_owner,SW_SHOWNOACTIVATE);DwmFlush();
    g_window=frame_owner;PositionSettingsWindow(frame_settings);ShowWindow(frame_settings,SW_SHOWNOACTIVATE);DwmFlush();
    RECT owner_frame,positioned_frame;assert(VisibleWindowFrame(frame_owner,&owner_frame));assert(VisibleWindowFrame(frame_settings,&positioned_frame));
    assert(positioned_frame.left-owner_frame.right==S(2));assert(positioned_frame.top==owner_frame.top);
    DestroyWindow(frame_settings);DestroyWindow(frame_owner);g_window=original_owner;
    WNDCLASSW wc={0};wc.lpfnWndProc=SettingsProcedure;wc.hInstance=g_instance;wc.lpszClassName=L"Native.Settings.Test";RegisterClassW(&wc);
    HWND settings=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,700,700,g_window,NULL,g_instance,NULL);
    assert(settings&&s_url&&s_interval);
    wchar_t temp[MAX_PATH],root[MAX_PATH],diagnostics[MAX_PATH];GetTempPathW(MAX_PATH,temp);
    GetTempFileNameW(temp,L"ftd",0,root);DeleteFileW(root);
    swprintf(g_settings_path,MAX_PATH,L"%ls\\nested\\settings.ini",root);
    GetModuleFileNameW(NULL,diagnostics,MAX_PATH);PathRemoveFileSpecW(diagnostics);PathRemoveFileSpecW(diagnostics);PathAppendW(diagnostics,L"diagnostics");
    SendMessageW(settings,WM_COMMAND,234,0);
    assert(GetFileAttributesW(diagnostics)!=INVALID_FILE_ATTRIBUTES);
    assert(wcsstr(opened_file,L"explorer.exe")!=NULL);
    assert(opened_parameters[0]==L'"'&&wcsstr(opened_parameters,diagnostics)!=NULL);
    RemoveDirectoryW(root);
    g_settings_path[0]=0;
    /* Settings typography must not grow/shrink with the task-list density. */
    LOGFONTW settings_font={0};GetObjectW((HFONT)SendMessageW(s_url,WM_GETFONT,0,0),sizeof(settings_font),&settings_font);
    assert(settings_font.lfHeight==-18); /* 12 DIP at 144 DPI, Legacy's size */
    assert(GetDlgItem(settings,213)); /* tray count: a real editable preference */
    assert(GetDlgItem(settings,214)); /* startup: a real checkbox */
    TaskItem fixture[]={{L"T-2",L"Zulu active task with a very long name",0,TRUE,NULL,L"Важная локальная заметка"},{L"T-10",L"Alpha",1,FALSE},{L"T-1",L"Beta",0,FALSE}};
    assert(IndicatorFillColor(FALSE)!=IndicatorFillColor(TRUE));
    assert(IndicatorHoverFillColor()!=IndicatorFillColor(FALSE));
    assert(IndicatorHoveredActiveColor()!=IndicatorFillColor(FALSE));
    assert(PopoverHeaderColor()!=PopoverColor());
    TaskItem old_comment={0},new_comment={0};
    old_comment.latest_comment_id=L"100";new_comment.latest_comment_id=L"101";
    new_comment.unread_comment=TRUE;
    assert(ShouldPulseComment(&old_comment,&new_comment));
    new_comment.unread_comment=FALSE;assert(!ShouldPulseComment(&old_comment,&new_comment));
    new_comment.unread_comment=TRUE;new_comment.latest_comment_id=L"100";
    assert(!ShouldPulseComment(&old_comment,&new_comment));
    old_comment.comment_pulse_started=1000;
    assert(CommentPulseActive(&old_comment,1000)&&CommentPulseActive(&old_comment,1649));
    assert(!CommentPulseActive(&old_comment,1650));
    fixture[0].local_note=DuplicateWide(fixture[0].local_note);assert(fixture[0].local_note);
    for(int i=0;i<3;i++)fixture[i].assigned_to_me=TRUE;
    fixture[1].reported_by_me=TRUE;
    TaskItem reporter_only={0};reporter_only.reported_by_me=TRUE;
    assert(!TaskInTab(&reporter_only,0)&&!TaskInTab(&reporter_only,1));
    g_reported_scope=TRUE;
    assert(TaskInTab(&reporter_only,0)&&TaskInTab(&reporter_only,1));
    g_reported_scope=FALSE;
    g_tasks=fixture;g_task_count=3;g_status_count=2;g_status_categories[0]=0;g_status_categories[1]=2;
    fixture[0].is_new=TRUE;
    fixture[1].unread_comment=TRUE; /* in both scopes: not a reason to switch */
    fixture[2].assigned_to_me=FALSE;fixture[2].reported_by_me=TRUE;
    fixture[2].unread_comment=TRUE;
    g_reported_scope=FALSE;assert(OtherScopeAttentionCount()==1);
    g_reported_scope=TRUE;assert(OtherScopeAttentionCount()==1);
    fixture[0].is_new=FALSE;fixture[1].unread_comment=FALSE;
    fixture[2].assigned_to_me=TRUE;fixture[2].reported_by_me=FALSE;
    fixture[2].unread_comment=FALSE;g_reported_scope=FALSE;
    RefreshTaskList();
    ListView_SetItemState(g_list,0,LVIS_FOCUSED,LVIS_FOCUSED);
    int context_index=TaskIndexAtRow(0);const wchar_t *saved_comment_id=fixture[context_index].latest_comment_id;
    for(int language=0;language<2;language++){
        g_russian=language;
        fixture[context_index].latest_comment_id=NULL;ShowTaskContextMenu((POINT){-1,-1});
        assert(!wcscmp(captured_comment_action,language?L"Написать Jira комментарий":L"Add comment"));
        fixture[context_index].latest_comment_id=L"";ShowTaskContextMenu((POINT){-1,-1});
        assert(!wcscmp(captured_comment_action,language?L"Написать Jira комментарий":L"Add comment"));
        fixture[context_index].latest_comment_id=L"101";ShowTaskContextMenu((POINT){-1,-1});
        assert(!wcscmp(captured_comment_action,language?L"Комментарии Jira…":L"Jira comments…"));
    }
    fixture[context_index].latest_comment_id=saved_comment_id;g_russian=TRUE;
    wcscpy_s(g_status_names[0],64,L"To Do");wcscpy_s(g_status_names[1],64,L"Done");
    g_flash_task=1;BeginPendingSwitchFeedback(L"T-10");assert(g_flash_task==-1);
    assert(!wcscmp(TaskStatusDisplayText(&fixture[1]),L"Done"));
    assert(!wcscmp(TaskStatusDisplayText(&fixture[0]),L"To Do"));
    assert(HourglassFrame(1000,1000)==0&&HourglassFrame(1179,1000)==0);
    assert(HourglassFrame(1180,1000)==1&&HourglassFrame(1360,1000)==0);
    EndPendingSwitchFeedback();assert(!g_pending_switch_key[0]);
    JiraTransitions menu_transitions={0};menu_transitions.count=2;
    wcscpy_s(menu_transitions.items[0].status_name,128,L"On Hold");wcscpy_s(menu_transitions.items[1].status_name,128,L"Done");
    HMENU transition_menu=CreateStatusTransitionsMenu(&menu_transitions);
    MENUITEMINFOW transition_info={0};transition_info.cbSize=sizeof(transition_info);transition_info.fMask=MIIM_DATA;
    assert(GetMenuItemInfoW(transition_menu,0,TRUE,&transition_info));
    assert(((PopupItem*)transition_info.dwItemData)->plain);DestroyNativeMenu(transition_menu);
    JiraTransitionItem done_transition={0};
    wcscpy_s(done_transition.id,80,L"31");wcscpy_s(done_transition.status_name,128,L"Done");
    wcscpy_s(done_transition.rating_field_id,80,L"customfield_15812");done_transition.rating_count=6;done_transition.field_count=1;
    for(int i=0;i<6;i++)swprintf(done_transition.ratings[i].id,80,L"1571%d",i);
    wcscpy_s(done_transition.ratings[0].value,64,L"☆☆☆☆☆");
    wcscpy_s(done_transition.ratings[1].value,64,L"⭐☆☆☆☆");
    wcscpy_s(done_transition.ratings[2].value,64,L"⭐⭐☆☆☆");
    wcscpy_s(done_transition.ratings[3].value,64,L"⭐⭐⭐☆☆");
    wcscpy_s(done_transition.ratings[4].value,64,L"⭐⭐⭐⭐☆");
    wcscpy_s(done_transition.ratings[5].value,64,L"⭐⭐⭐⭐⭐");
    assert(RatingScoreForOption(done_transition.ratings[0].value,0)==0);
    assert(RatingScoreForOption(done_transition.ratings[4].value,4)==4);
    TransitionInputDialog transition_input={0};
    HWND transition_window=CreateTransitionInputWindow(&transition_input,&done_transition,L"T-2",FALSE);
    assert(!wcscmp(transition_input.placeholder,L"Комментарий к задаче: Zulu active task with a very long name"));
    assert(transition_window&&!GetDlgItem(transition_window,IDC_TRANSITION_RATING));
    for(int i=1;i<6;i++)assert(GetDlgItem(transition_window,IDC_TRANSITION_RATING+i));
    assert((GetWindowLongPtrW(GetDlgItem(transition_window,IDC_TRANSITION_RATING+1),GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW);
    RECT transition_client;GetClientRect(transition_window,&transition_client);
    assert(transition_client.right>=TransitionScale(400)&&transition_client.right<=TransitionScale(480));
    assert(transition_client.bottom<=TransitionScale(210));
    int transition_ids[]={IDC_TRANSITION_RATING+1,IDC_TRANSITION_RATING+5,IDC_TRANSITION_COMMENT,IDC_TRANSITION_REMEMBER,IDCANCEL,IDOK};
    for(int i=0;i<6;i++){
        RECT control;GetWindowRect(GetDlgItem(transition_window,transition_ids[i]),&control);MapWindowPoints(NULL,transition_window,(POINT*)&control,2);
        assert(control.left>=0&&control.top>=0&&control.right<=transition_client.right&&control.bottom<=transition_client.bottom);
    }
    assert(!(GetWindowLongPtrW(GetDlgItem(transition_window,IDC_TRANSITION_COMMENT),GWL_STYLE)&WS_VSCROLL));
    assert(GetDlgItem(transition_window,IDC_TRANSITION_REMEMBER));
    assert(!IsWindowEnabled(GetDlgItem(transition_window,IDOK)));
    RECT remember_rect,cancel_rect,done_rect,comment_rect,first_star,last_star,label_rect;
    GetWindowRect(GetDlgItem(transition_window,IDC_TRANSITION_REMEMBER),&remember_rect);
    GetWindowRect(GetDlgItem(transition_window,IDCANCEL),&cancel_rect);GetWindowRect(GetDlgItem(transition_window,IDOK),&done_rect);
    GetWindowRect(GetDlgItem(transition_window,IDC_TRANSITION_COMMENT),&comment_rect);
    GetWindowRect(GetDlgItem(transition_window,IDC_TRANSITION_RATING+1),&first_star);
    GetWindowRect(GetDlgItem(transition_window,IDC_TRANSITION_RATING+5),&last_star);
    HWND rating_label=GetDlgItem(transition_window,IDC_TRANSITION_HELPER);assert(rating_label);
    GetWindowRect(rating_label,&label_rect);wchar_t rating_caption[32];GetWindowTextW(rating_label,rating_caption,32);
    assert(!wcschr(rating_caption,L'*'));
    assert(remember_rect.top==cancel_rect.top&&cancel_rect.top==done_rect.top);
    assert(remember_rect.right<=cancel_rect.left&&cancel_rect.right<done_rect.left);
    assert(comment_rect.bottom+TransitionScale(8)<=done_rect.top);
    assert(first_star.top==last_star.top&&label_rect.right<=first_star.left);
    assert(first_star.left-label_rect.right<=TransitionScale(6));
    SendMessageW(transition_window,WM_COMMAND,IDOK,0);
    assert(IsWindow(transition_window)&&!transition_input.accepted);
    SendMessageW(transition_window,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING,BN_CLICKED),0);
    assert(transition_input.selected_score==-1&&!IsWindowEnabled(GetDlgItem(transition_window,IDOK)));
    g_dark=FALSE;
    SendMessageW(GetDlgItem(transition_window,IDC_TRANSITION_RATING+4),WM_MOUSEMOVE,0,MAKELPARAM(5,5));
    assert(transition_input.selected_score==-1&&!IsWindowEnabled(GetDlgItem(transition_window,IDOK)));
    assert(TransitionStarCenter(&transition_input,1)==RGB(213,142,0));
    assert(TransitionStarCenter(&transition_input,4)==RGB(213,142,0));
    assert(TransitionStarCenter(&transition_input,5)==PopoverColor());
    SendMessageW(GetDlgItem(transition_window,IDC_TRANSITION_RATING+4),WM_MOUSELEAVE,0,0);
    assert(TransitionStarCenter(&transition_input,1)==PopoverColor());
    SendMessageW(transition_window,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+4,BN_CLICKED),0);
    assert(IsWindowEnabled(GetDlgItem(transition_window,IDOK)));
    assert(TransitionStarCenter(&transition_input,4)==RGB(213,142,0));
    MSG arrow={0};arrow.hwnd=GetDlgItem(transition_window,IDC_TRANSITION_RATING+4);arrow.message=WM_KEYDOWN;arrow.wParam=VK_LEFT;
    if(!IsDialogMessageW(transition_window,&arrow))DispatchMessageW(&arrow);
    assert(transition_input.selected_score==3);
    MSG home={0};home.hwnd=GetDlgItem(transition_window,IDC_TRANSITION_RATING+3);home.message=WM_KEYDOWN;home.wParam=VK_HOME;
    if(!IsDialogMessageW(transition_window,&home))DispatchMessageW(&home);
    assert(transition_input.selected_score==1);
    SendMessageW(transition_window,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+4,BN_CLICKED),0);
    SetWindowTextW(GetDlgItem(transition_window,IDC_TRANSITION_COMMENT),L"Готово");
    SendMessageW(GetDlgItem(transition_window,IDC_TRANSITION_REMEMBER),BM_SETCHECK,BST_CHECKED,0);
    SendMessageW(transition_window,WM_COMMAND,IDOK,0);
    assert(transition_input.accepted&&transition_input.remember_comment&&!wcscmp(transition_input.option_id,L"15714")&&!wcscmp(transition_input.comment,L"Готово"));
    int dialog_dpis[]={96,144,192};
    for(int dpi_index=0;dpi_index<3;dpi_index++){
        g_dpi=dialog_dpis[dpi_index];
        for(int language=0;language<2;language++)for(int theme=0;theme<2;theme++){
            g_russian=language;g_dark=theme;RecreateBrushes();
            TransitionInputDialog variant;
            HWND dialog=CreateTransitionInputWindow(&variant,&done_transition,L"TEST-42",FALSE);assert(dialog);
            assert(!wcscmp(variant.placeholder,language?L"Комментарий к задаче…":L"Comment on task…"));
            RECT client;GetClientRect(dialog,&client);assert(client.bottom<=MulDiv(210,(int)g_dpi,96));
            for(int i=0;i<6;i++){
                RECT control;GetWindowRect(GetDlgItem(dialog,transition_ids[i]),&control);MapWindowPoints(NULL,dialog,(POINT*)&control,2);
                assert(control.left>=0&&control.top>=0&&control.right<=client.right&&control.bottom<=client.bottom);
            }
            HWND remember=GetDlgItem(dialog,IDC_TRANSITION_REMEMBER);wchar_t caption[80];GetWindowTextW(remember,caption,80);
            HDC dc=GetDC(remember);HGDIOBJ font=SelectObject(dc,variant.small_font);SIZE text;
            GetTextExtentPoint32W(dc,caption,(int)wcslen(caption),&text);SelectObject(dc,font);ReleaseDC(remember,dc);
            RECT check;GetClientRect(remember,&check);assert(text.cx+TransitionScale(24)<=check.right);
            SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+2,BN_CLICKED),0);
            assert(TransitionStarCenter(&variant,2)==(theme?RGB(255,190,66):RGB(213,142,0)));
            assert(TransitionStarCenter(&variant,3)==PopoverColor());
            DestroyWindow(dialog);
        }
    }
    g_dpi=144;g_russian=TRUE;g_dark=FALSE;RecreateBrushes();
    int selected[30];assert(SelectTrayTasks(selected,1)==1&&selected[0]==0);
    assert(SelectTrayTasks(selected,30)==2&&selected[1]==2);
    assert(CountForTab(0)==3&&CountForTab(1)==2&&CountForTab(2)==1&&g_tab_reported);
    g_tab=1;g_scope_tabs[0]=1;g_scope_tabs[1]=0;
    ToggleReportedScope();
    assert(g_reported_scope&&g_tab==0&&CountForTab(0)==1&&CountForTab(1)==0&&CountForTab(2)==1);
    assert(ListView_GetItemCount(g_list)==1&&ActiveFilterIndex()==3);
    g_tab=2;g_scope_tabs[1]=2;
    ToggleReportedScope();
    assert(!g_reported_scope&&g_tab==1&&ActiveFilterIndex()==1);
    ToggleReportedScope();
    assert(g_reported_scope&&g_tab==2&&ListView_GetItemCount(g_list)==1&&ActiveFilterIndex()==5);
    ToggleReportedScope();
    assert(!g_reported_scope&&g_tab==1);
    g_tab=0;RefreshTaskList();
    ListView_SetColumnWidth(g_list,2,D(80));
    wchar_t tip[128];assert(TaskSummaryInfoTip(0,tip,_countof(tip)));
    assert(!wcscmp(tip,fixture[0].summary));
    g_comments_mode=0;RECT note_indicator,name_cell;
    assert(NoteIndicatorRectForRow(0,&note_indicator));
    assert(ListView_GetSubItemRect(g_list,0,2,LVIR_BOUNDS,&name_cell));
    assert(note_indicator.left>=name_cell.left&&note_indicator.right<=name_cell.right);
    const wchar_t *original_summary=fixture[0].summary;fixture[0].summary=L"Short task";
    ListView_SetColumnWidth(g_list,2,D(320));
    assert(NoteIndicatorRectForRow(0,&note_indicator));
    assert(ListView_GetSubItemRect(g_list,0,2,LVIR_BOUNDS,&name_cell));
    HDC title_dc=GetDC(g_list);HGDIOBJ title_font=SelectObject(title_dc,g_bold_font);SIZE short_title={0};
    GetTextExtentPoint32W(title_dc,fixture[0].summary,(int)wcslen(fixture[0].summary),&short_title);
    SelectObject(title_dc,title_font);ReleaseDC(g_list,title_dc);
    RECT comment_indicator;assert(!CommentIndicatorRectForRow(0,&comment_indicator));
    assert(note_indicator.left-(name_cell.left+D(7)+short_title.cx)<=D(12));
    assert(note_indicator.right<name_cell.right-D(50));
    fixture[0].summary=original_summary;
    ListView_SetColumnWidth(g_list,2,D(220));
    fixture[0].is_new=TRUE;
    fixture[0].unread_comment=TRUE;fixture[0].latest_comment_id=L"101";
    assert(CommentIndicatorRectForRow(0,&comment_indicator));
    RECT new_indicator;assert(NewIndicatorRectForRow(0,&new_indicator));
    assert(NoteIndicatorRectForRow(0,&note_indicator));
    assert(new_indicator.right<comment_indicator.left&&comment_indicator.right<note_indicator.left);
    ListView_SetColumnWidth(g_list,2,D(140));
    assert(NewIndicatorRectForRow(0,&new_indicator));
    assert(CommentIndicatorRectForRow(0,&comment_indicator)&&NoteIndicatorRectForRow(0,&note_indicator));
    assert(ListView_GetSubItemRect(g_list,0,2,LVIR_BOUNDS,&name_cell));
    assert(new_indicator.right<comment_indicator.left&&comment_indicator.right<note_indicator.left);
    assert(note_indicator.right<=name_cell.right-D(5));
    ListView_SetColumnWidth(g_list,2,D(220));
    wchar_t *saved_note=fixture[0].local_note;fixture[0].local_note=NULL;
    assert(!NoteIndicatorRectForRow(0,&note_indicator));
    assert(PopoverFallbackAnchorForRow(0,&note_indicator));
    fixture[0].local_note=saved_note;
    assert(NoteIndicatorRectForRow(0,&note_indicator));
    fixture[0].is_new=FALSE;fixture[0].unread_comment=FALSE;
    assert(TaskNoteInfoTip(0,tip,_countof(tip))&&wcsstr(tip,L"Важная локальная заметка"));
    POINT note_point={(note_indicator.left+note_indicator.right)/2,(note_indicator.top+note_indicator.bottom)/2};
    assert(NoteIndicatorTaskAtPoint(note_point)==TaskIndexAtRow(0));
    int noted_task=TaskIndexAtRow(0);
    wchar_t note_path[MAX_PATH],persisted_note[2048];
    assert(GetTempFileNameW(temp,L"nts",0,note_path));DeleteFileW(note_path);
    wcscpy_s(g_settings_path,_countof(g_settings_path),note_path);
    wcscpy_s(g_snapshot.account,_countof(g_snapshot.account),L"Test Account");
    fixture[noted_task].summary=L"Short";
    fixture[noted_task].is_new=TRUE;
    assert(CommentIndicatorRectForRow(0,&comment_indicator));
    NMITEMACTIVATE comment_click={0};comment_click.hdr.hwndFrom=g_list;
    comment_click.hdr.idFrom=IDC_TASKS;comment_click.hdr.code=NM_CLICK;
    comment_click.iItem=0;comment_click.iSubItem=2;
    comment_click.ptAction=(POINT){(comment_indicator.left+comment_indicator.right)/2,(comment_indicator.top+comment_indicator.bottom)/2};
    g_jira_busy=TRUE;
    WindowProcedure(g_window,WM_NOTIFY,IDC_TASKS,(LPARAM)&comment_click);
    assert(g_popover.window&&g_popover.kind==1);
    assert(!fixture[noted_task].is_new);
    CloseTaskPopover();g_jira_busy=FALSE;
    fixture[noted_task].summary=original_summary;
    assert(NoteIndicatorRectForRow(0,&note_indicator));
    OpenTaskPopover(noted_task,2,note_indicator);
    assert(g_popover.window&&g_popover.kind==2&&g_popover.input);
    assert(GetWindowLongPtrW(g_popover.input,GWL_STYLE)&WS_VSCROLL);
    RECT unused_dock_area={g_popover.body_right-D(69),D(3),g_popover.body_right-D(38),D(32)};
    SendMessageW(g_popover.window,WM_LBUTTONUP,0,
        MAKELPARAM((unused_dock_area.left+unused_dock_area.right)/2,(unused_dock_area.top+unused_dock_area.bottom)/2));
    assert(g_popover.window&&!g_detached_popovers); /* A click is not an undock command. */
    RECT note_outer_before,note_input_before,note_input_after;
    GetWindowRect(g_popover.window,&note_outer_before);GetWindowRect(g_popover.input,&note_input_before);
    SetWindowPos(g_popover.window,NULL,0,0,note_outer_before.right-note_outer_before.left+D(45),
        note_outer_before.bottom-note_outer_before.top+D(35),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    GetWindowRect(g_popover.input,&note_input_after);
    assert(note_input_after.right-note_input_after.left>note_input_before.right-note_input_before.left);
    assert(note_input_after.bottom-note_input_after.top>note_input_before.bottom-note_input_before.top);
    SetWindowTextW(g_popover.input,L"Draft saved while popover is open");
    SendMessageW(g_popover.window,WM_COMMAND,MAKEWPARAM(IDC_POPOVER_INPUT,EN_CHANGE),(LPARAM)g_popover.input);
    assert(NativeNoteLoad(g_settings_path,g_snapshot.account,L"T-2",persisted_note,_countof(persisted_note)));
    assert(!wcscmp(persisted_note,L"Draft saved while popover is open"));
    g_russian=!g_russian;UpdateLanguage();
    assert(g_popover.window&&g_popover.kind==2);
    wchar_t draft[2048];GetWindowTextW(g_popover.input,draft,_countof(draft));
    assert(!wcscmp(draft,L"Draft saved while popover is open"));
    RECT note_input_attached;GetWindowRect(g_popover.input,&note_input_attached);
    TaskPopover *detached_note=DetachTaskPopover();
    assert(detached_note&&detached_note->detached&&detached_note->window&&!g_popover.window);
    RECT note_input_detached;GetWindowRect(detached_note->input,&note_input_detached);
    assert(EqualRect(&note_input_attached,&note_input_detached));
    RECT detached_bounds;GetClientRect(detached_note->window,&detached_bounds);
    assert(detached_note->body_left==0&&detached_note->body_right==detached_bounds.right);
    LayoutControls(740,550);assert(IsWindow(detached_note->window));
    assert(CommentIndicatorRectForRow(0,&comment_indicator));
    g_jira_busy=TRUE;OpenTaskPopover(noted_task,1,comment_indicator);
    assert(g_popover.window&&g_popover.kind==1&&g_popover.input&&g_popover.text);
    assert(g_popover.scroll_to_latest);
    /* An overlapping popover must not compress the main window's footer. */
    RECT owner_client,footer_before,updated_before,footer_after,updated_after,popover_before;
    GetClientRect(g_window,&owner_client);GetWindowRect(g_popover.window,&popover_before);
    ShowWindow(g_popover.window,SW_HIDE);
    LayoutFooterControls(owner_client.right,owner_client.bottom);
    GetWindowRect(g_refresh,&footer_before);GetWindowRect(g_updated,&updated_before);
    POINT overlap={D(240),owner_client.bottom-D(70)};ClientToScreen(g_window,&overlap);
    SetWindowPos(g_popover.window,NULL,overlap.x,overlap.y,0,0,
        SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    LayoutFooterControls(owner_client.right,owner_client.bottom);
    GetWindowRect(g_refresh,&footer_after);GetWindowRect(g_updated,&updated_after);
    assert(EqualRect(&footer_before,&footer_after));
    assert(EqualRect(&updated_before,&updated_after));
    SetWindowPos(g_popover.window,NULL,popover_before.left,popover_before.top,0,0,
        SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    RECT popover_client;GetClientRect(g_popover.window,&popover_client);
    assert(PopoverResizeGripAt(popover_client.right-D(8),popover_client.bottom-D(8),popover_client));
    assert(!PopoverResizeGripAt(popover_client.right/2,popover_client.bottom/2,popover_client));
    HRGN resize_region=CreateRectRgn(0,0,1,1);assert(resize_region);
    assert(GetWindowRgn(g_popover.window,resize_region)!=ERROR);
    SendMessageW(g_popover.window,WM_LBUTTONDOWN,0,MAKELPARAM(popover_client.right-D(8),popover_client.bottom-D(8)));
    assert(g_popover.resizing&&GetWindowRgn(g_popover.window,resize_region)!=ERROR);
    assert(!PtInRegion(resize_region,D(2),popover_client.bottom/2));
    SetWindowPos(g_popover.window,NULL,0,0,popover_client.right+D(70),popover_client.bottom+D(40),
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    assert(GetWindowRgn(g_popover.window,resize_region)!=ERROR);
    assert(PtInRegion(resize_region,popover_client.right+D(30),popover_client.bottom+D(10)));
    /* Each drag step must present the parent and its children before returning. */
    GetWindowRect(g_popover.window,&g_popover.resize_initial);
    g_popover.resize_initial.right+=D(30);g_popover.resize_initial.bottom+=D(20);
    GetCursorPos(&g_popover.resize_start);
    SetWindowSubclass(g_popover.text,TraceResizeFramePaint,77,0);resize_nc_paints=0;
    SendMessageW(g_popover.window,WM_MOUSEMOVE,MK_LBUTTON,0);
    assert(!GetUpdateRect(g_popover.window,NULL,FALSE));
    assert(!GetUpdateRect(g_popover.text,NULL,FALSE));
    assert(!GetUpdateRect(g_popover.input,NULL,FALSE));
    assert(!GetUpdateRect(g_popover.send,NULL,FALSE));
    assert(resize_nc_paints>0); /* Native scrollbar frame must also be presented. */
    RemoveWindowSubclass(g_popover.text,TraceResizeFramePaint,77);
    SendMessageW(g_popover.window,WM_LBUTTONUP,0,MAKELPARAM(popover_client.right-D(8),popover_client.bottom-D(8)));
    assert(!g_popover.resizing&&GetWindowRgn(g_popover.window,resize_region)!=ERROR);
    DeleteObject(resize_region);
    g_popover.loading=FALSE;
    g_popover.comments.count=12;
    g_popover.comments.items=calloc(12,sizeof(JiraComment));assert(g_popover.comments.items);
    for(int i=0;i<12;i++){
        wcscpy_s(g_popover.comments.items[i].author,256,L"Jira author");
        wcscpy_s(g_popover.comments.items[i].created,64,L"2026-10-02T12:45:00.000+0300");
        wcscpy_s(g_popover.comments.items[i].body,2048,L"A longer Jira comment that should wrap inside its feed item and require scrolling.");
    }
    RefreshTaskPopover();
    assert(g_popover.comment_scroll_max>0);
    assert(!g_popover.scroll_to_latest&&g_popover.comment_scroll==g_popover.comment_scroll_max);
    SendMessageW(g_popover.text,WM_VSCROLL,SB_PAGEUP,0);
    assert(g_popover.comment_scroll<g_popover.comment_scroll_max);
    assert(g_popover.comment_scroll>0);
    int browsed_position=g_popover.comment_scroll;
    RefreshTaskPopover();
    assert(g_popover.comment_scroll==browsed_position);
    SetWindowTextW(g_popover.input,L"An unfinished Jira draft");
    g_dark=!g_dark;ApplyTheme();
    assert(g_popover.window&&g_popover.comment_scroll>0);
    g_russian=!g_russian;UpdateLanguage();
    assert(g_popover.window&&g_popover.comment_scroll>0);
    ToggleDensity();
    assert(g_popover.window&&g_popover.comment_scroll>0);
    assert(IsWindow(detached_note->window));
    GetWindowTextW(g_popover.input,draft,_countof(draft));
    assert(!wcscmp(draft,L"An unfinished Jira draft"));
    RECT compose_bounds,send_bounds;GetWindowRect(g_popover.input,&compose_bounds);
    GetWindowRect(g_popover.send,&send_bounds);
    assert(send_bounds.top-compose_bounds.bottom>=D(8));
    RECT comment_input_attached;GetWindowRect(g_popover.input,&comment_input_attached);
    TaskPopover *detached_comments=DetachTaskPopover();
    assert(detached_comments&&detached_comments->detached&&detached_comments!=detached_note);
    RECT comment_input_detached;GetWindowRect(detached_comments->input,&comment_input_detached);
    assert(EqualRect(&comment_input_attached,&comment_input_detached));
    assert(!g_popover.window&&g_detached_popovers==detached_comments&&detached_comments->next==detached_note);
    GetWindowTextW(detached_comments->input,draft,_countof(draft));
    assert(!wcscmp(draft,L"An unfinished Jira draft"));
    assert(AttachTaskPopover(detached_comments,FALSE));
    assert(g_popover.window&&g_popover.kind==1&&g_detached_popovers==detached_note);
    RECT comment_input_reattached;GetWindowRect(g_popover.input,&comment_input_reattached);
    assert(comment_input_reattached.right-comment_input_reattached.left==comment_input_detached.right-comment_input_detached.left);
    assert(comment_input_reattached.bottom-comment_input_reattached.top==comment_input_detached.bottom-comment_input_detached.top);
    CloseTaskPopover();assert(!g_popover.window);
    assert(AttachTaskPopover(detached_note,FALSE));
    assert(g_popover.window&&g_popover.kind==2&&!g_detached_popovers);
    GetWindowTextW(g_popover.input,draft,_countof(draft));
    assert(!wcscmp(draft,L"Draft saved while popover is open"));
    CloseTaskPopover();g_jira_busy=FALSE;
    ApplyTaskSort(1);assert(TaskIndexAtRow(0)==2&&TaskIndexAtRow(1)==0&&TaskIndexAtRow(2)==1);
    ApplyTaskSort(1);assert(TaskIndexAtRow(0)==1&&TaskIndexAtRow(1)==0&&TaskIndexAtRow(2)==2);
    ApplyTaskSort(2);assert(TaskIndexAtRow(0)==1&&TaskIndexAtRow(1)==2&&TaskIndexAtRow(2)==0);
    ApplyTaskSort(STATUS_COLUMN);assert(TaskIndexAtRow(0)==1);
    g_russian=TRUE;g_comments_mode=0;UpdateColumnLabels();
    RefreshTaskList();
    DeleteFileW(note_path);g_settings_path[0]=0;g_snapshot.account[0]=0;
    wchar_t column_text[80];HDITEMW column={0};column.mask=HDI_TEXT;column.pszText=column_text;column.cchTextMax=80;
    Header_GetItem(ListView_GetHeader(g_list),COMMENTS_COLUMN,&column);assert(!column_text[0]);
    g_russian=FALSE;UpdateColumnLabels();Header_GetItem(ListView_GetHeader(g_list),COMMENTS_COLUMN,&column);assert(!column_text[0]);
    RECT pill,cell;assert(StatusPillRectForRow(0,&pill));
    assert(ListView_GetSubItemRect(g_list,0,STATUS_COLUMN,LVIR_BOUNDS,&cell));
    POINT inside={(pill.left+pill.right)/2,(pill.top+pill.bottom)/2};
    POINT outside={cell.left+1,(cell.top+cell.bottom)/2};
    assert(StatusTaskAtPoint(inside)==TaskIndexAtRow(0));
    assert(StatusTaskAtPoint(outside)==-1);
    RECT pressed_pill=StatusPressedPillRect(pill,TRUE),released_pill=StatusPressedPillRect(pill,FALSE);
    assert(pressed_pill.top==pill.top+D(1)&&pressed_pill.bottom==pill.bottom+D(1));
    assert(EqualRect(&released_pill,&pill));
    COLORREF normal=StatusBackground(0,FALSE,FALSE),pressed=StatusBackground(0,FALSE,TRUE);
    assert(normal!=pressed);
    g_status_pulse_task=TaskIndexAtRow(0);g_status_pulse_started=1000;
    assert(StatusPulseActive(TaskIndexAtRow(0),1159));assert(!StatusPulseActive(TaskIndexAtRow(0),1160));
    assert(!StatusPulseActive(TaskIndexAtRow(1),1100));g_status_pulse_task=-1;
    assert(!ShouldClearStatusPress(WM_LBUTTONDOWN));
    assert(ShouldClearStatusPress(WM_LBUTTONUP));
    assert(ShouldClearStatusPress(WM_RBUTTONUP));
    assert(ShouldClearStatusPress(WM_CAPTURECHANGED));
    assert(ShouldClearStatusPress(WM_CANCELMODE));
    RECT updated_bounds={0,0,100,28},updated_text=UpdatedTextRect(updated_bounds);
    assert(updated_text.right==100-D(3));
    free(fixture[0].local_note);g_tasks=NULL;g_task_count=0;
    SetWindowTextW(s_interval,L"0");BeginAccountCheck();assert(!s_busy);
    SetWindowTextW(s_interval,L"15");SetWindowTextW(s_url,L"http://invalid.example");BeginAccountCheck();assert(!s_busy);
    s_connected=TRUE;LayoutSettings();assert(!(GetWindowLongPtrW(s_token,GWL_STYLE)&WS_VISIBLE));
    /* Signing out must rebuild the disconnected form atomically: keep the valid
       server URL, clear only the secret and leave no pending half-painted layout. */
    const wchar_t *signed_out_url=L"https://jira.example.test/jira/";
    SetWindowTextW(s_url,signed_out_url);SetWindowTextW(s_token,L"PAT");
    ApplySignedOutSettingsUi(signed_out_url);UpdateWindow(settings);
    wchar_t signed_out_value[768];GetWindowTextW(s_url,signed_out_value,_countof(signed_out_value));
    assert(!wcscmp(signed_out_value,signed_out_url));
    assert(GetWindowTextLengthW(s_token)==0);
    assert(!s_connected&&!s_has_account);
    assert(GetWindowLongPtrW(s_token,GWL_STYLE)&WS_VISIBLE);
    assert(!(GetWindowLongPtrW(GetDlgItem(settings,215),GWL_STYLE)&WS_VISIBLE));
    assert(!GetUpdateRect(settings,NULL,FALSE));
    SetWindowTextW(s_url,L"https://different-account.example.test");SetWindowTextW(s_token,L"");BeginAccountCheck();assert(!s_busy);
    assert(GetWindowLongPtrW(s_token,GWL_STYLE)&WS_VISIBLE); /* New server must expose token input, not reuse hidden credentials. */
    for(int p=0;p<2;p++)for(int i=0;i<s_counts[p];i++){
        LONG_PTR style=GetWindowLongPtrW(s_pages[p][i],GWL_STYLE);
        assert(!(style&(WS_VSCROLL|WS_HSCROLL)));
    }
    assert(SETTINGS_HEIGHT_DIP<=395);
    assert(GetDlgItem(settings,200)&&GetDlgItem(settings,201));
    assert(!GetDlgItem(settings,202)&&!GetDlgItem(settings,220));
    wchar_t about_tab[64];GetWindowTextW(GetDlgItem(settings,201),about_tab,64);
    assert(!wcscmp(about_tab,L"О приложении"));
    SettingsTab(1);assert(GetWindowLongPtrW(GetDlgItem(settings,230),GWL_STYLE)&WS_VISIBLE);
    g_russian=FALSE;UpdateSettingsLanguage();
    GetWindowTextW(GetDlgItem(settings,201),about_tab,64);assert(!wcscmp(about_tab,L"About"));
    wchar_t settings_label[96];GetWindowTextW(GetDlgItem(settings,221),settings_label,96);
    assert(!wcscmp(settings_label,L"Update interval, min"));
    GetWindowTextW(settings,settings_label,96);assert(!wcscmp(settings_label,L"Jira Task Manager · Settings"));
    DestroyWindow(settings);
    /* Opening settings must return immediately, leaving both windows usable. */
    SetTimer(g_window,42,20,CloseModalRegression);
    OpenSettingsWindow();
    KillTimer(g_window,42);
    assert(s_window&&IsWindow(s_window)&&IsWindowEnabled(g_window));
    HWND first=s_window;OpenSettingsWindow();assert(s_window==first);
    SetWindowTextW(s_interval,L"25");
    g_jira_busy=TRUE;BeginAccountCheck();assert(!s_busy&&!s_thread);
    g_jira_busy=FALSE;
    DestroyWindow(s_window);
    puts("Settings and footer tests passed");
    return 0;
}
