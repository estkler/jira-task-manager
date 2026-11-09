#include "native_auto_refresh.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    NativeRefreshPresence presence={0};assert(NativeAutoRefreshAllowed(TRUE,&presence));
    presence.locked=TRUE;assert(!NativeAutoRefreshAllowed(TRUE,&presence));
    assert(NativeAutoRefreshAllowed(FALSE,&presence));
    presence.suspended=TRUE;assert(!NativeAutoRefreshAllowed(FALSE,&presence));
    presence.locked=FALSE;assert(!NativeAutoRefreshAllowed(TRUE,&presence));
    presence.suspended=FALSE;assert(NativeAutoRefreshAllowed(TRUE,&presence));
    wchar_t folder[MAX_PATH],path[MAX_PATH];GetTempPathW(MAX_PATH,folder);GetTempFileNameW(folder,L"apr",0,path);
    assert(NativeAutoRefreshPauseLoad(path));
    assert(WritePrivateProfileStringW(L"Jira",L"WorkSchedule",L"1;62;540;1080",path));
    assert(NativeAutoRefreshPauseLoad(path)&&"Legacy hours must not determine the new preference");
    assert(NativeAutoRefreshPauseSave(path,FALSE)&&!NativeAutoRefreshPauseLoad(path));
    assert(NativeAutoRefreshPauseSave(path,TRUE)&&NativeAutoRefreshPauseLoad(path));
    DeleteFileW(path);puts("Automatic refresh presence, sleep and preferences passed");
}
