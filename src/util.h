#ifndef _UTIL_
#define _UTIL_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void help(void);
char *fmt2desc(int fmt);
int save_output(void * addr, size_t len, int index, char * fmt);
#endif
