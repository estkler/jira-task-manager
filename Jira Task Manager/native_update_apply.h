#ifndef JIRA_TASK_MANAGER_UPDATE_APPLY_H
#define JIRA_TASK_MANAGER_UPDATE_APPLY_H
#include "native_update.h"
/* Internal OS seam: tests inject only rename/restart failures and shorten the
   parent timeout. Public helper always uses the normal Windows operations. */
typedef struct UpdateApplyOps {
    BOOL (*move)(const wchar_t *source,const wchar_t *target);
    BOOL (*start)(const wchar_t *target,const wchar_t *directory);
    DWORD parent_timeout_ms;
    BOOL (*nonce)(wchar_t out[33]);
} UpdateApplyOps;
int UpdateRunHelperUsing(int argc,wchar_t **argv,const UpdateApplyOps *ops);
#endif
