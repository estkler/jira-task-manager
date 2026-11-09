/* UI-owned, bounded reconciliation of explicitly involved issue keys only.
   Worker jobs own copies; no queued read can patch a different account/switch. */
typedef struct SwitchSync {
    JiraSnapshot scope;wchar_t target[80];wchar_t (*keys)[80];int count,attempt;
    UINT generation;ULONGLONG due,expires;
} SwitchSync;
static SwitchSync g_switch_sync;
static void SwitchSyncClear(void)
{
    UINT generation=g_switch_sync.generation+1;
    free(g_switch_sync.keys);g_switch_sync=(SwitchSync){.generation=generation};
    if(g_window)KillTimer(g_window,947);
}
static BOOL SwitchSyncCapture(JiraJob *job)
{
    if(job->action!=2&&job->action!=3&&job->action!=6)return TRUE;
    if(job->action==6){
        if(!g_switch_sync.count||!NativeStatusSameScope(&g_switch_sync.scope,&g_snapshot))return FALSE;
        job->status_key_count=g_switch_sync.count;job->status_generation=g_switch_sync.generation;
        job->status_deadline=g_switch_sync.expires;
        job->status_keys=calloc((size_t)job->status_key_count,sizeof(*job->status_keys));
        if(!job->status_keys)return FALSE;
        memcpy(job->status_keys,g_switch_sync.keys,(size_t)job->status_key_count*sizeof(*job->status_keys));return TRUE;
    }
    int target=TaskByKey(job->key);if(target<0)return FALSE;
    int count=1;for(int i=0;i<g_task_count;i++)if(i!=target&&g_tasks[i].active)count++;
    job->status_keys=calloc((size_t)count,sizeof(*job->status_keys));if(!job->status_keys)return FALSE;
    wcscpy_s(job->status_keys[job->status_key_count++],80,job->key);
    for(int i=0;i<g_task_count;i++)if(i!=target&&g_tasks[i].active)
        wcscpy_s(job->status_keys[job->status_key_count++],80,g_tasks[i].key);
    job->status_only=job->action==3;return TRUE;
}
static BOOL InstallIssueStatuses(const JiraSnapshot *patch,const wchar_t *preferred)
{
    if(!NativeStatusSameScope(&g_snapshot,patch))return FALSE;
    int *mapped=calloc((size_t)(patch->count+1),sizeof(*mapped));if(!mapped)return FALSE;
    int count=g_status_count;wchar_t ids[64][80],names[64][128];int categories[64];
    memcpy(ids,g_status_ids,sizeof(ids));memcpy(names,g_status_names,sizeof(names));memcpy(categories,g_status_categories,sizeof(categories));
    for(int i=0;i<patch->count;i++){
        const JiraItem *source=&patch->items[i];int status=-1;
        for(int j=0;j<count;j++)if(!wcscmp(ids[j],source->status_id)){status=j;break;}
        if(status<0){if(count==64){free(mapped);return FALSE;}status=count++;wcscpy_s(ids[status],80,source->status_id);}
        wcscpy_s(names[status],128,source->status_name);
        categories[status]=!wcscmp(source->category,L"done")?2:!wcscmp(source->category,L"indeterminate")?1:0;mapped[i]=status;
    }
    g_status_count=count;memcpy(g_status_ids,ids,sizeof(ids));memcpy(g_status_names,names,sizeof(names));memcpy(g_status_categories,categories,sizeof(categories));
    for(int i=0;i<patch->count;i++){
        const JiraItem *source=&patch->items[i];int index=TaskByKey(source->key);if(index<0)continue;
        TaskItem *task=&g_tasks[index];task->status=(TaskStatus)mapped[i];
        task->assigned_to_me=source->assigned_to_me;task->reported_by_me=source->reported_by_me;
        task->active=source->assigned_to_me&&!_wcsicmp(source->status_name,L"In Progress");
        for(int j=0;j<g_snapshot.count;j++)if(!_wcsicmp(g_snapshot.items[j].key,source->key)){
            JiraItem *item=&g_snapshot.items[j];item->updated[0]=0;wcscpy_s(item->status_id,80,source->status_id);wcscpy_s(item->status_name,128,source->status_name);
            wcscpy_s(item->category,32,source->category);item->assigned_to_me=source->assigned_to_me;item->reported_by_me=source->reported_by_me;break;
        }
    }
    free(mapped);
    /* Newly selected active issue leads even while Jira is pausing the old one. */
    for(int i=1;i<g_task_count;i++)if(g_tasks[i].active){
        TaskItem value=g_tasks[i];int j=i;
        while(j>0&&(!g_tasks[j-1].active||(preferred&&!wcscmp(value.key,preferred)&&wcscmp(g_tasks[j-1].key,preferred)))){g_tasks[j]=g_tasks[j-1];j--;}
        g_tasks[j]=value;
    }
    /* Keep only unobserved own writes, so a later full refresh still recognizes
       an ambiguous POST, without suppressing unrelated future changes. */
    if(NativeStatusSameScope(&g_status_writes.scope,&g_snapshot))for(int i=g_status_writes.count-1;i>=0;i--){
        NativeStatusWrite *write=&g_status_writes.items[i];BOOL observed=FALSE;
        for(int j=0;j<patch->count;j++)if(!wcscmp(write->key,patch->items[j].key)&&!wcscmp(write->status_id,patch->items[j].status_id))observed=TRUE;
        if(observed){memmove(write,write+1,(size_t)(g_status_writes.count-i-1)*sizeof(*write));g_status_writes.count--;}
    }
    g_flash_task=-1;g_status_pulse_task=-1;KillTimer(g_list,2);KillTimer(g_list,4);
    BOOL preserve=g_preserve_popover_refresh;g_preserve_popover_refresh=TRUE;RefreshTaskList();g_preserve_popover_refresh=preserve;
    UpdateTooltips();return TRUE;
}
static void SwitchSyncResolve(void)
{
    if(!g_switch_sync.count)return;
    if(!NativeStatusSameScope(&g_switch_sync.scope,&g_snapshot)){SwitchSyncClear();return;}
    for(int i=g_switch_sync.count-1;i>=0;i--){
        int task=TaskByKey(g_switch_sync.keys[i]);BOOL target=!wcscmp(g_switch_sync.keys[i],g_switch_sync.target);
        if(task<0||(target?g_tasks[task].active:!g_tasks[task].active)){
            memmove(g_switch_sync.keys[i],g_switch_sync.keys[i+1],(size_t)(g_switch_sync.count-i-1)*sizeof(*g_switch_sync.keys));g_switch_sync.count--;
        }
    }
    if(!g_switch_sync.count)SwitchSyncClear();
}
static void SwitchSyncArm(const JiraJob *job)
{
    if(!job->loaded||!job->status_start||!NativeStatusSameScope(&job->snapshot,&g_snapshot))return;
    int target=TaskByKey(job->key);
    if(!job->success&&(target<0||!g_tasks[target].active))return; /* Failed unconfirmed start is not an automation trigger. */
    SwitchSyncClear();g_switch_sync.keys=calloc((size_t)job->status_key_count,sizeof(*g_switch_sync.keys));
    if(!g_switch_sync.keys){g_status_sync_warning=TRUE;wcscpy_s(g_jira_message,256,L"Could not track Jira status synchronization.");return;}
    memcpy(g_switch_sync.keys,job->status_keys,(size_t)job->status_key_count*sizeof(*g_switch_sync.keys));g_switch_sync.count=job->status_key_count;
    g_switch_sync.scope.identity=g_snapshot.identity;wcscpy_s(g_switch_sync.scope.server,800,g_snapshot.server);wcscpy_s(g_switch_sync.target,80,job->key);
    ULONGLONG now=GetTickCount64();g_switch_sync.due=now+750;g_switch_sync.expires=now+15000;
    SwitchSyncResolve();if(g_switch_sync.count)SetTimer(g_window,947,150,NULL);
}
static void SwitchSyncWarn(void)
{
    SwitchSyncClear();g_status_sync_warning=TRUE;
    wcscpy_s(g_jira_message,256,g_russian?L"Jira пока не подтвердила статусы переключённых задач. Показаны последние полученные значения.":
        L"Jira has not confirmed the switched issues' statuses yet. The last received values are displayed.");UpdateLastUpdated();
}
static void SwitchSyncTick(ULONGLONG now)
{
    SwitchSyncResolve();if(!g_switch_sync.count)return;
    if(now>=g_switch_sync.expires||g_switch_sync.attempt>=4){if(!g_jira_busy)SwitchSyncWarn();return;}
    if(g_jira_busy||g_update_handoff||now<g_switch_sync.due)return;
    if(StartJiraJob(6,NULL,NULL,(POINT){0}))g_switch_sync.attempt++;
    else SwitchSyncWarn();
}
static void SwitchSyncFinished(JiraJob *job)
{
    if(job->status_generation!=g_switch_sync.generation||!g_switch_sync.count)return;
    if(!NativeStatusSameScope(&job->status_writes.scope,&g_snapshot)){SwitchSyncClear();return;}
    if(GetTickCount64()>=g_switch_sync.expires){SwitchSyncWarn();return;}
    if(job->loaded){
        if(!InstallIssueStatuses(&job->snapshot,NULL)){SwitchSyncWarn();return;}
        SwitchSyncResolve();
    }
    if(!g_switch_sync.count)return;
    static const UINT pauses[]={750,1500,3000,5000};
    g_switch_sync.due=GetTickCount64()+pauses[min(g_switch_sync.attempt,3)];
    if(g_switch_sync.attempt>=4)SwitchSyncWarn();
}
