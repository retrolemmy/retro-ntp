/* Host-side test for timecalc.c: compares against the host libc (gmtime) for many instants.
 *   cc -O2 -Wall -I.. -o test_timecalc test_timecalc.c ../timecalc.c && ./test_timecalc */
#include <stdio.h>
#include <time.h>
#include "timecalc.h"

#define UNIX_TO_AMIGA 252460800L   /* 1970-01-01 -> 1978-01-01 */

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    long long u;
    int n = 0;

    /* split/make round trip vs gmtime, every ~7.3 h from 1978 to 2110 */
    for (u = UNIX_TO_AMIGA; u < 4407000000LL; u += 26249) {
        tc_u32 a = (tc_u32)(u - UNIX_TO_AMIGA);
        time_t t = (time_t)u;
        struct tm *g = gmtime(&t);
        struct tc_date d;
        tc_split(a, &d);
        CHECK(d.year == g->tm_year + 1900 && d.month == g->tm_mon + 1 && d.day == g->tm_mday &&
              d.hour == g->tm_hour && d.min == g->tm_min && d.sec == g->tm_sec && d.wday == g->tm_wday,
              "split %lld", u);
        CHECK(tc_make(d.year, d.month, d.day, d.hour, d.min, d.sec) == a, "make %lld", u);
        n++;
    }

    /* EU DST boundaries (all at 01:00 UTC) */
    {
        static const struct { int y, m, d; } on[] = { {2026,3,29}, {2027,3,28}, {2024,3,31}, {2030,3,31} };
        static const struct { int y, m, d; } off[] = { {2026,10,25}, {2027,10,31}, {2024,10,27}, {2030,10,27} };
        int i;
        for (i = 0; i < 4; i++) {
            tc_u32 s = tc_make(on[i].y, on[i].m, on[i].d, 1, 0, 0);
            tc_u32 e = tc_make(off[i].y, off[i].m, off[i].d, 1, 0, 0);
            CHECK(!tc_eu_dst(s - 1) && tc_eu_dst(s), "dst start %d", on[i].y);
            CHECK(tc_eu_dst(e - 1) && !tc_eu_dst(e), "dst end %d", off[i].y);
        }
        CHECK(!tc_eu_dst(tc_make(2026, 11, 25, 12, 0, 0)), "Nov not DST");
        CHECK(tc_eu_dst(tc_make(2026, 7, 1, 12, 0, 0)), "Jul DST");
    }

    /* NTP conversion incl. era 1 (after 2036-02-07) */
    {
        tc_u32 ntp_2026 = (tc_u32)(1795600000UL + 2208988800UL);   /* unix 1795600000 */
        CHECK(tc_ntp_to_amiga(ntp_2026) == (tc_u32)(1795600000L - UNIX_TO_AMIGA), "ntp 2026");
        {   /* unix 2200000000 (2039) -> NTP era 1 seconds */
            tc_u32 ntp_2039 = (tc_u32)((2200000000ULL + 2208988800ULL) & 0xFFFFFFFFULL);
            CHECK(tc_ntp_to_amiga(ntp_2039) == (tc_u32)(2200000000ULL - UNIX_TO_AMIGA), "ntp 2039");
        }
    }

    /* fraction -> microseconds */
    CHECK(tc_frac_to_micro(0) == 0, "frac 0");
    CHECK(tc_frac_to_micro(0x80000000UL) == 500000, "frac 0.5");
    CHECK(tc_frac_to_micro(0xFFFFFFFFUL) <= 999999 && tc_frac_to_micro(0xFFFFFFFFUL) >= 999984, "frac max");

    printf("%d instants checked, %d failures\n", n, fails);
    return fails != 0;
}
