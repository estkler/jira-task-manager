#include "native_update.h"
#include <bcrypt.h>
#include <sddl.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

static BOOL canonical_handle(HANDLE file,wchar_t *path){
    wchar_t raw[MAX_PATH+8];DWORD n=GetFinalPathNameByHandleW(file,raw,_countof(raw),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!n||n>=_countof(raw)||wcsncmp(raw,L"\\\\?\\",4)||!raw[4]||raw[5]!=L':'||wcslen(raw+4)>=MAX_PATH)return FALSE;
    wcscpy_s(path,MAX_PATH,raw+4);return TRUE;
}
BOOL UpdatePathSafe(const wchar_t *path,BOOL directory){
    if(!path||wcslen(path)>=MAX_PATH||wcslen(path)<3||path[1]!=L':'||path[2]!=L'\\')return FALSE;
    HANDLE file=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:0),NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;BY_HANDLE_FILE_INFORMATION info;wchar_t actual[MAX_PATH];
    BOOL ok=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
        !!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==!!directory&&canonical_handle(file,actual)&&!_wcsicmp(actual,path);
    CloseHandle(file);return ok;
}
UpdateStatus UpdateStageCreate(UpdateStage *out){
    if(!out)return UPDATE_INVALID;memset(out,0,sizeof(*out));out->directory_lock=INVALID_HANDLE_VALUE;
    wchar_t temp[MAX_PATH],resolved[MAX_PATH];DWORD n=GetTempPathW(MAX_PATH,temp);if(!n||n>=MAX_PATH)return UPDATE_IO;
    HANDLE parent=CreateFileW(temp,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(parent==INVALID_HANDLE_VALUE)return UPDATE_IO;BOOL valid=canonical_handle(parent,resolved);CloseHandle(parent);
    if(!valid||!UpdatePathSafe(resolved,TRUE))return UPDATE_IO;size_t end=wcslen(resolved);if(end>3&&resolved[end-1]==L'\\')resolved[end-1]=0;
    HANDLE token=NULL;DWORD needed=0;TOKEN_USER *user=NULL;LPWSTR sid=NULL;PSECURITY_DESCRIPTOR sd=NULL;UpdateStatus result=UPDATE_IO;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))goto done;
    GetTokenInformation(token,TokenUser,NULL,0,&needed);if(!needed)goto done;user=malloc(needed);if(!user)goto done;
    if(!GetTokenInformation(token,TokenUser,user,needed,&needed)||!ConvertSidToStringSidW(user->User.Sid,&sid))goto done;
    wchar_t sddl[256];if(swprintf(sddl,_countof(sddl),L"D:P(A;;FA;;;SY)(A;;FA;;;%ls)",sid)<0||!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&sd,NULL))goto done;
    SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};
    for(int attempt=0;attempt<8;attempt++){
        BYTE random[16];wchar_t nonce[33];if(BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)goto done;
        for(int i=0;i<16;i++)swprintf(nonce+2*i,3,L"%02x",random[i]);
        if(swprintf(out->directory,MAX_PATH,L"%ls\\JiraTaskUpdate-%ls",resolved,nonce)<0)goto done;
        if(!CreateDirectoryW(out->directory,&sa)){if(GetLastError()==ERROR_ALREADY_EXISTS)continue;goto done;}
        out->directory_lock=CreateFileW(out->directory,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(out->directory_lock==INVALID_HANDLE_VALUE||!UpdatePathSafe(out->directory,TRUE))goto done;
        if(swprintf(out->executable,MAX_PATH,L"%ls\\Jira.Task.Manager.exe",out->directory)<0)goto done;
        result=UPDATE_OK;break;
    }
done:if(token)CloseHandle(token);if(sid)LocalFree(sid);if(sd)LocalFree(sd);free(user);if(result!=UPDATE_OK)UpdateStageCleanup(out);return result;
}
void UpdateStageCleanup(UpdateStage *stage){
    if(!stage)return;
    if(stage->directory[0]&&UpdatePathSafe(stage->directory,TRUE)){
        wchar_t expected[MAX_PATH];if(swprintf(expected,MAX_PATH,L"%ls\\Jira.Task.Manager.exe",stage->directory)>=0&&!wcscmp(expected,stage->executable))DeleteFileW(expected);
        if(stage->directory_lock&&stage->directory_lock!=INVALID_HANDLE_VALUE)CloseHandle(stage->directory_lock);stage->directory_lock=INVALID_HANDLE_VALUE;
        RemoveDirectoryW(stage->directory);
    }else if(stage->directory_lock&&stage->directory_lock!=INVALID_HANDLE_VALUE)CloseHandle(stage->directory_lock);
    memset(stage,0,sizeof(*stage));stage->directory_lock=INVALID_HANDLE_VALUE;
}
typedef struct PeImage {const BYTE *bytes;DWORD size;DWORD sections_at;WORD sections;} PeImage;
static BOOL range(DWORD at,DWORD length,DWORD size){return at<=size&&length<=size-at;}
static const BYTE *rva(const PeImage *p,DWORD address,DWORD size){
    for(int i=0;i<p->sections;i++){
        IMAGE_SECTION_HEADER s;memcpy(&s,p->bytes+p->sections_at+i*sizeof(s),sizeof(s));
        if(address>=s.VirtualAddress){DWORD delta=address-s.VirtualAddress;if(range(delta,size,s.SizeOfRawData)&&s.PointerToRawData<=p->size&&range(delta,size,p->size-s.PointerToRawData))return p->bytes+s.PointerToRawData+delta;}
    }return NULL;
}
static BOOL resource_child(const BYTE *root,DWORD size,DWORD directory,int id,DWORD *offset,BOOL *is_directory){
    if(!range(directory,sizeof(IMAGE_RESOURCE_DIRECTORY),size))return FALSE;IMAGE_RESOURCE_DIRECTORY d;memcpy(&d,root+directory,sizeof(d));
    DWORD count=(DWORD)d.NumberOfNamedEntries+d.NumberOfIdEntries,at=directory+sizeof(d);
    if(count>size/sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY)||!range(at,count*sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY),size))return FALSE;
    BOOL found=FALSE;for(DWORD i=0;i<count;i++){
        IMAGE_RESOURCE_DIRECTORY_ENTRY e;memcpy(&e,root+at+i*sizeof(e),sizeof(e));
        if(id<0||(e.NameIsString==0&&e.Id==id)){if(found)return FALSE;found=TRUE;*offset=e.OffsetToDirectory;*is_directory=e.DataIsDirectory;}
    }return found;
}
static BOOL pe_version(const BYTE *bytes,DWORD size,UpdateVersion *version){
    if(size<sizeof(IMAGE_DOS_HEADER))return FALSE;IMAGE_DOS_HEADER dos;memcpy(&dos,bytes,sizeof(dos));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||!range((DWORD)dos.e_lfanew,4+sizeof(IMAGE_FILE_HEADER),size))return FALSE;
    DWORD signature;memcpy(&signature,bytes+dos.e_lfanew,4);if(signature!=IMAGE_NT_SIGNATURE)return FALSE;
    IMAGE_FILE_HEADER file;memcpy(&file,bytes+dos.e_lfanew+4,sizeof(file));
    if(file.Machine!=IMAGE_FILE_MACHINE_AMD64||!file.NumberOfSections||file.NumberOfSections>96||file.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER64)||!(file.Characteristics&IMAGE_FILE_EXECUTABLE_IMAGE)||file.Characteristics&IMAGE_FILE_DLL)return FALSE;
    DWORD optional_at=(DWORD)dos.e_lfanew+4+sizeof(file);if(!range(optional_at,file.SizeOfOptionalHeader,size))return FALSE;
    IMAGE_OPTIONAL_HEADER64 optional;memcpy(&optional,bytes+optional_at,sizeof(optional));
    if(optional.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||optional.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_RESOURCE)return FALSE;
    DWORD sections_at=optional_at+file.SizeOfOptionalHeader;if(!range(sections_at,file.NumberOfSections*sizeof(IMAGE_SECTION_HEADER),size))return FALSE;
    PeImage image={bytes,size,sections_at,file.NumberOfSections};IMAGE_DATA_DIRECTORY dir=optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    if(!dir.VirtualAddress||!dir.Size||dir.Size>size)return FALSE;const BYTE *root=rva(&image,dir.VirtualAddress,dir.Size);if(!root)return FALSE;
    DWORD offset;BOOL is_dir;
    if(!resource_child(root,dir.Size,0,16,&offset,&is_dir)||!is_dir||
       !resource_child(root,dir.Size,offset,-1,&offset,&is_dir)||!is_dir||
       !resource_child(root,dir.Size,offset,-1,&offset,&is_dir)||is_dir||!range(offset,sizeof(IMAGE_RESOURCE_DATA_ENTRY),dir.Size))return FALSE;
    IMAGE_RESOURCE_DATA_ENTRY data;memcpy(&data,root+offset,sizeof(data));if(data.Size<6||data.Size>size)return FALSE;
    const BYTE *value=rva(&image,data.OffsetToData,data.Size);if(!value)return FALSE;
    WORD length,value_length,type;memcpy(&length,value,2);memcpy(&value_length,value+2,2);memcpy(&type,value+4,2);
    const wchar_t key[]=L"VS_VERSION_INFO";DWORD fixed_at=(6+sizeof(key)+3)&~3u;
    if(length>data.Size||value_length!=sizeof(VS_FIXEDFILEINFO)||type||!range(6,sizeof(key),length)||memcmp(value+6,key,sizeof(key))||!range(fixed_at,sizeof(VS_FIXEDFILEINFO),length))return FALSE;
    VS_FIXEDFILEINFO info;memcpy(&info,value+fixed_at,sizeof(info));
    if(info.dwSignature!=0xfeef04bd||info.dwFileType!=VFT_APP||LOWORD(info.dwFileVersionLS)!=0)return FALSE;
    *version=(UpdateVersion){HIWORD(info.dwFileVersionMS),LOWORD(info.dwFileVersionMS),HIWORD(info.dwFileVersionLS)};return TRUE;
}
UpdateStatus UpdateInspectFile(HANDLE file,UpdateRelease *out){
    if(!out||!file||file==INVALID_HANDLE_VALUE)return UPDATE_INVALID;memset(out,0,sizeof(*out));BY_HANDLE_FILE_INFORMATION info;LARGE_INTEGER length,zero={0};
    if(GetFileType(file)!=FILE_TYPE_DISK||!GetFileInformationByHandle(file,&info)||info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)||
       !GetFileSizeEx(file,&length)||length.QuadPart<=0||length.QuadPart>UPDATE_EXE_LIMIT||!SetFilePointerEx(file,zero,NULL,FILE_BEGIN))return UPDATE_INVALID;
    DWORD size=(DWORD)length.QuadPart;BYTE *bytes=malloc(size);if(!bytes)return UPDATE_IO;UpdateStatus result=UPDATE_INVALID;DWORD used=0;
    while(used<size){DWORD got=0;if(!ReadFile(file,bytes+used,size-used,&got,NULL)||!got)goto done;used+=got;}
    if(!pe_version(bytes,size,&out->version))goto done;
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;
    BOOL ok=BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)>=0&&BCryptHashData(hash,bytes,size,0)>=0&&BCryptFinishHash(hash,out->sha256,32,0)>=0;
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(ok){out->size=size;result=UPDATE_OK;}
done:free(bytes);SetFilePointerEx(file,zero,NULL,FILE_BEGIN);return result;
}
UpdateStatus UpdateVerifyFile(HANDLE file,const UpdateRelease *release){
    if(!release||!release->size||release->size>UPDATE_EXE_LIMIT)return UPDATE_INVALID;UpdateRelease actual;
    UpdateStatus result=UpdateInspectFile(file,&actual);if(result!=UPDATE_OK)return result;
    return actual.size==release->size&&!UpdateCompareVersion(actual.version,release->version)&&!memcmp(actual.sha256,release->sha256,32)?UPDATE_OK:UPDATE_INVALID;
}
