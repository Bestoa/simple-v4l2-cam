#include "log.h"

static int log_level = INFO;

void set_log_level(int l)
{
    if (l <= LOG_LEVEL_START || l >= LOG_LEVEL_END) {
        l = ERROR;
    }
    log_level = l;
}

int get_log_level()
{
    return log_level;
}

void __camera_log(int dump_errno, int level, const char *msg, ...)
{
    va_list ap;
    FILE *fp;

    if (level < log_level) return;
    if (level == ERROR) {
        fp = stderr;
    }else{
        fp = stdout;
    }

    va_start(ap, msg);
    vfprintf(fp, msg, ap);
    va_end(ap);

    if (dump_errno) {
        fprintf(fp, "Error: %s.\n", strerror(errno));
    }
}
