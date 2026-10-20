/* Synthetic in-memory BMP for tests/offline previews, never product data. */
#ifndef JTM_IMAGE_FIXTURE_H
#define JTM_IMAGE_FIXTURE_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *ImageFixtureBytes(UINT width,UINT height,size_t *length)
{
    size_t stride=((size_t)width*3+3)&~3u;*length=54+stride*height;
    unsigned char *bytes=calloc(1,*length);if(!bytes)return NULL;
    BITMAPFILEHEADER file={0};BITMAPINFOHEADER info={0};file.bfType=0x4d42;file.bfSize=(DWORD)*length;file.bfOffBits=54;
    info.biSize=40;info.biWidth=(LONG)width;info.biHeight=(LONG)height;info.biPlanes=1;info.biBitCount=24;info.biCompression=BI_RGB;
    memcpy(bytes,&file,sizeof(file));memcpy(bytes+14,&info,sizeof(info));
    for(UINT y=0;y<height;y++)for(UINT x=0;x<width;x++){
        BYTE *pixel=bytes+54+stride*y+x*3;
        pixel[0]=(BYTE)(80+(x*120/max(1,width)));pixel[1]=(BYTE)(80+y*120/max(1,height));pixel[2]=220;
        if(x>width/5&&x<width*4/5&&y>height/4&&y<height*3/4){pixel[0]=210;pixel[1]=190;pixel[2]=30;}
    }
    return bytes;
}
#endif
