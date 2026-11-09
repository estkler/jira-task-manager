#define main ProgressFeedbackTestsUnused
#include "progress_feedback_test.c"
#undef main
static void PollStatuses(void)
{
    fixture_now=g_switch_sync.due;WindowProcedure(g_window,WM_TIMER,947,0);
    assert(fixture_job&&((JiraJob*)fixture_job)->action==6);CompleteWorker();
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_notifications.tasks=g_notifications.comments=g_notifications.statuses=FALSE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"ssy",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    assert(fixture_searches==0&&fixture_posts==1&&fixture_status_reads==3&&g_switch_sync.count==1);
    assert(!wcscmp(g_switch_sync.keys[0],L"A-1"));
    int index=TaskByKey(L"C-3");TaskItem unchanged=g_tasks[index];JiraItem source_unchanged={0};
    for(int i=0;i<g_snapshot.count;i++)if(!wcscmp(g_snapshot.items[i].key,L"C-3"))source_unchanged=g_snapshot.items[i];
    PollStatuses();assert(g_switch_sync.count==1&&g_tasks[TaskByKey(L"A-1")].active);
    old_status="On Hold";PollStatuses();assert(!g_switch_sync.count&&!g_tasks[TaskByKey(L"A-1")].active);
    assert(g_tasks[TaskByKey(L"B-2")].active&&fixture_posts==1&&fixture_searches==0&&fixture_status_reads==5);
    assert(!memcmp(&unchanged,&g_tasks[TaskByKey(L"C-3")],sizeof(unchanged)));
    for(int i=0;i<g_snapshot.count;i++)if(!wcscmp(g_snapshot.items[i].key,L"C-3"))assert(!memcmp(&source_unchanged,&g_snapshot.items[i],sizeof(source_unchanged)));
    WindowProcedure(g_window,WM_TIMER,947,0);assert(!fixture_job);

    Seed();fail_post=TRUE;StartAndFinish(3);
    assert(fixture_posts==1&&!g_switch_sync.count&&!g_tasks[TaskByKey(L"B-2")].active);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);old_status="On Hold"; /* Jira is ahead of the displayed two-active snapshot. */
    assert(StartJiraJob(3,L"A-1",NULL,(POINT){0}));CompleteWorker();
    assert(g_switch_sync.count==1&&!wcscmp(g_switch_sync.keys[0],L"B-2"));
    target_status="On Hold";PollStatuses();
    assert(g_tasks[TaskByKey(L"A-1")].active&&!g_tasks[TaskByKey(L"B-2")].active&&fixture_posts==2&&fixture_searches==0);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);fail_post=TRUE;
    assert(StartJiraJob(3,L"A-1",NULL,(POINT){0}));CompleteWorker();
    assert(g_switch_sync.count==1&&!wcscmp(g_switch_sync.keys[0],L"A-1")); /* A failed second action must not cancel the first rule's readback. */
    old_status="On Hold";fail_post=FALSE;PollStatuses();assert(!g_switch_sync.count&&fixture_posts==2);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    for(int attempt=0;attempt<4;attempt++)PollStatuses();
    assert(!g_switch_sync.count&&g_status_sync_warning&&fixture_posts==1&&fixture_searches==0&&fixture_status_reads==7);
    assert(g_tasks[TaskByKey(L"A-1")].active&&g_tasks[TaskByKey(L"B-2")].active); /* Timeout must not invent Hold. */
    WindowProcedure(g_window,WM_TIMER,947,0);assert(!fixture_job);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);fail_reload=TRUE;
    for(int attempt=0;attempt<4;attempt++)PollStatuses();
    assert(!g_switch_sync.count&&g_status_sync_warning&&fixture_posts==1&&g_tasks[TaskByKey(L"A-1")].active);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    fixture_now=g_switch_sync.expires;WindowProcedure(g_window,WM_TIMER,947,0);
    assert(!fixture_job&&!g_switch_sync.count&&g_status_sync_warning&&fixture_posts==1);

    Seed();delay_old_pause=TRUE;StartAndFinish(3);old_status="On Hold";
    fixture_now=g_switch_sync.due;WindowProcedure(g_window,WM_TIMER,947,0);assert(fixture_job);
    fixture_now=g_switch_sync.expires+1;CompleteWorker();
    assert(g_tasks[TaskByKey(L"A-1")].active&&!g_switch_sync.count&&g_status_sync_warning); /* An expired read is not a fresh result. */

    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    fixture_now=g_switch_sync.due;WindowProcedure(g_window,WM_TIMER,947,0);assert(fixture_job);
    SwitchSyncClear();old_status="On Hold";CompleteWorker();
    assert(g_tasks[TaskByKey(L"A-1")].active&&!g_switch_sync.count); /* Superseded response cannot patch the UI. */

    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    wcscpy_s(g_snapshot.server,800,L"different.example.test:443");WindowProcedure(g_window,WM_TIMER,947,0);
    assert(!fixture_job&&!g_switch_sync.count&&fixture_posts==1);

    Seed();StartAndFinish(3);fixture_loads=0;StartAndFinish(3);
    assert(!g_tasks[TaskByKey(L"B-2")].active&&!g_switch_sync.count&&fixture_posts==2&&fixture_searches==0);
    WindowProcedure(g_window,WM_DESTROY,0,0);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    assert(!g_switch_sync.keys&&!g_switch_sync.count);
    puts("Switch sync: selected/old issue GETs only, unresolved-key pruning, automatic delayed Hold, unchanged unrelated rows, timeout/failure/generation/scope guards, explicit pause and shutdown passed");return 0;
}
