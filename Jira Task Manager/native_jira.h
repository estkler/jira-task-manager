#ifndef FTASKS_NATIVE_JIRA_H
#define FTASKS_NATIVE_JIRA_H
#include <windows.h>
#include "native_images.h"
#include "native_avatar_cache.h"
#define JIRA_MAX_TASKS 10000
#define JIRA_MAX_TRANSITIONS 128
#define JIRA_MAX_RATINGS 16
#define JIRA_MAX_COMMENTS 1000
typedef struct JiraUserIdentity { wchar_t account_id[256],key[256],name[256]; } JiraUserIdentity;
typedef struct JiraItem {
    wchar_t key[80], summary[1024], status_id[80], status_name[128], category[32];
    wchar_t latest_comment[2048];
    wchar_t latest_comment_id[80],latest_comment_author[256];
    JiraUserIdentity latest_comment_identity;
    wchar_t latest_comment_email[256],latest_comment_created[64];
    BOOL assigned_to_me,reported_by_me;
} JiraItem;
typedef struct JiraSnapshot { JiraItem *items; int count; wchar_t account[256]; JiraUserIdentity identity; wchar_t server[800]; } JiraSnapshot;
typedef struct JiraRatingOption { wchar_t id[80], value[64]; } JiraRatingOption;
typedef struct JiraTransitionItem {
    wchar_t id[80], name[128], status_name[128], category[32], rating_field_id[80];
    JiraRatingOption ratings[JIRA_MAX_RATINGS];int rating_count,field_count;
    wchar_t status_id[80];
} JiraTransitionItem;
typedef struct JiraTransitions { JiraTransitionItem items[JIRA_MAX_TRANSITIONS]; int count; } JiraTransitions;
#define JIRA_COMMENT_MAX_IMAGES 4
#define JIRA_FEED_MAX_IMAGES 16
typedef struct JiraCommentImage {
    wchar_t reference[512],filename[256],attachment_id[80];
    int kind,error; /* reference: 1 filename, 2 URL, 3 attachment/media ID; error: 1 unavailable, 2 size, 3 unsafe, 4 decode, 5 limit */
    NativeImage *image;
} JiraCommentImage;
typedef struct JiraComment {
    wchar_t id[80], author[256], created[64], body[2048];
    JiraUserIdentity identity;
    wchar_t email[256];
    JiraCommentImage *images;int image_count;
    wchar_t avatar_url[512],profile_url[1024];NativeImage *avatar;
} JiraComment;
typedef struct JiraComments { JiraComment *items; int count; } JiraComments;
typedef struct JiraConnection {
    wchar_t host[256], prefix[512], error[256];
    wchar_t *token; unsigned short port;
    size_t token_chars;
    BOOL write_attempted;
    const wchar_t *excluded_emails; /* Borrowed for the connection lifetime; NULL uses the default. */
    const volatile LONG *cancelled;
    size_t *image_bytes_remaining; /* Borrowed transfer budget, charged even on partial failures. */
    NativeAvatarCache avatar_cache; /* Server/account-scoped encoded media only; never credentials. */
    void *session, *connection;
    BOOL (*send)(struct JiraConnection *,const wchar_t *,const wchar_t *,const char *,char **,size_t *);
    /* Optional local notification journal. Failure prevents an untracked POST. */
    BOOL (*before_status_write)(void *,const wchar_t *,const wchar_t *,const wchar_t *);
    void (*after_status_write)(void *,BOOL);
    void *status_write_context;
    BOOL request_sent;DWORD response_status; /* Last request, not cumulative write_attempted. */
    BOOL switch_unavailable;wchar_t switch_status[128]; /* Successful preflight, but no start/pause transition. */
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
void JiraLoadCommentImages(JiraConnection *client,const wchar_t *key,JiraComments *comments);
void JiraLoadCommentAuthorMedia(JiraConnection *client,JiraComments *comments);
BOOL JiraConfigureAvatarCache(JiraConnection *client,const wchar_t *settings_path,const wchar_t *account_identity);
BOOL JiraCommentIsOwn(const JiraComment *comment,const JiraSnapshot *snapshot);
BOOL JiraLatestCommentIsOwn(const JiraItem *item,const JiraSnapshot *snapshot);
void JiraFilterComments(JiraComments *comments,const wchar_t *excluded_emails);
BOOL JiraPostComment(JiraConnection *client,const wchar_t *key,const wchar_t *comment);
void JiraFreeComments(JiraComments *comments);
BOOL JiraPostTransition(JiraConnection *client,const wchar_t *key,const wchar_t *transition_id);
BOOL JiraPostTransitionWithInput(JiraConnection *client,const wchar_t *key,const wchar_t *transition_id,
    const wchar_t *field_id,const wchar_t *option_id,const wchar_t *comment);
BOOL JiraPostKnownTransitionWithInput(JiraConnection *client,const wchar_t *key,const JiraTransitionItem *transition,
    const wchar_t *field_id,const wchar_t *option_id,const wchar_t *comment);
BOOL JiraBuildTransitionBody(const wchar_t *transition_id,const wchar_t *field_id,const wchar_t *option_id,
    const wchar_t *comment,char **body,size_t *length);
BOOL JiraSwitch(JiraConnection *client,const wchar_t *key);
void JiraFreeSnapshot(JiraSnapshot *snapshot);
/* Parse complete responses independently of transport, also used by fixture tests. */
BOOL JiraParsePage(const char *json,size_t length,JiraSnapshot *snapshot,int *total,int *start);
BOOL JiraParsePageFiltered(const char *json,size_t length,JiraSnapshot *snapshot,int *total,int *start,const wchar_t *excluded_emails);
BOOL JiraParseTransitions(const char *json,size_t length,JiraTransitions *transitions);
BOOL JiraParseComments(const char *json,size_t length,JiraComments *comments,int *total,int *start);
BOOL JiraCheckHttpStatus(JiraConnection *client,DWORD status);
#endif
