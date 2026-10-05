#ifndef JIRA_TASK_MANAGER_UPDATE_H
#define JIRA_TASK_MANAGER_UPDATE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#define UPDATE_JSON_LIMIT (1024u*1024u)
#define UPDATE_EXE_LIMIT (16u*1024u*1024u)
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
#endif
