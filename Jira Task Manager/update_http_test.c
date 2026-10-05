#include "native_update_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Fixture {DWORD status;const wchar_t *redirect;const char *body;size_t at;BOOL length;ULONGLONG declared;int calls,reads,closes,redirects;BOOL read_error;UpdateCancel *cancel;} Fixture;
static UpdateStatus open_fixture(void *ctx,const wchar_t *url,const wchar_t *accept,ULONGLONG deadline,UpdateHttpResponse *out){
    Fixture *f=ctx;assert(UpdateUrlAllowed(url));assert(deadline>GetTickCount64());assert(!wcscmp(accept,L"application/json")||!wcscmp(accept,L"application/octet-stream"));
    if(f->status==404)assert(!wcscmp(url,L"https://api.github.com/repos/estkler/jira-task-manager/releases/latest"));
    if(!wcscmp(accept,L"application/octet-stream"))assert(!wcscmp(url,L"https://api.github.com/repos/estkler/jira-task-manager/releases/assets/4294967296"));
    f->calls++;f->at=0;*out=(UpdateHttpResponse){.status=f->status,.has_length=f->length,.length=f->declared,.request=f};
    if(f->redirect)wcscpy_s(out->location,_countof(out->location),f->redirect);
    if(f->calls<=f->redirects){out->status=302;swprintf(out->location,_countof(out->location),L"/step%d",f->calls);}return UPDATE_OK;
}
static BOOL read_fixture(void *request,BYTE *out,DWORD cap,DWORD *got){Fixture *f=request;f->reads++;if(f->cancel)InterlockedExchange(&f->cancel->cancelled,1);if(f->read_error)return FALSE;size_t n=strlen(f->body)-f->at;if(n>cap)n=cap;memcpy(out,f->body+f->at,n);f->at+=n;*got=(DWORD)n;return TRUE;}
static void close_fixture(void *request){((Fixture*)request)->closes++;}
static const UpdateTransport transport={open_fixture,read_fixture,close_fixture};
static BOOL sink(const BYTE *data,DWORD n,void *ctx){*(DWORD*)ctx+=n;return TRUE;}
static BOOL failing_sink(const BYTE *data,DWORD n,void *ctx){return FALSE;}
static void test_check_no_release(void){UpdateRelease r;UpdateCancel c={0};Fixture f={.status=404,.body=""};assert(UpdateCheckUsing((UpdateVersion){0,9,29},&c,&r,&transport,&f)==UPDATE_NONE);assert(f.calls==1&&f.closes==1);f.status=403;assert(UpdateCheckUsing((UpdateVersion){0,9,29},&c,&r,&transport,&f)==UPDATE_NETWORK);f.status=500;assert(UpdateCheckUsing((UpdateVersion){0,9,29},&c,&r,&transport,&f)==UPDATE_NETWORK);f.status=200;f.body="{";assert(UpdateCheckUsing((UpdateVersion){0,9,29},&c,&r,&transport,&f)==UPDATE_INVALID);}
static void test_transport_policy(void){
    const wchar_t *bad[]={L"http://github.com/x",L"https://evil/x",L"https://user@github.com/x",L"https://github.com:444/x",L"https://github.com.evil/x"};UpdateCancel c={0};DWORD code,n=0;
    for(int i=0;i<_countof(bad);i++){Fixture f={.status=302,.redirect=bad[i],.body=""};assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&f)==UPDATE_INVALID);assert(f.calls==1&&f.closes==1);}
    Fixture loop={.status=302,.redirect=L"/a",.body=""};assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&loop)==UPDATE_INVALID);assert(loop.calls<=6&&loop.calls==loop.closes);
    Fixture hops={.status=200,.body="abc",.redirects=5};assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&hops)==UPDATE_OK);assert(hops.calls==6&&hops.closes==6);
    hops.calls=hops.closes=0;hops.redirects=6;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&hops)==UPDATE_INVALID);assert(hops.calls==6&&hops.closes==6);
    Fixture f={.status=200,.body="abc",.length=TRUE,.declared=4};assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&f)==UPDATE_INVALID);
    f.declared=1025;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&f)==UPDATE_INVALID);
    f.length=FALSE;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",2,&c,sink,&n,&code,&transport,&f)==UPDATE_INVALID);
    assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,failing_sink,&n,&code,&transport,&f)==UPDATE_IO);
    f.read_error=TRUE;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",1024,&c,sink,&n,&code,&transport,&f)==UPDATE_NETWORK);
}
static void test_cancel(void){UpdateCancel c={1};Fixture f={.status=200,.body="abc"};DWORD code,n=0;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",100,&c,sink,&n,&code,&transport,&f)==UPDATE_CANCELLED);assert(f.calls==0);c.cancelled=0;f.cancel=&c;assert(UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",100,&c,sink,&n,&code,&transport,&f)==UPDATE_CANCELLED);}
static void test_download_size(void){
    wchar_t temp[MAX_PATH],path[MAX_PATH];GetTempPathW(MAX_PATH,temp);assert(GetTempFileNameW(temp,L"jut",0,path));HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);assert(file!=INVALID_HANDLE_VALUE);
    Fixture f={.status=200,.body="abc"};UpdateCancel c={0};UpdateRelease r={.asset_id=4294967296ull,.size=3};assert(UpdateDownloadUsing(&r,file,&c,&transport,&f)==UPDATE_OK);DWORD n;char bytes[4]={0};SetFilePointer(file,0,NULL,FILE_BEGIN);assert(ReadFile(file,bytes,3,&n,NULL));assert(n==3&&!strcmp(bytes,"abc"));r.size=4;assert(UpdateDownloadUsing(&r,file,&c,&transport,&f)==UPDATE_INVALID);CloseHandle(file);
}
int main(void){test_check_no_release();test_transport_policy();test_cancel();test_download_size();puts("update_http_test: PASS");return 0;}
