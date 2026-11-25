# NTPSet (Amiga)

Tiny SNTP clock setter for AmigaOS 2.04+ on any 680x0 with a `bsdsocket.library` stack
(Roadshow; AmiTCP and Miami should work too). It asks an NTP server (default `time.metas.ch`)
for the time, sets the system clock, and writes the battery clock if there is one. Tested with
Roadshow on a real Amiga.

## Install

1. Copy `NTPSet` to `C:` on the Amiga.
2. Add it to `S:User-Startup`, **after** the network is started (for example after
   `AddNetInterface` / Roadshow's `S:Network-Startup`):

   ```
   C:NTPSet QUIET
   ```

   On machines with no RTC or a flat battery the clock is right from the first boot onwards.

## Usage

```
NTPSet [SERVER <ip|host>] [ZONE <min>] [NODST] [NORTC] [TIMEOUT <s>] [TEST] [QUIET]
```

| Option    | Meaning |
|-----------|---------|
| `SERVER`  | NTP server, as an IP or a hostname. Default `time.metas.ch`. |
| `ZONE`    | Time zone in minutes **east** of UTC (Central Europe: `60`, UK: `0`). Default: taken from Locale prefs. |
| `NODST`   | Don't apply EU summer time. Use it outside the EU, or with `ZONE=0` to keep the Amiga on plain UTC. |
| `NORTC`   | Don't write the battery-backed clock. |
| `TIMEOUT` | Seconds to keep trying (one query per second). Default 5. Covers a stack that is still bringing up DHCP when User-Startup runs. |
| `TEST`    | Query and print only; the clock is left alone. |
| `QUIET`   | No output at all. |

Example: `NTPSet TEST` prints
`NTPSet: server time 2026-11-25 08:12:33 (UTC+01:00) from 195.176.26.206, was off by +3.4 s`.

Return codes: `0` = clock set, `5` (WARN) = no answer, no network, or the server refused
(the clock is left alone, and User-Startup carries on), `10` = bad arguments or an implausible time, `20` = no timer.device.
Ctrl-C aborts. Started from Workbench, it runs silently with the defaults.

## Time zone notes

- AmigaOS keeps **local** time and has no DST support of its own. NTPSet adds the zone offset,
  plus EU summer time (last Sunday of March to last Sunday of October, 01:00 UTC), and does that
  on every boot.
- The zone comes from Locale prefs (`loc_GMTOffset`): pick your zone in `Prefs/Locale` and **Save**.
  Or pass `ZONE=` explicitly. The `TEST` output shows the offset NTPSet used.
- Don't also run a DST shifter (such as `SetDST`) or a second time-sync tool, or the clock gets
  shifted twice.

## Build (Linux host)

```
./setup-toolchain.sh   # once: vbcc/vasm/vlink + NDK 3.2 into ./toolchain (interactive dtgen step)
./build.sh             # -> ./NTPSet, and runs the host unit test of the time math
```

Files: `ntpset.c` (Amiga side: ReadArgs, timer.device, bsdsocket, battclock), `start.s` (minimal
startup with no C library), `../common/timecalc.c` (calendar/DST/NTP math shared with the Mac
version, tested on the host against glibc in `../common/test`).

Public domain (CC0 1.0).
