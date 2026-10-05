#ifndef FTASKS_NATIVE_LIFECYCLE_H
#define FTASKS_NATIVE_LIFECYCLE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct NativeLifecycle {
    wchar_t directory[MAX_PATH];
    wchar_t log_path[MAX_PATH];
    wchar_t marker_path[MAX_PATH];
    wchar_t update_path[MAX_PATH];
    BOOL started;
    BOOL stopped;
} NativeLifecycle;

BOOL NativeLifecycleStart(NativeLifecycle *lifecycle, const wchar_t *executable_path,
    const wchar_t *version, BOOL launched_by_autostart);
BOOL NativeLifecycleEvent(NativeLifecycle *lifecycle, const wchar_t *event_name,
    const wchar_t *details);
BOOL NativeLifecycleStop(NativeLifecycle *lifecycle, const wchar_t *reason);
void NativeLifecycleCrash(NativeLifecycle *lifecycle, DWORD exception_code);

#endif
