# FujiNet Atari BBS Client

Southern AMIS Projects · FujiNet N: Device Client Version 4.0

A 40-column ATASCII terminal for Atari 8-bit computers and FujiNet. Browse the online BBS directory, connect directly to a board, use saved macros, and review recent output without leaving the connection.

Run

Launch `ataribbs.xex` from FujiNet CONFIG. The release file is intended for
`tnfs.fujinet.online`, subject to FujiNet team approval.

The directory uses `https://fujinet.online/bbs/list/atari8/atascii`. If it cannot be loaded, the client offers the Atari BBS Gateway relay. Up to 32 boards are supported, with 14 entries per page.

Keys

| Screen | Controls |
| --- | --- |
| Directory | Enter a BBS number, then Return to connect. Arrows or `<` / `>` select a board. |
| Directory | N Next page, P Previous page, R Reload, U List URL, M Macros, H Help, C CONFIG. |
| Connected | SELECT opens local controls. OPTION disconnects. |
| Local controls | 1–4 sends a saved macro, S opens scrollback, H opens Help, 0 cancels a running macro, Esc returns to the live screen. |
| Scrollback | Up/down moves one line, left/right moves one page, Esc returns to the live screen. |
| Disconnected | L returns to the directory, R reconnects, C returns to firmware CONFIG. |

Up/down selection moves the highlight without redrawing the BBS list. Changing pages refreshes the list.

Shortcut labels appear in inverse; press ordinary letter keys. Directory and scrollback navigation also accept `-` or `_` for up, `=` for down, `+` for left and `*` for right. These aliases do not alter characters sent to the BBS.

Before connecting, a five-second notice shows the local controls; OPTION cancels it. At disconnect, the last BBS screen is retained briefly with a status message. The BREAK key is disabled while the client runs. Returning to CONFIG boots FujiNet's firmware CONFIG.

Macros

M in the directory opens the macro editor. Four macros of up to 63 characters are stored on the FujiNet SD card. During a connection, use SELECT and then 1–4 to send one.

- `\r` sends ATASCII Return.
- `\p` pauses for about one second.
- `\\` sends a backslash.

Typing cancels an active macro. Receiving continues while macros run and while local controls or scrollback are displayed. Scrollback retains the last 64 rendered rows in RAM.

Macros are stored as plain text in `/FujiNet/53414201.key` through `/FujiNet/53414204.key`. Treat files containing passwords as private. Missing files are normal before the first save. 

Build

Requires cc65 and FujiNet-lib 5.0.1. The dependency is pinned in the Makefile.
The source bundle includes the matching library headers, source and Atari archive.

```sh
make release-xex
```

Transport

Session status/read commands use one continuous receive interrupt manager for
ACK, completion, payload and checksum. The receiver is set before COMMAND
release and reports success only after a complete valid frame. Open/write/close
use ROM SIO; directory and SD operations use FujiNet-lib. Each session read
requires fresh status. Read size is set at 256 bytes and keyboard batches
at 64 bytes; failed reads are not rendered or automatically replayed.


The layout fits 48K RAM: executable starts at $1200, BSS ends below $7400,
screen is $7400-$77BF, display list $7800-$781F, transport $7820-$7FFF,
and scrollback $8000-$89FF. A 2 KiB C stack remains below the existing
PicoBoot MEMTOP of $9C1F.

This is a raw TCP ATASCII client. Southern AMIS Projects.
