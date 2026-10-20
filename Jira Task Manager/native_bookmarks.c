#include "native_bookmarks.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
void NativeBookmarksFree(NativeBookmarks *m){if(m){free(m->saved.keys);free(m->draft.keys);memset(m,0,sizeof(*m));}}
static BOOL key_normalize(const wchar_t *key,wchar_t output[BOOKMARK_KEY_CHARS]){
    if(!key||!*key)return FALSE;memset(output,0,BOOKMARK_KEY_CHARS*sizeof(wchar_t));
    for(int i=0;i<BOOKMARK_KEY_CHARS;i++){
        wchar_t c=key[i];if(!c)return TRUE;if(i==BOOKMARK_KEY_CHARS-1)return FALSE;
        if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';
        if(!((c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'-'||c==L'_'))return FALSE;
        output[i]=c;
    }return FALSE;
}
static int key_position(const BookmarkKeySet *set,const wchar_t *key,BOOL *found){
    int left=0,right=set->count;while(left<right){int middle=left+(right-left)/2;if(wcscmp(set->keys[middle],key)<0)left=middle+1;else right=middle;}
    *found=left<set->count&&!wcscmp(set->keys[left],key);return left;
}
static const BookmarkKeySet *current_set(const NativeBookmarks *m){return m->editing?&m->draft:&m->saved;}
BOOL NativeBookmarksBegin(NativeBookmarks *m){
    if(!m)return FALSE;if(m->editing)return TRUE;BookmarkKeySet draft={0};
    if(m->saved.count){draft.keys=calloc((size_t)m->saved.count,sizeof(*draft.keys));if(!draft.keys)return FALSE;
        draft.count=draft.capacity=m->saved.count;memcpy(draft.keys,m->saved.keys,(size_t)draft.count*sizeof(*draft.keys));}
    free(m->draft.keys);m->draft=draft;m->previous_showing=m->showing;m->showing=FALSE;m->editing=TRUE;return TRUE;
}
void NativeBookmarksCancel(NativeBookmarks *m){if(!m||!m->editing)return;free(m->draft.keys);memset(&m->draft,0,sizeof(m->draft));m->editing=FALSE;m->showing=m->previous_showing;}
BOOL NativeBookmarksContains(const NativeBookmarks *m,const wchar_t *key){
    wchar_t normal[BOOKMARK_KEY_CHARS];if(!m||!key_normalize(key,normal))return FALSE;BOOL found;key_position(current_set(m),normal,&found);return found;
}
BOOL NativeBookmarksSet(NativeBookmarks *m,const wchar_t *key,BOOL selected){
    wchar_t normal[BOOKMARK_KEY_CHARS];if(!m||!m->editing||!key_normalize(key,normal)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    selected=selected!=FALSE;
    BookmarkKeySet *set=&m->draft;BOOL found;int position=key_position(set,normal,&found);
    if(selected==found)return TRUE;
    if(!selected){memmove(set->keys+position,set->keys+position+1,(size_t)(set->count-position-1)*sizeof(*set->keys));set->count--;return TRUE;}
    if(set->count>=BOOKMARK_MAX_KEYS){SetLastError(ERROR_BUFFER_OVERFLOW);return FALSE;}
    if(set->count==set->capacity){int capacity=min(BOOKMARK_MAX_KEYS,max(16,set->capacity*2));
        void *keys=realloc(set->keys,(size_t)capacity*sizeof(*set->keys));if(!keys)return FALSE;set->keys=keys;set->capacity=capacity;}
    memmove(set->keys+position+1,set->keys+position,(size_t)(set->count-position)*sizeof(*set->keys));memcpy(set->keys[position],normal,sizeof(normal));set->count++;return TRUE;
}
BOOL NativeBookmarksDirty(const NativeBookmarks *m){return m&&m->editing&&(m->saved.count!=m->draft.count||(m->saved.count&&memcmp(m->saved.keys,m->draft.keys,(size_t)m->saved.count*sizeof(*m->saved.keys))));}
int NativeBookmarksCount(const NativeBookmarks *m){return m?current_set(m)->count:0;}
static ULONGLONG hash_bytes(const void *data,size_t size){ULONGLONG hash=1469598103934665603ULL;const BYTE *bytes=data;for(size_t i=0;i<size;i++){hash^=bytes[i];hash*=1099511628211ULL;}return hash;}
static BOOL storage_path(const wchar_t *settings,const wchar_t *server,const wchar_t *account,wchar_t path[MAX_PATH],wchar_t directory[MAX_PATH]){
    if(!settings||!server||!*server||!account||!*account)return FALSE;
    const wchar_t *slash=wcsrchr(settings,L'\\'),*forward=wcsrchr(settings,L'/');if(forward&&(!slash||forward>slash))slash=forward;
    if(!slash||slash-settings>=MAX_PATH-50)return FALSE;size_t length=(size_t)(slash-settings);wmemcpy(directory,settings,length);directory[length]=0;
    ULONGLONG hash=1469598103934665603ULL;const wchar_t *parts[]={server,L"\n",account};
    for(int p=0;p<3;p++)for(const wchar_t *at=parts[p];*at;at++){hash^=(unsigned short)*at;hash*=1099511628211ULL;}
    return swprintf(path,MAX_PATH,L"%ls\\bookmarks-%016llx.dat",directory,(unsigned long long)hash)>0;
}
typedef struct BookmarkFileHeader {DWORD magic,version,count,reserved;ULONGLONG checksum;} BookmarkFileHeader;
static BOOL save_set(const BookmarkKeySet *set,const wchar_t *settings,const wchar_t *server,const wchar_t *account){
    wchar_t path[MAX_PATH],directory[MAX_PATH],temp[MAX_PATH];if(!storage_path(settings,server,account,path,directory))return FALSE;
    if(!GetTempFileNameW(directory,L"bmk",0,temp))return FALSE;
    HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    BOOL ok=file!=INVALID_HANDLE_VALUE;DWORD wrote=0;size_t bytes=(size_t)set->count*sizeof(*set->keys);
    BookmarkFileHeader header={0x314b4d42,1,(DWORD)set->count,0,hash_bytes(set->keys,bytes)};
    if(ok)ok=WriteFile(file,&header,sizeof(header),&wrote,NULL)&&wrote==sizeof(header);
    if(ok&&bytes)ok=WriteFile(file,set->keys,(DWORD)bytes,&wrote,NULL)&&wrote==bytes;
    if(ok)ok=FlushFileBuffers(file);DWORD error=GetLastError();if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(ok){ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);error=GetLastError();}
    if(!ok){DeleteFileW(temp);SetLastError(error);}return ok;
}
BOOL NativeBookmarksLoad(NativeBookmarks *m,const wchar_t *settings,const wchar_t *server,const wchar_t *account){
    wchar_t path[MAX_PATH],directory[MAX_PATH];if(!m||!storage_path(settings,server,account,path,directory))return FALSE;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_NOT_FOUND)return FALSE;NativeBookmarksFree(m);return TRUE;}
    BookmarkFileHeader header={0};LARGE_INTEGER length;DWORD got=0;BookmarkKeySet set={0};
    BOOL ok=GetFileSizeEx(file,&length)&&ReadFile(file,&header,sizeof(header),&got,NULL)&&got==sizeof(header)&&header.magic==0x314b4d42&&header.version==1&&!header.reserved&&header.count<=BOOKMARK_MAX_KEYS;
    size_t bytes=ok?(size_t)header.count*sizeof(*set.keys):0;
    if(ok)ok=length.QuadPart==(LONGLONG)(sizeof(header)+bytes);
    if(ok&&bytes){set.keys=calloc(header.count,sizeof(*set.keys));ok=set.keys&&ReadFile(file,set.keys,(DWORD)bytes,&got,NULL)&&got==bytes;}
    CloseHandle(file);if(ok)ok=hash_bytes(set.keys,bytes)==header.checksum;
    for(DWORD i=0;ok&&i<header.count;i++){wchar_t normal[BOOKMARK_KEY_CHARS];ok=key_normalize(set.keys[i],normal)&&!memcmp(normal,set.keys[i],sizeof(normal))&&(!i||wcscmp(set.keys[i-1],set.keys[i])<0);}
    if(!ok){free(set.keys);SetLastError(ERROR_INVALID_DATA);return FALSE;}
    set.count=set.capacity=(int)header.count;NativeBookmarksFree(m);m->saved=set;return TRUE;
}
BOOL NativeBookmarksCommit(NativeBookmarks *m,const wchar_t *settings,const wchar_t *server,const wchar_t *account){
    if(!m||!m->editing||!save_set(&m->draft,settings,server,account))return FALSE;
    free(m->saved.keys);m->saved=m->draft;memset(&m->draft,0,sizeof(m->draft));m->editing=FALSE;m->showing=m->saved.count>0;return TRUE;
}
BOOL NativeBookmarksClear(NativeBookmarks *m,const wchar_t *settings,const wchar_t *server,const wchar_t *account){
    BookmarkKeySet empty={0};if(!m||!save_set(&empty,settings,server,account))return FALSE;NativeBookmarksFree(m);return TRUE;
}
