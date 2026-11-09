/* Keep the real worker, snapshot installation and painter; isolate only HTTP,
   thread dispatch, clock and Windows' animation preference at their boundaries. */
#include <windows.h>
#include <assert.h>
static int bitmap_failure;
static int lift_popup_calls;
static HGDIOBJ lift_bitmaps[512],lift_dcs[512];
static int lift_bitmap_count,lift_dc_count;
static void LiftTrack(HGDIOBJ *objects,int *count,HGDIOBJ object)
{if(!object)return;for(int i=0;i<512;i++)if(!objects[i]){objects[i]=object;(*count)++;return;}assert(!"Fixture resource tracker capacity");}
static void LiftUntrack(HGDIOBJ *objects,int *count,HGDIOBJ object)
{for(int i=0;i<512;i++)if(objects[i]==object){objects[i]=NULL;(*count)--;return;}}
static BOOL WINAPI LiftFixturePopup(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT *exclude)
{(void)menu;(void)flags;(void)x;(void)y;(void)reserved;(void)owner;(void)exclude;lift_popup_calls++;return 0;}
static HBITMAP WINAPI LiftFixtureBitmap(HDC dc,int width,int height)
{if(bitmap_failure>0&&!--bitmap_failure)return NULL;HBITMAP bitmap=CreateCompatibleBitmap(dc,width,height);LiftTrack(lift_bitmaps,&lift_bitmap_count,(HGDIOBJ)bitmap);return bitmap;}
static HDC WINAPI LiftFixtureDC(HDC source)
{HDC dc=CreateCompatibleDC(source);LiftTrack(lift_dcs,&lift_dc_count,(HGDIOBJ)dc);return dc;}
static BOOL WINAPI LiftFixtureDeleteObject(HGDIOBJ object)
{BOOL ok=DeleteObject(object);if(ok&&object)LiftUntrack(lift_bitmaps,&lift_bitmap_count,object);return ok;}
static BOOL WINAPI LiftFixtureDeleteDC(HDC dc)
{BOOL ok=DeleteDC(dc);if(ok&&dc)LiftUntrack(lift_dcs,&lift_dc_count,(HGDIOBJ)dc);return ok;}
#define CreateCompatibleBitmap LiftFixtureBitmap
#define CreateCompatibleDC LiftFixtureDC
#define DeleteObject LiftFixtureDeleteObject
#define DeleteDC LiftFixtureDeleteDC
#define TrackPopupMenu LiftFixturePopup
#define main FeedbackRegressionEntryUnused
#include "progress_feedback_test.c"
#undef main
static void LiftTick(UINT elapsed){fixture_now+=elapsed;WindowProcedure(g_window,WM_TIMER,946,0);}
static void FlushFixturePainting(void)
{RedrawWindow(g_window,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);GdiFlush();}
int main(void)
{
    g_instance=GetModuleHandleW(NULL);g_dpi=96;g_notifications.tasks=g_notifications.comments=g_notifications.statuses=FALSE;
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};assert(InitCommonControlsEx(&common));
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));assert(GetTempFileNameW(temp,L"lift",0,g_settings_path));
    g_window=CreateWindowW(L"STATIC",L"",WS_POPUP,30000,30000,900,600,NULL,NULL,g_instance,NULL);assert(g_window);CreateControls();
    ShowWindow(g_window,SW_SHOWNOACTIVATE);Seed();
    RECT original;assert(ListView_GetItemRect(g_list,RowForKey(L"B-2"),&original,LVIR_BOUNDS));
    StartAndFinish(3);
    HWND layer=GetDlgItem(g_list,3050),actor=layer?GetDlgItem(layer,3051):NULL;
    assert(layer&&actor&&"A confirmed start must move the row, not teleport it");
    RECT initial,middle;GetWindowRect(actor,&initial);MapWindowPoints(NULL,g_list,(POINT*)&initial,2);
    assert(initial.top==original.top&&initial.bottom==original.bottom);
    assert(g_tasks[TaskByKey(L"B-2")].active&&RowForKey(L"B-2")==0);
    assert(!progress_timer&&"The amber wave must wait for the moving row to arrive");
    LiftTick(175);GetWindowRect(actor,&middle);MapWindowPoints(NULL,g_list,(POINT*)&middle,2);
    RECT destination;ListView_GetItemRect(g_list,0,&destination,LVIR_BOUNDS);
    assert(middle.top<original.top&&middle.top>destination.top);
    LiftTick(175);assert(!GetDlgItem(g_list,3050)&&progress_timer);
    Advance(225);assert(RowPixel(L"B-2",25)!=PanelColor()&&RowPixel(L"B-2",25)!=ActiveColor());Advance(225);
    Seed();StartAndFinish(3);layer=GetDlgItem(g_list,3050);assert(layer);int menus=lift_popup_calls;
    SendMessageW(layer,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(100,20));
    SendMessageW(g_list,WM_RBUTTONUP,0,MAKELPARAM(100,original.top+2));
    assert(lift_popup_calls==menus&&!fixture_job&&"The release of a moving-row click must not open another issue's context menu");
    Seed();StartAndFinish(3);layer=GetDlgItem(g_list,3050);assert(layer);
    SendMessageW(layer,WM_RBUTTONDOWN,MK_RBUTTON,0);ReleaseCapture();
    ListView_SetItemState(g_list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);menus=lift_popup_calls;
    SendMessageW(g_list,WM_CONTEXTMENU,(WPARAM)g_list,-1);
    assert(lift_popup_calls==menus+1&&"Capture loss must not disable the keyboard context menu");
    SendMessageW(g_list,WM_RBUTTONUP,0,0);assert(lift_popup_calls==menus+1);
    Seed();StartAndFinish(3);assert(GetDlgItem(g_list,3050));
    SendMessageW(GetDlgItem(g_list,3050),WM_CANCELMODE,0,0);
    assert(!GetDlgItem(g_list,3050)&&!g_row_lift.tiles&&!g_row_lift.wave.previous);
    Seed();StartAndFinish(3);assert(GetDlgItem(g_list,3050));
    ListView_SetColumnWidth(g_list,1,ListView_GetColumnWidth(g_list,1)+20);
    assert(!GetDlgItem(g_list,3050));
    Seed();StartAndFinish(3);assert(GetDlgItem(g_list,3050));
    SendMessageW(GetDlgItem(GetDlgItem(g_list,3050),3051),WM_LBUTTONDOWN,MK_LBUTTON,0);
    assert(!GetDlgItem(g_list,3050)&&!fixture_job&&g_tasks[TaskByKey(L"B-2")].active);
    Seed();StartAndFinish(3);RefreshTaskList();assert(!GetDlgItem(g_list,3050));
    Seed();StartAndFinish(3);ApplyTaskSort(1);assert(!GetDlgItem(g_list,3050));
    Seed();StartAndFinish(3);g_dark=TRUE;ApplyTheme();assert(!GetDlgItem(g_list,3050));
    Seed();StartAndFinish(3);SendMessageW(g_list,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);assert(!GetDlgItem(g_list,3050));
    Seed();StartAndFinish(3);fixture_animation=FALSE;LiftTick(1);assert(!GetDlgItem(g_list,3050)&&!progress_timer);
    Seed();fixture_animation=FALSE;StartAndFinish(3);assert(!GetDlgItem(g_list,3050)&&!progress_timer);
    Seed();ShowWindow(g_window,SW_HIDE);StartAndFinish(3);assert(!GetDlgItem(g_list,3050)&&progress_timer);ShowWindow(g_window,SW_SHOWNOACTIVATE);
    Seed();fail_post=TRUE;StartAndFinish(3);assert(!GetDlgItem(g_list,3050)&&!progress_timer);
    Seed();fail_reload=TRUE;StartAndFinish(3);assert(!GetDlgItem(g_list,3050)&&!progress_timer);
    Seed();StartAndFinish(3);wcscpy_s(g_snapshot.server,800,L"other.example.test:443");LiftTick(1);assert(!GetDlgItem(g_list,3050)&&!progress_timer);
    for(int failure=1;failure<=4;failure++){
        Seed();int before_failure=lift_bitmap_count,before_dc=lift_dc_count;bitmap_failure=failure;StartAndFinish(3);bitmap_failure=0;
        assert(!GetDlgItem(g_list,3050)&&progress_timer&&!g_row_lift.tiles&&!g_row_lift.wave.previous);
        assert(lift_bitmap_count==before_failure&&lift_dc_count==before_dc);
    }
    for(int dpi=96;dpi<=192;dpi+=48)for(int density=0;density<2;density++)for(int dark=0;dark<2;dark++){
        Seed();g_dpi=dpi;g_density=density;g_dark=dark;UpdateFonts();RecreateBrushes();
        SetWindowPos(g_window,NULL,0,0,D(900),D(600),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);RefreshTaskList();
        /* Track the actual native bitmaps/DCs owned by this code. The process-wide
           GDI count also fluctuates as Windows creates/releases theme resources. */
        StartAndFinish(3);LiftTick(350);Advance(450);
        FlushFixturePainting();
        int objects=lift_bitmap_count,dcs=lift_dc_count;
        for(int repeat=0;repeat<3;repeat++){
            Seed();g_dark=dark;StartAndFinish(3);assert(GetDlgItem(g_list,3050));assert(lift_bitmap_count>objects);LiftTick(175);assert(GetDlgItem(g_list,3050));LiftTick(175);assert(!GetDlgItem(g_list,3050)&&progress_timer);Advance(450);
            FlushFixturePainting();
            assert(lift_bitmap_count==objects&&lift_dc_count==dcs);
        }
    }
    Seed();StartAndFinish(3);assert(GetDlgItem(g_list,3050));
    WindowProcedure(g_window,WM_DESTROY,0,0);DestroyWindow(g_window);assert(DeleteFileW(g_settings_path));
    assert(!g_row_lift.tiles&&!g_row_lift.wave.previous&&!g_row_lift.buffer&&!g_row_lift.canvas);
    assert(!lift_bitmap_count&&!lift_dc_count);
    puts("Row lift: confirmed result, intermediate positions, deferred wave, cancellation/input/geometry, reduced motion, scope/failure guards, GDI fallback and repeated DPI/theme cleanup passed");return 0;
}
