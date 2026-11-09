/* Transactional membership index + changed detail batches. Included by native_jira.c. */
static BOOL RefreshIssueId(const wchar_t *id)
{
    size_t length=wcslen(id);if(!length||length>20)return FALSE;
    for(size_t i=0;i<length;i++)if(id[i]<L'0'||id[i]>L'9')return FALSE;return TRUE;
}
static BOOL RefreshIndexPage(JiraConnection *c,JiraSnapshot *result,const char *json,size_t length,int offset,int *total)
{
    NJDoc d={0};if(!nj_parse(&d,json,length))return fail(c,L"Invalid Jira index response.");
    int array=nj_get(&d,0,"issues"),start=nj_int(&d,nj_get(&d,0,"startAt"),-1);
    *total=nj_int(&d,nj_get(&d,0,"total"),-1);BOOL ok=FALSE;
    if(array<0||d.tokens[array].kind!=NJ_ARRAY||start!=offset||*total<0||*total>JIRA_MAX_TASKS)goto done;
    int count=d.tokens[array].count;
    if(count<0||count>JIRA_MAX_TASKS-result->count||result->count+count>*total||(!count&&offset<*total))goto done;
    JiraItem *expanded=realloc(result->items,(size_t)(result->count+count+1)*sizeof(*expanded));if(!expanded)goto done;
    result->items=expanded;int at=array+1;
    for(int i=0;i<count;i++){
        JiraItem *item=&result->items[result->count];memset(item,0,sizeof(*item));int fields=nj_get(&d,at,"fields");
        if(!field(&d,at,"id",item->id,80)||!RefreshIssueId(item->id)||
           !field(&d,at,"key",item->key,80)||!item->key[0]||
           !field(&d,fields,"updated",item->updated,80)||!item->updated[0]||
           nj_get(&d,fields,"assignee")<0||nj_get(&d,fields,"reporter")<0)goto done;
        for(int j=0;j<result->count;j++)if(!wcscmp(result->items[j].id,item->id)||!_wcsicmp(result->items[j].key,item->key))goto done;
        JiraUserIdentity owner,reporter;user_identity(&d,nj_get(&d,fields,"assignee"),&owner);user_identity(&d,nj_get(&d,fields,"reporter"),&reporter);
        item->assigned_to_me=SameJiraUser(&owner,&result->identity);item->reported_by_me=SameJiraUser(&reporter,&result->identity);
        if(!item->assigned_to_me&&!item->reported_by_me)goto done;
        result->count++;at=d.tokens[at].next;
    }
    ok=TRUE;
done:nj_free(&d);return ok?TRUE:fail(c,L"Incomplete Jira index. Previous tasks retained.");
}
BOOL JiraRefreshWithReporter(JiraConnection *c,const JiraSnapshot *previous,JiraSnapshot *snapshot)
{
    if(!c||!snapshot||snapshot==previous)return FALSE;
    JiraSnapshot result={0},disk={0};
    if(previous&&previous->refresh_cache)JiraSeedAccount(c,previous);
    if(!JiraLoadAccount(c,&result))return FALSE;
    const wchar_t *filter=c->excluded_emails?c->excluded_emails:NATIVE_EXCLUDED_EMAILS_DEFAULT;
    if(wcslen(filter)>=_countof(result.comment_filter))return fail(c,L"Invalid comment filter.");
    BOOL reuse=previous&&previous->refresh_cache&&!wcscmp(result.server,previous->server)&&SameJiraUser(&result.identity,&previous->identity)&&
        previous->credential_stamp==c->credential_stamp&&!wcscmp(filter,previous->comment_filter);
    if(!reuse&&JiraSnapshotCacheLoad(c,&result,filter,&disk)){previous=&disk;reuse=TRUE;}
    int offset=0,total=0,index_total=-1;
    do{
        wchar_t path[512];swprintf(path,512,L"/rest/api/2/search?jql=(assignee%%3DcurrentUser()%%20OR%%20reporter%%3DcurrentUser())%%20ORDER%%20BY%%20id%%20ASC&startAt=%d&maxResults=100&fields=updated,assignee,reporter",offset);
        char *data=NULL;size_t length=0;if(!request(c,L"GET",path,NULL,&data,&length))goto failed;
        BOOL ok=RefreshIndexPage(c,&result,data,length,offset,&total);free(data);if(!ok)goto failed;
        if(index_total>=0&&total!=index_total){fail(c,L"Jira membership changed during loading. Refresh again.");goto failed;}
        index_total=total;offset=result.count;
    }while(offset<total);
    /* Empty summary marks rows requiring details; membership always comes from this index. */
    if(reuse)for(int i=0;i<result.count;i++)for(int j=0;j<previous->count;j++){
        JiraItem *item=&result.items[i];const JiraItem *cached=&previous->items[j];
        if(!wcscmp(item->id,cached->id)&&!wcscmp(item->key,cached->key)&&cached->updated[0]&&!wcscmp(item->updated,cached->updated)){
            BOOL assigned=item->assigned_to_me,reported=item->reported_by_me;*item=*cached;item->assigned_to_me=assigned;item->reported_by_me=reported;break;
        }
    }
    for(int next=0;next<result.count;){
        int indices[25],count=0;wchar_t ids[600]=L"";
        for(;next<result.count&&count<25;next++)if(!result.items[next].summary[0]){
            if(count)wcscat_s(ids,600,L", ");wcscat_s(ids,600,result.items[next].id);indices[count++]=next;
        }
        if(!count)continue;
        /* IDs are validated decimal strings; a batch fits safely in the transport URL limit. */
        wchar_t encoded[1800]=L"";size_t used=0;
        for(const wchar_t *p=ids;*p;p++)if(*p==L' '){wcscpy_s(encoded+used,1800-used,L"%20");used+=3;}else encoded[used++]=*p;
        encoded[used]=0;JiraSnapshot details={0};offset=total=0;
        do{
            wchar_t path[2048];swprintf(path,2048,L"/rest/api/2/search?jql=id%%20in%%20(%ls)%%20ORDER%%20BY%%20id%%20ASC&startAt=%d&maxResults=100&fields=summary,status,updated,comment,duedate,created",encoded,offset);
            char *data=NULL;size_t length=0;if(!request(c,L"GET",path,NULL,&data,&length)){JiraFreeSnapshot(&details);goto failed;}
            int start=-1,before=details.count;BOOL ok=JiraParsePageFiltered(data,length,&details,&total,&start,c->excluded_emails);free(data);
            if(!ok||start!=offset||total!=count||details.count>count||(details.count==before&&offset<total)){
                JiraFreeSnapshot(&details);fail(c,L"Jira tasks changed during detail loading. Previous tasks retained.");goto failed;
            }
            offset=details.count;
        }while(offset<total);
        for(int i=0;i<count;i++){
            JiraItem *item=&result.items[indices[i]];int match=-1;
            for(int j=0;j<details.count;j++)if(!wcscmp(item->id,details.items[j].id)){if(match>=0){match=-1;break;}match=j;}
            if(match<0||wcscmp(item->key,details.items[match].key)||!details.items[match].updated[0]||wcscmp(item->updated,details.items[match].updated)){
                JiraFreeSnapshot(&details);fail(c,L"Jira details no longer match the index. Refresh again.");goto failed;
            }
            BOOL assigned=item->assigned_to_me,reported=item->reported_by_me;*item=details.items[match];item->assigned_to_me=assigned;item->reported_by_me=reported;
        }
        JiraFreeSnapshot(&details);
    }
    wcscpy_s(result.comment_filter,2048,filter);result.refresh_cache=TRUE;result.credential_stamp=c->credential_stamp;
    /* Persistence is optional: a failed cache write must not fail a verified refresh. */
    JiraSnapshotCacheSave(c,&result);JiraFreeSnapshot(&disk);*snapshot=result;return TRUE;
failed:JiraFreeSnapshot(&disk);JiraFreeSnapshot(&result);return FALSE;
}
