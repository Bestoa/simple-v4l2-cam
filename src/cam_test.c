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

static struct configure gc; 

static int read_frame(int count)
{
    struct v4l2_buffer buf;
    struct timeval tv1, tv2;

    ZAP(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    //Count the time of get one frame
    gettimeofday(&tv1, NULL);
    if(-1 == xioctl(gc.fd, VIDIOC_DQBUF, &buf))
    {
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
    gettimeofday(&tv2, NULL);

    printf("Get one frame take %ld.%03lds\n",
            tv2.tv_sec - tv1.tv_sec - ((tv2.tv_usec < tv1.tv_usec)? 1 : 0),
            (tv2.tv_usec - tv1.tv_usec)/1000 + ((tv2.tv_usec < tv1.tv_usec)? 1000 : 0));

    assert(buf.index < gc.buffer_count);

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
    fwrite(gc.buffers[buf.index].start, gc.buffers[buf.index].length, 1, fp);
    fclose(fp);

    if(-1 == xioctl(gc.fd, VIDIOC_QBUF, &buf))
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
    if(-1 == xioctl(gc.fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for(i = 0; i < gc.buffer_count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(gc.fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(gc.fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

static void deinit_device(void)
{
    unsigned int i;

    for(i = 0; i < gc.buffer_count; ++i)
        if(-1 == munmap(gc.buffers[i].start, gc.buffers[i].length))
            errno_exit("munmap");
    free(gc.buffers);
}

static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    ZAP(req);

    req.count               = MAX_BUFFER_NUM;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if(-1 == xioctl(gc.fd, VIDIOC_REQBUFS, &req))
    {
        if(EINVAL == errno)
        {
            fprintf(stderr, "%s does not support "
                    "memory mapping\n", gc.dev_name);
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
                gc.dev_name);
        exit(EXIT_FAILURE);
    }
    gc.buffers = calloc(req.count, sizeof(struct buffer));

    if(!gc.buffers)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for(gc.buffer_count = 0; gc.buffer_count < req.count; gc.buffer_count++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = gc.buffer_count;

        if(-1 == xioctl(gc.fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        gc.buffers[gc.buffer_count].length = buf.length;
        gc.buffers[gc.buffer_count].start = mmap(NULL /* start anywhere */,
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                gc.fd, buf.m.offset);

        if(MAP_FAILED == gc.buffers[gc.buffer_count].start)
            errno_exit("mmap");
    }
}

static void init_device()
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;

    if(-1 == xioctl(gc.fd, VIDIOC_QUERYCAP, &cap))
    {
        if(EINVAL == errno)
        {
            fprintf(stderr, "%s is no V4L2 device\n", gc.dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n", gc.dev_name);
        exit(EXIT_FAILURE);
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n", gc.dev_name);
        exit(EXIT_FAILURE);
    }

    ZAP(fmt);
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = gc.width;
    fmt.fmt.pix.height      = gc.height;
    fmt.fmt.pix.pixelformat = gc.fmt;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if(-1 == xioctl(gc.fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit("VIDIOC_S_FMT");
    }
    /* Note VIDIOC_S_FMT may change width and height. */
    if(-1 == xioctl(gc.fd, VIDIOC_G_FMT, &fmt)) {
        errno_exit("VIDIOC_G_FMT");
    }
    printf("Output image will be %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    printf("Output pixelformat will be:%c%c%c%c\n",
            fmt.fmt.pix.pixelformat & 0xFF,
            (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 24) & 0xFF);

    init_mmap();
}

static void close_device(void)
{
    if(-1 == close(gc.fd)) errno_exit("close");
    gc.fd = -1;
}

static void open_device(void)
{
    struct stat st;

    if(-1 == stat(gc.dev_name, &st))
    {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                gc.dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(!S_ISCHR(st.st_mode))
    {
        fprintf(stderr, "%s is no device\n", gc.dev_name);
        exit(EXIT_FAILURE);
    }
    gc.fd = open(gc.dev_name, O_RDWR /* required */ , 0);
    if(-1 == gc.fd)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                gc.dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void query_format(void)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (xioctl(gc.fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        printf("Support pixel format[%d]: %s\n",
                fmtdesc.index, fmtdesc.description);
        fmtdesc.index++;
    }
}

#if 0
static void query_ctrl(void)
{
    struct v4l2_control control;
    int i = V4L2_CID_BRIGHTNESS;
    while (i < V4L2_CID_LASTP1) {
        memset(&control, 0, sizeof(control));
        control.id = i;

        if (-1 == xioctl (gc.fd, VIDIOC_QUERYCTRL, &control)) {
            printf("err = %s\n", strerror(errno));
        } else {
            printf("Support control action: %d\n", i);
        }
        i++;
    }
}
#endif

void help() {
    printf("Usage: -d device -w width -h height -f format\n");
    printf("Format: 1 MJPEG 2 YUYV 3 H264\n");
    exit(0);
}

void init_configure() {
    if (gc.dev_name == NULL) {
        gc.dev_name = DEFAULT_DEVICE;
    }
    if (gc.width == 0) {
        gc.width = DEFAULT_IMAGE_WIDTH;
    }
    if (gc.height == 0) {
        gc.height = DEFAULT_IMAGE_HEIGHT;
    }
    if (gc.fmt == 0) {
        gc.fmt = V4L2_PIX_FMT_YUYV;
    }
}

int main(int argc, char **argv)
{
    char opt;

    ZAP(gc);

    while ((opt = getopt(argc, argv, "d:w:h:f:")) != -1) {
        switch(opt){
            case 'd':
                gc.dev_name = optarg;
            case 'w':
                gc.width = atoi(optarg);
                break;
            case 'h':
                gc.height = atoi(optarg);
                break;
            case 'f':
                switch (*optarg) {
                    case '1': gc.fmt = V4L2_PIX_FMT_MJPEG; break;
                    case '2': gc.fmt = V4L2_PIX_FMT_YUYV; break;
                    case '3': gc.fmt = V4L2_PIX_FMT_H264; break;
                    default: gc.fmt = V4L2_PIX_FMT_YUYV;
                }
                break;
            default:
                help();
        }
    }

    init_configure();
    open_device();
    query_format();
    init_device();
    start_capturing();
    mainloop();
    stop_capturing();
    deinit_device();
    close_device();
    exit(EXIT_SUCCESS);
    return 0;
}
