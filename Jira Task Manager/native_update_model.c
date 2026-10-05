#include "native_update.h"
#include "native_json.h"
#include <winhttp.h>
#include <wchar.h>
#include <limits.h>

BOOL UpdateParseVersion(const wchar_t *text,UpdateVersion *out){
    if(!text||!out)return FALSE;
    const wchar_t *p=text;if(*p==L'v')p++;
    DWORD parts[3];
    for(int i=0;i<3;i++){
        if(*p<L'0'||*p>L'9')return FALSE;
        DWORD n=0;
        do{DWORD d=*p++-L'0';if(n>(MAXDWORD-d)/10)return FALSE;n=n*10+d;}while(*p>=L'0'&&*p<=L'9');
        parts[i]=n;if(i<2){if(*p++!=L'.')return FALSE;}else if(*p)return FALSE;
    }
    *out=(UpdateVersion){parts[0],parts[1],parts[2]};return TRUE;
}
int UpdateCompareVersion(UpdateVersion a,UpdateVersion b){
    if(a.major!=b.major)return a.major>b.major?1:-1;
    if(a.minor!=b.minor)return a.minor>b.minor?1:-1;
    return a.patch==b.patch?0:a.patch>b.patch?1:-1;
}
/* Reject duplicated security-relevant keys; nj_get intentionally permits them for other consumers. */
static int unique(const NJDoc *d,int object,const char *key){
    if(object<0||d->tokens[object].kind!=NJ_OBJECT)return -1;
    int at=object+1,result=-1;
    for(int i=0;i<d->tokens[object].count;i++){
        if(nj__key_equal(d,&d->tokens[at],key)){if(result!=-1)return -1;result=at+1;}
        at=d->tokens[at+1].next;
    }
    return result;
}
static BOOL string(const NJDoc *d,int t,wchar_t *out,size_t cap){
    size_t units=0;
    return t>=0&&d->tokens[t].kind==NJ_STRING&&nj__string_units(d,&d->tokens[t],&units)&&
        nj_string(d,t,out,cap)&&wcslen(out)==units;
}
static BOOL unsigned_number(const NJDoc *d,int t,ULONGLONG *out){
    if(t<0||d->tokens[t].kind!=NJ_NUMBER)return FALSE;
    ULONGLONG n=0;NJToken token=d->tokens[t];
    for(int at=token.start;at<token.end;at++){
        unsigned digit=(unsigned char)d->text[at]-'0';
        if(digit>9||n>(ULLONG_MAX-digit)/10)return FALSE;n=n*10+digit;
    }
    *out=n;return token.end>token.start;
}
static BOOL false_flag(const NJDoc *d,int t){
    return t>=0&&d->tokens[t].kind==NJ_BOOL&&d->tokens[t].end-d->tokens[t].start==5&&
        !memcmp(d->text+d->tokens[t].start,"false",5);
}
static int hex(wchar_t c){return c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;}
UpdateStatus UpdateParseRelease(const char *json,size_t length,UpdateVersion current,UpdateRelease *out){
    if(!out||!json||!length||length>UPDATE_JSON_LIMIT)return UPDATE_INVALID;
    memset(out,0,sizeof(*out));NJDoc d={0};if(!nj_parse(&d,json,length))return UPDATE_INVALID;
    UpdateStatus status=UPDATE_INVALID;UpdateRelease r={0};wchar_t tag[64];
    if(!string(&d,unique(&d,0,"tag_name"),tag,64)||!UpdateParseVersion(tag,&r.version)||
       !false_flag(&d,unique(&d,0,"draft"))||!false_flag(&d,unique(&d,0,"prerelease")))goto done;
    int array=unique(&d,0,"assets");if(array<0||d.tokens[array].kind!=NJ_ARRAY)goto done;
    int at=array+1,found=-1;
    for(int i=0;i<d.tokens[array].count;i++){
        wchar_t name[256];if(!string(&d,unique(&d,at,"name"),name,256))goto done;
        if(!wcscmp(name,L"Jira.Task.Manager.exe")){if(found!=-1)goto done;found=at;}
        at=d.tokens[at].next;
    }
    if(found==-1){status=UPDATE_NONE;goto done;}
    ULONGLONG size;wchar_t digest[80];
    if(!unsigned_number(&d,unique(&d,found,"id"),&r.asset_id)||!r.asset_id||
       !unsigned_number(&d,unique(&d,found,"size"),&size)||!size||size>UPDATE_EXE_LIMIT||
       !string(&d,unique(&d,found,"digest"),digest,80)||wcslen(digest)!=71||wcsncmp(digest,L"sha256:",7))goto done;
    r.size=(DWORD)size;
    for(int i=0;i<32;i++){int a=hex(digest[7+2*i]),b=hex(digest[8+2*i]);if(a<0||b<0)goto done;r.sha256[i]=(BYTE)((a<<4)|b);}
    *out=r;status=UpdateCompareVersion(r.version,current)>0?UPDATE_OK:UPDATE_NONE;
done:nj_free(&d);return status;
}
BOOL UpdateUrlAllowed(const wchar_t *url){
    if(!url||wcslen(url)>8192)return FALSE;
    for(const wchar_t *p=url;*p;p++)if(*p<=32||*p==L'\\'||*p==L'#'||*p==127)return FALSE;
    wchar_t host[256];URL_COMPONENTS c={sizeof(c)};c.lpszHostName=host;c.dwHostNameLength=256;
    c.dwUserNameLength=c.dwPasswordLength=(DWORD)-1;
    if(!WinHttpCrackUrl(url,0,0,&c)||c.nScheme!=INTERNET_SCHEME_HTTPS||c.nPort!=443||
       c.dwUserNameLength||c.dwPasswordLength||c.dwHostNameLength>=256)return FALSE;
    host[c.dwHostNameLength]=0;
    return !_wcsicmp(host,L"api.github.com")||!_wcsicmp(host,L"github.com")||
        !_wcsicmp(host,L"release-assets.githubusercontent.com")||!_wcsicmp(host,L"objects.githubusercontent.com");
}
