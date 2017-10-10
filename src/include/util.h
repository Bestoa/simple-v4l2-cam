#ifndef _UTIL_
#define _UTIL_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "camera.h"

struct time_recorder {
    struct timeval start;
    struct timeval end;
    int state;
};

enum {
    TR_START,
    TR_END,
};

void help(void);
char *fmt2desc(int fmt);
int save_buffer(struct buffer buffer, char *ext);
void time_recorder_start(struct time_recorder *tr);
void time_recorder_end(struct time_recorder *tr);
void time_recorder_print_time(struct time_recorder *tr, const char *msg);

#endif
