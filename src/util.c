#include "camera.h"
#include "util.h"
#include "log.h"

void help(void) {
    printf("Usage:\n");
    printf("\t-g gui mode\n");
    printf("\t-p device path\n");
    printf("\t-w width\n\t-h height\n");
    printf("\t-f format\n");
    printf("\t-n output image number\n");
    printf("Format: 0 YUYV 1 MJPEG 2 H264\n");
}

char *fmt2desc(int fmt) {
    static char desc[5];
    sprintf(desc, "%c%c%c%c%c",
            fmt & 0xFF, (fmt >> 8) & 0xFF,
            (fmt >> 16) & 0xFF, (fmt >> 24) & 0xFF,
            0);
    return desc;
}

int save_output(void * addr, size_t len, int index, char * fmt) {
    char name[16] = { 0 };
    FILE *fp = NULL;

    sprintf(name, "./out_%d.%s", index, fmt);

    fp = fopen(name, "wb");
    if (fp == NULL) {
        LOGE(DUMP_ERRNO, "Can't open %s\n", name);
        return -EIO;
    }
    fwrite(addr, len, 1, fp);
    fclose(fp);
    return 0;
}
