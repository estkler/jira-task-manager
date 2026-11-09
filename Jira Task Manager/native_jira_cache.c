#define WIN32_LEAN_AND_MEAN
#include "native_jira_cache.h"
#include <wincrypt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Schema and item-size checks intentionally invalidate old layouts after a build change.
   No JiraSnapshot (which owns a pointer), token, notes or media is serialized. */
typedef struct CacheHeader {
    BYTE magic[8];DWORD version,item_size,count,reserved;
    ULONGLONG saved_at,credential_stamp;
    wchar_t account[256],server[800],comment_filter[2048];JiraUserIdentity identity;
} CacheHeader;
static const BYTE cache_magic[8]={'J','T','C','A','C','H','E',1};
static DATA_BLOB CacheEntropy(void)
{static BYTE entropy[]="JiraTasklet.TaskSnapshot.v1";return (DATA_BLOB){sizeof(entropy)-1,entropy};}
static ULONGLONG CacheNow(void)
{FILETIME time;GetSystemTimeAsFileTime(&time);return ((ULONGLONG)time.dwHighDateTime<<32)|time.dwLowDateTime;}
static BOOL CacheString(const wchar_t *text,size_t capacity)
{return wmemchr(text,0,capacity)!=NULL;}
#define CACHE_STRING(object,member) CacheString((object)->member,_countof((object)->member))
static BOOL CacheIdentity(const JiraUserIdentity *identity)
{return CACHE_STRING(identity,account_id)&&CACHE_STRING(identity,key)&&CACHE_STRING(identity,name);}
static BOOL CacheIdentityPresent(const JiraUserIdentity *identity)
{return CacheIdentity(identity)&&(identity->account_id[0]||identity->key[0]||identity->name[0]);}
static BOOL CacheSameIdentity(const JiraUserIdentity *a,const JiraUserIdentity *b)
{return !wcscmp(a->account_id,b->account_id)&&!wcscmp(a->key,b->key)&&!wcscmp(a->name,b->name);}
static BOOL CacheItemValid(const JiraItem *item)
{
    if(!CACHE_STRING(item,key)||!CACHE_STRING(item,summary)||!CACHE_STRING(item,status_id)||!CACHE_STRING(item,status_name)||
       !CACHE_STRING(item,category)||!CACHE_STRING(item,id)||!CACHE_STRING(item,updated)||!CACHE_STRING(item,latest_comment)||
       !CACHE_STRING(item,latest_comment_id)||!CACHE_STRING(item,latest_comment_author)||!CACHE_STRING(item,latest_comment_email)||
       !CACHE_STRING(item,latest_comment_created)||!CACHE_STRING(item,due_date)||!CacheIdentity(&item->latest_comment_identity))return FALSE;
    if(!item->key[0]||!item->summary[0]||!item->updated[0]||!item->status_id[0]||!item->status_name[0]||!item->category[0])return FALSE;
    size_t length=wcslen(item->id);if(!length||length>20)return FALSE;
    for(size_t i=0;i<length;i++)if(item->id[i]<L'0'||item->id[i]>L'9')return FALSE;
    return (item->assigned_to_me==FALSE||item->assigned_to_me==TRUE)&&(item->reported_by_me==FALSE||item->reported_by_me==TRUE);
}
static BOOL CacheScopeValid(const JiraSnapshot *scope)
{
    return scope&&scope->credential_stamp&&CACHE_STRING(scope,account)&&scope->account[0]&&CACHE_STRING(scope,server)&&scope->server[0]&&
        CACHE_STRING(scope,comment_filter)&&CacheIdentityPresent(&scope->identity);
}
static BOOL CachePath(const wchar_t *settings,wchar_t *out)
{
    if(!settings||!*settings||wcslen(settings)+wcslen(L".jira-cache.dat")>=MAX_PATH)return FALSE;
    swprintf(out,MAX_PATH,L"%ls.jira-cache.dat",settings);return TRUE;
}
BOOL JiraConfigureSnapshotCache(JiraConnection *client,const wchar_t *settings_path)
{if(!client)return FALSE;client->snapshot_cache_path[0]=0;return CachePath(settings_path,client->snapshot_cache_path);}
BOOL JiraSnapshotCacheRemove(const wchar_t *settings_path)
{
    wchar_t path[MAX_PATH];if(!CachePath(settings_path,path))return FALSE;
    return DeleteFileW(path)||GetLastError()==ERROR_FILE_NOT_FOUND;
}
BOOL JiraSnapshotCacheLoad(const JiraConnection *client,const JiraSnapshot *verified,const wchar_t *filter,JiraSnapshot *out)
{
    if(!client||!client->snapshot_cache_path[0]||!CacheScopeValid(verified)||!filter||!out||out->items||out->count)return FALSE;
    HANDLE file=CreateFileW(client->snapshot_cache_path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    LARGE_INTEGER size={0};BYTE *encrypted=NULL;DATA_BLOB plain={0};BOOL ok=FALSE;
    if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>JIRA_SNAPSHOT_CACHE_MAX_BYTES+65536ULL)goto done;
    encrypted=malloc((size_t)size.QuadPart);DWORD read=0;
    if(!encrypted||!ReadFile(file,encrypted,(DWORD)size.QuadPart,&read,NULL)||read!=(DWORD)size.QuadPart)goto done;
    DATA_BLOB source={(DWORD)size.QuadPart,encrypted},entropy=CacheEntropy();
    if(!CryptUnprotectData(&source,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain)||plain.cbData<sizeof(CacheHeader)||plain.cbData>JIRA_SNAPSHOT_CACHE_MAX_BYTES)goto done;
    CacheHeader *header=(CacheHeader*)plain.pbData;ULONGLONG now=CacheNow();
    if(memcmp(header->magic,cache_magic,8)||header->version!=1||header->item_size!=sizeof(JiraItem)||header->count>JIRA_MAX_TASKS||
       sizeof(*header)+(ULONGLONG)header->count*sizeof(JiraItem)!=plain.cbData||header->saved_at>now||now-header->saved_at>JIRA_SNAPSHOT_CACHE_MAX_AGE||
       !CACHE_STRING(header,account)||!CACHE_STRING(header,server)||!CACHE_STRING(header,comment_filter)||!CacheIdentityPresent(&header->identity)||
       header->credential_stamp!=verified->credential_stamp||wcscmp(header->server,verified->server)||wcscmp(header->comment_filter,filter)||
       !CacheSameIdentity(&header->identity,&verified->identity))goto done;
    JiraItem *items=(JiraItem*)(plain.pbData+sizeof(*header));
    for(DWORD i=0;i<header->count;i++)if(!CacheItemValid(&items[i]))goto done;
    JiraSnapshot result={0};if(header->count){result.items=malloc((size_t)header->count*sizeof(JiraItem));if(!result.items)goto done;
        memcpy(result.items,items,(size_t)header->count*sizeof(JiraItem));}
    result.count=(int)header->count;wcscpy_s(result.account,256,verified->account);result.identity=verified->identity;
    wcscpy_s(result.server,800,verified->server);wcscpy_s(result.comment_filter,2048,filter);
    result.credential_stamp=verified->credential_stamp;result.refresh_cache=TRUE;*out=result;ok=TRUE;
done:
    CloseHandle(file);free(encrypted);if(plain.pbData){SecureZeroMemory(plain.pbData,plain.cbData);LocalFree(plain.pbData);}return ok;
}
BOOL JiraSnapshotCacheSave(const JiraConnection *client,const JiraSnapshot *snapshot)
{
    if(!client||!client->snapshot_cache_path[0]||!CacheScopeValid(snapshot)||!snapshot->refresh_cache||snapshot->count<0||
       snapshot->count>JIRA_MAX_TASKS||(snapshot->count&&!snapshot->items))return FALSE;
    size_t bytes=sizeof(CacheHeader)+(size_t)snapshot->count*sizeof(JiraItem);if(bytes>JIRA_SNAPSHOT_CACHE_MAX_BYTES)return FALSE;
    for(int i=0;i<snapshot->count;i++)if(!CacheItemValid(&snapshot->items[i]))return FALSE;
    BYTE *plain=calloc(1,bytes);if(!plain)return FALSE;CacheHeader *header=(CacheHeader*)plain;
    memcpy(header->magic,cache_magic,8);header->version=1;header->item_size=sizeof(JiraItem);header->count=(DWORD)snapshot->count;
    header->saved_at=CacheNow();header->credential_stamp=snapshot->credential_stamp;header->identity=snapshot->identity;
    wcscpy_s(header->account,256,snapshot->account);wcscpy_s(header->server,800,snapshot->server);wcscpy_s(header->comment_filter,2048,snapshot->comment_filter);
    if(snapshot->count)memcpy(plain+sizeof(*header),snapshot->items,(size_t)snapshot->count*sizeof(JiraItem));
    DATA_BLOB input={(DWORD)bytes,plain},encrypted={0},entropy=CacheEntropy();
    BOOL protected=CryptProtectData(&input,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&encrypted);
    SecureZeroMemory(plain,bytes);free(plain);if(!protected)return FALSE;
    static volatile LONG sequence;wchar_t temporary[MAX_PATH+64];
    swprintf(temporary,_countof(temporary),L"%ls.%lu.%lu.tmp",client->snapshot_cache_path,GetCurrentProcessId(),(DWORD)InterlockedIncrement(&sequence));
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);BOOL ok=FALSE;
    if(file!=INVALID_HANDLE_VALUE){DWORD wrote=0;ok=WriteFile(file,encrypted.pbData,encrypted.cbData,&wrote,NULL)&&wrote==encrypted.cbData&&FlushFileBuffers(file);
        CloseHandle(file);if(ok)ok=MoveFileExW(temporary,client->snapshot_cache_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        if(!ok)DeleteFileW(temporary);}
    LocalFree(encrypted.pbData);return ok;
}
