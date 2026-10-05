#define WIN32_LEAN_AND_MEAN
#include "native_new_tasks.h"
#include <stdint.h>
#include <wchar.h>

static uint64_t HashText(const wchar_t *text,uint64_t hash)
{
    for(;*text;text++){
        wchar_t ch=*text;
        if(ch>=L'A'&&ch<=L'Z')ch=(wchar_t)(ch-L'A'+L'a');
        hash^=(uint16_t)ch;hash*=1099511628211ULL;
    }
    return hash;
}

static BOOL StateNames(const wchar_t *account,const wchar_t *key,wchar_t *section,wchar_t *entry)
{
    if(!account||!*account||!key||!*key)return FALSE;
    swprintf(section,64,L"NewTasks_%016llx",(unsigned long long)HashText(account,1469598103934665603ULL));
    swprintf(entry,64,L"issue_%016llx",(unsigned long long)HashText(key,1469598103934665603ULL));
    return TRUE;
}

BOOL NativeNewTasksObserve(const wchar_t *path,const wchar_t *account,
    const wchar_t *const *keys,int count,BOOL *is_new,int *newly_arrived,int *first_arrival)
{
    if(!path||!*path||!account||!*account||!keys||count<0||!is_new||!newly_arrived||!first_arrival)return FALSE;
    *newly_arrived=0;
    *first_arrival=-1;
    for(int i=0;i<count;i++)is_new[i]=FALSE;
    wchar_t section[64],entry[64],value[8];
    if(count&&!StateNames(account,keys[0],section,entry))return FALSE;
    if(!count){
        if(!StateNames(account,L"baseline",section,entry))return FALSE;
    }
    BOOL initialized=GetPrivateProfileIntW(section,L"Initialized",0,path)==1;
    for(int i=0;i<count;i++){
        if(!StateNames(account,keys[i],section,entry))return FALSE;
        GetPrivateProfileStringW(section,entry,L"",value,_countof(value),path);
        if(value[0]==L'2'&&value[1]==0){is_new[i]=TRUE;continue;}
        if(value[0]==L'1'&&value[1]==0)continue;
        if(!WritePrivateProfileStringW(section,entry,initialized?L"2":L"1",path))return FALSE;
        if(initialized){is_new[i]=TRUE;if(*first_arrival<0)*first_arrival=i;(*newly_arrived)++;}
    }
    if(!initialized&&!WritePrivateProfileStringW(section,L"Initialized",L"1",path))return FALSE;
    return TRUE;
}

BOOL NativeNewTaskMarkSeen(const wchar_t *path,const wchar_t *account,const wchar_t *key)
{
    wchar_t section[64],entry[64];
    return path&&*path&&StateNames(account,key,section,entry)&&
        WritePrivateProfileStringW(section,entry,L"1",path);
}

BOOL NativeNewTasksSeedReporter(const wchar_t *path,const wchar_t *account,
    const wchar_t *const *keys,const BOOL *reporter_only,int count)
{
    if(!path||!*path||!account||!*account||!keys||!reporter_only||count<0)return FALSE;
    wchar_t section[64],entry[64],value[8];
    if(!StateNames(account,L"baseline",section,entry))return FALSE;
    if(GetPrivateProfileIntW(section,L"ReporterInitialized",0,path)==1)return TRUE;
    for(int i=0;i<count;i++)if(reporter_only[i]){
        if(!StateNames(account,keys[i],section,entry))return FALSE;
        GetPrivateProfileStringW(section,entry,L"",value,_countof(value),path);
        if(!value[0]&&!WritePrivateProfileStringW(section,entry,L"1",path))return FALSE;
    }
    return WritePrivateProfileStringW(section,L"ReporterInitialized",L"1",path);
}

static BOOL CommentStateNames(const wchar_t *account,const wchar_t *key,
    wchar_t *section,wchar_t *entry)
{
    if(!account||!*account||!key||!*key)return FALSE;
    swprintf(section,64,L"SeenComments_%016llx",(unsigned long long)HashText(account,1469598103934665603ULL));
    swprintf(entry,64,L"issue_%016llx",(unsigned long long)HashText(key,1469598103934665603ULL));
    return TRUE;
}

BOOL NativeCommentObserve(const wchar_t *path,const wchar_t *account,
    const wchar_t *issue_key,const wchar_t *latest_comment_id,BOOL authored_by_me,BOOL *unread)
{
    if(!unread||!path||!*path)return FALSE;*unread=FALSE;
    wchar_t section[64],entry[64],seen[80];
    if(!CommentStateNames(account,issue_key,section,entry))return FALSE;
    GetPrivateProfileStringW(section,entry,L"",seen,_countof(seen),path);
    /* Remember that this known task had no comments. Its first later comment
       must be an arrival, rather than an initial-load baseline. */
    if(!latest_comment_id||!*latest_comment_id)
        return seen[0]||WritePrivateProfileStringW(section,entry,L"no-comments",path);
    if(!seen[0]||authored_by_me){
        if(wcscmp(seen,latest_comment_id)&&!WritePrivateProfileStringW(section,entry,latest_comment_id,path))return FALSE;
        return TRUE;
    }
    *unread=wcscmp(seen,latest_comment_id)!=0;
    return TRUE;
}

BOOL NativeCommentMarkSeen(const wchar_t *path,const wchar_t *account,
    const wchar_t *issue_key,const wchar_t *latest_comment_id)
{
    wchar_t section[64],entry[64];
    return path&&*path&&latest_comment_id&&*latest_comment_id&&
        CommentStateNames(account,issue_key,section,entry)&&
        WritePrivateProfileStringW(section,entry,latest_comment_id,path);
}
