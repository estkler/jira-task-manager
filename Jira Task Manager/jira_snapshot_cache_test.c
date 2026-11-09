#include "native_jira_cache.c"
#include <assert.h>
#include <stdio.h>

static void write_fixture(const wchar_t *path,BYTE *plain,DWORD length)
{
    DATA_BLOB input={length,plain},output={0},entropy=CacheEntropy();
    assert(CryptProtectData(&input,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output));
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD wrote;
    assert(file!=INVALID_HANDLE_VALUE&&WriteFile(file,output.pbData,output.cbData,&wrote,NULL)&&wrote==output.cbData);
    CloseHandle(file);LocalFree(output.pbData);
}
int main(void)
{
    wchar_t directory[MAX_PATH],settings[MAX_PATH];assert(GetTempPathW(MAX_PATH,directory));
    swprintf(settings,MAX_PATH,L"%lsjira-snapshot-%lu.ini",directory,GetCurrentProcessId());
    JiraConnection c={0};assert(JiraConfigureSnapshotCache(&c,settings));
    JiraItem item={0};wcscpy_s(item.key,80,L"A-1");wcscpy_s(item.id,80,L"10");wcscpy_s(item.updated,80,L"v1");
    wcscpy_s(item.summary,1024,L"PRIVATE-CACHE-MARKER");wcscpy_s(item.status_id,80,L"1");
    wcscpy_s(item.status_name,128,L"To Do");wcscpy_s(item.category,32,L"new");item.assigned_to_me=TRUE;
    JiraSnapshot scope={0},loaded={0};scope.items=&item;scope.count=1;scope.refresh_cache=TRUE;scope.credential_stamp=123;
    wcscpy_s(scope.account,256,L"Fixture User");wcscpy_s(scope.identity.key,256,L"fixture-user");wcscpy_s(scope.server,800,L"fixture.invalid:443");
    assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));
    assert(JiraSnapshotCacheSave(&c,&scope));
    assert(JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));
    assert(loaded.count==1&&!wcscmp(loaded.items[0].summary,item.summary)&&loaded.refresh_cache);JiraFreeSnapshot(&loaded);
    scope.credential_stamp++;assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));scope.credential_stamp--;
    scope.identity.key[0]=L'x';assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));scope.identity.key[0]=L'f';
    scope.server[0]=L'x';assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));scope.server[0]=L'f';
    assert(!JiraSnapshotCacheLoad(&c,&scope,L"hidden@example.invalid",&loaded));
    scope.credential_stamp=0;assert(!JiraSnapshotCacheSave(&c,&scope));scope.credential_stamp=123;
    HANDLE file=CreateFileW(c.snapshot_cache_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    DWORD size=GetFileSize(file,NULL),read;BYTE *bytes=malloc(size);assert(ReadFile(file,bytes,size,&read,NULL)&&read==size);CloseHandle(file);
    for(DWORD i=0;i+sizeof(L"PRIVATE-CACHE-MARKER")<=size;i++)assert(memcmp(bytes+i,L"PRIVATE-CACHE-MARKER",sizeof(L"PRIVATE-CACHE-MARKER")));
    DATA_BLOB encrypted={size,bytes},plain={0},entropy=CacheEntropy();assert(CryptUnprotectData(&encrypted,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain));
    CacheHeader *header=(CacheHeader*)plain.pbData;ULONGLONG saved=header->saved_at;
    header->saved_at-=JIRA_SNAPSHOT_CACHE_MAX_AGE+1;write_fixture(c.snapshot_cache_path,plain.pbData,plain.cbData);
    assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));header->saved_at=saved;
    header->version++;write_fixture(c.snapshot_cache_path,plain.pbData,plain.cbData);assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));header->version--;
    header->count=JIRA_MAX_TASKS+1;write_fixture(c.snapshot_cache_path,plain.pbData,plain.cbData);assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));header->count=1;
    JiraItem *bad=(JiraItem*)(plain.pbData+sizeof(*header));for(int i=0;i<80;i++)bad->key[i]=L'A';
    write_fixture(c.snapshot_cache_path,plain.pbData,plain.cbData);assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));
    SecureZeroMemory(plain.pbData,plain.cbData);LocalFree(plain.pbData);free(bytes);
    assert(JiraSnapshotCacheSave(&c,&scope));
    /* A failed replacement must preserve the previous valid file. */
    file=CreateFileW(c.snapshot_cache_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    item.summary[0]=L'X';assert(!JiraSnapshotCacheSave(&c,&scope));CloseHandle(file);
    assert(JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));assert(loaded.items[0].summary[0]==L'P');JiraFreeSnapshot(&loaded);
    file=CreateFileW(c.snapshot_cache_path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(WriteFile(file,"corrupt",7,&read,NULL));CloseHandle(file);assert(!JiraSnapshotCacheLoad(&c,&scope,L"",&loaded));
    assert(JiraSnapshotCacheRemove(settings));assert(GetFileAttributesW(c.snapshot_cache_path)==INVALID_FILE_ATTRIBUTES);
    puts("Snapshot cache: encrypted roundtrip, scope/credential/filter isolation, age/schema/bounds, corruption and atomic failure passed");
}
