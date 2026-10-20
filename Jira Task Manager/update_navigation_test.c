#include <windows.h>
#include <assert.h>
static HWND positioned_window,positioned_after;static UINT positioned_flags;static int zorder_requests;
static BOOL WINAPI ObservePopoverPosition(HWND window,HWND after,int x,int y,int width,int height,UINT flags){
    if(after==HWND_TOPMOST||after==HWND_NOTOPMOST){positioned_window=window;positioned_after=after;positioned_flags=flags;zorder_requests++;}
    return SetWindowPos(window,after,x,y,width,height,flags);
}
static BOOL WINAPI HidePopoverFixture(HWND window,INT show){(void)window;(void)show;return TRUE;}
#define ShowWindow HidePopoverFixture
#define SetWindowPos ObservePopoverPosition
#include "native.c"
#undef ShowWindow
#undef SetWindowPos
static void tab_double_click(int tab){WindowProcedure(g_window,WM_COMMAND,MAKEWPARAM(IDC_TAB_ALL+tab,BN_DOUBLECLICKED),0);}
int main(void){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_tab=0;RecreateBrushes();UpdateFonts();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"nav",0,g_settings_path));
    TaskItem fixture[]={
        {.key=L"NAV-1",.summary=L"Read",.assigned_to_me=TRUE,.status=STATUS_TODO},
        {.key=L"NAV-2",.summary=L"New current",.assigned_to_me=TRUE,.status=STATUS_TODO,.is_new=TRUE},
        {.key=L"NAV-3",.summary=L"Unread done",.assigned_to_me=TRUE,.status=STATUS_DONE,.unread_comment=TRUE,.latest_comment_id=L"11"},
        {.key=L"NAV-4",.summary=L"New done",.assigned_to_me=TRUE,.status=STATUS_DONE,.is_new=TRUE},
        {.key=L"NAV-5",.summary=L"Outgoing",.reported_by_me=TRUE,.status=STATUS_DONE,.unread_comment=TRUE,.latest_comment_id=L"12"}};
    g_tasks=fixture;g_task_count=5;g_status_count=STATUS_COUNT;g_status_categories[STATUS_DONE]=2;RefreshTaskList();
    tab_double_click(2);assert(g_tab==2&&g_flash_task==2);assert(fixture[2].unread_comment);
    tab_double_click(2);assert(g_flash_task==3&&fixture[3].is_new);
    tab_double_click(2);assert(g_flash_task==2);
    tab_double_click(1);assert(g_flash_task==1);
    tab_double_click(0);assert(g_flash_task==1);
    tab_double_click(0);assert(g_flash_task==2);
    fixture[2].unread_comment=FALSE;tab_double_click(0);assert(g_flash_task==3);
    g_reported_scope=TRUE;tab_double_click(2);assert(g_flash_task==4);
    g_reported_scope=FALSE;fixture[1].is_new=fixture[3].is_new=FALSE;
    tab_double_click(2);assert(g_flash_task==-1); /* No activity is a no-op, not a random row jump. */
    g_notifications.comments=FALSE;g_reported_scope=TRUE;tab_double_click(2);assert(g_flash_task==-1);
    g_notifications.comments=TRUE;g_reported_scope=FALSE;g_tab=0;RefreshTaskList();
    TaskItem many[220]={0};wchar_t many_keys[220][80];
    for(int i=0;i<220;i++){
        swprintf(many_keys[i],80,L"LONG-%d",i);many[i]=(TaskItem){.key=many_keys[i],.summary=L"Long done list",
            .status=STATUS_DONE,.assigned_to_me=TRUE,.is_new=i==180};
    }
    g_tasks=many;g_task_count=220;tab_double_click(2);assert(g_flash_task==180);
    assert(ListView_GetTopIndex(g_list)>150);assert(many[180].is_new);
    assert(FlashOpacity(g_flash_started+120)>0.05);assert(FlashOpacity(g_flash_started+1000)==0);
    g_tasks=fixture;g_task_count=5;g_tab=0;RefreshTaskList();
    /* Use the same native pin command for notes and comments; preserve their contents. */
    g_jira_busy=TRUE;
    ShowWindow(g_window,SW_SHOWNOACTIVATE);assert(IsWindowVisible(g_window));
    for(int kind=1;kind<=2;kind++){
        OpenTaskPopover(0,kind,(RECT){D(200),D(30),D(215),D(45)});assert(g_attached_popover.window);
        SetWindowTextW(g_popover.input,L"Retained draft");
        HWND popup=g_popover.window;
        /* Do not steal foreground from the user's apps. Exercise the real
           native request/ownership boundary; Windows may deny foreground
           promotion to a background harness. Verify its resulting style only
           when this fixture already has foreground permission. */
        SetWindowPos(popup,NULL,30000,30000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(popup,SW_SHOWNOACTIVATE);assert(IsWindowVisible(popup));
        BOOL foreground=GetForegroundWindow()==popup;int previous_requests=zorder_requests;
        SendMessageW(popup,WM_COMMAND,1043,0);
        assert(zorder_requests==previous_requests+1&&positioned_window==popup&&positioned_after==HWND_TOPMOST);
        assert((positioned_flags&(SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))==(SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE));
        TaskPopover *pinned=FindDetachedPopover(kind,L"NAV-1");assert(pinned&&pinned->window==popup);
        assert(GetWindow(popup,GW_OWNER)==NULL);
        if(foreground)assert(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST);
        wchar_t draft[80];GetWindowTextW(pinned->input,draft,80);assert(!wcscmp(draft,L"Retained draft"));
        SetWindowPos(popup,NULL,30000,30000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(popup,SW_SHOWNOACTIVATE);SetFocus(pinned->input);assert(GetFocus()==pinned->input);
        MSG tab={.hwnd=pinned->input,.message=WM_KEYDOWN,.wParam=VK_TAB};
        assert(HandleTaskPopoverDialogMessage(&tab));assert(GetFocus()==pinned->key_button);
        tab.hwnd=pinned->key_button;assert(HandleTaskPopoverDialogMessage(&tab));assert(GetFocus()==pinned->pin_button);
        tab.hwnd=pinned->pin_button;assert(HandleTaskPopoverDialogMessage(&tab));assert(GetFocus()==pinned->close_button);
        BYTE previous_keys[256],shift_keys[256];assert(GetKeyboardState(previous_keys));memcpy(shift_keys,previous_keys,256);
        shift_keys[VK_SHIFT]=0x80;assert(SetKeyboardState(shift_keys));tab.hwnd=pinned->close_button;
        assert(HandleTaskPopoverDialogMessage(&tab));assert(GetFocus()==pinned->pin_button);assert(SetKeyboardState(previous_keys));
        SetFocus(pinned->input);MSG enter={.hwnd=pinned->input,.message=WM_KEYDOWN,.wParam=VK_RETURN};
        assert(SendMessageW(pinned->input,WM_GETDLGCODE,VK_RETURN,(LPARAM)&enter)&DLGC_WANTMESSAGE);
        HandleTaskPopoverDialogMessage(&enter);assert(IsWindow(popup)&&!pinned->sending);
        SendMessageW(pinned->pin_button,WM_KEYDOWN,VK_SPACE,0);SendMessageW(pinned->pin_button,WM_KEYUP,VK_SPACE,0);
        assert(!pinned->pinned&&GetWindow(popup,GW_OWNER)==NULL);
        assert(positioned_window==popup&&positioned_after==HWND_NOTOPMOST);
        SendMessageW(pinned->pin_button,WM_KEYDOWN,VK_SPACE,0);SendMessageW(pinned->pin_button,WM_KEYUP,VK_SPACE,0);
        assert(pinned->pinned&&GetWindow(popup,GW_OWNER)==NULL);
        TaskPopover *previous_context=g_active_popover;g_active_popover=pinned;g_russian=FALSE;RefreshTaskPopover();
        wchar_t label[80];GetWindowTextW(pinned->pin_button,label,80);assert(!wcscmp(label,L"Unpin"));
        GetWindowTextW(pinned->close_button,label,80);assert(!wcscmp(label,L"Close"));
        g_russian=TRUE;RefreshTaskPopover();g_active_popover=previous_context;
        WindowProcedure(g_window,WM_SIZE,SIZE_MINIMIZED,MAKELPARAM(0,0));assert(IsWindow(popup));
        assert(!SnapDetachedPopover(pinned));
        SendMessageW(popup,WM_COMMAND,1043,0);assert(GetWindow(popup,GW_OWNER)==NULL);
        assert(!(GetWindowLongPtrW(popup,GWL_EXSTYLE)&WS_EX_TOPMOST));
        HWND close_button=pinned->close_button;SetFocus(close_button);
        SendMessageW(close_button,WM_KEYDOWN,VK_SPACE,0);SendMessageW(close_button,WM_KEYUP,VK_SPACE,0);assert(!IsWindow(popup));
        assert(!FindDetachedPopover(kind,L"NAV-1"));
    }
    g_tasks=NULL;g_task_count=0;g_jira_busy=FALSE;DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Activity navigation and pinned note/comment lifecycle passed");return 0;
}
