#ifndef JIRA_TASK_MANAGER_UPDATE_H
#define JIRA_TASK_MANAGER_UPDATE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#define UPDATE_JSON_LIMIT (1024u*1024u)
#define UPDATE_EXE_LIMIT (16u*1024u*1024u)
#define UPDATE_NOTES_CAP 8192
typedef struct UpdateVersion { DWORD major,minor,patch; } UpdateVersion;
typedef struct UpdateRelease { UpdateVersion version; ULONGLONG asset_id; DWORD size; BYTE sha256[32]; } UpdateRelease;
typedef enum UpdateStatus { UPDATE_OK,UPDATE_NONE,UPDATE_CANCELLED,UPDATE_INVALID,UPDATE_NETWORK,UPDATE_IO,UPDATE_BUSY } UpdateStatus;
BOOL UpdateParseVersion(const wchar_t *text,UpdateVersion *out);
int UpdateCompareVersion(UpdateVersion left,UpdateVersion right);
UpdateStatus UpdateParseRelease(const char *json,size_t length,UpdateVersion current,UpdateRelease *out);
BOOL UpdateUrlAllowed(const wchar_t *url);
typedef struct UpdateCancel { volatile LONG cancelled; } UpdateCancel;
typedef BOOL (*UpdateHttpSink)(const BYTE *bytes,DWORD length,void *context);
UpdateStatus UpdateHttpGet(const wchar_t *url,const wchar_t *accept,DWORD limit,UpdateCancel *cancel,UpdateHttpSink sink,void *context,DWORD *http_status);
UpdateStatus UpdateCheck(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out);
UpdateStatus UpdateDownload(const UpdateRelease *release,HANDLE destination,UpdateCancel *cancel);
/* Optional UI data never enters the persisted helper request/wire structures. */
void UpdateParseNotes(const char *json,size_t length,wchar_t *out,size_t capacity);
typedef void (*UpdateProgress)(DWORD received,DWORD total,void *context);
UpdateStatus UpdateCheckWithNotes(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out,wchar_t *notes,size_t capacity);
UpdateStatus UpdateDownloadWithProgress(const UpdateRelease *release,HANDLE destination,UpdateCancel *cancel,UpdateProgress progress,void *context);
typedef struct UpdateStage { wchar_t directory[MAX_PATH],executable[MAX_PATH]; HANDLE directory_lock; } UpdateStage;
UpdateStatus UpdateStageCreate(UpdateStage *out);
void UpdateStageCleanup(UpdateStage *stage);
UpdateStatus UpdateVerifyFile(HANDLE file,const UpdateRelease *release);
UpdateStatus UpdateInspectFile(HANDLE file,UpdateRelease *out);
BOOL UpdatePathSafe(const wchar_t *path,BOOL directory);
typedef struct UpdateApplyRequest { DWORD parent_pid; ULONGLONG parent_created; wchar_t target[MAX_PATH]; UpdateRelease release; } UpdateApplyRequest;
UpdateStatus UpdateLaunchHelper(const UpdateStage *stage,const UpdateApplyRequest *request,HANDLE *ready_event);
UpdateStatus UpdateLaunchHelperTracked(const UpdateStage *stage,const UpdateApplyRequest *request,HANDLE *ready_event,HANDLE *helper_process);
int UpdateRunHelper(int argc,wchar_t **argv);
#endif
