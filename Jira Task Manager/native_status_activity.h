#ifndef NATIVE_STATUS_ACTIVITY_H
#define NATIVE_STATUS_ACTIVITY_H
#include "native_jira.h"
#include <stdlib.h>
#include <wchar.h>
typedef struct NativeStatusActivity {
    int count,first;
    wchar_t previous[128];
} NativeStatusActivity;
typedef struct NativeStatusWrite {
    wchar_t key[80],status_id[80],status_name[128];
} NativeStatusWrite;
typedef struct NativeStatusWrites {
    JiraSnapshot scope; /* Identity/server only; never owns snapshot.items. */
    NativeStatusWrite *items;int count,capacity;
} NativeStatusWrites;
static BOOL NativeStatusRemember(void *context,const wchar_t *key,const wchar_t *id,const wchar_t *name)
{
    NativeStatusWrites *writes=context;
    if(writes->count==writes->capacity){
        int capacity=writes->capacity?writes->capacity*2:8;
        NativeStatusWrite *items=realloc(writes->items,(size_t)capacity*sizeof(*items));if(!items)return FALSE;
        writes->items=items;writes->capacity=capacity;
    }
    NativeStatusWrite *item=&writes->items[writes->count++];
    wcscpy_s(item->key,80,key);wcscpy_s(item->status_id,80,id);wcscpy_s(item->status_name,128,name);return TRUE;
}
static void NativeStatusWriteOutcome(void *context,BOOL may_have_changed)
{NativeStatusWrites *writes=context;if(!may_have_changed&&writes->count)writes->count--;}
/* Display names are deliberately never an account identifier. */
static BOOL NativeStatusSameScope(const JiraSnapshot *one,const JiraSnapshot *two)
{
    if(!one->server[0]||wcscmp(one->server,two->server))return FALSE;
    if(one->identity.account_id[0]&&two->identity.account_id[0])return !wcscmp(one->identity.account_id,two->identity.account_id);
    if(one->identity.key[0]&&two->identity.key[0])return !wcscmp(one->identity.key,two->identity.key);
    return one->identity.name[0]&&two->identity.name[0]&&!wcscmp(one->identity.name,two->identity.name);
}
static int NativeStatusKeyOrder(const void *one,const void *two)
{return wcscmp((*(const JiraItem *const *)one)->key,(*(const JiraItem *const *)two)->key);}
static BOOL NativeStatusCollect(const JiraSnapshot *before,const JiraSnapshot *after,const NativeStatusWrites *writes,NativeStatusActivity *activity)
{
    *activity=(NativeStatusActivity){.first=-1};
    if(!before->count||!NativeStatusSameScope(before,after))return TRUE;
    const JiraItem **ordered=malloc((size_t)before->count*sizeof(*ordered));if(!ordered)return FALSE;
    for(int i=0;i<before->count;i++)ordered[i]=&before->items[i];
    qsort(ordered,(size_t)before->count,sizeof(*ordered),NativeStatusKeyOrder);
    for(int i=0;i<after->count;i++){
        const JiraItem *item=&after->items[i];
        const JiraItem **found=bsearch(&item,ordered,(size_t)before->count,sizeof(*ordered),NativeStatusKeyOrder);
        if(!found||!(*found)->status_id[0]||!item->status_id[0]||!wcscmp((*found)->status_id,item->status_id))continue;
        BOOL own=FALSE;
        if(writes&&NativeStatusSameScope(&writes->scope,after))for(int j=0;j<writes->count;j++){
            const NativeStatusWrite *write=&writes->items[j];
            if(write->status_id[0]&&!wcscmp(write->key,item->key)&&!wcscmp(write->status_id,item->status_id)){own=TRUE;break;}
        }
        if(own)continue;
        if(activity->first<0){activity->first=i;wcscpy_s(activity->previous,128,(*found)->status_name);}
        activity->count++;
    }
    free(ordered);return TRUE;
}
#endif
