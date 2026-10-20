#ifndef TASK_MANAGER_NOTIFICATIONS_H
#define TASK_MANAGER_NOTIFICATIONS_H
#include <windows.h>
#include <wchar.h>
#include <wctype.h>
#include <stdint.h>
#include <stdio.h>
#define NATIVE_EXCLUDED_EMAILS_CAPACITY 2048
#define NATIVE_EXCLUDED_EMAILS_DEFAULT L"no-mail@kama.gs"
typedef struct NativeNotificationPreferences {
    BOOL tasks,comments;
    wchar_t excluded[NATIVE_EXCLUDED_EMAILS_CAPACITY];
    BOOL statuses;
} NativeNotificationPreferences;
static void NativeNotificationsLoad(const wchar_t *path,NativeNotificationPreferences *preferences)
{
    preferences->tasks=GetPrivateProfileIntW(L"Notifications",L"Tasks",1,path)!=0;
    preferences->comments=GetPrivateProfileIntW(L"Notifications",L"Comments",1,path)!=0;
    preferences->statuses=GetPrivateProfileIntW(L"Notifications",L"StatusChanges",1,path)!=0;
    GetPrivateProfileStringW(L"Notifications",L"ExcludedEmails",NATIVE_EXCLUDED_EMAILS_DEFAULT,
        preferences->excluded,NATIVE_EXCLUDED_EMAILS_CAPACITY,path);
}
/* Trim each comma-separated entry. No display-name fallback or partial matches. */
static BOOL NativeEmailExcluded(const wchar_t *list,const wchar_t *email)
{
    if(!email||!*email)return FALSE;
    if(!list)list=NATIVE_EXCLUDED_EMAILS_DEFAULT;
    size_t email_length=wcslen(email);
    for(const wchar_t *at=list;*at;){
        while(*at==L','||iswspace(*at))at++;
        const wchar_t *end=wcschr(at,L',');if(!end)end=at+wcslen(at);
        const wchar_t *trimmed=end;while(trimmed>at&&iswspace(trimmed[-1]))trimmed--;
        if((size_t)(trimmed-at)==email_length&&!_wcsnicmp(at,email,email_length))return TRUE;
        at=*end?end+1:end;
    }
    return FALSE;
}
static BOOL NativeNormalizeExcludedEmails(const wchar_t *input,wchar_t *out,size_t capacity)
{
    size_t used=0;if(!input||!out||!capacity)return FALSE;out[0]=0;
    for(const wchar_t *at=input;*at;){
        while(*at==L','||iswspace(*at))at++;
        const wchar_t *end=wcschr(at,L',');if(!end)end=at+wcslen(at);
        const wchar_t *trimmed=end;while(trimmed>at&&iswspace(trimmed[-1]))trimmed--;
        if(trimmed>at){
            size_t length=(size_t)(trimmed-at);const wchar_t *sign=NULL;
            for(const wchar_t *ch=at;ch<trimmed;ch++){
                if(iswspace(*ch)||*ch==L';'||*ch==L'='||*ch==L'"'||*ch==L'<'||*ch==L'>')return FALSE;
                if(*ch==L'@'){if(sign)return FALSE;sign=ch;}
            }
            if(!sign||sign==at||sign+1==trimmed||length>=256||used+length+(used?2:0)>=capacity)return FALSE;
            if(used){out[used++]=L',';out[used++]=L' ';}
            for(const wchar_t *ch=at;ch<trimmed;ch++)out[used++]=(wchar_t)towlower(*ch);
            out[used]=0;
        }
        at=*end?end+1:end;
    }
    return TRUE;
}
static BOOL NativeNotificationsSave(const wchar_t *path,const NativeNotificationPreferences *preferences)
{
    return WritePrivateProfileStringW(L"Notifications",L"Tasks",preferences->tasks?L"1":L"0",path)&&
        WritePrivateProfileStringW(L"Notifications",L"Comments",preferences->comments?L"1":L"0",path)&&
        WritePrivateProfileStringW(L"Notifications",L"StatusChanges",preferences->statuses?L"1":L"0",path)&&
        WritePrivateProfileStringW(L"Notifications",L"ExcludedEmails",preferences->excluded,path);
}
static void NativeEmailBaselineEntry(const wchar_t *email,wchar_t *entry,size_t capacity)
{
    uint64_t hash=1469598103934665603ULL;
    for(;*email;email++){hash^=(uint16_t)towlower(*email);hash*=1099511628211ULL;}
    swprintf(entry,capacity,L"email_%016llx",(unsigned long long)hash);
}
/* Only authors revealed by removing an exclusion get a history cutoff.
   This survives restart and never consumes unrelated colleagues' arrivals. */
static BOOL NativeRememberRemovedExclusions(const wchar_t *path,const wchar_t *old_list,const wchar_t *new_list)
{
    FILETIME now;GetSystemTimeAsFileTime(&now);ULARGE_INTEGER value={0};value.LowPart=now.dwLowDateTime;value.HighPart=now.dwHighDateTime;
    wchar_t cutoff[32];swprintf(cutoff,_countof(cutoff),L"%llu",(unsigned long long)value.QuadPart);
    for(const wchar_t *at=old_list;*at;){
        while(*at==L','||iswspace(*at))at++;
        const wchar_t *end=wcschr(at,L',');if(!end)end=at+wcslen(at);
        const wchar_t *trimmed=end;while(trimmed>at&&iswspace(trimmed[-1]))trimmed--;
        size_t count=(size_t)(trimmed-at);wchar_t email[256],entry[40];
        if(count&&count<_countof(email)){
            wmemcpy(email,at,count);email[count]=0;
            if(!NativeEmailExcluded(new_list,email)){
                NativeEmailBaselineEntry(email,entry,_countof(entry));
                if(!WritePrivateProfileStringW(L"CommentFilterBaselines",entry,cutoff,path))return FALSE;
            }
        }
        at=*end?end+1:end;
    }
    return TRUE;
}
static ULONGLONG NativeCommentUtcTime(const wchar_t *created)
{
    if(!created||wcslen(created)<19)return 0;
    unsigned year,month,day,hour,minute,second;
    if(swscanf(created,L"%4u-%2u-%2uT%2u:%2u:%2u",&year,&month,&day,&hour,&minute,&second)!=6)return 0;
    SYSTEMTIME time={0};time.wYear=(WORD)year;time.wMonth=(WORD)month;time.wDay=(WORD)day;
    time.wHour=(WORD)hour;time.wMinute=(WORD)minute;time.wSecond=(WORD)second;
    const wchar_t *zone=created+19;
    if(*zone==L'.'){
        zone++;int digits=0,milliseconds=0;
        while(*zone>=L'0'&&*zone<=L'9'){if(digits<3)milliseconds=milliseconds*10+(*zone-L'0');digits++;zone++;}
        if(!digits)return 0;while(digits<3){milliseconds*=10;digits++;}time.wMilliseconds=(WORD)milliseconds;
    }
    int offset=0;
    if(*zone==L'+'||*zone==L'-'){
        int sign=*zone++==L'+'?1:-1;
        if(zone[0]<L'0'||zone[0]>L'9'||zone[1]<L'0'||zone[1]>L'9')return 0;
        int hours=(zone[0]-L'0')*10+zone[1]-L'0';zone+=2;if(*zone==L':')zone++;
        if(zone[0]<L'0'||zone[0]>L'9'||zone[1]<L'0'||zone[1]>L'9')return 0;
        int minutes=(zone[0]-L'0')*10+zone[1]-L'0';zone+=2;if(*zone||hours>23||minutes>59)return 0;
        offset=sign*(hours*60+minutes);
    }else if(*zone!=L'Z'||zone[1])return 0;
    FILETIME file;ULARGE_INTEGER value={0};if(!SystemTimeToFileTime(&time,&file))return 0;
    value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;
    LONGLONG adjusted=(LONGLONG)value.QuadPart-(LONGLONG)offset*60*10000000;
    return adjusted>0?(ULONGLONG)adjusted:0;
}
static BOOL NativeCommentIsRevealedHistory(const wchar_t *path,const wchar_t *email,const wchar_t *created)
{
    if(!email||!*email)return FALSE;ULONGLONG date=NativeCommentUtcTime(created);if(!date)return FALSE;
    wchar_t entry[40],value[32];NativeEmailBaselineEntry(email,entry,_countof(entry));
    GetPrivateProfileStringW(L"CommentFilterBaselines",entry,L"0",value,_countof(value),path);
    ULONGLONG cutoff=_wcstoui64(value,NULL,10);return cutoff&&date<=cutoff;
}
#endif
