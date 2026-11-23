#include <windows.h>
static BOOL WINAPI MarkFixtureShow(HWND window,int command)
{
    wchar_t name[80];GetClassNameW(window,name,80);
    if(!wcscmp(name,L"Tasklet.StatusChangeDetail"))return TRUE;
    return ShowWindow(window,command);
}
#define ShowWindow MarkFixtureShow
static int mark_job_attempts;
static HANDLE WINAPI MarkFixtureThread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID d,DWORD e,LPDWORD f){mark_job_attempts++;return NULL;}
#define CreateThread MarkFixtureThread
static HWND mark_warning_owner;
static int WINAPI MarkFixtureMessage(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){mark_warning_owner=owner;SendMessageW(owner,WM_ACTIVATE,WA_INACTIVE,0);return IDOK;}
#define MessageBoxW MarkFixtureMessage
#define STATUS_ALERT_ENTRY StatusAlertMainUnused
#include "status_alert_test.c"
#undef ShowWindow
static int MarkRow(void){for(int i=0;i<ListView_GetItemCount(g_list);i++)if(!wcscmp(g_tasks[TaskIndexAtRow(i)].key,L"PDES-6487"))return i;return -1;}
static void CaptureNativeMarks(const wchar_t *path)
{
    int width=D(900),height=D(230);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);void *pixels;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(bitmap);HGDIOBJ old=SelectObject(dc,bitmap);
    RECT area={0,0,width,height};FillRect(dc,&area,g_window_brush);
    for(int row=0;row<ListView_GetItemCount(g_list);row++){NMLVCUSTOMDRAW draw={0};draw.nmcd.hdc=dc;draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT;draw.nmcd.dwItemSpec=row;DrawListCustom(&draw);}
    OpenStatusMark(MarkRow());RECT popup;GetClientRect(g_status_mark_window,&popup);int x=width-popup.right-D(15),y=D(110);
    int saved=SaveDC(dc);SetViewportOrgEx(dc,x,y,NULL);DrawStatusMarkDetail(g_status_mark_window,dc);RestoreDC(dc,saved);
    /* Header and close glyph are painted by the actual native card. */
    CloseStatusMark();GdiFlush();
    BITMAPFILEHEADER file={.bfType=0x4d42,.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)};DWORD size=(DWORD)width*height*4,wrote;file.bfSize=file.bfOffBits+size;
    HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);assert(output!=INVALID_HANDLE_VALUE);
    assert(WriteFile(output,&file,sizeof(file),&wrote,NULL)&&WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&wrote,NULL)&&WriteFile(output,pixels,size,&wrote,NULL));CloseHandle(output);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
int main(int argc,char **argv)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_russian=TRUE;g_tab=1;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);assert(GetTempFileNameW(temp,L"sui",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    statuses(L"review",L"In Review",L"progress",FALSE);statuses(L"upload",L"Upload project",L"progress",TRUE);
    if(argc>1){g_dpi=96;g_density=1;for(int dark=0;dark<2;dark++){g_dark=dark;UpdateFonts();RecreateBrushes();statuses(L"review",L"In Review",L"progress",FALSE);statuses(L"upload",L"Upload project",L"progress",FALSE);RefreshTaskList();CaptureNativeMarks(dark?L"bin/status-marks-native-dark.bmp":L"bin/status-marks-native-light.bmp");}}
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();SetWindowPos(g_window,NULL,0,0,D(900),D(600),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);RefreshTaskList();
        statuses(L"review",L"In Review",L"progress",FALSE);statuses(L"upload",L"Upload project",L"progress",FALSE);int row=MarkRow();RECT mark,pill;assert(StatusMarkRectForRow(row,&mark)&&StatusPillRectForRow(row,&pill));
        assert(mark.right<=pill.left&&"Unread marker must never overlap the status pill");
        HDC measure=GetDC(g_list);HGDIOBJ font=SelectObject(measure,g_font);SIZE label;GetTextExtentPoint32W(measure,L"Upload project",14,&label);SelectObject(measure,font);ReleaseDC(g_list,measure);
        if(pill.right-pill.left-D(35)<label.cx)fprintf(stderr,"Status label: dpi=%d density=%d available=%ld measured=%ld\n",dpi,density,pill.right-pill.left-D(35),label.cx);
        assert(pill.right-pill.left-D(35)>=label.cx&&"The marker must not truncate a normally fitting status label");
        POINT p={(mark.left+mark.right)/2,(mark.top+mark.bottom)/2};assert(StatusMarkAtPoint(p)>=0&&StatusTaskAtPoint(p)<0);
        g_jira_busy=FALSE;NMITEMACTIVATE click={.hdr={g_list,IDC_TASKS,NM_CLICK},.iItem=row,.iSubItem=STATUS_COLUMN,.ptAction=p};
        WindowProcedure(g_window,WM_NOTIFY,IDC_TASKS,(LPARAM)&click);assert(g_status_mark_window&&!g_jira_busy);
        assert(!NativeStatusMarksFind(&g_status_marks,L"PDES-6487")&&"Opening the change marks it read without an extra acknowledgement action");
        HWND popup=g_status_mark_window;g_updated_time.wYear=2026;g_jira_stale=FALSE;int jobs=mark_job_attempts;click.hdr.code=NM_DBLCLK;WindowProcedure(g_window,WM_NOTIFY,IDC_TASKS,(LPARAM)&click);assert(mark_job_attempts==jobs&&!g_jira_busy&&"A double-click on an acknowledged marker must never attempt a Jira action");
        SendMessageW(popup,WM_KEYDOWN,VK_ESCAPE,0);assert(!g_status_mark_window&&!StatusMarkRectForRow(row,&mark));
    }
    statuses(L"review",L"In Review",L"progress",FALSE);statuses(L"upload",L"Upload project",L"progress",FALSE);
    OpenStatusMark(MarkRow());assert(g_status_mark_window);RefreshTaskList();assert(!g_status_mark_window&&!NativeStatusMarksFind(&g_status_marks,L"PDES-6487"));
    statuses(L"review",L"In Review",L"progress",FALSE);statuses(L"upload",L"Upload project",L"progress",FALSE);
    wchar_t saved_path[MAX_PATH];wcscpy_s(saved_path,MAX_PATH,g_settings_path);wcscpy_s(g_settings_path,MAX_PATH,L"Z:/nonexistent-tasklet-folder/settings.ini");
    OpenStatusMark(MarkRow());
    assert(mark_warning_owner==g_window&&!g_status_mark_window&&NativeStatusMarksFind(&g_status_marks,L"PDES-6487")&&"Failed persistence retains mark and uses a stable warning owner");
    wcscpy_s(g_settings_path,MAX_PATH,saved_path);
    OpenStatusMark(MarkRow());NativeStatusMark stale=g_status_mark_shown;
    statuses(L"done",L"Done",L"progress",FALSE);assert(!g_status_mark_window&&!NativeStatusMarksAcknowledge(&g_status_marks,g_settings_path,&stale));
    g_tab=2;RefreshTaskList();int row=MarkRow();assert(row>=0);OpenStatusMark(row);assert(g_status_mark_window&&!NativeStatusMarksFind(&g_status_marks,L"PDES-6487"));CloseStatusMark();
    g_tab=1;statuses(L"upload",L"Upload project",L"progress",FALSE);assert(NativeStatusMarksFind(&g_status_marks,L"PDES-6487"));
    NativeStatusWrites own={.scope=g_snapshot};own.scope.items=NULL;assert(NativeStatusRemember(&own,L"PDES-6487",L"done",L"Done"));
    free(g_status_writes.items);g_status_writes=own;statuses(L"done",L"Done",L"progress",FALSE);
    assert(!NativeStatusMarksFind(&g_status_marks,L"PDES-6487"));
    NativeStatusMarksFree(&g_status_marks);FreeTaskItems();JiraFreeSnapshot(&g_snapshot);DestroyWindow(g_window);DeleteFileW(g_settings_path);
    puts("Status mark UI: exact marker/pill hit targets, no double-click POST, acknowledgement on open, failed-save preservation, stale popup and own Done across DPI/themes passed");return 0;
}
