/* Offline QA fixture only: never initializes the product singleton, storage,
   credentials, tray, Jira refresh, or update installation. */
#define main settings_regression_main
#define wWinMain unused_product_entry
#include "settings_test.c"
#undef main
#undef wWinMain
static WNDPROC preview_settings_procedure;
static HWND preview_states;
static LRESULT CALLBACK PreviewSettingsProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_COMMAND){int id=LOWORD(wp);if(id==IDOK||id==215||id==234||id==238)return 0;if(id==242&&(u_state==UPDATE_UI_READY||u_state==UPDATE_UI_APPLYING))return 0;}
    return CallWindowProcW(preview_settings_procedure,window,message,wp,lp);
}
static void PreviewSettings(UpdateUiState state){
    if(s_window)DestroyWindow(s_window);OpenSettingsWindow();assert(s_window);
    SetWindowLongPtrW(s_window,GWLP_HWNDPARENT,0); /* expose QA panel as its own capture target */
    preview_settings_procedure=(WNDPROC)SetWindowLongPtrW(s_window,GWLP_WNDPROC,(LONG_PTR)PreviewSettingsProcedure);
    RECT bounds={0,0,S(430),S(348)};AdjustWindowRectExForDpi(&bounds,(DWORD)GetWindowLongPtrW(s_window,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(s_window,GWL_EXSTYLE),GetDpiForWindow(s_window));
    SetWindowPos(s_window,NULL,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    u_state=state;u_release.version=(UpdateVersion){0,9,30};u_error=UPDATE_NETWORK;u_no_release=FALSE;SettingsTab(1);UpdateUiRender(s_window);
    SetWindowTextW(s_window,L"QA — Jira Task Manager · About (offline)");
}
static LRESULT CALLBACK PreviewHost(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(UpdateUiHandleMessage(message,wp,lp))return 0;
    if(message==WM_COMMAND){
        if(LOWORD(wp)==9000&&HIWORD(wp)==CBN_SELCHANGE){u_state=(UpdateUiState)SendMessageW(preview_states,CB_GETCURSEL,0,0);u_reason[0]=0;UpdateUiRender(s_window);}
        else if(LOWORD(wp)==9001){g_russian=!g_russian;UpdateSettingsLanguage();SetWindowTextW(s_window,L"QA — Jira Task Manager · About (offline)");}
        else if(LOWORD(wp)==9002){g_dark=!g_dark;RecreateBrushes();ApplyWindowCaptionTheme(s_window);RedrawWindow(s_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);}
        else if(LOWORD(wp)==9003){UpdateUiState state=u_state;g_dpi=g_dpi==96?144:g_dpi==144?192:96;PreviewSettings(state);}
        return 0;
    }
    if(message==WM_DESTROY){UpdateUiShutdown();PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show){
    (void)previous;(void)command;(void)show;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);g_instance=instance;g_dpi=GetDpiForSystem();g_dark=FALSE;g_russian=TRUE;RecreateBrushes();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&common);update_fixture_gate=CreateEventW(NULL,TRUE,TRUE,NULL);
    WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=PreviewHost;wc.lpszClassName=L"JTM.Update.OfflinePreview";wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
    g_window=CreateWindowExW(0,wc.lpszClassName,L"QA — Update controls (offline)",WS_OVERLAPPEDWINDOW,50,70,500,190,NULL,NULL,instance,NULL);assert(g_window);
    preview_states=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,16,16,430,240,g_window,(HMENU)9000,instance,NULL);
    const wchar_t *names[]={L"Idle",L"Checking",L"Current",L"Available",L"Downloading",L"Ready",L"Applying",L"Error"};for(int i=0;i<8;i++)SendMessageW(preview_states,CB_ADDSTRING,0,(LPARAM)names[i]);SendMessageW(preview_states,CB_SETCURSEL,0,0);
    CreateWindowExW(0,L"BUTTON",L"RU / EN",WS_CHILD|WS_VISIBLE,16,62,100,30,g_window,(HMENU)9001,instance,NULL);
    CreateWindowExW(0,L"BUTTON",L"Light / Dark",WS_CHILD|WS_VISIBLE,126,62,120,30,g_window,(HMENU)9002,instance,NULL);
    CreateWindowExW(0,L"BUTTON",L"96 / 144 / 192 DPI",WS_CHILD|WS_VISIBLE,256,62,180,30,g_window,(HMENU)9003,instance,NULL);
    ShowWindow(g_window,SW_SHOW);PreviewSettings(UPDATE_UI_IDLE);MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}CloseHandle(update_fixture_gate);return 0;
}
