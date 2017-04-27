#ifndef _TINY_CAMERA_
#define _TINY_CAMERA_

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define MAX_BUFFER_NUM (8)
#define MIN_BUFFER_NUM (2)

#define DEFAULT_FRAME_COUNT   	(3)
#define DEFAULT_IMAGE_WIDTH	    (1920)
#define DEFAULT_IMAGE_HEIGHT    (1280)
#define DEFAULT_DEVICE          "/dev/video0"

#define ZAP(x) memset (&(x), 0, sizeof (x))

enum {
    ACTION_STOP             = 1,
    ACTION_SAVE_PICTURE     = 2,
};

enum {
    FRAMEUSAGE_SAVE     = 1,
    FRAMEUSAGE_DISPLAY  = 2,
};

struct buffer {
    void        *addr;
    size_t      size;
};

struct buffer_queue {
    struct buffer       *buf;               /* buffer queue for driver */
    int                 count;              /* total buffer number */
};

struct v4l2_camera {

    char                *dev_name;
    int                 gui_mode;
    int                 fd;
    int                 frame_count;    /* total frame to save */

    struct v4l2_format  *fmt;
    struct buffer_queue bufq;
    struct window       *window;
};

static inline int xioctl(int fd,int request,void *arg)
{
    int r;
    do{ r = ioctl(fd, request, arg); }
    while(-1 == r && EINTR == errno);
    return r;
}
#endif
