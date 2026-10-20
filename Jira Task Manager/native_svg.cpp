#define WIN32_LEAN_AND_MEAN
#include "native_svg.h"
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "native_svg_css.h"

static bool Named(const wchar_t *name,const wchar_t *const *names,size_t count)
{for(size_t i=0;i<count;i++)if(!wcscmp(name,names[i]))return true;return false;}
static bool LocalPaint(const wchar_t *value)
{
    /* Values are XML-decoded. Limit URL references to this document's identifiers. */
    if(wcschr(value,L'@')||wcschr(value,L'\\'))return false;
    for(const wchar_t *at=value;*at;at++)if(!_wcsnicmp(at,L"url",3)){
        at+=3;while(*at==L' '||*at==L'\t')at++;
        if(*at++!=L'(')return false;while(*at==L' '||*at==L'\t')at++;
        wchar_t quote=(*at==L'\''||*at==L'\"')?*at++:0;if(*at++!=L'#')return false;
        unsigned count=0;while((*at>=L'a'&&*at<=L'z')||(*at>=L'A'&&*at<=L'Z')||(*at>=L'0'&&*at<=L'9')||*at==L'_'||*at==L'-'){at++;count++;}
        if(!count||count>128)return false;if(quote&&*at++!=quote)return false;
        while(*at==L' '||*at==L'\t')at++;if(*at!=L')')return false;
    }
    return true;
}
static bool Length(const wchar_t *value,double *length)
{
    wchar_t *end=NULL;double number=wcstod(value,&end);
    if(end==value||!isfinite(number)||number<=0||number>8192)return false;
    while(*end==L' ')end++;if(*end&&wcscmp(end,L"px")&&wcscmp(end,L"%"))return false;
    *length=!wcscmp(end,L"%")?0:number;return true;
}
static BOOL SvgValidatedDimensions(IStream *stream,UINT *width,UINT *height)
{
    if(!stream||!width||!height)return FALSE;
    IXmlReader *reader=NULL;HRESULT hr=CreateXmlReader(__uuidof(IXmlReader),(void**)&reader,NULL);
    if(SUCCEEDED(hr))hr=reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit);
    if(SUCCEEDED(hr))hr=reader->SetProperty(XmlReaderProperty_MaxElementDepth,32);
    if(SUCCEEDED(hr))hr=reader->SetInput(stream);
    const wchar_t *const elements[]={L"svg",L"g",L"path",L"rect",L"circle",L"ellipse",L"line",L"polyline",L"polygon",L"defs",L"linearGradient",L"radialGradient",L"stop",L"clipPath",L"mask",L"title",L"desc"};
    const wchar_t *const attributes[]={L"width",L"height",L"viewBox",L"preserveAspectRatio",L"version",L"id",L"color",L"fill",L"fill-rule",L"fill-opacity",L"stroke",L"stroke-width",L"stroke-opacity",L"stroke-linecap",L"stroke-linejoin",L"stroke-miterlimit",L"stroke-dasharray",L"stroke-dashoffset",L"opacity",L"d",L"x",L"y",L"x1",L"y1",L"x2",L"y2",L"cx",L"cy",L"r",L"rx",L"ry",L"points",L"transform",L"gradientUnits",L"gradientTransform",L"offset",L"stop-color",L"stop-opacity",L"clip-path",L"clip-rule",L"clipPathUnits",L"mask",L"maskUnits",L"maskContentUnits",L"fx",L"fy",L"spreadMethod",L"style"};
    bool valid=SUCCEEDED(hr),root=false;unsigned nodes=0;double source_w=0,source_h=0,view_w=0,view_h=0;
    XmlNodeType type=XmlNodeType_None;
    while(valid&&(hr=reader->Read(&type))==S_OK){
        if(++nodes>2048){valid=false;break;}
        if(type==XmlNodeType_DocumentType||type==XmlNodeType_ProcessingInstruction){valid=false;break;}
        if(type!=XmlNodeType_Element)continue;
        const wchar_t *name=NULL,*space=NULL;UINT count=0;
        if(FAILED(reader->GetLocalName(&name,&count))||FAILED(reader->GetNamespaceUri(&space,NULL))||
            wcscmp(space,L"http://www.w3.org/2000/svg")||!Named(name,elements,_countof(elements))){valid=false;break;}
        bool first=!root;if(first){if(wcscmp(name,L"svg")){valid=false;break;}root=true;}
        UINT depth=0;reader->GetDepth(&depth);if(!first&&!wcscmp(name,L"svg")){valid=false;break;}
        HRESULT attribute=reader->MoveToFirstAttribute();
        while(valid&&attribute==S_OK){
            const wchar_t *value=NULL,*attribute_space=NULL;UINT value_length=0;
            if(FAILED(reader->GetLocalName(&name,NULL))||FAILED(reader->GetNamespaceUri(&attribute_space,NULL))||FAILED(reader->GetValue(&value,&value_length))||value_length>32768){valid=false;break;}
            if(!wcscmp(attribute_space,L"http://www.w3.org/2000/xmlns/")){
                /* Namespace declarations contain URIs, but are never resource requests. */
            }else if(*attribute_space||!Named(name,attributes,_countof(attributes))||!LocalPaint(value)){valid=false;break;}
            else if(first&&(!wcscmp(name,L"width")||!wcscmp(name,L"height"))){
                if(!Length(value,!wcscmp(name,L"width")?&source_w:&source_h)){valid=false;break;}
            }else if(first&&!wcscmp(name,L"viewBox")){
                wchar_t *end=NULL;const wchar_t *at=value;double numbers[4]={0};
                for(int i=0;i<4;i++){while(*at==L' '||*at==L'\t'||*at==L'\r'||*at==L'\n'||*at==L',')at++;numbers[i]=wcstod(at,&end);if(end==at||!isfinite(numbers[i])||fabs(numbers[i])>8192){valid=false;break;}at=end;}
                while(*at==L' '||*at==L'\t'||*at==L'\r'||*at==L'\n')at++;
                if(*at||numbers[2]<=0||numbers[3]<=0)valid=false;view_w=numbers[2];view_h=numbers[3];
            }
            attribute=reader->MoveToNextAttribute();
        }
        if(FAILED(attribute))valid=false;reader->MoveToElement();
    }
    if(FAILED(hr)||!root)valid=false;
    if(!source_w)source_w=view_w?view_w:48;if(!source_h)source_h=view_h?view_h:48;
    if(source_w*source_h>12u*1024u*1024u)valid=false;
    if(valid){*width=(UINT)ceil(source_w);*height=(UINT)ceil(source_h);}
    if(reader)reader->Release();return valid;
}
extern "C" BOOL NativeSvgDimensions(const unsigned char *bytes,size_t length,UINT *width,UINT *height)
{IStream *stream=NativeSvgNormalizedStream(bytes,length);if(!stream)return FALSE;BOOL valid=SvgValidatedDimensions(stream,width,height);stream->Release();return valid;}
extern "C" BOOL NativeSvgRasterize(const unsigned char *bytes,size_t length,UINT source_width,UINT source_height,UINT width,UINT height,void *pixels)
{
    if(!pixels||!width||!height||width>1920||height>1440)return FALSE;
    ID3D11Device *d3d=NULL;IDXGIDevice *dxgi=NULL;ID2D1Factory1 *factory=NULL;ID2D1Device *device=NULL;ID2D1DeviceContext *base=NULL;
    ID2D1DeviceContext5 *context=NULL;ID2D1Bitmap1 *target=NULL,*readback=NULL;ID2D1SvgDocument *svg=NULL;
    IStream *stream=NativeSvgNormalizedStream(bytes,length);HRESULT hr=stream?S_OK:E_INVALIDARG;
    if(SUCCEEDED(hr))hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,NULL,0,D3D11_SDK_VERSION,&d3d,NULL,NULL);
    if(SUCCEEDED(hr))hr=d3d->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);
    if(SUCCEEDED(hr))hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),NULL,(void**)&factory);
    if(SUCCEEDED(hr))hr=factory->CreateDevice(dxgi,&device);
    if(SUCCEEDED(hr))hr=device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&base);
    if(SUCCEEDED(hr))hr=base->QueryInterface(__uuidof(ID2D1DeviceContext5),(void**)&context);
    D2D1_SIZE_U size={width,height};D2D1_BITMAP_PROPERTIES1 properties={};
    properties.pixelFormat.format=DXGI_FORMAT_B8G8R8A8_UNORM;properties.pixelFormat.alphaMode=D2D1_ALPHA_MODE_PREMULTIPLIED;
    properties.dpiX=properties.dpiY=96;properties.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET;
    if(SUCCEEDED(hr))hr=context->CreateBitmap(size,NULL,0,&properties,&target);
    properties.bitmapOptions=(D2D1_BITMAP_OPTIONS)(D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW);
    if(SUCCEEDED(hr))hr=context->CreateBitmap(size,NULL,0,&properties,&readback);
    D2D1_SIZE_F viewport={(float)source_width,(float)source_height};
    if(SUCCEEDED(hr))hr=context->CreateSvgDocument(stream,viewport,&svg);
    if(SUCCEEDED(hr)){
        context->SetTarget(target);context->SetDpi(96,96);D2D1_MATRIX_3X2_F scale={};scale._11=(float)width/source_width;scale._22=(float)height/source_height;context->SetTransform(&scale);
        context->BeginDraw();D2D1_COLOR_F transparent={0,0,0,0};context->Clear(&transparent);context->DrawSvgDocument(svg);hr=context->EndDraw();context->SetTarget(NULL);
    }
    if(SUCCEEDED(hr))hr=readback->CopyFromBitmap(NULL,target,NULL);
    D2D1_MAPPED_RECT mapped={};if(SUCCEEDED(hr))hr=readback->Map(D2D1_MAP_OPTIONS_READ,&mapped);
    if(SUCCEEDED(hr)){
        for(UINT y=0;y<height;y++){
            BYTE *row=(BYTE*)pixels+(size_t)y*width*4;memcpy(row,mapped.bits+(size_t)y*mapped.pitch,(size_t)width*4);
            for(UINT x=0;x<width;x++){BYTE *pixel=row+x*4;unsigned alpha=pixel[3];if(alpha)for(int c=0;c<3;c++){unsigned value=(pixel[c]*255u+alpha/2)/alpha;pixel[c]=(BYTE)(value>255?255:value);}}
        }
        readback->Unmap();
    }
    if(svg)svg->Release();if(readback)readback->Release();if(target)target->Release();if(context)context->Release();if(base)base->Release();if(device)device->Release();if(factory)factory->Release();if(dxgi)dxgi->Release();if(d3d)d3d->Release();if(stream)stream->Release();return SUCCEEDED(hr);
}
