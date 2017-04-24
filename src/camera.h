#ifndef _CAM_TEST_
#define _CAM_TEST_

#include <sys/ioctl.h>

#define MAX_BUFFER_NUM (8)
#define MIN_BUFFER_NUM (2)

#define DEFAULT_FRAME_COUNT   	(3)
#define DEFAULT_IMAGE_WIDTH	    (1920)
#define DEFAULT_IMAGE_HEIGHT    (1280)
#define DEFAULT_DEVICE          "/dev/video0"

#define ZAP(x) memset (&(x), 0, sizeof (x))

enum {
    FRAMEUSAGE_SAVE = 1,
    FRAMEUSAGE_DISPLAY,
};

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
    int                 gfx_mode;
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
