#ifndef JIRA_NATIVE_SNAPSHOT_CACHE_H
#define JIRA_NATIVE_SNAPSHOT_CACHE_H
#include "native_jira.h"
#define JIRA_SNAPSHOT_CACHE_MAX_BYTES (128u*1024u*1024u)
#define JIRA_SNAPSHOT_CACHE_MAX_AGE (7ULL*24*60*60*10000000)
BOOL JiraSnapshotCacheLoad(const JiraConnection *client,const JiraSnapshot *verified,const wchar_t *filter,JiraSnapshot *out);
BOOL JiraSnapshotCacheSave(const JiraConnection *client,const JiraSnapshot *snapshot);
#endif
