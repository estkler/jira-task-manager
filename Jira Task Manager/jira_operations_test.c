#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int scenario,posts,pages,old_transition_reads;
static const wchar_t *expected_post_key=L"A-2";
static const char *expected_post_transition="start-id";
static const char *tasks="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-1\",\"fields\":{\"summary\":\"Active\",\"status\":{\"id\":\"3\",\"name\":\"In Progress\",\"statusCategory\":{\"key\":\"indeterminate\"}}}},{\"key\":\"A-2\",\"fields\":{\"summary\":\"Next\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *first="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-1\",\"fields\":{\"summary\":\"One\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *second="{\"startAt\":1,\"total\":2,\"issues\":[{\"key\":\"A-2\",\"fields\":{\"summary\":\"Two\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}";
static const char *reported_tasks="{\"startAt\":0,\"total\":2,\"issues\":[{\"key\":\"A-2\",\"fields\":{\"summary\":\"Next\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}},{\"key\":\"R-1\",\"fields\":{\"summary\":\"Admin request\",\"status\":{\"id\":\"4\",\"name\":\"On Hold\",\"statusCategory\":{\"key\":\"indeterminate\"}}}}]}";
static const char *comment_first="{\"startAt\":0,\"total\":2,\"comments\":[{\"id\":\"1\",\"created\":\"2026-10-01T10:00:00\",\"body\":\"One\"}]}";
static const char *comment_second="{\"startAt\":1,\"total\":2,\"comments\":[{\"id\":\"2\",\"created\":\"2026-10-01T11:00:00\",\"body\":\"Two\"}]}";
static BOOL fake(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length){
    if(scenario==10&&!wcscmp(method,L"GET"))Sleep(30); /* Deadline applies even when /myself is cached. */
    const char *response="{}";
    if(!wcscmp(method,L"POST")){
        posts++;assert(posts<=2);
        if(scenario==9){assert(wcsstr(path,L"A-1/comment"));assert(strstr(body,"\"body\":\"Ready \\\"now\\\"\""));}
        else if(scenario==6){assert(wcsstr(path,L"A-1"));assert(strstr(body,"\"id\":\"31\""));assert(strstr(body,"customfield_15812"));assert(strstr(body,"15715"));assert(strstr(body,"Ready"));}
        else {assert(posts==1);assert(wcsstr(path,expected_post_key));assert(strstr(body,expected_post_transition));}
        if(scenario==2){wcscpy_s(c->error,256,L"Simulated ambiguous timeout");return FALSE;}
    }else if(wcsstr(path,L"myself")){response="{\"displayName\":\"Test account\",\"key\":\"user-1\",\"name\":\"test-account\"}";}
    else if(wcsstr(path,L"?fields=status,assignee,reporter")){
        assert(!wcscmp(method,L"GET"));
        if(wcsstr(path,L"/issue/A-1?"))response="{\"key\":\"A-1\",\"fields\":{\"status\":{\"id\":\"3\",\"name\":\"In Progress\",\"statusCategory\":{\"key\":\"indeterminate\"}},\"assignee\":{\"key\":\"user-1\"},\"reporter\":null}}";
        else if(wcsstr(path,L"/issue/A-2?"))response="{\"key\":\"A-2\",\"fields\":{\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}},\"assignee\":{\"key\":\"user-1\"},\"reporter\":null}}";
        else {wcscpy_s(c->error,256,L"Issue not accessible");return FALSE;}
        if(scenario==11)response="{\"key\":\"A-2\",\"fields\":{\"status\":{\"name\":\"On Hold\"}}}";
        if(scenario==12)response="{\"key\":\"different\",\"fields\":{\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}";
        if(scenario==13)response="{\"key\":\"A-2\",\"fields\":{\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}},\"assignee\":null,\"reporter\":{\"key\":\"user-1\"}}}";
    }
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
    }else if(wcsstr(path,L"A-1/transitions")){old_transition_reads++;assert(wcsstr(path,L"expand=transitions.fields"));response=scenario==1?"{\"transitions\":[]}":"{\"transitions\":[{\"id\":\"hold-id\",\"name\":\"Pause\",\"to\":{\"name\":\"On Hold\"}}]}";}
    else if(wcsstr(path,L"A-2/transitions")){assert(wcsstr(path,L"expand=transitions.fields"));response=scenario==3?"{\"transitions\":[]}":"{\"transitions\":[{\"id\":\"start-id\",\"name\":\"Start\",\"to\":{\"name\":\"In Progress\"}}]}";}
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
        posts=pages=old_transition_reads=0;BOOL ok=JiraSwitch(&c,L"A-2");
        assert(ok==(scenario==0||scenario==1));
        assert(posts==(scenario==3?0:1));
        assert(old_transition_reads==0); /* Jira owns automatic pauses, even if the old issue cannot be paused by this account. */
        assert(pages==0); /* A switch must not download either full task feed. */
    }
    scenario=0;posts=pages=old_transition_reads=0;
    expected_post_key=L"A-1";expected_post_transition="hold-id";
    assert(JiraSwitch(&c,L"A-1")&&posts==1&&old_transition_reads==1); /* Explicit pause still works. */
    expected_post_key=L"A-2";expected_post_transition="start-id";
    posts=pages=old_transition_reads=0;
    assert(!JiraSwitch(&c,L"missing")&&posts==0&&old_transition_reads==0);
    JiraSnapshot snapshot={0};scenario=4;pages=posts=0;
    wcscpy_s(c.host,256,L"JIRA.Example.Test");wcscpy_s(c.prefix,512,L"/jira");c.port=443;
    assert(JiraLoad(&c,&snapshot)&&snapshot.count==2&&pages==2);
    assert(!wcscmp(snapshot.identity.key,L"user-1")&&!wcscmp(snapshot.identity.name,L"test-account"));
    assert(!wcscmp(snapshot.server,L"jira.example.test:443/jira"));
    JiraFreeSnapshot(&snapshot);
    scenario=5;pages=posts=0;
    assert(!JiraLoad(&c,&snapshot)&&snapshot.count==0&&snapshot.items==NULL&&posts==0);
    scenario=7;pages=posts=0;
    assert(JiraLoadWithReporter(&c,&snapshot,FALSE)&&snapshot.count==3&&pages==2);
    assert(!wcscmp(snapshot.server,L"jira.example.test:443/jira"));
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
    wchar_t status_keys[1][80]={L"A-2"};scenario=10;posts=pages=0;
    assert(!JiraReadIssueStatusesWithin(&c,status_keys,1,&snapshot,10)&&!snapshot.items&&!snapshot.count&&!c.read_deadline);
    for(scenario=11;scenario<=12;scenario++)assert(!JiraReadIssueStatuses(&c,status_keys,1,&snapshot)&&!snapshot.items&&!snapshot.count);
    scenario=13;assert(!JiraSwitch(&c,L"A-2")&&posts==0); /* Fresh reassignment blocks a cached task's write. */
    assert(JiraReadIssueStatuses(&c,status_keys,1,&snapshot)&&!snapshot.items[0].assigned_to_me&&snapshot.items[0].reported_by_me);JiraFreeSnapshot(&snapshot);
    assert(!JiraReadIssueStatuses(&c,status_keys,0,&snapshot));
    wcscpy_s(status_keys[0],80,L"A-2?unexpected");assert(!JiraReadIssueStatuses(&c,status_keys,1,&snapshot)&&posts==0);
    puts("Jira operations tests passed: target-only switch, explicit pause, pagination, preflight, no POST retry");
    return 0;
}
