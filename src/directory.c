/* Southern AMIS Projects. Bounded, streamed ASCII directory parser. */
#include <string.h>
#include "directory.h"
Board boards[MAX_BOARDS];
unsigned char board_count;
unsigned int rejected_lines;
static char line[192];
static unsigned int used;
static bool overlong;
static void parse_line(void)
{
    char *a, *b, *p;
    unsigned int port = 0, digit;
    Board *entry;
    if (overlong) { ++rejected_lines; return; }
    line[used] = 0;
    if (!used || line[0] == '#') return;
    if (board_count == MAX_BOARDS) { ++rejected_lines; return; }
    a = strchr(line, '|');
    if (!a) { ++rejected_lines; return; }
    *a++ = 0; b = strchr(a, '|');
    if (!b) { ++rejected_lines; return; }
    *b++ = 0;
    if (!line[0] || strlen(line) > 31 || !a[0] || strlen(a) > 95 || !*b)
        goto bad;
    for (p = line; *p; ++p) if ((unsigned char)*p < 32 || (unsigned char)*p > 126) goto bad;
    for (p = a; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) goto bad;
    for (p = b; *p; ++p)
    {
        if (*p < '0' || *p > '9') goto bad;
        digit = *p - '0';
        if (port > 6553 || (port == 6553 && digit > 5)) goto bad;
        port = port * 10 + digit;
    }
    if (!port) goto bad;
    entry = &boards[board_count++];
    strcpy(entry->name, line); strcpy(entry->host, a); entry->port = port;
    return;
bad: ++rejected_lines;
}
void directory_reset(void) { board_count = 0; rejected_lines = 0; used = 0; overlong = false; }
void directory_feed(unsigned char c)
{
    if (c == 13 || c == 10 || c == 155)
    { parse_line(); used = 0; overlong = false; }
    else if (used < sizeof(line)-1) line[used++] = c ? c : 1;
    else overlong = true;
}
void directory_finish(void) { if (used || overlong) parse_line(); used = 0; overlong = false; }
void directory_fallback(void)
{
    directory_reset(); strcpy(boards[0].name, FALLBACK_NAME);
    strcpy(boards[0].host, FALLBACK_HOST); boards[0].port = FALLBACK_PORT; board_count = 1;
}
bool directory_https_url(const char *url)
{
    const char *p;
    if (strncmp(url, "https://", 8) || !url[8] || url[8] == '/' || strlen(url) > 180) return false;
    for (p=url+8; *p; ++p) if ((unsigned char)*p <= 32 || (unsigned char)*p > 126 || *p == '@') return false;
    return true;
}
