/* Southern AMIS Projects: standalone raw N: ATASCII terminal. */
#include <atari.h>
#include <conio.h>
#include <peekpoke.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include "directory.h"
#define WIDTH 40
#define HEIGHT 24
#define CELLS (WIDTH * HEIGHT)
extern void cold_start(void);
static char directory_url[192] = DIRECTORY_HTTPS_URL;
static char net_uri[256];
static const char *notice = "No list URL set";
static bool fallback_used;
static unsigned char initial_sound;
/* Keep status before closing the channel. */
static unsigned char last_result, last_device_status, last_connected, last_sio_status;
static unsigned int last_waiting;
static unsigned char terminal_dlist[32];
/* Short reads keep the keyboard responsive. */
static unsigned char rx[64];
/* Read the byte count already reported by status. */
/* Unit status avoids the URI parser clobbering the error pointer in lib 4.11.2. */
extern unsigned char network_status_unit(unsigned char unit,
    unsigned int *waiting, unsigned char *connected, unsigned char *err);
extern unsigned char sio_read(unsigned char unit, void *buffer,
                              unsigned int length);
static unsigned char tabs[WIDTH];
static unsigned char cx, cy;
static bool escaped;
static bool cursor_shown;
static unsigned int cursor_position;
static unsigned char cursor_value;
extern unsigned char bbs_getkey(void);
static unsigned char * const pixels = (unsigned char *)0x7400;

static void hide_cursor(void)
{
    if (cursor_shown) pixels[cursor_position] = cursor_value;
    cursor_shown = false;
}

static void show_cursor(void)
{
    if (!(OS.rtclok[2] & 32))
    {
        hide_cursor();
        return;
    }
    if (!cursor_shown)
    {
        cursor_position = (unsigned int)cy * WIDTH + cx;
        cursor_value = pixels[cursor_position];
        pixels[cursor_position] ^= 0x80;
        cursor_shown = true;
    }
}

static unsigned char screen_code(unsigned char c)
{
    unsigned char inverse = c & 0x80;
    c &= 0x7f;
    if (c < 32) c += 64;
    else if (c < 96) c -= 32;
    return c | inverse;
}

static void clear_terminal(void)
{
    memset(pixels, 0, CELLS);
    cx = cy = 0;
}

static void next_line(void)
{
    cx = 0;
    if (++cy == HEIGHT)
    {
        memmove(pixels, pixels + WIDTH, CELLS - WIDTH);
        memset(pixels + CELLS - WIDTH, 0, WIDTH);
        cy = HEIGHT - 1;
    }
}

/* Escaped controls are displayed as glyphs. */
static void terminal_byte(unsigned char c)
{
    unsigned int row = (unsigned int)cy * WIDTH;
    unsigned char next;
    if (escaped)
        escaped = false;
    else
    {
        switch (c)
        {
        case 27: escaped = true; return;
        case 28: if (cy) --cy; return;
        case 29: if (cy < HEIGHT - 1) ++cy; return;
        case 30: if (cx) --cx; return;
        case 31: if (cx < WIDTH - 1) ++cx; return;
        case 125: clear_terminal(); return;
        case 126:
            if (cx) --cx;
            else if (cy) { --cy; cx = WIDTH - 1; }
            pixels[(unsigned int)cy * WIDTH + cx] = 0;
            return;
        case 127: /* Tab to the next eight-column stop. */
            next = cx + 1;
            while (next < WIDTH && !tabs[next]) ++next;
            if (next == WIDTH) next_line(); else cx = next;
            return;
        case 155: next_line(); return;
        case 156: /* Delete line. */
            memmove(pixels + row, pixels + row + WIDTH,
                    CELLS - row - WIDTH);
            memset(pixels + CELLS - WIDTH, 0, WIDTH);
            cx = 0; return;
        case 157: /* Insert line. */
            memmove(pixels + row + WIDTH, pixels + row,
                    CELLS - row - WIDTH);
            memset(pixels + row, 0, WIDTH);
            cx = 0; return;
        case 158: tabs[cx] = 0; return;
        case 159: tabs[cx] = 1; return;
        case 253: return; /* Silent bell. */
        case 254:
            memmove(pixels + row + cx, pixels + row + cx + 1,
                    WIDTH - cx - 1);
            pixels[row + WIDTH - 1] = 0; return;
        case 255:
            memmove(pixels + row + cx + 1, pixels + row + cx,
                    WIDTH - cx - 1);
            pixels[row + cx] = 0; return;
        }
    }
    pixels[row + cx] = screen_code(c);
    if (++cx == WIDTH) next_line();
}

static void terminal_text(const char *s)
{
    while (*s) terminal_byte((unsigned char)*s++);
}

static void terminal_screen(void)
{
    unsigned char i;
    OS.sdmctl = 0;
    /* Three blank lines, 24 ANTIC mode-2 rows, jump back to this list. */
    memset(terminal_dlist, 2, sizeof(terminal_dlist));
    terminal_dlist[0] = terminal_dlist[1] = terminal_dlist[2] = 0x70;
    terminal_dlist[3] = 0x42;
    terminal_dlist[4] = 0x7400 & 255;
    terminal_dlist[5] = 0x7400 >> 8;
    terminal_dlist[29] = 0x41;
    terminal_dlist[30] = (unsigned int)terminal_dlist & 255;
    terminal_dlist[31] = (unsigned int)terminal_dlist >> 8;
    OS.sdlst = terminal_dlist;
    OS.savmsc = pixels;
    OS.rowcrs = OS.colcrs = OS.dindex = 0;
    OS.crsinh = 1;
    OS.chbas = 0xe0; /* Standard OS ATASCII character set. */
    OS.color1 = 0x0e;
    OS.color2 = OS.color4 = 0x90;
    POKE(0xd01d, 0); /* Disable player/missile graphics. */
    clear_terminal();
    escaped = cursor_shown = false;
    for (i = 0; i < WIDTH; ++i) tabs[i] = (i && !(i & 7));
    OS.sdmctl = 0x22;
}

static bool exit_pressed(void)
{
    return (PEEK(0xd01f) & 4) == 0; /* OPTION is never sent to the BBS. */
}

static bool option(void) { return exit_pressed(); }
static unsigned char choice(void)
{
    while (kbhit()) bbs_getkey();
    while (option()) {}
    while (!kbhit()) {}
    return bbs_getkey();
}
static void return_config(void)
{
    clrscr(); cputs("Returning to firmware CONFIG...");
    /* Mode zero inserts the firmware's CONFIG boot disk; enable its overlay. */
    if (!fuji_set_boot_mode(0) || !fuji_set_boot_config(1))
    { notice = "CONFIG boot failed; check FujiNet"; return; }
    OS.soundr = initial_sound;
    cold_start();
}
static bool edit_url(void)
{
    unsigned int length = 0;
    unsigned char c;
    char candidate[192];
    clrscr(); cputs("HTTPS List URL\r\nReturn clears the URL. Esc cancels.\r\n");
    while (true)
    {
        while (!kbhit()) {}
        c = bbs_getkey();
        if (c == 155) break;
        if (c == 27) return false;
        if (c == 126)
        {
            if (length)
            {
                --length;
                gotoxy(length % WIDTH, 2 + length / WIDTH);
                cputc(' ');
                gotoxy(length % WIDTH, 2 + length / WIDTH);
            }
        }
        else if (c >= 32 && c <= 126 && length < 180)
        { candidate[length++] = c; cputc(c); }
    }
    candidate[length] = 0;
    if (length && !directory_https_url(candidate))
    { notice = "URL must be https://host/path"; return false; }
    strcpy(directory_url, candidate);
    return true;
}
static void load_directory(void)
{
    unsigned int waiting, count, i, total = 0;
    unsigned char connected, err, status, last, now;
    unsigned int idle_frames = 0;
    bool opened = false, ok = false;
    directory_reset(); fallback_used = true;
    if (!directory_url[0]) { notice = "No list URL - Gateway available"; goto done; }
    if (!directory_https_url(directory_url)) { notice = "Invalid URL - Gateway available"; goto done; }
    strcpy(net_uri, "N1:"); strcat(net_uri, directory_url);
    clrscr(); cputs("Loading BBS list...\r\nOption: Cancel\r\n");
    notice = "List unavailable - Gateway available";
    OS.soundr=0;
    if (network_open(net_uri, 4, 0) != FN_ERR_OK) goto close;
    opened = true; last = OS.rtclok[2];
    while (!option())
    {
        now = OS.rtclok[2];
        if ((unsigned char)(now-last) < 3) continue;
        idle_frames += (unsigned char)(now-last); last = now;
        if (idle_frames > 1800) { notice = "List timed out - Gateway available"; break; }
        OS.soundr=0;
        status = network_status_unit(1, &waiting, &connected, &err);
        if (status != FN_ERR_OK) break;
        if (waiting)
        {
            count = waiting < sizeof(rx) ? waiting : sizeof(rx);
            if (total > 16384-count) { notice = "List too large - Gateway available"; break; }
            OS.soundr=0;
            if (sio_read(1, rx, count) != FN_ERR_OK) break;
            for (i=0; i<count; ++i) directory_feed(rx[i]);
            total += count; idle_frames = 0;
        }
        else if (err == 136 || (!connected && (err == 0 || err == 1))) { ok = true; break; }
        else if (err >= 128) break;
    }
close:
    OS.soundr=0;
    status = network_close(net_uri);
    if (opened && ok && status == FN_ERR_OK)
    {
        directory_finish();
        if (board_count)
        { fallback_used = false; notice = rejected_lines ? "List loaded; some entries skipped" : "BBS list loaded"; return; }
        notice = "List empty - Gateway available";
    }
done:
    directory_fallback();
    while (option()) {}
}
static void heading(void)
{
    clrscr();
    gotoxy(12,0); cputs("FujiNet Netcat");
    gotoxy(11,1); cputs("Atari BBS Terminal");
    gotoxy(0,3); cputs("Southern AMIS Projects");
}
static void draw_list(unsigned char page)
{
    unsigned char i, first = page * PAGE_SIZE;
    heading();
    gotoxy(0,4); cprintf("BBS Directory                  Page %u/%u",page+1,(board_count+PAGE_SIZE-1)/PAGE_SIZE);
    gotoxy(0,5); cputs(notice);
    for (i=0; i<PAGE_SIZE && first+i<board_count; ++i)
    {
        gotoxy(2,7+i); cprintf("%u. %s", i+1, boards[first+i].name);
    }
    gotoxy(0,8+i); cputs("1-8 Call   N Next   P Previous");
    gotoxy(0,9+i); cputs("R Reload   U List URL   C Config");
}
static const char *session(unsigned char index)
{
    unsigned char key, connected=0, err=0, last, now, poll_frames=3;
    unsigned int waiting = 0, count, i;
    const char *message = "Disconnected";
    unsigned char result;
    last_result=last_device_status=last_connected=last_sio_status=0;
    last_waiting=0;
    sprintf(net_uri,"N1:TCP://%s:%u",boards[index].host,boards[index].port);
    terminal_screen(); terminal_text(fallback_used ? "Connecting to Gateway relay...\x9bOption: Disconnect\x9b" : "Connecting to selected BBS...\x9bOption: Disconnect\x9b");
    while (option()) {}
    OS.soundr=0;
    result=network_open(net_uri,12,0);
    last_result=result; last_sio_status=PEEK(0x0303); last_device_status=fn_device_error;
    if (result != FN_ERR_OK) { message="Connection failed"; goto finish; }
    clear_terminal(); last=OS.rtclok[2]-3;
    while (true)
    {
        if (option()) { message="Connection closed"; break; }
        if (kbhit())
        {
            key=bbs_getkey();
            OS.soundr=0;
            result=network_write(net_uri,&key,1);
            if (result != FN_ERR_OK) { last_result=result;last_device_status=fn_device_error;last_sio_status=PEEK(0x0303);message="Write failed";break; }
            poll_frames=3; last=OS.rtclok[2]-3;
        }
        now=OS.rtclok[2];
        if (!waiting && (unsigned char)(now-last)>=poll_frames)
        {
            last=now;
            OS.soundr=0;
            result=network_status_unit(1,&waiting,&connected,&err);
            last_result=result;last_device_status=err;last_connected=connected;last_waiting=waiting;last_sio_status=PEEK(0x0303);
            if (result != FN_ERR_OK) { message="Status failed"; break; }
            if (!waiting)
            {
                poll_frames=12;
                if (!connected || err==136) break;
                /* Atari error statuses have bit 7 set. */
                if (err>=128) { message="Network error"; break; }
            }
        }
        if (waiting)
        {
            count=waiting<sizeof(rx)?waiting:sizeof(rx);
            OS.soundr=0;
            result=sio_read(1,rx,count);
            if (result!=FN_ERR_OK) { last_result=result;last_device_status=fn_device_error;last_sio_status=PEEK(0x0303);message="Read failed";break; }
            waiting-=count; hide_cursor();
            for(i=0;i<count;++i) terminal_byte(rx[i]);
            if(!waiting) { poll_frames=3; last=OS.rtclok[2]-3; }
        }
        show_cursor();
    }
finish:
    hide_cursor();
    OS.soundr=0;
    result=network_close(net_uri);
    if (result!=FN_ERR_OK) { message="Connection closed; cleanup failed";last_result=result;last_sio_status=PEEK(0x0303); }
    while (option()) {}
    return message;
}
int main(void)
{
    unsigned char key, page=0, selected, saved_font;
    unsigned char *saved_dlist, *saved_pixels;
    unsigned char saved_colors[3];
    const char *message;
    initial_sound=OS.soundr; OS.soundr=0;
    OS.soundr=0;
    if(network_init()!=FN_ERR_OK)
    { directory_fallback(); fallback_used=true; notice="FujiNet network unavailable"; }
    else load_directory();
    while(true)
    {
        draw_list(page); key=choice();
        if(key>='A' && key<='Z') key+=32;
        if(key=='c') return_config();
        else if(key=='u') { if (edit_url()) { load_directory(); page=0; } }
        else if(key=='r') { load_directory(); page=0; }
        else if(key=='n' && (page+1)*PAGE_SIZE<board_count) ++page;
        else if(key=='p' && page) --page;
        else if(key>='1' && key<='8')
        {
            selected=page*PAGE_SIZE+key-'1';
            if(selected>=board_count) continue;
            /* Restore the normal OS display before drawing local menus. */
            saved_dlist=OS.sdlst; saved_pixels=OS.savmsc; saved_font=OS.chbas;
            saved_colors[0]=OS.color1;saved_colors[1]=OS.color2;saved_colors[2]=OS.color4;
            do
            {
                message=session(selected);
                OS.sdmctl=0; OS.sdlst=saved_dlist; OS.savmsc=saved_pixels;OS.chbas=saved_font;
                OS.dindex=0; OS.rowcrs=OS.colcrs=0;OS.crsinh=1;
                OS.color1=saved_colors[0];OS.color2=saved_colors[1];OS.color4=saved_colors[2];OS.sdmctl=0x22;
                heading();
                gotoxy(2,5); cputs(boards[selected].name);
                gotoxy(2,6); cputs(message);
                if (last_result || (last_device_status>=128 && last_device_status!=136))
                {
                    gotoxy(0,8); cprintf("Library %u  SIO %u  Device %u",last_result,last_sio_status,last_device_status);
                    gotoxy(0,9); cprintf("Connected %u  Waiting %u",last_connected,last_waiting);
                    gotoxy(0,11); cputs(net_uri);
                }
                gotoxy(2,16); cputs("\x14 L  Return to BBS List");
                gotoxy(2,17); cputs("\x14 R  Reconnect");
                gotoxy(2,18); cputs("\x14 C  Return to Config");
                do { key=choice(); if(key>='A'&&key<='Z') key+=32; } while(key!='l'&&key!='r'&&key!='c');
                if(key=='c') { return_config(); key='l'; }
            } while(key=='r');
        }
    }
}
