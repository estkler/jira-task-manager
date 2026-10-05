#include "native_update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char asset[]="{\"name\":\"Jira.Task.Manager.exe\",\"id\":4294967296,\"size\":1024,\"digest\":\"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}";
static void test_version_order(void){
    UpdateVersion a,b;
    assert(UpdateParseVersion(L"0.9.99",&a));assert(UpdateParseVersion(L"v0.10.0",&b));
    assert(UpdateCompareVersion(b,a)==1);assert(UpdateCompareVersion(a,b)==-1);
    assert(UpdateParseVersion(L"0.10.0",&a));assert(UpdateCompareVersion(a,b)==0);
    const wchar_t *bad[]={L"",L"0.9",L"0.9.30-beta",L"-1.9.30",L"0.9.30 ",L"V0.9.30",L"0.9.4294967296",L"0..1",L"1.2.3.4"};
    for(int i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!UpdateParseVersion(bad[i],&a));
}
static UpdateStatus parse(const char *tag,const char *flags,const char *assets,UpdateRelease *out){
    char json[4096];snprintf(json,sizeof(json),"{\"tag_name\":\"%s\",%s,\"assets\":[%s],\"ignored\":{\"body\":\"hello\"}}",tag,flags,assets);
    return UpdateParseRelease(json,strlen(json),(UpdateVersion){0,9,29},out);
}
static void test_release_valid(void){
    UpdateRelease r;
    assert(parse("v0.9.30","\"draft\":false,\"prerelease\":false",asset,&r)==UPDATE_OK);
    assert(r.version.patch==30&&r.asset_id==4294967296ull&&r.size==1024&&r.sha256[0]==1&&r.sha256[31]==239);
    assert(parse("0.9.29","\"draft\":false,\"prerelease\":false",asset,&r)==UPDATE_NONE);
    assert(parse("0.9.28","\"draft\":false,\"prerelease\":false",asset,&r)==UPDATE_NONE);
    assert(parse("0.9.30","\"draft\":false,\"prerelease\":false","",&r)==UPDATE_NONE);
}
static void test_release_rejected(void){
    UpdateRelease r;char duplicate[2048];snprintf(duplicate,sizeof(duplicate),"%s,%s",asset,asset);
    assert(parse("0.9.30","\"draft\":false,\"prerelease\":false",duplicate,&r)==UPDATE_INVALID);
    const char *flags[]={"\"draft\":true,\"prerelease\":false","\"draft\":false,\"prerelease\":true","\"draft\":false,\"draft\":false,\"prerelease\":false","\"draft\":0,\"prerelease\":false","\"draft\":false","\"draft\":false,\"prerelease\":\"false\""};
    for(int i=0;i<sizeof(flags)/sizeof(*flags);i++)assert(parse("0.9.30",flags[i],asset,&r)==UPDATE_INVALID);
    const char *bad[]={
        "{\"name\":\"Jira.Task.Manager.exe\",\"id\":1,\"size\":1}",
        "{\"name\":\"Jira.Task.Manager.exe\",\"id\":0,\"size\":1,\"digest\":null}",
        "{\"name\":\"Jira.Task.Manager.exe\",\"id\":18446744073709551616,\"size\":1,\"digest\":\"sha256:00\"}",
        "{\"name\":\"Jira.Task.Manager.exe\\u0000evil\",\"id\":1,\"size\":1}",
        "{\"name\":\"Jira.Task.Manager.exe\",\"name\":\"other\",\"id\":1,\"size\":1}",
        "{\"name\":\"Jira.Task.Manager.exe\",\"id\":1,\"id\":2,\"size\":1}","null"};
    for(int i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(parse("0.9.30","\"draft\":false,\"prerelease\":false",bad[i],&r)==UPDATE_INVALID);
    const char *changes[][2]={{"1024","0"},{"1024","16777217"},{"1024","\"1024\""},{"4294967296","-1"},{"4294967296","1.5"},{"4294967296","18446744073709551616"},{"sha256:0123","sha256:g123"},{"sha256:0123","md5:0123"}};
    for(int i=0;i<sizeof(changes)/sizeof(*changes);i++){
        char mutated[2048];const char *at=strstr(asset,changes[i][0]);assert(at);
        snprintf(mutated,sizeof(mutated),"%.*s%s%s",(int)(at-asset),asset,changes[i][1],at+strlen(changes[i][0]));
        assert(parse("0.9.29","\"draft\":false,\"prerelease\":false",mutated,&r)==UPDATE_INVALID);
    }
    assert(parse("0.9.30","\"draft\":false,\"prerelease\":false,\"tag_name\":\"0.9.31\"",asset,&r)==UPDATE_INVALID);
    assert(UpdateParseRelease("{}",1048577,(UpdateVersion){0,9,29},&r)==UPDATE_INVALID);
    assert(UpdateParseRelease("{",1,(UpdateVersion){0,9,29},&r)==UPDATE_INVALID);
}
static void test_url_policy(void){
    const wchar_t *good[]={L"https://api.github.com/repos/x",L"https://GITHUB.COM:443/file",L"https://release-assets.githubusercontent.com/x?sig=a",L"https://objects.githubusercontent.com/x"};
    for(int i=0;i<sizeof(good)/sizeof(*good);i++)assert(UpdateUrlAllowed(good[i]));
    const wchar_t *bad[]={L"http://github.com/a",L"https://github.com.evil/a",L"https://user@github.com/a",L"https://github.com:444/a",L"https://github.com./a",L"https://evil/a",L"https://github.com/a#b",L"https://github.com/\r\nx",L"https://github.com\\@evil/a"};
    for(int i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!UpdateUrlAllowed(bad[i]));
}
int main(void){test_version_order();test_release_valid();test_release_rejected();test_url_policy();puts("update_model_test: PASS");return 0;}
