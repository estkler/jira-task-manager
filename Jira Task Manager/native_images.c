#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include "native_images.h"
#include "native_svg.h"
#include <wincodec.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static volatile LONG64 image_encoded_bytes,image_bitmap_bytes;
static SRWLOCK image_decode_lock=SRWLOCK_INIT;
static BOOL ReserveImageBytes(volatile LONG64 *counter,size_t bytes,size_t maximum)
{
    LONG64 previous=InterlockedExchangeAdd64(counter,(LONG64)bytes);
    if(previous+(LONG64)bytes<=(LONG64)maximum)return TRUE;
    InterlockedExchangeAdd64(counter,-(LONG64)bytes);return FALSE;
}
size_t NativeImageMemoryBytes(void)
{return (size_t)(InterlockedCompareExchange64(&image_encoded_bytes,0,0)+InterlockedCompareExchange64(&image_bitmap_bytes,0,0));}
void NativeImageBitmapFree(NativeImageBitmap *bitmap)
{
    if(!bitmap)return;if(bitmap->handle)DeleteObject(bitmap->handle);
    if(bitmap->bytes)InterlockedExchangeAdd64(&image_bitmap_bytes,-(LONG64)bitmap->bytes);memset(bitmap,0,sizeof(*bitmap));
}
BOOL NativeImageDecodeWithCancel(const unsigned char *bytes,size_t length,UINT maximum_width,UINT maximum_height,
    NativeImageBitmap *bitmap,UINT *source_width,UINT *source_height,const volatile LONG *cancelled)
{
    if(!bitmap)return FALSE;memset(bitmap,0,sizeof(*bitmap));
    if(!bytes||!length||length>NATIVE_IMAGE_MAX_BYTES||!maximum_width||!maximum_height||maximum_width>1920||maximum_height>1440)return FALSE;
    /* Limit codec scratch memory as well as the retained cache: one decode at a time. */
    AcquireSRWLockExclusive(&image_decode_lock);
    if(cancelled&&InterlockedCompareExchange((volatile LONG*)cancelled,0,0)){ReleaseSRWLockExclusive(&image_decode_lock);return FALSE;}
    HRESULT apartment=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE){ReleaseSRWLockExclusive(&image_decode_lock);return FALSE;}
    IWICImagingFactory *factory=NULL;IWICStream *stream=NULL;IWICBitmapDecoder *decoder=NULL;
    IWICBitmapFrameDecode *frame=NULL;IWICBitmapFlipRotator *rotator=NULL;IWICBitmapScaler *scaler=NULL;IWICFormatConverter *converter=NULL;
    HRESULT result=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory);
    if(SUCCEEDED(result))result=IWICImagingFactory_CreateStream(factory,&stream);
    if(SUCCEEDED(result))result=IWICStream_InitializeFromMemory(stream,(BYTE*)bytes,(DWORD)length);
    if(SUCCEEDED(result))result=IWICImagingFactory_CreateDecoderFromStream(factory,(IStream*)stream,NULL,WICDecodeMetadataCacheOnDemand,&decoder);
    if(SUCCEEDED(result))result=IWICBitmapDecoder_GetFrame(decoder,0,&frame);
    UINT width=0,height=0;BOOL svg=FALSE;if(SUCCEEDED(result))result=IWICBitmapFrameDecode_GetSize(frame,&width,&height);
    if(FAILED(result)&&NativeSvgDimensions(bytes,length,&width,&height)){svg=TRUE;result=S_OK;}
    if(SUCCEEDED(result)&&(!width||!height||width>8192||height>8192||(uint64_t)width*height>NATIVE_IMAGE_MAX_SOURCE_PIXELS))result=E_INVALIDARG;
    IWICBitmapSource *source=(IWICBitmapSource*)frame;
    /* JPEG EXIF orientation is optional metadata; missing metadata is harmless. */
    IWICMetadataQueryReader *metadata=NULL;PROPVARIANT orientation;PropVariantInit(&orientation);
    if(SUCCEEDED(result)&&!svg&&SUCCEEDED(IWICBitmapFrameDecode_GetMetadataQueryReader(frame,&metadata))){
        if(SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(metadata,L"/app1/ifd/{ushort=274}",&orientation))&&orientation.vt==VT_UI2&&orientation.uiVal>=2&&orientation.uiVal<=8){
            const WICBitmapTransformOptions options[]={0,0,WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate180,WICBitmapTransformFlipVertical,
                WICBitmapTransformRotate90|WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate90,
                WICBitmapTransformRotate270|WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate270};
            result=IWICImagingFactory_CreateBitmapFlipRotator(factory,&rotator);
            if(SUCCEEDED(result))result=IWICBitmapFlipRotator_Initialize(rotator,source,options[orientation.uiVal]);
            if(SUCCEEDED(result)){source=(IWICBitmapSource*)rotator;result=IWICBitmapFlipRotator_GetSize(rotator,&width,&height);}
        }
    }
    PropVariantClear(&orientation);if(metadata)IWICMetadataQueryReader_Release(metadata);
    UINT output_width=width,output_height=height;
    if(SUCCEEDED(result)){
        double factor=min(1.0,min((double)maximum_width/width,(double)maximum_height/height));
        output_width=max(1,(UINT)(width*factor));output_height=max(1,(UINT)(height*factor));
        if(!svg&&(output_width!=width||output_height!=height)){
            result=IWICImagingFactory_CreateBitmapScaler(factory,&scaler);
            if(SUCCEEDED(result))result=IWICBitmapScaler_Initialize(scaler,source,output_width,output_height,WICBitmapInterpolationModeFant);
            if(SUCCEEDED(result))source=(IWICBitmapSource*)scaler;
        }
    }
    if(SUCCEEDED(result)&&!svg)result=IWICImagingFactory_CreateFormatConverter(factory,&converter);
    if(SUCCEEDED(result)&&!svg)result=IWICFormatConverter_Initialize(converter,source,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0.0,WICBitmapPaletteTypeCustom);
    size_t allocation=(size_t)output_width*output_height*4;void *pixels=NULL;
    if(cancelled&&InterlockedCompareExchange((volatile LONG*)cancelled,0,0))result=HRESULT_FROM_WIN32(ERROR_CANCELLED);
    if(SUCCEEDED(result)){
        if(!ReserveImageBytes(&image_bitmap_bytes,allocation,16u*1024u*1024u))result=E_OUTOFMEMORY;
        else{
            bitmap->bytes=allocation;BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=(LONG)output_width;info.bmiHeader.biHeight=-(LONG)output_height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            bitmap->handle=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);if(!bitmap->handle)result=E_OUTOFMEMORY;
        }
    }
    if(SUCCEEDED(result))result=svg?(NativeSvgRasterize(bytes,length,width,height,output_width,output_height,pixels)?S_OK:E_FAIL):IWICFormatConverter_CopyPixels(converter,NULL,output_width*4,(UINT)allocation,pixels);
    if(cancelled&&InterlockedCompareExchange((volatile LONG*)cancelled,0,0))result=HRESULT_FROM_WIN32(ERROR_CANCELLED);
    if(SUCCEEDED(result)){
        /* Flatten transparency onto a neutral checkerboard without another DLL. */
        BYTE *pixel=pixels;
        for(UINT y=0;y<output_height;y++)for(UINT x=0;x<output_width;x++,pixel+=4){
            unsigned background=((x/8+y/8)&1)?232:248,alpha=pixel[3];
            for(int channel=0;channel<3;channel++)pixel[channel]=(BYTE)((pixel[channel]*alpha+background*(255-alpha)+127)/255);pixel[3]=255;
        }
        bitmap->width=output_width;bitmap->height=output_height;
        if(source_width)*source_width=width;if(source_height)*source_height=height;
    }else NativeImageBitmapFree(bitmap);
    if(converter)IWICFormatConverter_Release(converter);if(scaler)IWICBitmapScaler_Release(scaler);
    if(rotator)IWICBitmapFlipRotator_Release(rotator);if(frame)IWICBitmapFrameDecode_Release(frame);
    if(decoder)IWICBitmapDecoder_Release(decoder);if(stream)IWICStream_Release(stream);if(factory)IWICImagingFactory_Release(factory);
    if(SUCCEEDED(apartment))CoUninitialize();ReleaseSRWLockExclusive(&image_decode_lock);return SUCCEEDED(result);
}
BOOL NativeImageDecode(const unsigned char *bytes,size_t length,UINT width,UINT height,NativeImageBitmap *bitmap,UINT *source_width,UINT *source_height)
{return NativeImageDecodeWithCancel(bytes,length,width,height,bitmap,source_width,source_height,NULL);}
NativeImage *NativeImageCreateSized(unsigned char *bytes,size_t length,UINT width,UINT height)
{
    if(!bytes||!length||length>NATIVE_IMAGE_MAX_BYTES){free(bytes);return NULL;}
    if(!ReserveImageBytes(&image_encoded_bytes,length,24u*1024u*1024u)){free(bytes);return NULL;}
    NativeImage *image=calloc(1,sizeof(*image));
    if(image){image->references=1;image->encoded=bytes;image->length=length;
        if(NativeImageDecode(bytes,length,width,height,&image->thumbnail,&image->width,&image->height))return image;
        free(image);
    }
    InterlockedExchangeAdd64(&image_encoded_bytes,-(LONG64)length);free(bytes);return NULL;
}
NativeImage *NativeImageCreate(unsigned char *bytes,size_t length){return NativeImageCreateSized(bytes,length,320,200);}
void NativeImageRetain(NativeImage *image){if(image)InterlockedIncrement(&image->references);}
void NativeImageRelease(NativeImage *image)
{
    if(!image||InterlockedDecrement(&image->references))return;
    NativeImageBitmapFree(&image->thumbnail);InterlockedExchangeAdd64(&image_encoded_bytes,-(LONG64)image->length);
    free(image->encoded);free(image);
}
void NativeImageDraw(HDC dc,const NativeImageBitmap *bitmap,RECT area)
{
    if(!bitmap||!bitmap->handle||!bitmap->width||!bitmap->height||area.right<=area.left||area.bottom<=area.top)return;
    double factor=min((double)(area.right-area.left)/bitmap->width,(double)(area.bottom-area.top)/bitmap->height);
    int width=max(1,(int)(bitmap->width*factor)),height=max(1,(int)(bitmap->height*factor));
    HDC memory=CreateCompatibleDC(dc);if(!memory)return;HGDIOBJ previous=SelectObject(memory,bitmap->handle);
    int mode=SetStretchBltMode(dc,HALFTONE);POINT origin;SetBrushOrgEx(dc,0,0,&origin);
    StretchBlt(dc,area.left+(area.right-area.left-width)/2,area.top+(area.bottom-area.top-height)/2,width,height,memory,0,0,bitmap->width,bitmap->height,SRCCOPY);
    SetBrushOrgEx(dc,origin.x,origin.y,NULL);SetStretchBltMode(dc,mode);SelectObject(memory,previous);DeleteDC(memory);
}
