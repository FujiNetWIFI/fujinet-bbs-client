#ifndef DIRECTORY_H
#define DIRECTORY_H
#include <stdbool.h>
#include "settings.h"
typedef struct { char name[32]; char host[96]; unsigned int port; } Board;
extern Board boards[MAX_BOARDS];
extern unsigned char board_count;
extern unsigned int rejected_lines;
void directory_reset(void);
void directory_feed(unsigned char c);
void directory_finish(void);
void directory_fallback(void);
bool directory_https_url(const char *url);
#endif
