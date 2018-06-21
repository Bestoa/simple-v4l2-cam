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

enum camera_return_type {
    CAMERA_RETURN_SUCCESS = 0,
    CAMERA_RETURN_FAILURE,
};

#define CAMERA_STATE_LIST \
    __CONVERT__(CAMREA_STATE_INIT) \
    __CONVERT__(CAMREA_STATE_OPENED) \
    __CONVERT__(CAMREA_STATE_CONFIGURED) \
    __CONVERT__(CAMREA_STATE_BUFFER_MAPPED) \
    __CONVERT__(CAMREA_STATE_STREAM_ON) \
    __CONVERT__(CAMREA_STATE_BUFFER_LOCKED) \
    __CONVERT__(CAMERA_STATE_ERROR)

enum camera_state_type {
#define __CONVERT__(x) x,
    CAMERA_STATE_LIST
#undef __CONVERT__
};

struct buffer {
    void        *addr;                      /* Data start addr */
    size_t      size;                       /* Data size */
};

struct buffer_queue {
    struct buffer       *buf;               /* Array of struct buffer point */
    int                 count;              /* Total buffer number */
};

struct v4l2_camera {

    char                    *dev_name;      /* Device name */
    int                     fd;
    int                     state;          /* Current state */
    struct v4l2_format      fmt;            /* Output format */
    struct v4l2_capability  cap;
    struct buffer_queue     bufq;

    void                    *priv;          /* user spec data */
};

static inline char * camera_state_to_string(enum camera_state_type type)
{
    switch (type) {
#define __CONVERT__(x) case x: return #x;
        CAMERA_STATE_LIST
#undef __CONVERT__
    };
    return "CAMERA_STATE_ERROR";
}

static inline int xioctl(int fd,int request,void *arg)
{
    int r;
    do{ r = ioctl(fd, request, arg); }
    while(-1 == r && EINTR == errno);
    return r;
}
#endif
