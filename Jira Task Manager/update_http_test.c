#include "native_update_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <winhttp.h>
/* Exercise the production WinHTTP phase sequencing, with only external I/O
   replaced. Missing deadline/cancel checks must prevent header interpretation. */
typedef struct FakeRequest {WINHTTP_STATUS_CALLBACK callback;DWORD_PTR context;int hop;} FakeRequest;
static FakeRequest fake_requests[8];
static ULONGLONG fake_now;static BOOL fake_clock,defer_close;static int fake_mode,fake_sends,fake_receives,fake_queries,fake_closes;static UpdateCancel *fake_cancel;
static ULONGLONG test_tick(void){return fake_clock?fake_now:GetTickCount64();}
static HINTERNET test_open(LPCWSTR a,DWORD b,LPCWSTR c,LPCWSTR d,DWORD flags){return (HINTERNET)1;}
static HINTERNET test_connect(HINTERNET a,LPCWSTR b,INTERNET_PORT c,DWORD d){return (HINTERNET)2;}
static HINTERNET test_request(HINTERNET a,LPCWSTR b,LPCWSTR c,LPCWSTR d,LPCWSTR e,LPCWSTR *f,DWORD g){FakeRequest *r=&fake_requests[fake_sends];memset(r,0,sizeof(*r));r->hop=fake_sends;return r;}
static BOOL test_timeouts(HINTERNET h,int a,int b,int c,int d){return TRUE;}
static BOOL test_option(HINTERNET h,DWORD option,LPVOID data,DWORD size){if(option==WINHTTP_OPTION_CONTEXT_VALUE)((FakeRequest*)h)->context=*(DWORD_PTR*)data;return TRUE;}
static WINHTTP_STATUS_CALLBACK test_callback(HINTERNET h,WINHTTP_STATUS_CALLBACK cb,DWORD flags,DWORD reserved){((FakeRequest*)h)->callback=cb;return NULL;}
static void complete(FakeRequest *r,DWORD status,DWORD length){if(r->callback)r->callback(r,r->context,status,NULL,length);}
static BOOL test_send(HINTERNET h,LPCWSTR a,DWORD b,LPVOID c,DWORD d,DWORD e,DWORD_PTR context){FakeRequest *r=h;fake_sends++;if(context)r->context=context;if(fake_mode==1)fake_now+=120000;if(fake_mode!=9)complete(r,WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,0);return TRUE;}
static BOOL test_receive(HINTERNET h,LPVOID unused){FakeRequest *r=h;fake_receives++;if(fake_mode==2)fake_now+=120000;if(fake_mode==3)InterlockedExchange(&fake_cancel->cancelled,1);if(fake_mode==4)fake_now+=r->hop?10:119995;if(fake_mode!=8)complete(r,WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,0);return TRUE;}
static BOOL test_headers(HINTERNET h,DWORD info,LPCWSTR a,LPVOID out,LPDWORD size,LPDWORD index){FakeRequest *r=h;fake_queries++;if(info==(WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER)){*(DWORD*)out=fake_mode==4&&!r->hop?302:404;return TRUE;}if(info==WINHTTP_QUERY_LOCATION){wcscpy_s(out,*size/sizeof(wchar_t),L"/next");return TRUE;}SetLastError(ERROR_WINHTTP_HEADER_NOT_FOUND);return FALSE;}
static BOOL test_close(HINTERNET h){if(h!=(HINTERNET)1&&h!=(HINTERNET)2){fake_closes++;if(!defer_close)complete(h,WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING,0);}return TRUE;}
static BOOL test_read(HINTERNET h,LPVOID data,DWORD size,LPDWORD got){if(got)*got=0;complete(h,WINHTTP_CALLBACK_STATUS_READ_COMPLETE,0);return TRUE;}
#define GetTickCount64 test_tick
#define WinHttpOpen test_open
#define WinHttpConnect test_connect
#define WinHttpOpenRequest test_request
#define WinHttpSetTimeouts test_timeouts
#define WinHttpSetOption test_option
#define WinHttpSetStatusCallback test_callback
#define WinHttpSendRequest test_send
#define WinHttpReceiveResponse test_receive
#define WinHttpQueryHeaders test_headers
#define WinHttpCloseHandle test_close
#define WinHttpReadData test_read
#include "native_update_http.c"
#undef GetTickCount64
typedef struct Fixture {DWORD status;const wchar_t *redirect;const char *body;size_t at;BOOL length;ULONGLONG declared;int calls,reads,closes,redirects;BOOL read_error;UpdateCancel *cancel;} Fixture;
static UpdateStatus open_fixture(void *ctx,const wchar_t *url,const wchar_t *accept,ULONGLONG deadline,UpdateCancel *cancel,UpdateHttpResponse *out){
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
static void test_opening_deadline_and_cancel(void){
    for(int mode=1;mode<=4;mode++){
        fake_clock=TRUE;fake_now=1000;fake_mode=mode;fake_sends=fake_receives=fake_queries=fake_closes=0;UpdateCancel c={0};fake_cancel=&c;DWORD code,n=0;
        UpdateStatus status=UpdateHttpGetUsing(L"https://api.github.com/a",L"application/json",100,&c,sink,&n,&code,&windows_transport,NULL);
        assert(status==(mode==3?UPDATE_CANCELLED:UPDATE_NETWORK));assert(n==0);assert(fake_closes==fake_sends);
        if(mode==1)assert(fake_receives==0);if(mode==2||mode==3)assert(fake_queries==0);if(mode==4)assert(fake_sends==2&&fake_queries==3);
    }
    fake_clock=FALSE;
}
static DWORD WINAPI cancel_later(void *context){Sleep(20);InterlockedExchange(&((UpdateCancel*)context)->cancelled,1);return 0;}
static void test_pending_phase_lifetime(void){
    DWORD handles_before,handles_after;assert(GetProcessHandleCount(GetCurrentProcess(),&handles_before));
    for(int mode=8;mode<=9;mode++)for(int cancel=0;cancel<=1;cancel++){
        fake_mode=mode;fake_sends=fake_receives=fake_queries=fake_closes=0;defer_close=TRUE;UpdateCancel c={0};UpdateHttpResponse out={0};
        HANDLE thread=cancel?CreateThread(NULL,0,cancel_later,&c,0,NULL):NULL;ULONGLONG start=GetTickCount64();
        assert(win_open(NULL,L"https://api.github.com/a",L"application/json",start+(cancel?5000:20),&c,&out)==(cancel?UPDATE_CANCELLED:UPDATE_NETWORK));
        assert(GetTickCount64()-start<1000&&!out.request&&fake_queries==0&&fake_closes==1);if(mode==9)assert(fake_receives==0);
        /* Delayed final callback must still own the event/read buffer after
           the caller has returned; it releases the final reference itself. */
        WinRequest *pending=(WinRequest*)fake_requests[0].context;assert(WaitForSingleObject(pending->completed,0)==WAIT_TIMEOUT);
        complete(&fake_requests[0],WINHTTP_CALLBACK_STATUS_REQUEST_ERROR,0);complete(&fake_requests[0],WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING,0);
        if(thread){assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0);CloseHandle(thread);}defer_close=FALSE;
    }
    assert(GetProcessHandleCount(GetCurrentProcess(),&handles_after)&&handles_before==handles_after);
}
int main(void){test_check_no_release();test_transport_policy();test_cancel();test_download_size();test_opening_deadline_and_cancel();test_pending_phase_lifetime();puts("update_http_test: PASS");return 0;}
