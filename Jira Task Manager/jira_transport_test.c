#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static volatile LONG active,peak,calls;static int phase;
static DWORD hold;static HANDLE entered;
static BOOL fixture(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    LONG now=InterlockedIncrement(&active);if(now>peak)InterlockedExchange(&peak,now);InterlockedIncrement(&calls);
    if(entered)SetEvent(entered);Sleep(hold?hold:30);InterlockedDecrement(&active);
    if(phase==1){c->retry_after_seconds=1;JiraCheckHttpStatus(c,429);return FALSE;}
    if(phase==2){assert(!wcscmp(method,L"POST"));return FALSE;}
    const char *json="{\"displayName\":\"User\"}";*length=strlen(json);*out=malloc(*length+1);memcpy(*out,json,*length+1);return TRUE;
}
static DWORD WINAPI worker(void *arg)
{JiraConnection c={0};c.send=fixture;wchar_t account[256];assert(JiraVerifyAccount(&c,account,256));return 0;}
int main(void)
{
    HANDLE threads[2]={CreateThread(NULL,0,worker,NULL,0,NULL),CreateThread(NULL,0,worker,NULL,0,NULL)};
    WaitForMultipleObjects(2,threads,TRUE,INFINITE);CloseHandle(threads[0]);CloseHandle(threads[1]);
    assert(peak==1&&calls==2&&"Every connection must share one serialized Jira transport");
    hold=400;entered=CreateEventW(NULL,TRUE,FALSE,NULL);HANDLE slow=CreateThread(NULL,0,worker,NULL,0,NULL);
    assert(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0);
    JiraConnection bounded={0};bounded.send=fixture;const wchar_t keys[][80]={L"A-1"};JiraSnapshot snapshot={0};ULONGLONG began=GetTickCount64();
    assert(!JiraReadIssueStatusesWithin(&bounded,keys,1,&snapshot,50));
    assert(GetTickCount64()-began<200&&"Waiting in the shared queue must honor the read deadline");
    WaitForSingleObject(slow,2000);CloseHandle(slow);CloseHandle(entered);entered=NULL;hold=0;
    JiraConnection c={0};c.send=fixture;wchar_t account[256];phase=1;
    assert(!JiraVerifyAccount(&c,account,256));LONG before=calls;
    assert(!JiraVerifyAccount(&c,account,256)&&calls==before&&"Retry-After cooldown must prevent another request");
    Sleep(1050);phase=2;assert(!JiraPostComment(&c,L"A-1",L"Test"));assert(calls==before+1&&"Ambiguous POST must never be repeated");
    puts("Serialized transport, rate-limit cooldown and single POST passed");
}
