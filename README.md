# retro-ntp

Tiny SNTP clock setters for old computers that have no NTP client of their own, a flat RTC
battery, or no RTC at all. Each one asks an NTP server for the time once, sets the clock, and
quits. They're meant to run at every boot, and each binary is only a few KB with no dependencies.

| Dir | Target | Install |
|-----|--------|---------|
| [`amiga/`](amiga/) | AmigaOS 2.04+ with Roadshow, AmiTCP, Miami or any `bsdsocket.library` stack, any 680x0 | `C:NTPSet QUIET` in `S:User-Startup` |
| [`mac/`](mac/) | 68k Mac, System 7.0 – Mac OS 8.1, MacTCP or Open Transport | `NTPSet` in `System Folder:Startup Items` |
| [`win9x/`](win9x/) | Windows 95 / 98 / ME (Winsock 1.1) | `NTPSET.EXE` in the StartUp folder |
| [`common/`](common/) | calendar, EU summer time and NTP-era math shared by the 68k tools, with a host unit test | – |

The default server is `time.metas.ch` (METAS, the Swiss federal metrology institute, stratum 1).
Every tool takes another hostname or IP, so you can point them at a pool server or your own NTP
server on the LAN.

## Time zones

- **Windows** applies its own zone and DST (Control Panel > Date/Time), so `NTPSET.EXE` just
  sets UTC.
- **AmigaOS and classic Mac OS** keep *local* time and know nothing about summer time. The tools
  apply a zone offset plus the EU summer-time rule (last Sunday of March to last Sunday of October,
  01:00 UTC) themselves. The Amiga takes the zone from Locale prefs or `ZONE=`; the Mac reads it
  from a settings resource (default UTC+1). Outside the EU, turn the DST rule off (`NODST` / `none`).
  Don't also run another DST shifter on those machines.

## Good manners with public servers

One query per boot is fine for any public server. The tools retry at most once per second for a few
seconds, and stop at once if a server answers with a "kiss-o'-death" (rate-limit or deny) packet.
Don't run them in a tight loop or from a scheduler every minute.

## Building

Each directory has its own `build.sh` and README:

- **Amiga:** vbcc, vasm and vlink with the NDK 3.2 (`amiga/setup-toolchain.sh` downloads and builds them).
- **Mac:** [Retro68](https://github.com/autc04/Retro68) in Docker (`mac/Dockerfile.retro68`).
- **Win9x:** `i686-w64-mingw32-gcc`, no C runtime.

Prebuilt binaries are attached to the GitHub releases.

## Licence

Public domain, [CC0 1.0](LICENSE). Use it for anything; no credit needed.

The code was written by Claude (Anthropic's AI model) in Claude Code, and tested on real hardware
and in emulators by the repository owner.
