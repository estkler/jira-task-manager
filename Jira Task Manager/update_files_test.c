#include "native_update.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include <aclapi.h>
#include <wincrypt.h>
#include <winioctl.h>
static int hex(wchar_t c){return c>=L'0'&&c<=L'9'?c-L'0':c>=L'A'&&c<=L'F'?c-L'A'+10:c-L'a'+10;}
static void test_verified_fixture(const wchar_t *path,const wchar_t *hash){
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(f!=INVALID_HANDLE_VALUE);
    UpdateRelease r={.version={0,9,30}};LARGE_INTEGER size;assert(GetFileSizeEx(f,&size));r.size=(DWORD)size.QuadPart;for(int i=0;i<32;i++)r.sha256[i]=(BYTE)(hex(hash[2*i])*16+hex(hash[2*i+1]));
    assert(UpdateVerifyFile(f,&r)==UPDATE_OK);r.sha256[0]^=1;assert(UpdateVerifyFile(f,&r)==UPDATE_INVALID);r.sha256[0]^=1;r.size++;assert(UpdateVerifyFile(f,&r)==UPDATE_INVALID);r.size--;r.version.patch=31;assert(UpdateVerifyFile(f,&r)==UPDATE_INVALID);
    HANDLE writer=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(writer==INVALID_HANDLE_VALUE);assert(GetLastError()==ERROR_SHARING_VIOLATION);CloseHandle(f);
}
static void test_modified_fixture(void){
    UpdateStage s;assert(UpdateStageCreate(&s)==UPDATE_OK);
    HANDLE f=CreateFileW(s.executable,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(f!=INVALID_HANDLE_VALUE);DWORD n;assert(WriteFile(f,"hello",5,&n,NULL)&&n==5);
    UpdateRelease r={.size=5,.version={0,9,30}};const char *hash="2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    for(int i=0;i<32;i++)r.sha256[i]=(BYTE)(hex(hash[2*i])*16+hex(hash[2*i+1]));assert(UpdateVerifyFile(f,&r)==UPDATE_INVALID);CloseHandle(f);UpdateStageCleanup(&s);
}
static void test_pe_rejected(const wchar_t *path){
    HANDLE original=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(original!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(original,NULL),got;BYTE *bytes=malloc(size);assert(bytes&&ReadFile(original,bytes,size,&got,NULL)&&got==size);CloseHandle(original);
    DWORD nt=((IMAGE_DOS_HEADER*)bytes)->e_lfanew;DWORD offsets[]={0,nt+4,nt+4+sizeof(IMAGE_FILE_HEADER),nt+4+sizeof(IMAGE_FILE_HEADER)+offsetof(IMAGE_OPTIONAL_HEADER64,DataDirectory)+IMAGE_DIRECTORY_ENTRY_RESOURCE*sizeof(IMAGE_DATA_DIRECTORY)};
    for(int i=0;i<_countof(offsets)+1;i++){
        UpdateStage s;assert(UpdateStageCreate(&s)==UPDATE_OK);HANDLE f=CreateFileW(s.executable,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(f!=INVALID_HANDLE_VALUE);
        BYTE saved=0;if(i<_countof(offsets)){saved=bytes[offsets[i]];bytes[offsets[i]]^=0xff;}DWORD length=i==_countof(offsets)?16:size;assert(WriteFile(f,bytes,length,&got,NULL)&&got==length);
        UpdateRelease r={.version={0,9,30},.size=length};HCRYPTPROV provider;HCRYPTHASH hash;assert(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT));assert(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash));assert(CryptHashData(hash,bytes,length,0));DWORD count=32;assert(CryptGetHashParam(hash,HP_HASHVAL,r.sha256,&count,0));CryptDestroyHash(hash);CryptReleaseContext(provider,0);
        assert(UpdateVerifyFile(f,&r)==UPDATE_INVALID);CloseHandle(f);UpdateStageCleanup(&s);if(i<_countof(offsets))bytes[offsets[i]]=saved;
    }free(bytes);
}
static void test_reparse_isolation(void){
    UpdateStage a,b;assert(UpdateStageCreate(&a)==UPDATE_OK&&UpdateStageCreate(&b)==UPDATE_OK);wchar_t original[MAX_PATH];wcscpy_s(original,MAX_PATH,a.directory);
    HANDLE sentinel=CreateFileW(b.executable,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(sentinel!=INVALID_HANDLE_VALUE);CloseHandle(sentinel);
    struct {DWORD tag;WORD length,reserved,sub_offset,sub_length,print_offset,print_length;wchar_t path[2*MAX_PATH+8];} mount={0};
    mount.tag=IO_REPARSE_TAG_MOUNT_POINT;swprintf(mount.path,_countof(mount.path),L"\\??\\%ls",b.directory);mount.sub_length=(WORD)(wcslen(mount.path)*2);mount.print_offset=mount.sub_length+2;wcscpy_s((wchar_t*)((BYTE*)mount.path+mount.print_offset),MAX_PATH,b.directory);mount.print_length=(WORD)(wcslen(b.directory)*2);mount.length=8+mount.print_offset+mount.print_length+2;
    HANDLE junction=CreateFileW(a.directory,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);assert(junction!=INVALID_HANDLE_VALUE);DWORD got;assert(DeviceIoControl(junction,FSCTL_SET_REPARSE_POINT,&mount,8+mount.length,NULL,0,&got,NULL));CloseHandle(junction);
    assert(!UpdatePathSafe(a.directory,TRUE));UpdateStageCleanup(&a);assert(GetFileAttributesW(b.executable)!=INVALID_FILE_ATTRIBUTES);assert(RemoveDirectoryW(original));UpdateStageCleanup(&b);
}
static void test_staging_isolation(void){
    UpdateStage s;assert(UpdateStageCreate(&s)==UPDATE_OK);assert(GetFileAttributesW(s.directory)!=INVALID_FILE_ATTRIBUTES);
    PSECURITY_DESCRIPTOR sd=NULL;PACL acl=NULL;assert(GetNamedSecurityInfoW(s.directory,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&acl,NULL,&sd)==ERROR_SUCCESS);SECURITY_DESCRIPTOR_CONTROL control;DWORD revision;assert(GetSecurityDescriptorControl(sd,&control,&revision));assert(control&SE_DACL_PROTECTED);assert(acl&&acl->AceCount==2);LocalFree(sd);
    wchar_t sentinel[MAX_PATH],dir[MAX_PATH];wcscpy_s(dir,MAX_PATH,s.directory);swprintf(sentinel,MAX_PATH,L"%ls\\sentinel.txt",s.directory);HANDLE f=CreateFileW(sentinel,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(f!=INVALID_HANDLE_VALUE);CloseHandle(f);
    HANDLE rename=CreateFileW(s.directory,DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);assert(rename==INVALID_HANDLE_VALUE);
    UpdateStageCleanup(&s);assert(GetFileAttributesW(sentinel)!=INVALID_FILE_ATTRIBUTES);assert(DeleteFileW(sentinel));assert(RemoveDirectoryW(dir));
}
int wmain(int argc,wchar_t **argv){assert(argc==3&&wcslen(argv[2])==64);test_verified_fixture(argv[1],argv[2]);test_modified_fixture();test_pe_rejected(argv[1]);test_staging_isolation();test_reparse_isolation();puts("update_files_test: PASS");return 0;}
