#include "native_jira.h"
#include "native_notes.h"
#include "native_status_activity.h"
#include "native_status_marks.h"
#include <assert.h>
#include <stdio.h>
static JiraSnapshot Snapshot(const wchar_t *status,const wchar_t *name)
{
    JiraSnapshot s={0};wcscpy_s(s.server,800,L"fixture.invalid:443");wcscpy_s(s.identity.key,256,L"fixture");
    s.count=2;s.items=calloc(2,sizeof(*s.items));assert(s.items);
    for(int i=0;i<2;i++){swprintf(s.items[i].key,80,L"TASK-%d",i+1);wcscpy_s(s.items[i].status_id,80,status);wcscpy_s(s.items[i].status_name,128,name);}
    return s;
}
int main(void)
{
    wchar_t directory[MAX_PATH],path[MAX_PATH];assert(GetTempPathW(MAX_PATH,directory)&&GetTempFileNameW(directory,L"smk",0,path));
    JiraSnapshot empty={0},before=Snapshot(L"progress",L"In Progress"),after=Snapshot(L"upload",L"Upload project");
    NativeStatusMarks model={0};assert(NativeStatusMarksObserve(&model,path,&empty,&before,NULL,TRUE,TRUE));assert(!model.count);
    assert(NativeStatusMarksObserve(&model,path,&before,&after,NULL,TRUE,TRUE));assert(model.count==2);
    const NativeStatusMark *mark=NativeStatusMarksFind(&model,L"TASK-1");assert(mark&&!wcscmp(mark->previous,L"In Progress")&&!wcscmp(mark->current,L"Upload project"));
    assert(NativeStatusMarksObserve(&model,path,&after,&after,NULL,TRUE,TRUE)&&model.count==2);
    NativeStatusMarksFree(&model);assert(NativeStatusMarksObserve(&model,path,&empty,&after,NULL,TRUE,TRUE)&&model.count==2);
    NativeStatusMark shown=*NativeStatusMarksFind(&model,L"TASK-1");
    assert(NativeStatusMarksAcknowledge(&model,path,&shown)&&model.count==1);
    NativeStatusMarksFree(&model);assert(NativeStatusMarksObserve(&model,path,&empty,&after,NULL,TRUE,TRUE)&&model.count==1);
    JiraSnapshot renamed=Snapshot(L"upload",L"Upload: проект\nсрок");
    assert(NativeStatusMarksObserve(&model,path,&after,&renamed,NULL,TRUE,TRUE));
    assert(!wcscmp(NativeStatusMarksFind(&model,L"TASK-2")->current,L"Upload: проект\nсрок")&&"An unseen mark must follow the current label of its stable status ID");
    NativeStatusMarksFree(&model);assert(NativeStatusMarksObserve(&model,path,&empty,&renamed,NULL,TRUE,TRUE)&&model.count==1);
    assert(!wcscmp(NativeStatusMarksFind(&model,L"TASK-2")->current,L"Upload: проект\nсрок"));
    JiraSnapshot done=Snapshot(L"done",L"Done");NativeStatusWrites own={.scope=after};own.scope.items=NULL;
    assert(NativeStatusRemember(&own,L"TASK-2",L"done",L"Done"));
    assert(NativeStatusMarksObserve(&model,path,&after,&done,&own,TRUE,TRUE));assert(!NativeStatusMarksFind(&model,L"TASK-2")&&NativeStatusMarksFind(&model,L"TASK-1"));
    assert(!NativeStatusMarksAcknowledge(&model,path,&shown)&&"An old popup must not acknowledge a newer transition");
    JiraSnapshot sparse=Snapshot(L"review",L"In Review");sparse.count=1;
    assert(NativeStatusMarksObserve(&model,path,&done,&sparse,NULL,TRUE,FALSE));assert(!NativeStatusMarksFind(&model,L"TASK-1"));
    assert(NativeStatusMarksObserve(&model,path,&done,&after,NULL,TRUE,TRUE)&&model.count==2);
    shown=*NativeStatusMarksFind(&model,L"TASK-1");
    assert(!NativeStatusMarksAcknowledge(&model,L"Z:/nonexistent-tasklet-folder/settings.ini",&shown)&&NativeStatusMarksFind(&model,L"TASK-1"));
    wcscpy_s(sparse.server,800,L"other.invalid:443");assert(NativeStatusMarksObserve(&model,path,&after,&sparse,NULL,TRUE,TRUE)&&!model.count);
    assert(NativeStatusMarksObserve(&model,path,&empty,&after,NULL,TRUE,TRUE)&&model.count==2);
    assert(NativeStatusMarksObserve(&model,path,&after,&done,NULL,FALSE,TRUE)&&!model.count);
    NativeStatusMarksFree(&model);free(own.items);JiraFreeSnapshot(&before);JiraFreeSnapshot(&after);JiraFreeSnapshot(&done);JiraFreeSnapshot(&renamed);JiraFreeSnapshot(&sparse);DeleteFileW(path);
    puts("Status marks: baseline, external changes, coalescing, encrypted restart, own Done, stale acknowledgement, failed save, sparse patches and scope isolation passed");
    return 0;
}
