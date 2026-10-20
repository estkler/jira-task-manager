#include <windows.h>
#include <assert.h>
static HANDLE CompletionWatchThread(LPTHREAD_START_ROUTINE entry,void *parameter){return CreateThread(NULL,0,entry,parameter,0,NULL);}
static void *completion_test_job;
static int completion_test_starts,completion_test_messages;
static BOOL completion_test_fail_thread;
static HANDLE WINAPI CompletionTestThread(LPSECURITY_ATTRIBUTES security,SIZE_T stack,LPTHREAD_START_ROUTINE entry,LPVOID parameter,DWORD flags,LPDWORD id)
{
    (void)security;(void)stack;(void)entry;(void)flags;(void)id;
    if(completion_test_fail_thread)return NULL;completion_test_starts++;completion_test_job=parameter;return CreateEventW(NULL,TRUE,FALSE,NULL);
}
static int WINAPI CompletionTestMessage(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){(void)owner;(void)text;(void)title;(void)flags;completion_test_messages++;return IDOK;}
#define CreateThread CompletionTestThread
#define MessageBoxW CompletionTestMessage
#include "native.c"
static unsigned long long RatingPixels(TransitionInputDialog *dialog,UINT state,COLORREF *center)
{
    HWND star=GetDlgItem(dialog->window,IDC_TRANSITION_RATING+1);RECT bounds;GetClientRect(star,&bounds);
    HDC target=GetDC(star),dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,bounds.right,bounds.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
    DRAWITEMSTRUCT draw={.CtlID=IDC_TRANSITION_RATING+1,.hwndItem=star,.hDC=dc,.rcItem=bounds,.itemState=state};DrawTransitionButton(dialog,&draw);
    unsigned long long hash=1469598103934665603ULL;for(int y=0;y<bounds.bottom;y++)for(int x=0;x<bounds.right;x++){hash^=GetPixel(dc,x,y);hash*=1099511628211ULL;}
    if(center)*center=GetPixel(dc,bounds.right/2,bounds.bottom/2);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(star,target);return hash;
}
static HWND completion_test_popup;static BOOL completion_test_disabled;
static BOOL CALLBACK FindCompletionPopup(HWND window,LPARAM owner)
{
    wchar_t name[100];GetClassNameW(window,name,100);
    if(!wcscmp(name,L"Task Manager.Transition Input")&&GetWindow(window,GW_OWNER)==(HWND)owner){completion_test_popup=window;return FALSE;}return TRUE;
}
static DWORD WINAPI WatchCompletionModal(void *owner)
{
    DWORD thread=GetWindowThreadProcessId((HWND)owner,NULL);
    for(int i=0;i<1000;i++){EnumThreadWindows(thread,FindCompletionPopup,(LPARAM)owner);if(completion_test_popup&&IsWindowVisible(completion_test_popup)){
        completion_test_disabled=!IsWindowEnabled((HWND)owner);if(completion_test_disabled)PostMessageW(completion_test_popup,WM_COMMAND,IDCANCEL,0);return 0;}Sleep(2);}
    return 0;
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;RecreateBrushes();INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temporary[MAX_PATH];GetTempPathW(MAX_PATH,temporary);GetTempFileNameW(temporary,L"cmp",0,g_settings_path);
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    JiraTransitionItem transition={0};wcscpy_s(transition.id,80,L"finish");wcscpy_s(transition.rating_field_id,80,L"rating");wcscpy_s(transition.status_id,80,L"done");wcscpy_s(transition.status_name,128,L"Done");transition.rating_count=6;transition.field_count=1;
    for(int i=0;i<6;i++){swprintf(transition.ratings[i].id,80,L"%d",i);for(int j=0;j<5;j++)transition.ratings[i].value[j]=j<i?L'★':L'☆';}
    TransitionInputDialog dialog;HWND popup=CreateTransitionInputWindow(&dialog,&transition,L"A-1",FALSE);assert(popup);
    SendMessageW(popup,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+5,BN_CLICKED),0);
    COLORREF center;unsigned long long pressed=RatingPixels(&dialog,ODS_SELECTED,&center);
    assert(center==RGB(255,190,48)); /* Press feedback must not darken the selected gold. */
    assert(pressed!=RatingPixels(&dialog,0,NULL));
    SendMessageW(GetDlgItem(popup,IDC_TRANSITION_RATING+2),WM_MOUSEMOVE,0,MAKELPARAM(10,10));
    SendMessageW(GetDlgItem(popup,IDC_TRANSITION_RATING+2),WM_MOUSELEAVE,0,0);
    SendMessageW(popup,WM_MOUSEMOVE,0,MAKELPARAM(TransitionScale(123),TransitionScale(58)));
    assert(dialog.hover==GetDlgItem(popup,IDC_TRANSITION_RATING+2)&&dialog.selected_score==5);
    assert(RatingPixels(&dialog,ODS_FOCUS,NULL)==RatingPixels(&dialog,0,NULL));
    assert(!GetDlgItem(popup,5501));DestroyWindow(popup);
    HANDLE watcher=CompletionWatchThread(WatchCompletionModal,g_window);assert(watcher);
    assert(OpenCompletionEditor(&transition,L"A-1",TRUE));WaitForSingleObject(watcher,3000);CloseHandle(watcher);
    assert(!completion_test_disabled&&IsWindowEnabled(g_window)&&IsWindow(completion_test_popup));
    CompletionCloseEditors();
    wcscpy_s(g_snapshot.server,800,L"preview.invalid:443/jira");wcscpy_s(g_snapshot.identity.account_id,256,L"completion-test-id");wcscpy_s(g_snapshot.account,256,L"completion-test");
    g_tasks=calloc(1,sizeof(*g_tasks));assert(g_tasks);g_task_count=1;
    g_tasks[0]=(TaskItem){.key=L"A-1",.summary=L"Stable task",.status=STATUS_PROGRESS,.active=TRUE,.assigned_to_me=TRUE};
    g_status_count=STATUS_COUNT;g_status_categories[STATUS_DONE]=2;wcscpy_s(g_status_names[STATUS_PROGRESS],128,L"In Progress");wcscpy_s(g_status_names[STATUS_DONE],128,L"Done");wcscpy_s(g_status_ids[STATUS_PROGRESS],80,L"progress");wcscpy_s(g_status_ids[STATUS_DONE],80,L"done");
    g_tab=1;g_reported_scope=FALSE;g_jira_stale=FALSE;g_updated_time.wYear=2026;RefreshTaskList();
    assert(OpenCompletionEditor(&transition,L"A-1",FALSE));assert(OpenCompletionEditor(&transition,L"A-1",FALSE));
    assert(g_completion_editors&&!g_completion_editors->next);CompletionEditor *editor=g_completion_editors;popup=editor->dialog.window;
    SetWindowTextW(GetDlgItem(popup,IDC_TRANSITION_COMMENT),L"Preserve this draft");SendMessageW(popup,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+4,BN_CLICKED),0);
    WindowProcedure(g_window,WM_COMMAND,IDC_LANGUAGE,0);wchar_t caption[128];GetWindowTextW(GetDlgItem(popup,IDOK),caption,128);assert(!wcscmp(caption,L"Complete"));
    WindowProcedure(g_window,WM_COMMAND,IDC_THEME,0);WindowProcedure(g_window,WM_COMMAND,IDC_TAB_REPORTED,0);SetWindowTextW(g_search,L"another task");
    assert(IsWindow(popup)&&editor->dialog.selected_score==4&&!wcscmp(editor->dialog.key,L"A-1"));
    UINT original_dpi=editor->dialog.dpi;g_dpi=192;SendMessageW(popup,WM_MOUSEMOVE,0,MAKELPARAM(MulDiv(123,original_dpi,96),MulDiv(58,original_dpi,96)));
    assert(editor->dialog.hover==GetDlgItem(popup,IDC_TRANSITION_RATING+2));
    RECT suggested={0,0,860,388};SendMessageW(popup,WM_DPICHANGED,MAKEWPARAM(192,192),(LPARAM)&suggested);
    RECT star_bounds;GetClientRect(GetDlgItem(popup,IDC_TRANSITION_RATING+1),&star_bounds);assert(editor->dialog.dpi==192&&star_bounds.right==56);
    int starts=completion_test_starts;wcscpy_s(g_snapshot.identity.account_id,256,L"different-account");SendMessageW(popup,WM_COMMAND,IDOK,0);
    assert(completion_test_starts==starts&&IsWindow(popup));wcscpy_s(g_snapshot.identity.account_id,256,L"completion-test-id");
    g_jira_busy=TRUE;SendMessageW(popup,WM_COMMAND,IDOK,0);assert(completion_test_starts==starts&&IsWindow(popup));g_jira_busy=FALSE;
    SendMessageW(popup,WM_COMMAND,IDOK,0);assert(completion_test_starts==starts+1&&editor->sending&&g_jira_busy);
    JiraJob *request=completion_test_job;assert(request->completion_generation==editor->generation&&!wcscmp(request->key,L"A-1")&&!wcscmp(request->transition_option,L"4")&&!wcscmp(request->transition_comment,L"Preserve this draft"));
    SendMessageW(popup,WM_COMMAND,IDOK,0);assert(completion_test_starts==starts+1);
    assert(OpenCompletionEditor(&transition,L"A-2",FALSE));HWND independent=g_completion_editors->dialog.window;
    SetWindowTextW(GetDlgItem(independent,IDC_TRANSITION_COMMENT),L"Independent draft");SendMessageW(independent,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+2,BN_CLICKED),0);
    SendMessageW(independent,WM_COMMAND,IDOK,0);assert(completion_test_starts==starts+1&&IsWindow(independent));SendMessageW(independent,WM_COMMAND,IDCANCEL,0);CompletionCollectClosed();
    request->success=request->loaded=FALSE;wcscpy_s(request->error,256,L"Test failure");FinishJiraJob(request);
    GetWindowTextW(GetDlgItem(popup,IDC_TRANSITION_COMMENT),caption,128);assert(IsWindow(popup)&&!editor->sending&&editor->dialog.selected_score==4&&!wcscmp(caption,L"Preserve this draft"));
    g_jira_stale=FALSE;g_reported_scope=FALSE;SetWindowTextW(g_search,L"");RefreshTaskList();SendMessageW(popup,WM_COMMAND,IDOK,0);request=completion_test_job;
    request->success=request->loaded=TRUE;request->snapshot.identity=g_snapshot.identity;wcscpy_s(request->snapshot.server,800,g_snapshot.server);wcscpy_s(request->snapshot.account,256,g_snapshot.account);
    request->snapshot.items=calloc(1,sizeof(JiraItem));assert(request->snapshot.items);request->snapshot.count=1;
    JiraItem *confirmed=request->snapshot.items;wcscpy_s(confirmed->key,80,L"A-1");wcscpy_s(confirmed->summary,1024,L"Stable task");wcscpy_s(confirmed->status_id,80,L"done");wcscpy_s(confirmed->status_name,128,L"Done");wcscpy_s(confirmed->category,32,L"done");confirmed->assigned_to_me=TRUE;
    g_filter_masks[1]=1ull<<STATUS_PROGRESS;
    FinishJiraJob(request);assert(!IsWindow(popup)&&CountForTab(1)==0&&CountForTab(2)==1);
    assert(ListView_GetItemCount(g_list)==1); /* Keep the confirmed row long enough to show its reaction. */
    CompletionFeedback *effect=CompletionFeedbackFor(&g_tasks[0]);assert(effect&&effect->success);g_tasks[0].local_note=DuplicateWide(L"Visible note");
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;g_comments_mode=g_comments_visual_mode=2;g_comments_reveal=1000;SetWindowPos(g_window,NULL,0,0,D(900),D(600),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);RecreateBrushes();UpdateFonts();RefreshTaskList();
        RECT client,row,status;GetClientRect(g_list,&client);ListView_GetItemRect(g_list,0,&row,LVIR_BOUNDS);ListView_GetSubItemRect(g_list,0,STATUS_COLUMN,LVIR_BOUNDS,&status);
        HDC target=GetDC(g_list),dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
        NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=dc;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=0;effect->started=GetTickCount64()-400;DrawListCustom(&draw);
        COLORREF green=dark?RGB(29,65,46):RGB(225,246,234),ink=dark?RGB(162,236,187):RGB(30,112,67);
        assert(GetPixel(dc,client.right/2,row.top+1)==green);assert(GetPixel(dc,status.left+2,row.top+D(4))==green);
        int left=client.right,right=-1;for(int y=row.top;y<row.bottom-1;y++)for(int x=status.left;x<min(status.right,client.right);x++)if(GetPixel(dc,x,y)==ink){left=min(left,x);right=max(right,x);}
        assert(right>=left&&abs(left+right+1-status.left-min(status.right,client.right))<=D(3));
        RECT note;ListView_GetSubItemRect(g_list,0,COMMENTS_COLUMN,LVIR_BOUNDS,&note);BOOL note_ink=FALSE;
        for(int y=row.top;y<row.bottom-1;y++)for(int x=note.left+D(7);x<note.right-D(5);x++)if(GetPixel(dc,x,y)==MutedColor())note_ink=TRUE;
        if(!note_ink)fprintf(stderr,"note fixture dpi=%d density=%d theme=%d rect=%ld,%ld,%ld,%ld client=%ld mode=%d width=%d\n",dpi,density,dark,note.left,note.top,note.right,note.bottom,client.right,g_comments_mode,ListView_GetColumnWidth(g_list,COMMENTS_COLUMN));assert(note_ink);
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(g_list,target);
    }
    CompletionFeedbackTick(effect->started+851);assert(!CompletionFeedbackFor(&g_tasks[0])&&ListView_GetItemCount(g_list)==0&&wcsstr(g_completion_notice,L"A-1"));
    CompletionFeedbackTick(g_completion_notice_until+1);assert(!g_completion_notice_until);
    UpdateLastUpdated();GetWindowTextW(g_updated,caption,128);assert(!wcsstr(caption,L"A-1"));
    CompletionCollectClosed();FreeTaskItems();JiraFreeSnapshot(&g_snapshot);
    DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Completion UI regression passed");return 0;
}
