#include "native_jira.h"
#include "native_notifications.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const char *feed="{\"startAt\":0,\"total\":2,\"comments\":["
    "{\"id\":\"20\",\"created\":\"2026-10-12\",\"body\":\"Human\",\"author\":{\"displayName\":\"Robot\",\"emailAddress\":\"person@example.test\"}},"
    "{\"id\":\"21\",\"created\":\"2026-10-13\",\"body\":\"System\",\"author\":{\"displayName\":\"Robot\",\"emailAddress\":\"NO-MAIL@KAMA.GS\"}}]}";
static BOOL send_fixture(JiraConnection *client,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *size)
{
    (void)client;assert(!wcscmp(method,L"GET"));assert(!body);
    assert(!wcscmp(path,L"/rest/api/2/issue/EX-1/comment?startAt=0&maxResults=100"));
    *size=strlen(feed);*out=malloc(*size+1);assert(*out);memcpy(*out,feed,*size+1);return TRUE;
}
int main(void)
{
    JiraConnection client={0};client.send=send_fixture;JiraComments comments={0};
    assert(JiraReadComments(&client,L"EX-1",&comments));
    assert(comments.count==1&&!wcscmp(comments.items[0].id,L"20")); /* Email, never display-name guessing. */
    JiraFreeComments(&comments);
    client.excluded_emails=L"";assert(JiraReadComments(&client,L"EX-1",&comments));assert(comments.count==2);JiraFreeComments(&comments);
    client.excluded_emails=L" person@example.test , NO-MAIL@kama.gs ";assert(JiraReadComments(&client,L"EX-1",&comments));assert(comments.count==0);JiraFreeComments(&comments);
    assert(!NativeEmailExcluded(L"mail@kama.gs",L"no-mail@kama.gs"));assert(!NativeEmailExcluded(NULL,L""));
    wchar_t normalized[2048];assert(NativeNormalizeExcludedEmails(L" PERSON@EXAMPLE.TEST,\r\nno-mail@kama.gs ",normalized,_countof(normalized)));
    assert(!wcscmp(normalized,L"person@example.test, no-mail@kama.gs"));assert(!NativeNormalizeExcludedEmails(L"broken address@example.test",normalized,_countof(normalized)));
    const char *page="{\"startAt\":0,\"total\":1,\"issues\":[{\"key\":\"EX-1\",\"fields\":{\"summary\":\"Issue\",\"status\":{\"id\":\"1\",\"name\":\"Done\",\"statusCategory\":{\"key\":\"done\"}},\"comment\":{\"comments\":[{\"id\":\"20\",\"created\":\"2026-10-12\",\"body\":\"Human\",\"author\":{\"emailAddress\":\"person@example.test\"}},{\"id\":\"21\",\"created\":\"2026-10-13\",\"body\":\"System\",\"author\":{\"emailAddress\":\"no-mail@kama.gs\"}}]}}}]}";
    JiraSnapshot snapshot={0};int total,start;assert(JiraParsePage(page,strlen(page),&snapshot,&total,&start));
    assert(!wcscmp(snapshot.items[0].latest_comment_id,L"20"));JiraFreeSnapshot(&snapshot);
    puts("Excluded comment authors: default email match, case folding, visible latest comment and no name guessing passed");
}
