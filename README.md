# FujiNet Atari BBS Client

Southern AMIS Projects · Version 2

A 40-column ATASCII terminal for Atari 8-bit computers and FujiNet. Browse the online BBS directory, connect directly to a board, use saved macros, and review recent output without leaving the connection.

Run

Pressing G from CONFIG - Launch ataribbs.xex from nfs.fujinet.online

The directory uses `https://fujinet.online/bbs/list/atari8/atascii`. If it cannot be loaded, the client offers the Atari BBS Gateway relay. Up to 32 boards are supported, with 14 entries per page.

Keys

Screen Help 

| Directory | Enter a BBS number, then Return to connect. Arrows or `<` / `>` select a board. |
| Directory | N Next page, P Previous page, R Reload, U List URL, M Macros, H Help, C CONFIG. |
| Connected | SELECT opens local controls. OPTION disconnects. |
| Local controls | 1–4 sends a saved macro, S opens scrollback, H opens Help, 0 cancels a running macro, Esc returns to the live screen. |
| Scrollback | Up/down moves one line, left/right moves one page, Esc returns to the live screen. |
| Disconnected | L returns to the directory, R reconnects, C returns to firmware CONFIG. |

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

Require a current cc65 and the Atari FujiNet-lib 4.11.2 header and library. So this is set FNLIB to the directory containing `fujinet-network.h`, `fujinet-fuji.h`, and `fujinet-atari-4.11.2.lib`.

```sh
FNLIB=/path/to/fujinet-lib CL65=/path/to/cl65 make
```
## Tests

BBS Users tested and passed

Tests cover terminal control handling, scrolling, macro storage and playback, directory UI, session scheduling, PAL/NTSC timing, and keyboard translation. 

Some notes

The keyboard reader captures the OS key latch once and translates it locally, avoiding the blocking K: Standard Atari mappings, Caps/Control-lock, Inverse and a light console-speaker keyclick are provided. 

The program layout fits 48K RAM: executable starts at $1200, initialized data ends below $5700, normal BSS ends below $7400, screen is at $7400, display list at $7800, and scrollback at $8000–$89FF. 

This is a raw TCP ATASCII client. Southern Amis Projects
