#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <wtsapi32.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include "time_input.h"
#include "workday_clock.h"
#include "native_jira.h"
#include "native_notes.h"
#include "native_new_tasks.h"
#include "native_lifecycle.h"
#include "native_identity.h"
#include "native_update.h"

#define APP_VERSION L"0.9.30"
#define APP_TITLE TASK_MANAGER_TITLE
#define WM_TRAYICON (WM_APP + 1)

#define IDC_SEARCH       1001
#define IDC_CLEAR_SEARCH 1015
#define IDC_FILTER       1002
#define IDC_TAB_ALL      1003
#define IDC_TAB_CURRENT  1004
#define IDC_TAB_DONE     1005
#define IDC_TAB_REPORTED 1018
#define IDC_TASKS        1006
#define IDC_TIMER        1007
#define IDC_UPDATED      1008
#define IDC_REFRESH      1009
#define IDC_COMMENTS     1010
#define IDC_THEME        1011
#define IDC_LANGUAGE     1012
#define IDC_SETTINGS     1013
#define IDC_DENSITY      1014
#define IDC_START_EDIT   1016
#define IDC_NOTE_EDIT    1017
#define IDC_TRANSITION_RATING 1020
#define IDC_TRANSITION_COMMENT 1030
#define IDC_TRANSITION_REMEMBER 1031
#define IDC_TRANSITION_HELPER 1032
#define IDC_TRANSITION_HELPER_MAX 1033
#define IDC_TRANSITION_CLOSE 1034
#define IDC_POPOVER_TEXT 1040
#define IDC_POPOVER_INPUT 1041
#define IDC_POPOVER_SEND 1042
#define COMMENTS_COLUMN  3
#define STATUS_COLUMN    4

#define IDM_STATUS_BASE  2000
#define IDM_FILTER_BASE  2100
#define IDM_TIMER_FORMAT 2201
#define IDM_TIMER_RESET  2202
#define IDM_TIMER_PORTAL 2203
#define IDM_TRAY_OPEN    2301
#define IDM_TRAY_EXIT    2302
#define IDM_TASK_NOTE    2350
#define IDM_TASK_COMMENTS 2351

typedef enum TaskStatus {
    STATUS_TODO,
    STATUS_HOLD,
    STATUS_PROGRESS,
    STATUS_REVIEW,
    STATUS_DONE,
    STATUS_CANCELED,
    STATUS_COUNT
} TaskStatus;

typedef struct TaskItem {
    const wchar_t *key;
    const wchar_t *summary;
    TaskStatus status;
    BOOL active;
    const wchar_t *latest_comment;
    wchar_t *local_note;
    BOOL is_new;
    BOOL assigned_to_me,reported_by_me;
    const wchar_t *latest_comment_id;
    BOOL unread_comment;
    ULONGLONG comment_pulse_started;
} TaskItem;

static TaskItem *g_tasks;
static int g_task_count;
static JiraSnapshot g_snapshot;
static wchar_t g_status_names[64][128];
static wchar_t g_status_ids[64][80];
static int g_status_categories[64]; /* 0 new, 1 indeterminate, 2 done */
static int g_status_count;
static BOOL g_jira_busy, g_jira_stale=TRUE;
static HANDLE g_jira_thread;
static SYSTEMTIME g_updated_time;
static wchar_t g_jira_message[256];
static volatile LONG g_jira_closing;
static BOOL g_update_handoff;
static int g_completion_active_count;
#define WM_JIRA_RESULT (WM_APP+20)
typedef struct JiraJob {
    int action; /* 0 refresh, 1 transitions, 2 explicit transition, 3 switch, 4 read comments, 5 post comment */
    wchar_t key[80], transition[80], transition_field[80], transition_option[80];
    wchar_t transition_comment[2048], error[256];
    POINT anchor;
    JiraSnapshot snapshot;JiraTransitions transitions;JiraComments comments;
    BOOL success, loaded;
} JiraJob;
typedef struct TransitionInputDialog {
    const JiraTransitionItem *transition;
    HWND window;HFONT font,small_font,title_font;
    wchar_t key[80],option_id[80],comment[2048];
    wchar_t placeholder[1100];
    int selected_score,option_by_score[6];
    HWND hover;
    BOOL accepted,remember_comment;
} TransitionInputDialog;
static HINSTANCE g_instance;
static HWND g_window;
static HWND g_search;
static HWND g_clear_search;
static HWND g_search_count;
static HWND g_filter;
static HWND g_tab_all;
static HWND g_tab_current;
static HWND g_tab_done;
static HWND g_tab_reported;
static HWND g_list;
static HWND g_timer;
static HWND g_start_edit;
static HWND g_note_edit;
static wchar_t g_note_original[2048];
static BOOL g_editing_note;
static wchar_t g_note_key[80];
static wchar_t g_note_mode_anchor_key[80];
static int g_note_return_mode;
static BOOL g_editing_start;
static HWND g_keyboard_button;
static HWND g_updated;
static HWND g_refresh;
static HWND g_comments;
static HWND g_theme;
static HWND g_language;
static HWND g_settings;
static HWND g_density_button;
static int g_density;
static int g_refresh_minutes=30;
static int g_tray_task_count=20;
static HFONT g_font;
static HFONT g_small_font;
static HFONT g_timer_font;
static HFONT g_bold_font;
static HFONT g_symbol_font;
static HIMAGELIST g_row_height;
static UINT g_dpi = 96;
static HWND g_hover_button;
static int g_hover_row = -1;
static BOOL g_hover_status;
static BOOL g_hover_key;
static BOOL g_hover_note;
static BOOL g_hover_comment;
static int g_pressed_indicator_task = -1;
static int g_pressed_indicator_kind; /* 1 Jira comment, 2 local note */
static int g_pressed_status_task = -1;
static int g_status_pulse_task=-1;
static ULONGLONG g_status_pulse_started;
static wchar_t g_pending_switch_key[80];
static ULONGLONG g_pending_switch_started;
static int g_sort_column = -1;
static BOOL g_sort_ascending = TRUE;
static BOOL g_list_keyboard_focus;
static int g_flash_task = -1;
static ULONGLONG g_flash_started, g_flash_released;
static double g_flash_release_opacity;
static int g_flash_button=VK_LBUTTON;
static int g_context_task=-1;

static double FlashOpacity(ULONGLONG now)
{
    if(g_flash_task<0)return 0;
    double elapsed=(double)(now-g_flash_started);
    if(elapsed<90){double remaining=1.0-elapsed/90.0;return 0.34*(1-remaining*remaining*remaining);}
    if(g_flash_released){
        ULONGLONG fade_start=max(g_flash_released,g_flash_started+90);
        double remaining=1.0-(double)(now-fade_start)/360.0;
        return remaining>0?0.34*remaining*remaining*remaining:0;
    }
    if(elapsed<5000)return 0.34;
    double remaining=1.0-(elapsed-5000)/400.0;
    return remaining>0?0.34*remaining*remaining*remaining:0;
}
static void BeginPendingSwitchFeedback(const wchar_t *key)
{
    if(!key)return;wcsncpy_s(g_pending_switch_key,_countof(g_pending_switch_key),key,_TRUNCATE);
    g_pending_switch_started=GetTickCount64();g_flash_task=-1;
    if(g_list){KillTimer(g_list,2);SetTimer(g_list,5,60,NULL);InvalidateRect(g_list,NULL,FALSE);UpdateWindow(g_list);}
}
static void EndPendingSwitchFeedback(void)
{
    g_pending_switch_key[0]=0;if(g_list){KillTimer(g_list,5);InvalidateRect(g_list,NULL,FALSE);}
}
static HWND g_tooltips;
static wchar_t g_summary_tip[2048];
static wchar_t g_scope_tip[256];
static RECT g_search_frame;
static int g_reported_separator_x;
static BOOL g_filter_menu_open;
static DWORD g_filter_dismissed_tick;
static HBRUSH g_window_brush;
static HBRUSH g_panel_brush;
static HBRUSH g_popover_brush;
static BOOL g_dark;
static BOOL g_russian = TRUE;
static BOOL g_text_time;
static int g_comments_mode; /* 0 hidden, 1 Jira, 2 local */
static int g_comments_visual_mode,g_comments_reveal; /* visual transition, 0..1000 */
static int g_tab = 1,g_scope_tabs[2]={1,0};
static BOOL g_reported_scope;
static ULONGLONG g_filter_masks[6] = { ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull };
static ULONGLONG g_workday_started;
static BOOL g_allow_close;
static NativeLifecycle g_lifecycle;
static const wchar_t *g_shutdown_reason=L"normal";
static NOTIFYICONDATAW g_tray;
static wchar_t g_settings_path[MAX_PATH];
static BOOL g_footer_pinned,g_footer_animating,g_footer_hold_during_resize;
static int g_footer_right,g_footer_target_right,g_footer_deferred_width,g_window_expand_target,g_compact_width_dip;
typedef struct TaskPopover {
    HWND window,text,input,send;
    struct TaskPopover *next;
    int kind; /* 1 Jira comments, 2 local note */
    int body_left,body_right,pointer_y;
    int comment_scroll,comment_scroll_max;
    BOOL updating_scroll,scroll_to_latest;
    BOOL pointer_left,detached,retired;
    BOOL resizing,dragging,transitioning;
    int callback_depth;
    POINT resize_start;
    RECT resize_initial;
    POINT drag_start;
    RECT drag_initial;
    wchar_t key[80],error[256];
    BOOL loading,sending,updating_note,note_save_failed;
    JiraComments comments;
} TaskPopover;
static TaskPopover g_attached_popover;
static TaskPopover *g_detached_popovers;
static TaskPopover *g_active_popover=&g_attached_popover;
#define g_popover (*g_active_popover)

static TaskPopover *FindDetachedPopover(int kind,const wchar_t *key)
{
    for(TaskPopover *item=g_detached_popovers;item;item=item->next)
        if(item->kind==kind&&!_wcsicmp(item->key,key))return item;
    return NULL;
}

static BOOL PopoverOpenForTask(int kind,const wchar_t *key)
{
    return (g_attached_popover.window&&g_attached_popover.kind==kind&&
        !_wcsicmp(g_attached_popover.key,key))||FindDetachedPopover(kind,key)!=NULL;
}
static BOOL g_preserve_popover_refresh;
static int g_popover_width_dip[3]={0,430,350};
static int g_popover_height_dip[3]={0,405,215};

static int D(int value) {
    static const int scales[] = {90, 100};
    int mode = g_density == 1 ? 1 : 0;
    return MulDiv(value, (int)g_dpi * scales[mode], 9600);
}
static void LayoutControls(int width, int height);
static void LayoutStatusFilter(void);
static void UpdateFonts(void);
static void UpdateTooltips(void);
static BOOL SaveSettings(void);
static void UpdateTimer(void);
static void UpdateSettingsLanguage(void);
static void BeginStartEdit(void);
static BOOL StartJiraJob(int action,const wchar_t *key,const wchar_t *transition,POINT anchor);
static BOOL StartJiraJobWithInput(int action,const wchar_t *key,const wchar_t *transition,
    const wchar_t *field,const wchar_t *option,const wchar_t *comment,POINT anchor);
static void FinishJiraJob(JiraJob *job);
static HWND CreateTransitionInputWindow(TransitionInputDialog *dialog,const JiraTransitionItem *transition,const wchar_t *key,BOOL visible);
static BOOL ShowTransitionInputDialog(const JiraTransitionItem *transition,const wchar_t *key,TransitionInputDialog *dialog);
static BOOL CommitNoteEdit(BOOL save);
static void CloseTaskPopover(void);
static void OpenTaskPopover(int task_index,int kind,RECT icon_rect);
static void RefreshTaskPopover(void);
static void RepositionTaskPopover(void);
static int TaskIndexAtRow(int row);
static void ShowTaskContextMenu(POINT screen_point);
static void ShowNewTaskNotification(int count,const JiraItem *first);
static LRESULT CALLBACK StartEditProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
static LRESULT CALLBACK NoteEditProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
static LRESULT CALLBACK ButtonProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
static LRESULT CALLBACK ListProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);

static COLORREF WindowColor(void) { return g_dark ? RGB(32, 33, 36) : RGB(246, 246, 246); }
static COLORREF PanelColor(void) { return g_dark ? RGB(25, 26, 28) : RGB(255, 255, 255); }
static COLORREF PopoverColor(void) { return g_dark ? RGB(31, 35, 40) : RGB(248, 250, 253); }
static COLORREF PopoverHeaderColor(void) { return g_dark ? RGB(39, 51, 64) : RGB(236, 244, 252); }
static COLORREF PopoverBorderColor(void) { return g_dark ? RGB(78, 94, 110) : RGB(167, 190, 212); }
static COLORREF PopoverArrowColor(void) { return g_dark ? RGB(55, 77, 101) : RGB(222, 237, 251); }
static COLORREF TextColor(void) { return g_dark ? RGB(242, 242, 242) : RGB(24, 24, 24); }
static COLORREF MutedColor(void) { return g_dark ? RGB(174, 178, 184) : RGB(103, 103, 103); }
static COLORREF BorderColor(void) { return g_dark ? RGB(58, 62, 68) : RGB(222, 225, 229); }
static COLORREF AccentColor(void) { return g_dark ? RGB(76, 164, 245) : RGB(11, 116, 209); }
static COLORREF ActiveColor(void) { return g_dark ? RGB(55, 47, 31) : RGB(255, 244, 216); }
static COLORREF ActiveLineColor(void) { return g_dark ? RGB(227, 165, 36) : RGB(213, 138, 0); }

static const wchar_t *StatusName(TaskStatus status)
{
    return status >= 0 && status < g_status_count ? g_status_names[status] : L"";
}

static BOOL IsDoneStatus(TaskStatus status)
{
    return status>=0 && status<g_status_count && g_status_categories[status]==2;
}

static BOOL TaskIsPendingSwitch(const TaskItem *task)
{
    return task&&g_pending_switch_key[0]&&!_wcsicmp(task->key,g_pending_switch_key);
}

static const wchar_t *TaskStatusDisplayText(const TaskItem *task)
{
    return task?StatusName(task->status):L"";
}

static int HourglassFrame(ULONGLONG now,ULONGLONG started)
{
    return (int)(((now-started)/180ULL)&1ULL);
}

static BOOL StatusPulseActive(int task_index,ULONGLONG now)
{
    return task_index>=0&&task_index==g_status_pulse_task&&now-g_status_pulse_started<160ULL;
}

static ULONGLONG CurrentFileTime(void)
{
    FILETIME file_time;
    ULARGE_INTEGER value;
    GetSystemTimeAsFileTime(&file_time);
    value.LowPart = file_time.dwLowDateTime;
    value.HighPart = file_time.dwHighDateTime;
    return value.QuadPart;
}

static BOOL FileTimeToLocalSystem(ULONGLONG value,SYSTEMTIME *local_time)
{
    ULARGE_INTEGER integer;FILETIME utc,local;
    integer.QuadPart=value;utc.dwLowDateTime=integer.LowPart;utc.dwHighDateTime=integer.HighPart;
    return FileTimeToLocalFileTime(&utc,&local)&&FileTimeToSystemTime(&local,local_time);
}

static void StartWorkdayForInteractiveSession(void)
{
    SYSTEMTIME now;GetLocalTime(&now);
    if(g_workday_started||now.wHour<WORKDAY_ACTIVATION_HOUR)return;
    g_workday_started=CurrentFileTime();SaveSettings();UpdateTimer();
}

static void InitializeSettingsPath(void)
{
    wchar_t local_app_data[MAX_PATH] = L"";
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,local_app_data)))
        TaskManagerMigrateSettingsAtRoot(local_app_data,g_settings_path,MAX_PATH);
}

static BOOL SaveSettings(void)
{
    wchar_t value[64];
    BOOL saved=TRUE;
    swprintf(value, 64, L"%d", g_density);
    saved=WritePrivateProfileStringW(L"Interface", L"InterfaceSize", value, g_settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Interface", L"Dark", g_dark ? L"1" : L"0", g_settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Interface", L"Russian", g_russian ? L"1" : L"0", g_settings_path)&&saved;
    saved=WritePrivateProfileStringW(L"Timer", L"Text", g_text_time ? L"1" : L"0", g_settings_path)&&saved;
    swprintf(value,64,L"%d",g_comments_mode);
    saved=WritePrivateProfileStringW(L"Interface",L"CommentsMode",value,g_settings_path)&&saved;
    swprintf(value, 64, L"%llu", (unsigned long long)g_workday_started);
    saved=WritePrivateProfileStringW(L"Timer", L"Started", value, g_settings_path)&&saved;
    return saved;
}

static void LoadSettings(void)
{
    wchar_t value[64] = L"";
    g_refresh_minutes=GetPrivateProfileIntW(L"Jira",L"RefreshMinutes",30,g_settings_path);
    if(g_refresh_minutes<1||g_refresh_minutes>1440)g_refresh_minutes=30;
    g_tray_task_count=GetPrivateProfileIntW(L"Jira",L"TrayTaskCount",20,g_settings_path);
    if(g_tray_task_count<1||g_tray_task_count>30)g_tray_task_count=20;
    int legacy = GetPrivateProfileIntW(L"Interface", L"Density", 0, g_settings_path);
    g_density = GetPrivateProfileIntW(L"Interface", L"InterfaceSize", legacy == 2 ? 1 : 0, g_settings_path);
    if (g_density < 0 || g_density > 1) g_density = 0;
    g_dark = GetPrivateProfileIntW(L"Interface", L"Dark", 0, g_settings_path) != 0;
    g_russian = GetPrivateProfileIntW(L"Interface", L"Russian", 1, g_settings_path) != 0;
    g_text_time = GetPrivateProfileIntW(L"Timer", L"Text", 0, g_settings_path) != 0;
    g_comments_mode=GetPrivateProfileIntW(L"Interface",L"CommentsMode",0,g_settings_path);
    if(g_comments_mode<0||g_comments_mode>2)g_comments_mode=0;
    GetPrivateProfileStringW(L"Timer", L"Started", L"0", value, 64, g_settings_path);
    g_workday_started = _wcstoui64(value, NULL, 10);
    SYSTEMTIME now_local,start_local;GetLocalTime(&now_local);
    BOOL valid=g_workday_started&&g_workday_started<=CurrentFileTime()&&FileTimeToLocalSystem(g_workday_started,&start_local)&&
        WorkdayNormalizeStart(&now_local,&start_local,TRUE);
    if (!valid) {
        g_workday_started = 0;
        SaveSettings();
    }
}

static void SetControlFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
}

static HWND CreateOwnerButton(HWND parent, int id, const wchar_t *text)
{
    HWND button = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, g_instance, NULL);
    SetControlFont(button);
    SetWindowSubclass(button, ButtonProcedure, 1, 0);
    return button;
}

static BOOL TaskInTab(const TaskItem *task,int tab)
{
    if(tab<0||tab>2)return FALSE;
    if(g_reported_scope?!task->reported_by_me:!task->assigned_to_me)return FALSE;
    BOOL done=IsDoneStatus(task->status);
    return tab==0||(tab==1&&!done)||(tab==2&&done);
}

static int OtherScopeAttentionCount(void)
{
    int count=0;
    for(int i=0;i<g_task_count;i++){
        const TaskItem *task=&g_tasks[i];
        BOOL only_other=g_reported_scope?(task->assigned_to_me&&!task->reported_by_me):
            (task->reported_by_me&&!task->assigned_to_me);
        if(only_other&&(task->is_new||task->unread_comment))count++;
    }
    return count;
}

static int ActiveFilterIndex(void)
{
    return g_tab+(g_reported_scope?3:0);
}

static int CountForTab(int tab)
{
    int count = 0;
    for (int i = 0; i < g_task_count; ++i)if(TaskInTab(&g_tasks[i],tab))++count;
    return count;
}

static void UpdateTabLabels(void)
{
    HWND tabs[] = { g_tab_all, g_tab_current, g_tab_done };
    const wchar_t *names[] = { g_russian ? L"Все" : L"All", g_russian ? L"Текущие" : L"Current", g_russian ? L"Готово" : L"Done" };
    for (int tab = 0; tab < 3; ++tab) {
        int visible = 0, total = CountForTab(tab);
        for (int i = 0; i < g_task_count; ++i) {
            if(!TaskInTab(&g_tasks[i],tab))continue;
            if (g_tasks[i].active || (g_filter_masks[tab+(g_reported_scope?3:0)] & (1ull << g_tasks[i].status))) ++visible;
        }
        wchar_t label[80];
        if (visible == total) swprintf(label, 80, L"%ls %d", names[tab], total);
        else swprintf(label, 80, L"%ls %d/%d", names[tab], visible, total);
        SetWindowTextW(tabs[tab], label);
    }
}

static void UpdateColumnLabels(void)
{
    wchar_t task_label[64],name_label[64],status_label[64];
    swprintf(task_label,64,L"%ls%ls",g_russian?L"ЗАДАЧА":L"TASK",g_sort_column==1?(g_sort_ascending?L" ↑":L" ↓"):L"");
    swprintf(name_label,64,L"%ls%ls",g_russian?L"НАЗВАНИЕ":L"NAME",g_sort_column==2?(g_sort_ascending?L" ↑":L" ↓"):L"");
    swprintf(status_label,64,L"%ls%ls",g_russian?L"СТАТУС":L"STATUS",g_sort_column==STATUS_COLUMN?(g_sort_ascending?L" ↑":L" ↓"):L"");
    LVCOLUMNW column = { 0 };
    column.mask = LVCF_TEXT;
    column.pszText = L"#";
    ListView_SetColumn(g_list, 0, &column);
    column.pszText = task_label;
    ListView_SetColumn(g_list, 1, &column);
    column.pszText = name_label;
    ListView_SetColumn(g_list, 2, &column);
    column.pszText = g_comments_visual_mode==0?L"":g_comments_visual_mode==2?(g_russian?L"ЗАМЕТКИ":L"NOTES"):(g_russian?L"КОММЕНТАРИИ":L"COMMENTS");
    ListView_SetColumn(g_list, 3, &column);
    column.pszText = status_label;
    ListView_SetColumn(g_list, 4, &column);
    LayoutStatusFilter();
}

static BOOL ContainsInsensitive(const wchar_t *text, const wchar_t *query)
{
    if (!query[0]) return TRUE;
    return StrStrIW(text, query) != NULL;
}

static int CALLBACK CompareTaskItems(LPARAM left_value,LPARAM right_value,LPARAM sort_value)
{
    int left=(int)left_value,right=(int)right_value,column=(int)sort_value;
    if(left<0||left>=g_task_count||right<0||right>=g_task_count)return 0;
    const wchar_t *left_text=column==1?g_tasks[left].key:column==2?g_tasks[left].summary:StatusName(g_tasks[left].status);
    const wchar_t *right_text=column==1?g_tasks[right].key:column==2?g_tasks[right].summary:StatusName(g_tasks[right].status);
    int compared=column==1?StrCmpLogicalW(left_text,right_text):CompareStringOrdinal(left_text,-1,right_text,-1,TRUE)-CSTR_EQUAL;
    if(!compared)compared=StrCmpLogicalW(g_tasks[left].key,g_tasks[right].key);
    return g_sort_ascending?compared:-compared;
}

static void SortTaskList(void)
{
    if(g_sort_column==1||g_sort_column==2||g_sort_column==STATUS_COLUMN)
        ListView_SortItems(g_list,CompareTaskItems,(LPARAM)g_sort_column);
}

static void ApplyTaskSort(int column)
{
    if(column!=1&&column!=2&&column!=STATUS_COLUMN)return;
    if(g_sort_column==column)g_sort_ascending=!g_sort_ascending;
    else{g_sort_column=column;g_sort_ascending=TRUE;}
    SortTaskList();UpdateColumnLabels();InvalidateRect(g_list,NULL,FALSE);
}

static wchar_t *DuplicateWide(const wchar_t *text)
{
    size_t bytes=(wcslen(text)+1)*sizeof(wchar_t);wchar_t *copy=malloc(bytes);
    if(copy)memcpy(copy,text,bytes);return copy;
}

static void FreeTaskItems(void)
{
    for(int i=0;i<g_task_count;i++)free(g_tasks[i].local_note);
    free(g_tasks);g_tasks=NULL;g_task_count=0;
}

static void RefreshTaskList(void)
{
    if(!g_preserve_popover_refresh)CloseTaskPopover();
    wchar_t query[256] = L"";
    GetWindowTextW(g_search, query, 256);
    SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_list);
    int shown = 0;

    for (int i = 0; i < g_task_count; ++i) {
        TaskItem *task = &g_tasks[i];
        BOOL tab_match = TaskInTab(task,g_tab);
        BOOL filter_match = task->active || ((g_filter_masks[ActiveFilterIndex()] & (1ull << task->status)) != 0);
        BOOL search_match = ContainsInsensitive(task->key, query) || ContainsInsensitive(task->summary, query);
        if (!tab_match || !filter_match || !search_match) continue;

        wchar_t number[16];
        swprintf(number, 16, L"%d", shown + 1);
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = shown;
        item.pszText = number;
        item.lParam = i;
        int row = ListView_InsertItem(g_list, &item);
        ListView_SetItemText(g_list, row, 1, (wchar_t *)task->key);
        ListView_SetItemText(g_list, row, 2, (wchar_t *)task->summary);
        const wchar_t *comment=g_comments_visual_mode==1?task->latest_comment:g_comments_visual_mode==2?(task->local_note?task->local_note:L""):L"";
        ListView_SetItemText(g_list, row, COMMENTS_COLUMN, (wchar_t*)comment);
        ListView_SetItemText(g_list, row, STATUS_COLUMN, (wchar_t *)StatusName(task->status));
        ++shown;
    }

    SortTaskList();
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
    g_hover_row = -1;g_hover_status=g_hover_key=g_hover_note=g_hover_comment=FALSE;
    g_pressed_indicator_task=-1;g_pressed_indicator_kind=0;
    g_context_task=-1;
    UpdateTabLabels();
    InvalidateRect(g_filter, NULL, FALSE);
    RECT client;
    GetClientRect(g_window, &client);
    LayoutControls(client.right, client.bottom);
    InvalidateRect(g_list, NULL, FALSE);
}

static void ToggleReportedScope(void)
{
    g_scope_tabs[g_reported_scope?1:0]=g_tab;
    g_reported_scope=!g_reported_scope;
    g_tab=g_scope_tabs[g_reported_scope?1:0];
    SetWindowTextW(g_tab_reported,g_reported_scope?
        (g_russian?L"Поставлены мной":L"Reported by me"):
        (g_russian?L"Назначены мне":L"Assigned to me"));
    RefreshTaskList();UpdateTooltips();
    InvalidateRect(g_tab_all,NULL,TRUE);
    InvalidateRect(g_tab_current,NULL,TRUE);
    InvalidateRect(g_tab_done,NULL,TRUE);
    InvalidateRect(g_tab_reported,NULL,TRUE);
}

static void UpdateTimer(void)
{
    ULONGLONG now = CurrentFileTime();
    if(g_workday_started){
        SYSTEMTIME now_local,start_local;GetLocalTime(&now_local);
        if(!FileTimeToLocalSystem(g_workday_started,&start_local)||!WorkdaySameLocalDate(&now_local,&start_local)){
            g_workday_started=0;SaveSettings();
        }
    }
    ULONGLONG elapsed_seconds = WorkdayElapsedSeconds(g_workday_started/10000000ULL,now/10000000ULL);
    unsigned long long hours = elapsed_seconds / 3600ULL;
    unsigned long long minutes = (elapsed_seconds / 60ULL) % 60ULL;
    unsigned long long seconds = elapsed_seconds % 60ULL;
    wchar_t text[96];
    if (g_text_time) {
        swprintf(text, 96, g_russian ? L"%lluч %lluм %lluс" : L"%lluh %llum %llus", hours, minutes, seconds);
    } else {
        swprintf(text, 96, L"%02llu:%02llu:%02llu", hours, minutes, seconds);
    }
    if(g_workday_started&&elapsed_seconds>=WORKDAY_DURATION_SECONDS)
        wcscat_s(text,_countof(text),g_russian?L" · Пора домой ;)":L" · Time to go home ;)");
    SetWindowTextW(g_timer, text);
}

static void UpdateLastUpdated(void)
{
    wchar_t label[128];
    if(g_jira_busy)wcscpy_s(label,128,g_russian?L"Jira: загрузка…":L"Jira: loading…");
    else if(g_jira_stale)wcscpy_s(label,128,g_russian?L"Jira: ошибка · подробнее в ⋯":L"Jira: error · details in ⋯");
    else swprintf(label,128,g_russian?L"Обновлено %02u:%02u:%02u":L"Updated %02u:%02u:%02u",g_updated_time.wHour,g_updated_time.wMinute,g_updated_time.wSecond);
    SetWindowTextW(g_updated,label);
    if(g_settings){RECT client;GetClientRect(g_window,&client);LayoutControls(client.right,client.bottom);}
}

static void UpdateLanguage(void)
{
    SetWindowTextW(g_language, g_russian ? L"RU" : L"EN");
    SetWindowTextW(g_filter,g_russian?L"Фильтр статусов":L"Status filter");
    SetWindowTextW(g_refresh,g_russian?L"Обновить Jira":L"Refresh Jira");
    SetWindowTextW(g_comments,g_russian?L"Комментарии":L"Comments");
    SetWindowTextW(g_settings,g_russian?L"О приложении и подключении":L"About and connection");
    SetWindowTextW(g_theme,g_russian?L"Переключить тему":L"Switch theme");
    SetWindowTextW(g_tab_reported,g_reported_scope?
        (g_russian?L"Поставлены мной":L"Reported by me"):
        (g_russian?L"Назначены мне":L"Assigned to me"));
    UpdateTabLabels();
    UpdateColumnLabels();
    UpdateLastUpdated();
    UpdateTimer();
    g_preserve_popover_refresh=TRUE;
    RefreshTaskList();
    g_preserve_popover_refresh=FALSE;
    RepositionTaskPopover();
    RefreshTaskPopover();
    if(g_popover.input)InvalidateRect(g_popover.input,NULL,TRUE);
    for(TaskPopover *item=g_detached_popovers;item;item=item->next){
        TaskPopover *previous=g_active_popover;g_active_popover=item;
        RefreshTaskPopover();
        if(item->input)InvalidateRect(item->input,NULL,TRUE);
        g_active_popover=previous;
    }
    UpdateTooltips();
    UpdateSettingsLanguage();
    InvalidateRect(g_window, NULL, TRUE);
}

static BOOL BuildTaskUrl(const wchar_t *base_url,const wchar_t *key,wchar_t *url,size_t capacity)
{
    wchar_t base[768];
    if(!base_url||!*base_url||!key||!*key||!url||capacity<1)return FALSE;
    wcsncpy_s(base,_countof(base),base_url,_TRUNCATE);
    size_t length=wcslen(base);while(length&&base[length-1]==L'/')base[--length]=0;
    return swprintf(url,capacity,L"%ls/browse/%ls",base,key)>=0;
}

static BOOL OpenTaskInJira(const wchar_t *key)
{
    wchar_t base[768],url[1024];
    if(!JiraGetUrl(base,_countof(base))||!BuildTaskUrl(base,key,url,_countof(url)))return FALSE;
    return (INT_PTR)ShellExecuteW(g_window,L"open",url,NULL,NULL,SW_SHOWNORMAL)>32;
}

static void RecreateBrushes(void)
{
    if (g_window_brush) DeleteObject(g_window_brush);
    if (g_panel_brush) DeleteObject(g_panel_brush);
    if (g_popover_brush) DeleteObject(g_popover_brush);
    g_window_brush = CreateSolidBrush(WindowColor());
    g_panel_brush = CreateSolidBrush(PanelColor());
    g_popover_brush = CreateSolidBrush(PopoverColor());
}

static void ApplyWindowCaptionTheme(HWND window)
{
    if(!window)return;
    BOOL dark=g_dark;
    COLORREF caption=WindowColor(),text=TextColor();
    DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    /* DWMWA_CAPTION_COLOR and DWMWA_TEXT_COLOR (Windows 11). */
    DwmSetWindowAttribute(window,35,&caption,sizeof(caption));
    DwmSetWindowAttribute(window,36,&text,sizeof(text));
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);
}

static void ApplyTheme(void)
{
    RecreateBrushes();
    ApplyWindowCaptionTheme(g_window);
    SetWindowTheme(g_list, g_dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    SetWindowTheme(ListView_GetHeader(g_list), g_dark ? L"DarkMode_ItemsView" : L"Explorer", NULL);
    SetWindowTheme(g_search, g_dark ? L"DarkMode_CFD" : L"Explorer", NULL);
    ListView_SetBkColor(g_list, PanelColor());
    ListView_SetTextBkColor(g_list, PanelColor());
    ListView_SetTextColor(g_list, TextColor());
    if(g_popover.window){
        if(g_popover.text)SetWindowTheme(g_popover.text,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        if(g_popover.input)SetWindowTheme(g_popover.input,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        RefreshTaskPopover();
        RedrawWindow(g_popover.window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    }
    for(TaskPopover *item=g_detached_popovers;item;item=item->next){
        if(item->text)SetWindowTheme(item->text,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        if(item->input)SetWindowTheme(item->input,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        TaskPopover *previous=g_active_popover;g_active_popover=item;
        RefreshTaskPopover();g_active_popover=previous;
        RedrawWindow(item->window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    }
    RedrawWindow(g_window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static void LayoutFooterControls(int width,int height)
{
    const int margin=D(10),footer=D(36),footer_y=height-footer+D(4);
    HDC dc=GetDC(g_window);HGDIOBJ old_font=SelectObject(dc,g_timer_font);
    wchar_t timer_text[96]=L"";GetWindowTextW(g_timer,timer_text,_countof(timer_text));
    SIZE timer_size={0};GetTextExtentPoint32W(dc,timer_text,(int)wcslen(timer_text),&timer_size);
    SelectObject(dc,old_font);ReleaseDC(g_window,dc);
    MoveWindow(g_timer,margin,footer_y,timer_size.cx+D(4),D(28),TRUE);
    MoveWindow(g_start_edit,margin,footer_y+D(3),D(65),D(22),TRUE);
    g_footer_target_right=width-margin;
    if(!g_footer_right)g_footer_right=g_footer_target_right;
    if(!g_footer_pinned&&!g_footer_animating)g_footer_right=g_footer_target_right;
    int right=g_footer_right;
    HWND buttons[]={g_settings,g_theme,g_language,g_density_button,g_refresh};
    int widths[]={28,28,32,28,28};
    for(int i=0;i<5;i++){
        right-=D(widths[i]);MoveWindow(buttons[i],right,footer_y,D(widths[i]),D(28),TRUE);
        right-=D(3);
    }
    int label_left=margin+timer_size.cx+D(14);
    int available=max(0,right-label_left-D(8));
    if(!g_jira_busy&&!g_jira_stale){
        wchar_t label[128];swprintf(label,128,g_russian?L"Обновлено %02u:%02u:%02u":L"Updated %02u:%02u:%02u",g_updated_time.wHour,g_updated_time.wMinute,g_updated_time.wSecond);
        HDC measure=GetDC(g_window);HGDIOBJ saved=SelectObject(measure,g_small_font);SIZE size;
        GetTextExtentPoint32W(measure,label,(int)wcslen(label),&size);
        if(size.cx>available)swprintf(label,128,L"%02u:%02u:%02u",g_updated_time.wHour,g_updated_time.wMinute,g_updated_time.wSecond);
        SelectObject(measure,saved);ReleaseDC(g_window,measure);SetWindowTextW(g_updated,label);
    }
    MoveWindow(g_updated,label_left,footer_y,available,D(28),TRUE);
}

static void StatusHeaderRects(HDC dc,RECT cell,const wchar_t *label,RECT *text,RECT *button)
{
    int height=cell.bottom-cell.top;
    int size=max(12,min(D(22),height-D(2))),gap=D(5);
    HGDIOBJ old=SelectObject(dc,g_small_font);SIZE measured={0};
    GetTextExtentPoint32W(dc,label,(int)wcslen(label),&measured);SelectObject(dc,old);
    int text_width=min(measured.cx,max(0,cell.right-cell.left-size-gap-D(6)));
    int left=cell.left+max(0,(cell.right-cell.left-text_width-gap-size)/2);
    *text=(RECT){left,cell.top,left+text_width,cell.bottom};
    *button=(RECT){text->right+gap,cell.top+(height-size)/2,text->right+gap+size,cell.top+(height-size)/2+size};
}

static void LayoutStatusFilter(void)
{
    if(!g_list||!g_filter||!g_small_font)return;
    HWND header=ListView_GetHeader(g_list);RECT cell,text,button;
    if(!Header_GetItemRect(header,STATUS_COLUMN,&cell))return;
    wchar_t label[80]={0};HDITEMW item={0};item.mask=HDI_TEXT;item.pszText=label;item.cchTextMax=80;
    Header_GetItem(header,STATUS_COLUMN,&item);
    HDC dc=GetDC(header);StatusHeaderRects(dc,cell,label,&text,&button);ReleaseDC(header,dc);
    SetWindowPos(g_filter,HWND_TOP,button.left,button.top,button.right-button.left,button.bottom-button.top,SWP_NOACTIVATE);
    InvalidateRect(header,NULL,FALSE);
}

static void LayoutControls(int width, int height)
{
    if (!g_settings || width <= 0 || height <= 0) return;
    const int margin = D(10), gap = D(5), toolbar = D(28), footer = D(36);
    HWND tabs[] = { g_tab_all, g_tab_current, g_tab_done };
    int tab_widths[3], tabs_width = 0;
    HDC dc = GetDC(g_window);
    HGDIOBJ old_font = SelectObject(dc, g_font);
    for (int i = 0; i < 3; ++i) {
        wchar_t label[80]; SIZE size;
        GetWindowTextW(tabs[i], label, 80);
        GetTextExtentPoint32W(dc, label, (int)wcslen(label), &size);
        tab_widths[i] = size.cx + D(14);
        tabs_width += tab_widths[i];
    }
    const int scope_width=D(28),scope_gap=D(9);
    int search_width=max(D(90),width-margin*2-tabs_width-scope_width-gap-scope_gap);
    g_search_frame = (RECT){ margin, margin, margin + search_width, margin + toolbar };
    BOOL has_query = GetWindowTextLengthW(g_search) > 0;
    wchar_t found[64];
    swprintf(found,64,g_russian?L"Найдено: %d":L"Found: %d",ListView_GetItemCount(g_list));
    SetWindowTextW(g_search_count,found);
    HGDIOBJ count_font=SelectObject(dc,g_small_font);
    SIZE count_size;GetTextExtentPoint32W(dc,found,(int)wcslen(found),&count_size);
    SelectObject(dc,count_font);
    int count_width=has_query?count_size.cx+D(8):0;
    TEXTMETRICW metrics;GetTextMetricsW(dc,&metrics);
    MoveWindow(g_search, margin + D(27), margin + (toolbar-metrics.tmHeight)/2, max(D(24),search_width - D(58)-count_width), metrics.tmHeight, TRUE);
    MoveWindow(g_search_count,margin+search_width-D(28)-count_width,margin+D(2),count_width,toolbar-D(4),TRUE);
    ShowWindow(g_search_count,has_query?SW_SHOWNA:SW_HIDE);
    MoveWindow(g_clear_search, margin + search_width - D(26), margin + D(2), D(24), toolbar - D(4), TRUE);
    ShowWindow(g_clear_search, SW_SHOWNA);
    int x = margin + search_width + gap;
    MoveWindow(g_tab_reported,x,margin,scope_width,toolbar,TRUE);
    x+=scope_width;
    g_reported_separator_x=x+D(4);x+=scope_gap;
    for (int i = 0; i < 3; ++i) {
        MoveWindow(tabs[i], x, margin, tab_widths[i], toolbar, TRUE);
        x += tab_widths[i];
    }
    int list_top = margin + toolbar + D(9);
    MoveWindow(g_list, margin, list_top, width - margin * 2, max(0, height - list_top - footer), TRUE);
    RECT list_client;
    GetClientRect(g_list, &list_client);
    /* Keep column geometry independent of whether the vertical scrollbar is showing. */
    int scrollbar=GetSystemMetricsForDpi(SM_CXVSCROLL,g_dpi);
    int list_width=max(0,list_client.right-((GetWindowLongPtrW(g_list,GWL_STYLE)&WS_VSCROLL)?0:scrollbar));
    ListView_SetColumnWidth(g_list, 0, D(28));
    ListView_SetColumnWidth(g_list, 1, D(88));
    int fixed=D(28)+D(88)+D(126),content=max(0,list_width-fixed);
    int comments_width=g_comments_visual_mode?max(D(170),content*45/100)*g_comments_reveal/1000:0;
    RECT outer;GetWindowRect(g_window,&outer);
    int narrow_client=MulDiv(510,(int)g_dpi,96)-((outer.right-outer.left)-width);
    comments_width=min(comments_width,max(0,width-narrow_client));
    if(comments_width>content)comments_width=content;
    ListView_SetColumnWidth(g_list, COMMENTS_COLUMN, comments_width);
    ListView_SetColumnWidth(g_list, STATUS_COLUMN, D(126));
    ListView_SetColumnWidth(g_list, 2, max(0,content-comments_width));
    LayoutStatusFilter();

    SelectObject(dc, old_font);
    ReleaseDC(g_window, dc);
    LayoutFooterControls(width,height);
    InvalidateRect(g_window, NULL, FALSE);
}

static BOOL CursorInFooterGroup(void)
{
    if(!g_refresh||!g_settings)return FALSE;
    RECT left,right;POINT cursor;
    if(!GetWindowRect(g_refresh,&left)||!GetWindowRect(g_settings,&right)||!GetCursorPos(&cursor))return FALSE;
    RECT group={left.left-D(4),min(left.top,right.top)-D(5),right.right+D(4),max(left.bottom,right.bottom)+D(5)};
    return PtInRect(&group,cursor);
}

static int NextAnimatedWidth(int current,int target)
{
    int distance=abs(target-current),step=max(D(12),distance/4);
    return current<target?min(target,current+step):max(target,current-step);
}

static int NextCommentsReveal(int current,int target)
{
    int distance=abs(target-current),step=max(125,distance/4);
    return current<target?min(target,current+step):max(target,current-step);
}

static void LayoutAnimatedFrame(void)
{
    RECT client;GetClientRect(g_window,&client);
    SendMessageW(g_list,WM_SETREDRAW,FALSE,0);
    LayoutControls(client.right,client.bottom);
    SendMessageW(g_list,WM_SETREDRAW,TRUE,0);
    InvalidateRect(g_list,NULL,FALSE);
}

static void AnimateFooterTick(void)
{
    int target_reveal=g_comments_mode?1000:0;
    BOOL reveal_changed=g_comments_reveal!=target_reveal;
    BOOL window_resized=FALSE;
    if(reveal_changed){
        g_comments_reveal=NextCommentsReveal(g_comments_reveal,target_reveal);
        if(!g_comments_reveal&&g_comments_visual_mode){
            g_comments_visual_mode=0;UpdateColumnLabels();RefreshTaskList();
        }
    }
    if(g_window_expand_target){
        RECT outer;GetWindowRect(g_window,&outer);
        int next=NextAnimatedWidth(outer.right-outer.left,g_window_expand_target);
        SetWindowPos(g_window,NULL,0,0,next,outer.bottom-outer.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
        window_resized=TRUE;
        if(next==g_window_expand_target)g_window_expand_target=0;
    }
    if(g_footer_pinned){
        if(CursorInFooterGroup()){
            if(reveal_changed&&!window_resized)LayoutAnimatedFrame();
            RedrawWindow(g_window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_NOERASE);
            return;
        }
        g_footer_pinned=FALSE;g_footer_animating=TRUE;
    }
    if(g_footer_deferred_width){
        RECT outer;GetWindowRect(g_window,&outer);
        int current=outer.right-outer.left,target=g_footer_deferred_width;
        int next=NextAnimatedWidth(current,target);
        RECT client;GetClientRect(g_window,&client);
        if(!g_footer_hold_during_resize)g_footer_right=client.right+(next-current)-D(10);
        SetWindowPos(g_window,NULL,0,0,next,outer.bottom-outer.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
        window_resized=TRUE;
        if(next==target){g_footer_deferred_width=0;g_footer_hold_during_resize=FALSE;}
    }else if(g_footer_right!=g_footer_target_right){
        int distance=g_footer_target_right-g_footer_right;
        int step=max(D(4),abs(distance)/4);
        g_footer_right+=distance>0?min(step,distance):-min(step,-distance);
        RECT client;GetClientRect(g_window,&client);LayoutFooterControls(client.right,client.bottom);
    }
    if(reveal_changed&&!window_resized)LayoutAnimatedFrame();
    RedrawWindow(g_window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_NOERASE);
    if(!g_window_expand_target&&!g_footer_deferred_width&&g_footer_right==g_footer_target_right&&g_comments_reveal==target_reveal){
        g_footer_animating=FALSE;KillTimer(g_window,6);
    }
}

static BOOL FooterShrinkNeedsDeferral(int desired_width,int current_width,int client_width,int footer_right)
{
    if(desired_width>=current_width)return FALSE;
    int narrow_footer_right=client_width-(current_width-desired_width)-D(10);
    return footer_right>narrow_footer_right+D(1);
}

static int CommentsWindowTargetWidth(int mode)
{
    return MulDiv(mode?830:(g_compact_width_dip?g_compact_width_dip:510),(int)g_dpi,96);
}

static void DrawControlIcon(HDC dc, int id, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, max(1, D(1)), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    int r = D(6);
    if (id == IDC_FILTER) {
        POINT points[] = {{cx-r,cy-r},{cx+r,cy-r},{cx+D(2),cy},{cx+D(2),cy+r},{cx-D(2),cy+D(4)},{cx-D(2),cy},{cx-r,cy-r}};
        Polyline(dc, points, 7);
    } else if (id == IDC_COMMENTS) {
        if (g_comments_mode == 2) {
            // Local notes: a pencil, independent of the installed icon font.
            POINT pencil[]={{cx-D(6),cy+D(6)},{cx-D(4),cy+D(1)},
                {cx+D(3),cy-D(6)},{cx+D(6),cy-D(3)},
                {cx-D(1),cy+D(4)},{cx-D(6),cy+D(6)}};
            Polyline(dc,pencil,6);
            MoveToEx(dc,cx-D(4),cy+D(1),NULL);LineTo(dc,cx-D(1),cy+D(4));
        } else {
            POINT bubble[] = {{cx-r,cy-D(5)},{cx+r,cy-D(5)},{cx+r,cy+D(3)},
                {cx,cy+D(3)},{cx-D(4),cy+r},{cx-D(4),cy+D(3)},
                {cx-r,cy+D(3)},{cx-r,cy-D(5)}};
            Polyline(dc,bubble,8);
            if (g_comments_mode == 0) {
                // Hidden comments: the same bubble crossed out.
                MoveToEx(dc,cx-D(7),cy+D(7),NULL);
                LineTo(dc,cx+D(7),cy-D(7));
            }
        }
    } else if (id == IDC_REFRESH) {
        Arc(dc,cx-r,cy-r,cx+r,cy+r,cx+D(2),cy-r,cx+r,cy-D(3));
        POINT arrow[]={{cx+r,cy-r},{cx+r,cy-D(2)},{cx+D(2),cy-D(2)}};
        Polyline(dc,arrow,3);
    } else if (id == IDC_THEME && !g_dark) {
        // A single outline, no filled crescent or font-dependent glyph.
        POINT moon[]={{cx+D(1),cy-r},{cx-D(8),cy-D(5)},{cx-D(7),cy+D(6)},{cx+D(1),cy+r},
            {cx+D(4),cy+r},{cx+D(6),cy+D(4)},{cx+r,cy+D(1)},
            {cx-D(1),cy+D(4)},{cx-D(3),cy-D(3)},{cx+D(1),cy-r}};
        PolyBezier(dc,moon,10);
    } else if (id == IDC_THEME) {
        Ellipse(dc,cx-D(3),cy-D(3),cx+D(3)+1,cy+D(3)+1);
        int rays[][4]={{0,-7,0,-5},{0,5,0,7},{-7,0,-5,0},{5,0,7,0},{-5,-5,-4,-4},{4,4,5,5},{-5,5,-4,4},{4,-4,5,-5}};
        for(int i=0;i<8;i++){MoveToEx(dc,cx+D(rays[i][0]),cy+D(rays[i][1]),NULL);LineTo(dc,cx+D(rays[i][2]),cy+D(rays[i][3]));}
    } else if (id == IDC_SETTINGS) {
        HBRUSH fill=CreateSolidBrush(color); SelectObject(dc,fill);
        for(int i=-1;i<=1;i++) Ellipse(dc,cx+D(i*5)-1,cy-1,cx+D(i*5)+2,cy+2);
        SelectObject(dc,old_brush); DeleteObject(fill);
    } else if (id == IDC_CLEAR_SEARCH) {
        MoveToEx(dc,cx-D(3),cy-D(3),NULL);LineTo(dc,cx+D(4),cy+D(4));
        MoveToEx(dc,cx+D(3),cy-D(3),NULL);LineTo(dc,cx-D(4),cy+D(4));
    } else if (id == IDC_SEARCH) {
        Ellipse(dc,cx-D(5),cy-D(5),cx+D(3),cy+D(3));
        MoveToEx(dc,cx+D(2),cy+D(2),NULL);LineTo(dc,cx+D(6),cy+D(6));
    }
    SelectObject(dc,old_brush); SelectObject(dc,old_pen); DeleteObject(pen);
}

static void DrawScopeIcon(HDC dc,RECT rect,COLORREF color,BOOL reported,BOOL attention)
{
    /* Keep the icon below/left of the badge corner, including a thick high-DPI stroke. */
    int cx=(rect.left+rect.right)/2-D(4),cy=(rect.top+rect.bottom)/2+D(3);
    HPEN pen=CreatePen(PS_SOLID,max(1,D(1)),color);
    HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    if(reported){
        /* Outgoing: paper plane. Native line geometry, no image/font dependency. */
        POINT plane[]={{cx-D(7),cy-D(3)},{cx+D(7),cy-D(7)},
            {cx+D(3),cy+D(7)},{cx-D(1),cy+D(1)},{cx-D(7),cy-D(3)}};
        Polyline(dc,plane,_countof(plane));
        MoveToEx(dc,cx-D(1),cy+D(1),NULL);LineTo(dc,cx+D(7),cy-D(7));
    }else{
        /* Incoming: a simple inbox tray, matching the proposed compact silhouette. */
        POINT tray[]={{cx-D(7),cy},{cx-D(4),cy-D(6)},
            {cx+D(4),cy-D(6)},{cx+D(7),cy},
            {cx+D(7),cy+D(6)},{cx-D(7),cy+D(6)},{cx-D(7),cy}};
        Polyline(dc,tray,_countof(tray));
        POINT lip[]={{cx-D(7),cy},{cx-D(3),cy},
            {cx-D(1),cy+D(2)},{cx+D(1),cy+D(2)},
            {cx+D(3),cy},{cx+D(7),cy}};
        Polyline(dc,lip,_countof(lip));
    }
    SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(pen);
    if(attention){
        HBRUSH dot=CreateSolidBrush(g_dark?RGB(106,187,255):AccentColor());
        HPEN outline=CreatePen(PS_SOLID,1,WindowColor());
        old_brush=SelectObject(dc,dot);old_pen=SelectObject(dc,outline);
        Ellipse(dc,rect.right-D(9),rect.top+D(2),rect.right-D(2),rect.top+D(9));
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(dot);DeleteObject(outline);
    }
}

static void DrawOwnerButton(const DRAWITEMSTRUCT *draw)
{
    RECT rect = draw->rcItem;
    int id = (int)draw->CtlID;
    BOOL tab = id == IDC_TAB_ALL || id == IDC_TAB_CURRENT || id == IDC_TAB_DONE;
    BOOL text_only = id == IDC_TIMER;
    BOOL selected_tab = (id == IDC_TAB_ALL && g_tab == 0) ||
                        (id == IDC_TAB_CURRENT && g_tab == 1) ||
                        (id == IDC_TAB_DONE && g_tab == 2);
    BOOL selected_scope=id==IDC_TAB_REPORTED&&g_reported_scope;
    BOOL hovered = g_hover_button == draw->hwndItem;
    COLORREF background = id == IDC_CLEAR_SEARCH ? PanelColor() : WindowColor();
    if (!text_only && (hovered || selected_tab)) background = g_dark ? RGB(47,51,58) : RGB(233,236,240);
    if (!text_only && (draw->itemState & ODS_SELECTED)) background = g_dark ? RGB(58,65,75) : RGB(219,226,234);
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(draw->hDC, &rect, brush);
    DeleteObject(brush);

    BOOL framed = id==IDC_REFRESH || id==IDC_COMMENTS ||
        id==IDC_DENSITY || id==IDC_THEME || id==IDC_LANGUAGE || id==IDC_SETTINGS;
    if(framed){
        HPEN border=CreatePen(PS_SOLID,1,hovered?AccentColor():(g_dark?RGB(67,76,88):RGB(174,174,174)));
        HGDIOBJ previous_pen=SelectObject(draw->hDC,border);
        HGDIOBJ previous_brush=SelectObject(draw->hDC,GetStockObject(HOLLOW_BRUSH));
        Rectangle(draw->hDC,rect.left,rect.top,rect.right,rect.bottom);
        SelectObject(draw->hDC,previous_brush);SelectObject(draw->hDC,previous_pen);DeleteObject(border);
    }

    if (!text_only && g_keyboard_button==draw->hwndItem && (draw->itemState & ODS_FOCUS)) {
        HPEN pen = CreatePen(PS_SOLID, 1, AccentColor());
        HGDIOBJ old_pen = SelectObject(draw->hDC, pen);
        HGDIOBJ old_brush = SelectObject(draw->hDC, GetStockObject(HOLLOW_BRUSH));
        Rectangle(draw->hDC, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(draw->hDC, old_brush);
        SelectObject(draw->hDC, old_pen);
        DeleteObject(pen);
    }

    wchar_t text[128] = L"";
    GetWindowTextW(draw->hwndItem, text, 128);
    SetBkMode(draw->hDC, TRANSPARENT);
    COLORREF foreground = (tab && selected_tab) || id==IDC_TIMER ? TextColor() : MutedColor();
    if (hovered || selected_scope || (id == IDC_FILTER && g_filter_masks[ActiveFilterIndex()] != ~0ull) || (id==IDC_COMMENTS&&g_comments_mode)) foreground = AccentColor();
    SetTextColor(draw->hDC, foreground);
    HGDIOBJ old_font = SelectObject(draw->hDC, tab ? g_font : id==IDC_TIMER ? g_timer_font : g_small_font);
    if(id==IDC_FILTER){
        SelectObject(draw->hDC,g_symbol_font);
        DrawTextW(draw->hDC,L"\xE71C",1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(g_filter_masks[ActiveFilterIndex()]!=~0ull){
            HBRUSH dot=CreateSolidBrush(AccentColor());
            HGDIOBJ previous=SelectObject(draw->hDC,dot),old_pen=SelectObject(draw->hDC,GetStockObject(NULL_PEN));
            Ellipse(draw->hDC,rect.right-D(6),rect.top+D(2),rect.right-D(2),rect.top+D(6));
            SelectObject(draw->hDC,old_pen);SelectObject(draw->hDC,previous);DeleteObject(dot);
        }
    } else if (id == IDC_COMMENTS || id == IDC_THEME || id == IDC_REFRESH || id == IDC_SETTINGS || id == IDC_CLEAR_SEARCH) {
        DrawControlIcon(draw->hDC,id,(rect.left+rect.right)/2,(rect.top+rect.bottom)/2,foreground);
    } else if (id == IDC_TAB_REPORTED) {
        DrawScopeIcon(draw->hDC,rect,foreground,g_reported_scope,OtherScopeAttentionCount()>0);
    } else {
        UINT alignment = text_only ? DT_LEFT : DT_CENTER;
        DrawTextW(draw->hDC, text, -1, &rect, alignment | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    SelectObject(draw->hDC, old_font);

    if (selected_tab) {
        HBRUSH accent = CreateSolidBrush(AccentColor());
        RECT underline = { rect.left + D(7), rect.bottom - D(2), rect.right - D(7), rect.bottom };
        FillRect(draw->hDC, &underline, accent);
        DeleteObject(accent);
    }

}

static RECT UpdatedTextRect(RECT rect)
{
    rect.right=max(rect.left,rect.right-D(3));
    return rect;
}

static void DrawUpdatedLabel(const DRAWITEMSTRUCT *draw)
{
    RECT bounds=draw->rcItem,rect=UpdatedTextRect(bounds);
    FillRect(draw->hDC,&bounds,g_window_brush);
    SetBkMode(draw->hDC,TRANSPARENT);
    SetTextColor(draw->hDC,MutedColor());
    HGDIOBJ old_font=SelectObject(draw->hDC,g_small_font);
    wchar_t text[128];GetWindowTextW(draw->hwndItem,text,128);
    DrawTextW(draw->hDC,text,-1,&rect,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
    SelectObject(draw->hDC,old_font);
}

static LRESULT CALLBACK ButtonProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    (void)id; (void)data;
    if(message==WM_LBUTTONDOWN){g_keyboard_button=NULL;InvalidateRect(window,NULL,FALSE);}
    else if(message==WM_KEYDOWN || (message==WM_SETFOCUS && (GetKeyState(VK_TAB)&0x8000))){g_keyboard_button=window;InvalidateRect(window,NULL,FALSE);}
    else if(message==WM_KILLFOCUS){if(g_keyboard_button==window)g_keyboard_button=NULL;InvalidateRect(window,NULL,FALSE);}
    if(message==WM_MOUSEMOVE && g_hover_button!=window){
        HWND previous=g_hover_button; g_hover_button=window;
        if(previous) InvalidateRect(previous,NULL,FALSE);
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0}; TrackMouseEvent(&track);
        InvalidateRect(window,NULL,FALSE);
    } else if(message==WM_MOUSELEAVE){
        if(g_hover_button==window) g_hover_button=NULL;
        InvalidateRect(window,NULL,FALSE);
    } else if(message==WM_SETCURSOR){SetCursor(LoadCursorW(NULL,IDC_HAND));return TRUE;}
    return DefSubclassProc(window,message,wp,lp);
}

static void StatusColors(TaskStatus status, COLORREF *background, COLORREF *foreground)
{
    if (g_status_categories[status]==1) {
        *background = RGB(255, 240, 201);
        *foreground = RGB(118, 83, 0);
    } else if (IsDoneStatus(status)) {
        *background = g_dark ? RGB(31, 82, 56) : RGB(221, 242, 227);
        *foreground = g_dark ? RGB(212, 246, 221) : RGB(23, 102, 59);
    } else {
        *background = g_dark ? RGB(33, 74, 115) : RGB(207, 231, 255);
        *foreground = g_dark ? RGB(220, 238, 255) : RGB(22, 74, 120);
    }
}

static COLORREF StatusBackground(int status,BOOL hovered,BOOL pressed)
{
    COLORREF background,foreground;StatusColors((TaskStatus)status,&background,&foreground);
    (void)foreground;
    if(pressed){
        if(g_status_categories[status]==1)return g_dark?RGB(112,78,18):RGB(246,205,118);
        if(IsDoneStatus(status))return g_dark?RGB(48,118,80):RGB(177,222,190);
        return g_dark?RGB(46,103,155):RGB(151,202,253);
    }
    if(hovered){
        if(g_status_categories[status]==1)return RGB(255,228,167);
        if(IsDoneStatus(status))return g_dark?RGB(40,104,70):RGB(196,232,206);
        return g_dark?RGB(40,90,136):RGB(183,219,255);
    }
    return background;
}

static RECT StatusPressedPillRect(RECT pill,BOOL pressed)
{
    if(pressed)OffsetRect(&pill,0,D(1));
    return pill;
}

static void DrawHourglassGlyph(HDC dc,int cx,int cy,int frame,COLORREF color)
{
    int rx=D(4),ry=D(5);HPEN pen=CreatePen(PS_SOLID,max(1,D(1)),color);HGDIOBJ old_pen=SelectObject(dc,pen);
    MoveToEx(dc,cx-rx,cy-ry,NULL);LineTo(dc,cx+rx+1,cy-ry);
    MoveToEx(dc,cx-rx,cy+ry,NULL);LineTo(dc,cx+rx+1,cy+ry);
    MoveToEx(dc,cx-rx,cy-ry,NULL);LineTo(dc,cx,cy);LineTo(dc,cx+rx,cy+ry);
    MoveToEx(dc,cx+rx,cy-ry,NULL);LineTo(dc,cx,cy);LineTo(dc,cx-rx,cy+ry);
    HBRUSH sand=CreateSolidBrush(color);HGDIOBJ old_brush=SelectObject(dc,sand);
    POINT grains[3];
    if(frame==0){grains[0]=(POINT){cx-D(3),cy-ry+D(2)};grains[1]=(POINT){cx+D(3),cy-ry+D(2)};grains[2]=(POINT){cx,cy-D(1)};}
    else {grains[0]=(POINT){cx-D(3),cy+ry-D(2)};grains[1]=(POINT){cx+D(3),cy+ry-D(2)};grains[2]=(POINT){cx,cy+D(1)};}
    Polygon(dc,grains,3);SelectObject(dc,old_brush);DeleteObject(sand);SelectObject(dc,old_pen);DeleteObject(pen);
}

static BOOL StatusPillRectForRow(int row,RECT *pill)
{
    if(!pill||row<0)return FALSE;
    LVITEMW item={0};item.mask=LVIF_PARAM;item.iItem=row;
    if(!ListView_GetItem(g_list,&item)||item.lParam<0||item.lParam>=g_task_count)return FALSE;
    RECT cell;if(!ListView_GetSubItemRect(g_list,row,STATUS_COLUMN,LVIR_BOUNDS,&cell))return FALSE;
    TaskItem *task=&g_tasks[item.lParam];
    HDC dc=GetDC(g_list);if(!dc)return FALSE;
    HGDIOBJ old_font=SelectObject(dc,task->active?g_bold_font:g_font);
    const wchar_t *label=TaskStatusDisplayText(task);
    SIZE size={0};GetTextExtentPoint32W(dc,label,(int)wcslen(label),&size);
    SelectObject(dc,old_font);ReleaseDC(g_list,dc);
    int width=min(size.cx+D(task->active?47:35),cell.right-cell.left-D(4));
    *pill=cell;
    pill->left=cell.left+(cell.right-cell.left-width)/2;
    pill->right=pill->left+width;
    pill->top=cell.top+(cell.bottom-cell.top-D(24))/2;
    pill->bottom=pill->top+D(24);
    return TRUE;
}

static int StatusTaskAtPoint(POINT point)
{
    LVHITTESTINFO hit={0};hit.pt=point;
    int row=ListView_SubItemHitTest(g_list,&hit);
    if(row<0||hit.iSubItem!=STATUS_COLUMN)return -1;
    RECT pill;if(!StatusPillRectForRow(row,&pill)||!PtInRect(&pill,point))return -1;
    LVITEMW item={0};item.mask=LVIF_PARAM;item.iItem=row;
    return ListView_GetItem(g_list,&item)?(int)item.lParam:-1;
}

typedef struct {
    RECT note,comment,new_badge;
    BOOL has_note,has_comment,has_new;
    int text_right,title_end;
} NameIndicatorLayout;

static BOOL NameIndicatorsForRow(int row,NameIndicatorLayout *layout)
{
    if(!layout)return FALSE;
    ZeroMemory(layout,sizeof(*layout));
    int task_index=TaskIndexAtRow(row);
    if(task_index<0||task_index>=g_task_count)return FALSE;
    RECT cell;if(!ListView_GetSubItemRect(g_list,row,2,LVIR_BOUNDS,&cell))return FALSE;
    TaskItem *task=&g_tasks[task_index];
    layout->has_note=task->local_note&&task->local_note[0]&&cell.right-cell.left>=D(55);
    layout->has_comment=task->latest_comment_id&&task->latest_comment_id[0]&&cell.right-cell.left>=D(55);
    layout->has_new=task->is_new&&cell.right-cell.left>=D(105);
    int note_width=layout->has_note?D(16):0,comment_width=layout->has_comment?D(16):0;
    int new_width=layout->has_new?D(g_russian?48:40):0;
    int count=layout->has_note+layout->has_comment+layout->has_new;
    int cluster_width=note_width+comment_width+new_width+max(0,count-1)*D(5);
    int text_left=cell.left+D(7),usable=max(0,cell.right-D(5)-text_left);
    if(layout->has_new&&usable-cluster_width-D(6)<D(20)){
        layout->has_new=FALSE;new_width=0;count--;
        cluster_width=note_width+comment_width+max(0,count-1)*D(5);
    }
    HDC dc=GetDC(g_list);if(!dc)return FALSE;
    HGDIOBJ old_font=SelectObject(dc,task->active?g_bold_font:g_font);
    SIZE title_size={0};GetTextExtentPoint32W(dc,task->summary,(int)wcslen(task->summary),&title_size);
    SelectObject(dc,old_font);ReleaseDC(g_list,dc);
    int max_title=max(0,usable-(count?cluster_width+D(6):0));
    int shown_title=min(title_size.cx,max_title);
    layout->title_end=text_left+shown_title;
    int x=text_left+shown_title+(count?D(6):0);
    layout->text_right=count?x-D(5):cell.right-D(5);
    int icon_size=D(16),icon_top=cell.top+(cell.bottom-cell.top-icon_size)/2;
    if(layout->has_new){
        layout->new_badge=(RECT){x,cell.top+D(8),x+new_width,cell.bottom-D(8)};
        x+=new_width+D(5);
    }
    if(layout->has_comment){
        layout->comment=(RECT){x,icon_top,x+icon_size,icon_top+icon_size};
        x+=icon_size+D(5);
    }
    if(layout->has_note)layout->note=(RECT){x,icon_top,x+icon_size,icon_top+icon_size};
    return TRUE;
}

static BOOL NoteIndicatorRectForRow(int row,RECT *indicator)
{
    NameIndicatorLayout layout;
    if(!indicator||!NameIndicatorsForRow(row,&layout)||!layout.has_note)return FALSE;
    *indicator=layout.note;return TRUE;
}

static BOOL CommentIndicatorRectForRow(int row,RECT *indicator)
{
    NameIndicatorLayout layout;
    if(!indicator||!NameIndicatorsForRow(row,&layout)||!layout.has_comment)return FALSE;
    *indicator=layout.comment;return TRUE;
}

static BOOL NewIndicatorRectForRow(int row,RECT *indicator)
{
    NameIndicatorLayout layout;
    if(!indicator||!NameIndicatorsForRow(row,&layout)||!layout.has_new)return FALSE;
    *indicator=layout.new_badge;return TRUE;
}

static BOOL PopoverFallbackAnchorForRow(int row,RECT *anchor)
{
    NameIndicatorLayout layout;RECT cell;
    if(!anchor||!NameIndicatorsForRow(row,&layout)||
       !ListView_GetSubItemRect(g_list,row,2,LVIR_BOUNDS,&cell))return FALSE;
    int size=D(16),x=min(layout.title_end+D(5),cell.right-size-D(4));
    x=max(cell.left+D(5),x);
    int y=cell.top+(cell.bottom-cell.top-size)/2;
    *anchor=(RECT){x,y,x+size,y+size};return TRUE;
}

static void MarkTaskSeen(int task_index)
{
    if(task_index<0||task_index>=g_task_count||!g_tasks[task_index].is_new)return;
    if(!NativeNewTaskMarkSeen(g_settings_path,g_snapshot.account,g_tasks[task_index].key))return;
    g_tasks[task_index].is_new=FALSE;
    InvalidateRect(g_list,NULL,FALSE);InvalidateRect(g_tab_reported,NULL,FALSE);UpdateTooltips();
}

static void MarkCommentSeen(int task_index)
{
    if(task_index<0||task_index>=g_task_count||!g_tasks[task_index].unread_comment)return;
    if(!NativeCommentMarkSeen(g_settings_path,g_snapshot.account,g_tasks[task_index].key,g_tasks[task_index].latest_comment_id))return;
    g_tasks[task_index].unread_comment=FALSE;g_tasks[task_index].comment_pulse_started=0;
    InvalidateRect(g_list,NULL,FALSE);InvalidateRect(g_tab_reported,NULL,FALSE);UpdateTooltips();
}

static int NoteIndicatorTaskAtPoint(POINT point)
{
    LVHITTESTINFO hit={0};hit.pt=point;int row=ListView_SubItemHitTest(g_list,&hit);
    RECT indicator;if(row<0||hit.iSubItem!=2||!NoteIndicatorRectForRow(row,&indicator))return -1;
    InflateRect(&indicator,D(2),D(2));if(!PtInRect(&indicator,point))return -1;
    return TaskIndexAtRow(row);
}

static int CommentIndicatorTaskAtPoint(POINT point)
{
    LVHITTESTINFO hit={0};hit.pt=point;int row=ListView_SubItemHitTest(g_list,&hit);
    RECT indicator;if(row<0||hit.iSubItem!=2||!CommentIndicatorRectForRow(row,&indicator))return -1;
    InflateRect(&indicator,D(2),D(2));if(!PtInRect(&indicator,point))return -1;
    return TaskIndexAtRow(row);
}

static BOOL TaskNoteInfoTip(int row,wchar_t *text,size_t capacity)
{
    if(!text||capacity<1)return FALSE;text[0]=0;int task_index=TaskIndexAtRow(row);
    if(task_index<0||task_index>=g_task_count)return FALSE;TaskItem *task=&g_tasks[task_index];
    if(!task->local_note||!task->local_note[0]){
        wcscpy_s(text,capacity,g_russian?L"Добавить локальную заметку":L"Add local note");return TRUE;
    }
    wcscpy_s(text,capacity,g_russian?L"Заметка: ":L"Note: ");
    wcsncat_s(text,capacity,task->local_note,_TRUNCATE);return TRUE;
}

static COLORREF IndicatorFillColor(BOOL pressed)
{
    return pressed?(g_dark?RGB(39,125,204):RGB(4,91,172)):AccentColor();
}

static COLORREF IndicatorHoverFillColor(void)
{
    return g_dark?RGB(47,74,101):RGB(215,235,253);
}

static COLORREF IndicatorHoveredActiveColor(void)
{
    return g_dark?RGB(106,187,255):RGB(43,143,230);
}

static void DrawNoteIndicator(HDC dc,RECT rect,BOOL hovered,BOOL pressed,BOOL has_content,BOOL open)
{
    (void)has_content;
    BOOL filled=pressed||open;
    COLORREF ink=filled?IndicatorFillColor(pressed):
        hovered?AccentColor():(g_dark?RGB(126,187,242):RGB(34,119,196));
    COLORREF paper=filled?ink:hovered?IndicatorHoverFillColor():
        (g_dark?RGB(39,56,72):RGB(232,244,255));
    HPEN pen=CreatePen(PS_SOLID,1,ink);HBRUSH fill=CreateSolidBrush(paper);
    HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,fill);
    POINT page[]={{rect.left+D(2),rect.top+D(1)},{rect.right-D(5),rect.top+D(1)},
        {rect.right-D(1),rect.top+D(5)},{rect.right-D(1),rect.bottom-D(2)},
        {rect.left+D(2),rect.bottom-D(2)}};
    Polygon(dc,page,5);
    HPEN detail=CreatePen(PS_SOLID,1,filled?RGB(255,255,255):ink);
    SelectObject(dc,detail);
    MoveToEx(dc,rect.right-D(5),rect.top+D(2),NULL);
    LineTo(dc,rect.right-D(5),rect.top+D(5));LineTo(dc,rect.right-D(2),rect.top+D(5));
    MoveToEx(dc,rect.left+D(5),rect.top+D(8),NULL);LineTo(dc,rect.right-D(4),rect.top+D(8));
    MoveToEx(dc,rect.left+D(5),rect.top+D(11),NULL);LineTo(dc,rect.right-D(4),rect.top+D(11));
    SelectObject(dc,old_brush);SelectObject(dc,old_pen);
    DeleteObject(detail);DeleteObject(fill);DeleteObject(pen);
}

static BOOL CommentPulseActive(const TaskItem *task,ULONGLONG now)
{
    return task&&task->comment_pulse_started&&now>=task->comment_pulse_started&&
        now-task->comment_pulse_started<650ULL;
}

static BOOL ShouldPulseComment(const TaskItem *previous,const TaskItem *current)
{
    return previous&&current&&current->unread_comment&&previous->latest_comment_id&&
        current->latest_comment_id&&wcscmp(previous->latest_comment_id,current->latest_comment_id)!=0;
}

static COLORREF CommentPulseFillColor(const TaskItem *task,ULONGLONG now,BOOL hovered,BOOL pressed,BOOL open)
{
    COLORREF base=pressed?IndicatorFillColor(TRUE):
        hovered&&!open?IndicatorHoveredActiveColor():IndicatorFillColor(FALSE);
    if(!CommentPulseActive(task,now))return base;
    ULONGLONG elapsed=now-task->comment_pulse_started;
    int phase=(int)(elapsed%300ULL);
    double peak=phase<150?phase/150.0:(300-phase)/150.0;
    double blend=0.65*peak*(1.0-(double)elapsed/650.0);
    COLORREF bright=g_dark?RGB(151,209,255):RGB(117,195,255);
    return RGB((int)(GetRValue(base)*(1-blend)+GetRValue(bright)*blend),
        (int)(GetGValue(base)*(1-blend)+GetGValue(bright)*blend),
        (int)(GetBValue(base)*(1-blend)+GetBValue(bright)*blend));
}

static void DrawCommentIndicator(HDC dc,RECT rect,BOOL hovered,BOOL pressed,BOOL has_content,BOOL unread,BOOL open,
    const TaskItem *task)
{
    (void)has_content;
    BOOL filled=unread||pressed||open;
    COLORREF ink=filled?CommentPulseFillColor(task,GetTickCount64(),hovered,pressed,open):
        hovered?AccentColor():(g_dark?RGB(131,145,160):RGB(113,129,146));
    HBRUSH fill=filled?CreateSolidBrush(ink):
        hovered?CreateSolidBrush(IndicatorHoverFillColor()):NULL;
    HPEN outline=CreatePen(PS_SOLID,1,ink);
    HGDIOBJ old_fill=SelectObject(dc,fill?fill:GetStockObject(HOLLOW_BRUSH)),old_pen=SelectObject(dc,outline);
    RoundRect(dc,rect.left+D(1),rect.top+D(1),rect.right-D(1),rect.bottom-D(4),D(6),D(6));
    MoveToEx(dc,rect.left+D(4),rect.bottom-D(4),NULL);
    LineTo(dc,rect.left+D(4),rect.bottom-D(1));
    LineTo(dc,rect.left+D(8),rect.bottom-D(4));
    HPEN detail=CreatePen(PS_SOLID,1,filled?RGB(255,255,255):ink);
    SelectObject(dc,detail);
    MoveToEx(dc,rect.left+D(5),rect.top+D(5),NULL);LineTo(dc,rect.right-D(5),rect.top+D(5));
    MoveToEx(dc,rect.left+D(5),rect.top+D(8),NULL);LineTo(dc,rect.right-D(7),rect.top+D(8));
    SelectObject(dc,old_fill);SelectObject(dc,old_pen);
    if(fill)DeleteObject(fill);DeleteObject(outline);DeleteObject(detail);
}

static LRESULT DrawListCustom(NMLVCUSTOMDRAW *custom)
{
    if (custom->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    if (custom->nmcd.dwDrawStage == CDDS_POSTPAINT) {
        /* The report theme paints column dividers even below the last item.
           Our custom table owns the empty background as well as the rows. */
        RECT empty, header;
        GetClientRect(g_list, &empty);
        GetWindowRect(ListView_GetHeader(g_list), &header);
        MapWindowPoints(NULL, g_list, (POINT *)&header, 2);
        empty.top = header.bottom;
        int count = ListView_GetItemCount(g_list);
        if (count) {
            RECT last;
            if (ListView_GetItemRect(g_list, count - 1, &last, LVIR_BOUNDS))
                empty.top = max(empty.top, last.bottom);
        }
        if (empty.top < empty.bottom) {
            HBRUSH background = CreateSolidBrush(PanelColor());
            FillRect(custom->nmcd.hdc, &empty, background);
            DeleteObject(background);
        }
        return CDRF_DODEFAULT;
    }
    if (custom->nmcd.dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;

    int row = (int)custom->nmcd.dwItemSpec;
    LVITEMW item = { 0 };
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return CDRF_DODEFAULT;
    TaskItem *task = &g_tasks[item.lParam];
    BOOL switching=TaskIsPendingSwitch(task);
    BOOL selected = g_list_keyboard_focus && GetFocus()==g_list &&
        (ListView_GetItemState(g_list,row,LVIS_FOCUSED) & LVIS_FOCUSED) != 0;
    COLORREF row_color = switching ? (g_dark?RGB(35,54,73):RGB(225,240,253)) : task->active ? ActiveColor() : selected ? (g_dark?RGB(42,49,60):RGB(235,241,247))
        : row==g_hover_row ? (g_dark?RGB(33,37,43):RGB(247,249,251)) : PanelColor();
    if(item.lParam==g_context_task){
        COLORREF tint=g_dark?RGB(64,101,145):RGB(153,197,239);double alpha=0.25;
        row_color=RGB((int)(GetRValue(row_color)*(1-alpha)+GetRValue(tint)*alpha),
            (int)(GetGValue(row_color)*(1-alpha)+GetGValue(tint)*alpha),
            (int)(GetBValue(row_color)*(1-alpha)+GetBValue(tint)*alpha));
    }
    if(item.lParam==g_flash_task){
        double alpha=FlashOpacity(GetTickCount64());
        COLORREF tint=g_dark?RGB(64,101,145):RGB(153,197,239);
        row_color=RGB((int)(GetRValue(row_color)*(1-alpha)+GetRValue(tint)*alpha),
            (int)(GetGValue(row_color)*(1-alpha)+GetGValue(tint)*alpha),
            (int)(GetBValue(row_color)*(1-alpha)+GetBValue(tint)*alpha));
    }
    HDC dc=custom->nmcd.hdc;
    RECT bounds; ListView_GetItemRect(g_list,row,&bounds,LVIR_BOUNDS);
    RECT viewport;GetClientRect(g_list,&viewport);
    bounds.right=max(bounds.right,viewport.right);
    HBRUSH row_brush = CreateSolidBrush(row_color);
    FillRect(dc, &bounds, row_brush);
    DeleteObject(row_brush);
    HBRUSH line=CreateSolidBrush(BorderColor());
    RECT separator={bounds.left,bounds.bottom-1,bounds.right,bounds.bottom}; FillRect(dc,&separator,line); DeleteObject(line);
    if(task->active){
        line=CreateSolidBrush(ActiveLineColor());
        RECT marker={bounds.left,bounds.top,bounds.left+D(3),bounds.bottom};FillRect(dc,&marker,line);DeleteObject(line);
    }
    SetBkMode(dc,TRANSPARENT);
    NameIndicatorLayout indicators={0};NameIndicatorsForRow(row,&indicators);
    int x=bounds.left;
    for(int col=0;col<3;col++){
        int column_width=ListView_GetColumnWidth(g_list,col);
        RECT text_rect={x+D(col==0?4:7),bounds.top,x+column_width-D(5),bounds.bottom-1};
        wchar_t number[16];swprintf(number,16,L"%d",row+1);
        const wchar_t *text=col==0?number:col==1?task->key:task->summary;
        if(col==2&&indicators.text_right)text_rect.right=indicators.text_right;
        SetTextColor(dc,col==0?MutedColor():col==1?AccentColor():TextColor());
        HGDIOBJ font=SelectObject(dc,col==0?g_small_font:task->active&&col==2?g_bold_font:g_font);
        DrawTextW(dc,text,-1,&text_rect,(col==0?DT_CENTER:DT_LEFT)|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,font); x+=column_width;
    }
    BOOL note_open=PopoverOpenForTask(2,task->key);
    BOOL comment_open=PopoverOpenForTask(1,task->key);
    if(indicators.has_note)DrawNoteIndicator(dc,indicators.note,row==g_hover_row&&g_hover_note,
        item.lParam==g_pressed_indicator_task&&g_pressed_indicator_kind==2,
        task->local_note&&task->local_note[0],note_open);
    if(indicators.has_comment)DrawCommentIndicator(dc,indicators.comment,row==g_hover_row&&g_hover_comment,
        item.lParam==g_pressed_indicator_task&&g_pressed_indicator_kind==1,
        task->latest_comment_id&&task->latest_comment_id[0],task->unread_comment,comment_open,task);
    if(indicators.has_new){
        HBRUSH badge=CreateSolidBrush(g_dark?RGB(36,72,108):RGB(219,237,255));
        HPEN border=CreatePen(PS_SOLID,1,g_dark?RGB(73,139,199):RGB(143,195,244));
        HGDIOBJ old_badge=SelectObject(dc,badge),old_border=SelectObject(dc,border);
        RoundRect(dc,indicators.new_badge.left,indicators.new_badge.top,indicators.new_badge.right,indicators.new_badge.bottom,D(8),D(8));
        SelectObject(dc,old_badge);SelectObject(dc,old_border);DeleteObject(badge);DeleteObject(border);
        HGDIOBJ badge_font=SelectObject(dc,g_small_font);SetTextColor(dc,g_dark?RGB(216,237,255):RGB(14,94,160));
        DrawTextW(dc,g_russian?L"НОВАЯ":L"NEW",-1,&indicators.new_badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SelectObject(dc,badge_font);
    }
    if(g_comments_mode){
        RECT comment_cell;ListView_GetSubItemRect(g_list,row,COMMENTS_COLUMN,LVIR_BOUNDS,&comment_cell);
        RECT comment_text={comment_cell.left+D(7),comment_cell.top,comment_cell.right-D(5),comment_cell.bottom-1};
        const wchar_t *comment=g_comments_mode==1?task->latest_comment:(task->local_note?task->local_note:L"");
        SetTextColor(dc,MutedColor());HGDIOBJ comment_font=SelectObject(dc,g_font);
        DrawTextW(dc,comment,-1,&comment_text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,comment_font);
    }
    COLORREF unused,status_foreground;StatusColors(task->status,&unused,&status_foreground);
    BOOL status_pressed=item.lParam==g_pressed_status_task||StatusPulseActive((int)item.lParam,GetTickCount64());
    COLORREF status_background=StatusBackground(task->status,row==g_hover_row&&g_hover_status,status_pressed);
    if(switching){status_background=g_dark?RGB(40,105,158):RGB(176,219,255);status_foreground=g_dark?RGB(238,247,255):RGB(13,67,113);}
    HGDIOBJ old_font=SelectObject(dc,task->active?g_bold_font:g_font);
    RECT pill;StatusPillRectForRow(row,&pill);pill=StatusPressedPillRect(pill,status_pressed);
    HBRUSH pill_brush = CreateSolidBrush(status_background);
    HPEN pill_pen = CreatePen(PS_SOLID, 1, status_pressed?AccentColor():task->active?ActiveLineColor():status_background);
    HGDIOBJ old_brush = SelectObject(dc, pill_brush);
    HGDIOBJ old_pen = SelectObject(dc, pill_pen);
    RoundRect(dc, pill.left, pill.top, pill.right, pill.bottom, D(24), D(24));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pill_brush);
    DeleteObject(pill_pen);
    SetTextColor(dc,status_foreground);
    RECT text_rect=pill;text_rect.left+=D(task->active?26:14);text_rect.right-=D(21);
    DrawTextW(dc,TaskStatusDisplayText(task),-1,&text_rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    HPEN glyph_pen=CreatePen(PS_SOLID,1,status_foreground);old_pen=SelectObject(dc,glyph_pen);
    int cy=(pill.top+pill.bottom)/2,cx=pill.right-D(10);
    if(switching)DrawHourglassGlyph(dc,cx,cy,HourglassFrame(GetTickCount64(),g_pending_switch_started),status_foreground);
    else {POINT chevron[]={{cx-D(3),cy-D(1)},{cx,cy+D(2)},{cx+D(3),cy-D(1)}};Polyline(dc,chevron,3);}
    if(task->active){
        int left=pill.left+D(10);POINT play[]={{left,cy-D(3)},{left+D(5),cy},{left,cy+D(3)},{left,cy-D(3)}};Polyline(dc,play,4);
    }
    SelectObject(dc,old_pen);DeleteObject(glyph_pen);SelectObject(dc,old_font);
    if(selected && GetFocus()==g_list){RECT focus=bounds;InflateRect(&focus,-D(4),-D(2));DrawFocusRect(dc,&focus);}
    return CDRF_SKIPDEFAULT;
}

static BOOL ShouldFlashRowCell(int subitem,BOOL over_status_pill)
{
    return !over_status_pill&&!(subitem==COMMENTS_COLUMN&&g_comments_mode==2);
}

static BOOL ShouldClearStatusPress(UINT message)
{
    return message==WM_LBUTTONUP||message==WM_RBUTTONUP||message==WM_CAPTURECHANGED||message==WM_CANCELMODE;
}

static LRESULT CALLBACK ListProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_COMMAND&&LOWORD(wp)==IDC_NOTE_EDIT&&HIWORD(wp)==EN_CHANGE&&g_editing_note){
        wchar_t text[2048];GetWindowTextW(g_note_edit,text,_countof(text));
        NativeNoteSave(g_settings_path,g_snapshot.account,g_note_key,text);
        return 0;
    }
    if(message==WM_CONTEXTMENU){
        POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};
        ShowTaskContextMenu(point);
        return 0;
    }
    if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK || message==WM_RBUTTONDOWN){
        LVHITTESTINFO hit={0};hit.pt.x=(short)LOWORD(lp);hit.pt.y=(short)HIWORD(lp);
        int row=ListView_SubItemHitTest(window,&hit);
        LVITEMW item={0};item.mask=LVIF_PARAM;item.iItem=row;
        if(message==WM_RBUTTONDOWN){
            if(row>=0&&ListView_GetItem(window,&item)){
                g_flash_task=(int)item.lParam;g_flash_button=VK_RBUTTON;
                g_flash_started=GetTickCount64();g_flash_released=0;
                SetTimer(window,2,16,NULL);InvalidateRect(window,NULL,FALSE);UpdateWindow(window);
            }
            goto mouse_press_done;
        }
        int status_task=StatusTaskAtPoint(hit.pt),note_task=NoteIndicatorTaskAtPoint(hit.pt),comment_task=CommentIndicatorTaskAtPoint(hit.pt);
        if(status_task>=0){
            g_pressed_status_task=g_status_pulse_task=status_task;g_status_pulse_started=GetTickCount64();
            SetTimer(window,4,16,NULL);InvalidateRect(window,NULL,FALSE);UpdateWindow(window);
        } else if(note_task>=0||comment_task>=0){
            g_pressed_indicator_task=note_task>=0?note_task:comment_task;
            g_pressed_indicator_kind=note_task>=0?2:1;
            InvalidateRect(window,NULL,FALSE);UpdateWindow(window);
        } else if(note_task<0&&comment_task<0&&row>=0 && ShouldFlashRowCell(hit.iSubItem,status_task>=0) && ListView_GetItem(window,&item)){
            g_flash_task=(int)item.lParam;g_flash_button=VK_LBUTTON;
            g_flash_started=GetTickCount64();g_flash_released=0;
            SetTimer(window,2,16,NULL);
        }
mouse_press_done:;
    } else if(ShouldClearStatusPress(message)){
        if(g_pressed_status_task>=0){g_pressed_status_task=-1;InvalidateRect(window,NULL,FALSE);UpdateWindow(window);}
        if(g_pressed_indicator_task>=0){g_pressed_indicator_task=-1;g_pressed_indicator_kind=0;InvalidateRect(window,NULL,FALSE);}
        if(g_flash_task>=0 && !g_flash_released){
            ULONGLONG now=GetTickCount64();g_flash_release_opacity=FlashOpacity(now);g_flash_released=now;
        }
    } else if(message==WM_TIMER && wp==4){
        if(!StatusPulseActive(g_status_pulse_task,GetTickCount64())){g_status_pulse_task=-1;KillTimer(window,4);}
        InvalidateRect(window,NULL,FALSE);return 0;
    } else if(message==WM_TIMER && wp==5){
        if(!g_pending_switch_key[0])KillTimer(window,5);
        InvalidateRect(window,NULL,FALSE);return 0;
    } else if(message==WM_TIMER && wp==7){
        ULONGLONG now=GetTickCount64();BOOL active=FALSE;
        for(int i=0;i<g_task_count;i++)if(CommentPulseActive(&g_tasks[i],now)){active=TRUE;break;}
        if(!active)KillTimer(window,7);
        InvalidateRect(window,NULL,FALSE);return 0;
    } else if(message==WM_TIMER && wp==2){
        ULONGLONG now=GetTickCount64();
        if(g_flash_task>=0 && !g_flash_released && !(GetAsyncKeyState(g_flash_button)&0x8000)){
            g_flash_release_opacity=FlashOpacity(now);g_flash_released=now;
        }
        if(FlashOpacity(now)<=0 && now>g_flash_started){g_flash_task=-1;KillTimer(window,2);}
        InvalidateRect(window,NULL,FALSE);
        return 0;
    }
    /* Keep the list's selection for navigation, but show it only for keyboard input.
       Clearing LVIS_SELECTED on mouse-up would interfere with double-clicks. */
    if(message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_LBUTTONDBLCLK){
        g_list_keyboard_focus=FALSE;
        InvalidateRect(window,NULL,FALSE);
    } else if(message==WM_KEYDOWN){
        g_list_keyboard_focus=TRUE;
        InvalidateRect(window,NULL,FALSE);
    } else if(message==WM_SETFOCUS){
        g_list_keyboard_focus=(GetKeyState(VK_TAB)&0x8000)!=0;
        InvalidateRect(window,NULL,FALSE);
    } else if(message==WM_KILLFOCUS){
        InvalidateRect(window,NULL,FALSE);
    }
    if(message==WM_NOTIFY && ((NMHDR*)lp)->hwndFrom==ListView_GetHeader(window) &&
        (((NMHDR*)lp)->code==HDN_ITEMCHANGEDW||((NMHDR*)lp)->code==HDN_ITEMCHANGEDA))LayoutStatusFilter();
    if(message==WM_NOTIFY && ((NMHDR*)lp)->code==NM_CUSTOMDRAW){
        NMCUSTOMDRAW *draw=(NMCUSTOMDRAW*)lp;
        if(draw->dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
        if(draw->dwDrawStage==CDDS_POSTPAINT){
            RECT gutter,last;HWND header=ListView_GetHeader(window);GetClientRect(header,&gutter);
            if(Header_GetItemRect(header,STATUS_COLUMN,&last))gutter.left=max(0,last.right);
            if(gutter.left<gutter.right)FillRect(draw->hdc,&gutter,g_window_brush);
            return CDRF_DODEFAULT;
        }
        if(draw->dwDrawStage==CDDS_ITEMPREPAINT){
            RECT rect=draw->rc;
            FillRect(draw->hdc,&rect,g_window_brush);
            wchar_t text[80];HDITEMW header={0};header.mask=HDI_TEXT;header.pszText=text;header.cchTextMax=80;
            Header_GetItem(ListView_GetHeader(window),(int)draw->dwItemSpec,&header);
            HGDIOBJ font=SelectObject(draw->hdc,g_small_font);SetTextColor(draw->hdc,MutedColor());SetBkMode(draw->hdc,TRANSPARENT);
            if(draw->dwItemSpec==STATUS_COLUMN){RECT button;StatusHeaderRects(draw->hdc,rect,text,&rect,&button);}
            else{rect.left+=D(7);rect.right-=D(5);}
            DrawTextW(draw->hdc,text,-1,&rect,(draw->dwItemSpec==0 || draw->dwItemSpec==STATUS_COLUMN?DT_CENTER:DT_LEFT)|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            SelectObject(draw->hdc,font);return CDRF_SKIPDEFAULT;
        }
    }
    if(message==WM_MOUSEMOVE){
        LVHITTESTINFO hit={0};hit.pt.x=(short)LOWORD(lp);hit.pt.y=(short)HIWORD(lp);
        int row=ListView_SubItemHitTest(window,&hit);BOOL status=StatusTaskAtPoint(hit.pt)>=0,key=row>=0&&hit.iSubItem==1,note=NoteIndicatorTaskAtPoint(hit.pt)>=0,comment=CommentIndicatorTaskAtPoint(hit.pt)>=0;
        if(row!=g_hover_row||status!=g_hover_status||key!=g_hover_key||note!=g_hover_note||comment!=g_hover_comment){g_hover_row=row;g_hover_status=status;g_hover_key=key;g_hover_note=note;g_hover_comment=comment;InvalidateRect(window,NULL,FALSE);}
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);
    } else if(message==WM_MOUSELEAVE){g_hover_row=-1;g_hover_status=g_hover_key=g_hover_note=g_hover_comment=FALSE;InvalidateRect(window,NULL,FALSE);}
    else if(message==WM_SETCURSOR && (g_hover_status||g_hover_key||g_hover_note||g_hover_comment)){SetCursor(LoadCursorW(NULL,IDC_HAND));return TRUE;}
    return DefSubclassProc(window,message,wp,lp);
}

static LRESULT CALLBACK HeaderFilterProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_DRAWITEM&&((DRAWITEMSTRUCT*)lp)->CtlID==IDC_FILTER)
        return SendMessageW(g_window,message,wp,lp);
    if(message==WM_COMMAND&&LOWORD(wp)==IDC_FILTER)
        return SendMessageW(g_window,message,wp,lp);
    return DefSubclassProc(window,message,wp,lp);
}

static int TaskIndexAtRow(int row)
{
    if (row < 0) return -1;
    LVITEMW item = { 0 };
    item.mask = LVIF_PARAM;
    item.iItem = row;
    return ListView_GetItem(g_list, &item) ? (int)item.lParam : -1;
}

static BOOL TaskSummaryInfoTip(int row,wchar_t *text,size_t capacity)
{
    if(!text||capacity<1)return FALSE;text[0]=0;
    int task_index=TaskIndexAtRow(row);if(task_index<0||task_index>=g_task_count)return FALSE;
    RECT cell;if(!ListView_GetSubItemRect(g_list,row,2,LVIR_BOUNDS,&cell))return FALSE;
    HDC dc=GetDC(g_list);if(!dc)return FALSE;
    HGDIOBJ old_font=SelectObject(dc,g_tasks[task_index].active?g_bold_font:g_font);SIZE size={0};
    GetTextExtentPoint32W(dc,g_tasks[task_index].summary,(int)wcslen(g_tasks[task_index].summary),&size);
    SelectObject(dc,old_font);ReleaseDC(g_list,dc);
    NameIndicatorLayout indicators={0};NameIndicatorsForRow(row,&indicators);
    int text_right=indicators.text_right?indicators.text_right:cell.right-D(5);
    if(size.cx<=text_right-(cell.left+D(7)))return FALSE;
    wcsncpy_s(text,capacity,g_tasks[task_index].summary,_TRUNCATE);return TRUE;
}

static void ActivateTask(int task_index)
{
    if (task_index < 0 || task_index >= g_task_count) return;
    if(!g_tasks[task_index].assigned_to_me){OpenTaskInJira(g_tasks[task_index].key);return;}
    StartJiraJob(3,g_tasks[task_index].key,NULL,(POINT){0});
}

typedef struct PopupItem { wchar_t text[256]; BOOL checked; BOOL disabled; BOOL separator; BOOL plain; } PopupItem;

static HMENU CreateNativeMenu(void)
{
    HMENU menu=CreatePopupMenu();
    MENUINFO info={0};info.cbSize=sizeof(info);info.fMask=MIM_BACKGROUND;info.hbrBack=g_panel_brush;SetMenuInfo(menu,&info);
    return menu;
}

static void AddNativeMenuItem(HMENU menu, UINT flags, UINT command, const wchar_t *text)
{
    PopupItem *item=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*item));if(!item)return;
    if(text)wcsncpy_s(item->text,256,text,_TRUNCATE);
    item->checked=(flags&MF_CHECKED)!=0;item->disabled=(flags&(MF_DISABLED|MF_GRAYED))!=0;item->separator=(flags&MF_SEPARATOR)!=0;
    MENUITEMINFOW info={0};info.cbSize=sizeof(info);info.fMask=MIIM_FTYPE|MIIM_STATE|MIIM_ID|MIIM_DATA|MIIM_STRING;
    info.fType=MFT_OWNERDRAW;info.fState=item->disabled||item->separator?MFS_DISABLED:MFS_ENABLED;
    info.wID=command;info.dwItemData=(ULONG_PTR)item;info.dwTypeData=item->text;InsertMenuItemW(menu,GetMenuItemCount(menu),TRUE,&info);
}

static void DestroyNativeMenu(HMENU menu)
{
    for(int i=0;i<GetMenuItemCount(menu);i++){
        MENUITEMINFOW info={0};info.cbSize=sizeof(info);info.fMask=MIIM_DATA;
        if(GetMenuItemInfoW(menu,i,TRUE,&info)&&info.dwItemData)HeapFree(GetProcessHeap(),0,(void*)info.dwItemData);
    }
    DestroyMenu(menu);
}

static void MarkNativeMenuPlain(HMENU menu)
{
    for(int i=0;i<GetMenuItemCount(menu);i++){
        MENUITEMINFOW info={0};info.cbSize=sizeof(info);info.fMask=MIIM_DATA;
        if(GetMenuItemInfoW(menu,i,TRUE,&info)&&info.dwItemData)((PopupItem*)info.dwItemData)->plain=TRUE;
    }
}

static HMENU CreateStatusTransitionsMenu(const JiraTransitions *transitions)
{
    HMENU menu=CreateNativeMenu();
    if(transitions)for(int i=0;i<transitions->count;i++)AddNativeMenuItem(menu,MF_STRING,3000+i,transitions->items[i].status_name);
    if(!transitions||!transitions->count)AddNativeMenuItem(menu,MF_DISABLED,0,g_russian?L"Нет доступных переходов":L"No available transitions");
    MarkNativeMenuPlain(menu);return menu;
}

static int TransitionScale(int value){return MulDiv(value,(int)g_dpi,96);}

static int RatingScoreForOption(const wchar_t *value,int fallback)
{
    int stars=0,filled=0;
    if(value)for(const wchar_t *at=value;*at;at++){
        if(*at==L'\x2b50'||*at==L'\x2605'){stars++;filled++;}
        else if(*at==L'\x2606')stars++;
    }
    if(stars)return min(5,filled);
    return max(0,min(5,fallback));
}

static void InvalidateTransitionRatings(TransitionInputDialog *dialog)
{
    for(int i=1;i<=5;i++){
        HWND star=GetDlgItem(dialog->window,IDC_TRANSITION_RATING+i);
        if(star)InvalidateRect(star,NULL,FALSE);
    }
}

static LRESULT CALLBACK TransitionButtonProcedure(HWND button,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;HWND parent=GetParent(button);
    TransitionInputDialog *dialog=(TransitionInputDialog*)GetWindowLongPtrW(parent,GWLP_USERDATA);
    if(message==WM_MOUSEMOVE&&dialog&&dialog->hover!=button){
        if(dialog->hover)InvalidateRect(dialog->hover,NULL,FALSE);dialog->hover=button;
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,button,0};TrackMouseEvent(&track);InvalidateRect(button,NULL,FALSE);InvalidateTransitionRatings(dialog);
    }
    if(message==WM_MOUSELEAVE&&dialog&&dialog->hover==button){dialog->hover=NULL;InvalidateRect(button,NULL,FALSE);InvalidateTransitionRatings(dialog);}
    int control_id=GetDlgCtrlID(button);
    if(message==WM_GETDLGCODE&&control_id>IDC_TRANSITION_RATING&&control_id<IDC_TRANSITION_RATING+6)
        return DefSubclassProc(button,message,wp,lp)|DLGC_WANTARROWS;
    if(message==WM_KEYDOWN&&dialog&&control_id>IDC_TRANSITION_RATING&&control_id<IDC_TRANSITION_RATING+6&&
        (wp==VK_LEFT||wp==VK_RIGHT||wp==VK_HOME||wp==VK_END)){
        int step=wp==VK_LEFT||wp==VK_END?-1:1;
        int score=wp==VK_HOME?0:wp==VK_END?6:control_id-IDC_TRANSITION_RATING;
        for(int i=0;i<5;i++){
            score=(score-1+step+5)%5+1;
            if(dialog->option_by_score[score]>=0){
                dialog->hover=NULL;SendMessageW(parent,WM_COMMAND,MAKEWPARAM(IDC_TRANSITION_RATING+score,BN_CLICKED),0);
                SetFocus(GetDlgItem(parent,IDC_TRANSITION_RATING+score));break;
            }
        }
        return 0;
    }
    if(message==WM_SETCURSOR){SetCursor(LoadCursorW(NULL,IDC_HAND));return TRUE;}
    return DefSubclassProc(button,message,wp,lp);
}

static void DrawTransitionButton(TransitionInputDialog *dialog,const DRAWITEMSTRUCT *draw)
{
    RECT rect=draw->rcItem;int id=(int)draw->CtlID;
    BOOL rating=id>IDC_TRANSITION_RATING&&id<IDC_TRANSITION_RATING+6;
    BOOL hovered=dialog->hover==draw->hwndItem;
    BOOL disabled=(draw->itemState&ODS_DISABLED)!=0;
    COLORREF base=id==IDC_TRANSITION_CLOSE?PopoverHeaderColor():PopoverColor();
    HBRUSH base_brush=CreateSolidBrush(base);FillRect(draw->hDC,&rect,base_brush);DeleteObject(base_brush);
    if(rating){
        int score=dialog->selected_score;
        int hover_id=dialog->hover?GetDlgCtrlID(dialog->hover):0;
        if(hover_id>IDC_TRANSITION_RATING&&hover_id<IDC_TRANSITION_RATING+6&&IsWindowEnabled(dialog->hover))score=hover_id-IDC_TRANSITION_RATING;
        BOOL filled=!disabled&&id-IDC_TRANSITION_RATING<=score;
        COLORREF gold=g_dark?RGB(255,190,66):RGB(213,142,0);
        if(draw->itemState&ODS_SELECTED)gold=g_dark?RGB(240,168,30):RGB(169,109,0);
        COLORREF color=filled?gold:MutedColor();
        int width=rect.right-rect.left,height=rect.bottom-rect.top,scale=3;
        HDC memory=CreateCompatibleDC(draw->hDC);HBITMAP bitmap=memory?CreateCompatibleBitmap(draw->hDC,width*scale,height*scale):NULL;
        HDC dc=bitmap?memory:draw->hDC;HGDIOBJ previous_bitmap=bitmap?SelectObject(memory,bitmap):NULL;
        if(!bitmap)scale=1;
        RECT canvas={0,0,width*scale,height*scale};HBRUSH back=CreateSolidBrush(base);FillRect(dc,&canvas,back);DeleteObject(back);
        const POINT shape[]={{0,-10},{3,-3},{10,-3},{5,2},{6,9},{0,5},{-6,9},{-5,2},{-10,-3},{-3,-3}};
        POINT star[10];int radius=min(width,height)*scale*39/100,cx=width*scale/2,cy=height*scale/2;
        for(int i=0;i<10;i++)star[i]=(POINT){cx+MulDiv(shape[i].x,radius,10),cy+MulDiv(shape[i].y,radius,10)};
        HPEN outline=CreatePen(PS_SOLID,max(1,TransitionScale(1)*scale),color);HBRUSH star_fill=filled?CreateSolidBrush(color):NULL;
        HGDIOBJ old_pen=SelectObject(dc,outline),old_fill=SelectObject(dc,filled?star_fill:GetStockObject(NULL_BRUSH));
        Polygon(dc,star,10);SelectObject(dc,old_pen);SelectObject(dc,old_fill);DeleteObject(outline);if(star_fill)DeleteObject(star_fill);
        if(bitmap){
            int old_mode=SetStretchBltMode(draw->hDC,HALFTONE);POINT origin;SetBrushOrgEx(draw->hDC,0,0,&origin);
            StretchBlt(draw->hDC,rect.left,rect.top,width,height,memory,0,0,width*scale,height*scale,SRCCOPY);
            SetBrushOrgEx(draw->hDC,origin.x,origin.y,NULL);SetStretchBltMode(draw->hDC,old_mode);
            SelectObject(memory,previous_bitmap);DeleteObject(bitmap);
        }
        if(memory)DeleteDC(memory);
        if(draw->itemState&ODS_FOCUS){InflateRect(&rect,-TransitionScale(2),-TransitionScale(2));DrawFocusRect(draw->hDC,&rect);}
        return;
    }
    COLORREF background=base,foreground=disabled?MutedColor():TextColor();
    if(id==IDOK&&!disabled){background=AccentColor();foreground=RGB(255,255,255);}
    else if(hovered)background=PopoverHeaderColor();
    if(draw->itemState&ODS_SELECTED)background=id==IDOK?(g_dark?RGB(34,111,187):RGB(0,96,179)):(g_dark?RGB(55,68,82):RGB(215,231,247));
    if(id==IDOK&&disabled)background=PopoverHeaderColor();
    COLORREF border=background;
    HBRUSH fill=CreateSolidBrush(background);HPEN pen=CreatePen(PS_SOLID,1,border);
    HGDIOBJ old_fill=SelectObject(draw->hDC,fill),old_pen=SelectObject(draw->hDC,pen);
    RoundRect(draw->hDC,rect.left,rect.top,rect.right,rect.bottom,TransitionScale(7),TransitionScale(7));
    SelectObject(draw->hDC,old_fill);SelectObject(draw->hDC,old_pen);DeleteObject(fill);DeleteObject(pen);
    SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,foreground);HGDIOBJ old_font=SelectObject(draw->hDC,rating?dialog->small_font:dialog->font);
    wchar_t text[64];GetWindowTextW(draw->hwndItem,text,_countof(text));
    DrawTextW(draw->hDC,text,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    if(draw->itemState&ODS_FOCUS){InflateRect(&rect,-TransitionScale(3),-TransitionScale(3));DrawFocusRect(draw->hDC,&rect);}
    SelectObject(draw->hDC,old_font);
}

static LRESULT CALLBACK TransitionEditProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;LRESULT result=DefSubclassProc(window,message,wp,lp);
    if(message==WM_PAINT&&GetWindowTextLengthW(window)==0){
        TransitionInputDialog *dialog=(TransitionInputDialog*)GetWindowLongPtrW(GetParent(window),GWLP_USERDATA);
        if(dialog){HDC dc=GetDC(window);RECT rect;SendMessageW(window,EM_GETRECT,0,(LPARAM)&rect);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,MutedColor());HGDIOBJ font=SelectObject(dc,dialog->small_font);
            DrawTextW(dc,dialog->placeholder,-1,&rect,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);
            SelectObject(dc,font);ReleaseDC(window,dc);
        }
    }
    return result;
}

static LRESULT CALLBACK TransitionInputProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    TransitionInputDialog *dialog=(TransitionInputDialog*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_CREATE){
        dialog=(TransitionInputDialog*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)dialog);dialog->window=window;
        ++g_completion_active_count;
        dialog->font=CreateFontW(-TransitionScale(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        dialog->small_font=CreateFontW(-TransitionScale(10),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        dialog->title_font=CreateFontW(-TransitionScale(12),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        HWND label=CreateWindowExW(0,L"STATIC",g_russian?L"Оценка":L"Rating",WS_CHILD|WS_VISIBLE,
            TransitionScale(14),TransitionScale(48),TransitionScale(48),TransitionScale(20),window,(HMENU)(INT_PTR)IDC_TRANSITION_HELPER,g_instance,NULL);
        HWND ratings[6];
        for(int score=1;score<=5;score++){
            wchar_t text[64];
            swprintf(text,_countof(text),g_russian?L"Оценка: %d из 5":L"Rating: %d of 5",score);
            ratings[score]=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                TransitionScale(66+(score-1)*29),TransitionScale(44),TransitionScale(28),TransitionScale(28),window,
                (HMENU)(INT_PTR)(IDC_TRANSITION_RATING+score),g_instance,NULL);
            SendMessageW(ratings[score],WM_SETFONT,(WPARAM)dialog->font,TRUE);
            SetWindowSubclass(ratings[score],TransitionButtonProcedure,1,0);
            EnableWindow(ratings[score],dialog->option_by_score[score]>=0);
        }
        HWND comment=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,
            TransitionScale(21),TransitionScale(87),TransitionScale(388),TransitionScale(50),window,(HMENU)(INT_PTR)IDC_TRANSITION_COMMENT,g_instance,NULL);
        SetWindowSubclass(comment,TransitionEditProcedure,1,0);
        HWND remember=CreateWindowExW(0,L"BUTTON",g_russian?L"Запомнить комментарий":L"Remember comment",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,
            TransitionScale(14),TransitionScale(156),TransitionScale(194),TransitionScale(28),window,(HMENU)(INT_PTR)IDC_TRANSITION_REMEMBER,g_instance,NULL);
        HWND cancel=CreateWindowExW(0,L"BUTTON",g_russian?L"Отмена":L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            TransitionScale(220),TransitionScale(156),TransitionScale(74),TransitionScale(28),window,(HMENU)(INT_PTR)IDCANCEL,g_instance,NULL);
        HWND done=CreateWindowExW(0,L"BUTTON",g_russian?L"Завершить":L"Complete",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            TransitionScale(304),TransitionScale(156),TransitionScale(112),TransitionScale(28),window,(HMENU)(INT_PTR)IDOK,g_instance,NULL);
        HWND close=CreateWindowExW(0,L"BUTTON",L"×",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            TransitionScale(398),TransitionScale(4),TransitionScale(24),TransitionScale(26),window,(HMENU)(INT_PTR)IDC_TRANSITION_CLOSE,g_instance,NULL);
        HWND controls[]={label,comment,cancel,done,close};
        for(int i=0;i<5;i++)SendMessageW(controls[i],WM_SETFONT,(WPARAM)dialog->font,TRUE);
        SendMessageW(remember,WM_SETFONT,(WPARAM)dialog->small_font,TRUE);
        SetWindowSubclass(cancel,TransitionButtonProcedure,1,0);SetWindowSubclass(done,TransitionButtonProcedure,1,0);
        SetWindowSubclass(close,TransitionButtonProcedure,1,0);
        SendMessageW(comment,EM_SETLIMITTEXT,2047,0);
        wchar_t saved[2048];if(NativeCompletionTemplateLoad(g_settings_path,g_snapshot.account,saved,_countof(saved))&&saved[0]){
            SetWindowTextW(comment,saved);SendMessageW(remember,BM_SETCHECK,BST_CHECKED,0);
        }
        EnableWindow(done,FALSE);SetWindowTheme(comment,g_dark?L"DarkMode_CFD":L"Explorer",NULL);
        SetWindowTheme(remember,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        BOOL dark=g_dark;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));return 0;
    }
    if(message==WM_COMMAND&&dialog){
        int id=LOWORD(wp),code=HIWORD(wp);
        if(id>IDC_TRANSITION_RATING&&id<IDC_TRANSITION_RATING+6&&code==BN_CLICKED){
            int score=id-IDC_TRANSITION_RATING;if(dialog->option_by_score[score]<0)return 0;
            dialog->selected_score=score;EnableWindow(GetDlgItem(window,IDOK),TRUE);
            InvalidateTransitionRatings(dialog);return 0;
        }
        if(id==IDOK){
            int score=dialog->selected_score,index=score>=1&&score<=5?dialog->option_by_score[score]:-1;
            if(index<0||index>=dialog->transition->rating_count){MessageBeep(MB_ICONINFORMATION);return 0;}
            wcscpy_s(dialog->option_id,_countof(dialog->option_id),dialog->transition->ratings[index].id);
            GetWindowTextW(GetDlgItem(window,IDC_TRANSITION_COMMENT),dialog->comment,_countof(dialog->comment));
            dialog->remember_comment=SendMessageW(GetDlgItem(window,IDC_TRANSITION_REMEMBER),BM_GETCHECK,0,0)==BST_CHECKED;
            NativeCompletionTemplateSave(g_settings_path,g_snapshot.account,dialog->remember_comment?dialog->comment:L"");
            dialog->accepted=TRUE;DestroyWindow(window);return 0;
        }
        if(id==IDCANCEL||id==IDC_TRANSITION_CLOSE){DestroyWindow(window);return 0;}
        if(id==IDC_TRANSITION_COMMENT&&code==EN_CHANGE)InvalidateRect(GetDlgItem(window,IDC_TRANSITION_COMMENT),NULL,FALSE);
    }
    if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    if(message==WM_DRAWITEM&&dialog){DrawTransitionButton(dialog,(const DRAWITEMSTRUCT*)lp);return TRUE;}
    if(message==WM_DESTROY&&dialog){--g_completion_active_count;if(dialog->font)DeleteObject(dialog->font);if(dialog->small_font)DeleteObject(dialog->small_font);if(dialog->title_font)DeleteObject(dialog->title_font);dialog->font=dialog->small_font=dialog->title_font=NULL;dialog->window=NULL;return 0;}
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORBTN){
        HDC dc=(HDC)wp;BOOL edit=GetDlgCtrlID((HWND)lp)==IDC_TRANSITION_COMMENT;
        SetTextColor(dc,edit?TextColor():MutedColor());SetBkColor(dc,edit?PanelColor():PopoverColor());
        return (LRESULT)(edit?g_panel_brush:g_popover_brush);
    }
    if(message==WM_NCHITTEST){LRESULT hit=DefWindowProcW(window,message,wp,lp);if(hit==HTCLIENT){POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};ScreenToClient(window,&point);if(point.y<TransitionScale(34)&&point.x<TransitionScale(398))return HTCAPTION;}return hit;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT&&dialog){
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);FillRect(dc,&client,g_popover_brush);
        RECT header={0,0,client.right,TransitionScale(34)};HBRUSH fill=CreateSolidBrush(PopoverHeaderColor());FillRect(dc,&header,fill);DeleteObject(fill);
        RECT stripe={TransitionScale(13),TransitionScale(10),TransitionScale(16),TransitionScale(24)};fill=CreateSolidBrush(AccentColor());FillRect(dc,&stripe,fill);DeleteObject(fill);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,TextColor());HGDIOBJ font=SelectObject(dc,dialog->title_font);
        RECT title={TransitionScale(24),0,TransitionScale(285),TransitionScale(34)};
        DrawTextW(dc,g_russian?L"Завершение задачи":L"Complete task",-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,dialog->small_font);SetTextColor(dc,MutedColor());RECT key={TransitionScale(285),0,TransitionScale(387),TransitionScale(34)};
        DrawTextW(dc,dialog->key,-1,&key,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,font);
        HPEN border=CreatePen(PS_SOLID,1,PopoverBorderColor());HGDIOBJ pen=SelectObject(dc,border),brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
        RoundRect(dc,0,0,client.right,client.bottom,TransitionScale(10),TransitionScale(10));
        SelectObject(dc,pen);DeleteObject(border);SelectObject(dc,g_panel_brush);border=CreatePen(PS_SOLID,1,BorderColor());SelectObject(dc,border);
        RoundRect(dc,TransitionScale(14),TransitionScale(80),TransitionScale(416),TransitionScale(144),TransitionScale(8),TransitionScale(8));
        SelectObject(dc,pen);SelectObject(dc,brush);DeleteObject(border);EndPaint(window,&paint);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

static HICON LoadAppIcon(BOOL small)
{
    int width=GetSystemMetrics(small?SM_CXSMICON:SM_CXICON);
    int height=GetSystemMetrics(small?SM_CYSMICON:SM_CYICON);
    HICON icon=(HICON)LoadImageW(g_instance,MAKEINTRESOURCEW(1),IMAGE_ICON,width,height,LR_SHARED);
    return icon?icon:LoadIconW(NULL,IDI_APPLICATION);
}

static HWND CreateTransitionInputWindow(TransitionInputDialog *dialog,const JiraTransitionItem *transition,const wchar_t *key,BOOL visible)
{
    if(!dialog||!transition||!transition->rating_field_id[0]||transition->rating_count<1)return NULL;
    memset(dialog,0,sizeof(*dialog));dialog->transition=transition;dialog->selected_score=-1;
    wcsncpy_s(dialog->key,_countof(dialog->key),key?key:L"",_TRUNCATE);
    const wchar_t *summary=NULL;
    for(int i=0;i<g_task_count;i++)if(!_wcsicmp(g_tasks[i].key,dialog->key)){summary=g_tasks[i].summary;break;}
    if(summary&&summary[0])swprintf(dialog->placeholder,_countof(dialog->placeholder),
        g_russian?L"Комментарий к задаче: %ls":L"Comment on task: %ls",summary);
    else wcscpy_s(dialog->placeholder,_countof(dialog->placeholder),g_russian?L"Комментарий к задаче…":L"Comment on task…");
    for(int i=0;i<6;i++)dialog->option_by_score[i]=-1;
    for(int i=0;i<transition->rating_count;i++){
        int score=RatingScoreForOption(transition->ratings[i].value,i);
        if(dialog->option_by_score[score]<0)dialog->option_by_score[score]=i;
    }
    WNDCLASSW wc={0};wc.style=CS_DROPSHADOW;wc.lpfnWndProc=TransitionInputProcedure;wc.hInstance=g_instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);
    wc.hIcon=LoadAppIcon(FALSE);wc.lpszClassName=L"Task Manager.Transition Input";RegisterClassW(&wc);
    DWORD style=WS_POPUP|WS_CLIPCHILDREN,ex_style=WS_EX_TOOLWINDOW;
    RECT owner={0,0,TransitionScale(430),TransitionScale(194)};if(g_window&&IsWindow(g_window))GetWindowRect(g_window,&owner);
    RECT outer={0,0,TransitionScale(430),TransitionScale(194)};
    AdjustWindowRectExForDpi(&outer,style,FALSE,ex_style,g_dpi);
    int width=outer.right-outer.left,height=outer.bottom-outer.top,x=(owner.left+owner.right-width)/2,y=(owner.top+owner.bottom-height)/2;
    HMONITOR monitor=MonitorFromRect(&owner,MONITOR_DEFAULTTONEAREST);MONITORINFO info={sizeof(info)};
    if(GetMonitorInfoW(monitor,&info)){x=max(info.rcWork.left,min(x,info.rcWork.right-width));y=max(info.rcWork.top,min(y,info.rcWork.bottom-height));}
    HWND window=CreateWindowExW(ex_style,wc.lpszClassName,g_russian?L"Jira Task Manager · Завершение задачи":L"Jira Task Manager · Complete task",
        style,x,y,width,height,g_window,NULL,g_instance,dialog);
    if(window){HRGN region=CreateRoundRectRgn(0,0,width+1,height+1,TransitionScale(10),TransitionScale(10));if(region&&!SetWindowRgn(window,region,FALSE))DeleteObject(region);}
    if(window&&visible){ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(GetDlgItem(window,IDC_TRANSITION_RATING+1));}
    return window;
}

static BOOL ShowTransitionInputDialog(const JiraTransitionItem *transition,const wchar_t *key,TransitionInputDialog *dialog)
{
    HWND window=CreateTransitionInputWindow(dialog,transition,key,FALSE);if(!window)return FALSE;
    EnableWindow(g_window,FALSE);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(GetDlgItem(window,IDC_TRANSITION_RATING+1));
    MSG message;while(IsWindow(window)){
        BOOL result=GetMessageW(&message,NULL,0,0);if(result<=0){if(result==0)PostQuitMessage((int)message.wParam);break;}
        if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    EnableWindow(g_window,TRUE);SetForegroundWindow(g_window);return dialog->accepted;
}

static void DrawNativeMenuItem(const DRAWITEMSTRUCT *draw)
{
    PopupItem *item=(PopupItem*)draw->itemData;if(!item)return;
    RECT rect=draw->rcItem;
    COLORREF background=(draw->itemState&ODS_SELECTED)?(g_dark?RGB(45,59,77):RGB(227,238,250)):PanelColor();
    HBRUSH brush=CreateSolidBrush(background);FillRect(draw->hDC,&rect,brush);DeleteObject(brush);
    if(item->separator){
        brush=CreateSolidBrush(BorderColor());RECT line={rect.left+D(8),(rect.top+rect.bottom)/2,rect.right-D(8),(rect.top+rect.bottom)/2+1};
        FillRect(draw->hDC,&line,brush);DeleteObject(brush);return;
    }
    COLORREF color=item->disabled?MutedColor():TextColor();
    HGDIOBJ font=SelectObject(draw->hDC,g_font);SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,color);
    if(item->checked){
        HPEN pen=CreatePen(PS_SOLID,max(1,D(1)),color);HGDIOBJ previous=SelectObject(draw->hDC,pen);
        int x=rect.left+D(9),y=(rect.top+rect.bottom)/2;POINT check[]={{x,y},{x+D(3),y+D(3)},{x+D(8),y-D(4)}};
        Polyline(draw->hDC,check,3);SelectObject(draw->hDC,previous);DeleteObject(pen);
    }
    rect.left+=D(item->plain?10:28);rect.right-=D(10);DrawTextW(draw->hDC,item->text,-1,&rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(draw->hDC,font);
}

static void ShowStatusMenu(int task_index, POINT screen_point)
{
    if (task_index < 0 || task_index >= g_task_count) return;
    StartJiraJob(1,g_tasks[task_index].key,NULL,screen_point);
}

static void ShowFilterMenu(void)
{
    if(g_filter_menu_open)return;
    HMENU menu = CreateNativeMenu();
    for (int i = 0; i < g_status_count; ++i) {
        int count=0;
        for(int j=0;j<g_task_count;j++){
            if(!TaskInTab(&g_tasks[j],g_tab))continue;
            if(g_tasks[j].status==i)count++;
        }
        if(!count)continue;
        wchar_t label[96];swprintf(label,96,L"%ls  ·  %d",StatusName((TaskStatus)i),count);
        UINT flags = MF_STRING | ((g_filter_masks[ActiveFilterIndex()] & (1ull << i)) ? MF_CHECKED : MF_UNCHECKED);
        AddNativeMenuItem(menu, flags, IDM_FILTER_BASE + i, label);
    }
    RECT button_rect;
    GetWindowRect(g_filter, &button_rect);
    g_filter_menu_open=TRUE;
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN,
        button_rect.right, button_rect.bottom, 0, g_window, NULL);
    g_filter_menu_open=FALSE;
    POINT cursor;GetCursorPos(&cursor);
    if(!command&&PtInRect(&button_rect,cursor))g_filter_dismissed_tick=GetTickCount();
    DestroyNativeMenu(menu);
    if (command >= IDM_FILTER_BASE && command < IDM_FILTER_BASE + g_status_count) {
        int status = command - IDM_FILTER_BASE;
        g_filter_masks[ActiveFilterIndex()] ^= 1ull << status;
        RefreshTaskList();
    }
}

static int TaskByKey(const wchar_t *key)
{
    for(int i=0;i<g_task_count;i++)if(!_wcsicmp(g_tasks[i].key,key))return i;return -1;
}

static BOOL CommitNoteEdit(BOOL save)
{
    if(!g_editing_note)return TRUE;
    wchar_t text[2048];GetWindowTextW(g_note_edit,text,_countof(text));
    // Disarm focus notifications while the save or error dialog runs.
    g_editing_note=FALSE;
    BOOL stored=NativeNoteSave(g_settings_path,g_snapshot.account,g_note_key,save?text:g_note_original);
    if(!stored){
        DWORD error=GetLastError();wchar_t details[80];
        swprintf(details,_countof(details),L"error=%lu",(unsigned long)error);
        NativeLifecycleEvent(&g_lifecycle,L"note_save_failed",details);
        MessageBoxW(g_window,g_russian?L"Не удалось сохранить заметку на этом компьютере. Текст остался в редакторе; проверьте доступ к папке данных и повторите сохранение.":L"Could not save the note on this computer. The text remains in the editor; check access to the data folder and try again.",APP_TITLE,MB_OK|MB_ICONWARNING);
        g_editing_note=TRUE;
        SetFocus(g_note_edit);return FALSE;
    }
    ShowWindow(g_note_edit,SW_HIDE);
    int index=TaskByKey(g_note_key);
    if(save&&index>=0&&stored){
        free(g_tasks[index].local_note);g_tasks[index].local_note=text[0]?DuplicateWide(text):NULL;
    }
    g_note_key[0]=0;RefreshTaskList();return TRUE;
}

static void BeginNoteEdit(int row,int task_index)
{
    if(g_comments_mode!=2||row<0||task_index<0||task_index>=g_task_count)return;
    if(!CommitNoteEdit(TRUE))return;TaskItem *task=&g_tasks[task_index];
    RECT cell;if(!ListView_GetSubItemRect(g_list,row,COMMENTS_COLUMN,LVIR_BOUNDS,&cell))return;
    wcscpy_s(g_note_original,_countof(g_note_original),task->local_note?task->local_note:L"");
    wcscpy_s(g_note_key,_countof(g_note_key),task->key);SetWindowTextW(g_note_edit,task->local_note?task->local_note:L"");
    MoveWindow(g_note_edit,cell.left+D(2),cell.top+D(2),max(1,cell.right-cell.left-D(4)),max(1,cell.bottom-cell.top-D(4)),TRUE);
    g_editing_note=TRUE;ShowWindow(g_note_edit,SW_SHOW);SetFocus(g_note_edit);SendMessageW(g_note_edit,EM_SETSEL,0,-1);
}

static void ApplyCommentsMode(int mode)
{
    if(mode<0||mode>2||mode==g_comments_mode)return;
    if(!CommitNoteEdit(TRUE))return;
    RECT window;GetWindowRect(g_window,&window);
    if(!g_comments_mode&&mode&&!(g_footer_deferred_width||g_window_expand_target))
        g_compact_width_dip=MulDiv(window.right-window.left,96,(int)g_dpi);
    if(mode)g_comments_visual_mode=mode;
    g_comments_mode=mode;SaveSettings();
    int desired=CommentsWindowTargetWidth(mode);
    BOOL shrinking=desired<window.right-window.left;
    RECT client;GetClientRect(g_window,&client);
    BOOL footer_fits_compact=shrinking&&!FooterShrinkNeedsDeferral(desired,window.right-window.left,client.right,g_footer_right);
    g_window_expand_target=desired>window.right-window.left?desired:0;
    g_footer_hold_during_resize=footer_fits_compact;
    if(CursorInFooterGroup()&&!footer_fits_compact){
        g_footer_pinned=TRUE;g_footer_animating=FALSE;
        g_footer_deferred_width=shrinking?desired:0;
        SetTimer(g_window,6,20,NULL);
    }else{
        g_footer_pinned=FALSE;g_footer_animating=TRUE;
        g_footer_deferred_width=shrinking?desired:0;
    }
    SetTimer(g_window,6,20,NULL);
    UpdateColumnLabels();RefreshTaskList();UpdateTooltips();InvalidateRect(g_comments,NULL,TRUE);
    if(mode==1)StartJiraJob(0,NULL,NULL,(POINT){0});
}

static void ToggleLocalNotesForTask(int task_index)
{
    if(task_index<0||task_index>=g_task_count)return;TaskItem *task=&g_tasks[task_index];
    if(g_comments_mode==2){
        if(g_note_mode_anchor_key[0]&&!_wcsicmp(task->key,g_note_mode_anchor_key)){
            if(!CommitNoteEdit(TRUE))return;
            int target=g_note_return_mode==1?1:0;g_note_mode_anchor_key[0]=0;ApplyCommentsMode(target);return;
        }
        for(int row=0;row<ListView_GetItemCount(g_list);row++)if(TaskIndexAtRow(row)==task_index){
            BeginNoteEdit(row,task_index);break;
        }
        return;
    }
    if(!task->local_note||!task->local_note[0])return;
    g_note_return_mode=g_comments_mode;wcsncpy_s(g_note_mode_anchor_key,_countof(g_note_mode_anchor_key),task->key,_TRUNCATE);
    ApplyCommentsMode(2);
}

static void ShowTaskContextMenu(POINT screen_point)
{
    int row=-1;
    if(screen_point.x==-1&&screen_point.y==-1){
        row=ListView_GetNextItem(g_list,-1,LVNI_FOCUSED);
        if(row<0)return;
        RECT item;if(!ListView_GetItemRect(g_list,row,&item,LVIR_BOUNDS))return;
        screen_point=(POINT){item.left+D(20),item.top+(item.bottom-item.top)/2};
        ClientToScreen(g_list,&screen_point);
    }else{
        POINT client=screen_point;ScreenToClient(g_list,&client);
        LVHITTESTINFO hit={0};hit.pt=client;
        row=ListView_SubItemHitTest(g_list,&hit);
        if(row<0)return;
    }
    int task_index=TaskIndexAtRow(row);
    if(task_index<0)return;
    g_context_task=task_index;InvalidateRect(g_list,NULL,FALSE);UpdateWindow(g_list);
    wchar_t key[80];wcscpy_s(key,_countof(key),g_tasks[task_index].key);
    BOOL has_note=g_tasks[task_index].local_note&&g_tasks[task_index].local_note[0];
    BOOL has_comments=g_tasks[task_index].latest_comment_id&&g_tasks[task_index].latest_comment_id[0];
    HMENU menu=CreateNativeMenu();
    MENUINFO layout={0};layout.cbSize=sizeof(layout);layout.fMask=MIM_STYLE;layout.dwStyle=MNS_NOCHECK;
    SetMenuInfo(menu,&layout);
    AddNativeMenuItem(menu,MF_STRING,IDM_TASK_NOTE,
        has_note?(g_russian?L"Изменить заметку":L"Edit note"):(g_russian?L"Добавить заметку":L"Add note"));
    AddNativeMenuItem(menu,MF_STRING,IDM_TASK_COMMENTS,
        has_comments?(g_russian?L"Комментарии Jira…":L"Jira comments…"):(g_russian?L"Написать Jira комментарий":L"Add comment"));
    MarkNativeMenuPlain(menu);
    SetForegroundWindow(g_window);
    int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
        screen_point.x,screen_point.y,0,g_window,NULL);
    DestroyNativeMenu(menu);
    g_context_task=-1;InvalidateRect(g_list,NULL,FALSE);
    if(command!=IDM_TASK_NOTE&&command!=IDM_TASK_COMMENTS)return;
    task_index=TaskByKey(key);if(task_index<0)return;
    MarkTaskSeen(task_index);
    for(int i=0;i<ListView_GetItemCount(g_list);i++)if(TaskIndexAtRow(i)==task_index){
        ListView_EnsureVisible(g_list,i,FALSE);
        RECT icon;BOOL found=command==IDM_TASK_NOTE?NoteIndicatorRectForRow(i,&icon):CommentIndicatorRectForRow(i,&icon);
        if(!found)found=PopoverFallbackAnchorForRow(i,&icon);
        if(found)OpenTaskPopover(task_index,command==IDM_TASK_NOTE?2:1,icon);
        break;
    }
}

static int NextCommentsMode(int mode)
{
    return mode==0?1:mode==1?2:0;
}

static void CycleCommentsMode(void)
{
    if(!CommitNoteEdit(TRUE))return;
    g_note_mode_anchor_key[0]=0;
    ApplyCommentsMode(NextCommentsMode(g_comments_mode));
}

static LRESULT CALLBACK NoteEditProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN&&wp==VK_RETURN){if(CommitNoteEdit(TRUE))SetFocus(g_list);return 0;}
    if(message==WM_KEYDOWN&&wp==VK_ESCAPE){if(CommitNoteEdit(FALSE))SetFocus(g_list);return 0;}
    if(message==WM_KILLFOCUS&&g_editing_note){CommitNoteEdit(TRUE);return 0;}
    return DefSubclassProc(window,message,wp,lp);
}

static TaskPopover *EnterPopoverContext(TaskPopover *popover)
{
    TaskPopover *previous=g_active_popover;
    g_active_popover=popover;
    popover->callback_depth++;
    return previous;
}

static void LeavePopoverContext(TaskPopover *popover,TaskPopover *previous)
{
    popover->callback_depth--;
    g_active_popover=previous==popover&&popover->retired?&g_attached_popover:previous;
    if(popover->retired&&popover->callback_depth==0&&popover!=&g_attached_popover)free(popover);
}

static TaskPopover *PopoverForChild(HWND window)
{
    HWND parent=GetParent(window);
    TaskPopover *popover=parent?(TaskPopover*)GetWindowLongPtrW(parent,GWLP_USERDATA):NULL;
    return popover?popover:&g_attached_popover;
}

static void UnlinkDetachedPopover(TaskPopover *popover)
{
    TaskPopover **link=&g_detached_popovers;
    while(*link&&*link!=popover)link=&(*link)->next;
    if(*link==popover)*link=popover->next;
    popover->next=NULL;
}

static LRESULT PopoverEditProcedureInner(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_KEYDOWN&&wp==VK_ESCAPE){PostMessageW(g_popover.window,WM_CLOSE,0,0);return 0;}
    if(message==WM_KEYDOWN&&wp==VK_RETURN&&(GetKeyState(VK_CONTROL)&0x8000)&&g_popover.kind==1){
        PostMessageW(g_popover.window,WM_COMMAND,IDC_POPOVER_SEND,0);return 0;
    }
    LRESULT result=DefSubclassProc(window,message,wp,lp);
    if(message==WM_PAINT&&g_popover.kind==1&&window==g_popover.input&&GetWindowTextLengthW(window)==0){
        HDC dc=GetDC(window);if(dc){
            RECT rect;GetClientRect(window,&rect);rect.left+=D(8);rect.top+=D(4);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,MutedColor());
            HGDIOBJ previous=SelectObject(dc,g_small_font);
            DrawTextW(dc,g_russian?L"Написать комментарий…":L"Write a comment…",-1,&rect,DT_LEFT|DT_TOP|DT_SINGLELINE);
            SelectObject(dc,previous);ReleaseDC(window,dc);
        }
    }
    return result;
}

static LRESULT CALLBACK PopoverEditProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    TaskPopover *popover=PopoverForChild(window),*previous=EnterPopoverContext(popover);
    LRESULT result=PopoverEditProcedureInner(window,message,wp,lp,id,data);
    LeavePopoverContext(popover,previous);return result;
}

static void CloseTaskPopover(void)
{
    if(!g_popover.window)return;
    if(g_popover.kind==2&&g_popover.note_save_failed)return;
    TaskPopover *closing=g_active_popover;
    HWND window=g_popover.window;g_popover.window=NULL;
    DestroyWindow(window);JiraFreeComments(&g_popover.comments);
    if(closing==&g_attached_popover){
        int depth=closing->callback_depth;
        ZeroMemory(closing,sizeof(*closing));closing->callback_depth=depth;
    }else{
        UnlinkDetachedPopover(closing);closing->retired=TRUE;
        if(closing->callback_depth==0){g_active_popover=&g_attached_popover;free(closing);}
    }
    if(g_window&&g_settings){RECT client;GetClientRect(g_window,&client);LayoutFooterControls(client.right,client.bottom);}
    if(g_list)InvalidateRect(g_list,NULL,FALSE);
}

static void FormatCommentDate(const wchar_t *created,wchar_t *date,size_t capacity)
{
    if(wcslen(created)>=16)
        swprintf(date,capacity,L"%.2ls.%.2ls.%.4ls  %.2ls:%.2ls",
            created+8,created+5,created,created+11,created+14);
    else wcsncpy_s(date,capacity,created,_TRUNCATE);
}

static int CommentCardHeight(HDC dc,const JiraComment *item,int list_width)
{
    int text_width=max(D(30),list_width-D(63));
    RECT measured={0,0,text_width,0};
    HGDIOBJ old=SelectObject(dc,g_font);
    DrawTextW(dc,item->body[0]?item->body:L" ",-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(dc,old);
    return max(D(61),measured.bottom+D(42));
}

static int CommentContentHeight(HDC dc,int width)
{
    if(g_popover.loading||g_popover.comments.count==0)return 0;
    int height=D(8);
    if(g_popover.error[0])height+=D(54);
    for(int i=0;i<g_popover.comments.count;i++)
        height+=CommentCardHeight(dc,&g_popover.comments.items[i],width);
    return height+D(2);
}

static void UpdateCommentScroll(void)
{
    if(!g_popover.text||g_popover.kind!=1||g_popover.updating_scroll)return;
    g_popover.updating_scroll=TRUE;
    RECT client;GetClientRect(g_popover.text,&client);
    HDC dc=GetDC(g_popover.text);
    if(dc){
        int content=CommentContentHeight(dc,client.right);
        BOOL needs_scroll=content>client.bottom;
        BOOL visible=(GetWindowLongPtrW(g_popover.text,GWL_STYLE)&WS_VSCROLL)!=0;
        if(visible!=needs_scroll)ShowScrollBar(g_popover.text,SB_VERT,needs_scroll);
        GetClientRect(g_popover.text,&client);
        content=CommentContentHeight(dc,client.right);
        g_popover.comment_scroll_max=max(0,content-client.bottom);
        g_popover.comment_scroll=min(g_popover.comment_scroll,g_popover.comment_scroll_max);
        SCROLLINFO scroll={sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS,0,max(0,content-1),
            (UINT)max(1,client.bottom),g_popover.comment_scroll,0};
        SetScrollInfo(g_popover.text,SB_VERT,&scroll,!g_popover.resizing);
        ReleaseDC(g_popover.text,dc);
    }
    g_popover.updating_scroll=FALSE;
    InvalidateRect(g_popover.text,NULL,FALSE);
}

static void PaintCommentList(HWND window,HDC dc,RECT client)
{
    FillRect(dc,&client,g_popover_brush);
    SetBkMode(dc,TRANSPARENT);
    if(g_popover.loading||g_popover.comments.count==0){
        const wchar_t *message=g_popover.loading?(g_russian?L"Загрузка комментариев…":L"Loading comments…"):
            g_popover.error[0]?g_popover.error:(g_russian?L"Комментариев пока нет":L"No comments yet");
        SetTextColor(dc,g_popover.error[0]&&!g_popover.loading?
            (g_dark?RGB(255,177,146):RGB(157,64,38)):MutedColor());
        HGDIOBJ old=SelectObject(dc,g_font);
        RECT empty={D(18),D(18),client.right-D(18),client.bottom-D(18)};
        DrawTextW(dc,message,-1,&empty,DT_CENTER|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,old);return;
    }
    int y=D(8)-g_popover.comment_scroll;
    if(g_popover.error[0]){
        RECT warning={D(10),y,client.right-D(10),y+D(46)};
        HBRUSH brush=CreateSolidBrush(g_dark?RGB(65,43,38):RGB(255,240,235));
        FillRect(dc,&warning,brush);DeleteObject(brush);
        SetTextColor(dc,g_dark?RGB(255,177,146):RGB(157,64,38));
        HGDIOBJ font=SelectObject(dc,g_small_font);
        InflateRect(&warning,-D(9),-D(4));
        DrawTextW(dc,g_popover.error,-1,&warning,DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,font);y+=D(54);
    }
    for(int i=0;i<g_popover.comments.count;i++){
        const JiraComment *item=&g_popover.comments.items[i];
        int height=CommentCardHeight(dc,item,client.right),top=y;y+=height;
        if(y<0||top>client.bottom)continue;
        BOOL own=JiraCommentIsOwn(item,&g_snapshot);
        if(own){
            HBRUSH card=CreateSolidBrush(g_dark?RGB(35,53,73):RGB(230,241,254));
            HGDIOBJ brush=SelectObject(dc,card),pen=SelectObject(dc,GetStockObject(NULL_PEN));
            RoundRect(dc,D(5),top+D(2),client.right-D(5),y-D(4),D(10),D(10));
            SelectObject(dc,pen);SelectObject(dc,brush);DeleteObject(card);
        }
        int avatar_x=D(10),avatar_y=top+D(8),text_x=D(49),text_right=client.right-D(14);
        COLORREF avatar_color=own?(g_dark?RGB(48,75,105):RGB(205,226,251)):
            (g_dark?RGB(48,53,60):RGB(232,236,242));
        COLORREF avatar_text=own?(g_dark?RGB(195,222,255):RGB(31,104,183)):MutedColor();
        HBRUSH avatar_brush=CreateSolidBrush(avatar_color);HGDIOBJ old_brush=SelectObject(dc,avatar_brush);
        HGDIOBJ old_pen=SelectObject(dc,GetStockObject(NULL_PEN));
        Ellipse(dc,avatar_x,avatar_y,avatar_x+D(28),avatar_y+D(28));
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(avatar_brush);
        const wchar_t *name=item->author[0]?item->author:(g_russian?L"Пользователь Jira":L"Jira user");
        wchar_t initials[3]={name[0]?name[0]:L'J',0,0};
        SetTextColor(dc,avatar_text);HGDIOBJ old_font=SelectObject(dc,g_bold_font);
        RECT avatar={avatar_x,avatar_y,avatar_x+D(28),avatar_y+D(28)};
        DrawTextW(dc,initials,-1,&avatar,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SetTextColor(dc,TextColor());
        SIZE author_size={0};GetTextExtentPoint32W(dc,name,(int)wcslen(name),&author_size);
        int own_space=own?D(31):0;
        int date_space=D(105),author_right=min(text_right-date_space-own_space-D(8),text_x+author_size.cx+D(2));
        author_right=max(text_x+D(20),author_right);
        RECT author={text_x,top+D(6),author_right,top+D(28)};
        DrawTextW(dc,name,-1,&author,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,g_small_font);SetTextColor(dc,MutedColor());
        if(own){
            RECT badge={author_right+D(7),top+D(6),author_right+D(31),top+D(28)};
            SetTextColor(dc,g_dark?RGB(170,211,255):RGB(31,104,183));
            DrawTextW(dc,g_russian?L"Вы":L"You",-1,&badge,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            SetTextColor(dc,MutedColor());
        }
        wchar_t date[48]=L"";FormatCommentDate(item->created,date,_countof(date));
        RECT date_rect={author_right+own_space+D(7),top+D(6),text_right,top+D(28)};
        DrawTextW(dc,date,-1,&date_rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        SelectObject(dc,g_font);SetTextColor(dc,TextColor());
        RECT body={text_x,top+D(31),text_right,y-D(8)};
        DrawTextW(dc,item->body,-1,&body,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,old_font);
        if(i<g_popover.comments.count-1){HBRUSH line=CreateSolidBrush(BorderColor());RECT rule={text_x,y-1,text_right,y};FillRect(dc,&rule,line);DeleteObject(line);}
    }
    (void)window;
}

static LRESULT CommentListProcedureInner(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message){
    case WM_SIZE:UpdateCommentScroll();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEWHEEL: {
        int delta=GET_WHEEL_DELTA_WPARAM(wp);
        int next=g_popover.comment_scroll-MulDiv(delta,D(45),WHEEL_DELTA);
        g_popover.comment_scroll=max(0,min(next,g_popover.comment_scroll_max));
        SetScrollPos(window,SB_VERT,g_popover.comment_scroll,TRUE);
        InvalidateRect(window,NULL,FALSE);return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO info={sizeof(info),SIF_TRACKPOS};GetScrollInfo(window,SB_VERT,&info);
        int next=g_popover.comment_scroll;
        switch(LOWORD(wp)){
        case SB_LINEUP:next-=D(32);break;
        case SB_LINEDOWN:next+=D(32);break;
        case SB_PAGEUP:next-=D(130);break;
        case SB_PAGEDOWN:next+=D(130);break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:next=info.nTrackPos;break;
        case SB_TOP:next=0;break;
        case SB_BOTTOM:next=g_popover.comment_scroll_max;break;
        default:return 0;
        }
        g_popover.comment_scroll=max(0,min(next,g_popover.comment_scroll_max));
        SetScrollPos(window,SB_VERT,g_popover.comment_scroll,TRUE);
        InvalidateRect(window,NULL,FALSE);return 0;
    }
    case WM_LBUTTONDOWN:SetFocus(window);return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){SendMessageW(g_popover.window,WM_CLOSE,0,0);return 0;}
        if(wp==VK_UP||wp==VK_DOWN||wp==VK_PRIOR||wp==VK_NEXT||wp==VK_HOME||wp==VK_END){
            UINT command=wp==VK_UP?SB_LINEUP:wp==VK_DOWN?SB_LINEDOWN:wp==VK_PRIOR?SB_PAGEUP:
                wp==VK_NEXT?SB_PAGEDOWN:wp==VK_HOME?SB_TOP:SB_BOTTOM;
            SendMessageW(window,WM_VSCROLL,command,0);return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);
        HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=memory?CreateCompatibleBitmap(dc,client.right,client.bottom):NULL;
        if(bitmap){HGDIOBJ old=SelectObject(memory,bitmap);PaintCommentList(window,memory,client);
            BitBlt(dc,0,0,client.right,client.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteObject(bitmap);}
        else PaintCommentList(window,dc,client);
        if(memory)DeleteDC(memory);
        EndPaint(window,&paint);return 0;
    }
    }
    return DefWindowProcW(window,message,wp,lp);
}

static LRESULT CALLBACK CommentListProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    TaskPopover *popover=PopoverForChild(window),*previous=EnterPopoverContext(popover);
    LRESULT result=CommentListProcedureInner(window,message,wp,lp);
    LeavePopoverContext(popover,previous);return result;
}

static void RefreshTaskPopover(void)
{
    if(!g_popover.window)return;
    if(g_popover.kind==1&&g_popover.text){
        UpdateCommentScroll();
        if(g_popover.scroll_to_latest&&!g_popover.loading&&!g_popover.error[0]){
            g_popover.comment_scroll=g_popover.comment_scroll_max;
            SetScrollPos(g_popover.text,SB_VERT,g_popover.comment_scroll,TRUE);
            InvalidateRect(g_popover.text,NULL,FALSE);
            g_popover.scroll_to_latest=FALSE;
        }
        EnableWindow(g_popover.input,!g_popover.sending);
        EnableWindow(g_popover.send,!g_popover.loading&&!g_popover.sending);
        SetWindowTextW(g_popover.send,g_popover.sending?(g_russian?L"Отправка…":L"Sending…"):
            (g_russian?L"Отправить":L"Send"));
    }
    InvalidateRect(g_popover.window,NULL,TRUE);
}

static BOOL PopoverResizeGripAt(int x,int y,RECT client)
{
    if(y<client.bottom-D(22)||y>=client.bottom)return FALSE;
    return g_popover.detached||g_popover.pointer_left?x>=client.right-D(22):x<D(22);
}

static void UpdateTaskPopoverRegion(void)
{
    if(!g_popover.window)return;
    RECT client;GetClientRect(g_popover.window,&client);
    HRGN region=CreateRoundRectRgn(g_popover.body_left,0,g_popover.body_right,client.bottom,D(10),D(10));
    if(!region)return;
    if(!g_popover.detached){
        int tip=g_popover.pointer_left?0:client.right-1;
        int base=g_popover.pointer_left?g_popover.body_left:g_popover.body_right-1;
        POINT shape[]={{tip,g_popover.pointer_y},{base,g_popover.pointer_y-D(9)},{base,g_popover.pointer_y+D(9)}};
        HRGN arrow=CreatePolygonRgn(shape,3,WINDING);
        if(arrow){CombineRgn(region,region,arrow,RGN_OR);DeleteObject(arrow);}
    }
    if(!SetWindowRgn(g_popover.window,region,FALSE))DeleteObject(region);
}

static void LayoutTaskPopoverContents(void)
{
    if(!g_popover.window)return;
    RECT client;GetClientRect(g_popover.window,&client);
    int width=client.right,height=client.bottom,pointer_width=g_popover.detached?0:D(16);
    int body_width=width-pointer_width;
    g_popover.body_left=!g_popover.detached&&g_popover.pointer_left?pointer_width:0;
    g_popover.body_right=g_popover.body_left+body_width;
    g_popover.pointer_y=max(D(20),min(g_popover.pointer_y,height-D(20)));
    UpdateTaskPopoverRegion();
    int content_left=g_popover.body_left+D(19),content_width=max(D(30),body_width-D(38));
    HDWP batch=BeginDeferWindowPos(g_popover.kind==1?3:1);
    if(g_popover.kind==1){
        if(batch&&g_popover.text)batch=DeferWindowPos(batch,g_popover.text,NULL,g_popover.body_left+D(13),D(35),
            max(D(30),body_width-D(26)),max(D(60),height-D(140)),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
        if(batch&&g_popover.input)batch=DeferWindowPos(batch,g_popover.input,NULL,content_left,height-D(90),
            content_width,D(42),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
        if(batch&&g_popover.send)batch=DeferWindowPos(batch,g_popover.send,NULL,g_popover.body_right-D(112),height-D(36),
            D(99),D(29),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    }else if(batch&&g_popover.input){
        batch=DeferWindowPos(batch,g_popover.input,NULL,content_left,D(40),content_width,
            max(D(30),height-D(89)),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    }
    if(batch)EndDeferWindowPos(batch);
    else{
        if(g_popover.text)MoveWindow(g_popover.text,g_popover.body_left+D(13),D(35),
            max(D(30),body_width-D(26)),max(D(60),height-D(140)),FALSE);
        if(g_popover.input)MoveWindow(g_popover.input,content_left,g_popover.kind==1?height-D(90):D(40),
            content_width,g_popover.kind==1?D(42):max(D(30),height-D(89)),FALSE);
        if(g_popover.send)MoveWindow(g_popover.send,g_popover.body_right-D(112),height-D(36),D(99),D(29),FALSE);
    }
    if(g_popover.kind==1)UpdateCommentScroll();
    RedrawWindow(g_popover.window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}

static BOOL FindPopoverAnchorRect(const TaskPopover *popover,RECT *screen_icon)
{
    if(!popover||!g_list||!screen_icon)return FALSE;
    int task_index=TaskByKey(popover->key);
    for(int row=0;task_index>=0&&row<ListView_GetItemCount(g_list);row++){
        if(TaskIndexAtRow(row)!=task_index)continue;
        RECT icon;
        BOOL found=popover->kind==1?CommentIndicatorRectForRow(row,&icon):NoteIndicatorRectForRow(row,&icon);
        if(!found)found=PopoverFallbackAnchorForRow(row,&icon);
        if(!found)return FALSE;
        POINT points[2]={{icon.left,icon.top},{icon.right,icon.bottom}};
        MapWindowPoints(g_list,NULL,points,2);
        *screen_icon=(RECT){points[0].x,points[0].y,points[1].x,points[1].y};
        return TRUE;
    }
    return FALSE;
}

static TaskPopover *DetachTaskPopover(void)
{
    if(g_active_popover!=&g_attached_popover||!g_popover.window)return NULL;
    TaskPopover *detached=calloc(1,sizeof(*detached));if(!detached)return NULL;
    RECT old_bounds;GetWindowRect(g_popover.window,&old_bounds);
    int old_body_left=g_popover.body_left;
    int body_width=g_popover.body_right-g_popover.body_left;
    *detached=g_attached_popover;
    detached->detached=TRUE;detached->callback_depth=0;detached->transitioning=TRUE;
    detached->next=g_detached_popovers;g_detached_popovers=detached;
    SetWindowLongPtrW(detached->window,GWLP_USERDATA,(LONG_PTR)detached);
    int depth=g_attached_popover.callback_depth;
    ZeroMemory(&g_attached_popover,sizeof(g_attached_popover));
    g_attached_popover.callback_depth=depth;
    TaskPopover *previous=g_active_popover;g_active_popover=detached;
    SetWindowPos(detached->window,NULL,old_bounds.left+old_body_left,old_bounds.top,
        body_width,old_bounds.bottom-old_bounds.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    OffsetRect(&detached->drag_initial,old_body_left,0);
    detached->transitioning=FALSE;
    LayoutTaskPopoverContents();
    RedrawWindow(detached->window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    g_active_popover=previous;
    InvalidateRect(g_list,NULL,FALSE);
    return detached;
}

static BOOL AttachTaskPopover(TaskPopover *detached,BOOL show_error)
{
    if(!detached||!detached->detached||!detached->window)return FALSE;
    RECT icon;
    if(!FindPopoverAnchorRect(detached,&icon)){
        if(show_error)MessageBoxW(detached->window,
            g_russian?L"Чтобы закрепить окно, сначала покажите эту задачу в списке.":
                L"Show this task in the list before docking its window.",
            APP_TITLE,MB_OK|MB_ICONINFORMATION);
        return FALSE;
    }
    TaskPopover *previous=g_active_popover;g_active_popover=&g_attached_popover;
    if(g_attached_popover.window)CloseTaskPopover();
    if(g_attached_popover.window){g_active_popover=previous;return FALSE;}
    RECT old_bounds;GetWindowRect(detached->window,&old_bounds);
    UnlinkDetachedPopover(detached);
    g_attached_popover=*detached;
    g_attached_popover.detached=g_attached_popover.retired=FALSE;
    g_attached_popover.next=NULL;g_attached_popover.callback_depth=0;
    g_attached_popover.transitioning=TRUE;
    SetWindowLongPtrW(g_attached_popover.window,GWLP_USERDATA,(LONG_PTR)&g_attached_popover);
    detached->retired=TRUE;
    SetWindowPos(g_popover.window,NULL,old_bounds.left-(g_popover.pointer_left?D(16):0),old_bounds.top,
        old_bounds.right-old_bounds.left+D(16),old_bounds.bottom-old_bounds.top,
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    g_popover.transitioning=FALSE;
    LayoutTaskPopoverContents();RepositionTaskPopover();
    RedrawWindow(g_popover.window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    g_active_popover=previous==detached?&g_attached_popover:previous;
    if(detached->callback_depth==0)free(detached);
    InvalidateRect(g_list,NULL,FALSE);
    return TRUE;
}

static BOOL SnapDetachedPopover(TaskPopover *popover)
{
    if(!popover||!popover->detached)return FALSE;
    RECT icon,window;
    if(!FindPopoverAnchorRect(popover,&icon)||!GetWindowRect(popover->window,&window))return FALSE;
    int edge_distance=min(abs(window.left-icon.right),abs(window.right-icon.left));
    int vertical_distance=abs(window.top+D(48)-(icon.top+icon.bottom)/2);
    return edge_distance<=D(28)&&vertical_distance<=D(42)&&AttachTaskPopover(popover,FALSE);
}

static LRESULT TaskPopoverProcedureInner(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message){
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:
        if(window==g_popover.window&&!g_popover.transitioning){
            LayoutTaskPopoverContents();
        }
        return 0;
    case WM_SETCURSOR: {
        POINT cursor;RECT client;
        if(!GetCursorPos(&cursor)||!ScreenToClient(window,&cursor)||!GetClientRect(window,&client))break;
        if(PopoverResizeGripAt(cursor.x,cursor.y,client)){
            SetCursor(LoadCursorW(NULL,g_popover.detached||g_popover.pointer_left?IDC_SIZENWSE:IDC_SIZENESW));return TRUE;
        }
        if(cursor.y<D(35)&&cursor.x>=g_popover.body_left&&cursor.x<g_popover.body_right){
            SetCursor(LoadCursorW(NULL,cursor.x>=g_popover.body_right-D(37)?IDC_HAND:IDC_SIZEALL));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        RECT client;GetClientRect(window,&client);
        if(PopoverResizeGripAt((short)LOWORD(lp),(short)HIWORD(lp),client)){
            g_popover.resizing=TRUE;GetCursorPos(&g_popover.resize_start);
            GetWindowRect(window,&g_popover.resize_initial);
            SetCapture(window);return 0;
        }
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if(y<D(35)&&x>=g_popover.body_left&&x<g_popover.body_right&&
            x<g_popover.body_right-D(37)){
            g_popover.dragging=TRUE;GetCursorPos(&g_popover.drag_start);
            GetWindowRect(window,&g_popover.drag_initial);SetCapture(window);return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if(g_popover.dragging){
            POINT cursor;GetCursorPos(&cursor);
            int dx=cursor.x-g_popover.drag_start.x,dy=cursor.y-g_popover.drag_start.y;
            if(!g_popover.detached&&abs(dx)+abs(dy)>=D(6)){
                if(!DetachTaskPopover())return 0;
            }
            TaskPopover *moving=(TaskPopover*)GetWindowLongPtrW(window,GWLP_USERDATA);
            if(moving&&moving->detached&&moving->dragging)
                SetWindowPos(window,NULL,moving->drag_initial.left+dx,moving->drag_initial.top+dy,
                    0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            return 0;
        }
        if(g_popover.resizing){
            POINT cursor;GetCursorPos(&cursor);
            RECT initial=g_popover.resize_initial;
            int dx=cursor.x-g_popover.resize_start.x,dy=cursor.y-g_popover.resize_start.y;
            int width=initial.right-initial.left+(g_popover.detached||g_popover.pointer_left?dx:-dx);
            int height=initial.bottom-initial.top+dy;
            MONITORINFO monitor={sizeof(monitor)};
            GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
            width=max(D(g_popover.kind==1?310:260),min(width,
                g_popover.detached||g_popover.pointer_left?monitor.rcWork.right-initial.left-D(8):initial.right-monitor.rcWork.left-D(8)));
            height=max(D(g_popover.kind==1?250:160),min(height,monitor.rcWork.bottom-initial.top-D(8)));
            int x=g_popover.detached||g_popover.pointer_left?initial.left:initial.right-width;
            RECT current;GetWindowRect(window,&current);
            if(x!=current.left||width!=current.right-current.left||height!=current.bottom-current.top){
                /* Suppress intermediate repainting, then finish every queued client paint now. */
                SetWindowPos(window,NULL,x,initial.top,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
                RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
            }
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        if(g_popover.resizing){g_popover.resizing=FALSE;UpdateTaskPopoverRegion();RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);}
        g_popover.dragging=FALSE;break;
    case WM_CLOSE:
        if(g_popover.kind==2&&g_popover.note_save_failed){
            MessageBoxW(window,g_russian?L"Заметка не сохранилась. Проверьте доступ к папке данных; окно оставлено открытым.":
                L"The note was not saved. Check access to the data folder; this window remains open.",APP_TITLE,MB_OK|MB_ICONWARNING);
            return 0;
        }
        CloseTaskPopover();return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==IDC_POPOVER_INPUT&&HIWORD(wp)==EN_CHANGE&&g_popover.kind==1){
            InvalidateRect(g_popover.input,NULL,TRUE);return 0;
        }
        if(LOWORD(wp)==IDC_POPOVER_INPUT&&HIWORD(wp)==EN_CHANGE&&g_popover.kind==2&&!g_popover.updating_note){
            wchar_t note[2048];GetWindowTextW(g_popover.input,note,_countof(note));
            BOOL saved=NativeNoteSave(g_settings_path,g_snapshot.account,g_popover.key,note);
            g_popover.note_save_failed=!saved;
            if(saved){
                int index=TaskByKey(g_popover.key);
                if(index>=0){free(g_tasks[index].local_note);g_tasks[index].local_note=note[0]?DuplicateWide(note):NULL;}
                InvalidateRect(g_list,NULL,FALSE);
            }
            InvalidateRect(window,NULL,FALSE);return 0;
        }
        if(LOWORD(wp)==IDC_POPOVER_SEND&&g_popover.kind==1&&!g_popover.sending){
            wchar_t comment[2048];GetWindowTextW(g_popover.input,comment,_countof(comment));
            BOOL has_text=FALSE;for(const wchar_t *p=comment;*p;p++)if(*p!=L' '&&*p!=L'\t'&&*p!=L'\r'&&*p!=L'\n'){has_text=TRUE;break;}
            if(!has_text)return 0;
            if(StartJiraJobWithInput(5,g_popover.key,NULL,NULL,NULL,comment,(POINT){0})){
                g_popover.sending=TRUE;g_popover.error[0]=0;RefreshTaskPopover();
            }else if(g_jira_busy){
                wcscpy_s(g_popover.error,_countof(g_popover.error),g_russian?L"Дождитесь завершения запроса Jira.":L"Wait for the Jira request to finish.");
                RefreshTaskPopover();
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if(wp==2&&g_popover.kind==1&&g_popover.loading&&!g_jira_busy){
            if(StartJiraJob(4,g_popover.key,NULL,(POINT){0}))KillTimer(window,2);
        }
        return 0;
    case WM_LBUTTONUP: {
        if(g_popover.dragging){
            TaskPopover *moving=(TaskPopover*)GetWindowLongPtrW(window,GWLP_USERDATA);
            if(moving)moving->dragging=FALSE;
            if(GetCapture()==window)ReleaseCapture();
            if(moving&&moving->detached)SnapDetachedPopover(moving);
            return 0;
        }
        if(g_popover.resizing){
            g_popover.resizing=FALSE;UpdateTaskPopoverRegion();
            if(GetCapture()==window)ReleaseCapture();
            if(g_popover.kind==1)UpdateCommentScroll();
            RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
            if(!g_popover.detached&&g_window&&g_settings){RECT owner;GetClientRect(g_window,&owner);LayoutFooterControls(owner.right,owner.bottom);}
            RECT size;GetWindowRect(window,&size);
            int scale=(int)g_dpi*(g_density==1?100:90);
            g_popover_width_dip[g_popover.kind]=MulDiv(size.right-size.left-D(g_popover.detached?0:16),9600,scale);
            g_popover_height_dip[g_popover.kind]=MulDiv(size.bottom-size.top,9600,scale);
            return 0;
        }
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if(x>=g_popover.body_right-D(37)&&y<D(34)){SendMessageW(window,WM_CLOSE,0,0);return 0;}
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint;HDC target=BeginPaint(window,&paint);RECT client;GetClientRect(window,&client);
        HDC memory=CreateCompatibleDC(target);
        HBITMAP bitmap=memory?CreateCompatibleBitmap(target,max(1,client.right),max(1,client.bottom)):NULL;
        HGDIOBJ old_bitmap=bitmap?SelectObject(memory,bitmap):NULL;
        HDC dc=bitmap?memory:target;
        FillRect(dc,&client,g_popover_brush);
        RECT body={g_popover.body_left,0,g_popover.body_right,client.bottom};
        FillRect(dc,&body,g_popover_brush);
        HPEN border=CreatePen(PS_SOLID,1,PopoverBorderColor());HGDIOBJ old_pen=SelectObject(dc,border);
        HGDIOBJ old_brush=SelectObject(dc,g_popover_brush);
        RoundRect(dc,body.left,body.top,body.right,body.bottom,D(10),D(10));
        RECT header={body.left+1,1,body.right-1,D(35)};
        HBRUSH header_brush=CreateSolidBrush(PopoverHeaderColor());FillRect(dc,&header,header_brush);DeleteObject(header_brush);
        HPEN divider=CreatePen(PS_SOLID,1,PopoverBorderColor());SelectObject(dc,divider);
        MoveToEx(dc,body.left+D(13),D(35),NULL);LineTo(dc,body.right-D(13),D(35));
        SelectObject(dc,border);DeleteObject(divider);
        if(!g_popover.detached){
            int tip=g_popover.pointer_left?0:client.right-1;
            int base=g_popover.pointer_left?body.left:body.right-1;
            POINT pointer[]={{tip,g_popover.pointer_y},{base,g_popover.pointer_y-D(9)},{base,g_popover.pointer_y+D(9)}};
            HGDIOBJ pointer_pen=SelectObject(dc,GetStockObject(NULL_PEN));
            HBRUSH arrow_fill=CreateSolidBrush(PopoverArrowColor());
            HGDIOBJ pointer_brush=SelectObject(dc,arrow_fill);
            Polygon(dc,pointer,3);
            SelectObject(dc,pointer_brush);DeleteObject(arrow_fill);
            SelectObject(dc,pointer_pen);
            HPEN pointer_edge=CreatePen(PS_SOLID,1,g_dark?RGB(105,148,190):RGB(137,176,215));
            SelectObject(dc,pointer_edge);
            MoveToEx(dc,pointer[1].x,pointer[1].y,NULL);LineTo(dc,pointer[0].x,pointer[0].y);
            LineTo(dc,pointer[2].x,pointer[2].y);
            SelectObject(dc,old_pen);DeleteObject(pointer_edge);
        }
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(border);
        HPEN card_border=CreatePen(PS_SOLID,1,BorderColor());
        old_pen=SelectObject(dc,card_border);old_brush=SelectObject(dc,g_panel_brush);
        if(g_popover.kind==1){
            RECT composer={body.left+D(13),client.bottom-D(95),body.right-D(13),client.bottom-D(43)};
            RoundRect(dc,composer.left,composer.top,composer.right,composer.bottom,D(8),D(8));
        }else{
            RECT note_area={body.left+D(13),D(33),body.right-D(13),client.bottom-D(42)};
            RoundRect(dc,note_area.left,note_area.top,note_area.right,note_area.bottom,D(8),D(8));
        }
        SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(card_border);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,TextColor());HGDIOBJ old_font=SelectObject(dc,g_bold_font);
        RECT title={body.left+D(25),D(3),body.right-D(140),D(33)};
        if(title.right<title.left+D(65))title.right=body.right-D(44);
        DrawTextW(dc,g_popover.kind==2?(g_russian?L"Заметки":L"Notes"):
            (g_russian?L"Комментарии Jira":L"Jira comments"),-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        HBRUSH accent=CreateSolidBrush(AccentColor());RECT bar={body.left+D(14),D(11),body.left+D(17),D(25)};
        FillRect(dc,&bar,accent);DeleteObject(accent);
        SelectObject(dc,g_small_font);SetTextColor(dc,MutedColor());
        RECT key_rect={body.right-D(137),D(3),body.right-D(43),D(33)};
        if(key_rect.left>title.left+D(64))DrawTextW(dc,g_popover.key,-1,&key_rect,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        RECT close_rect={body.right-D(35),D(2),body.right-D(8),D(29)};
        DrawTextW(dc,L"×",-1,&close_rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(g_popover.kind==2){
            RECT status={body.left+D(14),client.bottom-D(35),body.right-D(14),client.bottom-D(8)};
            status.right-=D(15);
            DrawTextW(dc,g_popover.note_save_failed?(g_russian?L"Не сохранено · проверьте доступ к данным":L"Not saved · check data access"):
                (g_russian?L"Сохраняется на этом компьютере":L"Saved on this computer"),-1,&status,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        }
        HPEN grip_pen=CreatePen(PS_SOLID,1,g_dark?RGB(142,157,173):RGB(134,153,174));
        HGDIOBJ saved_grip=SelectObject(dc,grip_pen);
        int grip_x=g_popover.detached||g_popover.pointer_left?body.right-D(13):body.left+D(12),grip_y=client.bottom-D(12);
        int direction=g_popover.detached||g_popover.pointer_left?1:-1;
        MoveToEx(dc,grip_x-direction*D(5),grip_y+D(5),NULL);LineTo(dc,grip_x+D(5)*direction,grip_y-D(5));
        MoveToEx(dc,grip_x,grip_y+D(5),NULL);LineTo(dc,grip_x+D(5)*direction,grip_y);
        SelectObject(dc,saved_grip);DeleteObject(grip_pen);
        SelectObject(dc,old_font);
        if(bitmap){BitBlt(target,0,0,client.right,client.bottom,memory,0,0,SRCCOPY);
            SelectObject(memory,old_bitmap);DeleteObject(bitmap);}
        if(memory)DeleteDC(memory);
        EndPaint(window,&paint);return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wp,PanelColor());SetTextColor((HDC)wp,TextColor());return (LRESULT)g_panel_brush;
    case WM_DRAWITEM:
        if(((DRAWITEMSTRUCT*)lp)->CtlID==IDC_POPOVER_SEND){
            DRAWITEMSTRUCT *draw=(DRAWITEMSTRUCT*)lp;RECT rect=draw->rcItem;
            COLORREF background=draw->itemState&ODS_DISABLED?(g_dark?RGB(62,69,77):RGB(201,213,225)):
                draw->itemState&ODS_SELECTED?(g_dark?RGB(57,119,184):RGB(12,93,178)):AccentColor();
            HBRUSH fill=CreateSolidBrush(background);HGDIOBJ old_fill=SelectObject(draw->hDC,fill);
            HGDIOBJ old_pen=SelectObject(draw->hDC,GetStockObject(NULL_PEN));
            RoundRect(draw->hDC,rect.left,rect.top,rect.right,rect.bottom,D(8),D(8));
            SelectObject(draw->hDC,old_fill);SelectObject(draw->hDC,old_pen);DeleteObject(fill);
            SetBkMode(draw->hDC,TRANSPARENT);
            SetTextColor(draw->hDC,draw->itemState&ODS_DISABLED?(g_dark?RGB(191,199,207):RGB(82,96,111)):RGB(255,255,255));
            HGDIOBJ font=SelectObject(draw->hDC,g_font);
            wchar_t label[32];GetWindowTextW(draw->hwndItem,label,_countof(label));
            DrawTextW(draw->hDC,label,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(draw->hDC,font);return TRUE;
        }
        break;
    }
    return DefWindowProcW(window,message,wp,lp);
}

static LRESULT CALLBACK TaskPopoverProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    TaskPopover *popover=(TaskPopover*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(!popover)popover=&g_attached_popover;
    TaskPopover *previous=EnterPopoverContext(popover);
    LRESULT result=TaskPopoverProcedureInner(window,message,wp,lp);
    LeavePopoverContext(popover,previous);return result;
}

static void OpenTaskPopover(int task_index,int kind,RECT icon_rect)
{
    if(task_index<0||task_index>=g_task_count||kind<1||kind>2)return;
    wchar_t key[80];wcscpy_s(key,_countof(key),g_tasks[task_index].key);
    TaskPopover *existing=FindDetachedPopover(kind,key);
    if(existing){ShowWindow(existing->window,SW_SHOWNORMAL);SetForegroundWindow(existing->window);return;}
    if(g_popover.window&&g_popover.kind==kind&&!_wcsicmp(g_popover.key,key)){CloseTaskPopover();return;}
    CloseTaskPopover();
    if(g_popover.window)return;
    POINT corners[2]={{icon_rect.left,icon_rect.top},{icon_rect.right,icon_rect.bottom}};
    MapWindowPoints(g_list,NULL,corners,2);
    RECT icon={corners[0].x,corners[0].y,corners[1].x,corners[1].y};
    MONITORINFO monitor={sizeof(monitor)};GetMonitorInfoW(MonitorFromRect(&icon,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT work=monitor.rcWork;
    int pointer_width=D(16),body_width=min(D(g_popover_width_dip[kind]),work.right-work.left-D(32));
    int width=body_width+pointer_width,height=min(D(g_popover_height_dip[kind]),work.bottom-work.top-D(16));
    BOOL right=icon.right+D(2)+width<=work.right-D(8);
    int x=right?icon.right-D(2):icon.left+D(2)-width;
    x=max(work.left+D(8),min(x,work.right-D(8)-width));
    int y=max(work.top+D(8),min((icon.top+icon.bottom)/2-D(48),work.bottom-D(8)-height));
    RECT owner;GetWindowRect(g_window,&owner);
    if(x<owner.right&&x+width>owner.left){
        int above_footer=owner.bottom-D(48)-height;
        if(above_footer>=work.top+D(8))y=min(y,above_footer);
    }
    WNDCLASSW wc={0};wc.lpfnWndProc=TaskPopoverProcedure;wc.hInstance=g_instance;
    wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=NULL;
    wc.style=CS_DROPSHADOW;wc.lpszClassName=L"Task Manager.TaskPopover";
    RegisterClassW(&wc);
    WNDCLASSW comment_class={0};comment_class.lpfnWndProc=CommentListProcedure;
    comment_class.hInstance=g_instance;comment_class.hCursor=LoadCursorW(NULL,IDC_ARROW);
    comment_class.hbrBackground=NULL;comment_class.lpszClassName=L"Task Manager.CommentList";
    RegisterClassW(&comment_class);
    g_popover.kind=kind;wcscpy_s(g_popover.key,_countof(g_popover.key),key);
    g_popover.pointer_left=right;
    g_popover.body_left=right?pointer_width:0;
    g_popover.body_right=g_popover.body_left+body_width;
    g_popover.pointer_y=(icon.top+icon.bottom)/2-y;
    g_popover.pointer_y=max(D(20),min(g_popover.pointer_y,height-D(20)));
    g_popover.loading=kind==1;g_popover.sending=g_popover.note_save_failed=FALSE;
    g_popover.scroll_to_latest=kind==1;
    g_popover.error[0]=0;
    HWND panel=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,APP_TITLE,
        WS_POPUP|WS_CLIPCHILDREN,x,y,width,height,g_window,NULL,g_instance,NULL);
    if(!panel){g_popover.kind=0;g_popover.key[0]=0;return;}
    g_popover.window=panel;
    SetWindowLongPtrW(panel,GWLP_USERDATA,(LONG_PTR)&g_attached_popover);
    int content_left=g_popover.body_left+D(19),content_width=body_width-D(38);
    if(kind==1){
        g_popover.text=CreateWindowExW(0,comment_class.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL,
            g_popover.body_left+D(13),D(35),body_width-D(26),height-D(140),panel,(HMENU)(INT_PTR)IDC_POPOVER_TEXT,g_instance,NULL);
        g_popover.input=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL,
            content_left,height-D(90),content_width,D(42),panel,(HMENU)(INT_PTR)IDC_POPOVER_INPUT,g_instance,NULL);
        g_popover.send=CreateWindowExW(0,L"BUTTON",g_russian?L"Отправить":L"Send",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            g_popover.body_right-D(112),height-D(36),D(99),D(29),panel,(HMENU)(INT_PTR)IDC_POPOVER_SEND,g_instance,NULL);
        SendMessageW(g_popover.input,EM_SETLIMITTEXT,2047,0);
    }else{
        g_popover.input=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL,
            content_left,D(40),content_width,height-D(89),panel,(HMENU)(INT_PTR)IDC_POPOVER_INPUT,g_instance,NULL);
        SendMessageW(g_popover.input,EM_SETLIMITTEXT,2047,0);
        g_popover.updating_note=TRUE;
        SetWindowTextW(g_popover.input,g_tasks[task_index].local_note?g_tasks[task_index].local_note:L"");
        g_popover.updating_note=FALSE;
    }
    if(g_popover.text){
        SendMessageW(g_popover.text,WM_SETFONT,(WPARAM)g_font,TRUE);
        SetWindowTheme(g_popover.text,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
    }
    if(g_popover.input){
        SendMessageW(g_popover.input,WM_SETFONT,(WPARAM)g_font,TRUE);
        SetWindowTheme(g_popover.input,g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
        SetWindowSubclass(g_popover.input,PopoverEditProcedure,1,0);
    }
    if(g_popover.send)SendMessageW(g_popover.send,WM_SETFONT,(WPARAM)g_font,TRUE);
    LayoutTaskPopoverContents();
    RefreshTaskPopover();ShowWindow(panel,SW_SHOWNORMAL);UpdateWindow(panel);
    if(g_window&&g_settings){RECT client;GetClientRect(g_window,&client);LayoutFooterControls(client.right,client.bottom);}
    SetFocus(g_popover.input);InvalidateRect(g_list,NULL,FALSE);
    if(kind==1){SetTimer(panel,2,150,NULL);if(!g_jira_busy&&StartJiraJob(4,key,NULL,(POINT){0}))KillTimer(panel,2);}
}

static void RepositionTaskPopover(void)
{
    if(!g_popover.window||g_popover.detached)return;
    RECT screen_icon;
    if(!FindPopoverAnchorRect(g_active_popover,&screen_icon))return;
    RECT panel;GetWindowRect(g_popover.window,&panel);
    int width=panel.right-panel.left,height=panel.bottom-panel.top;
    MONITORINFO monitor={sizeof(monitor)};
    GetMonitorInfoW(MonitorFromRect(&screen_icon,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT work=monitor.rcWork;
    int x=g_popover.pointer_left?screen_icon.right-D(2):screen_icon.left+D(2)-width;
    x=max(work.left+D(8),min(x,work.right-D(8)-width));
    int y=max(work.top+D(8),min((screen_icon.top+screen_icon.bottom)/2-D(48),work.bottom-D(8)-height));
    RECT owner;GetWindowRect(g_window,&owner);
    if(x<owner.right&&x+width>owner.left){
        int above_footer=owner.bottom-D(48)-height;
        if(above_footer>=work.top+D(8))y=min(y,above_footer);
    }
    g_popover.pointer_y=max(D(20),min((screen_icon.top+screen_icon.bottom)/2-y,height-D(20)));
    UpdateTaskPopoverRegion();
    SetWindowPos(g_popover.window,NULL,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    if(g_window&&g_settings){RECT client;GetClientRect(g_window,&client);LayoutFooterControls(client.right,client.bottom);}
    InvalidateRect(g_popover.window,NULL,FALSE);
}

static DWORD WINAPI JiraWorker(void *parameter)
{
    JiraJob *job=parameter;JiraConnection client;
    if(JiraConnect(&client)){
        if(job->action==0){job->success=job->loaded=JiraLoadWithReporter(&client,&job->snapshot,TRUE);}
        else if(job->action==1)job->success=JiraReadTransitions(&client,job->key,&job->transitions);
        else if(job->action==4)job->success=job->loaded=JiraReadComments(&client,job->key,&job->comments);
        else if(job->action==5){
            job->success=JiraPostComment(&client,job->key,job->transition_comment);
            if(!job->success)wcscpy_s(job->error,256,client.error);
            job->loaded=JiraReadComments(&client,job->key,&job->comments);
            if(job->success&&!job->loaded)job->success=FALSE;
            if(!job->success&&client.write_attempted){
                wchar_t detail[128];wcsncpy_s(detail,_countof(detail),job->error[0]?job->error:client.error,_TRUNCATE);
                swprintf(job->error,_countof(job->error),L"Comment may have been posted. %ls\n%ls",
                    job->loaded?L"Jira comments were reloaded.":L"Reload failed; check Jira before resending.",detail);
            }
        }
        else {
            if(job->action==3)job->success=JiraSwitch(&client,job->key);
            else {
                /* Revalidate the chosen transition against this issue immediately before POST. */
                job->success=JiraReadTransitions(&client,job->key,&job->transitions);
                BOOL available=FALSE;
                if(job->success)for(int i=0;i<job->transitions.count;i++)if(!wcscmp(job->transition,job->transitions.items[i].id))available=TRUE;
                if(job->success&&!available){job->success=FALSE;wcscpy_s(client.error,256,L"Transition no longer available. No change made.");}
                if(job->success)job->success=JiraPostTransitionWithInput(&client,job->key,job->transition,
                    job->transition_field[0]?job->transition_field:NULL,
                    job->transition_option[0]?job->transition_option:NULL,
                    job->transition_comment[0]?job->transition_comment:NULL);
            }
            if(!job->success)wcscpy_s(job->error,256,client.error);
            /* Always reread after a possible write, including an ambiguous response. Never repeat POST. */
            job->loaded=JiraLoadWithReporter(&client,&job->snapshot,TRUE);
            if(!job->loaded&&job->success)job->success=FALSE;
            if(!job->success&&client.write_attempted){
                wchar_t detail[128];wcsncpy_s(detail,128,job->error[0]?job->error:client.error,_TRUNCATE);
                swprintf(job->error,256,L"Some changes may have succeeded. %ls\n%ls",
                    job->loaded?L"Actual Jira state was reloaded.":L"Reload failed; displayed tasks are stale. Refresh before further changes.",detail);
            }
        }
    }
    if(!job->success&&!job->error[0])wcscpy_s(job->error,256,client.error);
    JiraDisconnect(&client);
    if(InterlockedCompareExchange(&g_jira_closing,0,0)||!PostMessageW(g_window,WM_JIRA_RESULT,0,(LPARAM)job)){
        JiraFreeSnapshot(&job->snapshot);JiraFreeComments(&job->comments);free(job);
    }
    return 0;
}

static BOOL StartJiraJobWithInput(int action,const wchar_t *key,const wchar_t *transition,
    const wchar_t *field,const wchar_t *option,const wchar_t *comment,POINT anchor)
{
    if(g_jira_busy||g_update_handoff)return FALSE;
    if(!CommitNoteEdit(TRUE))return FALSE;
    if(action!=0&&action!=4 && (g_jira_stale||!g_updated_time.wYear)){
        MessageBoxW(g_window,g_russian?L"Сначала обновите задачи Jira кнопкой обновления.":L"Refresh Jira successfully before changing tasks.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return FALSE;
    }
    JiraJob *job=calloc(1,sizeof(*job));if(!job)return FALSE;
    job->action=action;job->anchor=anchor;
    if(key)wcsncpy_s(job->key,80,key,_TRUNCATE);
    if(transition)wcsncpy_s(job->transition,80,transition,_TRUNCATE);
    if(field)wcsncpy_s(job->transition_field,80,field,_TRUNCATE);
    if(option)wcsncpy_s(job->transition_option,80,option,_TRUNCATE);
    if(comment)wcsncpy_s(job->transition_comment,2048,comment,_TRUNCATE);
    g_jira_busy=TRUE;EnableWindow(g_refresh,FALSE);UpdateLastUpdated();
    if(action==3&&key)BeginPendingSwitchFeedback(key);
    g_jira_thread=CreateThread(NULL,0,JiraWorker,job,0,NULL);
    if(!g_jira_thread){free(job);g_jira_busy=FALSE;g_jira_stale=TRUE;EndPendingSwitchFeedback();wcscpy_s(g_jira_message,256,L"Could not start background worker.");EnableWindow(g_refresh,TRUE);UpdateLastUpdated();return FALSE;}
    return TRUE;
}

static BOOL StartJiraJob(int action,const wchar_t *key,const wchar_t *transition,POINT anchor)
{return StartJiraJobWithInput(action,key,transition,NULL,NULL,NULL,anchor);}

static BOOL InstallSnapshot(JiraSnapshot *snapshot)
{
    TaskItem *items=calloc((size_t)(snapshot->count+1),sizeof(*items));if(!items)return FALSE;
    const wchar_t **keys=calloc((size_t)(snapshot->count+1),sizeof(*keys));
    BOOL *new_flags=calloc((size_t)(snapshot->count+1),sizeof(*new_flags));
    BOOL *reporter_only=calloc((size_t)(snapshot->count+1),sizeof(*reporter_only));
    if(!keys||!new_flags||!reporter_only){free(items);free(keys);free(new_flags);free(reporter_only);return FALSE;}
    for(int i=0;i<snapshot->count;i++){
        keys[i]=snapshot->items[i].key;
        reporter_only[i]=snapshot->items[i].reported_by_me&&!snapshot->items[i].assigned_to_me;
    }
    int newly_arrived=0,first_arrival=-1;
    BOOL new_state_loaded=NativeNewTasksSeedReporter(g_settings_path,snapshot->account,keys,reporter_only,snapshot->count)&&
        NativeNewTasksObserve(g_settings_path,snapshot->account,keys,snapshot->count,new_flags,&newly_arrived,&first_arrival);
    if(!new_state_loaded)newly_arrived=0;
    free(keys);free(reporter_only);
    int status_count=g_status_count;
    wchar_t status_ids[64][80],status_names[64][128];int categories[64];
    memcpy(status_ids,g_status_ids,sizeof(status_ids));memcpy(status_names,g_status_names,sizeof(status_names));
    memcpy(categories,g_status_categories,sizeof(categories));
    for(int i=0;i<snapshot->count;i++){
        JiraItem *source=&snapshot->items[i];int status=-1;
        for(int j=0;j<status_count;j++)if(!wcscmp(source->status_id,status_ids[j])){status=j;break;}
        if(status<0){
            if(status_count==64){free(items);free(new_flags);return FALSE;}
            status=status_count++;
            wcscpy_s(status_ids[status],80,source->status_id);
        }
        wcscpy_s(status_names[status],128,source->status_name);
        categories[status]=!wcscmp(source->category,L"done")?2:!wcscmp(source->category,L"indeterminate")?1:0;
        items[i]=(TaskItem){source->key,source->summary,(TaskStatus)status,
            source->assigned_to_me&&!_wcsicmp(source->status_name,L"In Progress"),source->latest_comment,NULL};
        items[i].is_new=new_state_loaded&&new_flags[i];
        items[i].assigned_to_me=source->assigned_to_me;
        items[i].reported_by_me=source->reported_by_me;
        items[i].latest_comment_id=source->latest_comment_id;
        BOOL own_comment=source->latest_comment_author[0]&&!_wcsicmp(source->latest_comment_author,snapshot->account);
        if(!NativeCommentObserve(g_settings_path,snapshot->account,source->key,source->latest_comment_id,own_comment,&items[i].unread_comment))
            items[i].unread_comment=FALSE;
        int old_index=TaskByKey(source->key);
        if(old_index>=0){
            if(ShouldPulseComment(&g_tasks[old_index],&items[i]))
                items[i].comment_pulse_started=GetTickCount64();
            else if(items[i].unread_comment&&CommentPulseActive(&g_tasks[old_index],GetTickCount64()))
                items[i].comment_pulse_started=g_tasks[old_index].comment_pulse_started;
        }
        wchar_t note[2048];
        if(NativeNoteLoad(g_settings_path,snapshot->account,source->key,note,_countof(note))&&note[0])items[i].local_note=DuplicateWide(note);
    }
    free(new_flags);
    g_status_count=status_count;
    memcpy(g_status_ids,status_ids,sizeof(status_ids));memcpy(g_status_names,status_names,sizeof(status_names));
    memcpy(g_status_categories,categories,sizeof(categories));
    /* Stable active-first display, independent from the server's update order. */
    for(int i=1;i<snapshot->count;i++)if(items[i].active){
        TaskItem value=items[i];int j=i;while(j>0&&!items[j-1].active){items[j]=items[j-1];j--;}items[j]=value;
    }
    FreeTaskItems();JiraFreeSnapshot(&g_snapshot);
    g_snapshot=*snapshot;memset(snapshot,0,sizeof(*snapshot));g_tasks=items;g_task_count=g_snapshot.count;
    g_flash_task=-1;KillTimer(g_list,2);g_status_pulse_task=-1;KillTimer(g_list,4);KillTimer(g_list,7);
    g_pressed_indicator_task=-1;g_pressed_indicator_kind=0;g_context_task=-1;
    g_hover_row=-1;g_hover_status=g_hover_key=g_hover_note=g_hover_comment=FALSE;
    GetLocalTime(&g_updated_time);RefreshTaskList();UpdateTooltips();
    InvalidateRect(g_tab_reported,NULL,FALSE);
    for(int i=0;i<g_task_count;i++)if(g_tasks[i].comment_pulse_started){SetTimer(g_list,7,30,NULL);break;}
    if(newly_arrived)ShowNewTaskNotification(newly_arrived,first_arrival>=0?&g_snapshot.items[first_arrival]:NULL);
    return TRUE;
}

static void FinishJiraJob(JiraJob *job)
{
    if(g_jira_thread){CloseHandle(g_jira_thread);g_jira_thread=NULL;}
    g_jira_busy=FALSE;EnableWindow(g_refresh,TRUE);
    if(job->action==4||job->action==5){
        TaskPopover *previous=g_active_popover;
        TaskPopover *recipient=g_attached_popover.window&&g_attached_popover.kind==1&&
            !_wcsicmp(g_attached_popover.key,job->key)?&g_attached_popover:FindDetachedPopover(1,job->key);
        if(recipient)g_active_popover=recipient;
        if(recipient){
            g_popover.loading=g_popover.sending=FALSE;
            if(job->loaded){
                JiraFreeComments(&g_popover.comments);g_popover.comments=job->comments;
                memset(&job->comments,0,sizeof(job->comments));
                if(g_popover.comments.count){
                    JiraComment *latest=&g_popover.comments.items[g_popover.comments.count-1];
                    int task_index=TaskByKey(job->key);
                    if(task_index>=0){
                        for(int i=0;i<g_snapshot.count;i++)if(!_wcsicmp(g_snapshot.items[i].key,job->key)){
                            wcscpy_s(g_snapshot.items[i].latest_comment,_countof(g_snapshot.items[i].latest_comment),latest->body);
                            wcscpy_s(g_snapshot.items[i].latest_comment_id,_countof(g_snapshot.items[i].latest_comment_id),latest->id);
                            wcscpy_s(g_snapshot.items[i].latest_comment_author,_countof(g_snapshot.items[i].latest_comment_author),latest->author);
                            g_tasks[task_index].latest_comment=g_snapshot.items[i].latest_comment;
                            g_tasks[task_index].latest_comment_id=g_snapshot.items[i].latest_comment_id;
                            break;
                        }
                        MarkCommentSeen(task_index);
                    }
                }
            }
            wcscpy_s(g_popover.error,_countof(g_popover.error),job->success?L"":job->error);
            if(job->action==5&&job->success)SetWindowTextW(g_popover.input,L"");
            RefreshTaskPopover();InvalidateRect(g_list,NULL,FALSE);
        }
        g_active_popover=previous;
        UpdateLastUpdated();
        JiraFreeComments(&job->comments);free(job);return;
    }
    if(job->action==3)EndPendingSwitchFeedback();
    if(job->loaded&&!InstallSnapshot(&job->snapshot)){
        job->success=FALSE;wcscpy_s(job->error,256,L"Cannot display Jira list: memory or status limit exceeded.");job->loaded=FALSE;
    }
    if(job->action!=1)g_jira_stale=!job->loaded;
    if(job->success)g_jira_message[0]=0;
    else wcscpy_s(g_jira_message,256,job->error);
    UpdateLastUpdated();
    if(!job->success && job->action)MessageBoxW(g_window,job->error,APP_TITLE,MB_OK|MB_ICONWARNING);
    if(job->success&&job->action==1){
        HMENU menu=CreateStatusTransitionsMenu(&job->transitions);
        /* Prevent periodic refresh from changing the target while its menu is open. */
        g_jira_busy=TRUE;
        int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,job->anchor.x,job->anchor.y,0,g_window,NULL);
        g_jira_busy=FALSE;DestroyNativeMenu(menu);
        if(command>=3000&&command<3000+job->transitions.count){
            JiraTransitionItem *transition=&job->transitions.items[command-3000];
            if(transition->rating_count>0&&transition->field_count==1){
                TransitionInputDialog input;
                g_jira_busy=TRUE;BOOL accepted=ShowTransitionInputDialog(transition,job->key,&input);g_jira_busy=FALSE;
                if(accepted)StartJiraJobWithInput(2,job->key,transition->id,transition->rating_field_id,input.option_id,input.comment,(POINT){0});
            }else if(transition->field_count>0){
                MessageBoxW(g_window,g_russian?L"Этот переход содержит поля, которые Jira Task Manager пока не поддерживает. Выполните его в Jira.":L"This transition contains fields that Jira Task Manager does not support yet. Complete it in Jira.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
            }else StartJiraJob(2,job->key,transition->id,(POINT){0});
        }
    }
    JiraFreeSnapshot(&job->snapshot);JiraFreeComments(&job->comments);free(job);
}

static void OpenPortal(void)
{
    ShellExecuteW(g_window, L"open", L"https://portal.kama.gs/company/building_access.php", NULL, NULL, SW_SHOWNORMAL);
}

static void ToggleDensity(void)
{
    g_density = !g_density;
    UpdateFonts();
    UpdateTooltips();
    RECT client;
    GetClientRect(g_window, &client);
    LayoutControls(client.right, client.bottom);
    LayoutTaskPopoverContents();
    RepositionTaskPopover();RefreshTaskPopover();
    for(TaskPopover *item=g_detached_popovers;item;item=item->next){
        TaskPopover *previous=g_active_popover;g_active_popover=item;
        LayoutTaskPopoverContents();RefreshTaskPopover();g_active_popover=previous;
    }
    RedrawWindow(g_window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    SaveSettings();
}

static POINT TimerMenuAnchor(RECT timer_rect,RECT visible_frame)
{
    POINT anchor={visible_frame.left,timer_rect.bottom};return anchor;
}

static void ShowTimerMenu(void)
{
    HMENU menu = CreateNativeMenu();
    AddNativeMenuItem(menu, MF_STRING, IDM_TIMER_FORMAT,
        g_text_time ? (g_russian ? L"Показать цифрами" : L"Show digital format")
                    : (g_russian ? L"Показать словами" : L"Show text format"));
    AddNativeMenuItem(menu, MF_STRING, IDM_TIMER_RESET,
        g_russian ? L"Изменить начало дня…" : L"Change start time…");
    AddNativeMenuItem(menu, MF_STRING, IDM_TIMER_PORTAL,
        g_russian ? L"Анализ рабочего времени ↗" : L"Work time analysis ↗");
    RECT timer_rect;
    GetWindowRect(g_timer, &timer_rect);
    MarkNativeMenuPlain(menu);
    RECT visible_frame;
    if(FAILED(DwmGetWindowAttribute(g_window,DWMWA_EXTENDED_FRAME_BOUNDS,&visible_frame,sizeof(visible_frame))))
        GetWindowRect(g_window,&visible_frame);
    POINT anchor=TimerMenuAnchor(timer_rect,visible_frame);
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        anchor.x, anchor.y, 0, g_window, NULL);
    DestroyNativeMenu(menu);
    if (command == IDM_TIMER_FORMAT) {
        g_text_time = !g_text_time;
        SaveSettings();
        UpdateTimer();
    } else if (command == IDM_TIMER_RESET) {
        BeginStartEdit();
    } else if (command == IDM_TIMER_PORTAL) {
        OpenPortal();
    }
    RECT client;GetClientRect(g_window,&client);LayoutControls(client.right,client.bottom);
}

static void AddTrayIcon(void)
{
    ZeroMemory(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAYICON;
    g_tray.hIcon = LoadAppIcon(TRUE);
    wcscpy_s(g_tray.szTip, _countof(g_tray.szTip), APP_TITLE);
    Shell_NotifyIconW(NIM_ADD, &g_tray);
}

static void ShowNewTaskNotification(int count,const JiraItem *first)
{
    if(count<1)return;
    HWND foreground=GetForegroundWindow();
    if(IsWindowVisible(g_window)&&foreground&&GetAncestor(foreground,GA_ROOTOWNER)==g_window)return;
    NOTIFYICONDATAW notice=g_tray;
    notice.uFlags=NIF_INFO;
    notice.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
    wcscpy_s(notice.szInfoTitle,_countof(notice.szInfoTitle),g_russian?L"Новая задача Jira":L"New Jira task");
    if(count==1&&first)
        swprintf(notice.szInfo,_countof(notice.szInfo),L"%ls — %.180ls",first->key,first->summary);
    else swprintf(notice.szInfo,_countof(notice.szInfo),g_russian?L"Добавлено задач: %d":L"New tasks: %d",count);
    Shell_NotifyIconW(NIM_MODIFY,&notice);
}

static void EndStartEdit(void)
{
    g_editing_start=FALSE;
    ShowWindow(g_start_edit,SW_HIDE);ShowWindow(g_timer,SW_SHOWNA);
    UpdateTimer();
}

static void BeginStartEdit(void)
{
    FILETIME utc,local;ULARGE_INTEGER value;value.QuadPart=g_workday_started?g_workday_started:CurrentFileTime();
    utc.dwLowDateTime=value.LowPart;utc.dwHighDateTime=value.HighPart;
    SYSTEMTIME time;FileTimeToLocalFileTime(&utc,&local);FileTimeToSystemTime(&local,&time);
    wchar_t text[16];swprintf(text,16,L"%02u:%02u",time.wHour,time.wMinute);
    SetWindowTextW(g_start_edit,text);g_editing_start=TRUE;
    ShowWindow(g_timer,SW_HIDE);ShowWindow(g_start_edit,SW_SHOWNA);
    SetFocus(g_start_edit);SendMessageW(g_start_edit,EM_SETSEL,0,-1);
}

static LRESULT CALLBACK StartEditProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
    if(message==WM_KILLFOCUS && g_editing_start)EndStartEdit();
    if(message==WM_KEYDOWN && wp==VK_ESCAPE){EndStartEdit();SetFocus(g_timer);return 0;}
    if(message==WM_KEYDOWN && wp==VK_RETURN){
        wchar_t text[16];int h,m;GetWindowTextW(window,text,16);
        SYSTEMTIME local,utc;FILETIME file;ULARGE_INTEGER value;
        GetLocalTime(&local);
        BOOL valid=ParseStartTime(text,&h,&m);
        if(valid){
            local.wHour=(WORD)h;local.wMinute=(WORD)m;local.wSecond=local.wMilliseconds=0;
            valid=TzSpecificLocalTimeToSystemTime(NULL,&local,&utc)&&SystemTimeToFileTime(&utc,&file);
            if(valid){value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;valid=value.QuadPart<=CurrentFileTime();}
        }
        if(!valid){MessageBeep(MB_ICONWARNING);SendMessageW(window,EM_SETSEL,0,-1);return 0;}
        g_workday_started=value.QuadPart;SaveSettings();EndStartEdit();SetFocus(g_timer);return 0;
    }
    return DefSubclassProc(window,message,wp,lp);
}

static int SelectTrayTasks(int *indices,int limit)
{
    int count=0;limit=min(30,max(1,limit));
    for(int i=0;i<g_task_count&&count<limit;i++)if(g_tasks[i].assigned_to_me&&!IsDoneStatus(g_tasks[i].status))indices[count++]=i;
    return count;
}
static void ShowTrayMenu(void)
{
    HMENU menu = CreateNativeMenu();
    const wchar_t *active_label = g_russian ? L"Нет активной задачи" : L"No active task";
    wchar_t active_text[128];
    for (int i = 0; i < g_task_count; ++i) {
        if (g_tasks[i].active) {
            swprintf(active_text, 128, L"%ls · %ls", g_tasks[i].key, g_tasks[i].summary);
            active_label = active_text;
            break;
        }
    }
    AddNativeMenuItem(menu, MF_STRING | MF_DISABLED, 0, active_label);
    AddNativeMenuItem(menu, MF_SEPARATOR, 0, NULL);
    int indices[30];wchar_t keys[30][80];int count=SelectTrayTasks(indices,g_tray_task_count);
    for(int i=0;i<count;i++){
        TaskItem *task=&g_tasks[indices[i]];wcscpy_s(keys[i],80,task->key);
        wchar_t label[160];swprintf(label,160,L"%ls  %.60ls",task->key,task->summary);
        AddNativeMenuItem(menu,MF_STRING|(task->active?MF_CHECKED:0)|((g_jira_busy||g_jira_stale)?MF_DISABLED:0),2400+i,label);
    }
    if(count)AddNativeMenuItem(menu,MF_SEPARATOR,0,NULL);
    AddNativeMenuItem(menu, MF_STRING, IDM_TRAY_OPEN, g_russian ? L"Открыть Jira Task Manager" : L"Open Jira Task Manager");
    AddNativeMenuItem(menu, MF_STRING, IDM_TRAY_EXIT, g_russian ? L"Выйти" : L"Exit");
    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(g_window);
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN,
        cursor.x, cursor.y, 0, g_window, NULL);
    DestroyNativeMenu(menu);
    if(command>=2400&&command<2400+count)StartJiraJob(3,keys[command-2400],NULL,(POINT){0});
    if (command == IDM_TRAY_OPEN) {
        ShowWindow(g_window, SW_RESTORE);
        SetForegroundWindow(g_window);
    } else if (command == IDM_TRAY_EXIT) {
        g_shutdown_reason = L"user_tray_exit";
        g_allow_close = TRUE;
        SendMessageW(g_window,WM_CLOSE,0,0);
    }
    PostMessageW(g_window, WM_NULL, 0, 0);
}

static void CreateTaskList(void)
{
    g_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, g_window, (HMENU)(INT_PTR)IDC_TASKS, g_instance, NULL);
    ListView_SetExtendedListViewStyle(g_list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SetControlFont(g_list);
    SetWindowSubclass(g_list,ListProcedure,1,0);
    const wchar_t *headers[] = { L"#", L"ЗАДАЧА", L"НАЗВАНИЕ", L"КОММЕНТАРИИ", L"СТАТУС" };
    int widths[] = { 42, 126, 420, 0, 160 };
    for (int i = 0; i < 5; ++i) {
        LVCOLUMNW column = { 0 };
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        column.pszText = (wchar_t *)headers[i];
        column.cx = widths[i];
        column.fmt = i == 0 ? LVCFMT_RIGHT : LVCFMT_LEFT;
        ListView_InsertColumn(g_list, i, &column);
    }
}

static void CreateControls(void)
{
    g_dpi=GetDpiForWindow(g_window);
    UpdateFonts();

    g_search = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, g_window, (HMENU)(INT_PTR)IDC_SEARCH, g_instance, NULL);
    SetControlFont(g_search);
    g_clear_search = CreateOwnerButton(g_window, IDC_CLEAR_SEARCH, L"Clear search");
    g_search_count=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_RIGHT|SS_CENTERIMAGE,
        0,0,0,0,g_window,NULL,g_instance,NULL);
    g_filter = CreateOwnerButton(g_window, IDC_FILTER, L"");
    g_tab_all = CreateOwnerButton(g_window, IDC_TAB_ALL, L"");
    g_tab_current = CreateOwnerButton(g_window, IDC_TAB_CURRENT, L"");
    g_tab_done = CreateOwnerButton(g_window, IDC_TAB_DONE, L"");
    g_tab_reported = CreateOwnerButton(g_window, IDC_TAB_REPORTED, L"");
    CreateTaskList();
    HWND header=ListView_GetHeader(g_list);
    SetWindowLongPtrW(header,GWL_STYLE,GetWindowLongPtrW(header,GWL_STYLE)|WS_CLIPCHILDREN);
    SetWindowSubclass(header,HeaderFilterProcedure,1,0);
    SetParent(g_filter,header);
    g_note_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,
        0,0,0,0,g_list,(HMENU)(INT_PTR)IDC_NOTE_EDIT,g_instance,NULL);
    SendMessageW(g_note_edit,EM_SETLIMITTEXT,2047,0);SetWindowSubclass(g_note_edit,NoteEditProcedure,1,0);
    g_timer = CreateOwnerButton(g_window, IDC_TIMER, L"00:00:00");
    g_start_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,
        0,0,0,0,g_window,(HMENU)(INT_PTR)IDC_START_EDIT,g_instance,NULL);
    SendMessageW(g_start_edit,EM_SETLIMITTEXT,5,0);
    SetWindowSubclass(g_start_edit,StartEditProcedure,1,0);
    g_updated = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 0, 0, g_window, (HMENU)(INT_PTR)IDC_UPDATED, g_instance, NULL);
    SetControlFont(g_updated);
    g_refresh = CreateOwnerButton(g_window, IDC_REFRESH, L"↻");
    g_comments = CreateOwnerButton(g_window, IDC_COMMENTS, L"");
    ShowWindow(g_comments,SW_HIDE);
    g_theme = CreateOwnerButton(g_window, IDC_THEME, g_dark ? L"☀" : L"☾");
    g_language = CreateOwnerButton(g_window, IDC_LANGUAGE, g_russian ? L"RU" : L"EN");
    g_settings = CreateOwnerButton(g_window, IDC_SETTINGS, L"···");
    g_density_button = CreateOwnerButton(g_window, IDC_DENSITY, L"Aa");
    UpdateFonts();
    g_tooltips=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,g_window,NULL,g_instance,NULL);
    UpdateLanguage();
    ApplyTheme();
}

static void UpdateFonts(void)
{
    HFONT previous=g_font,previous_small=g_small_font,previous_bold=g_bold_font;
    HFONT previous_symbol=g_symbol_font;
    HFONT previous_timer=g_timer_font;
    g_timer_font=CreateFontW(-D(11),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Semibold");
    g_symbol_font=CreateFontW(-D(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe MDL2 Assets");
    g_font=CreateFontW(-D(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_small_font=CreateFontW(-D(11),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    g_bold_font=CreateFontW(-D(13),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Semibold");
    if(g_search_count)SendMessageW(g_search_count,WM_SETFONT,(WPARAM)g_small_font,TRUE);
    if(g_start_edit)SendMessageW(g_start_edit,WM_SETFONT,(WPARAM)g_font,TRUE);
    HWND controls[]={g_search,g_filter,g_tab_all,g_tab_current,g_tab_done,g_tab_reported,g_list,g_note_edit,g_timer,g_updated,g_refresh,g_comments,g_theme,g_language,g_settings,g_density_button};
    for(int i=0;i<16;i++)if(controls[i])SendMessageW(controls[i],WM_SETFONT,(WPARAM)(controls[i]==g_timer?g_timer_font:controls[i]==g_updated?g_small_font:g_font),TRUE);
    if(g_popover.input)SendMessageW(g_popover.input,WM_SETFONT,(WPARAM)g_font,TRUE);
    if(g_popover.send)SendMessageW(g_popover.send,WM_SETFONT,(WPARAM)g_font,TRUE);
    if(g_popover.text)SendMessageW(g_popover.text,WM_SETFONT,(WPARAM)g_font,TRUE);
    for(TaskPopover *item=g_detached_popovers;item;item=item->next){
        if(item->input)SendMessageW(item->input,WM_SETFONT,(WPARAM)g_font,TRUE);
        if(item->send)SendMessageW(item->send,WM_SETFONT,(WPARAM)g_font,TRUE);
        if(item->text)SendMessageW(item->text,WM_SETFONT,(WPARAM)g_font,TRUE);
    }
    if(g_list){
        HIMAGELIST previous_height=g_row_height;
        g_row_height=ImageList_Create(1,D(28),ILC_COLOR32,1,1);
        ListView_SetImageList(g_list,g_row_height,LVSIL_SMALL);
        if(previous_height)ImageList_Destroy(previous_height);
    }
    if(previous)DeleteObject(previous);
    if(previous_small)DeleteObject(previous_small);
    if(previous_bold)DeleteObject(previous_bold);
    if(previous_symbol)DeleteObject(previous_symbol);
    if(previous_timer)DeleteObject(previous_timer);
}

static void UpdateTooltips(void)
{
    if(!g_tooltips)return;
    SetWindowTextW(g_clear_search,g_russian?L"Очистить поиск":L"Clear search");
    TOOLINFOW clear_tip={0};clear_tip.cbSize=sizeof(clear_tip)-sizeof(void*);
    clear_tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;clear_tip.hwnd=g_window;
    clear_tip.uId=(UINT_PTR)g_clear_search;
    clear_tip.lpszText=g_russian?L"Очистить поиск":L"Clear search";
    SendMessageW(g_tooltips,TTM_DELTOOLW,0,(LPARAM)&clear_tip);
    SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&clear_tip);
    TOOLINFOW density_tip={0};density_tip.cbSize=sizeof(density_tip)-sizeof(void*);
    density_tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;density_tip.hwnd=g_window;
    density_tip.uId=(UINT_PTR)g_density_button;
    density_tip.lpszText=g_russian
        ? (g_density?L"Крупный → переключить на обычный":L"Обычный → переключить на крупный")
        : (g_density?L"Large → switch to regular":L"Regular → switch to large");
    SendMessageW(g_tooltips,TTM_DELTOOLW,0,(LPARAM)&density_tip);
    SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&density_tip);
    TOOLINFOW list_tip={0};list_tip.cbSize=sizeof(list_tip)-sizeof(void*);
    list_tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;list_tip.hwnd=g_window;list_tip.uId=(UINT_PTR)g_list;
    list_tip.lpszText=LPSTR_TEXTCALLBACKW;
    SendMessageW(g_tooltips,TTM_DELTOOLW,0,(LPARAM)&list_tip);
    SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&list_tip);
    SendMessageW(g_tooltips,TTM_SETMAXTIPWIDTH,0,D(520));
    HWND buttons[]={g_filter,g_refresh,g_comments,g_theme,g_language,g_settings,g_timer,g_tab_reported};
    int attention=OtherScopeAttentionCount();
    swprintf(g_scope_tip,_countof(g_scope_tip),g_russian?
        (g_reported_scope?L"Сейчас: поставлены мной · перейти к назначенным мне%ls":
            L"Сейчас: назначены мне · перейти к поставленным мной%ls"):
        (g_reported_scope?L"Showing: reported by me · switch to assigned to me%ls":
            L"Showing: assigned to me · switch to reported by me%ls"),
        attention?(g_russian?L" · в другом режиме новые задачи или непрочитанные комментарии":
            L" · new tasks or unread comments in the other mode"):L"");
    const wchar_t *ru[]={L"Фильтр статусов этой вкладки",L"Обновить задачи Jira",g_comments_mode==0?L"Без комментариев → комментарии Jira":g_comments_mode==1?L"Комментарии Jira → локальные заметки":L"Локальные заметки → без комментариев",L"Переключить светлую / тёмную тему",L"Switch language / Язык",L"О приложении и подключении",L"Формат времени и анализ рабочего времени",g_scope_tip};
    const wchar_t *en[]={L"Status filter for this tab",L"Refresh Jira tasks",g_comments_mode==0?L"No comments → Jira comments":g_comments_mode==1?L"Jira comments → local notes":L"Local notes → no comments",L"Switch light / dark theme",L"Язык / Switch language",L"About and connection",L"Time format and work time analysis",g_scope_tip};
    for(int i=0;i<8;i++){
        TOOLINFOW tip={0};tip.cbSize=sizeof(tip)-sizeof(void*);tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tip.hwnd=g_window;tip.uId=(UINT_PTR)buttons[i];tip.lpszText=(wchar_t*)(g_russian?ru[i]:en[i]);
        SendMessageW(g_tooltips,TTM_DELTOOLW,0,(LPARAM)&tip);
        SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&tip);
    }
}

#include "native_update_ui.h"
#include "native_settings.h"

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if(UpdateUiHandleMessage(message,w_param,l_param))return 0;
    switch (message) {
    case WM_CREATE:
        g_window = window;
        CreateControls();
        SetTimer(window, 1, 1000, NULL);
        SetTimer(window, 3, g_refresh_minutes*60*1000, NULL);
        AddTrayIcon();
        UpdateLastUpdated();
        PostMessageW(window,WM_COMMAND,IDC_REFRESH,0);
        WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION);
        StartWorkdayForInteractiveSession();
        return 0;

    case WM_WTSSESSION_CHANGE:
        if(w_param==WTS_SESSION_LOGON||w_param==WTS_SESSION_UNLOCK)StartWorkdayForInteractiveSession();
        return 0;

    case WM_SIZE:
        if(g_popover.window)SendMessageW(g_popover.window,WM_CLOSE,0,0);
        if(g_list)SendMessageW(g_list,WM_SETREDRAW,FALSE,0);
        LayoutControls(LOWORD(l_param), HIWORD(l_param));
        if(g_list){SendMessageW(g_list,WM_SETREDRAW,TRUE,0);InvalidateRect(g_list,NULL,FALSE);}
        RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
        return 0;

    case WM_MOVE:
        if(g_popover.window)SendMessageW(g_popover.window,WM_CLOSE,0,0);
        return 0;

    case WM_EXITSIZEMOVE:
        RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)l_param;
        info->ptMinTrackSize.x = MulDiv(510, (int)g_dpi, 96);
        info->ptMinTrackSize.y = MulDiv(310, (int)g_dpi, 96);
        return 0;
    }

    case WM_DPICHANGED: {
        g_dpi=HIWORD(w_param);UpdateFonts();
        RECT *suggested=(RECT*)l_param;
        SetWindowPos(window,NULL,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
        FillRect(dc,&paint.rcPaint,g_window_brush);
        HPEN pen=CreatePen(PS_SOLID,1,BorderColor());HGDIOBJ previous=SelectObject(dc,pen),brush=SelectObject(dc,g_panel_brush);
        RoundRect(dc,g_search_frame.left,g_search_frame.top,g_search_frame.right,g_search_frame.bottom,D(5),D(5));
        SelectObject(dc,brush);SelectObject(dc,previous);DeleteObject(pen);
        DrawControlIcon(dc,IDC_SEARCH,g_search_frame.left+D(13),(g_search_frame.top+g_search_frame.bottom)/2,MutedColor());
        HPEN separator=CreatePen(PS_SOLID,1,BorderColor());HGDIOBJ old_separator=SelectObject(dc,separator);
        MoveToEx(dc,g_reported_separator_x,D(16),NULL);LineTo(dc,g_reported_separator_x,D(32));
        SelectObject(dc,old_separator);DeleteObject(separator);
        EndPaint(window,&paint);return 0;
    }

    case WM_TIMER:
        if(w_param==6)AnimateFooterTick();
        else if(w_param==3)StartJiraJob(0,NULL,NULL,(POINT){0});
        else UpdateTimer();
        return 0;

    case WM_JIRA_RESULT:
        FinishJiraJob((JiraJob*)l_param);return 0;

    case WM_COMMAND: {
        int id = LOWORD(w_param);
        int code = HIWORD(w_param);
        if (id == IDC_SEARCH && code == EN_CHANGE) RefreshTaskList();
        else if (id == IDC_CLEAR_SEARCH) {
            SetWindowTextW(g_search,L"");
            SetFocus(g_search);
        }
        else if (id == IDC_FILTER) {
            if(g_filter_menu_open){EndMenu();g_filter_dismissed_tick=GetTickCount();}
            else if(g_filter_dismissed_tick&&GetTickCount()-g_filter_dismissed_tick<200)g_filter_dismissed_tick=0;
            else {g_filter_dismissed_tick=0;ShowFilterMenu();}
        }
        else if (id == IDC_DENSITY) ToggleDensity();
        else if (id == IDC_TAB_REPORTED) {
            ToggleReportedScope();
        } else if (id == IDC_TAB_ALL || id == IDC_TAB_CURRENT || id == IDC_TAB_DONE) {
            g_tab = id == IDC_TAB_ALL ? 0 : (id == IDC_TAB_CURRENT ? 1 : 2);
            g_scope_tabs[g_reported_scope?1:0]=g_tab;
            RefreshTaskList();
            InvalidateRect(g_tab_all, NULL, TRUE);
            InvalidateRect(g_tab_current, NULL, TRUE);
            InvalidateRect(g_tab_done, NULL, TRUE);
            InvalidateRect(g_tab_reported, NULL, TRUE);
        } else if (id == IDC_TIMER) ShowTimerMenu();
        else if (id == IDC_REFRESH) StartJiraJob(0,NULL,NULL,(POINT){0});
        else if (id == IDC_COMMENTS) CycleCommentsMode();
        else if (id == IDC_THEME) {
            g_dark = !g_dark;
            SetWindowTextW(g_theme,g_russian?L"Переключить тему":L"Switch theme");
            ApplyTheme();
            if(s_window&&IsWindow(s_window)){
                ApplyWindowCaptionTheme(s_window);
                RedrawWindow(s_window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
            }
            SaveSettings();
        } else if (id == IDC_LANGUAGE) {
            g_russian = !g_russian;
            UpdateLanguage();
            SaveSettings();
            RECT client;
            GetClientRect(window, &client);
            LayoutControls(client.right, client.bottom);
        } else if (id == IDC_SETTINGS) {
            OpenSettingsWindow();
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR *header = (NMHDR *)l_param;
        if(header->hwndFrom==g_tooltips&&header->code==TTN_GETDISPINFOW&&header->idFrom==(UINT_PTR)g_list){
            NMTTDISPINFOW *tip=(NMTTDISPINFOW*)l_param;g_summary_tip[0]=0;POINT cursor;
            if(GetCursorPos(&cursor)){
                ScreenToClient(g_list,&cursor);LVHITTESTINFO hit={0};hit.pt=cursor;
                int row=ListView_SubItemHitTest(g_list,&hit);
                if(row>=0&&CommentIndicatorTaskAtPoint(cursor)>=0)
                    wcscpy_s(g_summary_tip,_countof(g_summary_tip),g_russian?L"Открыть комментарии Jira":L"Open Jira comments");
                else if(row>=0&&NoteIndicatorTaskAtPoint(cursor)>=0)TaskNoteInfoTip(row,g_summary_tip,_countof(g_summary_tip));
                else if(row>=0&&hit.iSubItem==2)TaskSummaryInfoTip(row,g_summary_tip,_countof(g_summary_tip));
            }
            tip->lpszText=g_summary_tip;return 0;
        }
        if(header->idFrom==IDC_TASKS&&header->code==LVN_COLUMNCLICK){
            ApplyTaskSort(((NMLISTVIEW*)l_param)->iSubItem);return 0;
        }
        if (header->idFrom == IDC_TASKS && header->code == NM_CUSTOMDRAW)
            return DrawListCustom((NMLVCUSTOMDRAW *)l_param);
        if (header->idFrom == IDC_TASKS && header->code == NM_DBLCLK) {
            NMITEMACTIVATE *activate = (NMITEMACTIVATE *)l_param;
            BOOL status_pill=StatusTaskAtPoint(activate->ptAction)>=0,note_indicator=NoteIndicatorTaskAtPoint(activate->ptAction)>=0,
                comment_indicator=CommentIndicatorTaskAtPoint(activate->ptAction)>=0;
            if(activate->iSubItem!=1&&!status_pill&&!note_indicator&&!comment_indicator&&activate->iSubItem!=COMMENTS_COLUMN)
                ActivateTask(TaskIndexAtRow(activate->iItem));
            return 0;
        }
        if (header->idFrom == IDC_TASKS && header->code == NM_CLICK) {
            NMITEMACTIVATE *activate = (NMITEMACTIVATE *)l_param;
            int note_task=NoteIndicatorTaskAtPoint(activate->ptAction);
            int comment_task=CommentIndicatorTaskAtPoint(activate->ptAction);
            /* Hit-test before marking the row seen: removing NEW can move its icons. */
            if(activate->iItem>=0)MarkTaskSeen(TaskIndexAtRow(activate->iItem));
            if(comment_task>=0){
                RECT icon;if(CommentIndicatorRectForRow(activate->iItem,&icon))OpenTaskPopover(comment_task,1,icon);
            } else if(note_task>=0){
                RECT icon;if(NoteIndicatorRectForRow(activate->iItem,&icon))OpenTaskPopover(note_task,2,icon);
            } else if (activate->iItem >= 0 && activate->iSubItem == STATUS_COLUMN) {
                CloseTaskPopover();
                int task_index=StatusTaskAtPoint(activate->ptAction);
                if(task_index>=0){
                    RECT pill;StatusPillRectForRow(activate->iItem,&pill);
                    POINT point={pill.left,pill.bottom};ClientToScreen(g_list,&point);
                    ShowStatusMenu(task_index,point);
                }
            } else if(activate->iItem>=0&&activate->iSubItem==1){
                CloseTaskPopover();
                int index=TaskIndexAtRow(activate->iItem);if(index>=0)OpenTaskInJira(g_tasks[index].key);
            }else CloseTaskPopover();
            return 0;
        }
        break;
    }

    case WM_DRAWITEM:
        if(((DRAWITEMSTRUCT*)l_param)->CtlType==ODT_MENU){DrawNativeMenuItem((const DRAWITEMSTRUCT*)l_param);return TRUE;}
        if(((DRAWITEMSTRUCT*)l_param)->CtlID==IDC_UPDATED){DrawUpdatedLabel((const DRAWITEMSTRUCT*)l_param);return TRUE;}
        DrawOwnerButton((const DRAWITEMSTRUCT *)l_param);
        return TRUE;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *measure=(MEASUREITEMSTRUCT*)l_param;
        if(measure->CtlType!=ODT_MENU)break;
        PopupItem *item=(PopupItem*)measure->itemData;if(!item)break;
        HDC dc=GetDC(window);HGDIOBJ font=SelectObject(dc,g_font);SIZE size;
        GetTextExtentPoint32W(dc,item->text,(int)wcslen(item->text),&size);
        SelectObject(dc,font);ReleaseDC(window,dc);
        measure->itemWidth=size.cx+D(item->plain?12:32);measure->itemHeight=D(item->separator?9:28);return TRUE;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w_param;
        SetTextColor(dc, TextColor());
        SetBkColor(dc, PanelColor());
        return (LRESULT)g_panel_brush;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w_param;
        SetTextColor(dc, MutedColor());
        if((HWND)l_param==g_search_count){SetBkColor(dc,PanelColor());return (LRESULT)g_panel_brush;}
        SetBkColor(dc, WindowColor());
        return (LRESULT)g_window_brush;
    }

    case WM_ERASEBKGND: {
        RECT client;
        GetClientRect(window, &client);
        FillRect((HDC)w_param, &client, g_window_brush);
        return TRUE;
    }

    case WM_TRAYICON:
        if (l_param == WM_LBUTTONDBLCLK || l_param == NIN_BALLOONUSERCLICK) {
            ShowWindow(window, SW_RESTORE);
            SetForegroundWindow(window);
        } else if (l_param == WM_RBUTTONUP || l_param == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;

    case WM_QUERYENDSESSION:
        g_shutdown_reason = (l_param & ENDSESSION_LOGOFF) ? L"windows_logoff" : L"windows_shutdown";
        return TRUE;

    case WM_ENDSESSION:
        if (w_param) NativeLifecycleStop(&g_lifecycle, g_shutdown_reason);
        return 0;

    case WM_CLOSE:
        if(g_allow_close&&g_jira_busy){
            g_allow_close=FALSE;
            MessageBoxW(window,g_russian?L"Дождитесь завершения запроса Jira перед выходом.":L"Wait for the Jira request to finish before exiting.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
            return 0;
        }
        if(g_popover.window)SendMessageW(g_popover.window,WM_CLOSE,0,0);
        if(g_popover.window)return 0;
        if(g_allow_close){
            while(g_detached_popovers){
                TaskPopover *item=g_detached_popovers;
                SendMessageW(item->window,WM_CLOSE,0,0);
                if(g_detached_popovers==item)return 0;
            }
        }
        if (!g_allow_close) {
            NativeLifecycleEvent(&g_lifecycle,L"window_hidden",L"reason=close_button; application_remains_in_tray");
            ShowWindow(window, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        WTSUnRegisterSessionNotification(window);
        InterlockedExchange(&g_jira_closing,1);
        UpdateUiShutdown();
        if(s_window)DestroyWindow(s_window);
        FreeTaskItems();JiraFreeSnapshot(&g_snapshot);
        SaveSettings();
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
        if (g_font) DeleteObject(g_font);
        if (g_small_font) DeleteObject(g_small_font);
        if (g_timer_font) DeleteObject(g_timer_font);
        if (g_symbol_font) DeleteObject(g_symbol_font);
        if (g_bold_font) DeleteObject(g_bold_font);
        if (g_row_height) ImageList_Destroy(g_row_height);
        if (g_window_brush) DeleteObject(g_window_brush);
        if (g_panel_brush) DeleteObject(g_panel_brush);
        if (g_popover_brush) DeleteObject(g_popover_brush);
        NativeLifecycleStop(&g_lifecycle,g_shutdown_reason);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

static LONG WINAPI NativeCrashFilter(EXCEPTION_POINTERS *exception)
{
    DWORD code = exception && exception->ExceptionRecord ? exception->ExceptionRecord->ExceptionCode : 0;
    NativeLifecycleCrash(&g_lifecycle,code);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    (void)previous;
    int update_argc=0;
    wchar_t **update_argv=CommandLineToArgvW(GetCommandLineW(),&update_argc);
    if(update_argv&&update_argc>1&&!wcscmp(update_argv[1],L"--apply-update")){
        int update_result=UpdateRunHelper(update_argc,update_argv);
        LocalFree(update_argv);
        return update_result;
    }
    if(update_argv)LocalFree(update_argv);
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Task Manager.Singleton");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"Task Manager.Window", NULL);
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    wchar_t executable_path[MAX_PATH];
    DWORD executable_length=GetModuleFileNameW(NULL,executable_path,MAX_PATH);
    if(executable_length>0&&executable_length<MAX_PATH)
        NativeLifecycleStart(&g_lifecycle,executable_path,APP_VERSION,wcsstr(command_line,L"--startup")!=NULL);
    SetUnhandledExceptionFilter(NativeCrashFilter);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_dpi=GetDpiForSystem();
    g_instance = instance;
    InitializeSettingsPath();
    if(!TaskManagerMigrateCredential())NativeLifecycleEvent(&g_lifecycle,L"migration_warning",L"reason=credential_migration_failed");
    LoadSettings();
    g_comments_mode=g_comments_visual_mode=g_comments_reveal=0;
    RecreateBrushes();

    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&controls);

    WNDCLASSEXW window_class = { 0 };
    window_class.cbSize = sizeof(window_class);
    window_class.style = 0;
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadAppIcon(FALSE);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    /* WM_ERASEBKGND paints with the current theme brush; a class brush would
       keep a handle to the deleted brush after a theme switch. */
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = L"Task Manager.Window";
    window_class.hIconSm = LoadAppIcon(TRUE);
    if (!RegisterClassExW(&window_class)) {
        NativeLifecycleStop(&g_lifecycle,L"startup_failure_register_window");
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    g_window = CreateWindowExW(0, window_class.lpszClassName, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(510, (int)g_dpi, 96), MulDiv(420, (int)g_dpi, 96),
        NULL, NULL, instance, NULL);
    if (!g_window) {
        NativeLifecycleStop(&g_lifecycle,L"startup_failure_create_window");
        if (mutex) CloseHandle(mutex);
        return 2;
    }

    ShowWindow(g_window, wcsstr(command_line,L"--startup")?SW_HIDE:SW_SHOWNORMAL);
    UpdateWindow(g_window);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if(s_window&&(message.hwnd==s_window||IsChild(s_window,message.hwnd))&&IsDialogMessageW(s_window,&message))continue;
        if (!IsDialogMessageW(g_window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    NativeLifecycleStop(&g_lifecycle,L"message_loop_exit");
    if (mutex) CloseHandle(mutex);
    return (int)message.wParam;
}
