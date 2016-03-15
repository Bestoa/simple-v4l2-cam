#ifndef _CAM_TEST_
#define _CAM_TEST_

#define FRAME_NUM	(10)
#define MAX_BUFFER_NUM (8)

#define DEFAULT_IMAGE_WIDTH	    (1920)
#define DEFAULT_IMAGE_HEIGHT    (1280)
#define DEFAULT_DEVICE "/dev/video0"

#define ZAP(x) memset (&(x), 0, sizeof (x))

typedef struct buffer {
    void *                  start;
    size_t                  length;
}buffer;

struct configure {
    char    *dev_name;
    int     fd;     
    buffer  *buffers;
    int     buffer_count;
    int     width;
    int     height;
    int     fmt; /* 0 unset 1 MJPEG 2 YUYV 3 H264, default 2 */
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

#endif
