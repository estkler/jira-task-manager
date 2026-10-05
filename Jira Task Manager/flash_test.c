#include "native.c"
#include <assert.h>
int main(void) {
    assert(ShouldFlashRowCell(0,FALSE));
    assert(ShouldFlashRowCell(1,FALSE));
    assert(ShouldFlashRowCell(2,FALSE));
    assert(ShouldFlashRowCell(STATUS_COLUMN,FALSE));
    assert(!ShouldFlashRowCell(STATUS_COLUMN,TRUE));
    g_comments_mode=2;assert(!ShouldFlashRowCell(COMMENTS_COLUMN,FALSE));g_comments_mode=0;
    g_flash_task=0;g_flash_started=1000;g_flash_released=1000;g_flash_release_opacity=0;
    assert(FlashOpacity(1045)>0.2);
    assert(FlashOpacity(1090)>0.33);
    assert(FlashOpacity(1270)>0 && FlashOpacity(1270)<0.1);
    assert(FlashOpacity(1450)==0);
    g_flash_started=2000;g_flash_released=0;
    assert(FlashOpacity(2045)>0.2);
    assert(FlashOpacity(7400)==0);
    puts("Flash tests passed");
    return 0;
}
