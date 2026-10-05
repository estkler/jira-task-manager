#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "native_lifecycle.h"

static int failures;

static void Expect(BOOL condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static char *ReadBytes(const wchar_t *path)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    DWORD size = GetFileSize(file, NULL), read = 0;
    char *bytes = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1);
    if (!bytes || !ReadFile(file, bytes, size, &read, NULL)) {
        if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
        CloseHandle(file);
        return NULL;
    }
    bytes[read] = 0;
    CloseHandle(file);
    return bytes;
}

static int CountText(const char *haystack, const char *needle)
{
    int count = 0;
    size_t length = strlen(needle);
    for (const char *cursor = haystack; (cursor = strstr(cursor, needle)) != NULL; cursor += length) count++;
    return count;
}

int main(void)
{
    wchar_t temporary[MAX_PATH], root[MAX_PATH], executable[MAX_PATH], application[MAX_PATH], binary[MAX_PATH];
    GetTempPathW(MAX_PATH, temporary);
    GetTempFileNameW(temporary, L"ftl", 0, root);
    DeleteFileW(root);
    Expect(CreateDirectoryW(root, NULL), "temporary root is created");
    swprintf(application,MAX_PATH,L"%ls\\Task Manager",root);CreateDirectoryW(application,NULL);
    swprintf(binary,MAX_PATH,L"%ls\\bin",application);CreateDirectoryW(binary,NULL);
    swprintf(executable, MAX_PATH, L"%ls\\Task Manager.exe", binary);

    NativeLifecycle first = {0};
    Expect(NativeLifecycleStart(&first, executable, L"0.8.0", FALSE), "lifecycle starts");
    wchar_t expected_diagnostics[MAX_PATH],expected_log[MAX_PATH];
    swprintf(expected_diagnostics,MAX_PATH,L"%ls\\diagnostics",application);
    swprintf(expected_log,MAX_PATH,L"%ls\\Task Manager.log",expected_diagnostics);
    Expect(!wcscmp(first.directory,expected_diagnostics),"lifecycle uses the unified app diagnostics folder");
    Expect(!wcscmp(first.log_path,expected_log),"active log uses the Task Manager name");
    Expect(GetFileAttributesW(first.log_path) != INVALID_FILE_ATTRIBUTES, "log file exists");
    Expect(GetFileAttributesW(first.marker_path) != INVALID_FILE_ATTRIBUTES, "running marker exists");

    char *contents = ReadBytes(first.log_path);
    Expect(contents && strstr(contents, "event=start"), "start event is logged");
    Expect(contents && strstr(contents, "version=0.8.0"), "version is logged");
    Expect(contents && strstr(contents, "launch=manual"), "manual launch is logged");
    if (contents) HeapFree(GetProcessHeap(), 0, contents);

    NativeLifecycle second = {0};
    Expect(NativeLifecycleStart(&second, executable, L"0.8.0", TRUE), "second lifecycle starts");
    contents = ReadBytes(second.log_path);
    Expect(contents && CountText(contents, "event=previous_session_unclean") == 1,
        "leftover marker records one unclean previous session");
    Expect(contents && strstr(contents, "launch=autostart"), "autostart launch is logged");
    if (contents) HeapFree(GetProcessHeap(), 0, contents);

    NativeLifecycleCrash(&second, 0xC0000005u);
    contents = ReadBytes(second.log_path);
    Expect(contents && strstr(contents, "event=crash"), "best-effort crash event is logged");
    Expect(contents && strstr(contents, "exception=0xC0000005"), "exception code is logged");
    if (contents) HeapFree(GetProcessHeap(), 0, contents);
    Expect(GetFileAttributesW(second.marker_path) != INVALID_FILE_ATTRIBUTES,
        "crash keeps marker for next-launch detection");

    Expect(NativeLifecycleStop(&second, L"user_exit"), "normal stop succeeds");
    Expect(GetFileAttributesW(second.marker_path) == INVALID_FILE_ATTRIBUTES, "normal stop removes marker");
    contents = ReadBytes(second.log_path);
    Expect(contents && strstr(contents, "event=stop | reason=user_exit"), "normal stop reason is logged");
    if (contents) HeapFree(GetProcessHeap(), 0, contents);

    NativeLifecycle third = {0};
    Expect(NativeLifecycleStart(&third, executable, L"0.8.0", FALSE), "clean third lifecycle starts");
    NativeLifecycleStop(&third, L"test_complete");
    contents = ReadBytes(third.log_path);
    Expect(contents && CountText(contents, "event=previous_session_unclean") == 1,
        "clean stop does not create another unclean event");
    if (contents) HeapFree(GetProcessHeap(), 0, contents);

    NativeLifecycle before_update={0};
    Expect(NativeLifecycleStart(&before_update,executable,L"0.8.0",FALSE),"pre-update lifecycle starts");
    HANDLE planned=CreateFileW(before_update.update_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,
        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    Expect(planned!=INVALID_HANDLE_VALUE,"planned update marker is created");
    if(planned!=INVALID_HANDLE_VALUE)CloseHandle(planned);
    NativeLifecycle after_update={0};
    Expect(NativeLifecycleStart(&after_update,executable,L"0.8.1",FALSE),"update lifecycle starts");
    contents=ReadBytes(after_update.log_path);
    Expect(contents&&strstr(contents,"event=previous_session_updated"),"planned replacement is distinguished from a crash");
    Expect(contents&&CountText(contents,"event=previous_session_unclean")==1,
        "planned replacement does not create another unclean event");
    if(contents)HeapFree(GetProcessHeap(),0,contents);
    Expect(GetFileAttributesW(after_update.update_path)==INVALID_FILE_ATTRIBUTES,
        "planned update marker is consumed");
    NativeLifecycleStop(&after_update,L"test_complete");

    DeleteFileW(third.log_path);
    DeleteFileW(third.marker_path);
    DeleteFileW(third.update_path);
    RemoveDirectoryW(third.directory);RemoveDirectoryW(binary);RemoveDirectoryW(application);
    RemoveDirectoryW(root);

    if (failures) return 1;
    puts("lifecycle_log_test: PASS");
    return 0;
}
