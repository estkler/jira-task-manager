#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int accounts,status_reads;
static BOOL fixture(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length){
    assert(!wcscmp(method,L"GET")&&!body);const char *json;
    if(wcsstr(path,L"/myself")){accounts++;json="{\"displayName\":\"Fixture user\",\"key\":\"u\"}";}
    else{assert(wcsstr(path,L"/issue/A-1?fields=status,assignee,reporter"));status_reads++;json="{\"key\":\"A-1\",\"fields\":{\"status\":{\"id\":\"1\",\"name\":\"In Progress\",\"statusCategory\":{\"key\":\"indeterminate\"}},\"assignee\":{\"key\":\"u\"},\"reporter\":null}}";}
    *length=strlen(json);*out=malloc(*length+1);memcpy(*out,json,*length+1);return TRUE;
}
static JiraConnection connection(ULONGLONG stamp,const wchar_t *host){JiraConnection c={0};c.send=fixture;c.credential_stamp=stamp;c.port=443;wcscpy_s(c.host,256,host);return c;}
int main(void){
    const wchar_t keys[][80]={L"A-1"};JiraSnapshot first={0},next={0};JiraConnection c=connection(10,L"fixture.invalid");
    assert(JiraReadIssueStatuses(&c,keys,1,&first)&&accounts==1&&status_reads==1);
    assert(JiraReadIssueStatuses(&c,keys,1,&next)&&accounts==1&&status_reads==2&&"One connection must verify its account only once");JiraFreeSnapshot(&next);
    JiraConnection seeded=connection(10,L"fixture.invalid");assert(JiraSeedAccount(&seeded,&first));
    assert(JiraReadIssueStatuses(&seeded,keys,1,&next)&&accounts==1&&"A verified matching job scope avoids another account request");JiraFreeSnapshot(&next);
    JiraConnection changed=connection(11,L"fixture.invalid");assert(!JiraSeedAccount(&changed,&first));
    assert(JiraReadIssueStatuses(&changed,keys,1,&next)&&accounts==2);JiraFreeSnapshot(&next);
    changed=connection(10,L"other.invalid");assert(!JiraSeedAccount(&changed,&first));
    assert(JiraReadIssueStatuses(&changed,keys,1,&next)&&accounts==3);JiraFreeSnapshot(&next);
    c.credential_stamp=12;assert(JiraReadIssueStatuses(&c,keys,1,&next)&&accounts==4);JiraFreeSnapshot(&next);
    changed=connection(0,L"fixture.invalid");assert(!JiraSeedAccount(&changed,&first));
    JiraFreeSnapshot(&first);puts("Verified account reuse: one read per connection, matching job scope only, server and credential isolation");
}
