#ifndef NATIVE_TIMER_RENDER_H
#define NATIVE_TIMER_RENDER_H
/* Timer-only trusted resource decoder: preserve PNG transparency, unlike the
   checkerboard used for Jira attachment previews. No network or runtime assets. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <wincodec.h>
static HFONT g_timer_bold_font;
static BOOL g_timer_overrun;
static int g_timer_bar_amount;
static wchar_t g_timer_numbers[48];
static struct {HBITMAP bitmap;UINT width,height;BOOL attempted;} g_timer_emoji;
static void TimerEmojiFree(void)
{
    if(g_timer_emoji.bitmap)DeleteObject(g_timer_emoji.bitmap);
    memset(&g_timer_emoji,0,sizeof(g_timer_emoji));
}
static BOOL TimerEmojiLoad(void)
{
    if(g_timer_emoji.attempted)return g_timer_emoji.bitmap!=NULL;
    g_timer_emoji.attempted=TRUE;
    HRSRC resource=FindResourceW(g_instance,MAKEINTRESOURCEW(501),RT_RCDATA);
    DWORD length=resource?SizeofResource(g_instance,resource):0;
    HGLOBAL loaded=resource?LoadResource(g_instance,resource):NULL;
    const BYTE *bytes=loaded?LockResource(loaded):NULL;
    if(!bytes||!length||length>1024*1024)return FALSE;
    HRESULT apartment=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE)return FALSE;
    IWICImagingFactory *factory=NULL;IWICStream *stream=NULL;IWICBitmapDecoder *decoder=NULL;
    IWICBitmapFrameDecode *frame=NULL;IWICFormatConverter *converter=NULL;
    HRESULT result=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory);
    if(SUCCEEDED(result))result=IWICImagingFactory_CreateStream(factory,&stream);
    if(SUCCEEDED(result))result=IWICStream_InitializeFromMemory(stream,(BYTE*)bytes,length);
    if(SUCCEEDED(result))result=IWICImagingFactory_CreateDecoderFromStream(factory,(IStream*)stream,NULL,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(result))result=IWICBitmapDecoder_GetFrame(decoder,0,&frame);
    UINT width=0,height=0;
    if(SUCCEEDED(result))result=IWICBitmapFrameDecode_GetSize(frame,&width,&height);
    if(SUCCEEDED(result)&&(!width||!height||width>256||height>256))result=E_INVALIDARG;
    if(SUCCEEDED(result))result=IWICImagingFactory_CreateFormatConverter(factory,&converter);
    if(SUCCEEDED(result))result=IWICFormatConverter_Initialize(converter,(IWICBitmapSource*)frame,&GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom);
    void *pixels=NULL;HBITMAP bitmap=NULL;
    if(SUCCEEDED(result)){
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=(LONG)width;info.bmiHeader.biHeight=-(LONG)height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
        if(!bitmap)result=E_OUTOFMEMORY;
    }
    if(SUCCEEDED(result))result=IWICFormatConverter_CopyPixels(converter,NULL,width*4,width*height*4,pixels);
    if(converter)IWICFormatConverter_Release(converter);if(frame)IWICBitmapFrameDecode_Release(frame);
    if(decoder)IWICBitmapDecoder_Release(decoder);if(stream)IWICStream_Release(stream);if(factory)IWICImagingFactory_Release(factory);
    if(SUCCEEDED(apartment))CoUninitialize();
    if(FAILED(result)){if(bitmap)DeleteObject(bitmap);return FALSE;}
    g_timer_emoji.bitmap=bitmap;g_timer_emoji.width=width;g_timer_emoji.height=height;return TRUE;
}
static const wchar_t *TimerHomeCaption(void){return g_russian?L" · Пора домой ":L" · Time to go home ";}
static int TimerNumberSlotWidthForMode(HDC dc,BOOL text)
{
    const wchar_t *sample=text?(g_russian?L"88ч 88м 88с":L"88h 88m 88s"):L"88:88:88";
    HGDIOBJ old=SelectObject(dc,g_timer_font);SIZE normal,bold;
    GetTextExtentPoint32W(dc,sample,(int)wcslen(sample),&normal);SelectObject(dc,g_timer_bold_font);
    GetTextExtentPoint32W(dc,sample,(int)wcslen(sample),&bold);SelectObject(dc,old);
    return max(normal.cx,bold.cx)+D(3);
}
static int TimerNumberSlotWidth(HDC dc){return g_bar_time?D(110):TimerNumberSlotWidthForMode(dc,g_text_time);}
static int TimerReservedWidth(HDC dc)
{
    if(!g_timer_overrun){
        if(g_bar_time)return D(114);
        HGDIOBJ old=SelectObject(dc,g_timer_font);SIZE number={0};
        GetTextExtentPoint32W(dc,g_timer_numbers,(int)wcslen(g_timer_numbers),&number);SelectObject(dc,old);
        return number.cx+D(4);
    }
    HGDIOBJ old=SelectObject(dc,g_small_font);SIZE caption;
    const wchar_t *text=TimerHomeCaption();GetTextExtentPoint32W(dc,text,(int)wcslen(text),&caption);SelectObject(dc,old);
    return TimerNumberSlotWidth(dc)+caption.cx+D(16)+D(4);
}
static void DrawTimerCaption(HDC dc,RECT rect,COLORREF ordinary)
{
    HGDIOBJ old=SelectObject(dc,g_timer_overrun?g_timer_bold_font:g_timer_font);
    int number_width=TimerNumberSlotWidth(dc);RECT numbers=rect;numbers.right=min(rect.right,rect.left+number_width);
    SetTextColor(dc,g_timer_overrun?(g_dark?RGB(255,111,116):RGB(190,30,40)):ordinary);
    if(g_bar_time){
        int width=min(D(110),max(0,numbers.right-numbers.left)),height=D(6),y=rect.top+(rect.bottom-rect.top-height)/2;
        HRGN shape=width?CreateRoundRectRgn(rect.left,y,rect.left+width+1,y+height+1,height,height):NULL;
        if(shape){HBRUSH track=CreateSolidBrush(g_dark?RGB(66,70,76):RGB(224,229,235));
            HBRUSH fill=CreateSolidBrush(g_timer_overrun?(g_dark?RGB(255,111,116):RGB(190,30,40)):AccentColor());
            FillRgn(dc,shape,track);int saved=SaveDC(dc);if(saved){IntersectClipRect(dc,rect.left,y,rect.left+MulDiv(width,g_timer_bar_amount,1000),y+height+1);FillRgn(dc,shape,fill);RestoreDC(dc,saved);}
            DeleteObject(track);DeleteObject(fill);DeleteObject(shape);
        }
    }else DrawTextW(dc,g_timer_numbers,-1,&numbers,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    if(g_timer_overrun){
        SelectObject(dc,g_small_font);SetTextColor(dc,TextColor());
        RECT caption=rect;caption.left=min(rect.right,rect.left+number_width);
        const wchar_t *text=TimerHomeCaption();SIZE measured;GetTextExtentPoint32W(dc,text,(int)wcslen(text),&measured);
        int emoji_left=caption.left+measured.cx,emoji_size=D(16);
        DrawTextW(dc,text,-1,&caption,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        if(emoji_left+emoji_size<=rect.right){
            int y=rect.top+(rect.bottom-rect.top-emoji_size)/2;
            if(TimerEmojiLoad()){
                HDC source=CreateCompatibleDC(dc);if(source){
                    HGDIOBJ previous=SelectObject(source,g_timer_emoji.bitmap);BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
                    GdiAlphaBlend(dc,emoji_left,y,emoji_size,emoji_size,source,0,0,(int)g_timer_emoji.width,(int)g_timer_emoji.height,blend);
                    SelectObject(source,previous);DeleteDC(source);
                }
            }else{
                RECT fallback={emoji_left,rect.top,emoji_left+emoji_size,rect.bottom};
                DrawTextW(dc,L"\xD83E\xDD17",2,&fallback,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            }
        }
    }
    SelectObject(dc,old);
}
#endif
