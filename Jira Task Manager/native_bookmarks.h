#ifndef TASK_MANAGER_BOOKMARKS_H
#define TASK_MANAGER_BOOKMARKS_H
#include <windows.h>
#define BOOKMARK_KEY_CHARS 80
#define BOOKMARK_MAX_KEYS 10000
typedef struct BookmarkKeySet {wchar_t (*keys)[BOOKMARK_KEY_CHARS];int count,capacity;} BookmarkKeySet;
typedef struct NativeBookmarks {
    BookmarkKeySet saved,draft;
    BOOL editing,showing,previous_showing;
} NativeBookmarks;
void NativeBookmarksFree(NativeBookmarks *model);
BOOL NativeBookmarksBegin(NativeBookmarks *model);
void NativeBookmarksCancel(NativeBookmarks *model);
BOOL NativeBookmarksContains(const NativeBookmarks *model,const wchar_t *key);
BOOL NativeBookmarksSet(NativeBookmarks *model,const wchar_t *key,BOOL selected);
BOOL NativeBookmarksDirty(const NativeBookmarks *model);
int NativeBookmarksCount(const NativeBookmarks *model);
BOOL NativeBookmarksLoad(NativeBookmarks *model,const wchar_t *settings_path,const wchar_t *server,const wchar_t *account);
BOOL NativeBookmarksCommit(NativeBookmarks *model,const wchar_t *settings_path,const wchar_t *server,const wchar_t *account);
BOOL NativeBookmarksClear(NativeBookmarks *model,const wchar_t *settings_path,const wchar_t *server,const wchar_t *account);
#endif
