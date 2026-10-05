#include "native.c"
#include <assert.h>
int main(void){
    g_status_count=6;g_dark=FALSE;
    g_status_categories[4]=0; /* Jira ordering is not demo enum ordering. */
    g_status_categories[1]=2;g_status_categories[0]=1;
    COLORREF bg,fg;
    StatusColors((TaskStatus)4,&bg,&fg);assert(bg==RGB(207,231,255));
    StatusColors((TaskStatus)1,&bg,&fg);assert(bg==RGB(221,242,227));
    StatusColors((TaskStatus)0,&bg,&fg);assert(bg==RGB(255,240,201));
    g_dark=TRUE;
    StatusColors((TaskStatus)4,&bg,&fg);assert(bg==RGB(33,74,115));
    StatusColors((TaskStatus)1,&bg,&fg);assert(bg==RGB(31,82,56));
    g_status_count=1;wcscpy_s(g_status_ids[0],80,L"keep");wcscpy_s(g_status_names[0],128,L"Original");g_status_categories[0]=0;
    JiraSnapshot too_many={0};too_many.count=66;too_many.items=calloc(66,sizeof(JiraItem));
    for(int i=0;i<66;i++){
        swprintf(too_many.items[i].status_id,80,L"id-%d",i);
        wcscpy_s(too_many.items[i].status_name,128,L"New");wcscpy_s(too_many.items[i].category,32,L"done");
    }
    wcscpy_s(too_many.items[0].status_id,80,L"keep");
    assert(!InstallSnapshot(&too_many));
    assert(!wcscmp(g_status_names[0],L"Original")&&g_status_categories[0]==0&&g_status_count==1);
    JiraFreeSnapshot(&too_many);
    puts("Status category colors passed in both themes");
}
