#define WIN32_LEAN_AND_MEAN
#include "native_jira.h"
#include "native_json.h"
#include "native_identity.h"
#include <winhttp.h>
#include <wincred.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BOOL fail(JiraConnection *c,const wchar_t *message){wcsncpy_s(c->error,256,message,_TRUNCATE);return FALSE;}
static BOOL field(const NJDoc *d,int obj,const char *key,wchar_t *out,size_t cap){return nj_string(d,nj_get(d,obj,key),out,cap);}
static void user_identity(const NJDoc *doc,int user,JiraUserIdentity *identity)
{
    memset(identity,0,sizeof(*identity));
    field(doc,user,"accountId",identity->account_id,_countof(identity->account_id));
    field(doc,user,"key",identity->key,_countof(identity->key));
    field(doc,user,"name",identity->name,_countof(identity->name));
}
void JiraFreeSnapshot(JiraSnapshot *s){free(s->items);memset(s,0,sizeof(*s));}

static void append_comment_text(const NJDoc *doc,int token,wchar_t *out,size_t capacity,size_t *used)
{
    if(token<0||token>=doc->count||*used+1>=capacity)return;
    const NJToken *value=&doc->tokens[token];
    if(value->kind==NJ_STRING){
        wchar_t part[2048];if(!nj_string(doc,token,part,_countof(part))||!part[0])return;
        if(*used&&*used+1<capacity)out[(*used)++]=L' ';
        for(size_t i=0;part[i]&&*used+1<capacity;i++)out[(*used)++]=part[i];out[*used]=0;return;
    }
    if(value->kind==NJ_ARRAY){
        int child=token+1;for(int i=0;i<value->count&&child<doc->count;i++){append_comment_text(doc,child,out,capacity,used);child=doc->tokens[child].next;}return;
    }
    if(value->kind!=NJ_OBJECT)return;
    int text=nj_get(doc,token,"text");if(text>=0){append_comment_text(doc,text,out,capacity,used);return;}
    int content=nj_get(doc,token,"content");if(content>=0)append_comment_text(doc,content,out,capacity,used);
}

static void collapse_comment_space(wchar_t *text)
{
    size_t read=0,write=0;BOOL pending=FALSE;
    while(text[read]){
        wchar_t ch=text[read++];BOOL space=ch==L' '||ch==L'\t'||ch==L'\r'||ch==L'\n';
        if(space){pending=write>0;continue;}
        if(pending)text[write++]=L' ';pending=FALSE;text[write++]=ch;
    }
    text[write]=0;
}

static void latest_comment(const NJDoc *doc,int fields,JiraItem *item)
{
    item->latest_comment[0]=item->latest_comment_id[0]=item->latest_comment_author[0]=0;
    int page=nj_get(doc,fields,"comment"),array=nj_get(doc,page,"comments");
    if(array<0||doc->tokens[array].kind!=NJ_ARRAY)return;
    wchar_t newest[128]=L"";int selected=-1,at=array+1;
    for(int i=0;i<doc->tokens[array].count&&at<doc->count;i++){
        wchar_t created[128]=L"";field(doc,at,"created",created,_countof(created));
        if(selected<0||wcscmp(created,newest)>0){selected=at;wcscpy_s(newest,_countof(newest),created);}
        at=doc->tokens[at].next;
    }
    if(selected>=0){
        size_t used=0;
        append_comment_text(doc,nj_get(doc,selected,"body"),item->latest_comment,_countof(item->latest_comment),&used);
        collapse_comment_space(item->latest_comment);
        field(doc,selected,"id",item->latest_comment_id,_countof(item->latest_comment_id));
        field(doc,nj_get(doc,selected,"author"),"displayName",item->latest_comment_author,_countof(item->latest_comment_author));
    }
}

void JiraFreeComments(JiraComments *comments)
{
    if(!comments)return;
    free(comments->items);memset(comments,0,sizeof(*comments));
}

BOOL JiraCommentIsOwn(const JiraComment *comment,const JiraSnapshot *snapshot)
{
    if(!comment||!snapshot)return FALSE;
    const JiraUserIdentity *author=&comment->identity,*self=&snapshot->identity;
    /* Never use displayName: different users may share it, and it may change. */
    if(author->account_id[0]&&self->account_id[0])return !wcscmp(author->account_id,self->account_id);
    if(author->key[0]&&self->key[0])return !wcscmp(author->key,self->key);
    return author->name[0]&&self->name[0]&&!wcscmp(author->name,self->name);
}

static int CompareCommentChronology(const void *left,const void *right)
{
    const JiraComment *a=left,*b=right;
    int by_date=wcscmp(a->created,b->created);
    return by_date?by_date:wcscmp(a->id,b->id);
}

BOOL JiraParseComments(const char *json,size_t length,JiraComments *comments,int *total,int *start)
{
    if(!comments||!total||!start)return FALSE;
    NJDoc doc={0};if(!nj_parse(&doc,json,length))return FALSE;
    int array=nj_get(&doc,0,"comments");BOOL ok=FALSE;
    *total=nj_int(&doc,nj_get(&doc,0,"total"),-1);
    *start=nj_int(&doc,nj_get(&doc,0,"startAt"),-1);
    if(array<0||doc.tokens[array].kind!=NJ_ARRAY||*total<0||*start<0||
        *total>JIRA_MAX_COMMENTS||doc.tokens[array].count<0||
        comments->count>JIRA_MAX_COMMENTS-doc.tokens[array].count)goto end;
    int count=doc.tokens[array].count,at=array+1;
    JiraComment *expanded=realloc(comments->items,(size_t)(comments->count+count+1)*sizeof(*expanded));
    if(!expanded)goto end;
    comments->items=expanded;
    for(int i=0;i<count;i++){
        JiraComment *item=&comments->items[comments->count+i];memset(item,0,sizeof(*item));
        size_t used=0;
        if(!field(&doc,at,"id",item->id,_countof(item->id))||!item->id[0]||
            !field(&doc,at,"created",item->created,_countof(item->created)))goto end;
        field(&doc,nj_get(&doc,at,"author"),"displayName",item->author,_countof(item->author));
        user_identity(&doc,nj_get(&doc,at,"author"),&item->identity);
        append_comment_text(&doc,nj_get(&doc,at,"body"),item->body,_countof(item->body),&used);
        at=doc.tokens[at].next;
    }
    comments->count+=count;
    /* Jira may return a page in a different order; keep the feed oldest first. */
    qsort(comments->items,(size_t)comments->count,sizeof(*comments->items),CompareCommentChronology);
    ok=TRUE;
end:nj_free(&doc);return ok;
}

BOOL JiraParsePage(const char *json,size_t length,JiraSnapshot *s,int *total,int *start)
{
    NJDoc d={0};if(!nj_parse(&d,json,length))return FALSE;
    int array=nj_get(&d,0,"issues");BOOL ok=FALSE;
    *total=nj_int(&d,nj_get(&d,0,"total"),-1);*start=nj_int(&d,nj_get(&d,0,"startAt"),-1);
    if(array<0||d.tokens[array].kind!=NJ_ARRAY||*total<0||*start<0||*total>JIRA_MAX_TASKS)goto end;
    int count=d.tokens[array].count;
    if(count<0||s->count>JIRA_MAX_TASKS-count)goto end;
    JiraItem *items=calloc((size_t)(count?count:1),sizeof(*items));if(!items)goto end;
    int t=array+1;
    for(int i=0;i<count;i++){
        int fields=nj_get(&d,t,"fields"),status=nj_get(&d,fields,"status"),category=nj_get(&d,status,"statusCategory");
        if(!field(&d,t,"key",items[i].key,80)||!field(&d,fields,"summary",items[i].summary,1024)||
           !field(&d,status,"id",items[i].status_id,80)||!field(&d,status,"name",items[i].status_name,128)||
           !field(&d,category,"key",items[i].category,32)||!items[i].key[0]||!items[i].status_name[0]){free(items);goto end;}
        latest_comment(&d,fields,&items[i]);
        t=d.tokens[t].next;
    }
    JiraItem *expanded=realloc(s->items,(size_t)(s->count+count+1)*sizeof(*expanded));
    if(!expanded){free(items);goto end;}
    s->items=expanded;memcpy(s->items+s->count,items,(size_t)count*sizeof(*items));s->count+=count;free(items);ok=TRUE;
end:nj_free(&d);return ok;
}

BOOL JiraParseTransitions(const char *json,size_t length,JiraTransitions *out)
{
    memset(out,0,sizeof(*out));NJDoc d={0};if(!nj_parse(&d,json,length))return FALSE;
    int array=nj_get(&d,0,"transitions");BOOL ok=FALSE;
    if(array<0||d.tokens[array].kind!=NJ_ARRAY||d.tokens[array].count>JIRA_MAX_TRANSITIONS)goto end;
    int t=array+1;
    for(int i=0;i<d.tokens[array].count;i++){
        int to=nj_get(&d,t,"to"),cat=nj_get(&d,to,"statusCategory");JiraTransitionItem *item=&out->items[i];
        if(!field(&d,t,"id",item->id,80)||!field(&d,t,"name",item->name,128)||
           !field(&d,to,"name",item->status_name,128)||!item->id[0])goto end;
        if(!field(&d,cat,"key",item->category,32))wcscpy_s(item->category,32,L"new");
        int fields=nj_get(&d,t,"fields");
        if(fields>=0&&d.tokens[fields].kind==NJ_OBJECT){
            item->field_count=d.tokens[fields].count;
            int at=fields+1;
            for(int pair=0;pair<d.tokens[fields].count;pair++){
                int value=at+1;wchar_t field_key[80]=L"",field_name[128]=L"";
                if(at>=d.count||value>=d.count||!nj_string(&d,at,field_key,_countof(field_key)))goto end;
                field(&d,value,"name",field_name,_countof(field_name));
                if(!_wcsicmp(field_name,L"Оценка")||!_wcsicmp(field_name,L"Rating")){
                    int allowed=nj_get(&d,value,"allowedValues");
                    wcscpy_s(item->rating_field_id,_countof(item->rating_field_id),field_key);
                    if(allowed>=0&&d.tokens[allowed].kind==NJ_ARRAY){
                        int option=allowed+1,count=d.tokens[allowed].count;
                        if(count>JIRA_MAX_RATINGS)goto end;
                        for(int j=0;j<count;j++){
                            if(!field(&d,option,"id",item->ratings[j].id,_countof(item->ratings[j].id))||
                               !field(&d,option,"value",item->ratings[j].value,_countof(item->ratings[j].value)))goto end;
                            item->rating_count++;option=d.tokens[option].next;
                        }
                    }
                }
                at=d.tokens[value].next;
            }
        }
        out->count++;t=d.tokens[t].next;
    }
    ok=TRUE;
end:nj_free(&d);if(!ok)out->count=0;return ok;
}

int JiraParseInterval(const wchar_t *text)
{
    if(!text||!*text)return 0;int value=0;
    for(;*text;text++){if(*text<L'0'||*text>L'9')return 0;value=value*10+(*text-L'0');if(value>1440)return 0;}
    return value;
}

BOOL JiraGetUrl(wchar_t *url,size_t capacity)
{
    url[0]=0;PCREDENTIALW credential=NULL;
    if(CredReadW(TASK_MANAGER_CREDENTIAL,CRED_TYPE_GENERIC,0,&credential)){
        BOOL ok=credential->UserName&&wcslen(credential->UserName)<capacity;
        if(ok)wcscpy_s(url,capacity,credential->UserName);
        if(credential->CredentialBlob)SecureZeroMemory(credential->CredentialBlob,credential->CredentialBlobSize);
        CredFree(credential);return ok;
    }
    return FALSE;
}

static BOOL account_settings_path(wchar_t *path)
{
    wchar_t local[MAX_PATH];if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,local)))return FALSE;
    return TaskManagerMigrateSettingsAtRoot(local,path,MAX_PATH);
}
static BOOL account_signed_out(void)
{
    wchar_t path[MAX_PATH];return !account_settings_path(path)||GetPrivateProfileIntW(L"Jira",L"SignedOut",0,path)!=0;
}
BOOL JiraHasAccount(void)
{
    if(account_signed_out())return FALSE;
    PCREDENTIALW c=NULL;
    BOOL found=CredReadW(TASK_MANAGER_CREDENTIAL,CRED_TYPE_GENERIC,0,&c);
    if(c){if(c->CredentialBlob)SecureZeroMemory(c->CredentialBlob,c->CredentialBlobSize);CredFree(c);}return found;
}
BOOL JiraSignOut(void)
{
    wchar_t path[MAX_PATH];if(!account_settings_path(path)||!WritePrivateProfileStringW(L"Jira",L"SignedOut",L"1",path))return FALSE;
    return CredDeleteW(TASK_MANAGER_CREDENTIAL,CRED_TYPE_GENERIC,0)||GetLastError()==ERROR_NOT_FOUND;
}

BOOL JiraParseUrl(JiraConnection *c,const wchar_t *url)
{
    if(!url||!*url||wcslen(url)>512)return fail(c,L"Invalid Jira URL.");
    for(const wchar_t *p=url;*p;p++)if(*p<=32||*p==L'\\')return fail(c,L"Invalid Jira URL.");
    c->host[0]=c->prefix[0]=0;
    URL_COMPONENTS parts={0};parts.dwStructSize=sizeof(parts);parts.lpszHostName=c->host;parts.dwHostNameLength=256;
    parts.lpszUrlPath=c->prefix;parts.dwUrlPathLength=512;parts.dwUserNameLength=(DWORD)-1;parts.dwPasswordLength=(DWORD)-1;parts.dwExtraInfoLength=(DWORD)-1;
    if(!WinHttpCrackUrl(url,0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.dwUserNameLength||parts.dwPasswordLength||parts.dwExtraInfoLength)
        return fail(c,L"Jira URL must be HTTPS without credentials or query parameters.");
    c->port=parts.nPort;size_t n=wcslen(c->prefix);while(n&&c->prefix[n-1]==L'/')c->prefix[--n]=0;
    return c->host[0]!=0;
}

BOOL JiraConnectWithToken(JiraConnection *c,const wchar_t *url,const wchar_t *token)
{
    memset(c,0,sizeof(*c));if(!JiraParseUrl(c,url))return FALSE;
    size_t chars=token?wcslen(token):0;
    if(!chars||chars>1200)return fail(c,L"Invalid Jira token length.");
    for(size_t i=0;i<chars;i++)if(token[i]<33||token[i]>126)return fail(c,L"Invalid Jira token format.");
    c->token=calloc(chars+1,sizeof(wchar_t));if(!c->token)return fail(c,L"Not enough memory.");
    memcpy(c->token,token,(chars+1)*sizeof(wchar_t));c->token_chars=chars;
    c->session=WinHttpOpen(L"Task-Manager/0.9.20",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(c->session)WinHttpSetTimeouts(c->session,10000,10000,15000,15000);
    if(c->session)c->connection=WinHttpConnect(c->session,c->host,c->port,0);
    if(!c->connection){JiraDisconnect(c);return fail(c,L"Cannot initialize connection to Jira.");}
    return TRUE;
}

BOOL JiraSaveAccount(const wchar_t *url,const wchar_t *token)
{
    JiraConnection parsed={0};if(!JiraParseUrl(&parsed,url))return FALSE;
    size_t chars=wcslen(token);if(!chars||chars>1200)return FALSE;
    CREDENTIALW c={0};c.Type=CRED_TYPE_GENERIC;c.TargetName=TASK_MANAGER_CREDENTIAL;
    c.UserName=(wchar_t*)url;c.Persist=CRED_PERSIST_LOCAL_MACHINE;
    c.CredentialBlob=(LPBYTE)token;c.CredentialBlobSize=(DWORD)(chars*sizeof(wchar_t));
    if(!CredWriteW(&c,0))return FALSE;
    wchar_t path[MAX_PATH];return account_settings_path(path)&&WritePrivateProfileStringW(L"Jira",L"SignedOut",L"0",path);
}

BOOL JiraConnect(JiraConnection *c)
{
    memset(c,0,sizeof(*c));wchar_t url[768];
    if(account_signed_out())return fail(c,L"Signed out of Jira. Open Jira Task Manager settings to connect.");
    if(!JiraGetUrl(url,768))return fail(c,L"Configure Jira in Jira Task Manager settings.");
    PCREDENTIALW credential=NULL;
    if(!CredReadW(TASK_MANAGER_CREDENTIAL,CRED_TYPE_GENERIC,0,&credential))return fail(c,L"Jira token missing. Open Jira Task Manager settings.");
    size_t chars=credential->CredentialBlobSize/sizeof(wchar_t);
    BOOL valid=chars>0&&chars<8192&&credential->CredentialBlobSize%sizeof(wchar_t)==0;
    if(valid){
        c->token=calloc(chars+1,sizeof(wchar_t));
        if(c->token){memcpy(c->token,credential->CredentialBlob,credential->CredentialBlobSize);c->token_chars=chars;}
        else valid=FALSE;
    }
    if(credential->CredentialBlob&&credential->CredentialBlobSize)SecureZeroMemory(credential->CredentialBlob,credential->CredentialBlobSize);
    CredFree(credential);
    if(!valid)return fail(c,L"Invalid Jira token in Windows Credential Manager.");
    for(size_t i=0;i<chars;i++)if(c->token[i]<33||c->token[i]>126){JiraDisconnect(c);return fail(c,L"Invalid Jira token format.");}
    wchar_t *token=c->token;c->token=NULL;
    BOOL ok=JiraConnectWithToken(c,url,token);
    SecureZeroMemory(token,(chars+1)*sizeof(wchar_t));free(token);return ok;
}

void JiraDisconnect(JiraConnection *c)
{
    if(c->connection)WinHttpCloseHandle(c->connection);
    if(c->session)WinHttpCloseHandle(c->session);
    c->connection=c->session=NULL;
    if(c->token){SecureZeroMemory(c->token,(c->token_chars+1)*sizeof(wchar_t));free(c->token);c->token=NULL;c->token_chars=0;}
}

BOOL JiraCheckHttpStatus(JiraConnection *c,DWORD status)
{
    if(status>=200&&status<300)return TRUE;
    if(status==401)return fail(c,L"Jira rejected the token. Check the Jira Task Manager account settings.");
    if(status==403)return fail(c,L"Jira denied this operation (403).");
    swprintf(c->error,256,L"Jira returned HTTP %lu. No automatic retry was made.",status);return FALSE;
}

static BOOL request(JiraConnection *c,const wchar_t *method,const wchar_t *relative,const char *body,char **out,size_t *length)
{
    *out=NULL;*length=0;wchar_t path[2048];
    if(c->send)return c->send(c,method,relative,body,out,length);
    if(swprintf(path,2048,L"%ls%ls",c->prefix,relative)<0)return fail(c,L"Jira request path too long.");
    HINTERNET req=WinHttpOpenRequest(c->connection,method,path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!req)return fail(c,L"Cannot create Jira request.");
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    BOOL ok=WinHttpSetOption(req,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    size_t header_count=wcslen(c->token)+128;wchar_t *headers=calloc(header_count,sizeof(wchar_t));
    if(!headers){WinHttpCloseHandle(req);return fail(c,L"Not enough memory.");}
    swprintf(headers,header_count,L"Authorization: Bearer %ls\r\nAccept: application/json\r\nContent-Type: application/json\r\n",c->token);
    DWORD body_len=body?(DWORD)strlen(body):0;
    if(ok)ok=WinHttpSendRequest(req,headers,(DWORD)-1,(void*)body,body_len,body_len,0);
    SecureZeroMemory(headers,header_count*sizeof(wchar_t));free(headers);
    if(ok)ok=WinHttpReceiveResponse(req,NULL);
    if(!ok){WinHttpCloseHandle(req);return fail(c,L"Jira unavailable or request timed out. Check network/VPN. Do not repeat a status change before refreshing.");}
    DWORD status=0,size=sizeof(status);ok=WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&status,&size,NULL);
    if(!ok||!JiraCheckHttpStatus(c,status)){
        WinHttpCloseHandle(req);
        if(!ok)fail(c,L"Cannot read Jira response status.");return FALSE;
    }
    char *data=malloc(1);size_t used=0,cap=1;if(!data){WinHttpCloseHandle(req);return fail(c,L"Not enough memory.");}
    ULONGLONG began=GetTickCount64();
    for(;;){
        DWORD available=0,got=0;
        if(!WinHttpQueryDataAvailable(req,&available)){ok=FALSE;break;}
        if(!available)break;
        if(available>16*1024*1024-used||GetTickCount64()-began>30000){ok=FALSE;break;}
        if(used+available+1>cap){cap=used+available+1;char *next=realloc(data,cap);if(!next){ok=FALSE;break;}data=next;}
        if(!WinHttpReadData(req,data+used,available,&got)||!got){ok=FALSE;break;}
        used+=got;
    }
    WinHttpCloseHandle(req);
    if(!ok){free(data);return fail(c,L"Incomplete or oversized Jira response. Previous tasks retained.");}
    data[used]=0;*out=data;*length=used;return TRUE;
}

BOOL JiraVerifyAccount(JiraConnection *c,wchar_t *account,size_t capacity)
{
    char *data=NULL;size_t length=0;NJDoc d={0};
    if(!request(c,L"GET",L"/rest/api/2/myself",NULL,&data,&length))return FALSE;
    BOOL ok=nj_parse(&d,data,length)&&field(&d,0,"displayName",account,capacity);
    nj_free(&d);free(data);return ok?TRUE:fail(c,L"Unexpected Jira account response.");
}

static BOOL JiraLoadScope(JiraConnection *c,JiraSnapshot *result,BOOL include_comments,BOOL reporter)
{
    int offset=0,total=0;
    do{
        wchar_t path[512];swprintf(path,512,L"/rest/api/2/search?jql=%ls%%20%%3D%%20currentUser()%%20ORDER%%20BY%%20updated%%20DESC&startAt=%d&maxResults=100&fields=summary,status,updated%ls",
            reporter?L"reporter":L"assignee",offset,include_comments?L",comment":L"");
        char *data=NULL;size_t length=0;
        if(!request(c,L"GET",path,NULL,&data,&length))return FALSE;
        int start=-1,before=result->count;
        BOOL ok=JiraParsePage(data,length,result,&total,&start);free(data);
        if(!ok||start!=offset||(result->count==before&&offset<total)){
            return fail(c,L"Incomplete Jira task list. Previous tasks retained.");
        }
        for(int i=before;i<result->count;i++){
            result->items[i].assigned_to_me=!reporter;
            result->items[i].reported_by_me=reporter;
        }
        offset=result->count;
    }while(offset<total);
    /* updated sorting can move tasks between pages. Reject duplicates instead of publishing a corrupt list. */
    for(int i=0;i<result->count;i++)for(int j=0;j<i;j++)if(!_wcsicmp(result->items[i].key,result->items[j].key)){
        return fail(c,L"Jira list changed during loading. Refresh again.");
    }
    return TRUE;
}

BOOL JiraMergeReported(JiraSnapshot *assigned,const JiraSnapshot *reported)
{
    if(!assigned||!reported||assigned->count<0||reported->count<0)return FALSE;
    int unique=0;
    for(int i=0;i<reported->count;i++){
        BOOL found=FALSE;
        for(int j=0;j<assigned->count;j++)if(!_wcsicmp(assigned->items[j].key,reported->items[i].key)){found=TRUE;break;}
        if(!found&&++unique>JIRA_MAX_TASKS-assigned->count)return FALSE;
    }
    JiraItem *expanded=realloc(assigned->items,(size_t)(assigned->count+unique+1)*sizeof(*expanded));
    if(!expanded)return FALSE;assigned->items=expanded;
    for(int i=0;i<reported->count;i++){
        int match=-1;
        for(int j=0;j<assigned->count;j++)if(!_wcsicmp(assigned->items[j].key,reported->items[i].key)){match=j;break;}
        if(match>=0)assigned->items[match].reported_by_me=TRUE;
        else{
            assigned->items[assigned->count]=reported->items[i];
            assigned->items[assigned->count].assigned_to_me=FALSE;
            assigned->items[assigned->count].reported_by_me=TRUE;
            assigned->count++;
        }
    }
    return TRUE;
}

static BOOL JiraLoadAccount(JiraConnection *c,JiraSnapshot *result)
{
    char *data=NULL;size_t length=0;NJDoc d={0};
    if(!request(c,L"GET",L"/rest/api/2/myself",NULL,&data,&length))return FALSE;
    BOOL ok=nj_parse(&d,data,length)&&field(&d,0,"displayName",result->account,256);
    if(ok)user_identity(&d,0,&result->identity);
    nj_free(&d);free(data);return ok?TRUE:fail(c,L"Unexpected Jira account response.");
}

BOOL JiraLoadWithComments(JiraConnection *c,JiraSnapshot *snapshot,BOOL include_comments)
{
    JiraSnapshot result={0};
    if(!JiraLoadAccount(c,&result)||!JiraLoadScope(c,&result,include_comments,FALSE)){
        JiraFreeSnapshot(&result);return FALSE;
    }
    *snapshot=result;return TRUE;
}

BOOL JiraLoadWithReporter(JiraConnection *c,JiraSnapshot *snapshot,BOOL include_comments)
{
    JiraSnapshot assigned={0},reported={0};
    if(!JiraLoadAccount(c,&assigned)||!JiraLoadScope(c,&assigned,include_comments,FALSE)||
       !JiraLoadScope(c,&reported,include_comments,TRUE)){
        JiraFreeSnapshot(&assigned);JiraFreeSnapshot(&reported);return FALSE;
    }
    BOOL merged=JiraMergeReported(&assigned,&reported);JiraFreeSnapshot(&reported);
    if(!merged){JiraFreeSnapshot(&assigned);return fail(c,L"Too many Jira tasks or not enough memory.");}
    *snapshot=assigned;return TRUE;
}

BOOL JiraLoad(JiraConnection *c,JiraSnapshot *snapshot){return JiraLoadWithComments(c,snapshot,FALSE);}

static BOOL safe_segment(const wchar_t *text)
{
    if(!text||!text[0])return FALSE;
    for(;*text;text++)if(!((*text>=L'0'&&*text<=L'9')||(*text>=L'A'&&*text<=L'Z')||(*text>=L'a'&&*text<=L'z')||*text==L'-'||*text==L'_'))return FALSE;
    return TRUE;
}

BOOL JiraReadComments(JiraConnection *c,const wchar_t *key,JiraComments *comments)
{
    if(!comments||!safe_segment(key))return fail(c,L"Invalid Jira issue key.");
    JiraComments result={0};int offset=0,total=0;
    do{
        wchar_t path[320];swprintf(path,_countof(path),
            L"/rest/api/2/issue/%ls/comment?startAt=%d&maxResults=100",key,offset);
        char *data=NULL;size_t length=0;
        if(!request(c,L"GET",path,NULL,&data,&length)){JiraFreeComments(&result);return FALSE;}
        int start=-1,before=result.count;
        BOOL ok=JiraParseComments(data,length,&result,&total,&start);free(data);
        if(!ok||start!=offset||(result.count==before&&offset<total)){
            JiraFreeComments(&result);return fail(c,L"Incomplete Jira comments. Please reopen this task.");
        }
        offset=result.count;
    }while(offset<total);
    *comments=result;return TRUE;
}

BOOL JiraReadTransitions(JiraConnection *c,const wchar_t *key,JiraTransitions *transitions)
{
    if(!safe_segment(key))return fail(c,L"Invalid Jira issue key.");
    wchar_t path[320];swprintf(path,320,L"/rest/api/2/issue/%ls/transitions?expand=transitions.fields",key);
    char *data;size_t length;if(!request(c,L"GET",path,NULL,&data,&length))return FALSE;
    BOOL ok=JiraParseTransitions(data,length,transitions);free(data);
    return ok?TRUE:fail(c,L"Unexpected Jira transitions response.");
}

static BOOL json_escape_wide(const wchar_t *text,char **escaped,size_t *length)
{
    *escaped=NULL;*length=0;if(!text)text=L"";
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);if(bytes<=0)return FALSE;
    char *utf8=malloc((size_t)bytes);if(!utf8)return FALSE;
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,utf8,bytes,NULL,NULL)){free(utf8);return FALSE;}
    size_t capacity=(size_t)bytes*6+1,used=0;char *out=malloc(capacity);if(!out){free(utf8);return FALSE;}
    for(int i=0;i<bytes-1;i++){
        unsigned char ch=(unsigned char)utf8[i];
        if(ch==L'"'||ch==L'\\'){out[used++]='\\';out[used++]=(char)ch;}
        else if(ch==L'\b'){out[used++]='\\';out[used++]='b';}
        else if(ch==L'\f'){out[used++]='\\';out[used++]='f';}
        else if(ch==L'\n'){out[used++]='\\';out[used++]='n';}
        else if(ch==L'\r'){out[used++]='\\';out[used++]='r';}
        else if(ch==L'\t'){out[used++]='\\';out[used++]='t';}
        else if(ch<0x20){used+=(size_t)snprintf(out+used,capacity-used,"\\u%04x",ch);}
        else out[used++]=(char)ch;
    }
    out[used]=0;free(utf8);*escaped=out;*length=used;return TRUE;
}

BOOL JiraPostComment(JiraConnection *c,const wchar_t *key,const wchar_t *comment)
{
    if(!safe_segment(key)||!comment||!comment[0]||wcslen(comment)>2047)
        return fail(c,L"Invalid Jira comment.");
    char *escaped=NULL;size_t escaped_length=0;
    if(!json_escape_wide(comment,&escaped,&escaped_length))return fail(c,L"Invalid Jira comment text.");
    size_t capacity=escaped_length+32;char *body=malloc(capacity);
    if(!body){free(escaped);return fail(c,L"Not enough memory.");}
    int written=snprintf(body,capacity,"{\"body\":\"%s\"}",escaped);free(escaped);
    if(written<0||(size_t)written>=capacity){free(body);return fail(c,L"Invalid Jira comment text.");}
    wchar_t path[256];swprintf(path,_countof(path),L"/rest/api/2/issue/%ls/comment",key);
    c->write_attempted=TRUE;
    char *data=NULL;size_t length=0;BOOL ok=request(c,L"POST",path,body,&data,&length);
    free(body);free(data);return ok;
}

BOOL JiraBuildTransitionBody(const wchar_t *id,const wchar_t *field_id,const wchar_t *option_id,
    const wchar_t *comment,char **body,size_t *length)
{
    if(!body||!length){return FALSE;}*body=NULL;*length=0;
    BOOL has_field=field_id&&field_id[0],has_option=option_id&&option_id[0];
    if(!safe_segment(id)||has_field!=has_option||(has_field&&(!safe_segment(field_id)||!safe_segment(option_id))))return FALSE;
    char transition[80],field[80],option[80];
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,id,-1,transition,80,NULL,NULL))return FALSE;
    if(has_field&&(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,field_id,-1,field,80,NULL,NULL)||
       !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,option_id,-1,option,80,NULL,NULL)))return FALSE;
    char *escaped=NULL;size_t escaped_length=0;BOOL has_comment=comment&&comment[0];
    if(has_comment&&!json_escape_wide(comment,&escaped,&escaped_length))return FALSE;
    size_t capacity=512+escaped_length;char *result=malloc(capacity);if(!result){free(escaped);return FALSE;}
    int written;
    if(has_field&&has_comment)written=snprintf(result,capacity,"{\"transition\":{\"id\":\"%s\"},\"fields\":{\"%s\":{\"id\":\"%s\"}},\"update\":{\"comment\":[{\"add\":{\"body\":\"%s\"}}]}}",transition,field,option,escaped);
    else if(has_field)written=snprintf(result,capacity,"{\"transition\":{\"id\":\"%s\"},\"fields\":{\"%s\":{\"id\":\"%s\"}}}",transition,field,option);
    else if(has_comment)written=snprintf(result,capacity,"{\"transition\":{\"id\":\"%s\"},\"update\":{\"comment\":[{\"add\":{\"body\":\"%s\"}}]}}",transition,escaped);
    else written=snprintf(result,capacity,"{\"transition\":{\"id\":\"%s\"}}",transition);
    free(escaped);if(written<0||(size_t)written>=capacity){free(result);return FALSE;}
    *body=result;*length=(size_t)written;return TRUE;
}

BOOL JiraPostTransitionWithInput(JiraConnection *c,const wchar_t *key,const wchar_t *id,
    const wchar_t *field_id,const wchar_t *option_id,const wchar_t *comment)
{
    if(!safe_segment(key))return fail(c,L"Invalid Jira issue key.");
    wchar_t path[256];swprintf(path,256,L"/rest/api/2/issue/%ls/transitions",key);
    char *body=NULL;size_t body_length=0;
    if(!JiraBuildTransitionBody(id,field_id,option_id,comment,&body,&body_length))return fail(c,L"Invalid Jira transition input.");
    c->write_attempted=TRUE;
    char *data;size_t length;BOOL ok=request(c,L"POST",path,body,&data,&length);free(body);if(ok)free(data);return ok;
}

BOOL JiraPostTransition(JiraConnection *c,const wchar_t *key,const wchar_t *id)
{return JiraPostTransitionWithInput(c,key,id,NULL,NULL,NULL);}

static const wchar_t *destination(const JiraTransitions *ts,const wchar_t *name)
{for(int i=0;i<ts->count;i++)if(!_wcsicmp(ts->items[i].status_name,name))return ts->items[i].id;return NULL;}

BOOL JiraSwitch(JiraConnection *c,const wchar_t *key)
{
    JiraSnapshot current={0};if(!JiraLoad(c,&current))return FALSE;
    JiraItem *selected=NULL;for(int i=0;i<current.count;i++)if(!_wcsicmp(current.items[i].key,key))selected=&current.items[i];
    if(!selected){JiraFreeSnapshot(&current);return fail(c,L"Task no longer assigned to this account. Refresh the list.");}
    JiraTransitions transitions;BOOL ok=JiraReadTransitions(c,key,&transitions);
    const wchar_t *target=ok?destination(&transitions,!_wcsicmp(selected->status_name,L"In Progress")?L"On Hold":L"In Progress"):NULL;
    if(!target){JiraFreeSnapshot(&current);return ok?fail(c,L"Required Jira transition unavailable. Nothing changed."):FALSE;}
    wchar_t target_id[80];wcscpy_s(target_id,80,target);
    wchar_t (*holds)[80]=calloc((size_t)(current.count+1),sizeof(*holds));
    if(!holds){JiraFreeSnapshot(&current);return fail(c,L"Not enough memory.");}
    if(_wcsicmp(selected->status_name,L"In Progress")){
        for(int i=0;i<current.count;i++)if(_wcsicmp(current.items[i].key,key)&&!_wcsicmp(current.items[i].status_name,L"In Progress")){
            if(!JiraReadTransitions(c,current.items[i].key,&transitions)){ok=FALSE;break;}
            const wchar_t *hold=destination(&transitions,L"On Hold");
            if(!hold){ok=fail(c,L"An active task cannot move to On Hold. Nothing changed.");break;}
            wcscpy_s(holds[i],80,hold);
        }
    }
    if(ok)for(int i=0;i<current.count;i++)if(holds[i][0]&&!JiraPostTransition(c,current.items[i].key,holds[i])){ok=FALSE;break;}
    if(ok)ok=JiraPostTransition(c,key,target_id);
    free(holds);JiraFreeSnapshot(&current);return ok;
}
