/* NTPSet -- tiny SNTP clock setter for 68k Macs (System 7.0 - Mac OS 8.1), sibling of the Win9x
 * NTPSET.EXE and the Amiga NTPSet. Put it in "Startup Items"; it runs silently, sets the clock
 * from an NTP server (default time.metas.ch) and quits. Hold Option while it starts to see an alert
 * with the result. The outcome of the last run is always written to
 * "System Folder:Preferences:NTPSet Status".
 *
 * Network: MacTCP's UDP driver API (.IPP), which works with real MacTCP and with Open Transport's
 * MacTCP compatibility (OT 1.1+ on 7.5.x/7.6/8.x).
 * Settings: resource STR# 128 (edit with ResEdit):
 *   1  server hostname or IP           default time.metas.ch
 *   2  zone: minutes east of UTC, or "auto" (= Map / Date & Time offset)   default 60
 *   3  DST rule: "EU" or "none"        default EU
 *   4  timeout in seconds              default 10
 * Mac OS keeps local time and (before 8.5) knows nothing about summer time, so NTPSet adds the zone
 * plus EU summer time itself. Leave the Date & Time "Daylight Saving Time" box unticked.
 *
 * Public domain (CC0 1.0). */
#include <Multiverse.h>
#include "timecalc.h"

#ifndef kOnSystemDisk
#define kOnSystemDisk (-32768)
#endif
#ifndef kCreateFolder
#define kCreateFolder true
#endif

#define NTP_TO_MAC    126144000UL    /* 1900-01-01 -> 1904-01-01 */
#define MAC_TO_AMIGA  2335305600UL   /* 1904-01-01 -> 1978-01-01 (timecalc's epoch) */

/* ---- MacTCP UDP interface (Multiversal has only the TCP half: ip_addr, StreamPtr, errors) ---- */
typedef unsigned short udp_port;

enum { UDPCreate = 20, UDPRead = 21, UDPBfrReturn = 22, UDPWrite = 23, UDPRelease = 24 };

typedef struct { Ptr rcvBuff; unsigned long rcvBuffLen; Ptr notifyProc; udp_port localPort;
                 Ptr userDataPtr; udp_port endingPort; } UDPCreatePB;
typedef struct { unsigned short reserved; ip_addr remoteHost; udp_port remotePort; Ptr wdsPtr;
                 Boolean checkSum; short sendLength; Ptr userDataPtr; udp_port localPort; } UDPSendPB;
typedef struct { unsigned short timeOut; ip_addr remoteHost; udp_port remotePort; Ptr rcvBuff;
                 unsigned short rcvBuffLen; unsigned short secondTimeStamp; Ptr userDataPtr;
                 ip_addr destHost; udp_port destPort; } UDPReceivePB;
typedef struct {
    char fill12[12];
    Ptr ioCompletion;
    volatile short ioResult;
    Ptr ioNamePtr;
    short ioVRefNum;
    short ioCRefNum;
    short csCode;
    StreamPtr udpStream;
    union { UDPCreatePB create; UDPSendPB send; UDPReceivePB receive; } csParam;
} UDPiopb;
typedef struct { unsigned short length; Ptr ptr; unsigned short end; } WDS1;

/* layout checks against MacTCP.h (68k: 2-byte alignment) */
typedef char check_hdr[(__builtin_offsetof(UDPiopb, udpStream) == 28) ? 1 : -1];
typedef char check_send[(__builtin_offsetof(UDPSendPB, checkSum) == 12 &&
                         __builtin_offsetof(UDPSendPB, sendLength) == 14) ? 1 : -1];
typedef char check_recv[(__builtin_offsetof(UDPReceivePB, rcvBuff) == 8 &&
                         __builtin_offsetof(UDPReceivePB, rcvBuffLen) == 12) ? 1 : -1];

static short ipp;

static OSErr udp(UDPiopb *pb, short cs)
{
    pb->ioCompletion = NULL;
    pb->ioCRefNum = ipp;
    pb->csCode = cs;
    return PBControlSync((ParmBlkPtr)pb);
}

/* ---- MacTCP name resolver (DNR). The resolver is a 'dnrp' code resource in the MacTCP or Open
 * Transport TCP/IP control panel; this is a minimal version of Apple's dnr.c glue. ---- */
enum { OPENRESOLVER = 1, CLOSERESOLVER = 2, STRTOADDR = 3 };
/* inProgress (1), cacheFault (-23042) and noAnsErr come from Multiverse.h */
struct hostInfo { long rtnCode; char cname[255]; unsigned long addr[4]; };
typedef char check_hi[(__builtin_offsetof(struct hostInfo, addr) == 260) ? 1 : -1];

static Handle dnr_code;

/* the resolver uses MPW C conventions (args on the stack, D0-D2/A0-A1 scratch) */
static short dnr_call(long a, long b, long c, long d, long e)
{
    register long r __asm__("d0");
    Ptr proc = *dnr_code;
    __asm__ volatile ("move.l %5,-(%%sp)\n\tmove.l %4,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                      "move.l %2,-(%%sp)\n\tmove.l %1,-(%%sp)\n\tjsr (%6)\n\tlea 20(%%sp),%%sp"
                      : "=d"(r) : "r"(a), "r"(b), "r"(c), "r"(d), "r"(e), "a"(proc)
                      : "d1", "d2", "a0", "a1", "memory", "cc");
    return (short)r;
}

/* called at interrupt time when the lookup finishes; no globals, just the flag we passed */
static pascal void dnr_done(struct hostInfo *h, Ptr user) { (void)h; *(volatile char *)user = 1; }

/* open the resource fork of the TCP/IP ('ztcp') or MacTCP ('mtcp') control panel */
static short open_tcp_cdev(void)
{
    static const OSType folders[2] = { kControlPanelFolderType, kSystemFolderType };
    static const OSType creators[2] = { 'ztcp', 'mtcp' };
    int f, c; short i, vref, rf; long dir;
    Str63 name; CInfoPBRec cpb; FSSpec spec;
    for (c = 0; c < 2; c++)
        for (f = 0; f < 2; f++) {
            if (FindFolder(kOnSystemDisk, folders[f], false, &vref, &dir) != noErr) continue;
            for (i = 1; ; i++) {
                cpb.hFileInfo.ioCompletion = NULL; cpb.hFileInfo.ioNamePtr = name;
                cpb.hFileInfo.ioVRefNum = vref; cpb.hFileInfo.ioDirID = dir; cpb.hFileInfo.ioFDirIndex = i;
                if (PBGetCatInfoSync(&cpb) != noErr) break;
                if (cpb.hFileInfo.ioFlAttrib & 0x10) continue;                  /* folder */
                if (cpb.hFileInfo.ioFlFndrInfo.fdType != 'cdev' ||
                    cpb.hFileInfo.ioFlFndrInfo.fdCreator != creators[c]) continue;
                if (FSMakeFSSpec(vref, dir, name, &spec) != noErr) continue;
                rf = FSpOpenResFile(&spec, fsRdPerm);
                if (rf != -1) return rf;
            }
        }
    return -1;
}

static OSErr dnr_open(void)
{
    short save = CurResFile(), rf = open_tcp_cdev();
    OSErr err;
    dnr_code = GetIndResource('dnrp', 1);              /* also finds one in the System file */
    if (dnr_code) DetachResource(dnr_code);
    if (rf != -1) CloseResFile(rf);
    UseResFile(save);
    if (!dnr_code) return resNotFound;
    HLock(dnr_code);
    err = dnr_call(OPENRESOLVER, 0, 0, 0, 0);
    if (err != noErr) { DisposeHandle(dnr_code); dnr_code = NULL; }
    return err;
}

static void dnr_close(void)
{
    if (!dnr_code) return;
    dnr_call(CLOSERESOLVER, 0, 0, 0, 0);
    DisposeHandle(dnr_code);
    dnr_code = NULL;
}

/* resolve name until deadline (ticks); returns 0 or a MacTCP/DNR error */
static long resolve(const char *name, unsigned long deadline, unsigned long *ip)
{
    static struct hostInfo hi;                          /* filled at interrupt time */
    static volatile char done;
    OSErr err;
    long rc;
    if ((err = dnr_open()) != noErr) return err;
    done = 0;
    err = dnr_call(STRTOADDR, (long)name, (long)&hi, (long)dnr_done, (long)&done);
    if (err == cacheFault) {                            /* not cached: wait for dnr_done */
        while (!done && TickCount() < deadline) ;
        rc = done ? *(volatile long *)&hi.rtnCode : noAnsErr;
    } else rc = err;
    if (rc == noErr) *ip = hi.addr[0];
    dnr_close();
    return rc;
}

/* ---- XPRAM machine location (Map control panel), _ReadXPRam 12 bytes at $E4 ---- */
static long map_gmt_delta(void)
{
    long loc[3] = { 0, 0, 0 }, d;
    register long *a0 __asm__("a0") = loc;
    register long d0 __asm__("d0") = 0x000C00E4;
    __asm__ volatile (".short 0xA051" : "+r"(d0), "+r"(a0) : : "d1", "a1", "memory");
    d = loc[2] & 0x00FFFFFF;                      /* low 24 bits = gmtDelta (seconds, signed) */
    if (d & 0x00800000) d |= (long)0xFF000000;
    return d;
}

/* ---- small string helpers ---- */
static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *putu(char *d, unsigned long v)
{
    char b[12]; int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = b[--n];
    *d = 0; return d;
}
static char *put2(char *d, unsigned v) { *d++ = (char)('0' + v / 10 % 10); *d++ = (char)('0' + v % 10); *d = 0; return d; }
static char *putdate(char *d, unsigned long mac)
{
    struct tc_date t;
    tc_split((tc_u32)(mac - MAC_TO_AMIGA), &t);
    d = putu(d, (unsigned long)t.year); d = put(d, "-"); d = put2(d, t.month); d = put(d, "-");
    d = put2(d, t.day); d = put(d, " "); d = put2(d, t.hour); d = put(d, ":");
    d = put2(d, t.min); d = put(d, ":"); return put2(d, t.sec);
}
static char *puterr(char *d, OSErr e)
{
    if (e < 0) { *d++ = '-'; return putu(d, (unsigned long)-e); }
    return putu(d, (unsigned long)e);
}
static char *putip(char *d, unsigned long ip)
{
    d = putu(d, ip >> 24); d = put(d, "."); d = putu(d, (ip >> 16) & 255); d = put(d, ".");
    d = putu(d, (ip >> 8) & 255); d = put(d, "."); return putu(d, ip & 255);
}
static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}
static int parse_ip(const char *s, unsigned long *ip)
{
    unsigned long v = 0, part; int n;
    for (n = 0; n < 4; n++) {
        if (*s < '0' || *s > '9') return 0;
        for (part = 0; *s >= '0' && *s <= '9'; s++) part = part * 10 + (unsigned long)(*s - '0');
        if (part > 255) return 0;
        v = (v << 8) | part;
        if (n < 3 && *s++ != '.') return 0;
    }
    if (*s) return 0;
    *ip = v; return 1;
}
static int parse_long(const char *s, long *out)
{
    long v = 0; int neg = 0;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    if (!*s) return 0;
    for (; *s; s++) { if (*s < '0' || *s > '9') return 0; v = v * 10 + (*s - '0'); }
    *out = neg ? -v : v; return 1;
}
static int eq_nocase(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if ((*a | 0x20) != (*b | 0x20)) return 0;
    return *a == *b;
}
/* STR# 128 item n as a C string, or def */
static void setting(short n, const char *def, char *out)
{
    Str255 p;
    GetIndString(p, 128, n);
    if (p[0]) { int i; for (i = 0; i < p[0]; i++) out[i] = (char)p[i + 1]; out[p[0]] = 0; }
    else put(out, def);
}

/* ---- status file in the Preferences folder (last run only) ---- */
static void write_status(const char *msg)
{
    short vref, ref; long dir, len = 0;
    FSSpec spec;
    while (msg[len]) len++;
    if (FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &vref, &dir) != noErr) return;
    if (FSMakeFSSpec(vref, dir, "\pNTPSet Status", &spec) == fnfErr) FSpCreate(&spec, 'ttxt', 'TEXT', 0);
    if (FSpOpenDF(&spec, fsWrPerm, &ref) != noErr) return;
    SetEOF(ref, 0);
    FSWrite(ref, &len, msg);
    FSClose(ref);
    FlushVol(NULL, vref);
}

static void show_alert(const char *msg)
{
    Str255 p; int n = 0;
    InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus(); TEInit(); InitDialogs(NULL);
    InitCursor();
    while (msg[n] && n < 255) { p[n + 1] = (unsigned char)(msg[n] == '\n' ? '\r' : msg[n]); n++; }
    p[0] = (unsigned char)n;
    ParamText(p, "\p", "\p", "\p");
    NoteAlert(128, NULL);
}

static int option_down(void)
{
    unsigned char km[16];
    GetKeys(km);
    return (km[7] & 0x04) != 0;                      /* key code 0x3A */
}

/* ---- the SNTP exchange ---- */
static int run(char *msg)
{
    char sv[64], zs[16], ds[16], ts[16], *p;
    unsigned long server = 0, deadline, t1 = 0, t4 = 0, cookie = 0, was;
    long zone, timeout;
    int got = 0, kod = 0, dst_rule, dst = 0, sent = 0, rcvd = 0;
    OSErr werr = 0, rerr = 0;
    unsigned long myip = 0;
    static unsigned char rbuf[4096];
    unsigned char pkt[48], rx[48];
    UDPiopb pb;
    WDS1 wds;
    OSErr err;
    int i;

    setting(1, "time.metas.ch", sv);
    setting(2, "60", zs);
    setting(3, "EU", ds);
    setting(4, "10", ts);
    if (eq_nocase(zs, "auto")) zone = map_gmt_delta() / 60;
    else if (!parse_long(zs, &zone) || zone < -720 || zone > 840) { put(msg, "Bad zone in STR# 128 (minutes or auto)"); return 2; }
    dst_rule = eq_nocase(ds, "EU");
    if (!parse_long(ts, &timeout) || timeout < 1) timeout = 10;

    if ((err = OpenDriver("\p.IPP", &ipp)) != noErr) {
        p = put(msg, "TCP/IP (MacTCP or Open Transport) not available, error -");
        putu(p, (unsigned long)-err);
        return 1;
    }

    {   /* our own address, for the diagnostics */
        GetAddrParamBlock ga;
        for (i = 0; i < (int)sizeof ga; i++) ((char *)&ga)[i] = 0;
        ga.ioCRefNum = ipp; ga.csCode = ipctlGetAddr;
        if (PBControlSync((ParmBlkPtr)&ga) == noErr) myip = ga.ourAddress;
    }

    deadline = TickCount() + (unsigned long)timeout * 60;
    if (!parse_ip(sv, &server)) {
        long rc = resolve(sv, deadline, &server);
        if (rc != noErr) {
            p = put(msg, "Can't resolve "); p = put(p, sv); p = put(p, " (DNS error ");
            p = puterr(p, (OSErr)rc); p = put(p, "); clock left alone. [my IP "); p = putip(p, myip);
            put(p, "]");
            return 1;
        }
    }

    for (i = 0; i < (int)sizeof pb; i++) ((char *)&pb)[i] = 0;
    pb.csParam.create.rcvBuff = (Ptr)rbuf;
    pb.csParam.create.rcvBuffLen = sizeof rbuf;
    if ((err = udp(&pb, UDPCreate)) != noErr) {
        p = put(msg, "UDPCreate failed, error -"); putu(p, (unsigned long)-err); return 1;
    }
    {
        StreamPtr stream = pb.udpStream;
        while (!got && !kod && TickCount() < deadline) {
            for (i = 0; i < 48; i++) pkt[i] = 0;
            pkt[0] = 0x23;                                     /* LI 0, version 4, mode 3 (client) */
            GetDateTime(&was);
            t1 = TickCount();
            cookie = was ^ (t1 << 8) ^ 0x5A17C0DEUL;
            pkt[40] = (unsigned char)(cookie >> 24); pkt[41] = (unsigned char)(cookie >> 16);
            pkt[42] = (unsigned char)(cookie >> 8);  pkt[43] = (unsigned char)cookie;
            wds.length = 48; wds.ptr = (Ptr)pkt; wds.end = 0;
            for (i = 0; i < (int)sizeof pb; i++) ((char *)&pb)[i] = 0;
            pb.udpStream = stream;
            pb.csParam.send.remoteHost = server;
            pb.csParam.send.remotePort = 123;
            pb.csParam.send.wdsPtr = (Ptr)&wds;
            pb.csParam.send.checkSum = true;
            if ((werr = udp(&pb, UDPWrite)) != noErr) { unsigned long w = TickCount() + 60; while (TickCount() < w) ; continue; }
            sent++;

            for (;;) {                                         /* read until timeout (1 s) */
                for (i = 0; i < (int)sizeof pb; i++) ((char *)&pb)[i] = 0;
                pb.udpStream = stream;
                pb.csParam.receive.timeOut = 1;
                rerr = udp(&pb, UDPRead);
                if (rerr != noErr) break;
                t4 = TickCount();
                rcvd++;
                if (pb.csParam.receive.rcvBuffLen >= 48 && pb.csParam.receive.remoteHost == server &&
                    pb.csParam.receive.remotePort == 123) {
                    for (i = 0; i < 48; i++) rx[i] = ((unsigned char *)pb.csParam.receive.rcvBuff)[i];
                    if ((rx[0] & 7) == 4 && be32(rx + 24) == cookie) {
                        if (rx[1] == 0) kod = 1;               /* kiss-o'-death: stop asking */
                        else if ((rx[0] >> 6) != 3 && rx[1] <= 15) got = 1;
                    }
                }
                udp(&pb, UDPBfrReturn);                        /* rcvBuff still set from the read */
                if (got || kod) break;
            }
        }
        for (i = 0; i < (int)sizeof pb; i++) ((char *)&pb)[i] = 0;
        pb.udpStream = stream;
        udp(&pb, UDPRelease);
    }
    if (kod) {
        char code[5];
        for (i = 0; i < 4; i++) code[i] = (rx[12 + i] >= 32 && rx[12 + i] < 127) ? (char)rx[12 + i] : '?';
        code[4] = 0;
        p = put(msg, "Server "); p = putip(p, server); p = put(p, " refused the request (kiss-o'-death ");
        p = put(p, code); put(p, "); clock left alone.");
        return 1;
    }
    if (!got) {
        p = put(msg, "No answer from "); p = put(p, sv); p = put(p, " ("); p = putip(p, server); p = put(p, ")"); p = put(p, "; clock left alone. [my IP ");
        p = putip(p, myip); p = put(p, ", sent "); p = putu(p, (unsigned long)sent);
        p = put(p, ", received "); p = putu(p, (unsigned long)rcvd);
        p = put(p, ", write err "); p = puterr(p, werr); p = put(p, ", read err "); p = puterr(p, rerr);
        put(p, "]");
        return 1;
    }

    {
        /* server transmit time + half the round trip, in ticks (1/60 s) */
        unsigned long utc = be32(rx + 40) - NTP_TO_MAC;
        unsigned long ticks = ((be32(rx + 44) >> 16) * 60UL) >> 16;
        unsigned long local, now, tw;
        long z;
        struct tc_date d;

        ticks += (t4 - t1) / 2;
        utc += ticks / 60; ticks %= 60;
        tc_split((tc_u32)(utc - MAC_TO_AMIGA), &d);
        if (d.year < 2024 || d.year > 2039) { put(msg, "Server sent an implausible time; clock left alone."); return 2; }
        dst = dst_rule && tc_eu_dst((tc_u32)(utc - MAC_TO_AMIGA));
        local = utc + (unsigned long)(zone * 60 + (dst ? 3600 : 0));

        /* land on the next whole second: wait out the remaining ticks since the answer arrived */
        GetDateTime(&was);
        tw = TickCount();
        ticks += tw - t4;
        local += ticks / 60; ticks %= 60;
        if (ticks) { while (TickCount() < tw + (60 - ticks)) ; local++; }
        SetDateTime(local);
        GetDateTime(&now);

        p = put(msg, "Clock set to "); p = putdate(p, local);
        p = put(p, " (UTC");
        z = zone + (dst ? 60 : 0);
        if (z < 0) { p = put(p, "-"); z = -z; } else p = put(p, "+");
        p = put2(p, (unsigned)(z / 60)); p = put(p, ":"); p = put2(p, (unsigned)(z % 60));
        if (dst) p = put(p, " DST");
        p = put(p, ") from "); p = put(p, sv);
        {
            long ds2 = (long)(local - was);
            if (ds2 > -86400L && ds2 < 86400L) {
                p = put(p, ", was off by ");
                if (ds2 < 0) { p = put(p, "-"); ds2 = -ds2; } else p = put(p, "+");
                p = putu(p, (unsigned long)ds2); p = put(p, " s.");
            } else { p = put(p, ", was "); p = putdate(p, was); p = put(p, "."); }
        }
    }
    return 0;
}

int main(void)
{
    static char msg[300];
    int verbose = option_down(), rc;

    rc = run(msg);
    write_status(msg);
    if (verbose) show_alert(msg);
    return rc;
}
