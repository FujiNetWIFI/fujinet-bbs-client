# FujiNet Atari BBS Client

Southern AMIS Projects · FujiNet N: Device Client Version 4.0

A 40-column ATASCII terminal for Atari 8-bit computers and FujiNet. Browse the online BBS directory, connect directly to a board, use saved macros, and review recent output without leaving the connection.

Run

Launch `ataribbs.xex` from FujiNet CONFIG. The current version is
available on `tnfs.fujinet.online`. Add that server to your FujiNet
TNFS host list.

For FujiNet Go / Atari800, open the FujiNet web interface and under
Boot Settings set “Enable SIO STATUS wait routine” to “No”, then Save.

The directory uses `https://fujinet.online/bbs/list/atari8/atascii`.
If it cannot be loaded, the client offers the Atari BBS Gateway relay.
Up to 32 boards are supported, with 14 entries per page.

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

Requires GNU Make, cc65, curl and unzip. FujiNet-lib defaults to version 5.0.0.
The Makefile uses `cl65` from `PATH` to compile and link the Atari client:

```sh
make
```

The executable is `build/atari/ataribbs.xex`. Object files, dependencies,
linker map and labels also go in `build/atari/`. The entire `build/` directory
is ignored by Git. Use `make -j` for a parallel build and `make clean` to
remove the build outputs.

The build automatically downloads and extracts the selected Atari FujiNet-lib
release into `_cache/fujinet-lib/<version>-atari` when its library or required
headers are missing. Subsequent builds reuse that cache without network access.
Change `FUJINET_LIB_VERSION` at the top of the Makefile or on the command line:

```sh
make FUJINET_LIB_VERSION=5.0.0
```

Set `FNLIB_CACHE` to relocate the managed cache. To use an existing extracted
Atari release, set `FNLIB` to its directory of headers and `.lib` files.
The library filename is `fujinet-atari-<version>.lib`; override `FNLIB_LIB`
for another filename. Explicit `FNLIB` or `FNLIB_LIB` overrides disable automatic
downloads so local libraries are never overwritten. Switching versions or paths
rebuilds the C objects. FujiNet-lib 4.x does not provide the API used by this client.

cc65 normally locates its own headers and target libraries. For installations
that cannot, the Makefile checks `/usr/share/cc65` and `/usr/local/share/cc65`.
Override `CL65` and `CC65_HOME` for other installations; `CC65_HOME` is the
shared-data directory containing `include/`, `lib/`, `asminc/` and `cfg/`:

```sh
make CL65=/opt/cc65/bin/cl65 CC65_HOME=/opt/cc65/share/cc65
make FNLIB=/path/to/fujinet-lib
```

These settings can be edited at the top of the Makefile, supplied in the
environment, or passed on the make command line. `CPPFLAGS`, `CFLAGS`,
`ASFLAGS`, `LDFLAGS`, `LDLIBS`, `BUILD_DIR` and `TARGET` can also be overridden.

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
