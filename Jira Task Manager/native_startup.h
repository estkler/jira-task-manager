#ifndef TASKLET_STARTUP_H
#define TASKLET_STARTUP_H
typedef enum { INITIAL_NONE,INITIAL_WAIT,INITIAL_LOADING,INITIAL_FAILED,INITIAL_SIGNED_OUT,INITIAL_READY } InitialView;
static InitialView g_initial_view;
static HWND g_initial_retry;
static void UpdateTabLabels(void);
static void UpdateLastUpdated(void);
static BOOL InitialCountsUnknown(void){return g_initial_view!=INITIAL_NONE&&g_initial_view!=INITIAL_READY;}
static BOOL InitialLoading(void){return g_initial_view==INITIAL_WAIT||g_initial_view==INITIAL_LOADING;}
static void LayoutInitialView(void)
{
    if(!g_list)return;
    RECT body,header;GetClientRect(g_list,&body);GetWindowRect(ListView_GetHeader(g_list),&header);
    int middle=(body.bottom+header.bottom-header.top)/2;
    if(g_initial_retry){
        SetWindowTextW(g_initial_retry,g_russian?L"Повторить":L"Retry");
        SetWindowPos(g_initial_retry,NULL,max(D(8),(body.right-D(120))/2),middle+D(54),min(D(120),max(0,body.right-D(16))),D(30),SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(g_initial_retry,g_initial_view==INITIAL_FAILED?SW_SHOWNA:SW_HIDE);
    }
    if(InitialLoading()&&IsWindowVisible(g_window)&&!IsIconic(g_window))SetTimer(g_list,9,80,NULL);
    else KillTimer(g_list,9);
}
static void SetInitialView(InitialView state)
{
    g_initial_view=state;UpdateTabLabels();LayoutInitialView();UpdateLastUpdated();
    if(g_list)InvalidateRect(g_list,NULL,FALSE);
}
static void DrawInitialView(HDC dc,RECT body)
{
    if(!InitialCountsUnknown())return;
    int saved=SaveDC(dc);SetBkMode(dc,TRANSPARENT);SelectObject(dc,g_font);SetTextColor(dc,TextColor());
    int middle=(body.top+body.bottom)/2;
    RECT title={body.left+D(16),middle-D(16),body.right-D(16),middle+D(14)};
    const wchar_t *text=InitialLoading()?(g_russian?L"Загружаем задачи из Jira…":L"Loading tasks from Jira…"):
        g_initial_view==INITIAL_FAILED?(g_russian?L"Не удалось загрузить задачи":L"Couldn't load tasks"):
        (g_russian?L"Подключитесь к Jira в настройках":L"Connect to Jira in settings");
    DrawTextW(dc,text,-1,&title,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(InitialLoading()){
        int cx=(body.left+body.right)/2,cy=middle-D(35);double phase=(GetTickCount64()%1200)*6.283185307179586/1200;
        for(int i=0;i<10;i++){
            double a=phase+i*6.283185307179586/10;int x=cx+(int)(cos(a)*D(11)),y=cy+(int)(sin(a)*D(11));
            COLORREF accent=AccentColor(),base=PanelColor();int strength=40+i*20;
            COLORREF color=RGB((GetRValue(accent)*strength+GetRValue(base)*(255-strength))/255,(GetGValue(accent)*strength+GetGValue(base)*(255-strength))/255,(GetBValue(accent)*strength+GetBValue(base)*(255-strength))/255);
            HBRUSH brush=CreateSolidBrush(color);HGDIOBJ old=SelectObject(dc,brush),pen=SelectObject(dc,GetStockObject(NULL_PEN));
            Ellipse(dc,x-D(2),y-D(2),x+D(2),y+D(2));SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(brush);
        }
    }else if(g_initial_view==INITIAL_FAILED){
        SelectObject(dc,g_small_font);SetTextColor(dc,MutedColor());
        RECT detail={body.left+D(20),middle+D(16),body.right-D(20),middle+D(50)};
        DrawTextW(dc,g_jira_message,-1,&detail,DT_CENTER|DT_WORDBREAK|DT_END_ELLIPSIS);
    }
    RestoreDC(dc,saved);
}
#endif
