/* timecalc.c -- see timecalc.h */
#include "timecalc.h"

static const unsigned char mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int month_len(int y, int m) { return (m == 2 && leap(y)) ? 29 : mdays[m - 1]; }

void tc_split(tc_u32 secs, struct tc_date *d)
{
    tc_u32 days = secs / 86400UL, rem = secs % 86400UL;
    int y = 1978, m = 1;

    d->wday = (int)(days % 7);          /* 1978-01-01 was a Sunday */
    d->hour = (int)(rem / 3600);
    d->min  = (int)(rem / 60 % 60);
    d->sec  = (int)(rem % 60);
    for (;;) {
        tc_u32 yl = leap(y) ? 366 : 365;
        if (days < yl) break;
        days -= yl; y++;
    }
    for (;;) {
        tc_u32 ml = (tc_u32)month_len(y, m);
        if (days < ml) break;
        days -= ml; m++;
    }
    d->year = y; d->month = m; d->day = (int)days + 1;
}

tc_u32 tc_make(int year, int month, int day, int hour, int min, int sec)
{
    tc_u32 days = 0;
    int y, m;

    for (y = 1978; y < year; y++) days += leap(y) ? 366 : 365;
    for (m = 1; m < month; m++) days += (tc_u32)month_len(year, m);
    days += (tc_u32)(day - 1);
    return days * 86400UL + (tc_u32)hour * 3600UL + (tc_u32)min * 60UL + (tc_u32)sec;
}

/* 01:00 UTC on the last Sunday of the given month */
static tc_u32 last_sunday_0100(int year, int month)
{
    tc_u32 t = tc_make(year, month, month_len(year, month), 1, 0, 0);
    return t - (t / 86400UL % 7) * 86400UL;
}

int tc_eu_dst(tc_u32 utc)
{
    struct tc_date d;
    tc_split(utc, &d);
    return utc >= last_sunday_0100(d.year, 3) && utc < last_sunday_0100(d.year, 10);
}

/* frac * 10^6 / 2^32 == (frac >> 16) * 15625 / 1024 (fits in 32 bits, ~15 us resolution) */
tc_u32 tc_frac_to_micro(tc_u32 frac)
{
    return ((frac >> 16) * 15625UL) >> 10;
}

tc_u32 tc_ntp_to_amiga(tc_u32 ntp_secs)
{
    return (tc_u32)(ntp_secs - (tc_u32)TC_NTP_TO_AMIGA);   /* modulo 2^32 */
}
