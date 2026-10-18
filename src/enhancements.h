#ifndef ENHANCEMENTS_H
#define ENHANCEMENTS_H
#include <stdbool.h>
#define MACRO_COUNT 4
#define MACRO_LENGTH 63
#define HISTORY_LINES 64
extern char macros[MACRO_COUNT][MACRO_LENGTH+1];
void macros_load(void);
void macros_menu(void);
void ui_heading(const char *subtitle);
void ui_footer(const char *line1, const char *line2);
void history_reset(void);
void history_add(const unsigned char *line);
void history_draw(const unsigned char *live, unsigned char offset);
unsigned char history_count(void);
#endif
