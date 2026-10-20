#include <windows.h>
#include <shellapi.h>
#include <assert.h>
static BOOL active_fixture;
static int notices;
static NOTIFYICONDATAW last_notice;
static BOOL WINAPI CaptureNotice(DWORD,PNOTIFYICONDATAW);
static HWND WINAPI FixtureForeground(void);
static BOOL WINAPI FixtureVisible(HWND);
#define Shell_NotifyIconW CaptureNotice
#define GetForegroundWindow FixtureForeground
#define IsWindowVisible FixtureVisible
#include "native.c"
#undef Shell_NotifyIconW
#undef GetForegroundWindow
#undef IsWindowVisible
static BOOL WINAPI CaptureNotice(DWORD message,PNOTIFYICONDATAW notice)
{
    if(message==NIM_MODIFY&&(notice->uFlags&NIF_INFO)){notices++;last_notice=*notice;}
    return TRUE;
}
static HWND WINAPI FixtureForeground(void){return active_fixture?g_window:NULL;}
static BOOL WINAPI FixtureVisible(HWND window){return window==g_window?active_fixture:IsWindowVisible(window);}
static void install(const char *id,BOOL own,const char *second_id,BOOL new_task)
{
    char json[4000];
    snprintf(json,sizeof(json),"{\"startAt\":0,\"total\":%d,\"issues\":["
        "{\"key\":\"ALERT-1\",\"fields\":{\"summary\":\"Finished issue\",\"status\":{\"id\":\"4\",\"name\":\"Done\",\"statusCategory\":{\"key\":\"done\"}},\"comment\":{\"comments\":[{\"id\":\"%s\",\"created\":\"2026-10-13T10:00:00\",\"body\":\"Text\",\"author\":{\"displayName\":\"Same name\",\"key\":\"%s\",\"accountId\":\"%s\"}}]}}},"
        "{\"key\":\"ALERT-2\",\"fields\":{\"summary\":\"Other issue\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}},\"comment\":{\"comments\":[{\"id\":\"%s\",\"created\":\"2026-10-13T10:00:00\",\"body\":\"Text\",\"author\":{\"displayName\":\"Other\",\"key\":\"other-2\"}}]}}}%s]}",
        new_task?3:2,id,own?"self-1":"other-1",own?"self-account":"",second_id,
        new_task?",{\"key\":\"ALERT-3\",\"fields\":{\"summary\":\"New issue\",\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}":"");
    JiraSnapshot snapshot={0};int total,start;assert(JiraParsePage(json,strlen(json),&snapshot,&total,&start));
    wcscpy_s(snapshot.account,256,L"Same name");wcscpy_s(snapshot.identity.key,256,L"self-1");
    for(int i=0;i<snapshot.count;i++)snapshot.items[i].assigned_to_me=TRUE;
    assert(InstallSnapshot(&snapshot));assert(!snapshot.items);
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_tab=1;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    g_window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    wchar_t temporary[MAX_PATH];assert(GetTempPathW(MAX_PATH,temporary));assert(GetTempFileNameW(temporary,L"alt",0,g_settings_path));
    install("10",FALSE,"",FALSE);assert(notices==0);
    install("11",FALSE,"",FALSE);assert(notices==1&&g_tasks[0].unread_comment);
    assert(wcsstr(last_notice.szInfo,L"ALERT-1")&&wcsstr(last_notice.szInfo,L"Finished issue"));
    assert(last_notice.dwInfoFlags&NIIF_NOSOUND);
    install("11",FALSE,"",FALSE);assert(notices==1);
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);install("11",FALSE,"",FALSE);assert(notices==1);
    install("12",TRUE,"",FALSE);assert(notices==1&&!g_tasks[0].unread_comment);
    active_fixture=TRUE;install("13",FALSE,"",FALSE);assert(notices==1&&g_tasks[0].unread_comment);
    active_fixture=FALSE;install("13",FALSE,"",FALSE);assert(notices==1);
    install("14",FALSE,"20",FALSE);assert(notices==2&&wcsstr(last_notice.szInfo,L"2"));
    install("15",FALSE,"20",TRUE);assert(notices==3);
    assert(wcsstr(last_notice.szInfoTitle,L"задачи")&&wcsstr(last_notice.szInfoTitle,L"комментарии"));
    install("15",FALSE,"20",TRUE);assert(notices==3);
    g_notifications.comments=FALSE;install("16",FALSE,"21",TRUE);assert(notices==3&&ScopeActivityCounts(FALSE).unread_tasks==0);
    g_notifications.comments=TRUE;install("16",FALSE,"21",TRUE);assert(notices==3); /* No deferred OS burst after enabling. */
    install("17",FALSE,"21",TRUE);assert(notices==4);
    install("16",FALSE,"20",TRUE);assert(notices==4); /* Deleting newest comments cannot re-notify older ones. */
    install("17",FALSE,"21",TRUE);assert(notices==4);
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    puts("Comment tray alerts: once per arrival, own/shared-name safety, restart/foreground/history suppression, done tasks and combined task/comment notice passed");
    return 0;
}
