#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <wincrypt.h>
#include "native_notes.h"

static void seed_legacy(const wchar_t *path)
{
    wchar_t text[]=L"Старая заметка ✓",encoded[8192];BYTE bytes[]="fTasks.LocalNotes.v1";
    DATA_BLOB input={sizeof(text),(BYTE*)text},output={0},entropy={sizeof(bytes)-1,bytes};
    assert(CryptProtectData(&input,L"fTasks local note",&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output));
    DWORD count=_countof(encoded);
    assert(CryptBinaryToStringW(output.pbData,output.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded,&count));
    assert(WritePrivateProfileStringW(L"Notes",L"note_ba6a99d5550b1a50",encoded,path));LocalFree(output.pbData);
}

int main(void)
{
    wchar_t folder[MAX_PATH],path[MAX_PATH],loaded[2048],raw[8192];
    GetTempPathW(MAX_PATH,folder);GetTempFileNameW(folder,L"ftn",0,path);DeleteFileW(path);wcscat_s(path,MAX_PATH,L".ini");
    HANDLE seed=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);assert(seed!=INVALID_HANDLE_VALUE);CloseHandle(seed);
    seed_legacy(path);
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!wcscmp(loaded,L"Старая заметка ✓"));
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L"Локальная заметка ✓"));
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!wcscmp(loaded,L"Локальная заметка ✓"));
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L""));
    wcscpy_s(loaded,_countof(loaded),L"stale");
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!loaded[0]);
    /* Long Unicode notes must survive encrypted storage without truncation. */
    size_t length=100000;
    wchar_t *long_note=calloc(length+1,sizeof(wchar_t)),*roundtrip=calloc(length+1,sizeof(wchar_t));
    assert(long_note&&roundtrip);
    const wchar_t pattern[]=L"Заметка \xD83E\xDD17\r\n";
    for(size_t i=0;i<length;i++)long_note[i]=pattern[i%(_countof(pattern)-1)];
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",long_note));
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",roundtrip,length+1));
    assert(!wcscmp(long_note,roundtrip));
    wchar_t sidecar[MAX_PATH];swprintf(sidecar,_countof(sidecar),L"%ls.note_ba6a99d5550b1a50.dat",path);
    HANDLE locked=CreateFileW(sidecar,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(locked!=INVALID_HANDLE_VALUE);
    assert(!NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L"Must not replace the original"));CloseHandle(locked);
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",roundtrip,length+1)&&!wcscmp(roundtrip,long_note));
    wchar_t *allocated=NULL;assert(NativeNoteLoadAllocated(path,L"Roman Gulyaev",L"PDES-6271",&allocated));
    assert(!wcscmp(allocated,long_note));free(allocated);
    assert(NativeNoteLoad(path,L"Another User",L"PDES-6271",loaded,_countof(loaded))&&!loaded[0]);
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-OTHER",loaded,_countof(loaded))&&!loaded[0]);
    assert(!NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",loaded,_countof(loaded)));
    assert(!loaded[0]);
    seed_legacy(path); /* Clearing must not resurrect a stale legacy entry. */
    assert(NativeNoteSave(path,L"Roman Gulyaev",L"PDES-6271",L""));
    assert(NativeNoteLoad(path,L"Roman Gulyaev",L"PDES-6271",roundtrip,length+1));
    assert(!roundtrip[0]);free(long_note);free(roundtrip);
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
    assert(DeleteFileW(sidecar));DeleteFileW(path);puts("Encrypted local note tests passed");return 0;
}
