#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include "native_new_tasks.h"

int main(void)
{
    wchar_t directory[MAX_PATH],path[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,directory));
    assert(GetTempFileNameW(directory,L"ntt",0,path));
    const wchar_t *first[]={L"PDES-1",L"PDES-2"};
    BOOL flags[3]={TRUE,TRUE,TRUE};int arrived=-1,first_arrival=-1;
    assert(NativeNewTasksObserve(path,L"alice",first,2,flags,&arrived,&first_arrival));
    assert(arrived==0&&first_arrival==-1&&!flags[0]&&!flags[1]);
    const wchar_t *second[]={L"PDES-1",L"PDES-2",L"PDES-3"};
    assert(NativeNewTasksObserve(path,L"alice",second,3,flags,&arrived,&first_arrival));
    assert(arrived==1&&first_arrival==2&&!flags[0]&&!flags[1]&&flags[2]);
    assert(NativeNewTasksObserve(path,L"alice",second,3,flags,&arrived,&first_arrival));
    assert(arrived==0&&first_arrival==-1&&flags[2]);
    assert(NativeNewTaskMarkSeen(path,L"alice",L"PDES-3"));
    assert(NativeNewTasksObserve(path,L"alice",second,3,flags,&arrived,&first_arrival));
    assert(arrived==0&&!flags[2]);
    assert(NativeNewTasksObserve(path,L"bob",second,3,flags,&arrived,&first_arrival));
    assert(arrived==0&&!flags[0]&&!flags[1]&&!flags[2]);
    const wchar_t *reporter[]={L"PDES-1",L"PDES-4"};BOOL reporter_only[]={FALSE,TRUE};
    assert(NativeNewTasksSeedReporter(path,L"alice",reporter,reporter_only,2));
    assert(NativeNewTasksObserve(path,L"alice",reporter,2,flags,&arrived,&first_arrival));
    assert(arrived==0&&!flags[1]);
    const wchar_t *next_reporter[]={L"PDES-4",L"PDES-5"};
    assert(NativeNewTasksSeedReporter(path,L"alice",next_reporter,reporter_only,2));
    assert(NativeNewTasksObserve(path,L"alice",next_reporter,2,flags,&arrived,&first_arrival));
    assert(arrived==1&&first_arrival==1&&flags[1]);
    BOOL unread=TRUE;
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"100",FALSE,&unread)&&!unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"101",FALSE,&unread)&&unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"101",FALSE,&unread)&&unread);
    assert(NativeCommentMarkSeen(path,L"alice",L"PDES-1",L"101"));
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"101",FALSE,&unread)&&!unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"102",TRUE,&unread)&&!unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-1",L"103",FALSE,&unread)&&unread);
    assert(NativeCommentObserve(path,L"bob",L"PDES-1",L"103",FALSE,&unread)&&!unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-empty",L"",FALSE,&unread)&&!unread);
    assert(NativeCommentObserve(path,L"alice",L"PDES-empty",L"200",FALSE,&unread)&&unread);
    DeleteFileW(path);
    return 0;
}
