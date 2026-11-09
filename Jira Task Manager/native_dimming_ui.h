/* Local presentation marks. Reuse the transactional key set in its own file. */
static NativeBookmarks g_dimmed;
static wchar_t g_dimmed_server[800],g_dimmed_account[256];
static BOOL g_dimmed_ready;
static ULONGLONG g_dimmed_generation;
static BOOL TaskDimmed(const TaskItem *task){return g_dimmed_ready&&!task->active&&NativeBookmarksContains(&g_dimmed,task->key);}
static HFONT TaskTitleFont(const TaskItem *task){return task->active?g_bold_font:TaskDimmed(task)&&g_italic_font?g_italic_font:g_font;}
static void TaskDimmingSyncIdentity(void){
    const wchar_t *account=g_snapshot.identity.account_id[0]?g_snapshot.identity.account_id:g_snapshot.identity.key[0]?g_snapshot.identity.key:g_snapshot.identity.name;
    const wchar_t *server=account[0]?g_snapshot.server:L"";
    if(!wcscmp(server,g_dimmed_server)&&!wcscmp(account,g_dimmed_account)&&(g_dimmed_ready||!server[0]||!account[0]))return;
    ++g_dimmed_generation;NativeBookmarksFree(&g_dimmed);g_dimmed_ready=FALSE;
    wcscpy_s(g_dimmed_server,_countof(g_dimmed_server),server);wcscpy_s(g_dimmed_account,_countof(g_dimmed_account),account);
    if(server[0]&&account[0])g_dimmed_ready=NativeTaskMarksLoad(&g_dimmed,g_settings_path,server,account,TASK_MARK_DIMMED);
}
static BOOL TaskDimmingStore(void){
    if(NativeTaskMarksCommit(&g_dimmed,g_settings_path,g_dimmed_server,g_dimmed_account,TASK_MARK_DIMMED))return TRUE;
    NativeBookmarksCancel(&g_dimmed);return FALSE;
}
static void TaskDimmingClearActive(void){
    if(!g_dimmed_ready)return;
    BOOL changed=FALSE;
    for(int i=0;i<g_task_count;i++)if(g_tasks[i].active&&NativeBookmarksContains(&g_dimmed,g_tasks[i].key)){
        if(!changed&&!NativeBookmarksBegin(&g_dimmed))return;
        changed=TRUE;if(!NativeBookmarksSet(&g_dimmed,g_tasks[i].key,FALSE)){NativeBookmarksCancel(&g_dimmed);return;}
    }
    if(changed)TaskDimmingStore();
}
static void TaskDimmingToggle(const wchar_t *key){
    int index=TaskByKey(key);if(index<0||!g_dimmed_ready||g_tasks[index].active)return;
    BOOL dimmed=NativeBookmarksContains(&g_dimmed,key);
    if(!NativeBookmarksBegin(&g_dimmed)||!NativeBookmarksSet(&g_dimmed,key,!dimmed)||!TaskDimmingStore()){
        NativeBookmarksCancel(&g_dimmed);
        MessageBoxW(g_window,g_russian?L"Не удалось сохранить вид задачи. Проверьте доступ к папке данных.":L"Could not save the task appearance. Check access to the data folder.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    RowLiftClear(FALSE);SortTaskList();
    g_hover_row=-1;g_hover_status=g_hover_key=g_hover_note=g_hover_comment=FALSE;
    g_pressed_indicator_task=-1;g_pressed_indicator_kind=0;
    RepositionTaskPopover();InvalidateRect(g_list,NULL,FALSE);
}
static void TaskDimmingPaint(HDC dc,RECT row,COLORREF background){
    RECT client;GetClientRect(g_list,&client);RECT area;if(!IntersectRect(&area,&row,&client))return;
    int width=area.right-area.left,height=area.bottom-area.top;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    DWORD *pixels=NULL;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);if(!bitmap)return;
    HDC memory=CreateCompatibleDC(dc);if(!memory){DeleteObject(bitmap);return;}
    HGDIOBJ old=SelectObject(memory,bitmap);
    if(BitBlt(memory,0,0,width,height,dc,area.left,area.top,SRCCOPY)){
        GdiFlush();int base=(54*GetRValue(background)+183*GetGValue(background)+19*GetBValue(background))/256;
        for(size_t i=0;i<(size_t)width*height;i++){
            DWORD p=pixels[i];int gray=(54*((p>>16)&255)+183*((p>>8)&255)+19*(p&255))/256;
            gray=(gray*3+base*2)/5;pixels[i]=(DWORD)(gray*0x010101);
        }
        BitBlt(dc,area.left,area.top,width,height,memory,0,0,SRCCOPY);
    }
    SelectObject(memory,old);DeleteDC(memory);DeleteObject(bitmap);
}
