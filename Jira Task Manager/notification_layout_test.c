#include <windows.h>
#include <stddef.h>
#include <assert.h>
static BOOL LayoutNoAccount(void){return FALSE;}
static BOOL LayoutNoUrl(wchar_t *url,size_t count);
#define JiraHasAccount LayoutNoAccount
#define JiraGetUrl LayoutNoUrl
#include "native.c"
static BOOL LayoutNoUrl(wchar_t *url,size_t count){if(count)url[0]=0;return FALSE;}
static RECT relative_rect(HWND control,HWND parent){RECT r;assert(GetWindowRect(control,&r));MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;}
static BOOL checkbox_caption_readable(HWND checkbox)
{
    RECT bounds;GetClientRect(checkbox,&bounds);HDC screen=GetDC(checkbox),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,bounds.right,bounds.bottom);HGDIOBJ old=SelectObject(dc,bitmap);
    FillRect(dc,&bounds,g_window_brush);SendMessageW(checkbox,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);
    int ink=0;
    for(int y=0;y<bounds.bottom;y++)for(int x=S(22);x<bounds.right;x++){
        COLORREF pixel=GetPixel(dc,x,y);int r=GetRValue(pixel),g=GetGValue(pixel),b=GetBValue(pixel);
        if(g_dark?(r>160&&g>160&&b>160):(r<100&&g<100&&b<100))ink++;
    }
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(checkbox,screen);return ink>S(10);
}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_russian=TRUE;g_dpi=96;RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"nly",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    WNDCLASSW wc={.lpfnWndProc=SettingsProcedure,.hInstance=g_instance,.lpszClassName=L"NotificationLayoutFixture"};assert(RegisterClassW(&wc));
    for(int dpi=96;dpi<=192;dpi+=48)for(int ru=0;ru<2;ru++)for(int dark=1;dark>=0;dark--){
        g_dpi=dpi;g_russian=ru;g_dark=dark;RecreateBrushes();HWND settings=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,30000,30000,S(430),S(360),g_window,NULL,g_instance,NULL);assert(settings);
        assert(checkbox_caption_readable(SettingsItem(214)));assert(checkbox_caption_readable(SettingsItem(224)));
        SettingsTab(2);
        assert(checkbox_caption_readable(SettingsItem(250)));assert(checkbox_caption_readable(SettingsItem(251)));assert(checkbox_caption_readable(SettingsItem(261)));
        WindowProcedure(g_window,WM_COMMAND,IDC_THEME,0);
        const int checkbox_ids[]={214,224,250,251,261};
        for(int i=0;i<_countof(checkbox_ids);i++)assert(checkbox_caption_readable(SettingsItem(checkbox_ids[i])));
        assert(!SettingsItem(254)); /* Removed explanatory caption must not reserve a row. */
        RECT check=relative_rect(SettingsItem(261),s_notifications),label=relative_rect(SettingsItem(256),s_notifications);
        assert(label.top>=check.bottom&&label.top-check.bottom<=S(16));
        HWND emails=SettingsItem(252);assert(!(GetWindowLongPtrW(emails,GWL_STYLE)&WS_VSCROLL));
        SetWindowTextW(emails,L"one@example.test\r\ntwo@example.test\r\nthree@example.test\r\nfour@example.test");
        assert(GetWindowLongPtrW(emails,GWL_STYLE)&WS_VSCROLL);
        SetWindowTextW(emails,L"one@example.test");assert(!(GetWindowLongPtrW(emails,GWL_STYLE)&WS_VSCROLL));
        SetWindowTextW(emails,L"");assert(!(GetWindowLongPtrW(emails,GWL_STYLE)&WS_VSCROLL));
        assert(!(GetWindowLongPtrW(s_notifications,GWL_STYLE)&WS_VSCROLL));
        HWND advanced=GetAncestor(SettingsItem(259),GA_ROOT);RECT advanced_client;GetClientRect(advanced,&advanced_client);
        for(int id=258;id<=262;id++)if(id!=259&&id!=261){
            HWND control=SettingsItem(id);RECT bounds=relative_rect(control,advanced);
            if(!(bounds.left>=0&&bounds.right<=advanced_client.right&&bounds.bottom<=advanced_client.bottom))fprintf(stderr,"Advanced bounds dpi=%d ru=%d id=%d rect=%ld,%ld,%ld,%ld client=%ld,%ld\n",dpi,ru,id,bounds.left,bounds.top,bounds.right,bounds.bottom,advanced_client.right,advanced_client.bottom);
            assert(bounds.left>=0&&bounds.right<=advanced_client.right&&bounds.bottom<=advanced_client.bottom);
            HDC dc=GetDC(control);HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(control,WM_GETFONT,0,0));
            wchar_t caption[256];GetWindowTextW(control,caption,256);RECT measured={0,0,bounds.right-bounds.left,0};
            DrawTextW(dc,caption,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);assert(measured.bottom<=bounds.bottom-bounds.top);
            SelectObject(dc,old);ReleaseDC(control,dc);
        }
        DestroyWindow(settings);
    }
    DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));puts("Notification layout: compact spacing, overflow-only scroll and readable checkbox captions across RU/EN, DPI and live theme changes passed");return 0;
}
