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

int save_output(void * addr, size_t len, int index, char * fmt)
{
    char name[20] = { 0 };
    FILE *fp = NULL;

    if (index == -1) {
        time_t t;
        struct tm *ptm;
        time(&t);
        ptm = localtime(&t);
        sprintf(name, "./out_%02d%02d%02d%02d%02d.%s", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, fmt);
    } else {
        sprintf(name, "./out_%d.%s", index, fmt);
    }

    fp = fopen(name, "wb");
    if (fp == NULL) {
        LOGE(DUMP_ERRNO, "Can't open %s\n", name);
        return -EIO;
    }
    fwrite(addr, len, 1, fp);
    fclose(fp);
    LOGI("Save output: %s\n", name);
    return 0;
}
