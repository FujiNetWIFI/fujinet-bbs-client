FujiNet Netcat - Atari BBS Terminal

Southern AMIS Projects

A standalone ATASCII client for Atari and FujiNet. It follows the original ataribbs.xex raw N: TCP approach and adds an HTTPS directory, direct connections, and a local menu after disconnect. This is new source, and not a patch to the original XEX.

Running it

Mount ataribbs-directory.xex or boot ataribbs-directory.atr. This package does not change the hosted client or CONFIG launcher.

The directory URL is currently not set as awaiting for Mozz to provide a URL. Until the list URL is available, the program a default if no list for the Atari BBS Gateway. Set the URL with U for the current run, or set DIRECTORY_HTTPS_URL in source/src/settings.h and rebuild. A downloaded list replaces that entry; each selection connects to the listed host and port.

Keys

1-8 calls a listed board. N/P changes pages. R reloads the list. U changes the HTTPS list URL. C boots Config. During a connection, Option disconnects.

After disconnect: L returns to the list, R reconnects, C boots Config. C starts FujiNet's firmware Config; it does not restore a custom Config ATR previously loaded from SD. Connection errors show status details for troubleshooting.

List format

Plain text, one entry per line:

Name|hostname|port

Comments start with #. Maximum 32 boards, names up to 31 characters, hosts up to 95 characters, ports 1-65535. Hostnames and IPv4 are supported. Downloads are limited to 16 KB. JSON, IPv6 and Telnet negotiation are not implemented. See list-example.txt and list-empty.txt.

Building

From source/, with cc65 and fujinet-lib for Atari installed:

make CC65_HOME=/path/to/cc65 LIBDIR=/path/to/fujinet-lib/4.11.2-atari

The library is not a bundle. HTTPS is handled by FujiNet firmware. The official URL and list format still need agreement with the maintainers.

