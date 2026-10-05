#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include "native_notes.h"

int main(void)
{
    wchar_t folder[MAX_PATH],path[MAX_PATH],loaded[2048],raw[8192];
    GetTempPathW(MAX_PATH,folder);GetTempFileNameW(folder,L"ftn",0,path);DeleteFileW(path);wcscat_s(path,MAX_PATH,L".ini");
    HANDLE seed=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);assert(seed!=INVALID_HANDLE_VALUE);CloseHandle(seed);
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L"Локальная заметка ✓"));
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!wcscmp(loaded,L"Локальная заметка ✓"));
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L""));
    wcscpy_s(loaded,_countof(loaded),L"stale");
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!loaded[0]);
    assert(NativeCompletionTemplateSave(path,L"Roman Gulyaev",L"Работа завершена. Материалы загружены."));
    assert(NativeCompletionTemplateLoad(path,L"Roman Gulyaev",loaded,_countof(loaded)));
    assert(!wcscmp(loaded,L"Работа завершена. Материалы загружены."));
    GetPrivateProfileSectionW(L"Completion",raw,_countof(raw),path);
    assert(!wcsstr(raw,L"Работа завершена"));
    wcscpy_s(loaded,_countof(loaded),L"stale");
    assert(NativeCompletionTemplateLoad(path,L"Another User",loaded,_countof(loaded)));
    assert(!loaded[0]);
    assert(NativeCompletionTemplateSave(path,L"Roman Gulyaev",L""));
    wcscpy_s(loaded,_countof(loaded),L"stale");
    assert(NativeCompletionTemplateLoad(path,L"Roman Gulyaev",loaded,_countof(loaded)));
    assert(!loaded[0]);
    DeleteFileW(path);puts("Encrypted local note tests passed");return 0;
}
