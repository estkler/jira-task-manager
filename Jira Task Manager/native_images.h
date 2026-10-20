#ifndef TASK_MANAGER_IMAGES_H
#define TASK_MANAGER_IMAGES_H
#include <windows.h>
#include <stddef.h>
#define NATIVE_IMAGE_MAX_BYTES (6u*1024u*1024u)
#define NATIVE_IMAGE_FEED_BYTES (12u*1024u*1024u)
#define NATIVE_IMAGE_MAX_SOURCE_PIXELS (12u*1024u*1024u)
typedef struct NativeImageBitmap { HBITMAP handle;UINT width,height;size_t bytes; } NativeImageBitmap;
typedef struct NativeImage {
    volatile LONG references;
    unsigned char *encoded;size_t length;
    UINT width,height;
    NativeImageBitmap thumbnail;
} NativeImage;
/* Create takes ownership of bytes, including on failure. Decode is used off the UI thread. */
NativeImage *NativeImageCreate(unsigned char *bytes,size_t length);
NativeImage *NativeImageCreateSized(unsigned char *bytes,size_t length,UINT width,UINT height);
void NativeImageRetain(NativeImage *image);
void NativeImageRelease(NativeImage *image);
BOOL NativeImageDecode(const unsigned char *bytes,size_t length,UINT maximum_width,UINT maximum_height,
    NativeImageBitmap *bitmap,UINT *source_width,UINT *source_height);
BOOL NativeImageDecodeWithCancel(const unsigned char *bytes,size_t length,UINT maximum_width,UINT maximum_height,
    NativeImageBitmap *bitmap,UINT *source_width,UINT *source_height,const volatile LONG *cancelled);
void NativeImageBitmapFree(NativeImageBitmap *bitmap);
void NativeImageDraw(HDC dc,const NativeImageBitmap *bitmap,RECT area);
size_t NativeImageMemoryBytes(void);
#endif
