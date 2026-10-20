#ifndef JTM_AVATAR_CACHE_H
#define JTM_AVATAR_CACHE_H
#include "native_images.h"
#define NATIVE_AVATAR_MAX_BYTES (128u*1024u)
#define NATIVE_AVATAR_CACHE_MAX_FILES 256
#define NATIVE_AVATAR_CACHE_MAX_BYTES (32u*1024u*1024u)
typedef struct NativeAvatarCache { wchar_t root[MAX_PATH],scope[65];UINT days; } NativeAvatarCache;
UINT NativeAvatarCacheDays(const wchar_t *settings_path);
BOOL NativeAvatarCacheSaveDays(const wchar_t *settings_path,UINT days);
BOOL NativeAvatarCacheConfigure(NativeAvatarCache *cache,const wchar_t *settings_path,
    const wchar_t *host,USHORT port,const wchar_t *prefix,const wchar_t *account);
NativeImage *NativeAvatarCacheRead(const NativeAvatarCache *cache,const wchar_t *trusted_relative,BOOL *fresh);
BOOL NativeAvatarCacheWrite(const NativeAvatarCache *cache,const wchar_t *trusted_relative,const NativeImage *image);
#endif
