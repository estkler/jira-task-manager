#include <windows.h>
#include <assert.h>
#include <stdio.h>
static SYSTEMTIME due_today={.wYear=2026,.wMonth=11,.wDay=6,.wHour=12};
static void WINAPI DueLocalTime(LPSYSTEMTIME value){*value=due_today;}
static BOOL DueNoAccount(void){return FALSE;}
static BOOL DueNoUrl(wchar_t *value,size_t count){if(count)value[0]=0;return FALSE;}
static int due_menus;
static BOOL WINAPI DueTrackPopup(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *rect){due_menus++;return 0;}
static HDC due_capture_dc;
static HDC WINAPI DueBeginPaint(HWND window,LPPAINTSTRUCT paint){if(!due_capture_dc)return BeginPaint(window,paint);ZeroMemory(paint,sizeof(*paint));paint->hdc=due_capture_dc;GetClientRect(window,&paint->rcPaint);return due_capture_dc;}
static BOOL WINAPI DueEndPaint(HWND window,const PAINTSTRUCT *paint){return due_capture_dc?TRUE:EndPaint(window,paint);}
#define GetLocalTime DueLocalTime
#define JiraHasAccount DueNoAccount
#define JiraGetUrl DueNoUrl
#define BeginPaint DueBeginPaint
#define EndPaint DueEndPaint
#define TrackPopupMenu DueTrackPopup
#include "native.c"
static LRESULT CALLBACK DueFixtureProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(!g_list||message==WM_CREATE||message==WM_DESTROY)return DefWindowProcW(window,message,wp,lp);
    return WindowProcedure(window,message,wp,lp);
}
static void DuePrintChildren(HWND parent,HDC dc){
    for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))if(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE){
        RECT r;GetWindowRect(child,&r);MapWindowPoints(NULL,g_window,(POINT*)&r,2);int saved=SaveDC(dc);
        SetViewportOrgEx(dc,r.left,r.top,NULL);IntersectClipRect(dc,0,0,r.right-r.left,r.bottom-r.top);
        SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);DuePrintChildren(child,dc);
    }
}
static void DueCapture(const wchar_t *path){
    RECT bounds;GetClientRect(g_window,&bounds);HDC screen=GetDC(g_window),dc=CreateCompatibleDC(screen);
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bounds.right;info.bmiHeader.biHeight=-bounds.bottom;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void *pixels;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    due_capture_dc=dc;SendMessageW(g_window,WM_PAINT,0,0);due_capture_dc=NULL;
    DuePrintChildren(g_window,dc);
    BITMAPFILEHEADER file={.bfType=0x4d42,.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)};DWORD size=bounds.right*bounds.bottom*4,wrote;
    file.bfSize=file.bfOffBits+size;HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(output!=INVALID_HANDLE_VALUE);
    assert(WriteFile(output,&file,sizeof(file),&wrote,NULL)&&WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&wrote,NULL)&&WriteFile(output,pixels,size,&wrote,NULL));CloseHandle(output);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(g_window,screen);
}
int main(int argc,char **argv)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_jira_busy=TRUE;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"due",0,g_settings_path);
    WNDCLASSW cls={.lpfnWndProc=DueFixtureProcedure,.hInstance=g_instance,.lpszClassName=L"DueDateFixture"};RegisterClassW(&cls);
    g_window=CreateWindowW(cls.lpszClassName,L"",WS_OVERLAPPEDWINDOW,30000,30000,900,550,NULL,NULL,g_instance,NULL);CreateControls();
    TaskItem tasks[]={
        {.key=L"DUE-1",.summary=L"Today task",.status=STATUS_TODO,.assigned_to_me=TRUE,.due_date=L"2026-11-06"},
        {.key=L"DUE-2",.summary=L"Overdue task",.status=STATUS_HOLD,.assigned_to_me=TRUE,.due_date=L"2026-11-04"},
        {.key=L"LONGPROJECTKEY-123456",.summary=L"Previous-year deadline",.status=STATUS_TODO,.assigned_to_me=TRUE,.due_date=L"2025-12-31"},
        {.key=L"DUE-4",.summary=L"No date task",.status=STATUS_TODO,.assigned_to_me=TRUE},
        {.key=L"DUE-5",.summary=L"Done task",.status=STATUS_DONE,.assigned_to_me=TRUE,.due_date=L"2026-11-04"}
    };
    g_tasks=tasks;g_task_count=5;g_tab=0;g_status_count=STATUS_COUNT;g_status_categories[STATUS_DONE]=g_status_categories[STATUS_CANCELED]=2;
    for(int i=0;i<5;i++)tasks[i].created_utc=NativeCommentUtcTime(i==2?L"2025-11-01T00:00:00Z":L"2026-11-01T00:00:00Z");
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int ru=0;ru<2;ru++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_russian=ru;g_dark=dark;UpdateFonts();RecreateBrushes();UpdateColumnLabels();
        const wchar_t *saved[5];for(int i=0;i<5;i++){saved[i]=tasks[i].due_date;tasks[i].due_date=NULL;}
        RefreshTaskList();RECT baseline;assert(ListView_GetItemRect(g_list,0,&baseline,LVIR_BOUNDS));
        for(int i=0;i<5;i++)tasks[i].due_date=saved[i];
        RefreshTaskList();LayoutControls(D(900),D(550));
        assert(ListView_GetColumnWidth(g_list,DUE_COLUMN)>0);
        int order[6];assert(ListView_GetColumnOrderArray(g_list,6,order)&&order[4]==DUE_COLUMN&&order[5]==STATUS_COLUMN);
        RECT due,status,row;assert(ListView_GetSubItemRect(g_list,0,DUE_COLUMN,LVIR_BOUNDS,&due));
        assert(ListView_GetSubItemRect(g_list,0,STATUS_COLUMN,LVIR_BOUNDS,&status)&&due.right<=status.left);
        assert(ListView_GetItemRect(g_list,0,&row,LVIR_BOUNDS)&&row.bottom-row.top==baseline.bottom-baseline.top);
        POINT point={(due.left+due.right)/2,(due.top+due.bottom)/2};assert(StatusTaskAtPoint(point)<0&&!ShouldFlashRowCell(DUE_COLUMN,FALSE));
        NativeDueText text;assert(TaskDueDescribe(&tasks[0],&text)&&text.kind==NATIVE_DUE_TODAY);
        assert(TaskDueDescribe(&tasks[1],&text)&&text.kind==NATIVE_DUE_OVERDUE);
        assert(TaskDueDescribe(&tasks[4],&text)&&text.kind==NATIVE_DUE_NORMAL);
        wchar_t tooltip[128];assert(TaskDueDateInfoTip(0,tooltip,128)&&wcsstr(tooltip,L"2026"));
        assert(!TaskDueDateInfoTip(3,tooltip,128)&&!tooltip[0]);
        HDC dc=GetDC(g_list);HGDIOBJ font=SelectObject(dc,g_small_font);
        for(int i=0;i<5;i++)if(TaskDueDescribe(&tasks[i],&text)){
            SIZE size;GetTextExtentPoint32W(dc,text.label,(int)wcslen(text.label),&size);
            assert(size.cx+D(14)<=ListView_GetColumnWidth(g_list,DUE_COLUMN));
        }
        SelectObject(dc,font);ReleaseDC(g_list,dc);
        assert(TaskDueColor(NATIVE_DUE_TODAY)!=TaskDueColor(NATIVE_DUE_OVERDUE));
        const int widths[]={510,570,600,755,1000};
        for(int w=0;w<5;w++)for(int mode=0;mode<3;mode++){
            int width=widths[w];g_comments_mode=g_comments_visual_mode=mode;g_comments_reveal=mode?1000:0;
            SetWindowPos(g_window,NULL,0,0,MulDiv(width,dpi,96),D(550),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            RECT client,list;GetClientRect(g_window,&client);LayoutControls(client.right,client.bottom);GetClientRect(g_list,&list);
            assert(ListView_GetSubItemRect(g_list,0,STATUS_COLUMN,LVIR_BOUNDS,&status)&&status.right<=list.right);
            assert(ListView_GetSubItemRect(g_list,0,DUE_COLUMN,LVIR_BOUNDS,&due)&&due.right<=status.left);
            assert(ListView_GetColumnWidth(g_list,2)>=D(60)&&"A long deadline must leave room for the task title at minimum width");
            HDC compact=GetDC(g_list);HGDIOBJ compact_font=SelectObject(compact,g_small_font);
            for(int i=0;i<5;i++)if(TaskDueDisplayForWidth(compact,&tasks[i],due.right-due.left,&text)){
                SIZE size;GetTextExtentPoint32W(compact,text.label,(int)wcslen(text.label),&size);
                assert(size.cx+D(14)<=due.right-due.left&&"Compact date must fit without clipping its digits");
            }
            SelectObject(compact,compact_font);ReleaseDC(g_list,compact);
        }
    }
    /* Real list messages: valid dates toggle once; every other context menu survives. */
    g_comments_mode=g_comments_visual_mode=g_comments_reveal=0;g_dpi=96;g_density=1;UpdateFonts();RefreshTaskList();LayoutControls(900,550);
    RECT due_before,row_before;ListView_GetSubItemRect(g_list,0,DUE_COLUMN,LVIR_BOUNDS,&due_before);ListView_GetItemRect(g_list,0,&row_before,LVIR_BOUNDS);
    LPARAM due_point=MAKELPARAM((due_before.left+due_before.right)/2,(due_before.top+due_before.bottom)/2);
    WritePrivateProfileStringW(L"Timer",L"Started",L"123456789",g_settings_path);
    WritePrivateProfileStringW(L"LocalData",L"Sentinel",L"keep",g_settings_path);
    g_due_bar_mode=FALSE;g_flash_task=-1;
    SendMessageW(g_list,WM_RBUTTONDOWN,MK_RBUTTON,due_point);assert(!g_due_bar_mode&&g_flash_task==-1);
    SendMessageW(g_list,WM_RBUTTONUP,0,due_point);assert(g_due_bar_mode&&due_menus==0);
    assert(GetPrivateProfileIntW(L"Interface",L"DueDateBar",0,g_settings_path)==1);
    assert(GetPrivateProfileIntW(L"Timer",L"Started",0,g_settings_path)==123456789);
    wchar_t sentinel[16];GetPrivateProfileStringW(L"LocalData",L"Sentinel",L"",sentinel,16,g_settings_path);assert(!wcscmp(sentinel,L"keep"));
    wchar_t bar_tip[512];assert(TaskDueDateInfoTip(0,bar_tip,512)&&wcsstr(bar_tip,g_russian?L"создания":L"creation"));
    RECT due_after,row_after;ListView_GetSubItemRect(g_list,0,DUE_COLUMN,LVIR_BOUNDS,&due_after);ListView_GetItemRect(g_list,0,&row_after,LVIR_BOUNDS);
    assert(EqualRect(&due_before,&due_after)&&EqualRect(&row_before,&row_after));
    g_due_bar_mode=FALSE;LoadSettings();assert(g_due_bar_mode);
    SendMessageW(g_list,WM_RBUTTONDOWN,MK_RBUTTON,due_point);SendMessageW(g_list,WM_RBUTTONUP,0,due_point);assert(!g_due_bar_mode&&due_menus==0);
    assert(!GetPrivateProfileIntW(L"Interface",L"DueDateBar",1,g_settings_path));
    POINT context={(due_before.left+due_before.right)/2,(due_before.top+due_before.bottom)/2};ClientToScreen(g_list,&context);
    SendMessageW(g_list,WM_CONTEXTMENU,(WPARAM)g_list,MAKELPARAM(context.x,context.y));assert(g_due_bar_mode&&due_menus==0);
    RECT empty;ListView_GetSubItemRect(g_list,3,DUE_COLUMN,LVIR_BOUNDS,&empty);context=(POINT){(empty.left+empty.right)/2,(empty.top+empty.bottom)/2};ClientToScreen(g_list,&context);
    SendMessageW(g_list,WM_CONTEXTMENU,(WPARAM)g_list,MAKELPARAM(context.x,context.y));assert(due_menus==1&&g_due_bar_mode);
    ListView_SetItemState(g_list,0,LVIS_FOCUSED,LVIS_FOCUSED);SendMessageW(g_list,WM_CONTEXTMENU,(WPARAM)g_list,MAKELPARAM(-1,-1));assert(due_menus==2&&g_due_bar_mode);
    g_due_bar_mode=FALSE;
    if(argc>1){
        g_comments_mode=g_comments_visual_mode=g_comments_reveal=0;
        g_dpi=96;g_density=1;g_russian=TRUE;g_jira_busy=FALSE;UpdateFonts();UpdateColumnLabels();
        wcscpy_s(g_status_names[STATUS_TODO],128,L"To Do");wcscpy_s(g_status_names[STATUS_HOLD],128,L"On Hold");wcscpy_s(g_status_names[STATUS_DONE],128,L"Done");
        SetWindowPos(g_window,NULL,0,0,880,500,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        for(int dark=0;dark<2;dark++){g_dark=dark;RecreateBrushes();RefreshTaskList();DueCapture(dark?L"bin/due-native-dark.bmp":L"bin/due-native-light.bmp");
            g_due_bar_mode=TRUE;DueCapture(dark?L"bin/due-bar-native-dark.bmp":L"bin/due-bar-native-light.bmp");g_due_bar_mode=FALSE;}
    }
    SYSTEMTIME updated=g_updated_time;g_russian=TRUE;due_today.wDay=7;UpdateDueDateDay();
    wchar_t label[96];ListView_GetItemText(g_list,0,DUE_COLUMN,label,96);assert(wcsstr(label,L"Просрочено"));
    assert(!memcmp(&updated,&g_updated_time,sizeof(updated))&&"Local midnight must not pretend Jira has refreshed");
    g_due_bar_mode=TRUE;g_russian=TRUE;const wchar_t *original_due=tasks[1].due_date;ULONGLONG original_created=tasks[1].created_utc;int amount;
    tasks[1].due_date=L"2026-06-05";tasks[1].created_utc=NativeCommentUtcTime(L"2026-06-09T12:10:00Z");assert(TaskDueBarAmount(&tasks[1],&amount)&&amount==1000);
    tasks[1].due_date=L"2099-11-12";tasks[1].created_utc=0;assert(TaskDueBarAmount(&tasks[1],&amount)&&amount==-1);
    int unknown_row=-1;for(int row=0;row<ListView_GetItemCount(g_list);row++)if(TaskIndexAtRow(row)==1)unknown_row=row;
    assert(unknown_row>=0&&TaskDueDateInfoTip(unknown_row,bar_tip,512)&&wcsstr(bar_tip,L"заполнение недоступно"));
    tasks[1].due_date=original_due;tasks[1].created_utc=original_created;
    for(int i=0;i<5;i++)tasks[i].due_date=NULL;RefreshTaskList();assert(ListView_GetColumnWidth(g_list,DUE_COLUMN)==0);
    g_tasks=NULL;g_task_count=0;DestroyWindow(g_window);DeleteFileW(g_settings_path);
    puts("Due UI: before-status column, unchanged rows, read-only hit targets, full tooltip, RU/EN/themes/DPI, rollover and empty state passed");
}
