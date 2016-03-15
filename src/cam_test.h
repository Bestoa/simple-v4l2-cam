#ifndef _CAM_TEST_
#define _CAM_TEST_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>
#include <time.h>

#define MAX_BUFFER_NUM (8)

#define DEFAULT_FRAME_COUNT   	(3)
#define DEFAULT_IMAGE_WIDTH	    (1920)
#define DEFAULT_IMAGE_HEIGHT    (1280)
#define DEFAULT_DEVICE          "/dev/video0"

#define ZAP(x) memset (&(x), 0, sizeof (x))

struct buffer {
    void        *addr;
    size_t      size;
};

struct buffer_queue {
    struct buffer       *buf;               /* buffer queue for driver */
    int                 count;              /* total buffer number */
    int                 current;            /* current buffer index */
};

struct camera_config {

    char                *dev_name;
    int                 fd;

    struct v4l2_format  *fmt;

    struct buffer_queue bufq;

    int                 frame_count;    /* total frame to save */
};

static inline void errno_exit(const char *s)
{
    fprintf(stderr, "%s error %d, %s\n",s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static inline int xioctl(int fd,int request,void *arg)
{
    int r;
    do{ r = ioctl(fd, request, arg); }
    while(-1 == r && EINTR == errno);
    return r;
}

void help(void);
char *fmt2desc(int fmt);
int save_output(struct camera_config *conf, int count);
#endif
