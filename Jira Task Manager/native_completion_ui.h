/* Main-thread modeless editors. Each owns its transition and immutable account/task binding. */
#define WM_COMPLETION_CLOSED (WM_APP+44)
typedef struct CompletionEditor {
    TransitionInputDialog dialog;JiraTransitionItem transition;JiraSnapshot scope;
    wchar_t summary[1024];UINT generation;BOOL sending,closed;struct CompletionEditor *next;
} CompletionEditor;
static CompletionEditor *g_completion_editors;
static UINT g_next_completion_generation;
static CompletionEditor *CompletionEditorFor(TransitionInputDialog *dialog)
{for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(&at->dialog==dialog)return at;return NULL;}
static void CompletionCollectClosed(void)
{
    CompletionEditor **at=&g_completion_editors;
    while(*at){CompletionEditor *editor=*at;if(editor->closed){*at=editor->next;SecureZeroMemory(editor,sizeof(*editor));free(editor);}else at=&editor->next;}
}
static void CompletionRefreshEditor(CompletionEditor *editor)
{
    TransitionInputDialog *dialog=&editor->dialog;HWND window=dialog->window;if(!window)return;
    SetWindowTextW(window,g_russian?L"Jira Task Manager · Завершение задачи":L"Jira Task Manager · Complete task");
    SetWindowTextW(GetDlgItem(window,IDC_TRANSITION_HELPER),g_russian?L"Оценка":L"Rating");
    SetWindowTextW(GetDlgItem(window,IDC_TRANSITION_REMEMBER),g_russian?L"Запомнить комментарий":L"Remember comment");
    SetWindowTextW(GetDlgItem(window,IDCANCEL),g_russian?L"Отмена":L"Cancel");
    SetWindowTextW(GetDlgItem(window,IDOK),editor->sending?(g_russian?L"Отправка…":L"Sending…"):(g_russian?L"Завершить":L"Complete"));
    for(int score=1;score<=5;score++){
        wchar_t text[80];swprintf(text,80,g_russian?L"Оценка: %d из 5":L"Rating: %d of 5",score);SetWindowTextW(GetDlgItem(window,IDC_TRANSITION_RATING+score),text);
        EnableWindow(GetDlgItem(window,IDC_TRANSITION_RATING+score),!editor->sending&&dialog->option_by_score[score]>=0);
    }
    int index=NativeStatusSameScope(&editor->scope,&g_snapshot)?TaskByKey(dialog->key):-1;
    if(index>=0)wcsncpy_s(editor->summary,1024,g_tasks[index].summary,_TRUNCATE);
    if(editor->summary[0])swprintf(dialog->placeholder,_countof(dialog->placeholder),g_russian?L"Комментарий к задаче: %ls":L"Comment on task: %ls",editor->summary);
    else wcscpy_s(dialog->placeholder,_countof(dialog->placeholder),g_russian?L"Комментарий к задаче…":L"Comment on task…");
    int controls[]={IDC_TRANSITION_COMMENT,IDC_TRANSITION_REMEMBER,IDCANCEL,IDC_TRANSITION_CLOSE};
    for(int i=0;i<_countof(controls);i++)EnableWindow(GetDlgItem(window,controls[i]),!editor->sending);
    EnableWindow(GetDlgItem(window,IDOK),!editor->sending&&dialog->selected_score>0);
    SetWindowTheme(GetDlgItem(window,IDC_TRANSITION_COMMENT),g_dark?L"DarkMode_CFD":L"Explorer",NULL);
    SetWindowTheme(GetDlgItem(window,IDC_TRANSITION_REMEMBER),g_dark?L"DarkMode_Explorer":L"Explorer",NULL);
    BOOL dark=g_dark;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void CompletionRefreshEditors(void)
{for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(!at->closed)CompletionRefreshEditor(at);}
static LRESULT CALLBACK CompletionEditorProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR context)
{
    (void)id;CompletionEditor *editor=(CompletionEditor*)context;
    if(editor->sending&&(message==WM_CLOSE||(message==WM_COMMAND&&(LOWORD(wp)==IDCANCEL||LOWORD(wp)==IDC_TRANSITION_CLOSE||LOWORD(wp)==IDOK))))return 0;
    if(message==WM_NCDESTROY){editor->closed=TRUE;PostMessageW(g_window,WM_COMPLETION_CLOSED,0,0);}
    return DefSubclassProc(window,message,wp,lp);
}
static BOOL OpenCompletionEditor(const JiraTransitionItem *transition,const wchar_t *key,BOOL visible)
{
    CompletionCollectClosed();
    for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(!wcscmp(at->dialog.key,key)&&NativeStatusSameScope(&at->scope,&g_snapshot)){
        if(visible){ShowWindow(at->dialog.window,SW_SHOW);SetForegroundWindow(at->dialog.window);}return TRUE;
    }
    CompletionEditor *editor=calloc(1,sizeof(*editor));if(!editor)return FALSE;
    editor->transition=*transition;editor->scope.identity=g_snapshot.identity;wcscpy_s(editor->scope.server,800,g_snapshot.server);wcscpy_s(editor->scope.account,256,g_snapshot.account);
    int index=TaskByKey(key);if(index>=0)wcsncpy_s(editor->summary,1024,g_tasks[index].summary,_TRUNCATE);
    editor->generation=++g_next_completion_generation;if(!editor->generation)editor->generation=++g_next_completion_generation;
    HWND window=CreateTransitionInputWindow(&editor->dialog,&editor->transition,key,FALSE);if(!window){free(editor);return FALSE;}
    editor->next=g_completion_editors;g_completion_editors=editor;SetWindowSubclass(window,CompletionEditorProcedure,944,(DWORD_PTR)editor);
    if(visible){ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(GetDlgItem(window,IDC_TRANSITION_COMMENT));}return TRUE;
}
static UINT CompletionSubmittingGeneration(const wchar_t *key)
{for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(at->sending&&!at->closed&&!wcscmp(at->dialog.key,key)&&NativeStatusSameScope(&at->scope,&g_snapshot))return at->generation;return 0;}
static BOOL SubmitCompletionEditor(TransitionInputDialog *dialog)
{
    CompletionEditor *editor=CompletionEditorFor(dialog);if(!editor)return FALSE;if(editor->sending)return TRUE;
    if(!NativeStatusSameScope(&editor->scope,&g_snapshot)||TaskByKey(dialog->key)<0){
        MessageBoxW(dialog->window,g_russian?L"Подключение или доступность задачи изменились. Черновик сохранён в этом окне. Проверьте задачу перед отправкой.":L"The connection or task availability changed. Your draft is retained in this window. Check the task before submitting.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return TRUE;
    }
    if(g_jira_busy){MessageBoxW(dialog->window,g_russian?L"Дождитесь текущего запроса Jira. Оценка и комментарий сохранены в этом окне.":L"Wait for the current Jira request. The rating and comment are retained in this window.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return TRUE;}
    editor->sending=TRUE;CompletionRefreshEditor(editor);
    if(!StartJiraJobWithInput(2,dialog->key,editor->transition.id,editor->transition.rating_field_id,dialog->option_id,dialog->comment,(POINT){0})){
        editor->sending=FALSE;CompletionRefreshEditor(editor);
    }else{
        CompletionFeedbackBegin(editor->generation,&editor->scope,dialog->key,editor->transition.status_id);
        NativeCompletionTemplateSave(g_settings_path,editor->scope.account,dialog->remember_comment?dialog->comment:L"");
    }
    return TRUE;
}
static void CompletionJobFinished(const JiraJob *job)
{
    if(!job->completion_generation)return;
    if(!job->success||!job->loaded)CompletionFeedbackDiscard(job->completion_generation);
    for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(at->generation==job->completion_generation){
        at->sending=FALSE;
        if(job->success&&job->loaded&&NativeStatusSameScope(&at->scope,&g_snapshot)){at->dialog.accepted=TRUE;DestroyWindow(at->dialog.window);}
        else CompletionRefreshEditor(at);break;
    }
}
static BOOL HandleCompletionDialogMessage(MSG *message)
{
    for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(!at->closed&&(message->hwnd==at->dialog.window||IsChild(at->dialog.window,message->hwnd))){
        if(message->message==WM_KEYDOWN&&message->wParam==VK_TAB){at->dialog.keyboard=TRUE;at->dialog.hover=NULL;InvalidateTransitionRatings(&at->dialog);}
        return IsDialogMessageW(at->dialog.window,message);
    }return FALSE;
}
static void CompletionCloseEditors(void)
{for(CompletionEditor *at=g_completion_editors;at;at=at->next)if(!at->closed){at->sending=FALSE;DestroyWindow(at->dialog.window);}CompletionCollectClosed();}
