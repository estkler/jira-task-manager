#ifndef JTM_NATIVE_SVG_H
#define JTM_NATIVE_SVG_H
#include <windows.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Restricted, bounded SVG subset; no scripts, DTDs, external resources or use cycles. */
BOOL NativeSvgDimensions(const unsigned char *bytes,size_t length,UINT *width,UINT *height);
BOOL NativeSvgRasterize(const unsigned char *bytes,size_t length,UINT source_width,UINT source_height,
    UINT width,UINT height,void *pixels);
#ifdef __cplusplus
}
#endif
#endif
