/* timecalc.h -- portable calendar helpers for NTPSet (no OS calls, unit-testable on the host).
 * All "secs" values are AmigaOS-style: seconds since 1978-01-01 00:00:00 (unsigned 32 bit,
 * good until 2114). */
#ifndef TIMECALC_H
#define TIMECALC_H

#ifdef __VBCC__
typedef unsigned long tc_u32;
typedef long tc_s32;
#else
#include <stdint.h>
typedef uint32_t tc_u32;
typedef int32_t tc_s32;
#endif

/* 1900-01-01 (NTP epoch) -> 1978-01-01 (Amiga epoch): 28489 days.  Plain unsigned
 * subtraction also does the right thing after the NTP era rollover in 2036. */
#define TC_NTP_TO_AMIGA 2461449600UL

struct tc_date {
    int year, month, day;   /* 1978.., 1..12, 1..31 */
    int hour, min, sec;
    int wday;               /* 0 = Sunday */
};

void   tc_split(tc_u32 secs, struct tc_date *d);
tc_u32 tc_make(int year, int month, int day, int hour, int min, int sec);
int    tc_eu_dst(tc_u32 utc);          /* 1 if EU summer time is in effect at this UTC instant */
tc_u32 tc_frac_to_micro(tc_u32 frac);  /* NTP 2^-32 fraction -> microseconds (no 64-bit math) */
tc_u32 tc_ntp_to_amiga(tc_u32 ntp_secs); /* NTP seconds (any era up to 2114) -> Amiga seconds */

#endif
