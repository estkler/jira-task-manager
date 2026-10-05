#include <assert.h>
#include <stdio.h>
#include "time_input.h"
int main(void) {
    int h=-1,m=-1;
    assert(ParseStartTime(L"09:30",&h,&m) && h==9 && m==30);
    assert(ParseStartTime(L"930",&h,&m) && h==9 && m==30);
    assert(ParseStartTime(L"9",&h,&m) && h==9 && m==0);
    assert(ParseStartTime(L"2359",&h,&m) && h==23 && m==59);
    assert(!ParseStartTime(L"24:00",&h,&m));
    assert(!ParseStartTime(L"09:60",&h,&m));
    assert(!ParseStartTime(L"",&h,&m));
    assert(!ParseStartTime(L"9x30",&h,&m));
    assert(!ParseStartTime(L"12:3",&h,&m));
    puts("Time input tests passed");
}
