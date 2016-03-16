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

#include "cam_test.h"

static int read_frame(struct camera_config *conf, int count)
{
    struct v4l2_buffer buf;
    struct timeval tv1, tv2;

    ZAP(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    //Count the time of get one frame
    gettimeofday(&tv1, NULL);
    if(-1 == xioctl(conf->fd, VIDIOC_DQBUF, &buf))
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

    assert(buf.index < conf->bufq.count);
    conf->bufq.current = buf.index;

    save_output(conf, count);

    if(-1 == xioctl(conf->fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");

    return 0;
}

static void stop_capturing (struct camera_config *conf)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(conf->fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(struct camera_config *conf)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for(i = 0; i < conf->bufq.count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(conf->fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(conf->fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

static void mainloop(struct camera_config *conf)
{
    unsigned int count;
    start_capturing(conf);
    count = 0;
    while(count++ < conf->frame_count)
    {
        /* EAGAIN - continue select loop. */
        while(read_frame(conf, count) == EAGAIN);
    }
    stop_capturing(conf);
}

static void close_device(struct camera_config *conf)
{
    if(-1 == close(conf->fd)) errno_exit("close");
    conf->fd = -1;
}

static void deinit_device(struct camera_config *conf)
{
    unsigned int i;

    for(i = 0; i < conf->bufq.count; ++i)
        if(-1 == munmap(conf->bufq.buf[i].addr, conf->bufq.buf[i].size))
            errno_exit("munmap");
    close_device(conf);
}

static void init_mmap(struct camera_config *conf)
{
    struct v4l2_requestbuffers req;
    int i;

    ZAP(req);

    req.count               = MAX_BUFFER_NUM;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if(-1 == xioctl(conf->fd, VIDIOC_REQBUFS, &req))
        if(EINVAL == errno)
            errno_exit("VIDIOC_REQBUFS");

    if(req.count < 2)
    {
        fprintf(stderr, "Insufficient buffer memory on %s\n", conf->dev_name);
        exit(EXIT_FAILURE);
    }
    conf->bufq.buf = calloc(req.count, sizeof(struct buffer));

    if(!conf->bufq.buf)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    conf->bufq.count = req.count;
    for(i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(conf->fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        conf->bufq.buf[i].size = buf.length;
        conf->bufq.buf[i].addr = mmap(NULL /* start anywhere */,
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                conf->fd, buf.m.offset);

        if(MAP_FAILED == conf->bufq.buf[i].addr)
            errno_exit("mmap");
    }
}

static void open_device(struct camera_config *conf)
{
    struct stat st;

    if(-1 == stat(conf->dev_name, &st))
    {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                conf->dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(!S_ISCHR(st.st_mode))
    {
        fprintf(stderr, "%s is not char device\n", conf->dev_name);
        exit(EXIT_FAILURE);
    }

    conf->fd = open(conf->dev_name, O_RDWR /* required */ , 0);
    if(-1 == conf->fd)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                conf->dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void init_device(struct camera_config *conf)
{
    struct v4l2_capability cap;

    open_device(conf);

    if(-1 == xioctl(conf->fd, VIDIOC_QUERYCAP, &cap))
    {
        if(EINVAL == errno)
        {
            fprintf(stderr, "%s is not a V4L2 device\n", conf->dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n", conf->dev_name);
        exit(EXIT_FAILURE);
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n", conf->dev_name);
        exit(EXIT_FAILURE);
    }

    conf->fmt->type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    conf->fmt->fmt.pix.field       = V4L2_FIELD_ANY;

    if(-1 == xioctl(conf->fd, VIDIOC_S_FMT, conf->fmt)) {
        errno_exit("VIDIOC_S_FMT");
    }
    /* Note VIDIOC_S_FMT may change width and height. */
    if(-1 == xioctl(conf->fd, VIDIOC_G_FMT, conf->fmt)) {
        errno_exit("VIDIOC_G_FMT");
    }
    printf("Output image will be %dx%d\n",
            conf->fmt->fmt.pix.width,
            conf->fmt->fmt.pix.height);
    printf("Output pixelformat will be:%s\n",
            fmt2desc(conf->fmt->fmt.pix.pixelformat));

    init_mmap(conf);
}


#if 0
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

static struct camera_config *init_camera_config() {
    struct camera_config * conf = NULL;

    conf = malloc(sizeof(struct camera_config));
    if (!conf) {
        return NULL;
    }
    ZAP(*conf);
    conf->fmt = malloc(sizeof(struct v4l2_format));
    if (!conf->fmt) {
        free(conf);
        return NULL;
    }
    ZAP(*(conf->fmt));

    conf->dev_name = DEFAULT_DEVICE;
    conf->fmt->fmt.pix.width = DEFAULT_IMAGE_WIDTH;
    conf->fmt->fmt.pix.height = DEFAULT_IMAGE_HEIGHT;
    conf->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    conf->frame_count = DEFAULT_FRAME_COUNT;

    return conf;
}

static void deinit_camera_config(struct camera_config *conf) {
    if (conf->bufq.buf) {
        free(conf->bufq.buf);
    }
    if (conf->fmt) {
        free(conf->fmt);
    }
    if (conf) {
        free(conf);
    }
}

int main(int argc, char **argv)
{
    char opt;
    struct camera_config *conf = NULL;

    conf = init_camera_config();
    if (!conf) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "p:w:h:f:n:")) != -1) {
        switch(opt){
            case 'p':
                conf->dev_name = optarg;
            case 'w':
                conf->fmt->fmt.pix.width = atoi(optarg);
                break;
            case 'h':
                conf->fmt->fmt.pix.height = atoi(optarg);
                break;
            case 'n':
                conf->frame_count = atoi(optarg);
                if (conf->frame_count <= 0)
                    conf->frame_count = DEFAULT_FRAME_COUNT;
                break;
            case 'f':
                switch (*optarg) {
                    case '1':
                        conf->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
                        break;
                    case '2':
                        conf->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
                        break;
                    case '0': /* default, fall through */
                    default:
                        conf->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
                }
                break;
            default:
                help();
        }
    }

    init_device(conf);
    mainloop(conf);
    deinit_device(conf);
    deinit_camera_config(conf);

    return 0;
}
