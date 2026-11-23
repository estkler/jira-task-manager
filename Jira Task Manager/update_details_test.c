/* Native controls with external discovery/download kept offline by fixture. */
#define main AboutLayoutMainUnused
#include "about_settings_test.c"
#undef main
static void CaptureUpdateDetails(const wchar_t *path){
    RECT r;GetClientRect(u_details,&r);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);void *pixels;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    UpdateDetailsDraw(u_details,dc);
    for(HWND child=GetWindow(u_details,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT q;GetWindowRect(child,&q);MapWindowPoints(NULL,u_details,(POINT*)&q,2);int saved=SaveDC(dc);SetViewportOrgEx(dc,q.left,q.top,NULL);
        IntersectClipRect(dc,0,0,q.right-q.left,q.bottom-q.top);SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);
    }GdiFlush();
    assert(GetPixel(dc,0,(r.bottom-UD(74)))==WindowColor()&&"Child background printing must preserve the parent progress clipping");
    BITMAPFILEHEADER header={.bfType=0x4d42,.bfOffBits=sizeof(header)+sizeof(info.bmiHeader)};DWORD bytes=r.right*r.bottom*4,wrote;header.bfSize=header.bfOffBits+bytes;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(file!=INVALID_HANDLE_VALUE);assert(WriteFile(file,&header,sizeof(header),&wrote,NULL)&&WriteFile(file,&info.bmiHeader,sizeof(info.bmiHeader),&wrote,NULL)&&WriteFile(file,pixels,bytes,&wrote,NULL));CloseHandle(file);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
int main(int argc,char **argv){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);g_instance=GetModuleHandleW(NULL);g_dpi=96;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,740,550,NULL,NULL,g_instance,NULL);CreateControls();
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"uds",0,g_settings_path);
    OpenSettingsWindow();u_release=(UpdateRelease){.version={0,9,99},.asset_id=42,.size=1000};wcscpy_s(u_notes,UPDATE_NOTES_CAP,L"## Improvements\n- First change\n- Second change");u_state=UPDATE_UI_AVAILABLE;
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int ru=0;ru<2;ru++){
        wcscpy_s(u_notes,UPDATE_NOTES_CAP,L"## Improvements\n- First change\n- Second change");g_dpi=dpi;g_dark=dark;g_russian=ru;UpdateFonts();RecreateBrushes();UpdateDetailsOpen(s_window);assert(u_details);
        wchar_t text[256];GetWindowTextW(GetDlgItem(u_details,402),text,256);assert(wcsstr(text,L"First change")&&wcsstr(text,L"\r\n")&&!wcsstr(text,L"##"));
        assert(IsWindowEnabled(GetDlgItem(u_details,IDOK))&&"Release notes must appear before download begins");assert(!u_work);
        HFONT notes_font=(HFONT)SendMessageW(GetDlgItem(u_details,402),WM_GETFONT,0,0);LOGFONTW details_font;
        UpdateFonts();assert(GetObjectW(notes_font,sizeof(details_font),&details_font)&&"Changing main-window fonts must not delete fonts still used by update details");
        RECT sizing;GetWindowRect(u_details,&sizing);SendMessageW(u_details,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),(LPARAM)&sizing);
        HFONT resized=(HFONT)SendMessageW(GetDlgItem(u_details,402),WM_GETFONT,0,0);assert(GetObjectW(resized,sizeof(details_font),&details_font)&&details_font.lfHeight<0);
        UpdateWork *work=calloc(1,sizeof(*work));work->references=2;work->generation=++u_generation;work->action=1;work->release=u_release;work->received=450;work->owner=g_window;u_work=work;u_state=UPDATE_UI_DOWNLOADING;
        UpdateDetailsRender();GetWindowTextW(GetDlgItem(u_details,403),text,256);assert(wcsstr(text,L"45%"));assert(!IsWindowEnabled(GetDlgItem(u_details,IDOK)));
        if(argc>1&&dpi==144&&ru){wcscpy_s(u_notes,UPDATE_NOTES_CAP,L"## Изменения\n- Просмотр изменений перед установкой.\n- Прогресс скачивания обновления.\n- Компактная карточка смены статуса.\n\nНастройки и локальные заметки сохранятся.");UpdateDetailsNotes();CaptureUpdateDetails(dark?L"bin/update-details-native-dark.bmp":L"bin/update-details-native-light.bmp");}
        work->received=1000;work->verifying=1;UpdateDetailsRender();GetWindowTextW(GetDlgItem(u_details,403),text,256);assert(!wcsstr(text,L"100%")&&"Completed download must be distinguished from verification");
        UINT generation=u_generation;SendMessageW(u_details,WM_CLOSE,0,0);assert(!u_details&&!u_work&&work->cancel.cancelled&&u_generation>generation&&u_state==UPDATE_UI_AVAILABLE&&u_settings==s_window);
        work->status=UPDATE_OK;UpdateUiHandleMessage(WM_UPDATE_RESULT,0,(LPARAM)work);assert(u_state==UPDATE_UI_AVAILABLE&&"Late cancelled result cannot install or overwrite current state");
        UpdateDetailsOpen(s_window);u_state=UPDATE_UI_READY;UpdateDetailsRender();assert(IsWindowEnabled(GetDlgItem(u_details,IDOK)));UpdateDetailsClose();u_state=UPDATE_UI_AVAILABLE;
    }
    SendMessageW(s_window,WM_COMMAND,IDCANCEL,0);assert(!u_details);DestroyWindow(g_window);DeleteFileW(g_settings_path);puts("Update details: notes, byte percentage, verification, cancellation and late results across DPI/themes/languages passed");return 0;
}
