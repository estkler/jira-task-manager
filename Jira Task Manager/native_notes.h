#ifndef FTASKS_NATIVE_NOTES_H
#define FTASKS_NATIVE_NOTES_H

#include <windows.h>

BOOL NativeNoteLoad(const wchar_t *settings_path,const wchar_t *account,const wchar_t *issue_key,wchar_t *note,size_t capacity);
BOOL NativeNoteSave(const wchar_t *settings_path,const wchar_t *account,const wchar_t *issue_key,const wchar_t *note);
BOOL NativeCompletionTemplateLoad(const wchar_t *settings_path,const wchar_t *account,wchar_t *text,size_t capacity);
BOOL NativeCompletionTemplateSave(const wchar_t *settings_path,const wchar_t *account,const wchar_t *text);

#endif
