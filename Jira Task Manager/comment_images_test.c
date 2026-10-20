#include "native_jira.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *feed="{\"startAt\":0,\"total\":1,\"comments\":[{\"id\":\"10\",\"created\":\"2026-10-13T10:00:00.000Z\",\"body\":\"Please check !gg.png|thumbnail! Thanks\",\"author\":{\"displayName\":\"Colleague\",\"key\":\"other\",\"name\":\"other person\",\"emailAddress\":\"other@example.test\",\"avatarUrls\":{\"48x48\":\"https://jira.example.test/jira/secure/useravatar?ownerId=other&id=1\"}}}]}";
static int requests;
static const wchar_t *fixture_content=L"https://jira.example.test/jira/secure/attachment/1001/gg.png";
static int fixture_size=70;static BOOL corrupt_image,failed_transfer;
static BOOL image_transport(JiraConnection *client,const wchar_t *method,const wchar_t *path,const char *body,char **out,size_t *size)
{
    (void)client;assert(!wcscmp(method,L"GET")&&!body);requests++;
    if(!wcscmp(path,L"/rest/api/2/issue/IMG-1?fields=attachment")){
        char metadata[2200],url[1600];assert(WideCharToMultiByte(CP_UTF8,0,fixture_content,-1,url,sizeof(url),NULL,NULL));
        snprintf(metadata,sizeof(metadata),"{\"fields\":{\"attachment\":[{\"id\":\"1001\",\"filename\":\"gg.png\",\"mimeType\":\"image/png\",\"size\":%d,\"content\":\"%s\"}]}}",fixture_size,url);
        *size=strlen(metadata);*out=malloc(*size+1);memcpy(*out,metadata,*size+1);return TRUE;
    }
    assert(!wcscmp(path,L"/secure/attachment/1001/gg.png")||!wcscmp(path,L"/secure/useravatar?ownerId=other&id=1"));
    if(failed_transfer){*size=NATIVE_IMAGE_MAX_BYTES;*out=calloc(1,*size);assert(*out);return FALSE;} /* Partial body before a transport failure. */
    /* Native 2x1 BMP fixture: red, green. WIC checks actual content, not filename/mime. */
    unsigned char bitmap[62]={0};BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};file.bfType=0x4d42;file.bfSize=62;file.bfOffBits=54;
    info.biSize=40;info.biWidth=2;info.biHeight=1;info.biPlanes=1;info.biBitCount=24;
    memcpy(bitmap,&file,sizeof(file));memcpy(bitmap+14,&info,sizeof(info));bitmap[56]=255;bitmap[58]=255;
    if(corrupt_image)memset(bitmap,0,sizeof(bitmap));
    *size=sizeof(bitmap);*out=malloc(*size);memcpy(*out,bitmap,*size);return TRUE;
}
static void check_wiki_body(const char *name,const char *escaped_body,const wchar_t *expected,int images)
{
    char json[5000];snprintf(json,sizeof(json),"{\"startAt\":0,\"total\":1,\"comments\":[{\"id\":\"10\",\"created\":\"2026-10-13\",\"body\":\"%s\"}]}",escaped_body);
    JiraComments comments={0};int total,start;assert(JiraParseComments(json,strlen(json),&comments,&total,&start));
    assert(comments.count==1&&total==1&&start==0);
    if(wcscmp(comments.items[0].body,expected)){
        fprintf(stderr,"Wiki body mismatch: %s; got %zu characters, expected %zu\n",name,wcslen(comments.items[0].body),wcslen(expected));
        assert(!wcscmp(comments.items[0].body,expected));
    }
    assert(comments.items[0].image_count==images);
    if(images)assert(!wcscmp(comments.items[0].images[0].reference,L"gg.png"));
    if(images==2)assert(!wcscmp(comments.items[0].images[1].reference,L"second.png"));
    JiraFreeComments(&comments);
}
static void check_wiki_spacing(void)
{
    /* Removing a final media block must not leave blank text rows above its thumbnail. */
    check_wiki_body("final thumbnail","Comment\\r\\n\\r\\n!gg.png|thumbnail!",L"Comment",1);
    check_wiki_body("final thumbnail and trailing newlines","Comment\\r\\n\\r\\n!gg.png|thumbnail!\\r\\n\\r\\n",L"Comment",1);
    check_wiki_body("final thumbnail on LF lines","Comment\\n\\n!gg.png|thumbnail!\\n",L"Comment",1);
    check_wiki_body("media only","\\r\\n\\r\\n!gg.png|thumbnail!\\r\\n\\t ",L"",1);
    check_wiki_body("multiple final images","Comment\\r\\n\\r\\n!gg.png|thumbnail!\\r\\n!second.png|thumbnail!\\r\\n",L"Comment",2);
    check_wiki_body("multiple media only","!gg.png!\\r\\n!second.png!\\r\\n",L"",2);
    /* Literal text spacing survives: only the boundary of a final media block is trimmed. */
    check_wiki_body("internal paragraphs before final image","First\\r\\n\\r\\nSecond\\r\\n\\r\\n!gg.png!",L"First\r\n\r\nSecond",1);
    check_wiki_body("ordinary blank paragraphs","First\\r\\n\\r\\nSecond\\r\\n\\r\\n",L"First\r\n\r\nSecond\r\n\r\n",0);
    check_wiki_body("mixed inline text","Please check !gg.png|thumbnail! Thanks",L"Please check  Thanks",1);
    check_wiki_body("adjacent words","Before!gg.png!After",L"Before After",1);
    check_wiki_body("paragraphs around image","First\\r\\n\\r\\n!gg.png!\\r\\n\\r\\nSecond\\r\\n",L"First\r\n\r\n\r\n\r\nSecond\r\n",1);
    check_wiki_body("text after image keeps trailing blanks","!gg.png!After\\r\\n\\r\\n",L"After\r\n\r\n",1);
    check_wiki_body("unterminated markup","Comment\\r\\n\\r\\n!gg.png|thumbnail",L"Comment\r\n\r\n!gg.png|thumbnail",0);
    check_wiki_body("nonimage markup","Comment\\r\\n\\r\\n!not-an-image!\\r\\n",L"Comment\r\n\r\n!not-an-image!\r\n",0);
    check_wiki_body("malformed markup after image","Comment!gg.png!\\r\\n!second.png",L"Comment\r\n!second.png",1);
    puts("Wiki spacing: final media boundaries compact; ordinary and mixed text whitespace preserved");
}
int main(void)
{
    check_wiki_spacing();
    JiraComments comments={0};int total,start;assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));
    assert(!wcsstr(comments.items[0].body,L"!gg.png")); /* Render a real image slot, not raw wiki markup. */
    assert(wcsstr(comments.items[0].body,L"Please check")&&wcsstr(comments.items[0].body,L"Thanks"));
    assert(comments.items[0].image_count==1&&!wcscmp(comments.items[0].images[0].reference,L"gg.png"));
    assert(wcsstr(comments.items[0].avatar_url,L"/secure/useravatar?ownerId=other"));
    JiraConnection client={0};wcscpy_s(client.host,256,L"jira.example.test");wcscpy_s(client.prefix,512,L"/jira");client.port=443;client.send=image_transport;
    JiraLoadCommentImages(&client,L"IMG-1",&comments);assert(requests==2&&comments.items[0].images[0].image);
    assert(comments.items[0].images[0].image->width==2&&comments.items[0].images[0].image->height==1);
    DIBSECTION dib={0};assert(GetObjectW(comments.items[0].images[0].image->thumbnail.handle,sizeof(dib),&dib)==sizeof(dib));
    BYTE *pixels=dib.dsBm.bmBits;assert(pixels[0]==0&&pixels[1]==0&&pixels[2]==255&&pixels[4]==0&&pixels[5]==255&&pixels[6]==0);
    NativeImageBitmap enlarged={0};assert(NativeImageDecode(comments.items[0].images[0].image->encoded,comments.items[0].images[0].image->length,1920,1440,&enlarged,NULL,NULL));
    NativeImageBitmapFree(&enlarged);
    JiraLoadCommentAuthorMedia(&client,&comments);assert(requests==3&&comments.items[0].avatar);
    assert(!wcscmp(comments.items[0].profile_url,L"https://jira.example.test/jira/secure/ViewProfile.jspa?name=other%20person"));
    JiraFreeComments(&comments);
    assert(NativeImageMemoryBytes()==0);
    const wchar_t *unsafe[]={L"https://evil.example/secure/attachment/1001/gg.png",L"https://jira.example.test.evil/jira/secure/attachment/1001/gg.png",
        L"http://jira.example.test/jira/secure/attachment/1001/gg.png",L"https://jira.example.test:444/jira/secure/attachment/1001/gg.png",
        L"https://attacker@jira.example.test/jira/secure/attachment/1001/gg.png",L"https://jira.example.test/other/secure/attachment/1001/gg.png",
        L"https://jira.example.test/jira/secure/attachment/10010/gg.png",L"https://jira.example.test/jira/rest/api/2/myself",
        L"https://jira.example.test/jira/secure/attachment/1001/../../logout",L"https://jira.example.test/jira/secure/attachment/1001/%2e%2e/%2e%2e/logout",
        L"https://jira.example.test/jira/secure/attachment/1001/%2e%2e%2flogout",L"https://jira.example.test/jira/secure/attachment/1001/%252e%252e/logout"};
    for(int i=0;i<12;i++){
        fixture_content=unsafe[i];int before=requests;assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));JiraLoadCommentImages(&client,L"IMG-1",&comments);
        assert(requests==before+1&&!comments.items[0].images[0].image&&comments.items[0].images[0].error==3);JiraFreeComments(&comments);
    }
    fixture_content=L"https://jira.example.test/jira/secure/attachment/1001/gg.png";fixture_size=NATIVE_IMAGE_MAX_BYTES+1;int before=requests;
    assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));JiraLoadCommentImages(&client,L"IMG-1",&comments);
    assert(requests==before+1&&comments.items[0].images[0].error==2);JiraFreeComments(&comments);
    fixture_size=70;corrupt_image=TRUE;assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));JiraLoadCommentImages(&client,L"IMG-1",&comments);
    assert(!comments.items[0].images[0].image&&comments.items[0].images[0].error==4);JiraFreeComments(&comments);corrupt_image=FALSE;
    corrupt_image=TRUE;before=requests;for(int i=0;i<3;i++)assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));
    JiraLoadCommentAuthorMedia(&client,&comments);assert(requests==before+1);JiraFreeComments(&comments);corrupt_image=FALSE;
    fixture_size=NATIVE_IMAGE_MAX_BYTES;failed_transfer=TRUE;before=requests;
    for(int i=0;i<3;i++)assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));
    JiraLoadCommentImages(&client,L"IMG-1",&comments);assert(requests==before+3); /* Metadata + at most two failed 6 MiB transfers. */
    JiraFreeComments(&comments);failed_transfer=FALSE;fixture_size=70;
    volatile LONG cancelled=1;client.cancelled=&cancelled;before=requests;assert(JiraParseComments(feed,strlen(feed),&comments,&total,&start));JiraLoadCommentImages(&client,L"IMG-1",&comments);
    assert(requests==before);JiraFreeComments(&comments);client.cancelled=NULL;
    const char *adf="{\"startAt\":0,\"total\":1,\"comments\":[{\"id\":\"11\",\"created\":\"2026-10-13\",\"body\":{\"type\":\"doc\",\"content\":[{\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"ADF text\"}]},{\"type\":\"mediaSingle\",\"content\":[{\"type\":\"media\",\"attrs\":{\"type\":\"file\",\"id\":\"1001\",\"alt\":\"gg.png\"}}]}]}}]}";
    assert(JiraParseComments(adf,strlen(adf),&comments,&total,&start));assert(comments.items[0].image_count==1&&!wcscmp(comments.items[0].body,L"ADF text"));
    JiraLoadCommentImages(&client,L"IMG-1",&comments);assert(comments.items[0].images[0].image);JiraFreeComments(&comments);
    assert(NativeImageMemoryBytes()==0);puts("Comment images/authors: wiki/ADF, WIC pixels, avatars/profile URL, origin/route/size/cancellation guards and cleanup passed");
}
