/* A confirmed start reaction owns keys, never pointers into replaceable Jira rows.
   This state cannot change a task's status or keep an otherwise filtered row. */
typedef struct ProgressFeedback {
    JiraSnapshot scope; /* Identity/server only; no owned items. */
    wchar_t key[80];wchar_t (*previous)[80];int previous_count;
    ULONGLONG started;
} ProgressFeedback;
static ProgressFeedback g_progress_feedback;
static BOOL CommentAnimationEnabled(void);
static COLORREF ProgressBlend(COLORREF base,COLORREF tint,double amount)
{
    return RGB((int)(GetRValue(base)*(1-amount)+GetRValue(tint)*amount),
        (int)(GetGValue(base)*(1-amount)+GetGValue(tint)*amount),
        (int)(GetBValue(base)*(1-amount)+GetBValue(tint)*amount));
}
static BOOL ProgressFeedbackValid(void)
{
    if(!g_progress_feedback.key[0]||!NativeStatusSameScope(&g_progress_feedback.scope,&g_snapshot))return FALSE;
    int target=TaskByKey(g_progress_feedback.key);return target>=0&&g_tasks[target].active;
}
static void ProgressFeedbackClear(void)
{
    free(g_progress_feedback.previous);g_progress_feedback=(ProgressFeedback){0};
    if(g_window)KillTimer(g_window,945);
}
static ProgressFeedback ProgressFeedbackPrepare(const JiraJob *job)
{
    ProgressFeedback effect={0};
    if((job->action!=2&&job->action!=3)||!job->success||!job->loaded||!CommentAnimationEnabled()||
        !NativeStatusSameScope(&g_snapshot,&job->snapshot)||!NativeStatusSameScope(&job->status_writes.scope,&g_snapshot))return effect;
    int target=TaskByKey(job->key);if(target<0||g_tasks[target].active)return effect;
    BOOL confirmed=FALSE;
    for(int i=0;i<job->snapshot.count;i++)if(!wcscmp(job->snapshot.items[i].key,job->key)){
        confirmed=job->snapshot.items[i].assigned_to_me&&!_wcsicmp(job->snapshot.items[i].status_name,L"In Progress");break;
    }
    if(!confirmed)return effect;
    int count=0;for(int i=0;i<g_task_count;i++)if(g_tasks[i].active)count++;
    if(count){effect.previous=calloc((size_t)count,sizeof(*effect.previous));if(!effect.previous)return effect;}
    for(int i=0;i<g_task_count;i++)if(g_tasks[i].active)wcscpy_s(effect.previous[effect.previous_count++],80,g_tasks[i].key);
    effect.scope.identity=g_snapshot.identity;wcscpy_s(effect.scope.server,800,g_snapshot.server);wcscpy_s(effect.key,80,job->key);
    return effect;
}
static void ProgressFeedbackCommit(ProgressFeedback *effect,BOOL installed)
{
    if(!effect->key[0])return;
    int target=TaskByKey(effect->key);
    if(!installed||!NativeStatusSameScope(&effect->scope,&g_snapshot)||target<0||!g_tasks[target].active){free(effect->previous);return;}
    ProgressFeedbackClear();g_progress_feedback=*effect;g_progress_feedback.started=GetTickCount64();
    SetTimer(g_window,945,16,NULL);InvalidateRect(g_list,NULL,FALSE);
}
static double ProgressFeedbackPhase(ULONGLONG now)
{
    double p=now>=g_progress_feedback.started?min(1.0,(double)(now-g_progress_feedback.started)/450.0):0;
    return p*p*(3-2*p);
}
/* 1 is the newly active target; -1 is a former active row now neutral. */
static int ProgressFeedbackRole(const TaskItem *task,ULONGLONG now)
{
    if(!ProgressFeedbackValid()||!CommentAnimationEnabled()||now-g_progress_feedback.started>=450)return 0;
    if(!wcscmp(task->key,g_progress_feedback.key))return 1;
    if(!task->active)for(int i=0;i<g_progress_feedback.previous_count;i++)
        if(!wcscmp(task->key,g_progress_feedback.previous[i]))return -1;
    return 0;
}
static void ProgressFeedbackTick(ULONGLONG now)
{
    if(!ProgressFeedbackValid()||!CommentAnimationEnabled()||now-g_progress_feedback.started>=450)ProgressFeedbackClear();
    if(g_list)InvalidateRect(g_list,NULL,FALSE);
}
static void ProgressFeedbackPaint(HDC dc,RECT bounds,double phase)
{
    int width=bounds.right-bounds.left;bounds.bottom--;
    RECT fill=bounds;fill.right=fill.left+(int)(width*phase);
    HBRUSH brush=CreateSolidBrush(ActiveColor());FillRect(dc,&fill,brush);DeleteObject(brush);
    COLORREF amber=g_dark?RGB(111,75,28):RGB(255,211,124);
    /* Soft amber trailing edge travels once across the row and settles to its
       ordinary active fill. Paint before text, indicators and the status pill. */
    for(int slice=0;slice<32;slice++){
        RECT band=fill;band.left=max(bounds.left,fill.right-width/2+width*slice/64);
        band.right=min(fill.right,max(bounds.left,fill.right-width/2+width*(slice+1)/64));
        if(band.right<=band.left)continue;
        double amount=sin(phase*3.141592653589793)*(slice+1)/32.0;
        brush=CreateSolidBrush(ProgressBlend(ActiveColor(),amber,amount));FillRect(dc,&band,brush);DeleteObject(brush);
    }
}
