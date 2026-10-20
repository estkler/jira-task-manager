/* Attachment loading shares Jira's authenticated, no-redirect transport.
   Called only for an opened comment feed, never from task refresh. */
static int MediaHex(wchar_t value)
{return value>=L'0'&&value<=L'9'?value-L'0':value>=L'a'&&value<=L'f'?value-L'a'+10:value>=L'A'&&value<=L'F'?value-L'A'+10:-1;}
static BOOL SafeMediaPath(const wchar_t *path)
{
    int segment_length=0;BOOL dots=TRUE;
    for(const wchar_t *at=path;;at++){
        wchar_t ch=*at;
        if(!ch||ch==L'/'||ch==L'?'){
            if(dots&&(segment_length==1||segment_length==2))return FALSE;
            if(!ch||ch==L'?')return TRUE;segment_length=0;dots=TRUE;continue;
        }
        if(ch==L'%'){
            if(!at[1]||!at[2])return FALSE;int high=MediaHex(at[1]),low=MediaHex(at[2]);if(high<0||low<0)return FALSE;
            ch=(wchar_t)(high*16+low);at+=2;
            /* Encoded separators and double decoding are ambiguous to servers. */
            if(ch==L'/'||ch==L'\\'||ch==L'%'||ch==L'?'||ch==L'#')return FALSE;
        }
        if(ch<L' '||ch==127||ch==L'\\')return FALSE;segment_length++;if(ch!=L'.')dots=FALSE;
    }
}
static BOOL SameOriginImagePath(const JiraConnection *client,const wchar_t *url,wchar_t *relative,size_t capacity)
{
    if(!url||!*url||wcslen(url)>1536)return FALSE;
    for(const wchar_t *at=url;*at;at++)if(*at<L' '||*at==L'\\'||*at==L'#')return FALSE;
    const wchar_t *path=url;
    if(*url!=L'/'){
        URL_COMPONENTSW parts={sizeof(parts)};parts.dwHostNameLength=parts.dwUserNameLength=parts.dwPasswordLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=(DWORD)-1;
        if(!WinHttpCrackUrl(url,0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=client->port||
            parts.dwUserNameLength||parts.dwPasswordLength||parts.dwHostNameLength!=wcslen(client->host)||
            _wcsnicmp(parts.lpszHostName,client->host,parts.dwHostNameLength))return FALSE;
        path=parts.lpszUrlPath;
    }
    if(!path||path[0]!=L'/'||path[1]==L'/'||!SafeMediaPath(path))return FALSE;
    size_t prefix=wcslen(client->prefix);
    if(prefix&&(_wcsnicmp(path,client->prefix,prefix)||path[prefix]!=L'/'))return FALSE;
    path+=prefix;if(wcslen(path)>=capacity)return FALSE;
    return wcscpy_s(relative,capacity,path)==0;
}
static BOOL AttachmentImagePath(const JiraConnection *client,const wchar_t *content,const wchar_t *id,wchar_t *relative,size_t capacity)
{
    if(!safe_segment(id)||!SameOriginImagePath(client,content,relative,capacity))return FALSE;
    const wchar_t *routes[]={L"/secure/attachment/",L"/rest/api/2/attachment/content/",L"/attachments/"};
    for(int i=0;i<3;i++){
        size_t prefix=wcslen(routes[i]),length=wcslen(id);
        if(!wcsncmp(relative,routes[i],prefix)&&!wcsncmp(relative+prefix,id,length)){
            wchar_t delimiter=relative[prefix+length];return !delimiter||delimiter==L'/'||delimiter==L'?';
        }
    }
    return FALSE;
}
static BOOL SupportedImageMime(const wchar_t *mime)
{
    const wchar_t *formats[]={L"image/png",L"image/jpeg",L"image/gif",L"image/bmp",L"image/x-ms-bmp",L"image/tiff",L"image/webp"};
    for(int i=0;i<7;i++)if(!_wcsicmp(mime,formats[i]))return TRUE;return FALSE;
}
void JiraLoadCommentImages(JiraConnection *client,const wchar_t *key,JiraComments *comments)
{
    if(!client||!comments||!safe_segment(key))return;
    BOOL referenced=FALSE;for(int i=0;i<comments->count;i++)if(comments->items[i].image_count)referenced=TRUE;
    if(!referenced)return;
    wchar_t previous_error[256];wcscpy_s(previous_error,_countof(previous_error),client->error);
    wchar_t path[320];swprintf(path,_countof(path),L"/rest/api/2/issue/%ls?fields=attachment",key);
    char *json=NULL;size_t length=0;NJDoc doc={0};
    BOOL ok=request_bounded(client,L"GET",path,NULL,&json,&length,4u*1024u*1024u,FALSE)&&nj_parse(&doc,json,length);
    int fields=ok?nj_get(&doc,0,"fields"):-1,array=ok?nj_get(&doc,fields,"attachment"):-1;
    ok=ok&&array>=0&&doc.tokens[array].kind==NJ_ARRAY&&doc.tokens[array].count<=1000;
    size_t remaining=NATIVE_IMAGE_FEED_BYTES;size_t *previous_budget=client->image_bytes_remaining;client->image_bytes_remaining=&remaining;int loaded=0;
    for(int i=0;i<comments->count;i++)for(int j=0;j<comments->items[i].image_count;j++){
        JiraCommentImage *image=&comments->items[i].images[j];image->error=1;
        if(!ok||(client->cancelled&&InterlockedCompareExchange((volatile LONG*)client->cancelled,0,0)))continue;
        int selected=-1,matches=0,at=array+1;
        for(int entry=0;entry<doc.tokens[array].count;entry++){
            wchar_t filename[256]=L"",id[80]=L"",content[1536]=L"";
            field(&doc,at,"filename",filename,_countof(filename));field(&doc,at,"id",id,_countof(id));field(&doc,at,"content",content,_countof(content));
            BOOL match=image->kind==1?!_wcsicmp(filename,image->reference):image->kind==3?!wcscmp(id,image->reference):!wcscmp(content,image->reference);
            if(image->kind==2&&image->reference[0]==L'/'){
                wchar_t relative[1024],reference[1024];
                match=SameOriginImagePath(client,content,relative,_countof(relative))&&SameOriginImagePath(client,image->reference,reference,_countof(reference))&&!wcscmp(relative,reference);
            }
            /* ADF media UUIDs are not Jira attachment IDs. Use alt text only if it unambiguously names a file. */
            if(!match&&image->kind==3&&wcscmp(image->filename,image->reference))match=!_wcsicmp(filename,image->filename);
            if(match){selected=at;matches++;}at=doc.tokens[at].next;
        }
        if(matches!=1)continue;
        field(&doc,selected,"id",image->attachment_id,_countof(image->attachment_id));field(&doc,selected,"filename",image->filename,_countof(image->filename));
        BOOL cached=FALSE;
        for(int row=0;row<=i&&!cached;row++)for(int reference=0;reference<comments->items[row].image_count;reference++){
            JiraCommentImage *earlier=&comments->items[row].images[reference];if(earlier==image)break;
            if(earlier->image&&!wcscmp(earlier->attachment_id,image->attachment_id)){image->image=earlier->image;NativeImageRetain(image->image);image->error=0;cached=TRUE;break;}
        }
        if(cached)continue;
        if(loaded>=JIRA_FEED_MAX_IMAGES||!remaining){image->error=5;continue;}
        wchar_t mime[96]=L"",content[1536]=L"",relative[1024];
        field(&doc,selected,"mimeType",mime,_countof(mime));field(&doc,selected,"content",content,_countof(content));
        if(!SupportedImageMime(mime)){image->error=4;continue;}
        int size=nj_int(&doc,nj_get(&doc,selected,"size"),-1);
        if(size<0||(size_t)size>NATIVE_IMAGE_MAX_BYTES||(size_t)size>remaining){image->error=2;continue;}
        if(!AttachmentImagePath(client,content,image->attachment_id,relative,_countof(relative))){image->error=3;continue;}
        char *data=NULL;size_t bytes=0;loaded++;
        size_t limit=min(NATIVE_IMAGE_MAX_BYTES,remaining);
        if(!request_bounded(client,L"GET",relative,NULL,&data,&bytes,limit,TRUE))continue;
        image->image=NativeImageCreate((unsigned char*)data,bytes);image->error=image->image?0:4;
    }
    client->image_bytes_remaining=previous_budget;nj_free(&doc);free(json);wcscpy_s(client->error,_countof(client->error),previous_error);
}
static void CommentProfileUrl(const JiraConnection *client,JiraComment *comment)
{
    comment->profile_url[0]=0;
    const wchar_t *identity=comment->identity.name[0]?comment->identity.name:comment->identity.account_id;
    if(!*identity||!client->host[0])return;
    char utf8[1536];int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,identity,-1,utf8,sizeof(utf8),NULL,NULL);if(count<=0)return;
    wchar_t encoded[768];size_t used=0;const wchar_t hex[]=L"0123456789ABCDEF";
    for(int i=0;i<count-1;i++){
        unsigned char ch=(unsigned char)utf8[i];BOOL plain=(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.'||ch=='~';
        if(used+(plain?1u:3u)>=_countof(encoded))return;
        if(plain)encoded[used++]=(wchar_t)ch;else{encoded[used++]=L'%';encoded[used++]=hex[ch>>4];encoded[used++]=hex[ch&15];}
    }
    encoded[used]=0;wchar_t port[16]=L"";if(client->port!=443)swprintf(port,_countof(port),L":%u",client->port);
    const wchar_t *route=comment->identity.name[0]?L"/secure/ViewProfile.jspa?name=":L"/jira/people/";
    BOOL ipv6=wcschr(client->host,L':')!=NULL;
    if(swprintf(comment->profile_url,_countof(comment->profile_url),L"https://%ls%ls%ls%ls%ls%ls%ls",ipv6?L"[":L"",client->host,ipv6?L"]":L"",port,client->prefix,route,encoded)<0)comment->profile_url[0]=0;
}
BOOL JiraConfigureAvatarCache(JiraConnection *client,const wchar_t *settings_path,const wchar_t *account_identity)
{return client&&NativeAvatarCacheConfigure(&client->avatar_cache,settings_path,client->host,client->port,client->prefix,account_identity);}
static SRWLOCK avatar_media_lock=SRWLOCK_INIT;
void JiraLoadCommentAuthorMedia(JiraConnection *client,JiraComments *comments)
{
    if(!client||!comments)return;int downloaded=0;wchar_t saved_error[256];wcscpy_s(saved_error,_countof(saved_error),client->error);
    /* Serialize the read/refresh/write sequence so concurrent feeds share successful downloads. */
    AcquireSRWLockExclusive(&avatar_media_lock);
    for(int i=0;i<comments->count;i++){
        JiraComment *comment=&comments->items[i];CommentProfileUrl(client,comment);
        if(!comment->avatar_url[0])continue;
        BOOL cached=FALSE;
        for(int earlier=0;earlier<i;earlier++)if(!wcscmp(comments->items[earlier].avatar_url,comment->avatar_url)){
            comment->avatar=comments->items[earlier].avatar;NativeImageRetain(comment->avatar);cached=TRUE;break;
        }
        if(cached)continue;
        wchar_t relative[1024];if(!SameOriginImagePath(client,comment->avatar_url,relative,_countof(relative)))continue;
        const wchar_t *routes[]={L"/secure/useravatar",L"/rest/api/2/user/avatar/",L"/rest/api/2/universal_avatar/view/",L"/images/avatars/",L"/images/icons/profilepics/"};BOOL allowed=FALSE;
        for(int route=0;route<5;route++){size_t count=wcslen(routes[route]);if(!wcsncmp(relative,routes[route],count)&&(relative[count]==L'?'||relative[count]==0||routes[route][count-1]==L'/'))allowed=TRUE;}
        if(!allowed)continue;
        BOOL fresh=FALSE;comment->avatar=NativeAvatarCacheRead(&client->avatar_cache,relative,&fresh);
        if(fresh||downloaded>=24||(client->cancelled&&InterlockedCompareExchange((volatile LONG*)client->cancelled,0,0)))continue;
        downloaded++;char *bytes=NULL;size_t length=0;
        if(request_bounded(client,L"GET",relative,NULL,&bytes,&length,NATIVE_AVATAR_MAX_BYTES,TRUE)){
            NativeImage *replacement=NativeImageCreateSized((unsigned char*)bytes,length,48,48);
            if(replacement){NativeAvatarCacheWrite(&client->avatar_cache,relative,replacement);NativeImageRelease(comment->avatar);comment->avatar=replacement;}
        }
    }
    ReleaseSRWLockExclusive(&avatar_media_lock);
    wcscpy_s(client->error,_countof(client->error),saved_error);
}
