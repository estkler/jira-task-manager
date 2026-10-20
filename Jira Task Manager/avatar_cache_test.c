#include "native_jira.h"
#include "image_fixture.h"
#include <assert.h>
#include <stdio.h>
static int downloads;static BOOL transfer_failed,corrupt_response,svg_response;
static const wchar_t *avatar=L"https://jira.example.test/jira/secure/useravatar?avatarId=10122";
static wchar_t fixture_settings[MAX_PATH];static HANDLE simultaneous;
static BOOL transport(JiraConnection *client,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *size)
{
    (void)client;assert(!wcscmp(method,L"GET")&&!body&&!wcscmp(path,L"/secure/useravatar?avatarId=10122"));downloads++;
    if(transfer_failed)return FALSE;
    if(corrupt_response){*out=malloc(7);memcpy(*out,"corrupt",7);*size=7;return TRUE;}
    if(svg_response){const char *svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48'><style>.avatar{fill:#ffab00}</style><path class='avatar' d='M0 0h48v48H0z'/></svg>";*size=strlen(svg);*out=malloc(*size);memcpy(*out,svg,*size);return TRUE;}
    *out=(char*)ImageFixtureBytes(48,48,size);return *out!=NULL;
}
static void setup(JiraConnection *client,const wchar_t *settings,const wchar_t *account)
{
    memset(client,0,sizeof(*client));wcscpy_s(client->host,256,L"jira.example.test");wcscpy_s(client->prefix,512,L"/jira");client->port=443;client->send=transport;
    assert(JiraConfigureAvatarCache(client,settings,account));
}
static void load(JiraConnection *client,BOOL expected)
{
    JiraComments comments={0};comments.items=calloc(1,sizeof(JiraComment));comments.count=1;wcscpy_s(comments.items[0].avatar_url,512,avatar);
    JiraLoadCommentAuthorMedia(client,&comments);assert((comments.items[0].avatar!=NULL)==expected);JiraFreeComments(&comments);
}
static void expire(const wchar_t *root)
{
    wchar_t pattern[MAX_PATH],path[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\*.avatar",root);WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);
    do{swprintf(path,MAX_PATH,L"%ls\\%ls",root,found.cFileName);HANDLE file=CreateFileW(path,FILE_WRITE_ATTRIBUTES,0,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);FILETIME old={1234,1};assert(SetFileTime(file,NULL,NULL,&old));CloseHandle(file);}while(FindNextFileW(search,&found));FindClose(search);
}
static void corrupt_cache(const wchar_t *root)
{
    wchar_t pattern[MAX_PATH],path[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\*.avatar",root);WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);
    do{swprintf(path,MAX_PATH,L"%ls\\%ls",root,found.cFileName);HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);SetFilePointer(file,44,NULL,FILE_BEGIN);DWORD count=0;BYTE junk[6]={0};assert(WriteFile(file,junk,sizeof(junk),&count,NULL)&&count==sizeof(junk));CloseHandle(file);}while(FindNextFileW(search,&found));FindClose(search);
}
static DWORD WINAPI concurrent_load(void *unused)
{(void)unused;JiraConnection client;setup(&client,fixture_settings,L"concurrent-account");assert(WaitForSingleObject(simultaneous,5000)==WAIT_OBJECT_0);load(&client,TRUE);return 0;}
static void cleanup(const wchar_t *directory,const wchar_t *root)
{
    wchar_t pattern[MAX_PATH],path[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\*",root);WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);
    if(search!=INVALID_HANDLE_VALUE){do{if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;swprintf(path,MAX_PATH,L"%ls\\%ls",root,found.cFileName);assert(DeleteFileW(path));}while(FindNextFileW(search,&found));FindClose(search);}assert(RemoveDirectoryW(root));DeleteFileW(fixture_settings);assert(RemoveDirectoryW(directory));
}
int main(void)
{
    wchar_t directory[MAX_PATH],name[80];swprintf(name,80,L"bin/avatar-cache-test-%lu-%llu",GetCurrentProcessId(),GetTickCount64());assert(GetFullPathNameW(name,MAX_PATH,directory,NULL));
    assert(CreateDirectoryW(directory,NULL));swprintf(fixture_settings,MAX_PATH,L"%ls/settings.ini",directory);
    /* Missing disk persistence causes a second feed to redownload a fresh avatar. */
    JiraConnection first;setup(&first,fixture_settings,L"account-1");load(&first,TRUE);assert(downloads==1);
    JiraConnection reopened;setup(&reopened,fixture_settings,L"account-1");load(&reopened,TRUE);assert(downloads==1);
    JiraConnection other;setup(&other,fixture_settings,L"account-2");load(&other,TRUE);assert(downloads==2);
    wcscpy_s(other.host,256,L"other.example.test");assert(JiraConfigureAvatarCache(&other,fixture_settings,L"account-1"));avatar=L"https://other.example.test/jira/secure/useravatar?avatarId=10122";load(&other,TRUE);assert(downloads==3);
    other.port=444;assert(JiraConfigureAvatarCache(&other,fixture_settings,L"account-1"));avatar=L"https://other.example.test:444/jira/secure/useravatar?avatarId=10122";load(&other,TRUE);assert(downloads==4);
    wcscpy_s(other.prefix,512,L"/another");assert(JiraConfigureAvatarCache(&other,fixture_settings,L"account-1"));avatar=L"https://other.example.test:444/another/secure/useravatar?avatarId=10122";load(&other,TRUE);assert(downloads==5);
    avatar=L"https://jira.example.test/jira/secure/useravatar?avatarId=10122";
    expire(first.avatar_cache.root);transfer_failed=TRUE;load(&first,TRUE);assert(downloads==6);transfer_failed=FALSE;
    corrupt_response=TRUE;load(&first,TRUE);assert(downloads==7);corrupt_response=FALSE;load(&first,TRUE);assert(downloads==8);load(&first,TRUE);assert(downloads==8);
    corrupt_cache(first.avatar_cache.root);transfer_failed=TRUE;load(&first,FALSE);assert(downloads==9);transfer_failed=FALSE;load(&first,TRUE);assert(downloads==10);
    simultaneous=CreateEventW(NULL,TRUE,FALSE,NULL);assert(simultaneous);HANDLE workers[4];for(int i=0;i<4;i++){workers[i]=CreateThread(NULL,0,concurrent_load,NULL,0,NULL);assert(workers[i]);}SetEvent(simultaneous);assert(WaitForMultipleObjects(4,workers,TRUE,10000)==WAIT_OBJECT_0);for(int i=0;i<4;i++)CloseHandle(workers[i]);CloseHandle(simultaneous);assert(downloads==11);
    const wchar_t *unsafe[]={L"https://evil.test/secure/useravatar?avatarId=10122",L"https://jira.example.test/jira/rest/api/2/myself",L"https://jira.example.test/jira/secure/useravatar/../../logout"};
    for(int i=0;i<3;i++){avatar=unsafe[i];load(&first,FALSE);assert(downloads==11);}
    avatar=L"https://jira.example.test/jira/secure/useravatar?avatarId=10122";volatile LONG cancelled=1;first.cancelled=&cancelled;load(&first,TRUE);assert(downloads==11);first.cancelled=NULL;
    assert(NativeAvatarCacheDays(fixture_settings)==7);assert(NativeAvatarCacheSaveDays(fixture_settings,14));assert(NativeAvatarCacheDays(fixture_settings)==14);assert(!NativeAvatarCacheSaveDays(fixture_settings,0)&&!NativeAvatarCacheSaveDays(fixture_settings,366));
    assert(WritePrivateProfileStringW(L"Advanced",L"AvatarCacheDays",L"8days",fixture_settings));assert(NativeAvatarCacheDays(fixture_settings)==7);
    svg_response=TRUE;setup(&other,fixture_settings,L"svg-account");load(&other,TRUE);assert(downloads==12);setup(&other,fixture_settings,L"svg-account");load(&other,TRUE);assert(downloads==12);svg_response=FALSE;
    wchar_t orphan[MAX_PATH];swprintf(orphan,MAX_PATH,L"%ls\\0-0000000000000000.tmp",first.avatar_cache.root);HANDLE orphan_file=CreateFileW(orphan,GENERIC_WRITE|FILE_WRITE_ATTRIBUTES,0,NULL,CREATE_NEW,0,NULL);assert(orphan_file!=INVALID_HANDLE_VALUE);FILETIME old={1234,1};assert(SetFileTime(orphan_file,NULL,NULL,&old));CloseHandle(orphan_file);
    size_t length=0;unsigned char *fixture=ImageFixtureBytes(48,48,&length);NativeImage *image=NativeImageCreateSized(fixture,length,48,48);assert(image);
    assert(NativeAvatarCacheWrite(&first.avatar_cache,L"/secure/useravatar?avatarId=cleanup",image));assert(GetFileAttributesW(orphan)==INVALID_FILE_ATTRIBUTES);
    for(int i=0;i<270;i++){wchar_t relative[80];swprintf(relative,80,L"/secure/useravatar?avatarId=%d",i);assert(NativeAvatarCacheWrite(&first.avatar_cache,relative,image));}
    NativeImageRelease(image);wchar_t pattern[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\*.avatar",first.avatar_cache.root);WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);int count=0;ULONGLONG size=0;do{count++;size+=found.nFileSizeLow;assert(!found.nFileSizeHigh);}while(FindNextFileW(search,&found));FindClose(search);assert(count==256&&size<=NATIVE_AVATAR_CACHE_MAX_BYTES);
    fixture=ImageFixtureBytes(48,48,&length);fixture=realloc(fixture,NATIVE_AVATAR_MAX_BYTES);assert(fixture);memset(fixture+length,0,NATIVE_AVATAR_MAX_BYTES-length);image=NativeImageCreateSized(fixture,NATIVE_AVATAR_MAX_BYTES,48,48);assert(image);
    for(int i=0;i<270;i++){wchar_t relative[80];swprintf(relative,80,L"/secure/useravatar?avatarId=%d",i);assert(NativeAvatarCacheWrite(&first.avatar_cache,relative,image));}NativeImageRelease(image);
    search=FindFirstFileW(pattern,&found);assert(search!=INVALID_HANDLE_VALUE);count=0;size=0;do{count++;size+=found.nFileSizeLow;assert(!found.nFileSizeHigh);}while(FindNextFileW(search,&found));FindClose(search);assert(count<256&&size<=NATIVE_AVATAR_CACHE_MAX_BYTES);
    assert(NativeImageMemoryBytes()==0);cleanup(directory,first.avatar_cache.root);
    puts("Avatar cache: cross-feed reuse, server/account/port/prefix isolation, stale fallback, corruption guards, concurrent refresh, trusted routes, TTL and disk bounds passed");
}
