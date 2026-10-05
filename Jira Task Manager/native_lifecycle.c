#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "native_lifecycle.h"
#include "native_identity.h"

static BOOL AppendLine(NativeLifecycle *lifecycle, const wchar_t *event_name, const wchar_t *details)
{
    if (!lifecycle || !lifecycle->log_path[0] || !event_name || !event_name[0]) return FALSE;

    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t wide[1024];
    int length = swprintf(wide, sizeof(wide) / sizeof(wide[0]),
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u | pid=%lu | event=%ls%ls%ls\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        GetCurrentProcessId(), event_name, details && details[0] ? L" | " : L"", details && details[0] ? details : L"");
    if (length <= 0 || length >= (int)(sizeof(wide) / sizeof(wide[0]))) return FALSE;

    int byte_count = WideCharToMultiByte(CP_UTF8, 0, wide, length, NULL, 0, NULL, NULL);
    if (byte_count <= 0) return FALSE;
    char bytes[3072];
    if (byte_count > (int)sizeof(bytes)) return FALSE;
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, length, bytes, byte_count, NULL, NULL)) return FALSE;

    HANDLE log = CreateFileW(lifecycle->log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL success = WriteFile(log, bytes, (DWORD)byte_count, &written, NULL) && written == (DWORD)byte_count;
    FlushFileBuffers(log);
    CloseHandle(log);
    return success;
}

static BOOL BuildPaths(NativeLifecycle *lifecycle, const wchar_t *executable_path)
{
    if (!lifecycle || !TaskManagerDiagnosticsDirectory(executable_path,lifecycle->directory,MAX_PATH)) return FALSE;
    int log_length=swprintf(lifecycle->log_path,MAX_PATH,L"%ls\\Task Manager.log",lifecycle->directory);
    int marker_length=swprintf(lifecycle->marker_path,MAX_PATH,L"%ls\\Task Manager.running",lifecycle->directory);
    int update_length=swprintf(lifecycle->update_path,MAX_PATH,L"%ls\\Task Manager.planned-update",lifecycle->directory);
    return log_length>0&&log_length<MAX_PATH&&marker_length>0&&marker_length<MAX_PATH&&
        update_length>0&&update_length<MAX_PATH;
}

static BOOL RecentPlannedUpdate(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if(!GetFileAttributesExW(path,GetFileExInfoStandard,&attributes))return FALSE;
    FILETIME current;GetSystemTimeAsFileTime(&current);
    ULARGE_INTEGER now,created;
    now.LowPart=current.dwLowDateTime;now.HighPart=current.dwHighDateTime;
    created.LowPart=attributes.ftLastWriteTime.dwLowDateTime;
    created.HighPart=attributes.ftLastWriteTime.dwHighDateTime;
    return now.QuadPart>=created.QuadPart&&now.QuadPart-created.QuadPart<=5ULL*60*10000000;
}

BOOL NativeLifecycleStart(NativeLifecycle *lifecycle, const wchar_t *executable_path,
    const wchar_t *version, BOOL launched_by_autostart)
{
    if (!lifecycle) return FALSE;
    ZeroMemory(lifecycle, sizeof(*lifecycle));
    if (!BuildPaths(lifecycle, executable_path)) return FALSE;
    if (!CreateDirectoryW(lifecycle->directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    DWORD attributes = GetFileAttributesW(lifecycle->directory);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return FALSE;

    BOOL previous_unclean = GetFileAttributesW(lifecycle->marker_path) != INVALID_FILE_ATTRIBUTES;
    BOOL planned_update=previous_unclean&&RecentPlannedUpdate(lifecycle->update_path);
    if(previous_unclean&&!AppendLine(lifecycle,planned_update?L"previous_session_updated":L"previous_session_unclean",
        planned_update?L"reason=planned_binary_replacement":
        L"reason=missing_clean_stop; probable_crash_or_forced_termination"))return FALSE;
    DeleteFileW(lifecycle->update_path);

    wchar_t details[256];
    swprintf(details, sizeof(details) / sizeof(details[0]), L"version=%ls | launch=%ls",
        version && version[0] ? version : L"unknown", launched_by_autostart ? L"autostart" : L"manual");
    if (!AppendLine(lifecycle, L"start", details)) return FALSE;

    HANDLE marker = CreateFileW(lifecycle->marker_path, GENERIC_WRITE,
        FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (marker == INVALID_HANDLE_VALUE) return FALSE;
    wchar_t marker_text[128];
    int marker_length = swprintf(marker_text, sizeof(marker_text) / sizeof(marker_text[0]),
        L"pid=%lu\r\nversion=%ls\r\n", GetCurrentProcessId(), version && version[0] ? version : L"unknown");
    DWORD written = 0;
    BOOL marker_written = marker_length > 0 && WriteFile(marker, marker_text,
        (DWORD)(marker_length * sizeof(wchar_t)), &written, NULL);
    FlushFileBuffers(marker);
    CloseHandle(marker);
    if (!marker_written) return FALSE;
    lifecycle->started = TRUE;
    return TRUE;
}

BOOL NativeLifecycleEvent(NativeLifecycle *lifecycle, const wchar_t *event_name,
    const wchar_t *details)
{
    if (!lifecycle || !lifecycle->started || lifecycle->stopped) return FALSE;
    return AppendLine(lifecycle, event_name, details);
}

BOOL NativeLifecycleStop(NativeLifecycle *lifecycle, const wchar_t *reason)
{
    if (!lifecycle || !lifecycle->started) return FALSE;
    if (lifecycle->stopped) return TRUE;
    wchar_t details[256];
    swprintf(details, sizeof(details) / sizeof(details[0]), L"reason=%ls",
        reason && reason[0] ? reason : L"normal");
    BOOL logged = AppendLine(lifecycle, L"stop", details);
    BOOL removed = DeleteFileW(lifecycle->marker_path) || GetLastError() == ERROR_FILE_NOT_FOUND;
    lifecycle->stopped = logged && removed;
    return lifecycle->stopped;
}

void NativeLifecycleCrash(NativeLifecycle *lifecycle, DWORD exception_code)
{
    if (!lifecycle || !lifecycle->started || lifecycle->stopped) return;
    wchar_t details[80];
    swprintf(details, sizeof(details) / sizeof(details[0]), L"exception=0x%08lX", exception_code);
    AppendLine(lifecycle, L"crash", details);
    /* Deliberately keep the marker: the next launch independently confirms an unclean stop. */
}
