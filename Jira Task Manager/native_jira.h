#ifndef FTASKS_NATIVE_JIRA_H
#define FTASKS_NATIVE_JIRA_H
#include <windows.h>
#define JIRA_MAX_TASKS 10000
#define JIRA_MAX_TRANSITIONS 128
#define JIRA_MAX_RATINGS 16
#define JIRA_MAX_COMMENTS 1000
typedef struct JiraItem {
    wchar_t key[80], summary[1024], status_id[80], status_name[128], category[32];
    wchar_t latest_comment[2048];
    wchar_t latest_comment_id[80],latest_comment_author[256];
    BOOL assigned_to_me,reported_by_me;
} JiraItem;
typedef struct JiraUserIdentity { wchar_t account_id[256],key[256],name[256]; } JiraUserIdentity;
typedef struct JiraSnapshot { JiraItem *items; int count; wchar_t account[256]; JiraUserIdentity identity; } JiraSnapshot;
typedef struct JiraRatingOption { wchar_t id[80], value[64]; } JiraRatingOption;
typedef struct JiraTransitionItem {
    wchar_t id[80], name[128], status_name[128], category[32], rating_field_id[80];
    JiraRatingOption ratings[JIRA_MAX_RATINGS];int rating_count,field_count;
} JiraTransitionItem;
typedef struct JiraTransitions { JiraTransitionItem items[JIRA_MAX_TRANSITIONS]; int count; } JiraTransitions;
typedef struct JiraComment {
    wchar_t id[80], author[256], created[64], body[2048];
    JiraUserIdentity identity;
} JiraComment;
typedef struct JiraComments { JiraComment *items; int count; } JiraComments;
typedef struct JiraConnection {
    wchar_t host[256], prefix[512], error[256];
    wchar_t *token; unsigned short port;
    size_t token_chars;
    BOOL write_attempted;
    void *session, *connection;
    BOOL (*send)(struct JiraConnection *,const wchar_t *,const wchar_t *,const char *,char **,size_t *);
} JiraConnection;
BOOL JiraConnect(JiraConnection *client);
BOOL JiraParseUrl(JiraConnection *client,const wchar_t *url);
int JiraParseInterval(const wchar_t *text);
BOOL JiraGetUrl(wchar_t *url,size_t capacity);
BOOL JiraConnectWithToken(JiraConnection *client,const wchar_t *url,const wchar_t *token);
BOOL JiraSaveAccount(const wchar_t *url,const wchar_t *token);
BOOL JiraHasAccount(void);
BOOL JiraSignOut(void);
BOOL JiraVerifyAccount(JiraConnection *client,wchar_t *account,size_t capacity);
void JiraDisconnect(JiraConnection *client);
BOOL JiraLoad(JiraConnection *client,JiraSnapshot *snapshot);
BOOL JiraLoadWithComments(JiraConnection *client,JiraSnapshot *snapshot,BOOL include_comments);
BOOL JiraLoadWithReporter(JiraConnection *client,JiraSnapshot *snapshot,BOOL include_comments);
BOOL JiraMergeReported(JiraSnapshot *assigned,const JiraSnapshot *reported);
BOOL JiraReadTransitions(JiraConnection *client,const wchar_t *key,JiraTransitions *transitions);
BOOL JiraReadComments(JiraConnection *client,const wchar_t *key,JiraComments *comments);
BOOL JiraCommentIsOwn(const JiraComment *comment,const JiraSnapshot *snapshot);
BOOL JiraPostComment(JiraConnection *client,const wchar_t *key,const wchar_t *comment);
void JiraFreeComments(JiraComments *comments);
BOOL JiraPostTransition(JiraConnection *client,const wchar_t *key,const wchar_t *transition_id);
BOOL JiraPostTransitionWithInput(JiraConnection *client,const wchar_t *key,const wchar_t *transition_id,
    const wchar_t *field_id,const wchar_t *option_id,const wchar_t *comment);
BOOL JiraBuildTransitionBody(const wchar_t *transition_id,const wchar_t *field_id,const wchar_t *option_id,
    const wchar_t *comment,char **body,size_t *length);
BOOL JiraSwitch(JiraConnection *client,const wchar_t *key);
void JiraFreeSnapshot(JiraSnapshot *snapshot);
/* Parse complete responses independently of transport, also used by fixture tests. */
BOOL JiraParsePage(const char *json,size_t length,JiraSnapshot *snapshot,int *total,int *start);
BOOL JiraParseTransitions(const char *json,size_t length,JiraTransitions *transitions);
BOOL JiraParseComments(const char *json,size_t length,JiraComments *comments,int *total,int *start);
BOOL JiraCheckHttpStatus(JiraConnection *client,DWORD status);
#endif
