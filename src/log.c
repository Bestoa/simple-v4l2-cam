#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"

static int log_level = DEBUG;

void set_log_level(int l)
{
    if (l <= LOG_LEVEL_START || l >= LOG_LEVEL_END) {
        l = ERROR;
    }
    log_level = l;
}

void __log(int dump_errno, int level, const char *msg, ...)
{
    va_list ap;
    FILE *fp;

    if (level < log_level) return;
    if (level == ERROR) {
        fp = stderr;
    }else{
        fp = stdout;
    }

    if (dump_errno) {
        fprintf(fp, "%s. ", strerror(errno));
    }
    va_start(ap, msg);
    vfprintf(fp, msg, ap);
    va_end(ap);
}
