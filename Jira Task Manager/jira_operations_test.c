#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int scenario,posts,pages;
static const char *tasks="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-1\",\"fields\":{\"summary\":\"Active\",\"status\":{\"id\":\"3\",\"name\":\"In Progress\",\"statusCategory\":{\"key\":\"indeterminate\"}}}},{\"key\":\"A-2\",\"fields\":{\"summary\":\"Next\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *first="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-1\",\"fields\":{\"summary\":\"One\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *second="{\"startAt\":1,\"total\":2,\"issues\":[{\"key\":\"A-2\",\"fields\":{\"summary\":\"Two\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *reported_tasks="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-2\",\"fields\":{\"summary\":\"Next\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}},{\"key\":\"R-1\",\"fields\":{\"summary\":\"Admin request\",\"status\":{\"id\":\"4\",\"name\":\"On Hold\",\"statusCategory\":{\"key\":\"indeterminate\"}}}}]}";
static const char *comment_first="{\"startAt\":0,\"total\":2,\"comments\":[{\"id\":\"1\",\"created\":\"2026-10-01T10:00:00\",\"body\":\"One\"}]}";
static const char *comment_second="{\"startAt\":1,\"total\":2,\"comments\":[{\"id\":\"2\",\"created\":\"2026-10-01T11:00:00\",\"body\":\"Two\"}]}";
static BOOL fake(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length){
    const char *response="{}";
    if(!wcscmp(method,L"POST")){
        posts++;assert(posts<=2);
        if(scenario==9){assert(wcsstr(path,L"A-1/comment"));assert(strstr(body,"\"body\":\"Ready \\\"now\\\"\""));}
        else if(scenario==6){assert(wcsstr(path,L"A-1"));assert(strstr(body,"\"id\":\"31\""));assert(strstr(body,"customfield_15812"));assert(strstr(body,"15715"));assert(strstr(body,"Ready"));}
        else if(posts==1){assert(wcsstr(path,L"A-1"));assert(strstr(body,"hold-id"));}
        else if(posts==2){assert(wcsstr(path,L"A-2"));assert(strstr(body,"start-id"));}
        if(scenario==2 || (scenario==3&&posts==2)){wcscpy_s(c->error,256,L"Simulated ambiguous timeout");return FALSE;}
    }else if(wcsstr(path,L"myself"))response="{\"displayName\":\"Test account\"}";
    else if(wcsstr(path,L"A-1/comment?")){
        assert(scenario==8);
        response=wcsstr(path,L"startAt=1")?comment_second:comment_first;
    }
    else if(wcsstr(path,L"/search?")){
        pages++;
        if(scenario==7)response=wcsstr(path,L"jql=reporter")?reported_tasks:tasks;
        else if(scenario>=4){
            if(pages==1)response=first;
            else if(scenario==5){wcscpy_s(c->error,256,L"Simulated page failure");return FALSE;}
            else{assert(wcsstr(path,L"startAt=1"));response=second;}
        }else response=tasks;
    }else if(wcsstr(path,L"A-1/transitions")){assert(wcsstr(path,L"expand=transitions.fields"));response=scenario==1?"{\"transitions\":[]}":"{\"transitions\":[{\"id\":\"hold-id\",\"name\":\"Pause\",\"to\":{\"name\":\"On Hold\"}}]}";}
    else if(wcsstr(path,L"A-2/transitions")){assert(wcsstr(path,L"expand=transitions.fields"));response="{\"transitions\":[{\"id\":\"start-id\",\"name\":\"Start\",\"to\":{\"name\":\"In Progress\"}}]}";}
    else assert(0);
    *length=strlen(response);*out=malloc(*length+1);memcpy(*out,response,*length+1);return TRUE;
}
int main(void){
    JiraConnection c={0};c.send=fake;
    wchar_t account[256];assert(JiraVerifyAccount(&c,account,256));
    assert(!wcscmp(account,L"Test account")&&posts==0&&pages==0);
    assert(JiraCheckHttpStatus(&c,200));assert(JiraCheckHttpStatus(&c,204));
    assert(!JiraCheckHttpStatus(&c,401)&&wcsstr(c.error,L"token"));
    assert(!JiraCheckHttpStatus(&c,403)&&wcsstr(c.error,L"403"));
    assert(!JiraCheckHttpStatus(&c,302));assert(!JiraCheckHttpStatus(&c,500));
    char *transition_body=NULL;size_t transition_length=0;
    assert(JiraBuildTransitionBody(L"31",L"customfield_15812",L"15715",L"Готово \"да\"\n",&transition_body,&transition_length));
    assert(transition_length==strlen(transition_body));
    assert(strstr(transition_body,"\"transition\":{\"id\":\"31\"}"));
    assert(strstr(transition_body,"\"customfield_15812\":{\"id\":\"15715\"}"));
    assert(strstr(transition_body,"\\\"да\\\"\\n"));
    free(transition_body);
    assert(!JiraBuildTransitionBody(L"31",L"customfield_15812",L"",L"",&transition_body,&transition_length));
    scenario=6;posts=pages=0;c.write_attempted=FALSE;
    assert(JiraPostTransitionWithInput(&c,L"A-1",L"31",L"customfield_15812",L"15715",L"Ready"));
    assert(posts==1&&c.write_attempted);
    for(scenario=0;scenario<4;scenario++){
        posts=pages=0;BOOL ok=JiraSwitch(&c,L"A-2");
        assert(ok==(scenario==0));
        assert(posts==(scenario==1?0:scenario==2?1:2));
    }
    JiraSnapshot snapshot={0};scenario=4;pages=posts=0;
    assert(JiraLoad(&c,&snapshot)&&snapshot.count==2&&pages==2);JiraFreeSnapshot(&snapshot);
    scenario=5;pages=posts=0;
    assert(!JiraLoad(&c,&snapshot)&&snapshot.count==0&&snapshot.items==NULL&&posts==0);
    scenario=7;pages=posts=0;
    assert(JiraLoadWithReporter(&c,&snapshot,FALSE)&&snapshot.count==3&&pages==2);
    assert(snapshot.items[0].assigned_to_me&&!snapshot.items[0].reported_by_me);
    assert(snapshot.items[1].assigned_to_me&&snapshot.items[1].reported_by_me);
    assert(!snapshot.items[2].assigned_to_me&&snapshot.items[2].reported_by_me);
    JiraFreeSnapshot(&snapshot);
    scenario=8;posts=pages=0;JiraComments comments={0};
    assert(JiraReadComments(&c,L"A-1",&comments)&&comments.count==2);
    assert(!wcscmp(comments.items[1].body,L"Two"));JiraFreeComments(&comments);
    scenario=9;posts=0;c.write_attempted=FALSE;
    assert(JiraPostComment(&c,L"A-1",L"Ready \"now\""));
    assert(posts==1&&c.write_attempted);
    assert(!JiraPostComment(&c,L"A-1",L""));
    puts("Jira operations tests passed: pagination, preflight, partial failure, no POST retry");
}
