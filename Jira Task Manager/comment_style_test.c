#include "native.c"
#include <assert.h>

/* Exercise the real feed painter on a memory surface; no account or Jira calls. */
int main(void)
{
    g_instance=GetModuleHandleW(NULL);
    JiraComment items[2]={0};
    wcscpy_s(g_snapshot.identity.key,256,L"user-1");
    for(int i=0;i<2;i++){
        wcscpy_s(items[i].author,256,L"Same display name");
        wcscpy_s(items[i].created,64,L"2026-10-07T10:00:00");
        wcscpy_s(items[i].body,2048,L"A readable comment that wraps consistently across themes and DPI.");
        wcscpy_s(items[i].identity.key,256,i?L"user-2":L"user-1");
    }
    g_popover.comments=(JiraComments){items,2};
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    for(int dpi=96;dpi<=192;dpi+=48)for(int dark=0;dark<2;dark++)for(int ru=0;ru<2;ru++){
        g_dpi=dpi;g_dark=dark;g_russian=ru;RecreateBrushes();
        g_font=CreateFontW(-D(13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        g_bold_font=CreateFontW(-D(13),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        g_small_font=CreateFontW(-D(11),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        for(int width=280;width<=430;width+=150){
            RECT client={0,0,D(width),D(300)};
            HBITMAP bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom);
            HGDIOBJ old=SelectObject(dc,bitmap);
            int own_height=CommentCardHeight(dc,&items[0],client.right);
            assert(own_height==CommentCardHeight(dc,&items[1],client.right));
            PaintCommentList(NULL,dc,client);GdiFlush();
            COLORREF mine=GetPixel(dc,D(7),D(20));
            COLORREF colleague=GetPixel(dc,D(7),D(20)+own_height);
            assert(mine!=colleague); /* Removing the own-message branch must fail. */
            assert(colleague==PopoverColor());
            /* Body ink must remain present in both cards, with no width lost to alignment. */
            int own_ink=0,other_ink=0;
            for(int y=D(41);y<D(55);y++)for(int x=D(49);x<D(230);x++){
                if(GetPixel(dc,x,y)==TextColor())own_ink++;
                if(GetPixel(dc,x,y+own_height)==TextColor())other_ink++;
            }
            assert(own_ink>0&&other_ink>0);
            if(width==280){
                /* The long author name is ellipsized: badge occupies 129..153 DIP. */
                COLORREF badge_color=dark?RGB(170,211,255):RGB(31,104,183);
                int badge_ink=0;
                for(int y=D(14);y<D(36);y++)for(int x=D(129);x<D(153);x++)
                    if(GetPixel(dc,x,y)==badge_color)badge_ink++;
                assert(badge_ink>0);
            }
            SelectObject(dc,old);DeleteObject(bitmap);
        }
        DeleteObject(g_font);DeleteObject(g_bold_font);DeleteObject(g_small_font);
        g_font=g_bold_font=g_small_font=NULL;
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
    DeleteObject(g_window_brush);DeleteObject(g_panel_brush);DeleteObject(g_popover_brush);
    puts("Comment author styling: own/colleague, both themes, RU/EN and 96/144/192 DPI passed");
}
