#include <windows.h>
#include <assert.h>
static BOOL AboutNoAccount(void){return FALSE;}
static BOOL AboutNoUrl(wchar_t *url,size_t capacity){if(capacity)url[0]=0;return FALSE;}
static BOOL AboutHiddenShow(HWND window,int command){return (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)?ShowWindow(window,command):FALSE;}
#define JiraHasAccount AboutNoAccount
#define JiraGetUrl AboutNoUrl
#define ShowWindow AboutHiddenShow
#include "native.c"
#undef ShowWindow

static BOOL shown(int id){return (GetWindowLongPtrW(SettingsItem(id),GWL_STYLE)&WS_VISIBLE)!=0;}
static RECT bounds(int id){RECT r;assert(GetWindowRect(SettingsItem(id),&r));MapWindowPoints(NULL,s_window,(POINT*)&r,2);return r;}
static void text_fits(int id,BOOL multiline){
    HWND item=SettingsItem(id);RECT r;assert(GetClientRect(item,&r));
    wchar_t text[512];GetWindowTextW(item,text,512);HDC dc=GetDC(item);
    HGDIOBJ font=SelectObject(dc,(HFONT)SendMessageW(item,WM_GETFONT,0,0));
    RECT measured={0,0,r.right,0};DrawTextW(dc,text,-1,&measured,DT_CALCRECT|DT_NOPREFIX|(multiline?DT_WORDBREAK:DT_SINGLELINE));
    if(measured.right>r.right||measured.bottom>r.bottom)fprintf(stderr,"About text clipped: id=%d dpi=%u ru=%d text=%ls\n",id,g_dpi,g_russian,text);
    assert(measured.right<=r.right&&measured.bottom<=r.bottom);
    SelectObject(dc,font);ReleaseDC(item,dc);
}
static SIZE text_size(int id){wchar_t text[512];GetWindowTextW(SettingsItem(id),text,512);HDC dc=GetDC(SettingsItem(id));HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(SettingsItem(id),WM_GETFONT,0,0));SIZE size;assert(GetTextExtentPoint32W(dc,text,(int)wcslen(text),&size));SelectObject(dc,old);ReleaseDC(SettingsItem(id),dc);return size;}
static void compact_spacing(void){
    RECT title=bounds(230),version=bounds(268),status=bounds(235),action=bounds(242),contact=bounds(238),diagnostics=bounds(234);
    SIZE name=text_size(230),label=text_size(242);
    assert(version.left-title.left-name.cx>=S(6)&&version.left-title.left-name.cx<=S(12)&&"Version must follow the rendered title rather than a fixed empty column");
    HWND item=SettingsItem(235);wchar_t text[512];GetWindowTextW(item,text,512);HDC dc=GetDC(item);HGDIOBJ old=SelectObject(dc,s_small);RECT measured={0,0,status.right-status.left,0};DrawTextW(dc,text,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(item,dc);
    assert(action.top-status.top-measured.bottom>=S(8)&&action.top-status.top-measured.bottom<=S(14)&&"Update action must follow actual status text closely");
    assert(action.right-action.left<=label.cx+S(28)&&"Primary action must not retain an oversized fixed width");
    assert(diagnostics.left-contact.right>=S(16)&&diagnostics.left-contact.right<=S(24));
}
static DWORD *link_pixels(HWND settings,int id,UINT state,BOOL hover,int *width,int *height){
    HWND item=SettingsItem(id);RECT r;GetClientRect(item,&r);*width=r.right;*height=r.bottom;
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=*width;info.bmiHeader.biHeight=-*height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void *bits;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    s_hover=hover?item:NULL;DRAWITEMSTRUCT draw={.CtlType=ODT_BUTTON,.CtlID=id,.itemAction=ODA_DRAWENTIRE,.itemState=state,.hwndItem=item,.hDC=dc,.rcItem=r};
    assert(SendMessageW(settings,WM_DRAWITEM,id,(LPARAM)&draw));GdiFlush();size_t bytes=(size_t)*width**height*4;DWORD *copy=malloc(bytes);assert(copy);memcpy(copy,bits,bytes);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return copy;
}
static void link_feedback(HWND settings){
    int ids[]={238,234};for(int i=0;i<2;i++){
        HWND item=SettingsItem(ids[i]);
        SendMessageW(settings,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
        SendMessageW(item,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(1,1));
        int input_w,input_h;DWORD *mouse_input=link_pixels(settings,ids[i],ODS_FOCUS,FALSE,&input_w,&input_h),*neutral=link_pixels(settings,ids[i],0,FALSE,&input_w,&input_h);
        if(memcmp(mouse_input,neutral,(size_t)input_w*input_h*4))fprintf(stderr,"Mouse flag retained=%d id%d\n",GetPropW(item,L"Tasklet.Settings.MouseFocus")!=NULL,ids[i]);
        assert(!memcmp(mouse_input,neutral,(size_t)input_w*input_h*4)&&"Real mouse-down must keep focus decoration off the link");free(mouse_input);free(neutral);
        SendMessageW(item,WM_CANCELMODE,0,0);SendMessageW(item,BM_SETSTATE,FALSE,0);if(GetCapture()==item)ReleaseCapture();
        SendMessageW(item,WM_KEYDOWN,VK_SPACE,0);
        SendMessageW(item,WM_CANCELMODE,0,0);SendMessageW(item,BM_SETSTATE,FALSE,0);if(GetCapture()==item)ReleaseCapture();
        int w,h;DWORD *idle=link_pixels(settings,ids[i],0,FALSE,&w,&h),*hover=link_pixels(settings,ids[i],0,TRUE,&w,&h);
        assert(memcmp(idle,hover,(size_t)w*h*4)&&"Both support links must react visibly to hover");
        DWORD *mouse_focus=link_pixels(settings,ids[i],ODS_FOCUS|ODS_NOFOCUSRECT,FALSE,&w,&h);
        assert(!memcmp(idle,mouse_focus,(size_t)w*h*4)&&"Mouse focus must not paint a rectangular focus box");
        DWORD *pressed=link_pixels(settings,ids[i],ODS_SELECTED,TRUE,&w,&h);assert(memcmp(hover,pressed,(size_t)w*h*4));
        DWORD *keyboard=link_pixels(settings,ids[i],ODS_FOCUS,FALSE,&w,&h);assert(memcmp(idle,keyboard,(size_t)w*h*4)&&"Keyboard focus must remain visible");
        int text_width=text_size(ids[i]).cx;
        for(int y=0;y<h;y++)for(int x=text_width+1;x<w;x++)assert(keyboard[y*w+x]==idle[y*w+x]&&"Keyboard emphasis belongs to the link text, not a rectangular border");
        free(idle);free(hover);free(mouse_focus);free(pressed);free(keyboard);
    }s_hover=NULL;
}
static void capture_native_about(HWND settings,const char *prefix){
    HWND requested_hover=s_hover;ShowWindow(settings,SW_SHOWNOACTIVATE);UpdateWindow(settings);SetFocus(NULL);
    SendMessageW(SettingsItem(238),WM_KILLFOCUS,0,0);SendMessageW(SettingsItem(234),WM_KILLFOCUS,0,0);
    SendMessageW(SettingsItem(238),WM_UPDATEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);SendMessageW(SettingsItem(234),WM_UPDATEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
    SendMessageW(settings,WM_UPDATEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);s_hover=requested_hover;
    RECT r;assert(GetClientRect(settings,&r));HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void *bits;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    /* Render the real native controls, not an HTML recreation. No user profile or Jira writes. */
    SendMessageW(settings,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);GdiFlush();
    assert(GetPixel(dc,0,0)==WindowColor()&&"Native settings capture must contain the real painted background");
    assert(GetPixel(dc,S(14),S(44))==(g_dark?RGB(65,65,68):RGB(210,210,210))&&"Native capture must include the real panel frame");
    char path[1024];snprintf(path,sizeof(path),"%s-%s%s.bmp",prefix,g_dark?"dark":"light",s_hover?"-hover":"");FILE *file=fopen(path,"wb");assert(file);
    BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+r.right*r.bottom*4;
    assert(fwrite(&header,sizeof(header),1,file)==1&&fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1&&fwrite(bits,(size_t)r.right*r.bottom*4,1,file)==1);fclose(file);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ShowWindow(settings,SW_HIDE);
}
int main(int argc,char **argv){
    g_instance=GetModuleHandleW(NULL);g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"abt",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW cls={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"AboutSettingsFixture"};assert(RegisterClassW(&cls));
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_russian=ru;g_dark=dark;RecreateBrushes();
        HWND settings=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(417),g_window,NULL,g_instance,NULL);assert(settings);
        SettingsTab(1);UpdateSettingsLanguage();
        link_feedback(settings);compact_spacing();
        assert(!shown(267)&&!shown(241)&&"About must hide Defaults and its duplicate footer version");
        wchar_t text[256];GetWindowTextW(SettingsItem(230),text,256);assert(!wcscmp(text,L"Jira Tasklet"));
        GetWindowTextW(SettingsItem(268),text,256);assert(!wcscmp(text,APP_VERSION));
        GetWindowTextW(SettingsItem(238),text,256);assert(!wcscmp(text,ru?L"Сообщить о проблеме ↗":L"Report a problem ↗"));
        const int ids[]={230,268,231,232,236,238,234,IDOK,IDCANCEL};
        for(int i=0;i<_countof(ids);i++){assert(shown(ids[i]));text_fits(ids[i],FALSE);}
        RECT contact=bounds(238),diagnostics=bounds(234),footer=bounds(IDOK);
        assert(contact.top==diagnostics.top&&contact.right<diagnostics.left&&contact.bottom<footer.top);
        assert(bounds(230).right<bounds(268).left&&bounds(232).top>bounds(231).bottom);
        const int keyboard[]={201,242,238,234,IDCANCEL,IDOK};
        for(int i=1;i<_countof(keyboard);i++)assert(GetNextDlgTabItem(settings,SettingsItem(keyboard[i-1]),FALSE)==SettingsItem(keyboard[i]));
        for(int state=UPDATE_UI_IDLE;state<=UPDATE_UI_ERROR;state++){
            u_state=state;u_reason[0]=0;u_error=UPDATE_NETWORK;UpdateUiRender(settings);
            compact_spacing();
            text_fits(235,TRUE);text_fits(242,FALSE);
            assert(bounds(235).bottom<=bounds(242).top&&bounds(242).bottom<bounds(236).top);
            assert(IsWindowEnabled(SettingsItem(242))==!UpdateUiBusy());
        }
        u_state=UPDATE_UI_ERROR;wcscpy_s(u_reason,256,ru?L"Не удалось сохранить состояние. Обновление отменено.":L"Save notes and send or clear comment drafts before installing the update.");
        UpdateUiRender(settings);text_fits(235,TRUE);
        u_state=UPDATE_UI_IDLE;u_reason[0]=0;UpdateUiRender(settings);
        if(argc>1&&dpi==144&&ru==1){
            if(argc>2&&!strcmp(argv[2],"jira")){
                SettingsTab(0);capture_native_about(settings,argv[1]);SettingsTab(1);
            }else{capture_native_about(settings,argv[1]);s_hover=SettingsItem(238);capture_native_about(settings,argv[1]);s_hover=NULL;}
        }
        /* Cross-page drafts and the normal footer survive visiting About. */
        SendMessageW(s_interval,TBM_SETPOS,TRUE,25);SettingsTab(3);assert(shown(267)&&!SettingsItem(241));
        SettingsTab(1);assert(!shown(267)&&!shown(241)&&SettingsRefreshMinutes()==25);
        DestroyWindow(settings);
    }
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));puts("Compact About: visibility, keyboard order, updater states, drafts, RU/EN, DPI and themes passed");return 0;
}
