#ifndef _LOG_
#define _LOG_

enum {
    LOG_LEVEL_START,
    DEBUG,
    INFO,
    ERROR,
    LOG_LEVEL_END,
};

#define DUMP_ERRNO (1)
#define NO_DUMP_ERRNO (0)

void __log(int, int, const char *, ...);

#define LOGE(dump_errno, msg, ...) do {\
    __log(dump_errno, ERROR, msg, ##__VA_ARGS__);\
}while(0)

#define LOGI(msg, ...) do {\
    __log(0, INFO, msg, ##__VA_ARGS__);\
}while(0)

#define LOGD(msg, ...) do {\
    __log(0, DEBUG, msg, ##__VA_ARGS__);\
}while(0)

void set_log_level(int l);
#endif
