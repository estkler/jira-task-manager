#ifndef FTASKS_NATIVE_WORKDAY_CLOCK_H
#define FTASKS_NATIVE_WORKDAY_CLOCK_H

#include <windows.h>

#define WORKDAY_ACTIVATION_HOUR 6
#define WORKDAY_DURATION_SECONDS (9ULL * 60ULL * 60ULL)

static inline BOOL WorkdaySameLocalDate(const SYSTEMTIME *a,const SYSTEMTIME *b)
{
    return a&&b&&a->wYear==b->wYear&&a->wMonth==b->wMonth&&a->wDay==b->wDay;
}

static inline BOOL WorkdayNormalizeStart(const SYSTEMTIME *now,const SYSTEMTIME *start,BOOL has_start)
{
    return has_start&&WorkdaySameLocalDate(now,start);
}

static inline ULONGLONG WorkdayElapsedSeconds(ULONGLONG started,ULONGLONG now)
{
    if(!started||now<=started)return 0;
    ULONGLONG elapsed=now-started;
    return elapsed>WORKDAY_DURATION_SECONDS?WORKDAY_DURATION_SECONDS:elapsed;
}

#endif
