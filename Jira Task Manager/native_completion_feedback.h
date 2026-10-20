/* UI-only feedback. Statuses always come from the reloaded Jira snapshot. */
typedef struct CompletionFeedback {
    JiraSnapshot scope;wchar_t key[80],destination[80];UINT generation;
    ULONGLONG started;BOOL success,was_active;
} CompletionFeedback;
static CompletionFeedback g_completion_feedback[16];
static wchar_t g_completion_notice[240];static ULONGLONG g_completion_notice_until;
static CompletionFeedback *CompletionFeedbackFor(const TaskItem *task)
{
    for(int i=0;i<_countof(g_completion_feedback);i++){
        CompletionFeedback *effect=&g_completion_feedback[i];
        if(effect->generation&&!wcscmp(effect->key,task->key)&&NativeStatusSameScope(&effect->scope,&g_snapshot)&&
            (!effect->success||!wcscmp(effect->destination,g_status_ids[task->status])))return effect;
    }return NULL;
}
static void CompletionFeedbackBegin(UINT generation,const JiraSnapshot *scope,const wchar_t *key,const wchar_t *destination)
{
    CompletionFeedback *effect=NULL;ULONGLONG now=GetTickCount64();
    for(int i=0;i<_countof(g_completion_feedback);i++)if(!g_completion_feedback[i].generation||(g_completion_feedback[i].success&&now-g_completion_feedback[i].started>=850)){effect=&g_completion_feedback[i];break;}
    if(!effect)return;*effect=(CompletionFeedback){.generation=generation,.started=now};effect->scope.identity=scope->identity;
    wcscpy_s(effect->scope.server,800,scope->server);wcscpy_s(effect->key,80,key);wcscpy_s(effect->destination,80,destination);
    for(int i=0;i<g_task_count;i++)if(!wcscmp(g_tasks[i].key,key))effect->was_active=g_tasks[i].active;
    SetTimer(g_window,944,16,NULL);InvalidateRect(g_list,NULL,FALSE);
}
static void CompletionFeedbackPrepare(JiraJob *job)
{
    if(!job->completion_generation)return;
    for(int i=0;i<_countof(g_completion_feedback);i++){
        CompletionFeedback *effect=&g_completion_feedback[i];if(effect->generation!=job->completion_generation)continue;
        BOOL confirmed=FALSE;
        if(job->success&&job->loaded&&NativeStatusSameScope(&effect->scope,&g_snapshot)&&NativeStatusSameScope(&effect->scope,&job->snapshot))
            for(int j=0;j<job->snapshot.count;j++)if(!wcscmp(job->snapshot.items[j].key,effect->key)&&!wcscmp(job->snapshot.items[j].status_id,effect->destination)){confirmed=TRUE;break;}
        if(confirmed){effect->success=TRUE;effect->started=GetTickCount64();}else *effect=(CompletionFeedback){0};break;
    }
}
static void CompletionFeedbackDiscard(UINT generation)
{for(int i=0;i<_countof(g_completion_feedback);i++)if(g_completion_feedback[i].generation==generation)g_completion_feedback[i]=(CompletionFeedback){0};}
static void CompletionFeedbackTick(ULONGLONG now)
{
    BOOL alive=FALSE,refresh=FALSE;
    for(int i=0;i<_countof(g_completion_feedback);i++){
        CompletionFeedback *effect=&g_completion_feedback[i];if(!effect->generation)continue;
        if(!NativeStatusSameScope(&effect->scope,&g_snapshot)){*effect=(CompletionFeedback){0};refresh=TRUE;continue;}
        if(effect->success&&now-effect->started>=850){
            for(int j=0;j<g_task_count;j++)if(!wcscmp(g_tasks[j].key,effect->key)&&!wcscmp(g_status_ids[g_tasks[j].status],effect->destination)){
                swprintf(g_completion_notice,240,L"%ls → %ls",effect->key,StatusName(g_tasks[j].status));g_completion_notice_until=now+6000;break;
            }
            *effect=(CompletionFeedback){0};refresh=TRUE;
        }else alive=TRUE;
    }
    if(refresh)RefreshTaskList();
    if(IsWindowVisible(g_window)&&!IsIconic(g_window))InvalidateRect(g_list,NULL,FALSE);
    if(g_completion_notice_until&&now>=g_completion_notice_until){g_completion_notice_until=0;g_completion_notice[0]=0;}
    if(!alive){if(g_completion_notice_until)SetTimer(g_window,944,1000,NULL);else KillTimer(g_window,944);}
}
static void CompletionFadeRow(HDC target,RECT bounds,double amount)
{
    if(amount<=0)return;int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;if(width<=0||height<=0)return;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    DWORD *pixels=NULL;HBITMAP bitmap=CreateDIBSection(target,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);HDC dc=bitmap?CreateCompatibleDC(target):NULL;
    if(!dc){if(bitmap)DeleteObject(bitmap);return;}HGDIOBJ old=SelectObject(dc,bitmap);BitBlt(dc,0,0,width,height,target,bounds.left,bounds.top,SRCCOPY);GdiFlush();COLORREF base=PanelColor();
    for(int i=0;i<width*height;i++){DWORD p=pixels[i];int r=(int)(((p>>16)&255)*(1-amount)+GetRValue(base)*amount),g=(int)(((p>>8)&255)*(1-amount)+GetGValue(base)*amount),b=(int)((p&255)*(1-amount)+GetBValue(base)*amount);pixels[i]=(r<<16)|(g<<8)|b;}
    BitBlt(target,bounds.left,bounds.top,width,height,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);
}
