#include <assert.h>
#include "native_jira.h"
static BOOL FixtureConnect(JiraConnection *client);
static ULONGLONG fixture_now=10000;
static BOOL fixture_animation=TRUE;
static void *fixture_job;
static BOOL progress_timer;
static UINT_PTR WINAPI FixtureSetTimer(HWND window,UINT_PTR id,UINT delay,TIMERPROC callback)
{if(id==945)progress_timer=TRUE;return SetTimer(window,id,delay,callback);}
static BOOL WINAPI FixtureKillTimer(HWND window,UINT_PTR id)
{if(id==945)progress_timer=FALSE;return KillTimer(window,id);}
static HANDLE WINAPI FixtureThread(LPSECURITY_ATTRIBUTES security,SIZE_T stack,LPTHREAD_START_ROUTINE entry,LPVOID parameter,DWORD flags,LPDWORD id)
{
    (void)security;(void)stack;(void)entry;(void)flags;(void)id;fixture_job=parameter;
    return CreateEventW(NULL,TRUE,FALSE,NULL);
}
static ULONGLONG WINAPI FixtureTick(void){return fixture_now;}
static BOOL WINAPI FixtureSystemParameters(UINT action,UINT parameter,PVOID value,UINT flags)
{
    if(action==SPI_GETCLIENTAREAANIMATION){*(BOOL*)value=fixture_animation;return TRUE;}
    return SystemParametersInfoW(action,parameter,value,flags);
}
static int WINAPI FixtureMessage(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags)
{(void)owner;(void)text;(void)title;(void)flags;return IDOK;}
#define JiraConnect FixtureConnect
#define CreateThread FixtureThread
#define GetTickCount64 FixtureTick
#define SystemParametersInfoW FixtureSystemParameters
#define MessageBoxW FixtureMessage
#define SetTimer FixtureSetTimer
#define KillTimer FixtureKillTimer
#include "native.c"

/* No credential/profile/HTTP access: the real worker and Jira model use this
   transport. The switch performs its real preflight, target POST and reload.
   This fixture server models Jira's automatic pause when another issue starts. */
static const char *old_status="In Progress",*target_status="To Do";
static BOOL fail_post,fail_reload,omit_target,no_status_change,scope_changed,reporter_only,delay_old_pause;
static int fixture_loads,fixture_posts,fixture_searches,fixture_status_reads;
static BOOL FixtureSend(JiraConnection *client,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    char json[4096];const char *response="{}";(void)body;
    if(fail_reload&&fixture_posts>0&&!wcscmp(method,L"GET")){wcscpy_s(client->error,256,L"Reload failed");return FALSE;}
    if(wcsstr(path,L"/myself")){
        fixture_loads++;
        response=scope_changed&&fixture_posts>0?"{\"displayName\":\"Other\",\"accountId\":\"other-id\",\"key\":\"other\"}":
            "{\"displayName\":\"Fixture\",\"accountId\":\"fixture-id\",\"key\":\"fixture\"}";
    }else if(wcsstr(path,L"?fields=status,assignee,reporter")){
        fixture_status_reads++;assert(!wcscmp(method,L"GET"));
        BOOL old=wcsstr(path,L"/issue/A-1?")!=NULL;
        assert(old||wcsstr(path,L"/issue/B-2?"));
        if(!old&&omit_target&&fixture_posts>0){wcscpy_s(client->error,256,L"Issue no longer accessible");return FALSE;}
        const char *status=old?old_status:target_status;
        snprintf(json,sizeof(json),"{\"key\":\"%s\",\"fields\":{\"status\":{\"id\":\"%s\",\"name\":\"%s\",\"statusCategory\":{\"key\":\"indeterminate\"}},\"assignee\":{\"accountId\":\"%s\"},\"reporter\":{\"accountId\":\"%s\"}}}",
            old?"A-1":"B-2",old?(!strcmp(status,"In Progress")?"progress":"hold"):"target",status,
            reporter_only&&!old&&fixture_posts>0?"other-id":"fixture-id",reporter_only&&!old?"fixture-id":"other-id");response=json;
    }else if(wcsstr(path,L"/search?")){
        fixture_searches++;
        BOOL index=wcsstr(path,L"fields=updated,assignee,reporter")!=NULL;
        BOOL detail=wcsstr(path,L"jql=id%20in%20(")!=NULL;
        BOOL legacy_reporter=!index&&!detail&&wcsstr(path,L"jql=reporter");
        const char *keys[]={"A-1","C-3","B-2"},*ids[]={"10","30","20"};
        const char *statuses[]={old_status,"To Do",target_status};
        const char *titles[]={"Previous active","Unaffected","Start this task"};
        char rows[3500]="";int count=0;
        for(int row=0;row<3;row++){
            if(row==2&&fixture_posts>0&&omit_target)continue;
            if(legacy_reporter&&!(reporter_only&&fixture_posts>0&&row==2))continue;
            if(detail){
                wchar_t id[8],first[32],middle[32],last[32],only[32];swprintf(id,8,L"%hs",ids[row]);
                swprintf(first,32,L"(%ls,",id);swprintf(middle,32,L",%%20%ls,",id);
                swprintf(last,32,L",%%20%ls)",id);swprintf(only,32,L"(%ls)",id);
                if(!wcsstr(path,first)&&!wcsstr(path,middle)&&!wcsstr(path,last)&&!wcsstr(path,only))continue;
            }
            const char *self=scope_changed&&fixture_posts>0?"other-id":"fixture-id";
            char item[1000];snprintf(item,sizeof(item),"%s{\"id\":\"%s\",\"key\":\"%s\",\"fields\":{\"updated\":\"%s\",\"assignee\":{\"accountId\":\"%s\"},\"reporter\":{\"accountId\":\"%s\"},\"summary\":\"%s\",\"status\":{\"id\":\"%s\",\"name\":\"%s\",\"statusCategory\":{\"key\":\"%s\"}}}}",
                count?",":"",ids[row],keys[row],statuses[row],reporter_only&&row==2&&fixture_posts>0?"not-self":self,
                reporter_only&&row==2?self:"not-self",titles[row],row==0?(!strcmp(old_status,"In Progress")?"progress":"hold"):row==1?"todo":"target",statuses[row],row==1?"new":"indeterminate");
            strcat_s(rows,sizeof(rows),item);count++;
        }
        snprintf(json,sizeof(json),"{\"startAt\":0,\"total\":%d,\"issues\":[%s]}",count,rows);response=json;
    }else if(wcsstr(path,L"/transitions")){
        if(!wcscmp(method,L"POST")){
            fixture_posts++;
            /* Model credential replacement explicitly; do not assume every readback repeats /myself. */
            if(scope_changed)client->credential_stamp++;
            if(fail_post){wcscpy_s(client->error,256,L"Start failed");return FALSE;}
            if(wcsstr(path,L"A-1")){
                old_status=body&&strstr(body,"hold")?"On Hold":"In Progress";
                if(!strcmp(old_status,"In Progress")&&!delay_old_pause)target_status="On Hold";
            }
            else if(!no_status_change){
                target_status=body&&strstr(body,"hold")?"On Hold":"In Progress";
                if(!strcmp(target_status,"In Progress")&&!delay_old_pause)old_status="On Hold";
            }
        }else response="{\"transitions\":[{\"id\":\"start\",\"name\":\"Start\",\"to\":{\"id\":\"target\",\"name\":\"In Progress\",\"statusCategory\":{\"key\":\"indeterminate\"}}},{\"id\":\"hold\",\"name\":\"Hold\",\"to\":{\"id\":\"hold\",\"name\":\"On Hold\",\"statusCategory\":{\"key\":\"indeterminate\"}}}]}";
    }else assert(!"Unexpected fixture request");
    *length=strlen(response);*out=malloc(*length+1);assert(*out);memcpy(*out,response,*length+1);return TRUE;
}
static BOOL FixtureConnect(JiraConnection *client)
{memset(client,0,sizeof(*client));wcscpy_s(client->host,256,L"jira.example.test");client->port=443;client->send=FixtureSend;return TRUE;}
static void CompleteWorker(void)
{
    JiraWorker(fixture_job);fixture_job=NULL;MSG result;
    assert(PeekMessageW(&result,g_window,WM_JIRA_RESULT,WM_JIRA_RESULT,PM_REMOVE));FinishJiraJob((JiraJob*)result.lParam);
}
static int RowForKey(const wchar_t *key)
{for(int row=0;row<ListView_GetItemCount(g_list);row++)if(!wcscmp(g_tasks[TaskIndexAtRow(row)].key,key))return row;return -1;}
static COLORREF RowPixel(const wchar_t *key,int fraction)
{
    int row=RowForKey(key);assert(row>=0);RECT client,bounds;GetClientRect(g_list,&client);ListView_GetItemRect(g_list,row,&bounds,LVIR_BOUNDS);
    HDC target=GetDC(g_list),dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
    NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=dc;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;DrawListCustom(&draw);
    COLORREF result=GetPixel(dc,client.right*fraction/100,bounds.top+2);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(g_list,target);return result;
}
static void Advance(UINT milliseconds)
{fixture_now+=milliseconds;WindowProcedure(g_window,WM_TIMER,945,0);}
static void Seed(void)
{
    Advance(1000);assert(!progress_timer);
    fail_post=fail_reload=omit_target=no_status_change=scope_changed=reporter_only=delay_old_pause=FALSE;
    old_status="In Progress";target_status="To Do";fixture_loads=0;g_tab=0;g_reported_scope=FALSE;
    g_sort_column=-1;g_hover_row=-1;g_list_keyboard_focus=FALSE;fixture_animation=TRUE;g_dark=FALSE;
    assert(StartJiraJob(0,NULL,NULL,(POINT){0}));CompleteWorker();fixture_loads=fixture_posts=fixture_searches=fixture_status_reads=0;
    assert(!progress_timer&&RowPixel(L"B-2",50)==PanelColor());
}
static void StartAndFinish(int action)
{assert(StartJiraJob(action,L"B-2",action==2?L"start":NULL,(POINT){0}));CompleteWorker();}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_notifications.tasks=g_notifications.comments=g_notifications.statuses=FALSE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"prg",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    g_tab=0;assert(StartJiraJob(0,NULL,NULL,(POINT){0}));CompleteWorker();
    assert(RowPixel(L"A-1",50)==ActiveColor()&&RowPixel(L"B-2",50)==PanelColor());
    assert(StartJiraJob(3,L"B-2",NULL,(POINT){0}));
    assert(RowPixel(L"B-2",50)==RGB(225,240,253)); /* Pending remains the ordinary hourglass treatment. */
    CompleteWorker();assert(g_tasks[TaskByKey(L"B-2")].active&&!g_tasks[TaskByKey(L"A-1")].active);
    fixture_now+=225;
    COLORREF warm=RowPixel(L"B-2",25),previous=RowPixel(L"A-1",50);
    assert(warm!=ActiveColor()&&warm!=PanelColor()&&"Confirmed start must paint the amber row wave");
    assert(previous!=ActiveColor()&&previous!=PanelColor()&&"Previous active row must fade in sync");
    assert(RowPixel(L"C-3",50)==PanelColor());
    assert(RowPixel(L"B-2",75)==PanelColor()); /* The wave has not reached the far edge. */
    assert(progress_timer);
    /* Snapshot replacement + active-first sort move the target's storage index.
       Changing list sort must still paint the effect on its issue key. */
    g_sort_column=1;g_sort_ascending=FALSE;RefreshTaskList();
    assert(RowPixel(L"B-2",25)==warm&&RowPixel(L"A-1",50)==previous&&RowPixel(L"C-3",50)==PanelColor());
    assert(StartJiraJob(0,NULL,NULL,(POINT){0}));CompleteWorker();
    assert(RowPixel(L"B-2",25)==warm&&RowPixel(L"A-1",50)==previous); /* Replaced task storage retains keys and original timing. */
    Advance(224);assert(progress_timer);Advance(1);assert(!progress_timer);
    assert(RowPixel(L"B-2",25)==ActiveColor()&&RowPixel(L"A-1",50)==PanelColor());
    /* Same active snapshot, hover, tabs and refresh never start another wave. */
    RefreshTaskList();g_hover_row=RowForKey(L"C-3");g_tab=1;RefreshTaskList();g_tab=0;g_hover_row=-1;RefreshTaskList();
    assert(!progress_timer&&RowPixel(L"B-2",25)==ActiveColor());
    assert(StartJiraJob(0,NULL,NULL,(POINT){0}));CompleteWorker();assert(!progress_timer);
    Seed();StartAndFinish(2);assert(progress_timer);Advance(225);
    assert(RowPixel(L"B-2",25)!=ActiveColor()); /* Explicit status-menu start also reacts. */
    Advance(225);fixture_loads=0;StartAndFinish(2);assert(!progress_timer); /* Already active no-op. */
    fixture_loads=0;StartAndFinish(3);assert(!progress_timer&&!g_tasks[TaskByKey(L"B-2")].active); /* Pause. */
    Seed();delay_old_pause=TRUE;StartAndFinish(3);
    assert(fixture_posts==1&&g_tasks[TaskByKey(L"B-2")].active&&g_tasks[TaskByKey(L"A-1")].active);
    assert(fixture_searches==0&&fixture_status_reads==3); /* Selected preflight + selected/old readback only. */
    Advance(450);assert(RowPixel(L"A-1",50)==ActiveColor()); /* Never invent a Hold while server automation is pending. */
    old_status="On Hold"; /* Server rule finishes later; automatic targeted read must discover it. */
    fixture_now+=1000;WindowProcedure(g_window,WM_TIMER,947,0);
    assert(fixture_job&&"Pending Jira automation must schedule a targeted read, without manual Refresh");CompleteWorker();
    assert(fixture_posts==1&&!g_tasks[TaskByKey(L"A-1")].active&&g_tasks[TaskByKey(L"B-2")].active);
    assert(fixture_searches==0&&fixture_status_reads==4&&!g_switch_sync.count); /* Only the still-pending old issue was reread. */
    Seed();fail_post=TRUE;StartAndFinish(3);assert(!progress_timer&&!g_tasks[TaskByKey(L"B-2")].active);
    Seed();fail_reload=TRUE;StartAndFinish(3);assert(!progress_timer&&g_jira_stale);
    Seed();omit_target=TRUE;StartAndFinish(3);assert(!progress_timer&&TaskByKey(L"B-2")>=0&&g_jira_stale); /* Failed sparse reads never remove cached issues. */
    Seed();no_status_change=TRUE;StartAndFinish(3);assert(!progress_timer&&!g_tasks[TaskByKey(L"B-2")].active);
    Seed();reporter_only=TRUE;StartAndFinish(3);assert(!progress_timer&&!g_tasks[TaskByKey(L"B-2")].active);
    Seed();scope_changed=TRUE;StartAndFinish(3);assert(!progress_timer); /* Account switches cannot inherit feedback. */
    Seed();assert(StartJiraJob(3,L"B-2",NULL,(POINT){0}));
    wcscpy_s(((JiraJob*)fixture_job)->status_writes.scope.identity.account_id,256,L"previous-account");
    CompleteWorker();assert(!progress_timer&&"A result from another request scope must not inherit the current list");
    Seed();fixture_animation=FALSE;StartAndFinish(3);
    assert(!progress_timer&&RowPixel(L"B-2",25)==ActiveColor());
    Seed();StartAndFinish(3);fixture_animation=FALSE;Advance(1);assert(!progress_timer&&RowPixel(L"B-2",25)==ActiveColor());
    Seed();StartAndFinish(3);wcscpy_s(g_snapshot.server,800,L"other.example.test:443");Advance(1);assert(!progress_timer);
    Seed();StartAndFinish(3);g_tasks[TaskByKey(L"B-2")].active=FALSE;Advance(1);assert(!progress_timer);
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        Seed();g_dpi=dpi;g_density=density;g_dark=dark;
        SetWindowPos(g_window,NULL,0,0,D(900),D(600),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);RecreateBrushes();UpdateFonts();RefreshTaskList();
        StartAndFinish(3);RECT before,after;assert(StatusPillRectForRow(RowForKey(L"B-2"),&before));
        Advance(225);assert(StatusPillRectForRow(RowForKey(L"B-2"),&after));assert(EqualRect(&before,&after));
        warm=RowPixel(L"B-2",25);assert(warm!=ActiveColor()&&GetRValue(warm)>=GetGValue(warm)&&GetGValue(warm)>GetBValue(warm));
        assert(RowPixel(L"C-3",50)==PanelColor());Advance(225);
        assert(!progress_timer&&RowPixel(L"B-2",25)==ActiveColor()&&RowPixel(L"A-1",50)==PanelColor());
    }
    Seed();StartAndFinish(3);assert(progress_timer);
    WindowProcedure(g_window,WM_DESTROY,0,0);
    assert(!progress_timer&&!g_progress_feedback.previous&&"Closing must stop the timer and release retained issue keys");
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Progress feedback regression passed");return 0;
}
