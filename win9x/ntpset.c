/* NTPSET.EXE -- tiny SNTP clock setter for Windows 95/98 (no CRT, no install, no window)
 *   NTPSET [/V] [server]         server = hostname or IP, default time.metas.ch
 *   /V shows a message box with the result (for testing); otherwise silent.
 * Never prompts, never waits more than ~3 s (3 tries x 1 s) plus the DNS lookup. Exit code 0 = clock set.
 *
 * Public domain (CC0 1.0). */
#include <windows.h>
#include <winsock.h>

#define DEFAULT_SERVER "time.metas.ch"
#define NTP_TO_FILETIME_SECS 9435484800ULL   /* 1601-01-01 -> 1900-01-01 */

static void say(int verbose, const char *msg, UINT icon)
{
    if (verbose) MessageBoxA(NULL, msg, "NTPSET", MB_OK | icon | MB_SETFOREGROUND);
}

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *putu(char *d, unsigned long v)
{
    char b[12]; int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = b[--n];
    *d = 0; return d;
}
static char *put2(char *d, unsigned v) { *d++ = (char)('0' + v / 10); *d++ = (char)('0' + v % 10); *d = 0; return d; }

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static int run(int verbose, const char *server)
{
    WSADATA wd;
    SOCKET s;
    struct sockaddr_in a;
    unsigned char pkt[48], rx[48];
    unsigned long cookie;
    int i, tries, got = 0;
    char msg[160], *p;

    if (WSAStartup(MAKEWORD(1, 1), &wd) != 0) { say(verbose, "Winsock not available.", MB_ICONERROR); return 2; }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) { WSACleanup(); say(verbose, "No UDP socket.", MB_ICONERROR); return 2; }

    for (i = 0; i < (int)sizeof a; i++) ((char *)&a)[i] = 0;
    a.sin_family = AF_INET;
    a.sin_port = htons(123);
    a.sin_addr.s_addr = inet_addr(server);
    if (a.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *h = gethostbyname(server);
        if (!h || h->h_length != 4) {
            closesocket(s); WSACleanup();
            p = put(msg, "Can't resolve "); put(p, server);
            say(verbose, msg, MB_ICONWARNING); return 1;
        }
        for (i = 0; i < 4; i++) ((char *)&a.sin_addr.s_addr)[i] = h->h_addr_list[0][i];
    }

    for (tries = 0; tries < 3 && !got; tries++) {   /* got: 1 = answer, -1 = kiss-o'-death */
        fd_set fs; struct timeval tv; int n;
        for (i = 0; i < 48; i++) pkt[i] = 0;
        pkt[0] = 0x23;                          /* LI 0, version 4, mode 3 (client) */
        cookie = GetTickCount() ^ 0x5A17C0DEUL ^ ((unsigned long)tries << 28);
        pkt[40] = (unsigned char)(cookie >> 24); pkt[41] = (unsigned char)(cookie >> 16);
        pkt[42] = (unsigned char)(cookie >> 8);  pkt[43] = (unsigned char)cookie;   /* transmit ts = cookie */
        if (sendto(s, (const char *)pkt, 48, 0, (struct sockaddr *)&a, sizeof a) != 48) continue;
        for (;;) {
            FD_ZERO(&fs); FD_SET(s, &fs);
            tv.tv_sec = 1; tv.tv_usec = 0;
            if (select(0, &fs, NULL, NULL, &tv) <= 0) break;            /* timeout -> resend */
            n = recvfrom(s, (char *)rx, 48, 0, NULL, NULL);
            if (n < 48) continue;
            if ((rx[0] & 7) != 4) continue;                              /* not a server reply */
            if (be32(rx + 24) != cookie) continue;                       /* origin must echo our ts */
            if (rx[1] == 0) { got = -1; break; }                         /* KoD: stop asking */
            if ((rx[0] >> 6) == 3 || rx[1] > 15) continue;               /* unsynchronised */
            got = 1; break;
        }
    }
    closesocket(s);
    WSACleanup();
    if (got < 0) {
        p = put(msg, "The time server refused the request (kiss-o'-death ");
        for (i = 12; i < 16; i++) *p++ = (rx[i] >= 32 && rx[i] < 127) ? (char)rx[i] : '?';
        put(p, ").");
        say(verbose, msg, MB_ICONWARNING); return 1;
    }
    if (!got) { say(verbose, "No answer from the time server.", MB_ICONWARNING); return 1; }

    {
        unsigned long sec = be32(rx + 40), frac = be32(rx + 44);
        unsigned long long ft;
        FILETIME f; SYSTEMTIME st, before;

        ft = ((unsigned long long)sec + NTP_TO_FILETIME_SECS) * 10000000ULL
           + (((unsigned long long)frac * 10000000ULL) >> 32);
        f.dwLowDateTime = (DWORD)ft; f.dwHighDateTime = (DWORD)(ft >> 32);
        if (!FileTimeToSystemTime(&f, &st) || st.wYear < 2024 || st.wYear > 2099) {
            say(verbose, "Server sent an implausible time; clock left alone.", MB_ICONWARNING); return 1;
        }
        GetSystemTime(&before);
        if (!SetSystemTime(&st)) { say(verbose, "SetSystemTime failed.", MB_ICONERROR); return 1; }
        if (verbose) {
            p = put(msg, "Clock set from "); p = put(p, inet_ntoa(a.sin_addr));
            p = put(p, "\nUTC now:  "); p = putu(p, st.wYear); p = put(p, "-"); p = put2(p, st.wMonth);
            p = put(p, "-"); p = put2(p, st.wDay); p = put(p, " "); p = put2(p, st.wHour); p = put(p, ":");
            p = put2(p, st.wMinute); p = put(p, ":"); p = put2(p, st.wSecond);
            p = put(p, "\nwas:      "); p = put2(p, before.wHour); p = put(p, ":");
            p = put2(p, before.wMinute); p = put(p, ":"); p = put2(p, before.wSecond);
            say(1, msg, MB_ICONINFORMATION);
        }
    }
    return 0;
}

void __stdcall entry(void)
{
    char *c = GetCommandLineA(), *server = DEFAULT_SERVER;
    static char arg[64];
    int verbose = 0;

    /* skip program name (possibly quoted) */
    if (*c == '"') { c++; while (*c && *c != '"') c++; if (*c) c++; }
    else while (*c && *c != ' ' && *c != '\t') c++;
    for (;;) {
        int n = 0;
        while (*c == ' ' || *c == '\t') c++;
        if (!*c) break;
        while (*c && *c != ' ' && *c != '\t' && n < 63) arg[n++] = *c++;
        arg[n] = 0;
        while (*c && *c != ' ' && *c != '\t') c++;
        if ((arg[0] == '/' || arg[0] == '-') && (arg[1] == 'v' || arg[1] == 'V') && !arg[2]) verbose = 1;
        else if (arg[0] != '/' && arg[0] != '-') {
            static char srv[64]; int k;
            for (k = 0; k <= n; k++) srv[k] = arg[k];
            server = srv;
        }
    }
    ExitProcess((UINT)run(verbose, server));
}
