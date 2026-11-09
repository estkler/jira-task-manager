#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Synthetic transport only; no network, real credentials or user profile. */
static int tasks,changed,index_cap=100,detail_cap=100,accounts,indices,details,fail_details;
static BOOL fixture(JiraConnection *c,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *length)
{
    assert(!wcscmp(method,L"GET")&&!body);char *json=calloc(1,100000);assert(json);size_t used=0;
    if(wcsstr(path,L"/myself")){accounts++;strcpy_s(json,100000,"{\"displayName\":\"Fixture\",\"key\":\"fixture\"}");}
    else{
        int start=_wtoi(wcsstr(path,L"startAt=")+8),ids[25],batch=0,total=tasks,cap=index_cap;
        BOOL index=wcsstr(path,L"fields=updated,assignee,reporter")!=NULL;
        if(index)indices++;
        else{
            details++;if(fail_details){free(json);return FALSE;}
            assert(wcsstr(path,L"fields=summary,status,updated,comment,duedate,created"));
            const wchar_t *at=wcsstr(path,L"id%20in%20(");assert(at);at+=11;
            while(*at!=L')'){
                assert(batch<25);wchar_t *end;ids[batch++]=(int)wcstol(at,&end,10);assert(end!=at);at=end;
                if(*at==L','){assert(!wcsncmp(at,L",%20",4));at+=4;}else assert(*at==L')');
            }
            total=batch;cap=detail_cap;
        }
        assert(start>=0&&start<=total);int end=min(total,start+cap);
        used+=(size_t)snprintf(json+used,100000-used,"{\"startAt\":%d,\"total\":%d,\"issues\":[",start,total);
        for(int i=start;i<end;i++){
            int id=index?i+1:ids[i];assert(id>0&&id<=tasks);
            used+=(size_t)snprintf(json+used,100000-used,"%s{\"id\":\"%d\",\"key\":\"A-%d\",\"fields\":{\"updated\":\"v%d\",",i==start?"":",",id,id,id<=changed?2:1);
            if(index)used+=(size_t)snprintf(json+used,100000-used,"\"assignee\":{\"key\":\"fixture\"},\"reporter\":null}}");
            else used+=(size_t)snprintf(json+used,100000-used,"\"summary\":\"Task %d\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}",id);
            assert(used<99000);
        }
        strcat_s(json,100000,"]}");
    }
    *out=json;*length=strlen(json);return TRUE;
}
static BOOL restart(const wchar_t *settings,ULONGLONG stamp,const wchar_t *filter,JiraSnapshot *out)
{
    JiraConnection c={0};c.send=fixture;c.port=443;c.credential_stamp=stamp;c.excluded_emails=filter;
    wcscpy_s(c.host,256,L"fixture.invalid");assert(JiraConfigureSnapshotCache(&c,settings));
    accounts=indices=details=0;return JiraRefreshWithReporter(&c,NULL,out);
}
static void sample(const wchar_t *settings,int count)
{
    tasks=count;changed=0;index_cap=detail_cap=100;JiraSnapshot result={0};JiraSnapshotCacheRemove(settings);
    assert(restart(settings,42,L"",&result));assert(result.count==tasks&&accounts==1&&indices==(tasks+99)/100&&details==(tasks+24)/25);
    int cold=accounts+indices+details;JiraFreeSnapshot(&result);
    assert(restart(settings,42,L"",&result));assert(accounts==1&&indices==(tasks+99)/100&&details==0);
    int cached=accounts+indices+details;JiraFreeSnapshot(&result);
    changed=3;fail_details=1;assert(!restart(settings,42,L"",&result)&&!result.items);
    fail_details=0;changed=0;assert(restart(settings,42,L"",&result)&&details==0);JiraFreeSnapshot(&result);
    changed=3;assert(restart(settings,42,L"",&result)&&details==1&&result.count==tasks);JiraFreeSnapshot(&result);
    tasks--;assert(restart(settings,42,L"",&result)&&details==0&&result.count==tasks);JiraFreeSnapshot(&result);
    assert(restart(settings,43,L"",&result)&&details==(tasks+24)/25);JiraFreeSnapshot(&result);
    assert(restart(settings,43,L"hidden@example.invalid",&result)&&details==(tasks+24)/25);JiraFreeSnapshot(&result);
    /* The server may return fewer records than requested; offsets use actual page size. */
    JiraSnapshotCacheRemove(settings);index_cap=17;detail_cap=7;
    assert(restart(settings,42,L"",&result)&&indices==(tasks+16)/17&&result.count==tasks);JiraFreeSnapshot(&result);
    assert(JiraSnapshotCacheRemove(settings));
    printf("%d tasks: cold %d GETs, verified restart %d GETs, 3 changes one detail batch; removal, failure, credential/filter and server-capped pagination passed\n",count,cold,cached);
}
int main(void)
{
    wchar_t temp[MAX_PATH],settings[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));
    swprintf(settings,MAX_PATH,L"%lsjira-restart-%lu.ini",temp,GetCurrentProcessId());
    sample(settings,54);sample(settings,1000);
}
