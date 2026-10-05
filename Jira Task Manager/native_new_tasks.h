#ifndef TASK_MANAGER_NEW_TASKS_H
#define TASK_MANAGER_NEW_TASKS_H
#include <windows.h>
#include <stddef.h>

/* The first successful list is a baseline. Later unseen issue keys stay new until opened. */
BOOL NativeNewTasksObserve(const wchar_t *settings_path,const wchar_t *account,
    const wchar_t *const *keys,int count,BOOL *is_new,int *newly_arrived,int *first_arrival);
BOOL NativeNewTaskMarkSeen(const wchar_t *settings_path,const wchar_t *account,const wchar_t *key);
BOOL NativeNewTasksSeedReporter(const wchar_t *settings_path,const wchar_t *account,
    const wchar_t *const *keys,const BOOL *reporter_only,int count);

/* Existing comments form the first-load baseline. A changed latest comment
   stays unread across refreshes and restarts until explicitly opened. */
BOOL NativeCommentObserve(const wchar_t *settings_path,const wchar_t *account,
    const wchar_t *issue_key,const wchar_t *latest_comment_id,BOOL authored_by_me,BOOL *unread);
BOOL NativeCommentMarkSeen(const wchar_t *settings_path,const wchar_t *account,
    const wchar_t *issue_key,const wchar_t *latest_comment_id);

#endif
