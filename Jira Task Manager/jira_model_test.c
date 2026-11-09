#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_jira.h"
static void due_fields(void){
    const char *values[]={"\"2026-11-12\"","null","\"2026-02-29\"","\"2026-11-12extra\"","\"2024-02-29\""};
    const wchar_t *expected[]={L"2026-11-12",L"",L"",L"",L"2024-02-29"};
    for(int i=0;i<5;i++){
        char page[1024];snprintf(page,sizeof(page),"{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"DUE-1\",\"fields\":{\"summary\":\"Due fixture\",\"duedate\":%s,\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}",values[i]);
        JiraSnapshot snapshot={0};int total,start;assert(JiraParsePage(page,strlen(page),&snapshot,&total,&start));
        assert(snapshot.count==1&&!wcscmp(snapshot.items[0].due_date,expected[i]));JiraFreeSnapshot(&snapshot);
    }
    const char *created[]={"\"2026-11-01T12:00:00.000+0300\"","\"2026-11-01T09:00:00Z\"","null","\"bad\""};
    for(int i=0;i<4;i++){
        char page[1024];snprintf(page,sizeof(page),"{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"DUE-1\",\"fields\":{\"summary\":\"Created fixture\",\"created\":%s,\"status\":{\"id\":\"1\",\"name\":\"To Do\",\"statusCategory\":{\"key\":\"new\"}}}}]}",created[i]);
        JiraSnapshot snapshot={0};int total,start;assert(JiraParsePage(page,strlen(page),&snapshot,&total,&start));
        SYSTEMTIME utc={.wYear=2026,.wMonth=11,.wDay=1,.wHour=9};FILETIME file;ULARGE_INTEGER value;
        assert(SystemTimeToFileTime(&utc,&file));value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;
        assert(snapshot.items[0].created_utc==(i<2?value.QuadPart:0));JiraFreeSnapshot(&snapshot);
    }
}
int main(void){
    due_fields();
    const char *page="{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"ABC-1\",\"fields\":{\"summary\":\"Test\",\"status\":{\"id\":\"91\",\"name\":\"Custom status\",\"statusCategory\":{\"key\":\"done\"}},\"comment\":{\"comments\":[{\"id\":\"10\",\"created\":\"2026-09-21T10:00:00.000+0300\",\"body\":\"older\"},{\"id\":\"11\",\"author\":{\"displayName\":\"Other User\"},\"created\":\"2026-09-22T10:00:00.000+0300\",\"body\":{\"type\":\"doc\",\"content\":[{\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"Latest\"},{\"type\":\"text\",\"text\":\" comment\"}]}]}}]}}}]}";
    JiraSnapshot snapshot={0};int total,start;
    assert(JiraParsePage(page,strlen(page),&snapshot,&total,&start));
    assert(total==1&&start==0&&snapshot.count==1);
    assert(!wcscmp(snapshot.items[0].status_name,L"Custom status"));
    assert(!wcscmp(snapshot.items[0].category,L"done"));
    assert(!wcscmp(snapshot.items[0].latest_comment,L"Latest comment"));
    assert(!wcscmp(snapshot.items[0].latest_comment_id,L"11"));
    assert(!wcscmp(snapshot.items[0].latest_comment_author,L"Other User"));
    snapshot.items[0].assigned_to_me=TRUE;
    JiraSnapshot reported={0};reported.items=calloc(2,sizeof(JiraItem));reported.count=2;
    wcscpy_s(reported.items[0].key,80,L"ABC-1");
    wcscpy_s(reported.items[1].key,80,L"ABC-2");
    reported.items[0].reported_by_me=reported.items[1].reported_by_me=TRUE;
    assert(JiraMergeReported(&snapshot,&reported)&&snapshot.count==2);
    assert(snapshot.items[0].assigned_to_me&&snapshot.items[0].reported_by_me);
    assert(!snapshot.items[1].assigned_to_me&&snapshot.items[1].reported_by_me);
    JiraFreeSnapshot(&reported);
    JiraFreeSnapshot(&snapshot);
    const char *bad="{\"issues\":[{}]}";
    assert(!JiraParsePage(bad,strlen(bad),&snapshot,&total,&start));
    const char *tr="{\"transitions\":[{\"id\":\"31\",\"name\":\"Done\",\"to\":{\"name\":\"Done\",\"statusCategory\":{\"key\":\"done\"}},\"fields\":{\"customfield_15812\":{\"required\":false,\"name\":\"Оценка\",\"fieldId\":\"customfield_15812\",\"allowedValues\":[{\"value\":\"☆☆☆☆☆\",\"id\":\"15710\"},{\"value\":\"⭐⭐⭐⭐⭐\",\"id\":\"15715\"}]}}}]}";
    JiraTransitions transitions;
    assert(JiraParseTransitions(tr,strlen(tr),&transitions));
    assert(transitions.count==1&&!wcscmp(transitions.items[0].id,L"31"));
    assert(transitions.items[0].field_count==1);
    assert(!wcscmp(transitions.items[0].rating_field_id,L"customfield_15812"));
    assert(transitions.items[0].rating_count==2);
    assert(!wcscmp(transitions.items[0].ratings[0].id,L"15710"));
    assert(!wcscmp(transitions.items[0].ratings[1].value,L"⭐⭐⭐⭐⭐"));
    assert(!JiraParseTransitions("{\"transitions\":null}",20,&transitions));
    const char *thread="{\"startAt\":0,\"total\":2,\"comments\":[{\"id\":\"10\",\"author\":{\"displayName\":\"Alex\"},\"created\":\"2026-10-01T10:04:00.000+0300\",\"body\":\"First\"},{\"id\":\"11\",\"author\":{\"displayName\":\"Maria\"},\"created\":\"2026-10-01T11:05:00.000+0300\",\"body\":{\"type\":\"doc\",\"content\":[{\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"Second\"}]}]}}]}";
    JiraComments comments={0};
    assert(JiraParseComments(thread,strlen(thread),&comments,&total,&start));
    assert(total==2&&start==0&&comments.count==2);
    assert(!wcscmp(comments.items[0].author,L"Alex")&&!wcscmp(comments.items[1].body,L"Second"));
    JiraFreeComments(&comments);
    const char *reverse_thread="{\"startAt\":0,\"total\":2,\"comments\":[{\"id\":\"11\",\"created\":\"2026-10-01T11:05:00.000+0300\",\"body\":\"New\"},{\"id\":\"10\",\"created\":\"2026-10-01T10:04:00.000+0300\",\"body\":\"Old\"}]}";
    assert(JiraParseComments(reverse_thread,strlen(reverse_thread),&comments,&total,&start));
    assert(comments.count==2&&!wcscmp(comments.items[0].body,L"Old")&&!wcscmp(comments.items[1].body,L"New"));
    JiraFreeComments(&comments);
    const char *bad_comments="{\"comments\":null}";
    assert(!JiraParseComments(bad_comments,strlen(bad_comments),&comments,&total,&start));
    /* A display-name collision must never paint a colleague's message as ours. */
    const char *authors="{\"startAt\":0,\"total\":4,\"comments\":["
        "{\"id\":\"1\",\"created\":\"2026-10-07T10:00:00\",\"body\":\"Mine\",\"author\":{\"displayName\":\"Alex\",\"key\":\"user-1\",\"name\":\"alex\"}},"
        "{\"id\":\"2\",\"created\":\"2026-10-07T10:01:00\",\"body\":\"Colleague\",\"author\":{\"displayName\":\"Alex\",\"key\":\"user-2\",\"name\":\"alex2\"}},"
        "{\"id\":\"3\",\"created\":\"2026-10-07T10:02:00\",\"body\":\"Unknown\",\"author\":{\"displayName\":\"Alex\"}},"
        "{\"id\":\"4\",\"created\":\"2026-10-07T10:03:00\",\"body\":\"Cloud\",\"author\":{\"displayName\":\"Renamed Alex\",\"accountId\":\"cloud-1\"}}]}";
    assert(JiraParseComments(authors,strlen(authors),&comments,&total,&start));
    JiraSnapshot self={0};wcscpy_s(self.account,256,L"Alex");
    wcscpy_s(self.identity.key,256,L"user-1");wcscpy_s(self.identity.name,256,L"alex");
    assert(JiraCommentIsOwn(&comments.items[0],&self));
    assert(!JiraCommentIsOwn(&comments.items[1],&self));
    assert(!JiraCommentIsOwn(&comments.items[2],&self));
    assert(!JiraCommentIsOwn(&comments.items[3],&self));
    wcscpy_s(self.identity.account_id,256,L"cloud-1");
    assert(JiraCommentIsOwn(&comments.items[3],&self));
    wcscpy_s(comments.items[3].identity.key,256,L"user-1");
    wcscpy_s(self.identity.account_id,256,L"cloud-2");
    assert(!JiraCommentIsOwn(&comments.items[3],&self)); /* Stable ID wins over a weaker match. */
    memset(&self.identity,0,sizeof(self.identity));wcscpy_s(self.identity.name,256,L"alex");
    assert(JiraCommentIsOwn(&comments.items[0],&self));
    assert(!JiraCommentIsOwn(&comments.items[1],&self));
    assert(!JiraCommentIsOwn(NULL,&self)&&!JiraCommentIsOwn(&comments.items[0],NULL));
    JiraFreeComments(&comments);
    puts("Jira model tests passed");
}
