#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Fixture transport exercises real pagination/merge code; never accesses Jira. */
static int stage,index_reads,detail_reads,accounts,mode;
static BOOL fixture(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    assert(!wcscmp(method,L"GET"));assert(!body);const char *json=NULL;char page[4096];
    if(wcsstr(path,L"/myself")){accounts++;json="{\"displayName\":\"User\",\"key\":\"u\"}";}
    else if(wcsstr(path,L"fields=updated,assignee,reporter")){
        index_reads++;
        if(mode==3&&index_reads==2)return FALSE;
        if(mode==5){
            int at=wcsstr(path,L"startAt=1")?1:0;
            snprintf(page,sizeof(page),"{\"startAt\":%d,\"total\":2,\"issues\":[{\"id\":\"%d\",\"key\":\"A-%d\",\"fields\":{\"updated\":\"v1\",\"assignee\":{\"key\":\"u\"},\"reporter\":null}}]}",at,10+at,1+at);json=page;
        }
        else if(mode==4)json="{\"startAt\":0,\"total\":0,\"issues\":[]}";
        else if(mode==3&&index_reads==1)json="{\"startAt\":0,\"total\":2,\"issues\":[{\"id\":\"10\",\"key\":\"A-1\",\"fields\":{\"updated\":\"v1\",\"assignee\":{\"key\":\"u\"},\"reporter\":null}}]}";
        else json=stage==0?
            "{\"startAt\":0,\"total\":1,\"issues\":[{\"id\":\"10\",\"key\":\"A-1\",\"fields\":{\"updated\":\"v1\",\"assignee\":{\"key\":\"u\"},\"reporter\":{\"key\":\"u\"}}}]}" :
            "{\"startAt\":0,\"total\":1,\"issues\":[{\"id\":\"10\",\"key\":\"A-1\",\"fields\":{\"updated\":\"v2\",\"assignee\":null,\"reporter\":{\"key\":\"u\"}}}]}";
    }else{
        assert(wcsstr(path,L"fields=summary,status,updated,comment")&&wcsstr(path,L"duedate")&&wcsstr(path,L",created"));detail_reads++;
        if(mode==2)return FALSE;
        if(mode==5){
            int at=wcsstr(path,L"startAt=1")?1:0;
            snprintf(page,sizeof(page),"{\"startAt\":%d,\"total\":2,\"issues\":[{\"id\":\"%d\",\"key\":\"A-%d\",\"fields\":{\"updated\":\"v1\",\"summary\":\"Paged\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}",at,10+at,1+at);json=page;
        }else json=stage==0||mode==6?
            "{\"startAt\":0,\"total\":1,\"issues\":[{\"id\":\"10\",\"key\":\"A-1\",\"fields\":{\"updated\":\"v1\",\"summary\":\"First\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}" :
            "{\"startAt\":0,\"total\":1,\"issues\":[{\"id\":\"10\",\"key\":\"A-1\",\"fields\":{\"updated\":\"v2\",\"summary\":\"Changed\",\"status\":{\"id\":\"2\",\"name\":\"On Hold\",\"statusCategory\":{\"key\":\"indeterminate\"}}}}]}";
    }
    *length=strlen(json);*out=malloc(*length+1);memcpy(*out,json,*length+1);return TRUE;
}
int main(void)
{
    JiraConnection c={0};c.send=fixture;wcscpy_s(c.host,256,L"fixture.invalid");c.port=443;
    JiraSnapshot first={0},next={0};
    assert(JiraRefreshWithReporter(&c,NULL,&first));
    assert(first.count==1&&first.items[0].assigned_to_me&&first.items[0].reported_by_me&&detail_reads==1);
    wcscpy_s(first.items[0].due_date,11,L"2026-11-12"); /* Cached date metadata from a previous detail response. */
    first.items[0].created_utc=123456789;
    assert(JiraRefreshWithReporter(&c,&first,&next));assert(detail_reads==1&&index_reads==2);
    assert(!wcscmp(next.items[0].summary,L"First")&&!wcscmp(next.items[0].due_date,L"2026-11-12")&&next.items[0].created_utc==123456789);JiraFreeSnapshot(&next);
    stage=1;assert(JiraRefreshWithReporter(&c,&first,&next));
    assert(detail_reads==2&&!next.items[0].assigned_to_me&&next.items[0].reported_by_me);
    assert(!next.items[0].due_date[0]&&"An updated issue without a date must clear the cached due date");
    assert(!next.items[0].created_utc);
    assert(!wcscmp(next.items[0].summary,L"Changed"));JiraFreeSnapshot(&next);
    mode=6;assert(!JiraRefreshWithReporter(&c,&first,&next)&&"An older detail replica must not override a newer index revision");assert(!next.items&&first.count==1);
    mode=2;assert(!JiraRefreshWithReporter(&c,&first,&next));assert(!next.items&&first.count==1&&!wcscmp(first.items[0].summary,L"First"));
    mode=3;index_reads=0;assert(!JiraRefreshWithReporter(&c,&first,&next));assert(!next.items&&first.count==1);
    mode=4;assert(JiraRefreshWithReporter(&c,&first,&next));assert(next.count==0);JiraFreeSnapshot(&next);
    mode=stage=0;c.excluded_emails=L"someone@example.org";int before=detail_reads;
    assert(JiraRefreshWithReporter(&c,&first,&next));assert(detail_reads==before+1);
    JiraFreeSnapshot(&next);
    mode=5;index_reads=0;assert(JiraRefreshWithReporter(&c,NULL,&next));assert(next.count==2&&index_reads==2);JiraFreeSnapshot(&next);
    mode=0;c.excluded_emails=NULL;c.credential_stamp=10;JiraSnapshot bound={0};assert(JiraRefreshWithReporter(&c,NULL,&bound));
    int account_before=accounts;before=detail_reads;assert(JiraRefreshWithReporter(&c,&bound,&next));assert(accounts==account_before&&detail_reads==before);JiraFreeSnapshot(&next);
    c.credential_stamp=11;assert(JiraRefreshWithReporter(&c,&bound,&next));assert(accounts==account_before+1&&detail_reads==before+1);JiraFreeSnapshot(&next);
    JiraFreeSnapshot(&bound);JiraFreeSnapshot(&first);puts("Jira refresh fixtures passed");
}
