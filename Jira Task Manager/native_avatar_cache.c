#define WIN32_LEAN_AND_MEAN
#include "native_avatar_cache.h"
#include <bcrypt.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static SRWLOCK avatar_disk_lock=SRWLOCK_INIT;
typedef struct AvatarHeader { BYTE magic[8];DWORD length;BYTE digest[32]; } AvatarHeader;
static const BYTE avatar_magic[8]={'J','T','M','A','V','1',0,0};
static BOOL HashParts(const void *const *parts,const size_t *lengths,size_t count,BYTE digest[32])
{
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;BOOL ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)==0;
    if(ok)ok=BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)==0;
    for(size_t i=0;ok&&i<count;i++){
        /* Length prefixes make scope components unambiguous; secret tokens never enter this hash. */
        ULONGLONG length=lengths[i];ok=BCryptHashData(hash,(BYTE*)&length,sizeof(length),0)==0&&BCryptHashData(hash,(BYTE*)parts[i],(ULONG)length,0)==0;
    }
    if(ok)ok=BCryptFinishHash(hash,digest,32,0)==0;
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);return ok;
}
static void Hex(const BYTE digest[32],wchar_t text[65])
{const wchar_t hex[]=L"0123456789abcdef";for(int i=0;i<32;i++){text[i*2]=hex[digest[i]>>4];text[i*2+1]=hex[digest[i]&15];}text[64]=0;}
UINT NativeAvatarCacheDays(const wchar_t *settings_path)
{
    if(!settings_path||!*settings_path)return 7;wchar_t text[16];GetPrivateProfileStringW(L"Advanced",L"AvatarCacheDays",L"7",text,_countof(text),settings_path);
    UINT days=0;if(!*text)return 7;for(const wchar_t *at=text;*at;at++){if(*at<L'0'||*at>L'9')return 7;days=days*10+(*at-L'0');if(days>365)return 7;}
    return days?days:7;
}
BOOL NativeAvatarCacheSaveDays(const wchar_t *settings_path,UINT days)
{if(!settings_path||!*settings_path||days<1||days>365)return FALSE;wchar_t value[16];swprintf(value,16,L"%u",days);return WritePrivateProfileStringW(L"Advanced",L"AvatarCacheDays",value,settings_path);}
BOOL NativeAvatarCacheConfigure(NativeAvatarCache *cache,const wchar_t *settings_path,const wchar_t *host,USHORT port,const wchar_t *prefix,const wchar_t *account)
{
    if(!cache)return FALSE;memset(cache,0,sizeof(*cache));
    if(!settings_path||!*settings_path||!host||!*host||!prefix||!account||!*account||wcslen(account)>255)return FALSE;
    wchar_t directory[MAX_PATH],normalized_host[256];DWORD chars=GetFullPathNameW(settings_path,MAX_PATH,directory,NULL);
    if(!chars||chars>=MAX_PATH||!PathRemoveFileSpecW(directory)||!PathAppendW(directory,L"avatar-cache"))return FALSE;
    if(wcslen(host)>=_countof(normalized_host))return FALSE;wcscpy_s(normalized_host,_countof(normalized_host),host);CharLowerBuffW(normalized_host,(DWORD)wcslen(normalized_host));
    const void *parts[]={normalized_host,&port,prefix,account};size_t lengths[]={wcslen(normalized_host)*sizeof(wchar_t),sizeof(port),wcslen(prefix)*sizeof(wchar_t),wcslen(account)*sizeof(wchar_t)};BYTE digest[32];
    if(!HashParts(parts,lengths,_countof(parts),digest))return FALSE;
    wcscpy_s(cache->root,MAX_PATH,directory);Hex(digest,cache->scope);cache->days=NativeAvatarCacheDays(settings_path);return TRUE;
}
static BOOL CachePath(const NativeAvatarCache *cache,const wchar_t *relative,wchar_t path[MAX_PATH])
{
    if(!cache||!cache->root[0]||!cache->scope[0]||!relative||relative[0]!=L'/'||wcslen(relative)>1536)return FALSE;
    const void *parts[]={cache->scope,relative};size_t lengths[]={64*sizeof(wchar_t),wcslen(relative)*sizeof(wchar_t)};BYTE digest[32];wchar_t hex[65];
    if(!HashParts(parts,lengths,2,digest))return FALSE;Hex(digest,hex);
    return swprintf(path,MAX_PATH,L"%ls\\%ls.avatar",cache->root,hex)>0;
}
static BOOL SafeDirectory(const wchar_t *path,BOOL create)
{
    DWORD attributes=GetFileAttributesW(path);
    if(attributes==INVALID_FILE_ATTRIBUTES&&create){if(!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;attributes=GetFileAttributesW(path);}
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}
static ULONGLONG Stamp(FILETIME time){return ((ULONGLONG)time.dwHighDateTime<<32)|time.dwLowDateTime;}
NativeImage *NativeAvatarCacheRead(const NativeAvatarCache *cache,const wchar_t *relative,BOOL *fresh)
{
    if(fresh)*fresh=FALSE;wchar_t path[MAX_PATH];if(!CachePath(cache,relative,path))return NULL;
    AcquireSRWLockShared(&avatar_disk_lock);NativeImage *image=NULL;
    if(!SafeDirectory(cache->root,FALSE)){ReleaseSRWLockShared(&avatar_disk_lock);return NULL;}
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file!=INVALID_HANDLE_VALUE){
        BY_HANDLE_FILE_INFORMATION info={0};AvatarHeader header={0};DWORD count=0;BYTE digest[32];unsigned char *bytes=NULL;
        BOOL ok=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&!info.nFileSizeHigh&&
            info.nFileSizeLow>sizeof(header)&&info.nFileSizeLow<=sizeof(header)+NATIVE_AVATAR_MAX_BYTES&&ReadFile(file,&header,sizeof(header),&count,NULL)&&count==sizeof(header)&&
            !memcmp(header.magic,avatar_magic,sizeof(avatar_magic))&&header.length&&header.length<=NATIVE_AVATAR_MAX_BYTES&&header.length+sizeof(header)==info.nFileSizeLow;
        if(ok){bytes=malloc(header.length);ok=bytes&&ReadFile(file,bytes,header.length,&count,NULL)&&count==header.length;}
        if(ok){const void *parts[]={bytes};size_t lengths[]={header.length};ok=HashParts(parts,lengths,1,digest)&&!memcmp(digest,header.digest,32);}
        if(ok){image=NativeImageCreateSized(bytes,header.length,48,48);bytes=NULL;
            FILETIME now;GetSystemTimeAsFileTime(&now);ULONGLONG written=Stamp(info.ftLastWriteTime),current=Stamp(now),ttl=(ULONGLONG)cache->days*86400u*10000000u;
            if(fresh&&image)*fresh=written<=current&&current-written<ttl;
        }
        free(bytes);CloseHandle(file);
    }
    ReleaseSRWLockShared(&avatar_disk_lock);return image;
}
typedef struct CacheEntry { wchar_t name[72];ULONGLONG stamp;DWORD bytes; } CacheEntry;
static BOOL CacheFilename(const wchar_t *name)
{if(wcslen(name)!=71||wcscmp(name+64,L".avatar"))return FALSE;for(int i=0;i<64;i++)if(!((name[i]>=L'0'&&name[i]<=L'9')||(name[i]>=L'a'&&name[i]<=L'f')))return FALSE;return TRUE;}
static BOOL TemporaryFilename(const wchar_t *name)
{
    const wchar_t *at=name;unsigned digits=0;while(*at>=L'0'&&*at<=L'9'){at++;digits++;}if(!digits||digits>10||*at++!=L'-'||wcslen(at)!=20||wcscmp(at+16,L".tmp"))return FALSE;
    for(int i=0;i<16;i++)if(!((at[i]>=L'0'&&at[i]<=L'9')||(at[i]>=L'a'&&at[i]<=L'f')))return FALSE;return TRUE;
}
static BOOL TemporaryUsage(const wchar_t *directory,unsigned *count,ULONGLONG *bytes)
{
    *count=0;*bytes=0;wchar_t pattern[MAX_PATH];if(swprintf(pattern,MAX_PATH,L"%ls\\*.tmp",directory)<=0)return FALSE;
    WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);if(search==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
    FILETIME now;GetSystemTimeAsFileTime(&now);ULONGLONG current=Stamp(now);unsigned seen=0;BOOL ok=TRUE;
    do{
        if(++seen>4096){ok=FALSE;break;}if(!TemporaryFilename(found.cFileName)||found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        ULONGLONG written=Stamp(found.ftLastWriteTime);
        if(written<=current&&current-written>5ull*60*10000000){wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",directory,found.cFileName);if(DeleteFileW(path))continue;}
        (*count)++;*bytes+=found.nFileSizeHigh?MAXDWORD:found.nFileSizeLow;
    }while(FindNextFileW(search,&found));FindClose(search);return ok;
}
static BOOL TrimCache(const wchar_t *directory,size_t incoming)
{
    unsigned temporary_count=0;ULONGLONG temporary_bytes=0;if(!TemporaryUsage(directory,&temporary_count,&temporary_bytes)||temporary_count>=NATIVE_AVATAR_CACHE_MAX_FILES||temporary_bytes+incoming>NATIVE_AVATAR_CACHE_MAX_BYTES)return FALSE;
    wchar_t pattern[MAX_PATH];if(swprintf(pattern,MAX_PATH,L"%ls\\*.avatar",directory)<=0)return FALSE;
    CacheEntry entries[NATIVE_AVATAR_CACHE_MAX_FILES+1];unsigned kept=0,seen=0;ULONGLONG total=0;WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);
    if(search==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
    BOOL ok=TRUE;
    do{
        if(++seen>4096){ok=FALSE;break;}if(!CacheFilename(found.cFileName)||found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        CacheEntry entry={0};wcscpy_s(entry.name,_countof(entry.name),found.cFileName);entry.stamp=Stamp(found.ftLastWriteTime);entry.bytes=found.nFileSizeHigh?MAXDWORD:found.nFileSizeLow;
        if(kept==_countof(entries)){
            unsigned oldest=0;for(unsigned i=1;i<kept;i++)if(entries[i].stamp<entries[oldest].stamp)oldest=i;
            wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",directory,entries[oldest].name);
            if(!DeleteFileW(path)){ok=FALSE;break;}total-=entries[oldest].bytes;entries[oldest]=entries[--kept];
        }
        entries[kept++]=entry;total+=entry.bytes;
    }while(FindNextFileW(search,&found));FindClose(search);
    while(ok&&(kept+temporary_count>=NATIVE_AVATAR_CACHE_MAX_FILES||total+temporary_bytes+incoming>NATIVE_AVATAR_CACHE_MAX_BYTES)){
        if(!kept){ok=FALSE;break;}unsigned oldest=0;for(unsigned i=1;i<kept;i++)if(entries[i].stamp<entries[oldest].stamp)oldest=i;
        wchar_t path[MAX_PATH];swprintf(path,MAX_PATH,L"%ls\\%ls",directory,entries[oldest].name);
        if(!DeleteFileW(path)){ok=FALSE;break;}total-=entries[oldest].bytes;entries[oldest]=entries[--kept];
    }
    return ok;
}
BOOL NativeAvatarCacheWrite(const NativeAvatarCache *cache,const wchar_t *relative,const NativeImage *image)
{
    if(!image||!image->encoded||!image->length||image->length>NATIVE_AVATAR_MAX_BYTES||!image->thumbnail.handle)return FALSE;
    wchar_t path[MAX_PATH],temporary[MAX_PATH];if(!CachePath(cache,relative,path))return FALSE;
    AcquireSRWLockExclusive(&avatar_disk_lock);BOOL ok=SafeDirectory(cache->root,TRUE)&&TrimCache(cache->root,image->length+sizeof(AvatarHeader));
    AvatarHeader header={0};memcpy(header.magic,avatar_magic,8);header.length=(DWORD)image->length;
    const void *parts[]={image->encoded};size_t lengths[]={image->length};if(ok)ok=HashParts(parts,lengths,1,header.digest);
    BYTE random[8];if(ok)ok=BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0;
    if(ok)ok=swprintf(temporary,MAX_PATH,L"%ls\\%lu-%02x%02x%02x%02x%02x%02x%02x%02x.tmp",cache->root,GetCurrentProcessId(),random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7])>0;
    HANDLE file=ok?CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL):INVALID_HANDLE_VALUE;
    if(file==INVALID_HANDLE_VALUE)ok=FALSE;
    else{DWORD count=0;ok=WriteFile(file,&header,sizeof(header),&count,NULL)&&count==sizeof(header)&&WriteFile(file,image->encoded,(DWORD)image->length,&count,NULL)&&count==image->length&&FlushFileBuffers(file);CloseHandle(file);
        if(ok){DWORD attributes=GetFileAttributesW(path);ok=attributes==INVALID_FILE_ATTRIBUTES||!(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY));if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
        if(!ok)DeleteFileW(temporary);
    }
    ReleaseSRWLockExclusive(&avatar_disk_lock);return ok;
}
