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

/* Jira's numeric comment sequence is a read watermark, not merely the last
   visible ID. Filtering/deleting the tail must never move that watermark back.
   Compare decimal strings without integer overflow; opaque IDs retain equality
   behavior for compatibility. */
static BOOL CommentIdNotAfter(const wchar_t *current,const wchar_t *watermark)
{
    if(!current||!*current||!watermark||!*watermark)return FALSE;
    if(!wcscmp(current,watermark))return TRUE;
    if(wcsspn(current,L"0123456789")!=wcslen(current)||
       wcsspn(watermark,L"0123456789")!=wcslen(watermark))return FALSE;
    while(*current==L'0')current++;while(*watermark==L'0')watermark++;
    size_t a=wcslen(current),b=wcslen(watermark);
    return a<b||(a==b&&wcscmp(current,watermark)<=0);
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
    if(CommentIdNotAfter(latest_comment_id,seen))return TRUE;
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
    wchar_t section[64],entry[64],seen[80];
    if(!path||!*path||!latest_comment_id||!*latest_comment_id||
       !CommentStateNames(account,issue_key,section,entry))return FALSE;
    GetPrivateProfileStringW(section,entry,L"",seen,_countof(seen),path);
    return CommentIdNotAfter(latest_comment_id,seen)||
        WritePrivateProfileStringW(section,entry,latest_comment_id,path);
}

BOOL NativeCommentObserveArrival(const wchar_t *path,const wchar_t *account,
    const wchar_t *key,const wchar_t *latest,BOOL own,BOOL *unread,BOOL *arrival)
{
    if(!arrival||!unread)return FALSE;*arrival=FALSE;
    if(!NativeCommentObserve(path,account,key,latest,own,unread))return FALSE;
    wchar_t section[64],entry[64],observed[80];
    if(!CommentStateNames(account,key,section,entry))return FALSE;
    swprintf(section,64,L"ObservedComments_%016llx",(unsigned long long)HashText(account,1469598103934665603ULL));
    GetPrivateProfileStringW(section,entry,L"",observed,_countof(observed),path);
    const wchar_t *current=latest&&*latest?latest:L"no-comments";
    /* Consume IDs separately from the latest pointer: deletion/reordering must
       not re-announce a previously observed comment, including while disabled. */
    wchar_t consumed_entry[96];swprintf(consumed_entry,_countof(consumed_entry),L"%ls_comment_%016llx",entry,
        (unsigned long long)HashText(current,1469598103934665603ULL));
    BOOL consumed=GetPrivateProfileIntW(section,consumed_entry,0,path)!=0;
    if(!consumed&&!WritePrivateProfileStringW(section,consumed_entry,L"1",path))return FALSE;
    if(!wcscmp(observed,current))return TRUE;
    if(!WritePrivateProfileStringW(section,entry,current,path))return FALSE;
    BOOL older_numeric=CommentIdNotAfter(current,observed);
    *arrival=!consumed&&!older_numeric&&observed[0]&&latest&&*latest&&*unread&&!own;
    return TRUE;
}
