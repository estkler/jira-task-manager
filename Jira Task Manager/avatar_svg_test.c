#include "native_images.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void reject(const char *xml)
{
    NativeImageBitmap bitmap={0};
    assert(!NativeImageDecode((const unsigned char*)xml,strlen(xml),48,48,&bitmap,NULL,NULL));
    assert(!bitmap.handle&&NativeImageMemoryBytes()==0);
}
static void live_fixture(const wchar_t *path,UINT expected_source,UINT output,const BYTE color[3],UINT x,UINT y)
{
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),read=0;assert(size&&size<4096);unsigned char bytes[4096];assert(ReadFile(file,bytes,size,&read,NULL)&&read==size);CloseHandle(file);
    NativeImageBitmap bitmap={0};UINT width=0,height=0;assert(NativeImageDecode(bytes,size,48,48,&bitmap,&width,&height));assert(width==expected_source&&height==expected_source&&bitmap.width==output&&bitmap.height==output);
    DIBSECTION dib={0};assert(GetObjectW(bitmap.handle,sizeof(dib),&dib)==sizeof(dib));BYTE *pixel=(BYTE*)dib.dsBm.bmBits+(y*output+x)*4;assert(pixel[0]==color[0]&&pixel[1]==color[1]&&pixel[2]==color[2]);NativeImageBitmapFree(&bitmap);
}
int main(void)
{
    /* The missing SVG branch loses successful Jira SVG avatar downloads. */
    const char *svg="<?xml version=\"1.0\"?><svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\" viewBox=\"0 0 64 64\"><g><path fill=\"#ff0000\" d=\"M0 0h32v64H0z\"/><path fill=\"#00ff00\" d=\"M32 0h32v64H32z\"/></g></svg>";
    NativeImageBitmap bitmap={0};UINT width=0,height=0;
    assert(NativeImageDecode((const unsigned char*)svg,strlen(svg),48,48,&bitmap,&width,&height));
    assert(bitmap.width==48&&bitmap.height==48&&width==64&&height==64);
    DIBSECTION dib={0};assert(GetObjectW(bitmap.handle,sizeof(dib),&dib)==sizeof(dib));
    BYTE *left=(BYTE*)dib.dsBm.bmBits+24*48*4+8*4,*right=left+32*4;
    assert(left[0]==0&&left[1]==0&&left[2]==255&&left[3]==255);
    assert(right[0]==0&&right[1]==255&&right[2]==0&&right[3]==255);
    NativeImageBitmapFree(&bitmap);assert(NativeImageMemoryBytes()==0);
    const char *alpha_svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48'><path fill='#ff0000' opacity='0.5' d='M0 0h48v48H0z'/></svg>";
    assert(NativeImageDecode((const unsigned char*)alpha_svg,strlen(alpha_svg),48,48,&bitmap,NULL,NULL));assert(GetObjectW(bitmap.handle,sizeof(dib),&dib)==sizeof(dib));
    BYTE *alpha_pixel=dib.dsBm.bmBits;assert(alpha_pixel[0]>=123&&alpha_pixel[0]<=125&&alpha_pixel[1]==alpha_pixel[0]&&alpha_pixel[2]>=251&&alpha_pixel[2]<=253&&alpha_pixel[3]==255);NativeImageBitmapFree(&bitmap);
    const char *color_svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48' color='#00ff00'><path fill='currentColor' d='M0 0h48v48H0z'/></svg>";
    assert(NativeImageDecode((const unsigned char*)color_svg,strlen(color_svg),48,48,&bitmap,NULL,NULL));assert(GetObjectW(bitmap.handle,sizeof(dib),&dib)==sizeof(dib));
    BYTE *color_pixel=dib.dsBm.bmBits;assert(color_pixel[0]==0&&color_pixel[1]==255&&color_pixel[2]==0);NativeImageBitmapFree(&bitmap);
    const BYTE gray[]={208,199,193},orange[]={0,171,255};
    live_fixture(L"fixtures/avatar-10122.svg",24,24,gray,12,1);
    live_fixture(L"fixtures/avatar-10336.svg",120,48,orange,15,25);
    reject("<!DOCTYPE svg [<!ENTITY x SYSTEM 'file:///C:/secret'>]><svg xmlns='http://www.w3.org/2000/svg' width='48' height='48'>&x;</svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><script>alert(1)</script></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><image href='https://evil.test/x.png'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path onload='go()' d='M0 0h48v48z'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path fill='url(https://evil.test/x)' d='M0 0h48v48z'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path fill='u&#114;l(https://evil.test/x)' d='M0 0h48v48z'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path style='fill:URL(https://evil.test/x)' d='M0 0h48v48z'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><foreignObject/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg' width='999999' height='48'/>");
    reject("<svg xmlns='http://www.w3.org/2000/svg' viewBox='broken'/>");
    reject("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 8192 8192'/>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><use href='#self' id='self'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style>path{fill:red}</style></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style>@import 'https://evil.test/style.css';.x{fill:#ffffff}</style><path class='x'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style>.x{fill:url(https://evil.test/x)}</style><path class='x'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style>.x:hover{fill:#fff}</style><path class='x'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style>.x{filter:blur(2px)}</style><path class='x'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path class='undefined'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><style onload='bad()'>.x{fill:#fff}</style><path class='x'/></svg>");
    reject("<svg xmlns='http://www.w3.org/2000/svg'><path></svg>");
    char nested[2000]="<svg xmlns='http://www.w3.org/2000/svg'>";for(int i=0;i<40;i++)strcat(nested,"<g>");for(int i=0;i<40;i++)strcat(nested,"</g>");strcat(nested,"</svg>");reject(nested);
    volatile LONG cancelled=1;assert(!NativeImageDecodeWithCancel((const unsigned char*)svg,strlen(svg),48,48,&bitmap,NULL,NULL,&cancelled));
    assert(NativeImageMemoryBytes()==0);puts("Avatar SVG: OS rendered pixels, unsafe XML/resource guards, cancellation and cleanup passed");
}
