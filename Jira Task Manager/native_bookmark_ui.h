/* Included after native window helpers: bookmark view never mutates Jira tasks. */
static BOOL bookmark_dragging;
static BOOL bookmark_drag_value;
static int bookmark_last_row;
static POINT bookmark_mouse;
static int bookmark_presented_editing=-1,bookmark_presented_showing=-1,bookmark_presented_dirty=-1,bookmark_presented_language=-1;
static BOOL BookmarkNoticeVisible(void){return g_bookmark_notice_until&&GetTickCount64()<g_bookmark_notice_until;}
static COLORREF BookmarkHeaderColor(void){return g_dark?RGB(32,48,66):RGB(235,244,255);}
static void BookmarkSetText(HWND window,const wchar_t *text){
    if(!window)return;wchar_t current[512]={0};GetWindowTextW(window,current,_countof(current));
    if(wcscmp(current,text))SetWindowTextW(window,text);
}
static void BookmarkError(void){
    MessageBoxW(g_window,g_russian?L"Не удалось прочитать или сохранить закладки. Выбор не потерян. Проверьте доступ к папке настроек.":
        L"Unable to read or save bookmarks. Your selection is unchanged. Check access to the settings folder.",APP_TITLE,MB_OK|MB_ICONERROR);
}
static void BookmarkStopDrag(void){
    bookmark_dragging=FALSE;if(g_list){KillTimer(g_list,780);if(GetCapture()==g_list)ReleaseCapture();}
}
static void BookmarkSyncIdentity(void){
    const wchar_t *account=g_snapshot.identity.account_id[0]?g_snapshot.identity.account_id:
        g_snapshot.identity.key[0]?g_snapshot.identity.key:g_snapshot.identity.name;
    const wchar_t *server=account[0]?g_snapshot.server:L"";
    if(!wcscmp(server,g_bookmark_server)&&!wcscmp(account,g_bookmark_account))return;
    ++g_bookmark_identity_generation;
    BookmarkStopDrag();NativeBookmarksFree(&g_bookmarks);g_bookmark_notice_until=0;
    wcscpy_s(g_bookmark_server,_countof(g_bookmark_server),server);
    wcscpy_s(g_bookmark_account,_countof(g_bookmark_account),account);
    g_bookmark_ready=FALSE;g_bookmark_loading_failed=FALSE;
    if(server[0]&&g_bookmark_account[0]){
        g_bookmark_ready=NativeBookmarksLoad(&g_bookmarks,g_settings_path,server,g_bookmark_account);
        g_bookmark_loading_failed=!g_bookmark_ready;
    }
}
static void BookmarkGlyph(HDC dc,int cx,int cy,BOOL selected,COLORREF color){
    HPEN pen=CreatePen(PS_SOLID,max(1,D(1)),color);HBRUSH fill=CreateSolidBrush(color);
    HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,selected?fill:GetStockObject(HOLLOW_BRUSH));
    POINT points[]={{cx-D(4),cy-D(7)},{cx+D(4),cy-D(7)},{cx+D(4),cy+D(7)},{cx,cy+D(4)},{cx-D(4),cy+D(7)}};
    Polygon(dc,points,5);SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(fill);DeleteObject(pen);
}
static void LayoutBookmarkControls(void){
    if(!g_bookmark_button)return;
    RECT edit;GetWindowRect(g_search,&edit);MapWindowPoints(NULL,g_window,(POINT*)&edit,2);
    ShowWindow(g_search_count,SW_HIDE);
    /* The icon's own HWND never depends on caption, dirty state or selection count. */
    int icon_left=g_search_frame.right-D(50),caption_left=icon_left;
    BOOL caption=g_bookmarks.editing||BookmarkNoticeVisible();
    if(caption){
        HDC dc=GetDC(g_window);HGDIOBJ font=SelectObject(dc,g_small_font);SIZE size;int text_width=0;
        const wchar_t *labels[]={g_bookmarks.editing?L"Готово":L"Закладки удалены",g_bookmarks.editing?L"Done":L"Bookmarks cleared",L"Выбор",L"Select"};
        for(int i=0;i<(g_bookmarks.editing?4:2);i++){GetTextExtentPoint32W(dc,labels[i],(int)wcslen(labels[i]),&size);text_width=max(text_width,size.cx);}
        SelectObject(dc,font);ReleaseDC(g_window,dc);
        int group_gap=g_bookmarks.editing?0:D(6);
        int available=max(0,icon_left-group_gap-edit.left-D(120)-D(8)),caption_width=text_width+D(8);
        if(available<caption_width)caption=FALSE;
        else {caption_left=icon_left-group_gap-caption_width;SetWindowPos(g_bookmark_caption,HWND_TOP,caption_left,g_search_frame.top+D(2),caption_width,D(24),SWP_NOACTIVATE);}
    }
    ShowWindow(g_bookmark_caption,caption?SW_SHOWNA:SW_HIDE);
    SetWindowPos(g_search,NULL,edit.left,edit.top,max(1,caption_left-D(8)-edit.left),edit.bottom-edit.top,SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(g_bookmark_button,HWND_TOP,icon_left,g_search_frame.top+D(2),D(24),D(24),SWP_NOACTIVATE);
}
static void BookmarkFooterOverride(void){
    if(!g_updated)return;wchar_t label[256];int count=NativeBookmarksCount(&g_bookmarks);
    if(BookmarkNoticeVisible())wcscpy_s(label,256,g_russian?L"Закладки удалены · показаны все задачи":L"Bookmarks cleared · showing all tasks");
    else if(g_bookmarks.editing)swprintf(label,256,NativeBookmarksDirty(&g_bookmarks)?
        (g_russian?L"Выбрано: %d · \u2713 сохранить · Esc — отменить":L"Selected: %d · \u2713 save · Esc — cancel"):
        (g_russian?L"Выбрано: %d · Enter — завершить · Esc — отменить":L"Selected: %d · Enter — finish · Esc — cancel"),count);
    else if(g_bookmarks.showing)swprintf(label,256,g_russian?L"Закладки: %d · показано %d":L"Bookmarks: %d · shown %d",count,ListView_GetItemCount(g_list));
    else return;
    BookmarkSetText(g_updated,label);
}
static void BookmarkPresent(void){
    if(!g_bookmark_button)return;
    BOOL dirty=NativeBookmarksDirty(&g_bookmarks);
    BOOL mode_changed=bookmark_presented_editing!=g_bookmarks.editing||bookmark_presented_showing!=g_bookmarks.showing||bookmark_presented_language!=g_russian;
    BOOL edit_changed=bookmark_presented_editing!=g_bookmarks.editing;
    BOOL icon_changed=mode_changed||bookmark_presented_dirty!=dirty;
    bookmark_presented_editing=g_bookmarks.editing;bookmark_presented_showing=g_bookmarks.showing;
    bookmark_presented_dirty=dirty;bookmark_presented_language=g_russian;
    const wchar_t *caption=g_bookmarks.editing?(dirty?(g_russian?L"Готово":L"Done"):(g_russian?L"Выбор":L"Select")):
        BookmarkNoticeVisible()?(g_russian?L"Закладки удалены":L"Bookmarks cleared"):L"";
    BookmarkSetText(g_bookmark_caption,caption);
    BookmarkSetText(g_bookmark_button,g_bookmarks.editing?caption:g_bookmarks.showing?
        (g_russian?L"Закладки включены — показать все задачи":L"Bookmarks on — show all tasks"):(g_russian?L"Закладки":L"Bookmarks"));
    EnableWindow(g_bookmark_button,g_bookmark_ready||g_bookmark_loading_failed);
    if(mode_changed){
        SendMessageW(g_search,EM_SETCUEBANNER,TRUE,(LPARAM)(g_russian?L"Поиск":L"Search"));
        TOOLINFOW tip={0};tip.cbSize=sizeof(tip)-sizeof(void*);tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tip.hwnd=g_window;tip.uId=(UINT_PTR)g_bookmark_button;
        tip.lpszText=(wchar_t*)(g_bookmarks.editing?(g_russian?L"Enter или нажатие — сохранить. Esc — отменить.":L"Enter or click to save. Esc to cancel."):
            (g_russian?L"Нажатие — открыть/скрыть закладки. ПКМ — изменить или удалить.":L"Click to show/hide bookmarks. Right-click to edit or clear."));
        SendMessageW(g_tooltips,TTM_UPDATETIPTEXTW,0,(LPARAM)&tip);
        if(!g_bookmarks.editing&&!g_bookmarks.showing)UpdateScopeLabels();
    }
    if(g_bookmarks.editing||g_bookmarks.showing){
        wchar_t title[256];swprintf(title,256,L"%ls — %ls%ls",APP_TITLE,g_bookmarks.editing?(g_russian?L"Выбор закладок":L"Select bookmarks"):
            (g_russian?L"Закладки":L"Bookmarks"),dirty?(g_russian?L" · есть изменения":L" · unsaved changes"):L"");BookmarkSetText(g_window,title);
    }
    if(edit_changed){
        LVCOLUMNW column={.mask=LVCF_TEXT,.pszText=g_bookmarks.editing?L"":L"#"};ListView_SetColumn(g_list,0,&column);
        InvalidateRect(g_window,&g_search_frame,FALSE);InvalidateRect(ListView_GetHeader(g_list),NULL,FALSE);InvalidateRect(g_filter,NULL,FALSE);
    }
    BookmarkFooterOverride();if(icon_changed)InvalidateRect(g_bookmark_button,NULL,FALSE);
}
static void BookmarkRefresh(void){
    BOOL preserve=g_preserve_popover_refresh;g_preserve_popover_refresh=TRUE;RefreshTaskList();g_preserve_popover_refresh=preserve;
}
static void BookmarkShowCleared(void){g_bookmark_notice_until=GetTickCount64()+4000;SetTimer(g_window,785,4000,NULL);}
static void BookmarkBegin(void){
    if(!g_bookmark_ready){BookmarkError();return;}if(!CommitNoteEdit(TRUE))return;
    CloseTaskPopover();if(g_popover.window)return;
    if(!NativeBookmarksBegin(&g_bookmarks)){BookmarkError();return;}
    g_bookmark_notice_until=0;KillTimer(g_window,785);BookmarkRefresh();SetFocus(g_search);
}
static void BookmarkSave(void){
    BookmarkStopDrag();BOOL cleared=g_bookmarks.saved.count>0&&!g_bookmarks.draft.count;
    if(!NativeBookmarksCommit(&g_bookmarks,g_settings_path,g_bookmark_server,g_bookmark_account)){BookmarkError();return;}
    if(cleared)BookmarkShowCleared();BookmarkRefresh();
}
static void BookmarkCancel(void){BookmarkStopDrag();NativeBookmarksCancel(&g_bookmarks);BookmarkRefresh();UpdateLastUpdated();}
static void BookmarkActivate(void){
    if(g_bookmark_loading_failed){
        g_bookmark_ready=NativeBookmarksLoad(&g_bookmarks,g_settings_path,g_bookmark_server,g_bookmark_account);
        g_bookmark_loading_failed=!g_bookmark_ready;
    }
    if(!g_bookmark_ready){BookmarkError();return;}
    if(g_bookmarks.editing)BookmarkSave();
    else if(!g_bookmarks.saved.count)BookmarkBegin();
    else {g_bookmarks.showing=!g_bookmarks.showing;BookmarkRefresh();UpdateLastUpdated();}
}
static void BookmarkClear(void){
    BookmarkStopDrag();if(!g_bookmark_ready||!NativeBookmarksClear(&g_bookmarks,g_settings_path,g_bookmark_server,g_bookmark_account)){BookmarkError();return;}
    BookmarkShowCleared();BookmarkRefresh();
}
static void BookmarkToggleTask(const wchar_t *key){
    if(!g_bookmark_ready)return;
    BOOL editing=g_bookmarks.editing,showing=g_bookmarks.showing,selected=NativeBookmarksContains(&g_bookmarks,key);
    if(!editing&&!NativeBookmarksBegin(&g_bookmarks)){BookmarkError();return;}
    if(!NativeBookmarksSet(&g_bookmarks,key,!selected)){
        if(!editing)NativeBookmarksCancel(&g_bookmarks);BookmarkError();return;
    }
    if(!editing){
        BOOL cleared=g_bookmarks.saved.count>0&&!g_bookmarks.draft.count;
        if(!NativeBookmarksCommit(&g_bookmarks,g_settings_path,g_bookmark_server,g_bookmark_account)){BookmarkRefresh();BookmarkError();return;}
        g_bookmarks.showing=showing&&g_bookmarks.saved.count>0;if(cleared)BookmarkShowCleared();BookmarkRefresh();
    }else {BookmarkPresent();InvalidateRect(g_list,NULL,FALSE);}
}
static void BookmarkMenu(void){
    ULONGLONG identity=g_bookmark_identity_generation;
    HMENU menu=CreateNativeMenu();AddNativeMenuItem(menu,MF_STRING|(g_bookmark_ready?0:MF_GRAYED),IDM_BOOKMARK_EDIT,g_bookmarks.editing?
        (g_russian?L"Отменить изменения":L"Cancel changes"):(g_russian?L"Изменить закладки":L"Edit bookmarks"));
    AddNativeMenuItem(menu,MF_STRING|(g_bookmark_ready&&(g_bookmarks.saved.count||NativeBookmarksCount(&g_bookmarks))?0:MF_GRAYED),IDM_BOOKMARK_CLEAR,
        g_russian?L"Удалить все закладки":L"Clear all bookmarks");
    MarkNativeMenuPlain(menu);RECT rect;GetWindowRect(g_bookmark_button,&rect);
    int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,rect.left,rect.bottom,0,g_window,NULL);DestroyNativeMenu(menu);
    if(identity!=g_bookmark_identity_generation)return;
    if(command==IDM_BOOKMARK_EDIT){if(g_bookmarks.editing)BookmarkCancel();else BookmarkBegin();}
    if(command==IDM_BOOKMARK_CLEAR)BookmarkClear();
}
static void BookmarkPaintRange(int row){
    int count=ListView_GetItemCount(g_list);if(row<0||row>=count)return;BOOL changed=FALSE;
    for(int i=min(bookmark_last_row,row);i<=max(bookmark_last_row,row);i++){
        int index=TaskIndexAtRow(i);if(index<0||index>=g_task_count)continue;const wchar_t *key=g_tasks[index].key;
        if(NativeBookmarksContains(&g_bookmarks,key)!=bookmark_drag_value){
            if(!NativeBookmarksSet(&g_bookmarks,key,bookmark_drag_value)){BookmarkStopDrag();BookmarkError();return;}changed=TRUE;
        }
    }
    bookmark_last_row=row;if(changed){BookmarkPresent();InvalidateRect(g_list,NULL,FALSE);}
}
static int BookmarkHit(HWND window,POINT point){LVHITTESTINFO hit={.pt=point};return ListView_SubItemHitTest(window,&hit);}
static BOOL BookmarkListMessage(HWND window,UINT message,WPARAM wp,LPARAM lp,LRESULT *result){
    *result=0;if(!g_bookmarks.editing)return FALSE;
    if(message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK){
        POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};int row=BookmarkHit(window,point),index=TaskIndexAtRow(row);if(index<0)return TRUE;
        SetFocus(window);ListView_SetItemState(window,row,LVIS_FOCUSED,LVIS_FOCUSED);g_list_keyboard_focus=FALSE;
        bookmark_dragging=TRUE;bookmark_drag_value=!NativeBookmarksContains(&g_bookmarks,g_tasks[index].key);bookmark_last_row=row;bookmark_mouse=point;SetCapture(window);BookmarkPaintRange(row);return TRUE;
    }
    if(message==WM_MOUSEMOVE&&bookmark_dragging){
        bookmark_mouse=(POINT){(short)LOWORD(lp),(short)HIWORD(lp)};BookmarkPaintRange(BookmarkHit(window,bookmark_mouse));
        RECT client,header;GetClientRect(window,&client);GetWindowRect(ListView_GetHeader(window),&header);MapWindowPoints(NULL,window,(POINT*)&header,2);
        if(bookmark_mouse.y<header.bottom+D(6)||bookmark_mouse.y>client.bottom-D(8))SetTimer(window,780,80,NULL);else KillTimer(window,780);return TRUE;
    }
    if(message==WM_TIMER&&wp==780){
        if(!bookmark_dragging)return TRUE;RECT client,header;GetClientRect(window,&client);GetWindowRect(ListView_GetHeader(window),&header);MapWindowPoints(NULL,window,(POINT*)&header,2);
        ListView_Scroll(window,0,bookmark_mouse.y<header.bottom+D(6)?-D(28):D(28));POINT point=bookmark_mouse;point.y=max(header.bottom+2,min(client.bottom-2,point.y));BookmarkPaintRange(BookmarkHit(window,point));return TRUE;
    }
    if(message==WM_LBUTTONUP){BookmarkStopDrag();return TRUE;}
    if(message==WM_CAPTURECHANGED){bookmark_dragging=FALSE;KillTimer(window,780);return TRUE;}
    if(message==WM_KEYDOWN&&wp==VK_SPACE){int row=ListView_GetNextItem(window,-1,LVNI_FOCUSED),index=TaskIndexAtRow(row);if(index>=0)BookmarkToggleTask(g_tasks[index].key);return TRUE;}
    if(message==WM_SETCURSOR){SetCursor(LoadCursorW(NULL,IDC_ARROW));*result=TRUE;return TRUE;}
    return FALSE;
}
static BOOL HandleBookmarkKeyMessage(MSG *message){
    if(!g_bookmarks.editing||message->message!=WM_KEYDOWN||(message->hwnd!=g_window&&!IsChild(g_window,message->hwnd)))return FALSE;
    if(message->hwnd==g_note_edit||message->hwnd==g_start_edit||
        (g_attached_popover.window&&(message->hwnd==g_attached_popover.window||IsChild(g_attached_popover.window,message->hwnd))))return FALSE;
    if(message->wParam==VK_RETURN){BookmarkSave();return TRUE;}
    if(message->wParam==VK_ESCAPE){BookmarkCancel();return TRUE;}return FALSE;
}
static LRESULT CALLBACK BookmarkButtonProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    (void)id;(void)data;
    if(message==WM_CONTEXTMENU){BookmarkMenu();return 0;}
    if(message==WM_ERASEBKGND)return 1;
    return DefSubclassProc(window,message,wp,lp);
}
static void CreateBookmarkControls(void){
    g_bookmark_button=CreateOwnerButton(g_window,IDC_BOOKMARKS,g_russian?L"Закладки":L"Bookmarks");
    g_bookmark_caption=CreateOwnerButton(g_window,IDC_BOOKMARK_CAPTION,L"");ShowWindow(g_bookmark_caption,SW_HIDE);
    HWND siblings[]={g_search,g_clear_search,g_bookmark_button,g_bookmark_caption};
    for(int i=0;i<4;i++)SetWindowLongPtrW(siblings[i],GWL_STYLE,GetWindowLongPtrW(siblings[i],GWL_STYLE)|WS_CLIPSIBLINGS);
    SetWindowSubclass(g_bookmark_button,BookmarkButtonProcedure,80,0);SetWindowSubclass(g_bookmark_caption,BookmarkButtonProcedure,80,0);
    TOOLINFOW tip={0};tip.cbSize=sizeof(tip)-sizeof(void*);tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tip.hwnd=g_window;tip.uId=(UINT_PTR)g_bookmark_button;tip.lpszText=L"Bookmarks";
    SendMessageW(g_tooltips,TTM_ADDTOOLW,0,(LPARAM)&tip);BookmarkPresent();RECT client;GetClientRect(g_window,&client);LayoutControls(client.right,client.bottom);
}
static void BookmarkDrawButtonContent(const DRAWITEMSTRUCT *draw){
    RECT rect=draw->rcItem;
    HBRUSH background=CreateSolidBrush(PanelColor());FillRect(draw->hDC,&rect,background);DeleteObject(background);
    int cx=(rect.left+rect.right)/2,cy=(rect.top+rect.bottom)/2;
    COLORREF accent=IsWindowEnabled(draw->hwndItem)?AccentColor():MutedColor();
    if(draw->CtlID==IDC_BOOKMARK_CAPTION){
        RECT text=rect;text.left+=D(4);text.right-=D(4);wchar_t label[128];GetWindowTextW(draw->hwndItem,label,_countof(label));
        SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,BookmarkNoticeVisible()&&!g_bookmarks.editing?(g_dark?RGB(121,205,149):RGB(23,102,59)):accent);
        HGDIOBJ font=SelectObject(draw->hDC,g_small_font);DrawTextW(draw->hDC,label,-1,&text,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);SelectObject(draw->hDC,font);
    }else if(g_bookmarks.editing){
        BOOL dirty=NativeBookmarksDirty(&g_bookmarks);HPEN pen=CreatePen(PS_SOLID,max(1,D(dirty?2:1)),dirty?accent:MutedColor());HGDIOBJ old=SelectObject(draw->hDC,pen);
        if(dirty){MoveToEx(draw->hDC,cx-D(6),cy,NULL);LineTo(draw->hDC,cx-D(2),cy+D(4));LineTo(draw->hDC,cx+D(6),cy-D(5));}
        else {HGDIOBJ brush=SelectObject(draw->hDC,GetStockObject(HOLLOW_BRUSH));POINT pencil[]={{cx-D(6),cy+D(3)},{cx+D(3),cy-D(6)},{cx+D(6),cy-D(3)},{cx-D(3),cy+D(6)},{cx-D(7),cy+D(7)}};Polygon(draw->hDC,pencil,5);MoveToEx(draw->hDC,cx+D(1),cy-D(4),NULL);LineTo(draw->hDC,cx+D(4),cy-D(1));SelectObject(draw->hDC,brush);}
        SelectObject(draw->hDC,old);DeleteObject(pen);
    }else BookmarkGlyph(draw->hDC,cx,cy,g_bookmarks.showing,g_bookmarks.editing||g_bookmarks.showing?accent:MutedColor());
    if((draw->itemState&ODS_FOCUS)&&g_keyboard_button==draw->hwndItem){
        HPEN pen=CreatePen(PS_SOLID,1,AccentColor());HGDIOBJ old=SelectObject(draw->hDC,pen),brush=SelectObject(draw->hDC,GetStockObject(HOLLOW_BRUSH));
        Rectangle(draw->hDC,rect.left+1,rect.top+1,rect.right-1,rect.bottom-1);SelectObject(draw->hDC,brush);SelectObject(draw->hDC,old);DeleteObject(pen);
    }
}
static void BookmarkDrawButton(const DRAWITEMSTRUCT *draw){
    int width=draw->rcItem.right-draw->rcItem.left,height=draw->rcItem.bottom-draw->rcItem.top;if(width<=0||height<=0)return;
    HDC memory=CreateCompatibleDC(draw->hDC);HBITMAP bitmap=memory?CreateCompatibleBitmap(draw->hDC,width,height):NULL;
    if(!memory||!bitmap){if(memory)DeleteDC(memory);BookmarkDrawButtonContent(draw);return;}
    HGDIOBJ previous=SelectObject(memory,bitmap);DRAWITEMSTRUCT buffered=*draw;buffered.hDC=memory;buffered.rcItem=(RECT){0,0,width,height};
    BookmarkDrawButtonContent(&buffered);BitBlt(draw->hDC,draw->rcItem.left,draw->rcItem.top,width,height,memory,0,0,SRCCOPY);SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);
}
