/* Main-thread controller. Only results and cancellation cross thread boundaries. */
#define WM_UPDATE_RESULT (WM_APP+41)
#define WM_UPDATE_READY (WM_APP+42)
typedef enum UpdateUiState {UPDATE_UI_IDLE,UPDATE_UI_CHECKING,UPDATE_UI_CURRENT,UPDATE_UI_AVAILABLE,UPDATE_UI_DOWNLOADING,UPDATE_UI_READY,UPDATE_UI_APPLYING,UPDATE_UI_ERROR} UpdateUiState;
typedef struct UpdateWork {
    volatile LONG references;UpdateCancel cancel;UINT generation;int action;
    UpdateStatus status;UpdateVersion current;UpdateRelease release;UpdateStage stage;
    HWND owner;HANDLE thread,ready,helper;
} UpdateWork;
static UpdateUiState u_state;
static UINT u_generation;
static UpdateWork *u_work;
static HWND u_settings;
static UpdateRelease u_release;
static UpdateStage u_stage;
static UpdateStatus u_error;
static BOOL u_no_release,u_lock_initialized;
#define u_handoff g_update_handoff
static volatile LONG u_shutdown;
static HANDLE u_helper;
static wchar_t u_reason[256];
static CRITICAL_SECTION u_post_lock;
static BOOL UpdateSettingsDirty(void);
static BOOL UpdateSettingsAccountBusy(void);
static void UpdateUiRender(HWND settings);
static BOOL UpdateInstallAllowed(wchar_t *reason,size_t capacity);
static BOOL UpdatePopoverDirty(TaskPopover *p){return p&&p->window&&(p->sending||p->note_save_failed||(p->kind==1&&p->input&&GetWindowTextLengthW(p->input)>0));}
static BOOL UpdateInstallAllowed(wchar_t *reason,size_t capacity){
    const wchar_t *message=NULL;
    if(g_jira_busy||UpdateSettingsAccountBusy())message=g_russian?L"Дождитесь завершения запроса Jira.":L"Wait for the Jira request to finish.";
    else if(g_completion_active_count)message=g_russian?L"Закройте окно завершения задачи.":L"Close the task completion dialog.";
    else if(g_editing_start||g_editing_note)message=g_russian?L"Сохраните или отмените текущее редактирование.":L"Save or cancel the current edit.";
    else if(UpdateSettingsDirty())message=g_russian?L"Сохраните или отмените изменения настроек.":L"Save or cancel your settings changes.";
    else{
        BOOL dirty=UpdatePopoverDirty(&g_attached_popover);for(TaskPopover *p=g_detached_popovers;p;p=p->next)dirty|=UpdatePopoverDirty(p);
        if(dirty)message=g_russian?L"Сохраните заметки и отправьте или очистите черновики.":L"Save notes and send or clear comment drafts.";
    }
    if(reason&&capacity)wcsncpy_s(reason,capacity,message?message:L"",_TRUNCATE);return !message;
}
static BOOL UpdateUiBusy(void){return u_state==UPDATE_UI_CHECKING||u_state==UPDATE_UI_DOWNLOADING||u_state==UPDATE_UI_APPLYING;}
static void UpdateWorkRelease(UpdateWork *work){
    if(!work||InterlockedDecrement(&work->references))return;
    if(work->thread)CloseHandle(work->thread);if(work->ready)CloseHandle(work->ready);if(work->helper)CloseHandle(work->helper);
    UpdateStageCleanup(&work->stage);free(work);
}
static void UpdateFreeze(BOOL freeze){
    EnableWindow(g_window,!freeze);if(u_settings&&IsWindow(u_settings))EnableWindow(u_settings,!freeze);
    if(g_attached_popover.window)EnableWindow(g_attached_popover.window,!freeze);
    for(TaskPopover *p=g_detached_popovers;p;p=p->next)if(p->window)EnableWindow(p->window,!freeze);
}
static void UpdateStopHelper(HANDLE helper){
    /* This is exclusively the child handle returned by CreateProcess, never a
       PID lookup. Parent is still alive, so the helper cannot have begun apply. */
    if(helper&&WaitForSingleObject(helper,0)==WAIT_TIMEOUT&&TerminateProcess(helper,UPDATE_CANCELLED))WaitForSingleObject(helper,200);
}
static void UpdateUiCancel(void){
    ++u_generation;
    if(u_work){InterlockedExchange(&u_work->cancel.cancelled,1);UpdateStopHelper(u_work->helper);UpdateWorkRelease(u_work);u_work=NULL;}
    if(!u_handoff){UpdateStageCleanup(&u_stage);u_state=UPDATE_UI_IDLE;u_reason[0]=0;}
    u_settings=NULL;
}
static void UpdateUiAttach(HWND settings){u_settings=settings;UpdateUiRender(settings);}
static void UpdateUiAbortHandoff(void){
    if(!u_handoff)return;UpdateStopHelper(u_helper);if(u_helper)CloseHandle(u_helper);u_helper=NULL;
    u_handoff=FALSE;g_allow_close=FALSE;g_shutdown_reason=L"normal";UpdateFreeze(FALSE);
    if(g_lifecycle.started&&g_lifecycle.update_path[0])DeleteFileW(g_lifecycle.update_path);
    UpdateStageCleanup(&u_stage);u_state=UPDATE_UI_ERROR;u_error=UPDATE_BUSY;UpdateUiRender(u_settings);
}
static DWORD WINAPI UpdateUiWorker(void *arg){
    UpdateWork *work=arg;
    if(work->action==0)work->status=UpdateCheck(work->current,&work->cancel,&work->release);
    else if(work->action==1){
        work->status=UpdateStageCreate(&work->stage);
        if(work->status==UPDATE_OK){
            HANDLE file=CreateFileW(work->stage.executable,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
            if(file==INVALID_HANDLE_VALUE)work->status=UPDATE_IO;
            else{work->status=UpdateDownload(&work->release,file,&work->cancel);if(work->status==UPDATE_OK)work->status=UpdateVerifyFile(file,&work->release);CloseHandle(file);}
        }
    }else{
        HANDLE handles[]={work->ready,work->helper};DWORD wait=WaitForMultipleObjects(2,handles,FALSE,5000);
        work->status=wait==WAIT_OBJECT_0&&WaitForSingleObject(work->helper,0)==WAIT_TIMEOUT?UPDATE_OK:UPDATE_IO;
    }
    if(work->cancel.cancelled)work->status=UPDATE_CANCELLED;
    EnterCriticalSection(&u_post_lock);
    BOOL posted=!u_shutdown&&PostMessageW(work->owner,work->action==2?WM_UPDATE_READY:WM_UPDATE_RESULT,0,(LPARAM)work);
    LeaveCriticalSection(&u_post_lock);
    if(!posted)UpdateWorkRelease(work);return 0;
}
static BOOL UpdateUiStart(UpdateWork *work,HWND settings){
    if(!u_lock_initialized){InitializeCriticalSection(&u_post_lock);u_lock_initialized=TRUE;}
    work->references=2;work->generation=++u_generation;work->owner=g_window;u_settings=settings;u_work=work;
    work->thread=CreateThread(NULL,0,UpdateUiWorker,work,0,NULL);
    if(!work->thread){UpdateStopHelper(work->helper);UpdateWorkRelease(work);UpdateWorkRelease(work);u_work=NULL;u_state=UPDATE_UI_ERROR;u_error=UPDATE_IO;UpdateUiRender(settings);return FALSE;}
    UpdateUiRender(settings);return TRUE;
}
static void UpdateUiBeginCheck(HWND settings){
    if(UpdateUiBusy()||u_work||u_handoff||u_shutdown)return;
    if(UpdateSettingsAccountBusy()){u_state=UPDATE_UI_ERROR;u_error=UPDATE_BUSY;UpdateUiRender(settings);return;}
    UpdateWork *work=calloc(1,sizeof(*work));if(!work)return;
    UpdateStageCleanup(&u_stage);work->action=0;UpdateParseVersion(APP_VERSION,&work->current);u_reason[0]=0;u_state=UPDATE_UI_CHECKING;UpdateUiStart(work,settings);
}
static void UpdateUiBeginDownload(HWND settings){
    if(u_state!=UPDATE_UI_AVAILABLE||u_work||UpdateSettingsAccountBusy()||u_handoff||u_shutdown)return;
    UpdateWork *work=calloc(1,sizeof(*work));if(!work)return;work->action=1;work->release=u_release;u_state=UPDATE_UI_DOWNLOADING;UpdateUiStart(work,settings);
}
static void UpdateUiBeginInstall(HWND settings){
    if(u_state!=UPDATE_UI_READY||u_work||u_handoff)return;
    if(!UpdateInstallAllowed(u_reason,_countof(u_reason))){UpdateUiRender(settings);return;}
    wchar_t text[256];swprintf(text,_countof(text),g_russian?L"Обновить Jira Task Manager %ls до %lu.%lu.%lu и перезапустить?":L"Update Jira Task Manager %ls to %lu.%lu.%lu and restart?",APP_VERSION,u_release.version.major,u_release.version.minor,u_release.version.patch);
    if(MessageBoxW(settings,text,APP_TITLE,MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    if(!UpdateInstallAllowed(u_reason,_countof(u_reason))){UpdateUiRender(settings);return;}
    UpdateApplyRequest request={0};request.parent_pid=GetCurrentProcessId();request.release=u_release;FILETIME c,e,k,t;
    if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&t)){u_state=UPDATE_UI_ERROR;u_error=UPDATE_IO;UpdateUiRender(settings);return;}
    request.parent_created=((ULONGLONG)c.dwHighDateTime<<32)|c.dwLowDateTime;
    wchar_t module[MAX_PATH],canonical[MAX_PATH+8];DWORD length=GetModuleFileNameW(NULL,module,MAX_PATH);
    HANDLE f=length&&length<MAX_PATH?CreateFileW(module,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL):INVALID_HANDLE_VALUE;
    DWORD n=f!=INVALID_HANDLE_VALUE?GetFinalPathNameByHandleW(f,canonical,_countof(canonical),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS):0;if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
    if(!n||n>=_countof(canonical)||wcsncmp(canonical,L"\\\\?\\",4)||wcslen(canonical+4)>=MAX_PATH){u_state=UPDATE_UI_ERROR;u_error=UPDATE_INVALID;UpdateUiRender(settings);return;}
    wcscpy_s(request.target,MAX_PATH,canonical+4);
    UpdateWork *work=calloc(1,sizeof(*work));if(!work)return;work->action=2;work->release=u_release;
    UpdateStatus status=UpdateLaunchHelperTracked(&u_stage,&request,&work->ready,&work->helper);
    if(status!=UPDATE_OK){free(work);u_state=UPDATE_UI_ERROR;u_error=status;UpdateUiRender(settings);return;}
    work->stage=u_stage;memset(&u_stage,0,sizeof(u_stage));u_state=UPDATE_UI_APPLYING;u_reason[0]=0;
    UpdateUiStart(work,settings);
}
static BOOL UpdateUiHandleMessage(UINT message,WPARAM wp,LPARAM lp){
    (void)wp;if(message!=WM_UPDATE_RESULT&&message!=WM_UPDATE_READY)return FALSE;UpdateWork *work=(UpdateWork*)lp;if(!work)return TRUE;
    BOOL current=work->generation==u_generation&&!u_shutdown&&u_settings&&IsWindow(u_settings);
    if(work==u_work){u_work=NULL;UpdateWorkRelease(work);}
    if(current){
        u_error=work->status;u_reason[0]=0;
        if(work->status!=UPDATE_OK&&work->status!=UPDATE_NONE){u_state=UPDATE_UI_ERROR;UpdateStopHelper(work->helper);}
        else if(work->action==0){u_release=work->release;u_no_release=work->status==UPDATE_NONE&&!work->release.asset_id;u_state=work->status==UPDATE_OK?UPDATE_UI_AVAILABLE:UPDATE_UI_CURRENT;}
        else if(work->action==1){UpdateStageCleanup(&u_stage);u_stage=work->stage;memset(&work->stage,0,sizeof(work->stage));u_state=UPDATE_UI_READY;}
        else if(!UpdateInstallAllowed(u_reason,_countof(u_reason))){UpdateStopHelper(work->helper);u_state=UPDATE_UI_ERROR;u_error=UPDATE_BUSY;}
        else{
            u_helper=work->helper;work->helper=NULL;u_stage=work->stage;memset(&work->stage,0,sizeof(work->stage));u_handoff=TRUE;g_allow_close=TRUE;g_shutdown_reason=L"update";UpdateFreeze(TRUE);
            if(!SaveSettings()){wcscpy_s(u_reason,256,g_russian?L"Не удалось сохранить состояние. Обновление отменено.":L"Could not save state. Update cancelled.");UpdateUiAbortHandoff();}
            else{
                if(g_lifecycle.started&&g_lifecycle.update_path[0]){HANDLE marker=CreateFileW(g_lifecycle.update_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_HIDDEN,NULL);if(marker!=INVALID_HANDLE_VALUE){FlushFileBuffers(marker);CloseHandle(marker);}}
                NativeLifecycleEvent(&g_lifecycle,L"update_handoff",L"reason=verified_helper_ready");SendMessageW(g_window,WM_CLOSE,0,0);
                if(IsWindow(g_window))UpdateUiAbortHandoff();
            }
        }
        if(!u_shutdown)UpdateUiRender(u_settings);
    }
    UpdateWorkRelease(work);return TRUE;
}
static void UpdateUiShutdown(void){
    if(u_lock_initialized){EnterCriticalSection(&u_post_lock);InterlockedExchange(&u_shutdown,1);LeaveCriticalSection(&u_post_lock);}else InterlockedExchange(&u_shutdown,1);
    UpdateUiCancel();MSG msg;while(PeekMessageW(&msg,g_window,WM_UPDATE_RESULT,WM_UPDATE_READY,PM_REMOVE))UpdateWorkRelease((UpdateWork*)msg.lParam);
    if(u_helper){CloseHandle(u_helper);u_helper=NULL;} /* committed child waits for normal process exit */
}
static void UpdateUiRender(HWND settings){
    if(!settings||!IsWindow(settings))return;wchar_t status[256],button[128];const wchar_t *label=g_russian?L"Проверить обновления":L"Check for updates";
    switch(u_state){
    case UPDATE_UI_CHECKING:wcscpy_s(status,256,g_russian?L"Проверка обновлений…":L"Checking for updates…");break;
    case UPDATE_UI_CURRENT:wcscpy_s(status,256,u_no_release?(g_russian?L"Пока нет опубликованных версий":L"No published releases yet"):(g_russian?L"Установлена актуальная версия":L"You have the latest version"));break;
    case UPDATE_UI_AVAILABLE:swprintf(status,256,g_russian?L"Доступна версия %lu.%lu.%lu":L"Version %lu.%lu.%lu is available",u_release.version.major,u_release.version.minor,u_release.version.patch);label=g_russian?L"Скачать обновление":L"Download update";break;
    case UPDATE_UI_DOWNLOADING:wcscpy_s(status,256,g_russian?L"Скачивание и проверка файла…":L"Downloading and verifying…");break;
    case UPDATE_UI_READY:wcscpy_s(status,256,u_reason[0]?u_reason:(g_russian?L"Файл проверен. Можно установить обновление.":L"Verified. Ready to install."));label=g_russian?L"Установить и перезапустить":L"Install and restart";break;
    case UPDATE_UI_APPLYING:wcscpy_s(status,256,g_russian?L"Подготовка безопасного перезапуска…":L"Preparing a safe restart…");break;
    case UPDATE_UI_ERROR:wcscpy_s(status,256,u_reason[0]?u_reason:u_error==UPDATE_BUSY?(g_russian?L"Дождитесь завершения текущей операции.":L"Wait for the current operation."):u_error==UPDATE_INVALID?(g_russian?L"Файл или данные версии не прошли проверку.":L"Release or file verification failed."):u_error==UPDATE_NETWORK?(g_russian?L"Не удалось проверить обновления. Повторите позже.":L"Update request failed. Try again later."):(g_russian?L"Не удалось подготовить обновление.":L"Could not prepare the update."));break;
    default:wcscpy_s(status,256,g_russian?L"Обновления через GitHub Releases":L"Updates via GitHub Releases");break;
    }
    wcscpy_s(button,128,label);SetWindowTextW(GetDlgItem(settings,235),status);SetWindowTextW(GetDlgItem(settings,242),button);EnableWindow(GetDlgItem(settings,242),!UpdateUiBusy()&&!UpdateSettingsAccountBusy());
}
