#define WIN32_LEAN_AND_MEAN
#include "native_notes.h"
#include <wincrypt.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

static void note_key(const wchar_t *account,const wchar_t *issue_key,wchar_t *key,size_t capacity)
{
    uint64_t hash=1469598103934665603ULL;
    const wchar_t *parts[]={account?account:L"",L"::",issue_key?issue_key:L""};
    for(int p=0;p<3;p++)for(const wchar_t *at=parts[p];*at;at++){
        wchar_t ch=*at;if(ch>=L'A'&&ch<=L'Z')ch=(wchar_t)(ch-L'A'+L'a');
        hash^=(uint16_t)ch;hash*=1099511628211ULL;
    }
    swprintf(key,capacity,L"note_%016llx",(unsigned long long)hash);
}

static DATA_BLOB note_entropy(void)
{
    static BYTE bytes[]="fTasks.LocalNotes.v1";
    DATA_BLOB value={(DWORD)(sizeof(bytes)-1),bytes};return value;
}

static void completion_key(const wchar_t *account,wchar_t *key,size_t capacity)
{
    uint64_t hash=1469598103934665603ULL;
    const wchar_t *text=account?account:L"";
    for(;*text;text++){
        wchar_t ch=*text;if(ch>=L'A'&&ch<=L'Z')ch=(wchar_t)(ch-L'A'+L'a');
        hash^=(uint16_t)ch;hash*=1099511628211ULL;
    }
    swprintf(key,capacity,L"template_%016llx",(unsigned long long)hash);
}

static DATA_BLOB completion_entropy(void)
{
    static BYTE bytes[]="TaskManager.CompletionTemplate.v1";
    DATA_BLOB value={(DWORD)(sizeof(bytes)-1),bytes};return value;
}

BOOL NativeNoteLoad(const wchar_t *path,const wchar_t *account,const wchar_t *issue_key,wchar_t *note,size_t capacity)
{
    if(!path||!note||capacity<1)return FALSE;note[0]=0;
    wchar_t key[64],encoded[8192];note_key(account,issue_key,key,_countof(key));
    DWORD got=GetPrivateProfileStringW(L"Notes",key,L"",encoded,_countof(encoded),path);
    if(!got)return TRUE;
    DWORD protected_size=0;
    if(!CryptStringToBinaryW(encoded,0,CRYPT_STRING_BASE64,NULL,&protected_size,NULL,NULL)||!protected_size)return FALSE;
    BYTE *protected_bytes=(BYTE*)malloc(protected_size);if(!protected_bytes)return FALSE;
    BOOL ok=CryptStringToBinaryW(encoded,0,CRYPT_STRING_BASE64,protected_bytes,&protected_size,NULL,NULL);
    DATA_BLOB input={protected_size,protected_bytes},plain={0},entropy=note_entropy();
    if(ok)ok=CryptUnprotectData(&input,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain);
    if(ok){
        size_t chars=plain.cbData/sizeof(wchar_t);ok=plain.cbData%sizeof(wchar_t)==0&&chars>0&&chars<=capacity&&((wchar_t*)plain.pbData)[chars-1]==0;
        if(ok)wcscpy_s(note,capacity,(wchar_t*)plain.pbData);
    }
    if(plain.pbData){SecureZeroMemory(plain.pbData,plain.cbData);LocalFree(plain.pbData);}
    SecureZeroMemory(protected_bytes,protected_size);free(protected_bytes);return ok;
}

BOOL NativeNoteSave(const wchar_t *path,const wchar_t *account,const wchar_t *issue_key,const wchar_t *note)
{
    if(!path||!note)return FALSE;wchar_t key[64];note_key(account,issue_key,key,_countof(key));
    if(!*note)return WritePrivateProfileStringW(L"Notes",key,NULL,path);
    size_t chars=wcslen(note)+1;if(chars>2048)return FALSE;
    DATA_BLOB input={(DWORD)(chars*sizeof(wchar_t)),(BYTE*)note},output={0},entropy=note_entropy();
    if(!CryptProtectData(&input,L"fTasks local note",&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output))return FALSE;
    DWORD encoded_chars=0;BOOL ok=CryptBinaryToStringW(output.pbData,output.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,NULL,&encoded_chars);
    wchar_t *encoded=ok?(wchar_t*)calloc(encoded_chars,sizeof(wchar_t)):NULL;if(!encoded)ok=FALSE;
    if(ok)ok=CryptBinaryToStringW(output.pbData,output.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded,&encoded_chars);
    if(ok)ok=WritePrivateProfileStringW(L"Notes",key,encoded,path);
    if(encoded){SecureZeroMemory(encoded,encoded_chars*sizeof(wchar_t));free(encoded);}
    SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return ok;
}

BOOL NativeCompletionTemplateLoad(const wchar_t *path,const wchar_t *account,wchar_t *text,size_t capacity)
{
    if(!path||!*path||!text||capacity<1)return FALSE;text[0]=0;
    wchar_t key[64],encoded[8192];completion_key(account,key,_countof(key));
    DWORD got=GetPrivateProfileStringW(L"Completion",key,L"",encoded,_countof(encoded),path);
    if(!got)return TRUE;
    DWORD protected_size=0;
    if(!CryptStringToBinaryW(encoded,0,CRYPT_STRING_BASE64,NULL,&protected_size,NULL,NULL)||!protected_size)return FALSE;
    BYTE *protected_bytes=(BYTE*)malloc(protected_size);if(!protected_bytes)return FALSE;
    BOOL ok=CryptStringToBinaryW(encoded,0,CRYPT_STRING_BASE64,protected_bytes,&protected_size,NULL,NULL);
    DATA_BLOB input={protected_size,protected_bytes},plain={0},entropy=completion_entropy();
    if(ok)ok=CryptUnprotectData(&input,NULL,&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain);
    if(ok){
        size_t chars=plain.cbData/sizeof(wchar_t);ok=plain.cbData%sizeof(wchar_t)==0&&chars>0&&chars<=capacity&&((wchar_t*)plain.pbData)[chars-1]==0;
        if(ok)wcscpy_s(text,capacity,(wchar_t*)plain.pbData);
    }
    if(plain.pbData){SecureZeroMemory(plain.pbData,plain.cbData);LocalFree(plain.pbData);}
    SecureZeroMemory(protected_bytes,protected_size);free(protected_bytes);return ok;
}

BOOL NativeCompletionTemplateSave(const wchar_t *path,const wchar_t *account,const wchar_t *text)
{
    if(!path||!*path||!text)return FALSE;wchar_t key[64];completion_key(account,key,_countof(key));
    if(!*text)return WritePrivateProfileStringW(L"Completion",key,NULL,path);
    size_t chars=wcslen(text)+1;if(chars>2048)return FALSE;
    DATA_BLOB input={(DWORD)(chars*sizeof(wchar_t)),(BYTE*)text},output={0},entropy=completion_entropy();
    if(!CryptProtectData(&input,L"Task Manager completion template",&entropy,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output))return FALSE;
    DWORD encoded_chars=0;BOOL ok=CryptBinaryToStringW(output.pbData,output.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,NULL,&encoded_chars);
    wchar_t *encoded=ok?(wchar_t*)calloc(encoded_chars,sizeof(wchar_t)):NULL;if(!encoded)ok=FALSE;
    if(ok)ok=CryptBinaryToStringW(output.pbData,output.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded,&encoded_chars);
    if(ok)ok=WritePrivateProfileStringW(L"Completion",key,encoded,path);
    if(encoded){SecureZeroMemory(encoded,encoded_chars*sizeof(wchar_t));free(encoded);}
    SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return ok;
}
