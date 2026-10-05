#include <assert.h>
#include <stdio.h>
#include "workday_clock.h"

static SYSTEMTIME Day(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second) {
    SYSTEMTIME value = {0};
    value.wYear=year; value.wMonth=month; value.wDay=day;
    value.wHour=hour; value.wMinute=minute; value.wSecond=second;
    return value;
}

int main(void) {
    SYSTEMTIME now=Day(2026,9,22,9,0,0), today=Day(2026,9,22,8,45,0), yesterday=Day(2026,9,21,9,0,0);
    assert(WorkdaySameLocalDate(&now,&today));
    assert(!WorkdaySameLocalDate(&now,&yesterday));
    assert(WorkdayNormalizeStart(&now,&today,1));
    assert(!WorkdayNormalizeStart(&now,&yesterday,1));
    assert(WorkdayElapsedSeconds(3600,9000)==5400);
    assert(WorkdayElapsedSeconds(3600,3600+10*3600)==9*3600);
    assert(WorkdayElapsedSeconds(7200,3600)==0);
    puts("Workday clock tests passed");
}
