#include <assert.h>
#include "native_jira.h"
static BOOL FixtureSwitchConnect(JiraConnection *client);
static int WINAPI CaptureSwitchDialog(HWND,LPCWSTR,LPCWSTR,UINT);
#define JiraConnect FixtureSwitchConnect
#define MessageBoxW CaptureSwitchDialog
#include "native.c"
#undef JiraConnect
#undef MessageBoxW
static const char *fixture_stage;
static BOOL fail_transitions,fail_reload;
static int dialogs,posts,loads;
static UINT dialog_flags;
static wchar_t dialog_text[1024],dialog_title[128];
static int WINAPI CaptureSwitchDialog(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags)
{(void)owner;dialogs++;wcscpy_s(dialog_text,1024,text);wcscpy_s(dialog_title,128,title);dialog_flags=flags;return IDOK;}
static BOOL SwitchTransport(JiraConnection *client,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    (void)body;assert(wcscmp(method,L"POST"));if(!wcscmp(method,L"POST"))posts++;
    char json[1600];const char *response="{}";
    if(wcsstr(path,L"/myself")){
        loads++;if(fail_reload&&loads>1){wcscpy_s(client->error,256,L"Fixture reload unavailable");return FALSE;}
        response="{\"displayName\":\"Tester\",\"key\":\"self\"}";
    }else if(wcsstr(path,L"/search?")){
        snprintf(json,sizeof(json),"{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"PDES-6487\",\"fields\":{\"summary\":\"Managed workflow\",\"status\":{\"id\":\"stage\",\"name\":\"%s\",\"statusCategory\":{\"key\":\"indeterminate\"}}}}]}",fixture_stage);response=json;
    }else if(wcsstr(path,L"/transitions")){
        if(fail_transitions){wcscpy_s(client->error,256,L"Fixture network unavailable");return FALSE;}response="{\"transitions\":[]}";
    }else assert(0);
    *length=strlen(response);*out=malloc(*length+1);assert(*out);memcpy(*out,response,*length+1);return TRUE;
}
static BOOL FixtureSwitchConnect(JiraConnection *client)
{memset(client,0,sizeof(*client));wcscpy_s(client->host,256,L"jira.example.test");client->port=443;client->send=SwitchTransport;return TRUE;}
static void switch_fixture(const char *stage)
{
    fixture_stage=stage;dialogs=posts=loads=0;JiraJob *job=calloc(1,sizeof(*job));assert(job);job->action=3;wcscpy_s(job->key,80,L"PDES-6487");
    JiraWorker(job);MSG result;assert(PeekMessageW(&result,g_window,WM_JIRA_RESULT,WM_JIRA_RESULT,PM_REMOVE));FinishJiraJob((JiraJob*)result.lParam);
    assert(dialogs==1&&posts==0);
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_notifications.tasks=g_notifications.comments=g_notifications.statuses=FALSE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"swu",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    const char *stages[]={"In Review","Review","Upload project","Done"};
    for(int i=0;i<_countof(stages);i++){
        switch_fixture(stages[i]);assert((dialog_flags&MB_ICONMASK)==MB_ICONINFORMATION);
        assert(!wcscmp(dialog_title,L"Невозможно вернуть задачу в работу"));
        assert(wcsstr(dialog_text,L"PDES-6487")&&wcsstr(dialog_text,L"менеджер"));
        assert(!wcsstr(dialog_text,L"не изменены")&&!wcsstr(dialog_text,L"Nothing changed"));
    }
    g_russian=FALSE;switch_fixture("Upload project");assert((dialog_flags&MB_ICONMASK)==MB_ICONINFORMATION&&wcsstr(dialog_text,L"manager"));
    g_russian=TRUE;switch_fixture("To Do");assert((dialog_flags&MB_ICONMASK)==MB_ICONINFORMATION&&!wcsstr(dialog_text,L"менеджер"));
    switch_fixture("In Progress");assert((dialog_flags&MB_ICONMASK)==MB_ICONINFORMATION&&wcsstr(dialog_text,L"On Hold"));
    fail_transitions=TRUE;switch_fixture("Upload project");assert((dialog_flags&MB_ICONMASK)==MB_ICONWARNING&&wcsstr(dialog_text,L"Fixture network"));
    fail_transitions=FALSE;fail_reload=TRUE;switch_fixture("Upload project");assert((dialog_flags&MB_ICONMASK)==MB_ICONWARNING&&wcsstr(dialog_text,L"Fixture reload"));
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Unavailable switch: real worker/preflight, contextual RU/EN information, no POST and distinct network/reload warnings passed");return 0;
}
