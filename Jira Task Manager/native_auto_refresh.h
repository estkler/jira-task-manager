#ifndef TASKLET_AUTO_REFRESH_H
#define TASKLET_AUTO_REFRESH_H
#include <windows.h>
typedef struct NativeRefreshPresence { BOOL locked,suspended; } NativeRefreshPresence;
static BOOL NativeAutoRefreshAllowed(BOOL pause_when_locked,const NativeRefreshPresence *presence)
{return !presence->suspended&&(!pause_when_locked||!presence->locked);}
static BOOL NativeAutoRefreshPauseLoad(const wchar_t *path)
{return GetPrivateProfileIntW(L"Jira",L"PauseWhenLocked",1,path)!=0;}
static BOOL NativeAutoRefreshPauseSave(const wchar_t *path,BOOL enabled)
{return WritePrivateProfileStringW(L"Jira",L"PauseWhenLocked",enabled?L"1":L"0",path);}
#endif
