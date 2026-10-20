/* Keep the real snapshot/UI path; replace only Windows' external notification sink. */
#define main comment_alert_regression_main
#include "comment_alert_test.c"
#undef main
static const wchar_t *status_comment_id=L"";
static BOOL status_new_task;
static void statuses(const wchar_t *first_id,const wchar_t *first_name,const wchar_t *second_id,BOOL reverse)
{
    JiraSnapshot next={0};next.count=status_new_task?3:2;next.items=calloc((size_t)next.count,sizeof(*next.items));assert(next.items);
    wcscpy_s(next.server,800,L"https://jira.example.test");wcscpy_s(next.identity.key,256,L"self-1");wcscpy_s(next.account,256,L"Tester");
    JiraItem *one=&next.items[reverse?1:0],*two=&next.items[reverse?0:1];
    wcscpy_s(one->key,80,L"PDES-6487");wcscpy_s(one->summary,1024,L"External status change");
    wcscpy_s(one->status_id,80,first_id);wcscpy_s(one->status_name,128,first_name);wcscpy_s(one->category,32,L"indeterminate");one->assigned_to_me=TRUE;
    if(!wcscmp(first_id,L"done"))wcscpy_s(one->category,32,L"done");
    wcscpy_s(one->latest_comment_id,80,status_comment_id);wcscpy_s(one->latest_comment,2048,L"An external comment");
    wcscpy_s(one->latest_comment_identity.key,256,L"colleague");
    wcscpy_s(two->key,80,L"PDES-6527");wcscpy_s(two->summary,1024,L"Other issue");
    wcscpy_s(two->status_id,80,second_id);wcscpy_s(two->status_name,128,L"In Progress");wcscpy_s(two->category,32,L"indeterminate");two->assigned_to_me=TRUE;
    if(status_new_task){JiraItem *three=&next.items[2];wcscpy_s(three->key,80,L"PDES-9999");wcscpy_s(three->summary,1024,L"New task");wcscpy_s(three->status_id,80,L"todo");wcscpy_s(three->status_name,128,L"To Do");wcscpy_s(three->category,32,L"new");three->assigned_to_me=TRUE;}
    assert(InstallSnapshot(&next));
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_tab=1;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    wchar_t temporary[MAX_PATH];assert(GetTempPathW(MAX_PATH,temporary));assert(GetTempFileNameW(temporary,L"sta",0,g_settings_path));
    statuses(L"review",L"In Review",L"progress",FALSE);assert(notices==0);
    active_fixture=TRUE;statuses(L"upload",L"Upload project",L"progress",TRUE);
    assert(notices==1); /* Missing status notification OR foreground suppression must fail here. */
    assert(wcsstr(last_notice.szInfo,L"PDES-6487")&&wcsstr(last_notice.szInfo,L"In Review")&&wcsstr(last_notice.szInfo,L"Upload project"));
    statuses(L"upload",L"Upload project",L"progress",FALSE);assert(notices==1);
    statuses(L"upload",L"Renamed status",L"progress",TRUE);assert(notices==1); /* Names are not status identity. */
    active_fixture=FALSE;statuses(L"done",L"Done",L"hold",FALSE);assert(notices==2);
    assert(wcsstr(last_notice.szInfo,L"PDES-6487")&&wcsstr(last_notice.szInfo,L"1")); /* One popup, including the other change. */
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);statuses(L"done",L"Done",L"hold",TRUE);assert(notices==2); /* Restart baseline. */
    wcscpy_s(g_snapshot.identity.key,256,L"different-account");statuses(L"upload",L"Upload project",L"progress",FALSE);assert(notices==2);
    wcscpy_s(g_snapshot.server,800,L"https://other.example.test");statuses(L"review",L"In Review",L"progress",FALSE);assert(notices==2);
    g_notifications.statuses=FALSE;statuses(L"upload",L"Upload project",L"progress",FALSE);assert(notices==2);
    g_notifications.statuses=TRUE;statuses(L"upload",L"Upload project",L"progress",TRUE);assert(notices==2);
    /* A local POST must suppress only its actual destination, not an unrelated colleague's change. */
    JiraJob *job=calloc(1,sizeof(*job));assert(job);job->action=2;
    job->status_writes.scope=g_snapshot;job->status_writes.scope.items=NULL;job->status_writes.scope.count=0;
    assert(NativeStatusRemember(&job->status_writes,L"PDES-6487",L"review",L"In Review"));
    /* Simulate a failed reload: Finish retains this journal until the next successful snapshot. */
    job->success=TRUE;FinishJiraJob(job);
    statuses(L"review",L"In Review",L"hold",FALSE);assert(notices==3);
    assert(wcsstr(last_notice.szInfo,L"PDES-6527")&&!wcsstr(last_notice.szInfo,L"PDES-6487"));
    statuses(L"upload",L"Upload project",L"hold",FALSE);assert(notices==4); /* Journal consumed, later external edit is visible. */
    assert(NativeStatusRemember(&g_status_writes,L"PDES-6487",L"review",L"In Review"));
    g_status_writes.scope=g_snapshot;g_status_writes.scope.items=NULL;g_status_writes.scope.count=0;
    statuses(L"done",L"Done",L"hold",FALSE);assert(notices==5); /* Not the local operation's destination. */
    status_comment_id=L"900";status_new_task=TRUE;statuses(L"review",L"In Review",L"hold",FALSE);assert(notices==6);
    assert(wcsstr(last_notice.szInfo,L"Новых задач: 1")&&wcsstr(last_notice.szInfo,L"С комментариями: 1")&&wcsstr(last_notice.szInfo,L"Смен статуса: 1"));
    active_fixture=TRUE;status_comment_id=L"901";statuses(L"upload",L"Upload project",L"hold",TRUE);assert(notices==7);
    assert(wcsstr(last_notice.szInfo,L"PDES-6487")&&!wcsstr(last_notice.szInfo,L"С комментариями"));
    statuses(L"upload",L"Upload project",L"hold",FALSE);assert(notices==7);
    assert(NativeStatusRemember(&g_status_writes,L"PDES-6487",L"",L"In Review"));
    g_status_writes.scope=g_snapshot;g_status_writes.scope.items=NULL;g_status_writes.scope.count=0;
    statuses(L"different-review",L"In Review",L"hold",FALSE);assert(notices==8); /* Unknown IDs cannot suppress by name. */
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Status alerts: baseline, foreground, same-category changes, stable IDs, reorder, aggregation and account/server safety passed");return 0;
}
