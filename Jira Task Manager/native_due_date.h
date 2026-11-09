#ifndef TASKLET_DUE_DATE_H
#define TASKLET_DUE_DATE_H
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
enum { NATIVE_DUE_NONE,NATIVE_DUE_NORMAL,NATIVE_DUE_TODAY,NATIVE_DUE_OVERDUE };
typedef struct NativeDueText { wchar_t label[96],tooltip[128];int kind; } NativeDueText;
static BOOL NativeDueParse(const wchar_t *iso,SYSTEMTIME *out)
{
    if(out)ZeroMemory(out,sizeof(*out));if(!iso||wcslen(iso)!=10||iso[4]!=L'-'||iso[7]!=L'-')return FALSE;
    for(int i=0;i<10;i++)if(i!=4&&i!=7&&(iso[i]<L'0'||iso[i]>L'9'))return FALSE;
    int year=(iso[0]-L'0')*1000+(iso[1]-L'0')*100+(iso[2]-L'0')*10+iso[3]-L'0';
    int month=(iso[5]-L'0')*10+iso[6]-L'0',day=(iso[8]-L'0')*10+iso[9]-L'0';
    if(!year||month<1||month>12||day<1)return FALSE;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int maximum=days[month-1]+(month==2&&year%4==0&&(year%100!=0||year%400==0));
    if(day>maximum)return FALSE;
    if(out){out->wYear=(WORD)year;out->wMonth=(WORD)month;out->wDay=(WORD)day;}return TRUE;
}
static int NativeDueDayKey(const SYSTEMTIME *date){return date?(int)date->wYear*10000+date->wMonth*100+date->wDay:0;}
/* Jira due dates have no time: the whole local due day remains available. */
static ULONGLONG NativeDueDeadlineUtc(const wchar_t *iso)
{
    SYSTEMTIME local,utc;FILETIME file;ULARGE_INTEGER value;
    if(!NativeDueParse(iso,&local))return 0;
    local.wHour=23;local.wMinute=59;local.wSecond=59;local.wMilliseconds=999;
    if(!TzSpecificLocalTimeToSystemTime(NULL,&local,&utc)||!SystemTimeToFileTime(&utc,&file))return 0;
    value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;return value.QuadPart;
}
static BOOL NativeDueProgress(ULONGLONG created,const wchar_t *iso,ULONGLONG now,int *permille)
{
    ULONGLONG end=NativeDueDeadlineUtc(iso);
    if(!permille||!created||end<=created)return FALSE;
    *permille=now<=created?0:now>=end?1000:(int)((double)(now-created)/(double)(end-created)*1000.0);
    return TRUE;
}
static BOOL NativeDueDescribe(const wchar_t *iso,const SYSTEMTIME *today,BOOL done,BOOL russian,NativeDueText *out)
{
    if(!out)return FALSE;ZeroMemory(out,sizeof(*out));SYSTEMTIME due;
    if(!NativeDueParse(iso,&due))return FALSE;
    static const wchar_t *ru[]={L"янв",L"фев",L"мар",L"апр",L"мая",L"июн",L"июл",L"авг",L"сен",L"окт",L"ноя",L"дек"};
    static const wchar_t *full_ru[]={L"января",L"февраля",L"марта",L"апреля",L"мая",L"июня",L"июля",L"августа",L"сентября",L"октября",L"ноября",L"декабря"};
    static const wchar_t *en[]={L"Jan",L"Feb",L"Mar",L"Apr",L"May",L"Jun",L"Jul",L"Aug",L"Sep",L"Oct",L"Nov",L"Dec"};
    int compared=NativeDueDayKey(&due)-NativeDueDayKey(today);
    out->kind=done||!today||!today->wYear?NATIVE_DUE_NORMAL:compared==0?NATIVE_DUE_TODAY:compared<0?NATIVE_DUE_OVERDUE:NATIVE_DUE_NORMAL;
    wchar_t date[40];const wchar_t *month=russian?ru[due.wMonth-1]:en[due.wMonth-1];
    if(today&&due.wYear==today->wYear)swprintf(date,40,L"%u %ls",due.wDay,month);
    else swprintf(date,40,L"%u %ls %u",due.wDay,month,due.wYear);
    if(out->kind==NATIVE_DUE_TODAY)wcscpy_s(out->label,96,russian?L"Сегодня":L"Today");
    else swprintf(out->label,96,out->kind==NATIVE_DUE_OVERDUE?(russian?L"Просрочено · %ls":L"Overdue · %ls"):(russian?L"до %ls":L"by %ls"),date);
    swprintf(out->tooltip,128,russian?L"Срок: %u %ls %u г.":L"Due: %u %ls %u",due.wDay,russian?full_ru[due.wMonth-1]:en[due.wMonth-1],due.wYear);
    return TRUE;
}
static void NativeDueCompactLabel(const wchar_t *iso,const SYSTEMTIME *today,NativeDueText *text)
{
    SYSTEMTIME due;if(!text||text->kind==NATIVE_DUE_TODAY||!NativeDueParse(iso,&due))return;
    wchar_t date[32];
    if(today&&due.wYear==today->wYear)swprintf(date,32,L"%02u.%02u",due.wDay,due.wMonth);
    else swprintf(date,32,L"%02u.%02u.%02u",due.wDay,due.wMonth,due.wYear%100);
    swprintf(text->label,96,text->kind==NATIVE_DUE_OVERDUE?L"! %ls":L"%ls",date);
}
#endif
