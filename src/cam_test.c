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

#include "cam_test.h"

static char *           g_dev_name          = NULL;
static int              g_fd                = -1;
struct buffer *         g_buffers           = NULL;
static unsigned int     g_buffer_count      = 0;

static void errno_exit(const char *s)
{
    fprintf(stderr, "%s error %d, %s\n",s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int g_fd,int request,void *arg)
{
    int r;
    do{ r = ioctl(g_fd, request, arg); }
    while(-1 == r && EINTR == errno);
    return r;
}

static int read_frame(int count)
{
    struct v4l2_buffer buf;
    struct timeval tv;
    struct timezone tz;

    ZAP(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    gettimeofday(&tv, &tz);
    //Count the time of get one frame
    printf("==== Begin dequeue === %ld.%06ld\n", tv.tv_sec, tv.tv_usec );
    if(-1 == xioctl(g_fd, VIDIOC_DQBUF, &buf))
    {
        printf("Error\n");
        switch(errno)
        {
            case EAGAIN:
                return -EAGAIN;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                errno_exit("VIDIOC_DQBUF");
        }
    }
    gettimeofday(&tv, &tz);
    printf("==== End dequeue === %ld.%06ld\n\n", tv.tv_sec, tv.tv_usec );
    assert(buf.index < g_buffer_count);
    char name[20] = { 0 };
#ifdef _ANDROID_
    sprintf(name, "/data/%d.output", count);
#else
    sprintf(name, "./image/%d.output", count);
#endif
    FILE *fp = fopen(name, "wb");
    if (fp == NULL) {
        errno_exit("Create file");
    }
    fwrite(g_buffers[buf.index].start, g_buffers[buf.index].length, 1, fp);
    fclose(fp);
    if(-1 == xioctl(g_fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");

    return 0;
}

static void mainloop(void)
{
    unsigned int count;
    count = 0;
    while(count++ < FRAME_NUM)
    {
        /* EAGAIN - continue select loop. */
        while(read_frame(count) == EAGAIN);
    }
}

static void stop_capturing (void)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(g_fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for(i = 0; i < g_buffer_count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(g_fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(g_fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

static void deinit_device(void)
{
    unsigned int i;

    for(i = 0; i < g_buffer_count; ++i)
        if(-1 == munmap(g_buffers[i].start, g_buffers[i].length))
            errno_exit("munmap");
    free(g_buffers);
}

static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    ZAP(req);

    req.count               = MAX_BUFFER_NUM;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if(-1 == xioctl(g_fd, VIDIOC_REQBUFS, &req))
    {
        if(EINVAL == errno)
        {
            fprintf(stderr, "%s does not support "
                    "memory mapping\n", g_dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
            errno_exit("VIDIOC_REQBUFS");
        }
    }
    if(req.count < 2)
    {
        fprintf(stderr, "Insufficient buffer memory on %s\n",
                g_dev_name);
        exit(EXIT_FAILURE);
    }
    g_buffers = calloc(req.count, sizeof(*g_buffers));

    if(!g_buffers)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for(g_buffer_count = 0; g_buffer_count < req.count; ++g_buffer_count)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = g_buffer_count;

        if(-1 == xioctl(g_fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        g_buffers[g_buffer_count].length = buf.length;
        g_buffers[g_buffer_count].start =
            mmap(NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    g_fd, buf.m.offset);

        if(MAP_FAILED == g_buffers[g_buffer_count].start)
            errno_exit("mmap");
    }
}

static void init_device(int format)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    unsigned int min;

    if(-1 == xioctl(g_fd, VIDIOC_QUERYCAP, &cap))
    {
        if(EINVAL == errno)
        {
            fprintf(stderr, "%s is no V4L2 device\n",g_dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n",g_dev_name);
        exit(EXIT_FAILURE);
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n",
                g_dev_name);
        exit(EXIT_FAILURE);
    }

    ZAP(fmt);
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = IMAGE_WIDTH;
    fmt.fmt.pix.height      = IMAGE_HEIGHT;
    fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if(-1 == xioctl(g_fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit("VIDIOC_S_FMT");
    }
    /* Note VIDIOC_S_FMT may change width and height. */
    if(-1 == xioctl(g_fd, VIDIOC_G_FMT, &fmt)) {
        errno_exit("VIDIOC_G_FMT");
    }
    printf("Output image will be %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    printf("Output pixelformat:%c%c%c%c\n",
            fmt.fmt.pix.pixelformat & 0xFF,
            (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 24) & 0xFF);

    init_mmap();
}

static void close_device(void)
{
    if(-1 == close(g_fd)) errno_exit("close");
    g_fd = -1;
}

static void open_device(void)
{
    struct stat st;

    if(-1 == stat(g_dev_name, &st))
    {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                g_dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(!S_ISCHR(st.st_mode))
    {
        fprintf(stderr, "%s is no device\n", g_dev_name);
        exit(EXIT_FAILURE);
    }
    g_fd = open(g_dev_name, O_RDWR /* required */ , 0);
    if(-1 == g_fd)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                g_dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void query_format(void)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (xioctl(g_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        printf("Support pixel format[%d]: %s\n",
                fmtdesc.index, fmtdesc.description);
        fmtdesc.index++;
    }
}

static void query_ctrl(void)
{
    struct v4l2_control control;
    int i = V4L2_CID_BRIGHTNESS;
    while (i < V4L2_CID_LASTP1) {
        memset(&control, 0, sizeof(control));
        control.id = i;

        if (-1 == xioctl (g_fd, VIDIOC_QUERYCTRL, &control)) {
            printf("err = %s\n", strerror(errno));
        } else {
            printf("Support control action: %d\n", i);
        }
        i++;
    }
}


int main(int argc, char **argv)
{
    g_dev_name = "/dev/video0";

    printf("open device\n");
    open_device();
    printf("query format\n");
    query_format();
#if 0
    printf("query ctrl\n");
    query_ctrl();
#endif
    printf("init\n");
    if (argc > 1) {
        switch (*argv[1]) {
            case '1':
                printf("Select MJPEG\n");
                init_device(V4L2_PIX_FMT_MJPEG);
                break;
        }
    } else {
        printf("Select YUYV by default\n");
        init_device(V4L2_PIX_FMT_YUYV);
    }
    printf("start capturing\n");
    start_capturing();
    printf("mainloop\n");
    mainloop();
    printf("stop_capturing\n");
    stop_capturing();
    printf("deinit_device\n");
    deinit_device();
    printf("close_device\n");
    close_device();
    exit(EXIT_SUCCESS);
    return 0;
}
