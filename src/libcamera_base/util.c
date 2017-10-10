#include <time.h>

#include "camera.h"
#include "util.h"
#include "log.h"

void help(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t-g gui mode\n");
    fprintf(stderr, "\t-p device path\n");
    fprintf(stderr, "\t-w width\n\t-h height\n");
    fprintf(stderr, "\t-f format\n");
    fprintf(stderr, "\t-n output image number, noui mode only\n");
    fprintf(stderr, "\t-v verbose mode\n");
    fprintf(stderr, "Format: 0 YUYV 1 MJPEG 2 H264\n");
}

char *fmt2desc(int fmt)
{
    static char desc[5];
    sprintf(desc, "%c%c%c%c%c",
            fmt & 0xFF, (fmt >> 8) & 0xFF,
            (fmt >> 16) & 0xFF, (fmt >> 24) & 0xFF,
            0);
    return desc;
}

int save_buffer(struct buffer buffer, char * ext)
{
    char name[30] = { 0 };
    FILE *fp = NULL;
    struct time_recorder tr;
    time_recorder_start(&tr);
    sprintf(name, "image_%d_%d.%s", tr.start.tv_sec, tr.start.tv_usec, ext);
    fp = fopen(name, "wb");
    if (fp == NULL) {
        LOGE(DUMP_ERROR, "Can't open %s\n", name);
        return -EIO;
    }
    fwrite(buffer.addr, buffer.size, 1, fp);
    fclose(fp);
    time_recorder_end(&tr);
    LOGI("Save buffer: %s\n", name);
    time_recorder_print_time(&tr, "Save buffer");
    return CAMERA_RETURN_SUCCESS;
}

void time_recorder_start(struct time_recorder *tr)
{
    gettimeofday(&tr->start, NULL);
    tr->state = TR_START;
}

void time_recorder_end(struct time_recorder *tr)
{
    if (tr->state != TR_START) {
        LOGE(DUMP_NONE, "Time recorder haven't been started");
        return;
    }
    gettimeofday(&tr->end, NULL);
    tr->state = TR_END;
}

void time_recorder_print_time(struct time_recorder *tr, const char *msg)
{
    if (tr->state != TR_END) {
        LOGE(DUMP_NONE, "Time recorder haven't been stopped");
        return;
    }
    LOGD("%s take %ld.%03lds\n", msg,
            tr->end.tv_sec - tr->start.tv_sec - ((tr->end.tv_usec < tr->start.tv_usec)? 1 : 0),
            (tr->end.tv_usec - tr->start.tv_usec)/1000 + ((tr->end.tv_usec < tr->start.tv_usec)? 1000 : 0));
}
