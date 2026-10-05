#ifndef FTASKS_TIME_INPUT_H
#define FTASKS_TIME_INPUT_H
#include <wchar.h>
static int ParseStartTime(const wchar_t *text,int *hours,int *minutes)
{
    size_t n=wcslen(text); int h=0,m=0;
    if(n==5 && text[2]==L':') {
        if(text[0]<L'0'||text[0]>L'9'||text[1]<L'0'||text[1]>L'9'||
           text[3]<L'0'||text[3]>L'9'||text[4]<L'0'||text[4]>L'9')return 0;
        h=(text[0]-L'0')*10+text[1]-L'0';m=(text[3]-L'0')*10+text[4]-L'0';
    } else {
        if(n<1||n>4)return 0;
        for(size_t i=0;i<n;i++){if(text[i]<L'0'||text[i]>L'9')return 0;h=h*10+text[i]-L'0';}
        if(n>2){m=h%100;h/=100;}
    }
    if(h>23||m>59)return 0;
    *hours=h;*minutes=m;return 1;
}
#endif
