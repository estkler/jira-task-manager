#include <windows.h>
#include <assert.h>
#include <stddef.h>
/* Observe real raster output after each presentation-target write, rather than
   merely counting calls or checking the final frame. All GDI calls stay real. */
static HDC slider_observed_dc;
static POINT slider_observed_ticks[30];
static int slider_observed_count,slider_observed_frames;
static COLORREF slider_observed_color;
static BOOL slider_observed_complete;
static BOOL slider_audit_resources;
static HANDLE slider_owned_resources[32];
static int slider_owned_count,slider_owned_created;
static int slider_fail_buffer;
static void SliderTrackResource(HANDLE resource){if(resource&&slider_audit_resources){assert(slider_owned_count<32);slider_owned_resources[slider_owned_count++]=resource;slider_owned_created++;}}
static void SliderReleaseResource(HANDLE resource,BOOL released){if(released)for(int i=0;i<slider_owned_count;i++)if(slider_owned_resources[i]==resource){slider_owned_resources[i]=slider_owned_resources[--slider_owned_count];break;}}
static HDC SliderObservedCreateDC(HDC dc){if(slider_fail_buffer==1)return NULL;HDC result=CreateCompatibleDC(dc);SliderTrackResource(result);return result;}
static HBITMAP SliderObservedCreateBitmap(HDC dc,int width,int height){if(slider_fail_buffer==2)return NULL;HBITMAP result=CreateCompatibleBitmap(dc,width,height);SliderTrackResource(result);return result;}
static HBRUSH SliderObservedCreateBrush(COLORREF color){HBRUSH result=CreateSolidBrush(color);SliderTrackResource(result);return result;}
static BOOL SliderObservedDeleteDC(HDC dc){BOOL result=DeleteDC(dc);SliderReleaseResource(dc,result);return result;}
static BOOL SliderObservedDeleteObject(HGDIOBJ object){BOOL result=DeleteObject(object);SliderReleaseResource(object,result);return result;}
static void SliderObservePixels(HDC dc){
    if(!slider_observed_dc||dc!=slider_observed_dc)return;
    GdiFlush();slider_observed_frames++;
    for(int i=0;i<slider_observed_count;i++)if(GetPixel(dc,slider_observed_ticks[i].x,slider_observed_ticks[i].y)!=slider_observed_color)slider_observed_complete=FALSE;
}
static int SliderObservedFillRect(HDC dc,const RECT *rect,HBRUSH brush){int result=FillRect(dc,rect,brush);SliderObservePixels(dc);return result;}
static BOOL SliderObservedBitBlt(HDC dc,int x,int y,int w,int h,HDC source,int sx,int sy,DWORD operation){BOOL result=BitBlt(dc,x,y,w,h,source,sx,sy,operation);SliderObservePixels(dc);return result;}
static BOOL SliderNoAccount(void){return FALSE;}
static BOOL SliderNoUrl(wchar_t *url,size_t count){if(count)url[0]=0;return FALSE;}
static BOOL SliderHiddenShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
#define JiraHasAccount SliderNoAccount
#define JiraGetUrl SliderNoUrl
#define ShowWindow SliderHiddenShow
#define FillRect SliderObservedFillRect
#define BitBlt SliderObservedBitBlt
#define CreateCompatibleDC SliderObservedCreateDC
#define CreateCompatibleBitmap SliderObservedCreateBitmap
#define CreateSolidBrush SliderObservedCreateBrush
#define DeleteDC SliderObservedDeleteDC
#define DeleteObject SliderObservedDeleteObject
#include "native.c"
#undef ShowWindow
#undef FillRect
#undef BitBlt
#undef CreateCompatibleDC
#undef CreateCompatibleBitmap
#undef CreateSolidBrush
#undef DeleteDC
#undef DeleteObject
static int value(HWND w){return (int)SendMessageW(w,TBM_GETPOS,0,0);}
static RECT channel(HWND w){RECT r;SendMessageW(w,TBM_GETCHANNELRECT,0,(LPARAM)&r);assert(r.right>r.left);return r;}
static void point(HWND w,UINT message,int x,int y){SendMessageW(w,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(x,y));}
static void click(HWND w,int x,int y){point(w,WM_LBUTTONDOWN,x,y);point(w,WM_LBUTTONUP,x,y);}
static void expect_value(HWND w,int expected){
    if(value(w)!=expected)fprintf(stderr,"Slider%d pointer: got%d expected%d\n",GetDlgCtrlID(w),value(w),expected);
    assert(value(w)==expected&&"A track click must select the nearest allowed value under the pointer, not page-step");
    wchar_t text[32];GetWindowTextW(SettingsItem(GetDlgCtrlID(w)==212?226:263),text,32);
    assert(wcstol(text,NULL,10)==expected&&"Pointer selection and numeric readout must agree");
}
static void pointer_contract(HWND settings,HWND slider){
    int refresh=GetDlgCtrlID(slider)==212,minimum=refresh?15:1,maximum=refresh?45:30,initial=refresh?30:20;
    SettingsTab(refresh?0:3);RECT r=channel(slider);int span=r.right-r.left,y=(r.top+r.bottom)/2;
    SendMessageW(slider,TBM_SETPOS,TRUE,initial);
    click(slider,r.right,y);expect_value(slider,maximum);click(slider,r.left,y);expect_value(slider,minimum);
    const int refresh_values[]={15,20,25,30,35,40,45};
    for(int i=0;i<(refresh?7:30);i++){
        click(slider,r.left+MulDiv(span,i,refresh?6:29),y);expect_value(slider,refresh?refresh_values[i]:1+i);
    }
    SendMessageW(slider,TBM_SETPOS,TRUE,minimum);click(slider,r.left+MulDiv(span,3,4),y);expect_value(slider,refresh?40:23);
    point(slider,WM_LBUTTONDOWN,r.right,y);point(slider,WM_MOUSEMOVE,r.left-S(80),y);point(slider,WM_LBUTTONUP,r.left-S(80),y);expect_value(slider,minimum);assert(GetCapture()!=slider);
    point(slider,WM_LBUTTONDOWN,r.left,y);point(slider,WM_MOUSEMOVE,r.right+S(80),y);point(slider,WM_LBUTTONUP,r.right+S(80),y);expect_value(slider,maximum);assert(GetCapture()!=slider);
    point(slider,WM_LBUTTONDOWN,r.left,y);SendMessageW(slider,WM_CANCELMODE,0,0);int canceled=value(slider);
    point(slider,WM_MOUSEMOVE,r.right,y);assert(value(slider)==canceled&&GetCapture()!=slider);
    EnableWindow(slider,FALSE);click(slider,r.right,y);assert(value(slider)==canceled);EnableWindow(slider,TRUE);
    SendMessageW(slider,TBM_SETPOS,TRUE,initial);int center=r.left+MulDiv(span,initial-minimum,maximum-minimum),grab=S(2);
    point(slider,WM_LBUTTONDOWN,center+grab,y);expect_value(slider,initial);
    point(slider,WM_MOUSEMOVE,center+grab+MulDiv(span,refresh?5:1,maximum-minimum),y);
    point(slider,WM_LBUTTONUP,center+grab+MulDiv(span,refresh?5:1,maximum-minimum),y);expect_value(slider,initial+(refresh?5:1));
    point(slider,WM_LBUTTONDOWN,r.left,y);FreezeSettings(TRUE);assert(GetCapture()!=slider);canceled=value(slider);
    point(slider,WM_MOUSEMOVE,r.right,y);assert(value(slider)==canceled);FreezeSettings(FALSE);
    point(slider,WM_LBUTTONDOWN,r.left,y);SettingsTab(refresh?3:0);assert(GetCapture()!=slider);canceled=value(slider);
    point(slider,WM_MOUSEMOVE,r.right,y);assert(value(slider)==canceled);SettingsTab(refresh?0:3);
    point(slider,WM_LBUTTONDOWN,r.left,y);HWND other=GetDlgItem(settings,refresh?213:212);SetCapture(other);assert(GetCapture()!=slider);canceled=value(slider);
    point(slider,WM_MOUSEMOVE,r.right,y);assert(value(slider)==canceled);ReleaseCapture();
    point(slider,WM_LBUTTONDOWN,r.left,y);SetFocus(GetDlgItem(settings,IDOK));assert(GetCapture()!=slider);
    SendMessageW(slider,WM_KEYDOWN,VK_HOME,0);expect_value(slider,minimum);
    SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);expect_value(slider,refresh?20:2);
    SendMessageW(slider,WM_KEYDOWN,VK_END,0);expect_value(slider,maximum);
    assert(g_refresh_minutes==30&&g_tray_task_count==20&&GetPrivateProfileIntW(L"Jira",L"RefreshMinutes",0,g_settings_path)==30);
    SendMessageW(slider,TBM_SETPOS,TRUE,initial);SendMessageW(settings,WM_HSCROLL,TB_THUMBPOSITION,(LPARAM)slider);
    ValidateRect(slider,NULL);SendMessageW(slider,TBM_SETPOS,FALSE,minimum);
    assert(value(slider)==minimum&&!GetUpdateRect(slider,NULL,FALSE)&&"TBM_SETPOS(FALSE) must change the native value without requesting a repaint");
    SendMessageW(slider,TBM_SETPOS,TRUE,initial);SendMessageW(settings,WM_HSCROLL,TB_THUMBPOSITION,(LPARAM)slider);
}
static void paint_contract(HWND slider){
    RECT client;GetClientRect(slider,&client);HDC target=GetDC(slider),dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
    HBRUSH sentinel=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&client,sentinel);DeleteObject(sentinel);
    SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    COLORREF corner=GetPixel(dc,1,1);
    if(corner!=WindowColor())fprintf(stderr,"Slider%d dark%d corner=%06lx background=%06lx\n",GetDlgCtrlID(slider),g_dark,(unsigned long)corner,(unsigned long)WindowColor());
    assert(corner==WindowColor()&&GetPixel(dc,client.right-2,client.bottom-2)==WindowColor()&&"Slider must paint its own themed background, without a white rectangle");
    RECT r=channel(slider);int minimum=(int)SendMessageW(slider,TBM_GETRANGEMIN,0,0),maximum=(int)SendMessageW(slider,TBM_GETRANGEMAX,0,0);
    int x=r.left+MulDiv(r.right-r.left,value(slider)-minimum,maximum-minimum),y=(r.top+r.bottom)/2;
    assert(GetPixel(dc,x,y)==(IsWindowEnabled(slider)?AccentColor():MutedColor()));
    {
        int marks=GetDlgCtrlID(slider)==212?7:30;
        int tick_y=y+max(3,S(5))+S(5);
        COLORREF tick=IsWindowEnabled(slider)?MutedColor():(g_dark?RGB(76,80,86):RGB(210,215,222));
        for(int i=0;i<marks;i++){
            int tick_x=r.left+MulDiv(r.right-r.left,i,marks-1);
            assert(tick_y+S(2)<client.bottom&&"All marks must fit inside the control");
            assert(GetPixel(dc,tick_x,tick_y+S(1))==tick&&"Every selectable value must have an aligned, themed tick");
        }
        assert(GetPixel(dc,r.left+MulDiv(r.right-r.left,1,2*(marks-1)),tick_y+S(1))==WindowColor()&&"Ticks must stay separate, not become a second rail");
    }
    FillRect(dc,&client,g_panel_brush);SendMessageW(slider,WM_PAINT,(WPARAM)dc,0);assert(GetPixel(dc,1,1)==WindowColor());
    /* Audit actual slider-owned handles: process-wide counts include unrelated
       asynchronous native font/bitmap cache growth and are not a leak oracle. */
    assert(!slider_owned_count);slider_owned_created=0;slider_audit_resources=TRUE;
    for(int i=0;i<50;i++)SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    slider_audit_resources=FALSE;GdiFlush();
    assert(slider_owned_created>0&&!slider_owned_count&&"Every real slider brush, bitmap and DC must be successfully released");
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(slider,target);
}
static void atomic_frame_contract(HWND slider){
    RECT client;GetClientRect(slider,&client);HDC target=GetDC(slider),dc=CreateCompatibleDC(target);
    HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);assert(dc&&bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    RECT r=channel(slider);int minimum=(int)SendMessageW(slider,TBM_GETRANGEMIN,0,0),maximum=(int)SendMessageW(slider,TBM_GETRANGEMAX,0,0);
    SendMessageW(slider,TBM_SETPOS,TRUE,minimum);SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    slider_observed_count=GetDlgCtrlID(slider)==212?7:30;
    for(int i=0;i<slider_observed_count;i++)slider_observed_ticks[i]=(POINT){r.left+MulDiv(r.right-r.left,i,slider_observed_count-1),(r.top+r.bottom)/2+max(3,S(5))+S(6)};
    slider_observed_color=MutedColor();slider_observed_complete=TRUE;slider_observed_frames=0;
    SendMessageW(slider,TBM_SETPOS,TRUE,maximum);slider_observed_dc=dc;
    SendMessageW(slider,WM_PAINT,(WPARAM)dc,0);slider_observed_dc=NULL;
    assert(slider_observed_frames>0&&"Observe actual presentation output, not an unexercised mock");
    assert(slider_observed_complete&&"No visible intermediate repaint may erase all or part of the slider tick scale");
    for(int failure=1;failure<=2;failure++){
        HBRUSH sentinel=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&client,sentinel);DeleteObject(sentinel);
        slider_fail_buffer=failure;slider_audit_resources=TRUE;
        SendMessageW(slider,WM_PAINT,(WPARAM)dc,0);
        slider_audit_resources=FALSE;slider_fail_buffer=0;GdiFlush();
        assert(!slider_owned_count&&"Allocation failure must release any successfully allocated partial buffer");
        int thumb_x=r.left+MulDiv(r.right-r.left,value(slider)-minimum,maximum-minimum),thumb_y=(r.top+r.bottom)/2;
        assert(GetPixel(dc,1,1)==WindowColor()&&GetPixel(dc,thumb_x,thumb_y)==AccentColor()&&"Fallback must repaint background and current thumb over a poisoned target");
        for(int i=0;i<slider_observed_count;i++)assert(GetPixel(dc,slider_observed_ticks[i].x,slider_observed_ticks[i].y)==slider_observed_color&&"Low-memory fallback must still draw the complete usable control");
    }
    SendMessageW(slider,TBM_SETPOS,TRUE,GetDlgCtrlID(slider)==212?30:20);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(slider,target);
}
/* Repaint only the OS damage region over the old frame, then compare with a
   fresh full frame. Full-frame-only tests cannot catch stale thumb/halo pixels. */
static LRESULT CALLBACK record_damage(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    (void)id;
    if(message==WM_PAINT&&!wp){HRGN part=CreateRectRgn(0,0,0,0);GetUpdateRgn(window,part,FALSE);CombineRgn((HRGN)data,(HRGN)data,part,RGN_OR);DeleteObject(part);}
    return DefSubclassProc(window,message,wp,lp);
}
static void trail_contract(HWND settings,HWND slider){
    SettingsTab(GetDlgCtrlID(slider)==212?0:3);ShowWindow(settings,SW_SHOWNOACTIVATE);
    RECT client;GetClientRect(slider,&client);int width=client.right,height=client.bottom;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC a=CreateCompatibleDC(NULL),b=CreateCompatibleDC(NULL);void *ap,*bp;
    HBITMAP ab=CreateDIBSection(a,&info,DIB_RGB_COLORS,&ap,NULL,0),bb=CreateDIBSection(b,&info,DIB_RGB_COLORS,&bp,NULL,0);assert(ab&&bb);
    HGDIOBJ olda=SelectObject(a,ab),oldb=SelectObject(b,bb);HRGN damage=CreateRectRgn(0,0,0,0),pending=CreateRectRgn(0,0,0,0);
    assert(SetWindowSubclass(slider,record_damage,2,(DWORD_PTR)damage));
    int low=(int)SendMessageW(slider,TBM_GETRANGEMIN,0,0),high=(int)SendMessageW(slider,TBM_GETRANGEMAX,0,0);
    RECT r=channel(slider);int y=(r.top+r.bottom)/2;
    SetFocus(slider);SendMessageW(slider,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
    SendMessageW(slider,TBM_SETPOS,TRUE,(low+high)/2);
    SendMessageW(slider,WM_MOUSEMOVE,0,MAKELPARAM(r.left,y));
    for(int action=0;action<4;action++){
        if(action==1)point(slider,WM_LBUTTONDOWN,r.right,y);
        SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)a,PRF_CLIENT);ValidateRect(slider,NULL);SetRectRgn(damage,0,0,0,0);
        if(action==0)SendMessageW(slider,TBM_SETPOS,TRUE,high);
        else if(action==1)point(slider,WM_MOUSEMOVE,r.left,y);
        else if(action==2){point(slider,WM_LBUTTONUP,r.left,y);SendMessageW(slider,WM_KEYDOWN,VK_END,0);}
        else SendMessageW(slider,WM_KEYDOWN,VK_HOME,0);
        assert(GetUpdateRgn(slider,pending,FALSE)!=ERROR);CombineRgn(damage,damage,pending,RGN_OR);
        SelectClipRgn(a,damage);SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)a,PRF_CLIENT);SelectClipRgn(a,NULL);
        SendMessageW(slider,WM_PRINTCLIENT,(WPARAM)b,PRF_CLIENT);GdiFlush();
        if(memcmp(ap,bp,(size_t)width*height*4)){
            RECT dirty;GetRgnBox(damage,&dirty);fprintf(stderr,"Slider%d trail action%d damage%ld,%ld..%ld,%ld client%d,%d\n",GetDlgCtrlID(slider),action,dirty.left,dirty.top,dirty.right,dirty.bottom,width,height);
        }
        assert(!memcmp(ap,bp,(size_t)width*height*4)&&"Partial invalidation must clear the entire old thumb, focus halo and selected rail without trails");
        UpdateWindow(slider);assert(!GetUpdateRect(slider,NULL,FALSE)&&"Real BeginPaint/EndPaint must consume the dirty region");
    }
    RemoveWindowSubclass(slider,record_damage,2);SetFocus(GetDlgItem(settings,IDCANCEL));DeleteObject(damage);DeleteObject(pending);SelectObject(a,olda);SelectObject(b,oldb);DeleteObject(ab);DeleteObject(bb);DeleteDC(a);DeleteDC(b);
    ShowWindow(settings,SW_HIDE);
    SendMessageW(slider,TBM_SETPOS,TRUE,GetDlgCtrlID(slider)==212?30:20);SendMessageW(settings,WM_HSCROLL,TB_THUMBPOSITION,(LPARAM)slider);
}
int main(int argc,char **argv){
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temporary[MAX_PATH];GetTempPathW(MAX_PATH,temporary);assert(GetTempFileNameW(temporary,L"slt",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"SettingsSliderFixture"};assert(RegisterClassW(&cls));
    /* Finish isolated paint/resource checks before any native popup is shown. */
    for(int phase=0;phase<2;phase++)for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=(argc>1&&!strcmp(argv[1],"paint"))?1:0;dark<2;dark++)for(int density=0;density<2;density++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;g_density=density;g_refresh_minutes=30;g_tray_task_count=20;RecreateBrushes();
        assert(WritePrivateProfileStringW(L"Jira",L"RefreshMinutes",L"30",g_settings_path));
        HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(SETTINGS_WIDTH_DIP),S(SETTINGS_HEIGHT_DIP),g_window,NULL,g_instance,NULL);assert(settings);
        if(!phase)for(int id=212;id<=213;id++){
            HWND slider=GetDlgItem(settings,id);wchar_t name[64];GetClassNameW(slider,name,64);assert(!wcscmp(name,TRACKBAR_CLASSW));
            if(argc>1&&!strcmp(argv[1],"trail"))continue;
            if(argc<2||strcmp(argv[1],"paint"))pointer_contract(settings,slider);
            SettingsTab(id==212?0:3);paint_contract(slider);EnableWindow(slider,FALSE);paint_contract(slider);EnableWindow(slider,TRUE);
            atomic_frame_contract(slider);
        }
        if(phase)for(int id=212;id<=213;id++)trail_contract(settings,GetDlgItem(settings,id));
        HWND captured=GetDlgItem(settings,213);SettingsTab(3);RECT last=channel(captured);point(captured,WM_LBUTTONDOWN,last.left,(last.top+last.bottom)/2);
        DestroyWindow(settings);assert(GetCapture()!=captured&&!s_slider_drag&&!s_slider_hot);
    }
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));puts("Settings sliders: direct pointer selection, discrete steps, clamped drag, cancel/disabled capture, native keys, themed pixels and RU/EN/DPI/density passed");return 0;
}
