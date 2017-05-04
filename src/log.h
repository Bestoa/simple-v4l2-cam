#ifndef _LOG_
#define _LOG_

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

enum {
    LOG_LEVEL_START,
    DEBUG,
    INFO,
    ERROR,
    LOG_LEVEL_END,
};

#define DUMP_ERROR (1)
#define DUMP_NONE  (0)

void __camera_log(int, int, const char *, ...);

#define LOGE(dump_errno, msg, ...) do {\
    __camera_log(dump_errno, ERROR, "(%s):(%d): "msg, __func__, __LINE__, ##__VA_ARGS__);\
}while(0)

#define LOGI(msg, ...) do {\
    __camera_log(0, INFO, msg, ##__VA_ARGS__);\
}while(0)

#define LOGD(msg, ...) do {\
    __camera_log(0, DEBUG, msg, ##__VA_ARGS__);\
}while(0)

void set_log_level(int l);
int get_log_level();
#endif
