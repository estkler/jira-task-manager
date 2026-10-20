#define main jira_operations_regression_main
#include "jira_operations_test.c"
#undef main
#include "native_status_activity.h"
static BOOL reject_post;
static int reject_post_number;
static BOOL tracked_transport(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    if((reject_post||reject_post_number==posts+1)&&!wcscmp(method,L"POST")){posts++;return JiraCheckHttpStatus(c,403);}
    if(wcscmp(method,L"POST")&&wcsstr(path,L"/transitions")&&scenario!=1){
        const char *json=wcsstr(path,L"A-1")?
            "{\"transitions\":[{\"id\":\"hold-id\",\"name\":\"Pause\",\"to\":{\"id\":\"hold-status\",\"name\":\"On Hold\"}}]}":
            "{\"transitions\":[{\"id\":\"start-id\",\"name\":\"Start\",\"to\":{\"id\":\"progress-status\",\"name\":\"In Progress\"}}]}";
        *length=strlen(json);*out=malloc(*length+1);assert(*out);memcpy(*out,json,*length+1);return TRUE;
    }
    return fake(c,method,path,body,out,length);
}
static BOOL refuse_journal(void *context,const wchar_t *key,const wchar_t *id,const wchar_t *name)
{(void)context;(void)key;(void)id;(void)name;return FALSE;}
int main(void)
{
    NativeStatusWrites writes={0};JiraConnection c={0};c.send=tracked_transport;c.before_status_write=NativeStatusRemember;c.after_status_write=NativeStatusWriteOutcome;c.status_write_context=&writes;
    for(scenario=0;scenario<4;scenario++){
        posts=pages=0;writes.count=0;BOOL ok=JiraSwitch(&c,L"A-2");assert(ok==(scenario==0));
        assert(writes.count==(scenario==1?0:scenario==2?1:2));
        if(writes.count){assert(!wcscmp(writes.items[0].key,L"A-1"));assert(!wcscmp(writes.items[0].status_id,L"hold-status"));}
        if(writes.count==2){assert(!wcscmp(writes.items[1].key,L"A-2"));assert(!wcscmp(writes.items[1].status_id,L"progress-status"));}
    }
    scenario=0;posts=pages=0;c.before_status_write=refuse_journal;
    assert(!JiraSwitch(&c,L"A-2")&&posts==0); /* No POST when tracking fails; no automatic retry. */
    JiraTransitions transitions;const char *json="{\"transitions\":[{\"id\":\"31\",\"name\":\"Finish\",\"to\":{\"id\":\"upload\",\"name\":\"Upload project\"}}]}";
    assert(JiraParseTransitions(json,strlen(json),&transitions));
    c.before_status_write=NativeStatusRemember;writes.count=0;scenario=6;posts=0;
    assert(JiraPostKnownTransitionWithInput(&c,L"A-1",&transitions.items[0],L"customfield_15812",L"15715",L"Ready"));
    assert(writes.count==1&&!wcscmp(writes.items[0].status_id,L"upload")&&!wcscmp(writes.items[0].key,L"A-1"));
    writes.count=0;
    assert(!JiraPostKnownTransitionWithInput(&c,L"A-1",&transitions.items[0],L"customfield_15812",L"",NULL));
    assert(writes.count==0); /* Local validation never suppresses a future external change. */
    reject_post=TRUE;
    assert(!JiraPostKnownTransitionWithInput(&c,L"A-1",&transitions.items[0],NULL,NULL,NULL));
    assert(writes.count==0); /* A definitive 403 did not move the issue. */
    reject_post=FALSE;reject_post_number=2;posts=pages=0;scenario=0;
    assert(!JiraSwitch(&c,L"A-2")&&posts==2);
    assert(writes.count==1&&!wcscmp(writes.items[0].key,L"A-1")); /* Retain first successful POST, discard only rejected second. */
    reject_post_number=0;writes.count=0;c.send=NULL;
    assert(!JiraPostKnownTransitionWithInput(&c,L"A-1",&transitions.items[0],NULL,NULL,NULL));
    assert(!c.request_sent&&writes.count==0); /* Invalid connection: definitely no bytes were sent. */
    free(writes.items);puts("Status writes: explicit destination, switch target/hold tasks, partial/ambiguous failures and failed preflight tracking passed");return 0;
}
