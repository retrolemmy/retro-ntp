# NTPSET.EXE (Windows 95/98/ME)

Tiny SNTP clock setter for Windows 9x, which only has `net time` against a Windows server and no
NTP client. It asks an NTP server (default `time.metas.ch`) for the time, sets the clock, and
quits. No window, no install, no C runtime, about 5 KB.

## Install

1. Copy `NTPSET.EXE` to `C:\WINDOWS`.
2. Test it: **Start > Run > `NTPSET /V`** shows a box with the result.
3. Put a shortcut to `C:\WINDOWS\NTPSET.EXE` into
   `C:\WINDOWS\Start Menu\Programs\StartUp`.

## Usage

```
NTPSET [/V] [server]
```

- `server`: a hostname or an IP. Default `time.metas.ch`.
- `/V`: show a message box with the result. Without it NTPSET is silent.

It sends at most 3 queries, 1 s apart, then gives up and leaves the clock alone. Exit code `0` means
the clock was set.

The time is set as UTC; Windows applies the time zone and DST from Control Panel > Date/Time, so
nothing else needs configuring.

## Build

```
./build.sh    # needs i686-w64-mingw32-gcc; links only kernel32, user32 and wsock32
```

Public domain (CC0 1.0).
