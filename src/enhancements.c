#include <atari.h>
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <fujinet-fuji.h>
#include "enhancements.h"
extern unsigned char bbs_getkey(void);
char macros[MACRO_COUNT][MACRO_LENGTH+1];
#ifdef __CC65__
#pragma bss-name (push, "HISTORY")
#endif
static unsigned char history[HISTORY_LINES][40];
#ifdef __CC65__
#pragma bss-name (pop)
#endif
static unsigned char history_head, history_used;
#ifndef BBS_SCREEN
#define BBS_SCREEN ((unsigned char *)0x7400)
#endif
static char draft[MACRO_LENGTH+1];
/* Reserve the complete AppKey frame: two-byte length plus 64-byte payload. */
static unsigned char appkey_packet[66];
static const char *macro_status="Macros: not loaded";
/* Experimental local namespace; coordinate allocation before publishing. */
#define MACRO_CREATOR 0x5341
#define MACRO_APP 0x42
static void rule(unsigned char y)
{
    unsigned char i;
    gotoxy(0,y); for(i=0;i<40;++i)cputc(18);
}
void ui_heading(const char *subtitle)
{
    clrscr(); revers(1); gotoxy(0,0);
    cputs("  Southern AMIS / FujiNet BBS Client   ");
    revers(0); rule(1); gotoxy(1,2); cprintf("%.38s",subtitle);
}
void ui_footer(const char *a,const char *b)
{
    rule(20); gotoxy(1,21); cprintf("%.38s",a); gotoxy(1,22); cprintf("%.38s",b);
}
void macros_load(void)
{
    unsigned char i,j; uint16_t size;
    memset(macros,0,sizeof(macros));
    fuji_set_appkey_details(MACRO_CREATOR,MACRO_APP,DEFAULT);
    for(i=0;i<MACRO_COUNT;++i)
    {
        size=0; memset(appkey_packet,0,sizeof(appkey_packet));
        if(!fuji_read_appkey(i+1,&size,appkey_packet) || size>MACRO_LENGTH+1)
            macros[i][0]=0;
        else
        {
            memcpy(macros[i],appkey_packet,size<MACRO_LENGTH?size:MACRO_LENGTH);
            macros[i][size<MACRO_LENGTH?size:MACRO_LENGTH]=0;
        }
        for(j=0;macros[i][j];++j)
            if((unsigned char)macros[i][j]<32 || (unsigned char)macros[i][j]>126)
            {macros[i][0]=0;break;}
    }
    macro_status="Macros loaded (empty if unavailable)";
}
void macros_menu(void)
{
    unsigned char i,key,c; unsigned int n;
    while(1)
    {
        ui_heading("SD macros / 1-4 Edit, Esc Return");
        for(i=0;i<MACRO_COUNT;++i)
        {
            gotoxy(1,4+i*3); cprintf("%u: %.34s",i+1,macros[i][0]?macros[i]:"(empty)");
        }
        gotoxy(1,17); cputs(macro_status);
        gotoxy(1,18); cputs("In call: SELECT Macros/Scrollback");
        ui_footer("\\r = Return   \\p = 1 second pause", "\\\\ = backslash / stored as plain text");
        while(!kbhit()){} key=bbs_getkey();
        if(key==27)return;
        if(key<'1'||key>'4')continue;
        i=key-'1'; strcpy(draft,macros[i]); n=strlen(draft);
        ui_heading("Edit macro / Return Save, Esc Cancel");
        gotoxy(1,4); cputs("Max 63 chars. Ctrl-U clears text.");
        gotoxy(1,6); cputs(draft);
        ui_footer("\\r = Return   \\p = 1 second pause", "Passwords, if entered, are plain text.");
        while(1)
        {
            while(!kbhit()){} c=bbs_getkey();
            if(c==27)break;
            if(c==155)
            {
                draft[n]=0;
                fuji_set_appkey_details(MACRO_CREATOR,MACRO_APP,DEFAULT);
                if(fuji_write_appkey(i+1,n+1,(unsigned char *)draft))
                { strcpy(macros[i],draft); macro_status="Saved to FujiNet SD card"; }
                else macro_status="Save failed - check SD card";
                break;
            }
            if(c==21)n=0;
            else if(c==126){if(n)--n;}
            else if(c>=32 && c<=126 && n<MACRO_LENGTH)draft[n++]=c;
            draft[n]=0;
            gotoxy(1,6); cputs("                                      ");
            gotoxy(1,7); cputs("                                      ");
            gotoxy(1,6); cputs(draft);
        }
    }
}
void history_reset(void){ history_head=history_used=0; }
void history_add(const unsigned char *line)
{
    memcpy(history[history_head],line,40);
    history_head=(history_head+1)%HISTORY_LINES;
    if(history_used<HISTORY_LINES)++history_used;
}
unsigned char history_count(void){return history_used;}
void history_draw(const unsigned char *live,unsigned char offset)
{
    unsigned char y,pos,first;
    unsigned int logical;
    unsigned char *screen=BBS_SCREEN;
    if(offset>history_used)offset=history_used;
    first=(history_head+HISTORY_LINES-history_used)%HISTORY_LINES;
    for(y=0;y<24;++y)
    {
        logical=history_used-offset+y;
        if(logical<history_used)
        { pos=(first+logical)%HISTORY_LINES; memcpy(screen+y*40,history[pos],40); }
        else memcpy(screen+y*40,live+(logical-history_used)*40,40);
    }
    /* Review owns the last display row; the live terminal remains intact. */
    gotoxy(0,23); revers(1);
    cprintf(" Scrollback -%02u Arrows/Page Esc:Live ",offset);
    revers(0);
}
