#include "native_due_date.h"
#include <assert.h>
#include <stdio.h>
static ULONGLONG due_local_utc(int year,int month,int day,int hour){
    SYSTEMTIME local={.wYear=(WORD)year,.wMonth=(WORD)month,.wDay=(WORD)day,.wHour=(WORD)hour},utc;FILETIME file;ULARGE_INTEGER value;
    assert(TzSpecificLocalTimeToSystemTime(NULL,&local,&utc)&&SystemTimeToFileTime(&utc,&file));value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;return value.QuadPart;
}
int main(void)
{
    SYSTEMTIME parsed,today={.wYear=2026,.wMonth=11,.wDay=6};NativeDueText text;
    assert(NativeDueParse(L"2024-02-29",&parsed)&&parsed.wDay==29);
    assert(NativeDueParse(L"2000-02-29",&parsed));
    const wchar_t *bad[]={NULL,L"",L"2026-02-29",L"1900-02-29",L"2026-13-01",L"2026-11-31",L"2026-00-01",L"2026-11-00",L"2026-1-06",L"2026-11-06T00:00:00Z",L" 2026-11-06",L"0000-11-06"};
    for(int i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!NativeDueParse(bad[i],&parsed));
    assert(NativeDueDescribe(L"2026-11-12",&today,FALSE,TRUE,&text)&&text.kind==NATIVE_DUE_NORMAL&&!wcscmp(text.label,L"до 12 ноя"));
    assert(NativeDueDescribe(L"2026-11-06",&today,FALSE,TRUE,&text)&&text.kind==NATIVE_DUE_TODAY&&!wcscmp(text.label,L"Сегодня"));
    assert(wcsstr(text.tooltip,L"6 ноября 2026"));
    assert(NativeDueDescribe(L"2026-11-04",&today,FALSE,TRUE,&text)&&text.kind==NATIVE_DUE_OVERDUE&&!wcscmp(text.label,L"Просрочено · 4 ноя"));
    assert(NativeDueDescribe(L"2026-11-04",&today,TRUE,TRUE,&text)&&text.kind==NATIVE_DUE_NORMAL&&!wcscmp(text.label,L"до 4 ноя"));
    assert(NativeDueDescribe(L"2026-11-06",&today,TRUE,FALSE,&text)&&text.kind==NATIVE_DUE_NORMAL&&!wcscmp(text.label,L"by 6 Nov"));
    assert(NativeDueDescribe(L"2026-11-04",&today,FALSE,FALSE,&text)&&!wcscmp(text.label,L"Overdue · 4 Nov"));
    assert(NativeDueDescribe(L"2027-01-01",&today,FALSE,TRUE,&text)&&wcsstr(text.label,L"2027"));
    today=(SYSTEMTIME){.wYear=2027,.wMonth=1,.wDay=1};
    assert(NativeDueDescribe(L"2026-12-31",&today,FALSE,FALSE,&text)&&text.kind==NATIVE_DUE_OVERDUE&&wcsstr(text.label,L"2026"));
    assert(!NativeDueDescribe(NULL,&today,FALSE,TRUE,&text)&&!text.label[0]&&!text.tooltip[0]);
    ULONGLONG created=due_local_utc(2026,11,1,0),end=NativeDueDeadlineUtc(L"2026-11-12");int amount=-1;
    assert(end>created&&end>due_local_utc(2026,11,12,23));
    assert(NativeDueProgress(created,L"2026-11-12",created,&amount)&&amount==0);
    assert(NativeDueProgress(created,L"2026-11-12",created-1,&amount)&&amount==0);
    assert(NativeDueProgress(created,L"2026-11-12",created+(end-created)/2,&amount)&&amount>=499&&amount<=500);
    assert(NativeDueProgress(created,L"2026-11-12",end,&amount)&&amount==1000);
    assert(NativeDueProgress(created,L"2026-11-12",end+10000000,&amount)&&amount==1000);
    assert(!NativeDueProgress(0,L"2026-11-12",end,&amount));
    assert(!NativeDueProgress(end+1,L"2026-11-12",end,&amount));
    assert(!NativeDueProgress(created,L"bad",end,&amount));
    puts("Due dates: strict Gregorian date-only parsing, leap/year boundaries, RU/EN states and completed neutrality passed");
}
