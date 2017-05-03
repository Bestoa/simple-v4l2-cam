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

#define DEFAULT_IMAGE_WIDTH	    (1920)
#define DEFAULT_IMAGE_HEIGHT    (1280)
#define DEFAULT_DEVICE          "/dev/video0"

#define ZAP(x) memset (&(x), 0, sizeof (x))

enum {
    CAMERA_SUCCESS = 0,
    CAMERA_FAILURE,
};

enum {
    CAMERA_INIT,
    CAMERA_OPENED,
    CAMERA_CONFIGURED,
    CAMERA_BUFFER_MAPPED,
    CAMERA_STREAM_ON,
    CAMERA_BUFFER_LOCKED,
    CAMERA_STATE_ERROR,
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

    char                    *dev_name;
    int                     fd;
    int                     state;
    struct v4l2_format      fmt;
    struct v4l2_capability  cap;
    struct buffer_queue     bufq;

    void                    *priv;          /* user spec data */
};

static inline int xioctl(int fd,int request,void *arg)
{
    int r;
    do{ r = ioctl(fd, request, arg); }
    while(-1 == r && EINTR == errno);
    return r;
}
#endif
