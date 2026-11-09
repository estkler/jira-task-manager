/* Due display shares the existing list, font, clock and tooltip infrastructure. */
static int g_due_day_key;
static BOOL g_due_bar_mode;
static ULONGLONG g_due_minute;
static wchar_t g_due_press_key[80];
static int DueDateTaskAtPoint(POINT point)
{
    LVHITTESTINFO hit={0};hit.pt=point;int row=ListView_SubItemHitTest(g_list,&hit);
    int index=TaskIndexAtRow(row);
    return hit.iSubItem==DUE_COLUMN&&index>=0&&index<g_task_count&&NativeDueParse(g_tasks[index].due_date,NULL)?index:-1;
}
static void ToggleDueDateMode(void)
{
    BOOL mode=!g_due_bar_mode;
    if(WritePrivateProfileStringW(L"Interface",L"DueDateBar",mode?L"1":L"0",g_settings_path)){
        g_due_bar_mode=mode;InvalidateRect(g_list,NULL,FALSE);
    }
}
static BOOL DueDateListMessage(HWND window,UINT message,LPARAM lp)
{
    POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};
    if(message==WM_CANCELMODE||message==WM_CAPTURECHANGED){g_due_press_key[0]=0;return FALSE;}
    if(message==WM_CONTEXTMENU){
        if(point.x==-1&&point.y==-1)return FALSE;
        ScreenToClient(window,&point);
        if(DueDateTaskAtPoint(point)<0)return FALSE;
        ToggleDueDateMode();return TRUE;
    }
    if(message==WM_RBUTTONDOWN||message==WM_RBUTTONDBLCLK){
        g_due_press_key[0]=0;int index=DueDateTaskAtPoint(point);
        if(index<0)return FALSE;
        wcscpy_s(g_due_press_key,_countof(g_due_press_key),g_tasks[index].key);SetCapture(window);return TRUE;
    }
    if(message==WM_RBUTTONUP&&g_due_press_key[0]){
        int index=DueDateTaskAtPoint(point);BOOL same=index>=0&&!wcscmp(g_due_press_key,g_tasks[index].key);
        g_due_press_key[0]=0;if(GetCapture()==window)ReleaseCapture();
        if(same)ToggleDueDateMode();return TRUE;
    }
    return FALSE;
}
static BOOL TaskDueDescribe(const TaskItem *task,NativeDueText *text)
{
    SYSTEMTIME today;GetLocalTime(&today);
    return NativeDueDescribe(task?task->due_date:NULL,&today,task&&IsDoneStatus(task->status),g_russian,text);
}
static COLORREF TaskDueColor(int kind)
{
    if(kind==NATIVE_DUE_TODAY)return g_dark?RGB(240,189,98):RGB(148,96,0);
    if(kind==NATIVE_DUE_OVERDUE)return g_dark?RGB(255,146,144):RGB(179,53,50);
    return MutedColor();
}
static int DueDateColumnWidth(HDC dc)
{
    HGDIOBJ previous=SelectObject(dc,g_small_font);int width=0;BOOL any=FALSE;
    for(int i=0;i<g_task_count;i++){
        NativeDueText text;if(!TaskDueDescribe(&g_tasks[i],&text))continue;any=TRUE;
        SIZE size;GetTextExtentPoint32W(dc,text.label,(int)wcslen(text.label),&size);width=max(width,size.cx);
    }
    SelectObject(dc,previous);return any?max(D(64),width+D(16)):0;
}
static BOOL TaskDueDisplayForWidth(HDC dc,const TaskItem *task,int width,NativeDueText *text)
{
    if(!TaskDueDescribe(task,text))return FALSE;
    HGDIOBJ previous=SelectObject(dc,g_small_font);SIZE size;
    GetTextExtentPoint32W(dc,text->label,(int)wcslen(text->label),&size);SelectObject(dc,previous);
    if(size.cx+D(14)>width){SYSTEMTIME today;GetLocalTime(&today);NativeDueCompactLabel(task->due_date,&today,text);}
    return TRUE;
}
static BOOL TaskDueBarAmount(const TaskItem *task,int *amount)
{
    return g_due_bar_mode&&NativeDueProgress(task->created_utc,task->due_date,CurrentFileTime(),amount);
}
static void DrawTaskDueBar(HDC dc,RECT cell,const TaskItem *task,int kind,int amount)
{
    int width=min(D(88),cell.right-cell.left),height=D(6);if(width<=0||height<=0)return;
    int x=cell.left+(cell.right-cell.left-width)/2,y=cell.top+(cell.bottom-cell.top-height)/2;
    HRGN shape=CreateRoundRectRgn(x,y,x+width+1,y+height+1,height,height);if(!shape)return;
    COLORREF color=amount<0||IsDoneStatus(task->status)?MutedColor():kind==NATIVE_DUE_NORMAL?AccentColor():TaskDueColor(kind);
    HBRUSH track=CreateSolidBrush(g_dark?RGB(66,70,76):RGB(224,229,235)),fill=CreateSolidBrush(color);
    FillRgn(dc,shape,track);int saved=SaveDC(dc);
    if(saved){
        if(amount>=0){IntersectClipRect(dc,x,y,x+MulDiv(width,amount,1000),y+height+1);FillRgn(dc,shape,fill);}
        else{ /* Unknown duration is a bar, but never an invented percentage. */
            int dot=max(1,D(2)),middle=x+width/2;
            for(int i=-1;i<=1;i++){RECT r={middle+i*D(6)-dot/2,y+(height-dot)/2,middle+i*D(6)-dot/2+dot,y+(height-dot)/2+dot};FillRect(dc,&r,fill);}
        }
        RestoreDC(dc,saved);
    }
    DeleteObject(track);DeleteObject(fill);DeleteObject(shape);
}
static void DrawTaskDueDate(HDC dc,int row)
{
    int index=TaskIndexAtRow(row);NativeDueText text;RECT cell;
    if(index<0||index>=g_task_count||
       !ListView_GetSubItemRect(g_list,row,DUE_COLUMN,LVIR_BOUNDS,&cell)||cell.right<=cell.left)return;
    if(!TaskDueDisplayForWidth(dc,&g_tasks[index],cell.right-cell.left,&text))return;
    cell.left+=D(7);cell.right-=D(7);cell.bottom--;
    int amount;if(TaskDueBarAmount(&g_tasks[index],&amount)){DrawTaskDueBar(dc,cell,&g_tasks[index],text.kind,amount);return;}
    HGDIOBJ previous=SelectObject(dc,g_small_font);SetTextColor(dc,TaskDueColor(text.kind));
    DrawTextW(dc,text.label,-1,&cell,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(dc,previous);
}
static BOOL TaskDueDateInfoTip(int row,wchar_t *text,size_t capacity)
{
    if(!text||!capacity)return FALSE;text[0]=0;int index=TaskIndexAtRow(row);NativeDueText due;
    if(index<0||index>=g_task_count||!TaskDueDescribe(&g_tasks[index],&due))return FALSE;
    int amount;BOOL bar=TaskDueBarAmount(&g_tasks[index],&amount);
    _snwprintf_s(text,capacity,_TRUNCATE,L"%ls%ls\n%ls",due.tooltip,
        bar?(amount<0?(g_russian?L"\nНет корректной даты создания: заполнение недоступно":L"\nCreation date unavailable: fill cannot be calculated"):
            (g_russian?L"\nВремя от создания задачи до срока":L"\nTime from creation to due date")):L"",
        g_due_bar_mode?(g_russian?L"ПКМ — показать даты":L"Right-click to show dates"):(g_russian?L"ПКМ — показать бары":L"Right-click to show bars"));return TRUE;
}
static void UpdateDueDateDay(void)
{
    SYSTEMTIME today;GetLocalTime(&today);int key=NativeDueDayKey(&today);
    ULONGLONG minute=CurrentFileTime()/600000000ULL;BOOL minute_changed=minute!=g_due_minute;g_due_minute=minute;
    if(key==g_due_day_key){
        if(g_list&&g_due_bar_mode&&minute_changed){RECT column,bounds;
            if(ListView_GetItemCount(g_list)&&ListView_GetSubItemRect(g_list,0,DUE_COLUMN,LVIR_BOUNDS,&column)){
                GetClientRect(g_list,&bounds);column.bottom=bounds.bottom;InvalidateRect(g_list,&column,FALSE);
            }
        }
        return;
    }
    g_due_day_key=key;
    if(!g_list)return;
    for(int row=0;row<ListView_GetItemCount(g_list);row++){
        int index=TaskIndexAtRow(row);NativeDueText due;
        if(index>=0&&index<g_task_count){TaskDueDescribe(&g_tasks[index],&due);ListView_SetItemText(g_list,row,DUE_COLUMN,due.label);}
    }
    if(g_window){RECT bounds;GetClientRect(g_window,&bounds);LayoutControls(bounds.right,bounds.bottom);}
    InvalidateRect(g_list,NULL,FALSE);
}
