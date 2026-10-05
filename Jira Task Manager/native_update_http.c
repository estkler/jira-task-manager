#include "native_update_transport.h"
#include <winhttp.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>

typedef struct WinRequest {HINTERNET session,connection,request;ULONGLONG deadline;} WinRequest;
static BOOL cancelled(UpdateCancel *c){return c&&InterlockedCompareExchange(&c->cancelled,0,0)!=0;}
static void win_close(void *value){WinRequest *r=value;if(!r)return;if(r->request)WinHttpCloseHandle(r->request);if(r->connection)WinHttpCloseHandle(r->connection);if(r->session)WinHttpCloseHandle(r->session);free(r);}
static UpdateStatus win_open(void *context,const wchar_t *url,const wchar_t *accept,ULONGLONG deadline,UpdateHttpResponse *out){
    (void)context;wchar_t host[256],path[8193],extra[8193],target[8193],headers[256];
    URL_COMPONENTS parts={sizeof(parts)};parts.lpszHostName=host;parts.dwHostNameLength=_countof(host);parts.lpszUrlPath=path;parts.dwUrlPathLength=_countof(path);parts.lpszExtraInfo=extra;parts.dwExtraInfoLength=_countof(extra);
    if(!WinHttpCrackUrl(url,0,0,&parts)||parts.dwHostNameLength>=_countof(host)||parts.dwUrlPathLength>=_countof(path)||parts.dwExtraInfoLength>=_countof(extra))return UPDATE_INVALID;
    host[parts.dwHostNameLength]=path[parts.dwUrlPathLength]=extra[parts.dwExtraInfoLength]=0;
    if(swprintf(target,_countof(target),L"%ls%ls",path[0]?path:L"/",extra)<0||
       swprintf(headers,_countof(headers),L"Accept: %ls\r\nX-GitHub-Api-Version: 2022-11-28\r\n",accept)<0)return UPDATE_INVALID;
    WinRequest *r=calloc(1,sizeof(*r));if(!r)return UPDATE_IO;r->deadline=deadline;
    r->session=WinHttpOpen(L"Jira-Task-Manager-Updates",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!r->session)goto fail;
    if(!WinHttpSetTimeouts(r->session,10000,10000,15000,15000))goto fail;
    r->connection=WinHttpConnect(r->session,host,443,0);if(!r->connection)goto fail;
    r->request=WinHttpOpenRequest(r->connection,L"GET",target,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!r->request)goto fail;
    DWORD disable=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
    if(!WinHttpSetOption(r->request,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable))||
       !WinHttpSendRequest(r->request,headers,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(r->request,NULL))goto fail;
    DWORD size=sizeof(out->status);if(!WinHttpQueryHeaders(r->request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&out->status,&size,NULL))goto fail;
    if(out->status>=300&&out->status<400){size=sizeof(out->location);if(!WinHttpQueryHeaders(r->request,WINHTTP_QUERY_LOCATION,NULL,out->location,&size,NULL))goto fail;}
    wchar_t length[32];size=sizeof(length);
    if(WinHttpQueryHeaders(r->request,WINHTTP_QUERY_CONTENT_LENGTH,NULL,length,&size,NULL)){
        ULONGLONG n=0;if(!length[0])goto invalid;
        for(const wchar_t *p=length;*p;p++){unsigned digit=*p-L'0';if(digit>9||n>(~0ull-digit)/10)goto invalid;n=n*10+digit;}
        out->has_length=TRUE;out->length=n;
    }else if(GetLastError()!=ERROR_WINHTTP_HEADER_NOT_FOUND)goto fail;
    out->request=r;return UPDATE_OK;
invalid:win_close(r);return UPDATE_INVALID;
fail:win_close(r);return UPDATE_NETWORK;
}
static BOOL win_read(void *value,BYTE *bytes,DWORD capacity,DWORD *got){
    WinRequest *r=value;ULONGLONG now=GetTickCount64();if(now>=r->deadline)return FALSE;
    int timeout=(int)min(15000ull,r->deadline-now);
    if(!WinHttpSetTimeouts(r->request,10000,10000,timeout,timeout))return FALSE;
    return WinHttpReadData(r->request,bytes,capacity,got);
}
static const UpdateTransport windows_transport={win_open,win_read,win_close};
UpdateStatus UpdateHttpGetUsing(const wchar_t *url,const wchar_t *accept,DWORD limit,UpdateCancel *cancel,UpdateHttpSink sink,void *context,DWORD *http_status,const UpdateTransport *transport,void *transport_context){
    if(!http_status||!sink||!transport||!transport->open||!transport->read||!transport->close||!limit||limit>UPDATE_EXE_LIMIT||!UpdateUrlAllowed(url)||
       (!accept||(wcscmp(accept,L"application/json")&&wcscmp(accept,L"application/octet-stream"))))return UPDATE_INVALID;
    *http_status=0;wchar_t current[8193];wcscpy_s(current,_countof(current),url);ULONGLONG deadline=GetTickCount64()+120000;
    for(int hop=0;hop<=5;hop++){
        if(cancelled(cancel))return UPDATE_CANCELLED;if(GetTickCount64()>=deadline)return UPDATE_NETWORK;
        UpdateHttpResponse response={0};UpdateStatus result=transport->open(transport_context,current,accept,deadline,&response);if(result!=UPDATE_OK)return result;
        *http_status=response.status;
        if(cancelled(cancel)){transport->close(response.request);return UPDATE_CANCELLED;}
        if(response.status==301||response.status==302||response.status==303||response.status==307||response.status==308){
            wchar_t next[8193];DWORD count=_countof(next);BOOL valid=hop<5&&response.location[0]&&SUCCEEDED(UrlCombineW(current,response.location,next,&count,0))&&UpdateUrlAllowed(next)&&wcscmp(next,current);
            transport->close(response.request);if(!valid)return UPDATE_INVALID;wcscpy_s(current,_countof(current),next);continue;
        }
        if(response.status!=200){transport->close(response.request);return UPDATE_OK;}
        if(response.has_length&&response.length>limit){transport->close(response.request);return UPDATE_INVALID;}
        BYTE chunk[16384];DWORD used=0;
        for(;;){
            if(cancelled(cancel)){result=UPDATE_CANCELLED;break;}
            if(GetTickCount64()>=deadline){result=UPDATE_NETWORK;break;}
            DWORD got=0;if(!transport->read(response.request,chunk,sizeof(chunk),&got)){result=UPDATE_NETWORK;break;}
            if(cancelled(cancel)){result=UPDATE_CANCELLED;break;}
            if(got>sizeof(chunk)||got>limit-used){result=UPDATE_INVALID;break;}
            if(!got){result=response.has_length&&used!=response.length?UPDATE_INVALID:UPDATE_OK;break;}
            if(!sink(chunk,got,context)){result=UPDATE_IO;break;}used+=got;
        }
        transport->close(response.request);return result;
    }
    return UPDATE_INVALID;
}
UpdateStatus UpdateHttpGet(const wchar_t *url,const wchar_t *accept,DWORD limit,UpdateCancel *cancel,UpdateHttpSink sink,void *context,DWORD *http_status){return UpdateHttpGetUsing(url,accept,limit,cancel,sink,context,http_status,&windows_transport,NULL);}
typedef struct JsonBuffer {char *data;DWORD length;} JsonBuffer;
static BOOL json_sink(const BYTE *bytes,DWORD length,void *context){JsonBuffer *b=context;if(length>UPDATE_JSON_LIMIT-b->length)return FALSE;memcpy(b->data+b->length,bytes,length);b->length+=length;return TRUE;}
UpdateStatus UpdateCheckUsing(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out,const UpdateTransport *transport,void *context){
    if(!out)return UPDATE_INVALID;memset(out,0,sizeof(*out));JsonBuffer b={malloc(UPDATE_JSON_LIMIT+1),0};if(!b.data)return UPDATE_IO;DWORD code;
    UpdateStatus result=UpdateHttpGetUsing(L"https://api.github.com/repos/estkler/jira-task-manager/releases/latest",L"application/json",UPDATE_JSON_LIMIT,cancel,json_sink,&b,&code,transport,context);
    if(result==UPDATE_OK){if(code==404)result=UPDATE_NONE;else if(code!=200)result=UPDATE_NETWORK;else result=UpdateParseRelease(b.data,b.length,current,out);}free(b.data);return result;
}
UpdateStatus UpdateCheck(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out){return UpdateCheckUsing(current,cancel,out,&windows_transport,NULL);}
typedef struct FileSink {HANDLE file;DWORD length;} FileSink;
static BOOL file_sink(const BYTE *bytes,DWORD length,void *context){FileSink *f=context;DWORD written;BOOL ok=WriteFile(f->file,bytes,length,&written,NULL)&&written==length;if(ok)f->length+=length;return ok;}
UpdateStatus UpdateDownloadUsing(const UpdateRelease *release,HANDLE destination,UpdateCancel *cancel,const UpdateTransport *transport,void *context){
    if(!release||!release->asset_id||!release->size||release->size>UPDATE_EXE_LIMIT||destination==INVALID_HANDLE_VALUE)return UPDATE_INVALID;
    LARGE_INTEGER zero={0};if(!SetFilePointerEx(destination,zero,NULL,FILE_BEGIN)||!SetEndOfFile(destination))return UPDATE_IO;
    wchar_t url[256];swprintf(url,_countof(url),L"https://api.github.com/repos/estkler/jira-task-manager/releases/assets/%llu",release->asset_id);DWORD code;FileSink f={destination,0};
    UpdateStatus result=UpdateHttpGetUsing(url,L"application/octet-stream",release->size,cancel,file_sink,&f,&code,transport,context);
    if(result==UPDATE_OK)result=code!=200?UPDATE_NETWORK:f.length!=release->size?UPDATE_INVALID:FlushFileBuffers(destination)?UPDATE_OK:UPDATE_IO;
    return result;
}
UpdateStatus UpdateDownload(const UpdateRelease *release,HANDLE destination,UpdateCancel *cancel){return UpdateDownloadUsing(release,destination,cancel,&windows_transport,NULL);}
