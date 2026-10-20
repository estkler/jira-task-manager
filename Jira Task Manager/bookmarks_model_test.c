#include "native_bookmarks.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
int main(void){
    NativeBookmarks m={0},loaded={0};
    assert(!NativeBookmarksCount(&m)&&!m.editing&&!m.showing);
    assert(!NativeBookmarksSet(&m,L"PDES-1",TRUE)&&!NativeBookmarksCount(&m));
    assert(NativeBookmarksBegin(&m)&&m.editing&&!m.showing);
    assert(NativeBookmarksSet(&m,L"PDES-6507",TRUE)&&NativeBookmarksSet(&m,L"pdes-6529",TRUE));
    assert(NativeBookmarksDirty(&m)&&NativeBookmarksCount(&m)==2&&NativeBookmarksContains(&m,L"PDES-6529"));
    assert(NativeBookmarksSet(&m,L"PDES-6507",TRUE)&&NativeBookmarksCount(&m)==2);
    assert(NativeBookmarksSet(&m,L"PDES-6507",42)&&NativeBookmarksCount(&m)==2);
    assert(!NativeBookmarksSet(&m,L"bad\nkey",TRUE)&&!NativeBookmarksSet(&m,L"",TRUE));
    NativeBookmarksCancel(&m);assert(!m.editing&&!NativeBookmarksCount(&m));
    wchar_t temp[MAX_PATH],root[MAX_PATH],settings[MAX_PATH];GetTempPathW(MAX_PATH,temp);assert(GetTempFileNameW(temp,L"bmk",0,root));assert(DeleteFileW(root)&&CreateDirectoryW(root,NULL));
    swprintf(settings,MAX_PATH,L"%ls\\settings.ini",root);
    assert(NativeBookmarksLoad(&m,settings,L"https://jira.example/a",L"account-a")&&!NativeBookmarksCount(&m));
    assert(NativeBookmarksBegin(&m));assert(NativeBookmarksSet(&m,L"PDES-6507",TRUE)&&NativeBookmarksSet(&m,L"PDES-6529",TRUE));
    assert(NativeBookmarksCommit(&m,settings,L"https://jira.example/a",L"account-a")&&!m.editing&&m.showing);
    assert(NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&loaded)==2&&!loaded.showing);
    assert(NativeBookmarksContains(&loaded,L"PDES-6507")&&NativeBookmarksContains(&loaded,L"PDES-6529"));
    assert(NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-b")&&!NativeBookmarksCount(&loaded));
    assert(NativeBookmarksLoad(&loaded,settings,L"https://jira.example/b",L"account-a")&&!NativeBookmarksCount(&loaded));
    assert(NativeBookmarksBegin(&m));assert(NativeBookmarksSet(&m,L"PDES-6507",FALSE));assert(NativeBookmarksSet(&m,L"PDES-6507",TRUE));assert(!NativeBookmarksDirty(&m));
    assert(NativeBookmarksSet(&m,L"PDES-6532",TRUE));
    assert(!NativeBookmarksCommit(&m,L"Z:\\unavailable\\settings.ini",L"https://jira.example/a",L"account-a")&&m.editing&&NativeBookmarksCount(&m)==3);
    NativeBookmarksCancel(&m);assert(NativeBookmarksCount(&m)==2&&m.showing);
    assert(NativeBookmarksClear(&m,settings,L"https://jira.example/a",L"account-a")&&!NativeBookmarksCount(&m)&&!m.showing);
    assert(NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&!NativeBookmarksCount(&loaded));
    assert(NativeBookmarksBegin(&m));for(int i=0;i<1200;i++){wchar_t key[80];swprintf(key,80,L"LARGE-%d",i);assert(NativeBookmarksSet(&m,key,TRUE));}
    assert(NativeBookmarksCommit(&m,settings,L"https://jira.example/a",L"account-a"));
    assert(NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&loaded)==1200&&NativeBookmarksContains(&loaded,L"LARGE-1199"));
    wchar_t missing[MAX_PATH];swprintf(missing,MAX_PATH,L"%ls\\missing\\settings.ini",root);
    assert(!NativeBookmarksClear(&m,missing,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&m)==1200);
    /* Corruption, oversized record counts and truncation cannot replace a valid in-memory set. */
    wchar_t saved_pattern[MAX_PATH],saved_path[MAX_PATH];swprintf(saved_pattern,MAX_PATH,L"%ls\\bookmarks-*.dat",root);WIN32_FIND_DATAW item;HANDLE scan=FindFirstFileW(saved_pattern,&item);assert(scan!=INVALID_HANDLE_VALUE);FindClose(scan);
    swprintf(saved_path,MAX_PATH,L"%ls\\%ls",root,item.cFileName);HANDLE file=CreateFileW(saved_path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    DWORD length=GetFileSize(file,NULL),bytes;BYTE *original=malloc(length);assert(original&&ReadFile(file,original,length,&bytes,NULL)&&bytes==length);
    BYTE broken=0;SetFilePointer(file,0,NULL,FILE_BEGIN);assert(WriteFile(file,&broken,1,&bytes,NULL));CloseHandle(file);
    assert(!NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&loaded)==1200);
    file=CreateFileW(saved_path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(file!=INVALID_HANDLE_VALUE);assert(WriteFile(file,original,16,&bytes,NULL));CloseHandle(file);
    assert(!NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&loaded)==1200);
    DWORD oversized=10001;memcpy(original+8,&oversized,sizeof(oversized));file=CreateFileW(saved_path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(file!=INVALID_HANDLE_VALUE);assert(WriteFile(file,original,length,&bytes,NULL));CloseHandle(file);free(original);
    assert(!NativeBookmarksLoad(&loaded,settings,L"https://jira.example/a",L"account-a")&&NativeBookmarksCount(&loaded)==1200);
    assert(NativeBookmarksBegin(&m));for(int i=0;i<8800;i++){wchar_t key[80];swprintf(key,80,L"BOUND-%d",i);assert(NativeBookmarksSet(&m,key,TRUE));}
    assert(NativeBookmarksCount(&m)==10000&&!NativeBookmarksSet(&m,L"OVERFLOW-1",TRUE)&&NativeBookmarksCount(&m)==10000);NativeBookmarksCancel(&m);
    NativeBookmarksFree(&m);NativeBookmarksFree(&loaded);
    wchar_t pattern[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\bookmarks-*.dat",root);WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);
    if(search!=INVALID_HANDLE_VALUE){do{wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",root,found.cFileName);DeleteFileW(path);}while(FindNextFileW(search,&found));FindClose(search);}
    RemoveDirectoryW(root);puts("Bookmark model: stable keys, transactions, restart, isolation and large sets passed");return 0;
}
