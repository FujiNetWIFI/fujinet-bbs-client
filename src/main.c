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
#include "enhancements.h"
#define WIDTH 40
#define HEIGHT 24
#define CELLS (WIDTH * HEIGHT)
extern void cold_start(void);
static char directory_url[192] = DIRECTORY_HTTPS_URL;
static char net_uri[256];
static const char *notice = "No list URL set";
static bool fallback_used;
static unsigned char initial_sound, initial_break;
/* Keep status before closing the channel. */
static unsigned char last_result, last_device_status, last_connected, last_sio_status;
static unsigned int last_waiting;
static bool cleanup_needed;
static unsigned char cleanup_result;
/* Keep ANTIC data below the bank-switchable $8000-$BFFF region. */
static unsigned char * const terminal_dlist=(unsigned char *)0x7800;
/* Small synchronous reads limit visible pauses while SIO owns the CPU. */
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
static unsigned char *pixels = (unsigned char *)0x7400;
static unsigned char live_screen[CELLS];
static unsigned char panel, history_offset;
static unsigned char macro_active, macro_position, macro_pause, macro_clock;
static bool select_held;
static void panel_draw(void);


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
    unsigned char y;
    for(y=0;y<HEIGHT;++y)history_add(pixels+y*WIDTH);
    if(panel==2)history_offset=history_count();
    memset(pixels, 0, CELLS);
    cx = cy = 0;
}

static void next_line(void)
{
    cx = 0;
    if (++cy == HEIGHT)
    {
        history_add(pixels);
        if(panel==2 && history_offset<HISTORY_LINES)++history_offset;
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

/* Render text directly; handle ATASCII controls separately. */
static void render_bytes(const unsigned char *data, unsigned int length)
{
    unsigned char c;
    unsigned char *dest=pixels+(unsigned int)cy*WIDTH+cx;
    while(length--)
    {
        c=*data++;
        if(!escaped && ((c>=32 && c<125) || (c>=160 && c<253)))
        {
            if((c&127)<96)c-=32;
            *dest++=c;
            if(++cx==WIDTH)
            {
                next_line();
                dest=pixels+(unsigned int)cy*WIDTH;
            }
        }
        else
        {
            terminal_byte(c);
            dest=pixels+(unsigned int)cy*WIDTH+cx;
        }
    }
}

static void terminal_text(const char *s)
{
    while (*s) terminal_byte((unsigned char)*s++);
}

static void terminal_screen(void)
{
    unsigned char i;
    OS.sdmctl = 0;
    POKE(0xd400,0); /* Stop ANTIC while replacing the inherited display. */
    POKE(0x026f,0); /* GPRIOR shadow: select normal text, not GTIA color modes. */
    POKE(0xd01b,0); /* Apply normal GTIA priority immediately. */
    POKE(0xd40e,0x40); /* VBI only; no inherited CONFIG display interrupts. */
    POKE(0x02f3,2); POKE(0xd401,2); /* Normal character rendering. */
    /* Three blank lines, 24 ANTIC mode-2 rows, jump back to this list. */
    memset(terminal_dlist, 2, 32);
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
    pixels=(unsigned char *)0x7400; panel=0;
    history_reset(); macro_active=0; select_held=false;
    clear_terminal(); history_reset();
    escaped = cursor_shown = false;
    for (i = 0; i < WIDTH; ++i) tabs[i] = (i && !(i & 7));
    OS.sdmctl = 0x22;
}

static bool exit_pressed(void)
{
    return (PEEK(0xd01f) & 4) == 0; /* OPTION is never sent to the BBS. */
}

/* Disable only BREAK IRQ; retain keyboard and serial interrupts. */
static void disable_break(void)
{
    POKE(0x10,PEEK(0x10)&0x7f);
    POKE(0xd20e,PEEK(0x10));
    POKE(0x11,255);
}
/* Count frame deltas across the eight-bit clock wrap, on PAL or NTSC. */
static bool five_seconds(bool cancel)
{
    unsigned int remaining=PEEK(0x62)==0?300:250;
    unsigned char last=OS.rtclok[2],now,elapsed;
    while(remaining)
    {
        if(cancel && exit_pressed())return false;
        now=OS.rtclok[2];elapsed=now-last;last=now;
        if(elapsed>=remaining)remaining=0;else remaining-=elapsed;
    }
    return true;
}
static void help_draw(void)
{
    ui_heading("Help / Southern AMIS Projects");
    gotoxy(1,4);cputs("Enter a BBS number, press Return.");
    gotoxy(1,6);cputs("Arrows or <> select; N/P pages.");
    gotoxy(1,8);cputs("In a call, SELECT opens controls.");
    gotoxy(1,9);cputs("1-4 send macros; S opens scrollback.");
    gotoxy(1,11);cputs("Scrollback: up/down, left/right");
    gotoxy(1,12);cputs("pages. Esc returns to the live BBS.");
    gotoxy(1,14);cputs("OPTION cancels / disconnects.");
    gotoxy(1,16);cputs("M in directory edits SD macros.");
    gotoxy(1,17);cputs("Macro: \\r Return, \\p pause, \\\\ slash.");
    gotoxy(1,19);cputs("40-column ATASCII; BREAK disabled.");
    ui_footer("Built by Southern AMIS Projects", "Esc Return");
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
    POKE(0x10,(PEEK(0x10)&0x7f)|initial_break);
    POKE(0xd20e,PEEK(0x10));
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
    ui_heading("Atari BBS directory");
}
static void draw_number(unsigned char number)
{
    /* Clear all 40 cells; keep conio's saved cursor character in sync. */
    gotoxy(0,21);
    memset(OS.savmsc+21*WIDTH,0,WIDTH);
    OS.oldchr=0;
    gotoxy(1,21); revers(0);
    if(number)cprintf("Enter BBS number: %u",number);
    else cputs("Enter BBS number:");
}
static void draw_list(unsigned char page, unsigned char selected, unsigned char number)
{
    unsigned char i, first=page*PAGE_SIZE;
    heading();
    gotoxy(1,3); cprintf("Page %u/%u  /  %u boards",page+1,(board_count+PAGE_SIZE-1)/PAGE_SIZE,board_count);
    gotoxy(1,4); cprintf("%.38s",notice);
    for(i=0;i<PAGE_SIZE && first+i<board_count;++i)
    {
        gotoxy(1,6+i); revers(i==selected);
        cprintf("%2u %-32.32s",first+i+1,boards[first+i].name); revers(0);
    }
    ui_footer("Enter BBS number:", "");
    draw_number(number);
    gotoxy(1,22);
    revers(1); cputc('N'); revers(0); cputs(" Next / ");
    revers(1); cputc('P'); revers(0); cputs(" Previous Page  ");
    revers(1); cputc('H'); revers(0); cputs(" Help");
    gotoxy(1,23);
    revers(1); cputc('R'); revers(0); cputs(" Reload ");
    revers(1); cputc('U'); revers(0); cputs(" URL ");
    revers(1); cputc('M'); revers(0); cputs(" Macros ");
    revers(1); cputc('C'); revers(0); cputs(" Config");
}
static void panel_close(void)
{
    memcpy((void *)0x7400,live_screen,CELLS);
    pixels=(unsigned char *)0x7400; panel=0;
}
static void panel_draw(void)
{
    unsigned char i;
    if(panel==2){ history_draw(live_screen,history_offset); return; }
    if(panel==3){help_draw();return;}
    ui_heading("Local controls / receiving continues");
    gotoxy(1,5); revers(1);cputc('S');revers(0);cputs("  Review scrollback (64 lines)");
    gotoxy(1,7); cputs("1-4 Send saved macro:");
    for(i=0;i<MACRO_COUNT;++i)
    {gotoxy(2,9+i*2);cprintf("%u %.32s",i+1,macros[i][0]?macros[i]:"(empty)");}
    gotoxy(1,18);revers(1);cputc('H');revers(0);cputs(" Help / terminal controls");
    ui_footer("Esc Return   0 Cancel active macro", "OPTION Disconnect / SELECT opens menu");
}
static void panel_open(void)
{
    hide_cursor(); memcpy(live_screen,(void *)0x7400,CELLS);
    pixels=live_screen; panel=1; panel_draw();
}
/* Local aliases only: never translate characters sent to the remote BBS. */
static unsigned char navigation_key(unsigned char key)
{
    if(key=='<' || key=='-' || key=='_')return 28;
    if(key=='>' || key=='=')return 29;
    if(key=='+')return 30;
    if(key=='*')return 31;
    return key;
}
static void panel_key(unsigned char key)
{
    if(key==27){panel_close();return;}
    if(panel==1)
    {
        if(key=='h'||key=='H'){panel=3;panel_draw();}
        else if(key=='s'||key=='S'){panel=2;history_offset=history_count();panel_draw();}
        else if(key=='0'){macro_active=0;panel_close();}
        else if(key>='1'&&key<='4' && macros[key-'1'][0])
        {macro_active=key-'1'+1;macro_position=macro_pause=0;macro_clock=OS.rtclok[2];panel_close();}
    }
    else if(panel==2)
    {
        key=navigation_key(key);
        if(key==28 && history_offset<history_count())++history_offset;
        if(key==29 && history_offset)--history_offset;
        if(key==30)history_offset=history_offset+20>history_count()?history_count():history_offset+20;
        if(key==31)history_offset=history_offset<20?0:history_offset-20;
        panel_draw();
    }
}
/* Send one macro byte per frame, without blocking incoming traffic. */
static unsigned char macro_tick(void)
{
    unsigned char c,now=OS.rtclok[2],elapsed;
    if(!macro_active)return FN_ERR_OK;
    elapsed=now-macro_clock;
    if(!elapsed)return FN_ERR_OK;
    macro_clock=now;
    if(macro_pause){macro_pause=elapsed>=macro_pause?0:macro_pause-elapsed;return FN_ERR_OK;}
    c=macros[macro_active-1][macro_position++];
    if(!c){macro_active=0;return FN_ERR_OK;}
    if(c=='\\')
    {
        c=macros[macro_active-1][macro_position];
        if(c=='r'){++macro_position;c=155;}
        else if(c=='p'){++macro_position;macro_pause=PEEK(0x62)==0?60:50;return FN_ERR_OK;}
        else if(c=='\\')++macro_position;
        else c='\\';
    }
    OS.soundr=0;
    return network_write(net_uri,&c,1);
}
static const char *session(unsigned char index)
{
    unsigned char key, connected=0, err=0, last, now, poll_frames=1;
    unsigned int waiting = 0, count=0;
    const char *message = "Disconnected";
    unsigned char result, review_last=0;
    bool key_deferred=false;
    bool review_dirty=false;
    last_result=last_device_status=last_connected=last_sio_status=0;
    last_waiting=0;
    sprintf(net_uri,"N1:TCP://%s:%u",boards[index].host,boards[index].port);
    terminal_screen();
    terminal_text("Southern AMIS Projects\x9b");
    terminal_text(boards[index].name);
    terminal_text("\x9b\x9bSELECT: Macros and Scrollback\x9bOPTION: Cancel / disconnect\x9b");
    while (option()) {}
    if(!five_seconds(true)){message="Connection cancelled";return message;}
    OS.soundr=0;
    if(cleanup_needed)
    {
        cleanup_result=network_close(net_uri);
        if(cleanup_result!=FN_ERR_OK){message="Cleanup failed; reboot FujiNet";goto finish;}
        cleanup_needed=false;
    }
    result=network_open(net_uri,12,0);
    cleanup_needed=true;
    last_result=result; last_sio_status=PEEK(0x0303); last_device_status=fn_device_error;
    if (result != FN_ERR_OK) { message="Connection failed"; goto finish; }
    clear_terminal(); history_reset(); last=OS.rtclok[2]-1;
    while (true)
    {
        if (exit_pressed()) { message="Connection closed"; break; }
        if((PEEK(0xd01f)&2)==0)
        {
            if(!select_held && !panel)panel_open();
            select_held=true;
        }
        else select_held=false;
        if (kbhit())
        {
            key=bbs_getkey();
            if(panel){panel_key(key);key=0;}
            else if(macro_active){macro_active=0;}
            if(key)
            {
            OS.soundr=0;
            fn_network_error=0;
            result=network_write(net_uri,&key,1);
            if (result != FN_ERR_OK) { last_result=result;last_device_status=fn_network_error;last_sio_status=PEEK(0x0303);message="Write failed";break; }
            /* Typing may shorten the interval, but never restart its clock. */
            if(poll_frames>2)poll_frames=2;
            }
        }
        result=macro_tick();
        if(result!=FN_ERR_OK){message="Macro send failed";last_result=result;break;}
        now=OS.rtclok[2];
        if (!waiting && (unsigned char)(now-last)>=poll_frames)
        {
            OS.soundr=0;
            result=network_status_unit(1,&waiting,&connected,&err);
            last_result=result;last_device_status=err;last_connected=connected;last_waiting=waiting;last_sio_status=PEEK(0x0303);
            if (result != FN_ERR_OK) { message="Status failed"; break; }
            last=OS.rtclok[2];
            if (!waiting)
            {
                poll_frames=poll_frames<3?3:poll_frames<6?6:poll_frames<12?12:15;
                if (!connected || err==136) break;
                /* Atari error statuses have bit 7 set. */
                if (err>=128) { message="Network error"; break; }
            }
        }
        /* Defer a read once for a pending key, then service incoming data. */
        if (kbhit() && !key_deferred){key_deferred=true;continue;}
        if (waiting)
        {
            count=waiting<sizeof(rx)?waiting:sizeof(rx);
            OS.soundr=0;
            fn_network_error=0;
            result=sio_read(1,rx,count);
            if (result!=FN_ERR_OK) { last_result=result;last_device_status=fn_network_error;last_sio_status=PEEK(0x0303);message="Read failed";break; }
            waiting-=count; key_deferred=false; hide_cursor();
            render_bytes(rx,count);
            if(!waiting) { poll_frames=1; last=OS.rtclok[2]; }
        }
        if(panel==2)
        {
            if(count)review_dirty=true;
            /* Limit scrollback redraws to once every six frames. */
            if(review_dirty && (unsigned char)(OS.rtclok[2]-review_last)>=6)
            {panel_draw();review_last=OS.rtclok[2];review_dirty=false;}
        }
        else review_dirty=false;
        count=0;
        if(!panel)show_cursor();
    }
finish:
    if(panel)panel_close();
    macro_active=0;
    hide_cursor();
    OS.soundr=0;
    cleanup_result=network_close(net_uri);
    cleanup_needed=cleanup_result!=FN_ERR_OK;
    /* Retain the final remote screen (including any busy message). */
    gotoxy(0,21);memset(OS.savmsc+21*WIDTH,0,3*WIDTH);OS.oldchr=0;
    gotoxy(1,21);cputs(message);
    if(last_result!=FN_ERR_OK)
    {gotoxy(1,22);cprintf("Result %u Net %u SIO %u",last_result,last_device_status,last_sio_status);}
    if(cleanup_needed){gotoxy(1,23);cprintf("Cleanup failed: %u",cleanup_result);}
    five_seconds(false);
    while (option()) {}
    return message;
}
int main(void)
{
    unsigned char key, page=0, selected, highlight=0, number=0, saved_font;
    bool prompt_only=false;
    unsigned char *saved_dlist, *saved_pixels;
    unsigned char saved_colors[3];
    const char *message;
    initial_sound=OS.soundr; initial_break=PEEK(0x10)&0x80;
    disable_break(); OS.soundr=0;
    /* Set up the display before loading the directory and macros. */
    terminal_screen();
    OS.soundr=0;
    if(network_init()!=FN_ERR_OK)
    { directory_fallback(); fallback_used=true; notice="FujiNet network unavailable"; }
    else load_directory();
    macros_load();
    terminal_screen(); /* Reestablish text mode after loader/network/SD activity. */
    while(true)
    {
        if(prompt_only)draw_number(number);
        else draw_list(page,highlight,number);
        prompt_only=false; key=navigation_key(choice());
        if(key>='0' && key<='9')
        { if(number<10)number=number*10+key-'0'; prompt_only=true; continue; }
        if(key==126 && number){number/=10;prompt_only=true;continue;}
        if(key==27 && number){number=0;prompt_only=true;continue;}
        if(key!=155)number=0;
        if(key==28 && highlight){--highlight;continue;}
        if(key==29 && highlight+1<PAGE_SIZE && page*PAGE_SIZE+highlight+1<board_count){++highlight;continue;}
        if(key==30)key='p';
        if(key==31)key='n';

        if(key>='A' && key<='Z') key+=32;
        if(key=='c') return_config();
        else if(key=='m')macros_menu();
        else if(key=='h'){help_draw();while(choice()!=27){}}
        else if(key=='u') { if (edit_url()) { load_directory(); page=highlight=0; } }
        else if(key=='r') { load_directory(); page=highlight=0; }
        else if(key=='n' && (page+1)*PAGE_SIZE<board_count) {++page;highlight=0;}
        else if(key=='p' && page) {--page;highlight=0;}
        else if(key==155)
        {
            selected=number?number-1:page*PAGE_SIZE+highlight;
            number=0;
            if(selected>=board_count){notice="Invalid BBS number";continue;}
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
