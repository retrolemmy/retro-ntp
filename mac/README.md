# NTPSet (68k Mac)

Tiny SNTP clock setter for 68k Macs running System 7.0 to Mac OS 8.1. Apple's own NTP support (Date & Time "Use a Network
Time Server") only arrived with Mac OS 8.5, which needs a PowerPC, so 68k Macs never had any.

It asks an NTP server (default `time.metas.ch`) for the time, sets the clock to the next whole
second (about 1/60 s accuracy), and quits.

## Install

1. Copy `NTPSet` to the Mac. `build/NTPSet.bin` is MacBinary; `build/NTPSet.dsk` is a small HFS
   disk image for emulators or floppy tools.
2. Put it (or an alias to it) into **System Folder:Startup Items**.
3. In Date & Time, leave **Daylight Saving Time** unticked. NTPSet applies EU summer time itself.
4. Outside Central Europe, set your zone (and turn the EU rule off if it doesn't apply) in the
   settings below.

It runs silently at every boot. For the result:

- **Hold Option** while it starts (or while double-clicking it) to get an alert, for example
  `Clock set to 2026-11-25 08:12:34 (UTC+01:00) from time.metas.ch, was off by +3 s.`
- **The last run's result** is always in `System Folder:Preferences:NTPSet Status` (SimpleText).

## Settings

Resource `STR# 128`, which you can edit with ResEdit:

| # | Setting | Default |
|---|---------|---------|
| 1 | Server hostname or IP | `time.metas.ch` |
| 2 | Zone in minutes east of UTC, or `auto` for the Map / Date & Time offset | `60` |
| 3 | Summer-time rule: `EU` or `none` | `EU` |
| 4 | Timeout in seconds (one query per second) | `10` |

The zone defaults to a fixed 60 minutes (Central European Time) on purpose. With `auto`, NTPSet reads the Map control
panel's GMT offset, assumes it is *standard* time, and adds EU summer time on top.

## Network

It uses the MacTCP UDP driver API (`.IPP`) and the MacTCP name resolver (the `dnrp` resource in the
MacTCP or TCP/IP control panel). That works with real MacTCP (System 7.0–7.5) and with Open
Transport's MacTCP compatibility (7.5.x/7.6/8.x), so one binary covers all of them. If TCP/IP isn't
up yet, NTPSet keeps trying for the timeout, then leaves the clock alone. Hostnames need a DNS
server in the MacTCP / TCP/IP control panel.

## Tested

- On a real Quadra 700 with Open Transport (with an IP address; the hostname lookup is emulator-tested so far).
- In BasiliskII (68040 emulation, slirp networking), with the emulated clock set 3 days wrong, on
  Mac OS 8.1 (manual IP) and System 7.5.5 (DHCP), both with Open Transport, resolving `time.metas.ch`:
  `Clock set to 2026-11-25 19:41:15 (UTC+01:00) from time.metas.ch, was 2026-11-28 18:41:14.`

Not yet tested: real MacTCP (System 7.0–7.5 without OT), the Option-key alert, and the `auto` zone.
If something fails, the status file carries diagnostics: the Mac's own IP, packets sent and
received, and the last UDP error codes.

## Build (Linux host, Docker)

```
docker build -t retro68-native -f mac/Dockerfile.retro68 mac   # once, ~10 min on 8 cores
mac/build.sh                                                    # -> mac/build/NTPSet.{bin,dsk,APPL}
```

The official `ghcr.io/autc04/retro68` image is x86_64-only. Its C++ tools fail under qemu on
16K-page kernels (Asahi Linux), so `Dockerfile.retro68` builds Retro68 natively (68k only).

Files: `ntpset.c` (MacTCP UDP, XPRAM location, SetDateTime, status file and alert), `ntpset.r`
(settings, alert, SIZE), `../common/timecalc.c` (calendar/DST math shared with the Amiga version,
unit-tested in `../common/test`).

Public domain (CC0 1.0).
