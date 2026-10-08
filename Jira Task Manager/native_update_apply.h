#ifndef JIRA_TASK_MANAGER_UPDATE_APPLY_H
#define JIRA_TASK_MANAGER_UPDATE_APPLY_H
#include "native_update.h"
typedef enum UpdateApplyPhase { UPDATE_APPLY_CHECK_TARGET,UPDATE_APPLY_CREATE,UPDATE_APPLY_COPY,UPDATE_APPLY_VERIFY,UPDATE_APPLY_BACKUP,UPDATE_APPLY_REPLACE,UPDATE_APPLY_START } UpdateApplyPhase;
typedef struct UpdateApplyFailure {
    UpdateApplyPhase phase;DWORD windows_error,rollback_error;BOOL original_available;
    wchar_t target[MAX_PATH],backup[MAX_PATH];
} UpdateApplyFailure;
BOOL UpdateFormatApplyFailure(const UpdateApplyFailure *failure,BOOL russian,wchar_t *text,size_t capacity);
/* Internal OS seam: injected I/O and reporting keep fixtures isolated from the
   real user interface. Public helper always uses normal Windows operations. */
typedef struct UpdateApplyOps {
    BOOL (*move)(const wchar_t *source,const wchar_t *target);
    BOOL (*start)(const wchar_t *target,const wchar_t *directory);
    DWORD parent_timeout_ms;
    BOOL (*nonce)(wchar_t out[33]);
    BOOL (*copy)(HANDLE source,HANDLE target);
    void (*report)(const UpdateApplyFailure *failure);
} UpdateApplyOps;
int UpdateRunHelperUsing(int argc,wchar_t **argv,const UpdateApplyOps *ops);
#endif
