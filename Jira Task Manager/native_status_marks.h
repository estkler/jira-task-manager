#ifndef TASKLET_STATUS_MARKS_H
#define TASKLET_STATUS_MARKS_H
#include "native_notes.h"
#include "native_status_activity.h"

/* UI-only unread changes. No task pointers, credentials or network operations. */
typedef struct NativeStatusMark {wchar_t key[80],id[80],previous[128],current[128];} NativeStatusMark;
typedef struct NativeStatusMarks {JiraSnapshot scope;NativeStatusMark *items;int count,capacity;BOOL ready,dirty;} NativeStatusMarks;
static void NativeStatusMarksFree(NativeStatusMarks *m){free(m->items);*m=(NativeStatusMarks){0};}
static const wchar_t *StatusMarksAccount(const JiraSnapshot *s)
{return s->identity.account_id[0]?s->identity.account_id:s->identity.key[0]?s->identity.key:s->identity.name;}
static BOOL StatusMarksStorage(const wchar_t *settings,const JiraSnapshot *scope,wchar_t path[MAX_PATH+32],wchar_t account[1100])
{
    if(!settings||!*settings||wcslen(settings)>=MAX_PATH||!scope->server[0]||!*StatusMarksAccount(scope))return FALSE;
    swprintf(path,MAX_PATH+32,L"%ls.statusmarks",settings);swprintf(account,1100,L"%ls\n%ls",scope->server,StatusMarksAccount(scope));return TRUE;
}
static NativeStatusMark *NativeStatusMarksFind(NativeStatusMarks *m,const wchar_t *key)
{for(int i=0;i<m->count;i++)if(!wcscmp(m->items[i].key,key))return &m->items[i];return NULL;}
static void StatusMarksRemove(NativeStatusMarks *m,int i)
{memmove(m->items+i,m->items+i+1,(size_t)(m->count-i-1)*sizeof(*m->items));m->count--;m->dirty=TRUE;}
static BOOL StatusMarksReserve(NativeStatusMarks *m)
{
    if(m->count<m->capacity)return TRUE;if(m->count>=10000)return FALSE;
    int n=min(10000,max(8,m->capacity*2));NativeStatusMark *items=realloc(m->items,(size_t)n*sizeof(*items));
    if(!items)return FALSE;m->items=items;m->capacity=n;return TRUE;
}
static wchar_t *StatusMarksPack(wchar_t *out,const wchar_t *value)
{size_t n=wcslen(value);out+=swprintf(out,24,L"%zu:",n);wmemcpy(out,value,n);out[n]=0;return out+n;}
static BOOL StatusMarksUnpack(const wchar_t **at,wchar_t *out,size_t capacity)
{
    if(**at<L'0'||**at>L'9')return FALSE;wchar_t *end=NULL;unsigned long n=wcstoul(*at,&end,10);
    if(*end!=L':'||n>=capacity||wcslen(end+1)<n)return FALSE;wmemcpy(out,end+1,n);out[n]=0;*at=end+1+n;return TRUE;
}
static BOOL StatusMarksSave(NativeStatusMarks *m,const wchar_t *settings)
{
    wchar_t path[MAX_PATH+32],account[1100];if(!StatusMarksStorage(settings,&m->scope,path,account))return FALSE;
    size_t size=64+wcslen(m->scope.server)+wcslen(StatusMarksAccount(&m->scope));
    for(int i=0;i<m->count;i++){NativeStatusMark *v=m->items+i;size+=96+wcslen(v->key)+wcslen(v->id)+wcslen(v->previous)+wcslen(v->current);}
    wchar_t *text=calloc(size,sizeof(wchar_t));if(!text)return FALSE;wcscpy_s(text,size,L"SM1:");
    wchar_t *at=StatusMarksPack(text+4,m->scope.server);at=StatusMarksPack(at,StatusMarksAccount(&m->scope));
    for(int i=0;i<m->count;i++){NativeStatusMark *v=m->items+i;at=StatusMarksPack(at,v->key);at=StatusMarksPack(at,v->id);at=StatusMarksPack(at,v->previous);at=StatusMarksPack(at,v->current);}
    BOOL ok=NativeNoteSave(path,account,L"status-changes",text);SecureZeroMemory(text,size*sizeof(wchar_t));free(text);
    if(ok)m->dirty=FALSE;return ok;
}
static BOOL StatusMarksLoad(NativeStatusMarks *m,const wchar_t *settings,const JiraSnapshot *scope)
{
    if(m->ready&&NativeStatusSameScope(&m->scope,scope))return TRUE;
    NativeStatusMarksFree(m);m->scope.identity=scope->identity;wcscpy_s(m->scope.server,800,scope->server);
    wchar_t path[MAX_PATH+32],account[1100],*text=NULL;
    if(!StatusMarksStorage(settings,scope,path,account)||!NativeNoteLoadAllocated(path,account,L"status-changes",&text))return FALSE;
    BOOL ok=TRUE;const wchar_t *at=text;
    if(*text){
        wchar_t server[800],owner[256];ok=!wcsncmp(at,L"SM1:",4);if(ok)at+=4;
        ok=ok&&StatusMarksUnpack(&at,server,800)&&StatusMarksUnpack(&at,owner,256)&&!wcscmp(server,scope->server)&&!wcscmp(owner,StatusMarksAccount(scope));
        while(ok&&*at){
            NativeStatusMark v={0};ok=StatusMarksUnpack(&at,v.key,80)&&StatusMarksUnpack(&at,v.id,80)&&StatusMarksUnpack(&at,v.previous,128)&&StatusMarksUnpack(&at,v.current,128)&&v.key[0]&&v.id[0]&&v.current[0]&&!NativeStatusMarksFind(m,v.key)&&StatusMarksReserve(m);
            if(ok)m->items[m->count++]=v;
        }
    }
    SecureZeroMemory(text,(wcslen(text)+1)*sizeof(wchar_t));free(text);
    if(!ok){free(m->items);m->items=NULL;m->count=m->capacity=0;return FALSE;}
    m->ready=TRUE;return TRUE;
}
static BOOL NativeStatusMarksObserve(NativeStatusMarks *m,const wchar_t *settings,const JiraSnapshot *before,const JiraSnapshot *after,const NativeStatusWrites *writes,BOOL enabled,BOOL complete)
{
    if(!StatusMarksLoad(m,settings,after))return FALSE;
    BOOL same=NativeStatusSameScope(before,after);
    int old_count=same?before->count:0;
    const JiraItem **ordered=malloc((size_t)(old_count+after->count+1)*sizeof(*ordered));if(!ordered)return FALSE;
    const JiraItem **new_index=ordered+old_count;
    for(int i=0;i<old_count;i++)ordered[i]=before->items+i;
    for(int i=0;i<after->count;i++)new_index[i]=after->items+i;
    qsort(ordered,(size_t)old_count,sizeof(*ordered),NativeStatusKeyOrder);
    qsort(new_index,(size_t)after->count,sizeof(*new_index),NativeStatusKeyOrder);
    if(complete)for(int i=m->count-1;i>=0;i--){
        JiraItem key={0};wcscpy_s(key.key,80,m->items[i].key);const JiraItem *needle=&key;
        if(!bsearch(&needle,new_index,(size_t)after->count,sizeof(*new_index),NativeStatusKeyOrder))StatusMarksRemove(m,i);
    }
    for(int i=0;i<after->count;i++){
        const JiraItem *item=after->items+i;NativeStatusMark *mark=NativeStatusMarksFind(m,item->key);
        BOOL own=NativeStatusIsOwnWrite(writes,after,item);
        if(mark&&(own||(!enabled&&wcscmp(mark->id,item->status_id))||(!complete&&wcscmp(mark->id,item->status_id)))){StatusMarksRemove(m,(int)(mark-m->items));mark=NULL;}
        if(own||!enabled||!complete||!item->status_id[0])continue;
        if(mark&&!wcscmp(mark->id,item->status_id)&&wcscmp(mark->current,item->status_name)){wcscpy_s(mark->current,128,item->status_name);m->dirty=TRUE;}
        const JiraItem **found=bsearch(&item,ordered,(size_t)old_count,sizeof(*ordered),NativeStatusKeyOrder);
        const JiraItem *old=found?*found:NULL;
        const wchar_t *previous=old&&old->status_id[0]&&wcscmp(old->status_id,item->status_id)?old->status_name:mark&&wcscmp(mark->id,item->status_id)?mark->current:NULL;
        if(!previous)continue;
        NativeStatusMark value={0};wcscpy_s(value.key,80,item->key);wcscpy_s(value.id,80,item->status_id);wcscpy_s(value.previous,128,previous);wcscpy_s(value.current,128,item->status_name);
        if(!mark){if(!StatusMarksReserve(m)){free(ordered);return FALSE;}mark=m->items+m->count++;}*mark=value;m->dirty=TRUE;
    }
    free(ordered);
    return !m->dirty||StatusMarksSave(m,settings);
}
static BOOL NativeStatusMarksAcknowledge(NativeStatusMarks *m,const wchar_t *settings,const NativeStatusMark *shown)
{
    NativeStatusMark *mark=NativeStatusMarksFind(m,shown->key);if(!mark||memcmp(mark,shown,sizeof(*mark)))return FALSE;
    int index=(int)(mark-m->items);NativeStatusMark value=*mark;StatusMarksRemove(m,index);
    if(StatusMarksSave(m,settings))return TRUE;
    memmove(m->items+index+1,m->items+index,(size_t)(m->count-index)*sizeof(*m->items));m->items[index]=value;m->count++;m->dirty=TRUE;return FALSE;
}
#endif
