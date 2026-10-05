#ifndef JIRA_UPDATE_TRANSPORT_H
#define JIRA_UPDATE_TRANSPORT_H
#include "native_update.h"
/* Transport seam keeps release/redirect/size policy independent of OS I/O. */
typedef struct UpdateHttpResponse { DWORD status; BOOL has_length; ULONGLONG length; wchar_t location[8193]; void *request; } UpdateHttpResponse;
typedef struct UpdateTransport {
    UpdateStatus (*open)(void *context,const wchar_t *url,const wchar_t *accept,ULONGLONG deadline,UpdateHttpResponse *out);
    BOOL (*read)(void *request,BYTE *bytes,DWORD capacity,DWORD *read);
    void (*close)(void *request);
} UpdateTransport;
UpdateStatus UpdateHttpGetUsing(const wchar_t *url,const wchar_t *accept,DWORD limit,UpdateCancel *cancel,UpdateHttpSink sink,void *context,DWORD *http_status,const UpdateTransport *transport,void *transport_context);
UpdateStatus UpdateCheckUsing(UpdateVersion current,UpdateCancel *cancel,UpdateRelease *out,const UpdateTransport *transport,void *context);
UpdateStatus UpdateDownloadUsing(const UpdateRelease *release,HANDLE destination,UpdateCancel *cancel,const UpdateTransport *transport,void *context);
#endif
