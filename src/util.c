#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/videodev2.h>

#include "camera.h"

void help(void) {
    printf("Usage:\n");
    printf("\t-p device path\n");
    printf("\t-w width\n\t-h height\n");
    printf("\t-f format\n");
    printf("\t-n output image number\n");
    printf("Format: 0 YUYV 1 MJPEG 2 H264\n");
    exit(0);
}

char *fmt2desc(int fmt) {
    static char desc[5];
    sprintf(desc, "%c%c%c%c%c",
            fmt & 0xFF, (fmt >> 8) & 0xFF,
            (fmt >> 16) & 0xFF, (fmt >> 24) & 0xFF,
            0);
    return desc;
}

int save_output(struct camera_config *conf, int count) {
    char name[10] = { 0 };
    FILE *fp = NULL;

    sprintf(name, "./out_%d.%s", count,
            fmt2desc(conf->fmt->fmt.pix.pixelformat));

    fp = fopen(name, "wb");
    if (fp == NULL) {
        return -EIO;
    }
    fwrite(conf->bufq.buf[conf->bufq.current].addr,
            conf->bufq.buf[conf->bufq.current].size,
            1, fp);
    fclose(fp);
    return 0;
}
