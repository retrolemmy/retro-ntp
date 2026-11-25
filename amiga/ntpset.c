/* NTPSet -- tiny SNTP clock setter for AmigaOS 2.04+ with a bsdsocket.library stack (Roadshow,
 * AmiTCP, Miami, ...). Sibling of the Win9x NTPSET.EXE; meant for S:User-Startup.
 *
 *   NTPSet [SERVER <ip|host>] [ZONE <minutes east of UTC>] [NODST] [NORTC] [TIMEOUT <s>] [TEST] [QUIET]
 *
 * Default server time.metas.ch. AmigaOS keeps local time, so the UTC answer
 * is shifted by the Locale prefs time zone (or ZONE) plus EU summer time (unless NODST).
 * The battery clock is written too (unless NORTC or there is none).
 * Return codes: 0 = clock set, 5 (WARN) = no answer / no network, 10 = bad args or bad time,
 * 20 = no timer.device. Never waits longer than TIMEOUT seconds (default 5); Ctrl-C aborts.
 *
 * Public domain (CC0 1.0). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/timer.h>
#include <libraries/locale.h>
#include <workbench/startup.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>
#include <proto/battclock.h>
#include <proto/bsdsocket.h>
#include "timecalc.h"

#define DEFAULT_SERVER  "time.metas.ch"
#define DEFAULT_TIMEOUT 5
#define TEMPLATE "SERVER,ZONE/K/N,NODST/S,NORTC/S,TIMEOUT/K/N,TEST/S,QUIET/S"
enum { A_SERVER, A_ZONE, A_NODST, A_NORTC, A_TIMEOUT, A_TEST, A_QUIET, A_COUNT };

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct LocaleBase *LocaleBase;
struct Library *BattClockBase;
struct Library *SocketBase;

static int quiet;

/* ---- tiny output helpers (no C library) ---- */
static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *putu(char *d, unsigned long v)
{
    char b[12]; int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = b[--n];
    *d = 0; return d;
}
static char *put2(char *d, unsigned v) { *d++ = (char)('0' + v / 10 % 10); *d++ = (char)('0' + v % 10); *d = 0; return d; }
static char *putdate(char *d, tc_u32 secs)
{
    struct tc_date t;
    tc_split(secs, &t);
    d = putu(d, (unsigned long)t.year); d = put(d, "-"); d = put2(d, t.month); d = put(d, "-");
    d = put2(d, t.day); d = put(d, " "); d = put2(d, t.hour); d = put(d, ":");
    d = put2(d, t.min); d = put(d, ":"); return put2(d, t.sec);
}
static void say(const char *s) { if (!quiet && Output()) PutStr((STRPTR)s); }

static int broke;   /* Ctrl-C seen (reading the signal clears it, so latch it) */
static int break_hit(void)
{
    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) broke = 1;
    return broke;
}

static unsigned long be32(const UBYTE *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}
static void wbe32(UBYTE *p, unsigned long v)
{
    p[0] = (UBYTE)(v >> 24); p[1] = (UBYTE)(v >> 16); p[2] = (UBYTE)(v >> 8); p[3] = (UBYTE)v;
}

/* ---- timer.device ---- */
static struct MsgPort *tport;
static struct timerequest *treq;

static void get_time(struct timeval *tv)
{
    treq->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)treq);
    *tv = treq->tr_time;
}

static void set_time(const struct timeval *tv)
{
    treq->tr_node.io_Command = TR_SETSYSTIME;
    treq->tr_time = *tv;
    DoIO((struct IORequest *)treq);
}

/* microseconds from a to b, clamped to 0..10 s */
static unsigned long elapsed_us(const struct timeval *a, const struct timeval *b)
{
    long s = (long)(b->tv_secs - a->tv_secs), us = (long)b->tv_micro - (long)a->tv_micro;
    if (s < 0 || s > 10) return 0;
    us += s * 1000000L;
    return us < 0 ? 0 : (unsigned long)us;
}

/* zone in minutes east of UTC from Locale prefs (0 if no locale.library) */
static long locale_zone(void)
{
    long z = 0;
    if ((LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 38)) != NULL) {
        struct Locale *loc = OpenLocale(NULL);
        if (loc) { z = -loc->loc_GMTOffset; CloseLocale(loc); }   /* loc_GMTOffset = minutes WEST */
        CloseLibrary((struct Library *)LocaleBase);
        LocaleBase = NULL;
    }
    return z;
}

/* One SNTP exchange per second until an answer arrives or timeout runs out.
 * On success fills rx (48 bytes) and t1/t4 (local clock at send/receive), returns 1.
 * Returns -1 if the server sent a kiss-o'-death (stratum 0; rx holds it), 0 if nothing usable came. */
static int query(const char *server, long timeout, UBYTE *rx, struct timeval *t1, struct timeval *t4,
                 char *peer)
{
    struct sockaddr_in a;
    long s = -1, tries;
    int resolved = 0, got = 0, i;
    UBYTE pkt[48];

    for (i = 0; i < (int)sizeof a; i++) ((UBYTE *)&a)[i] = 0;
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = 123;                                   /* m68k is big endian already */

    for (tries = 0; tries < timeout && !got && !break_hit(); tries++) {
        struct timeval tv;
        fd_set fs;
        ULONG sigs;
        LONG n;

        /* the stack may still be coming up while User-Startup runs: keep trying */
        if (!SocketBase && !(SocketBase = OpenLibrary("bsdsocket.library", 3))) { Delay(50); continue; }
        if (!resolved) {
            a.sin_addr.s_addr = inet_addr((STRPTR)server);
            if (a.sin_addr.s_addr == INADDR_NONE) {
                struct hostent *h = gethostbyname((STRPTR)server);
                if (!h || h->h_length != 4) { Delay(50); continue; }
                a.sin_addr.s_addr = be32((const UBYTE *)h->h_addr_list[0]);   /* no odd-address long read on 68000 */
            }
            resolved = 1;
        }
        if (s < 0 && (s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { Delay(50); continue; }

        for (i = 0; i < 48; i++) pkt[i] = 0;
        pkt[0] = 0x23;                                  /* LI 0, version 4, mode 3 (client) */
        get_time(t1);
        wbe32(pkt + 40, t1->tv_secs);                   /* transmit ts doubles as our cookie */
        wbe32(pkt + 44, t1->tv_micro ^ ((unsigned long)tries << 24) ^ 0x5A17C0DEUL);
        if (sendto(s, pkt, 48, 0, (struct sockaddr *)&a, sizeof a) != 48) { Delay(50); continue; }

        for (;;) {
            FD_ZERO(&fs); FD_SET(s, &fs);
            tv.tv_secs = 1; tv.tv_micro = 0;
            sigs = SIGBREAKF_CTRL_C;
            n = WaitSelect(s + 1, &fs, NULL, NULL, &tv, &sigs);
            if (sigs & SIGBREAKF_CTRL_C) broke = 1;
            if (n <= 0) break;                                                /* timeout/break */
            n = recvfrom(s, rx, 48, 0, NULL, NULL);
            get_time(t4);
            if (n < 48) continue;
            if ((rx[0] & 7) != 4) continue;                                   /* not a server reply */
            if (be32(rx + 24) != be32(pkt + 40) || be32(rx + 28) != be32(pkt + 44)) continue;
            if (rx[1] == 0) { got = -1; break; }                              /* KoD: stop asking */
            if ((rx[0] >> 6) == 3 || rx[1] > 15) continue;                    /* unsynchronised */
            got = 1; break;
        }
    }
    if (s >= 0) CloseSocket(s);
    if (got > 0 && peer) {
        unsigned long ip = a.sin_addr.s_addr;
        peer = putu(peer, ip >> 24); peer = put(peer, ".");
        peer = putu(peer, (ip >> 16) & 255); peer = put(peer, ".");
        peer = putu(peer, (ip >> 8) & 255); peer = put(peer, ".");
        putu(peer, ip & 255);
    }
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
    return got;
}

static int run(LONG *args)
{
    const char *server = args[A_SERVER] ? (const char *)args[A_SERVER] : DEFAULT_SERVER;
    long timeout = args[A_TIMEOUT] ? *(LONG *)args[A_TIMEOUT] : DEFAULT_TIMEOUT;
    long zone = args[A_ZONE] ? *(LONG *)args[A_ZONE] : locale_zone();
    UBYTE rx[48];
    struct timeval t1, t4, now;
    tc_u32 utc, local, micro, shift;
    int dst, i;
    char msg[200], peer[16], *p;

    if (timeout < 1) timeout = 1;
    if (zone < -720 || zone > 840) { say("NTPSet: ZONE must be -720..840 minutes\n"); return RETURN_ERROR; }
    for (i = 0; server[i]; i++) ;
    if (i > 100) { say("NTPSet: SERVER name too long\n"); return RETURN_ERROR; }

    i = query(server, timeout, rx, &t1, &t4, peer);
    if (i < 0) {
        p = put(msg, "NTPSet: "); p = put(p, server); p = put(p, " refused the request (kiss-o'-death ");
        for (i = 12; i < 16; i++) *p++ = (rx[i] >= 32 && rx[i] < 127) ? (char)rx[i] : '?';
        put(p, "), clock left alone\n");
        say(msg);
        return RETURN_WARN;
    }
    if (!i) {
        if (break_hit()) { say("NTPSet: ***Break\n"); return RETURN_WARN; }
        p = put(msg, "NTPSet: no answer from "); p = put(p, server); put(p, ", clock left alone\n");
        say(msg);
        return RETURN_WARN;
    }

    /* server transmit time + half the round trip */
    utc = tc_ntp_to_amiga(be32(rx + 40));
    micro = tc_frac_to_micro(be32(rx + 44)) + elapsed_us(&t1, &t4) / 2;
    utc += micro / 1000000UL; micro %= 1000000UL;

    dst = !args[A_NODST] && tc_eu_dst(utc);
    shift = (tc_u32)(zone * 60 + (dst ? 3600 : 0));
    local = utc + shift;

    {
        struct tc_date d;
        tc_split(utc, &d);
        if (d.year < 2024 || d.year > 2099) { say("NTPSet: server sent an implausible time, clock left alone\n"); return RETURN_ERROR; }
    }

    get_time(&now);   /* "was" = local clock right now, before we change it */
    if (!args[A_TEST]) {
        struct timeval nt;
        nt.tv_secs = local; nt.tv_micro = micro;
        set_time(&nt);
        if (!args[A_NORTC] && (BattClockBase = (struct Library *)OpenResource("battclock.resource")) != NULL)
            WriteBattClock(local);
    }

    p = put(msg, args[A_TEST] ? "NTPSet: server time " : "NTPSet: clock set to ");
    p = putdate(p, local);
    p = put(p, " (UTC");
    {
        long z = zone + (dst ? 60 : 0);
        if (z < 0) { p = put(p, "-"); z = -z; } else p = put(p, "+");
        p = put2(p, (unsigned)(z / 60)); p = put(p, ":"); p = put2(p, (unsigned)(z % 60));
    }
    if (dst) p = put(p, " DST");
    p = put(p, ") from "); p = put(p, peer);
    {
        /* difference new - old in tenths of a second, if it is small enough to be interesting */
        long ds = (long)(local - now.tv_secs);
        if (ds > -86400L && ds < 86400L) {
            long tenths = ds * 10 + ((long)micro - (long)now.tv_micro) / 100000L;
            p = put(p, ", was off by ");
            if (tenths < 0) { p = put(p, "-"); tenths = -tenths; } else p = put(p, "+");
            p = putu(p, (unsigned long)tenths / 10); p = put(p, ".");
            p = putu(p, (unsigned long)tenths % 10); p = put(p, " s");
        } else {
            p = put(p, ", was "); p = putdate(p, now.tv_secs);
        }
    }
    put(p, "\n");
    say(msg);
    return RETURN_OK;
}

LONG ntpmain(void)
{
    struct Process *me;
    struct WBStartup *wbmsg = NULL;
    struct RDArgs *rda = NULL;
    LONG args[A_COUNT];
    LONG rc = RETURN_FAIL;
    int i;

    me = (struct Process *)FindTask(NULL);
    if (!me->pr_CLI) {                                  /* started from Workbench */
        WaitPort(&me->pr_MsgPort);
        wbmsg = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
    }
    if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37)) == NULL) goto out;
    for (i = 0; i < A_COUNT; i++) args[i] = 0;

    if (wbmsg) quiet = 1;                               /* no console: defaults, silent */
    else if ((rda = ReadArgs(TEMPLATE, args, NULL)) == NULL) {
        PrintFault(IoErr(), "NTPSet");
        rc = RETURN_ERROR;
        goto out;
    } else quiet = args[A_QUIET] != 0;

    if ((tport = CreateMsgPort()) != NULL &&
        (treq = (struct timerequest *)CreateIORequest(tport, sizeof *treq)) != NULL &&
        OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)treq, 0) == 0) {
        rc = run(args);
        CloseDevice((struct IORequest *)treq);
    } else say("NTPSet: cannot open timer.device\n");
    if (treq) DeleteIORequest((struct IORequest *)treq);
    if (tport) DeleteMsgPort(tport);

out:
    if (rda) FreeArgs(rda);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    if (wbmsg) { Forbid(); ReplyMsg((struct Message *)wbmsg); }
    return rc;
}
