#include "native.c"
#include <assert.h>

/* Optional native-painter snapshots for visual QA; never used by the app. */
static void SaveCommentSnapshot(HDC dc,HBITMAP bitmap,const char *path,int width,int height)
{
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    DWORD size=(DWORD)width*height*4;BYTE *pixels=malloc(size);assert(pixels);
    assert(GetDIBits(dc,bitmap,0,height,pixels,&info,DIB_RGB_COLORS)==height);
    BITMAPFILEHEADER file={0};file.bfType=0x4d42;
    file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+size;
    FILE *output=NULL;assert(!fopen_s(&output,path,"wb"));
    assert(fwrite(&file,sizeof(file),1,output)==1);
    assert(fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,output)==1);
    assert(fwrite(pixels,size,1,output)==1);assert(!fclose(output));free(pixels);
}

/* Exercise the real feed painter on a memory surface; no account or Jira calls. */
int main(int argc,char **argv)
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
            /* Authorship must not tint a read message like unread activity. */
            assert(mine==PopoverColor());
            assert(colleague==PopoverColor());
            COLORREF neutral_avatar=dark?RGB(48,53,60):RGB(232,236,242);
            assert(GetPixel(dc,D(15),D(28))==neutral_avatar);
            assert(GetPixel(dc,D(15),D(28)+own_height)==neutral_avatar);
            /* Body ink must remain present in both cards, with no width lost to alignment. */
            int own_ink=0,other_ink=0;
            for(int y=D(41);y<D(55);y++)for(int x=D(49);x<D(230);x++){
                if(GetPixel(dc,x,y)==TextColor())own_ink++;
                if(GetPixel(dc,x,y+own_height)==TextColor())other_ink++;
            }
            assert(own_ink>0&&other_ink>0);
            if(width==280){
                /* The long author name is ellipsized: badge occupies 129..153 DIP. */
                int badge_ink=0,other_badge_ink=0;
                for(int y=D(14);y<D(36);y++)for(int x=D(129);x<D(153);x++)
                {
                    if(GetPixel(dc,x,y)==MutedColor())badge_ink++;
                    if(GetPixel(dc,x,y+own_height)==MutedColor())other_badge_ink++;
                }
                assert(badge_ink>0);
                assert(other_badge_ink==0); /* Identical display name is not ownership. */
            }
            SelectObject(dc,old);
            if(argc>1&&width==430&&dpi==96&&ru){
                char path[MAX_PATH];snprintf(path,sizeof(path),"%s-%s.bmp",argv[1],dark?"dark":"light");
                SaveCommentSnapshot(dc,bitmap,path,client.right,client.bottom);
            }
            DeleteObject(bitmap);
        }
        DeleteObject(g_font);DeleteObject(g_bold_font);DeleteObject(g_small_font);
        g_font=g_bold_font=g_small_font=NULL;
    }
    DeleteDC(dc);ReleaseDC(NULL,screen);
    DeleteObject(g_window_brush);DeleteObject(g_panel_brush);DeleteObject(g_popover_brush);
    puts("Comment author styling: own/colleague, both themes, RU/EN and 96/144/192 DPI passed");
}
