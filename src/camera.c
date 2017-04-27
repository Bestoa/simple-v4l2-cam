#include "camera.h"
#include "window.h"
#include "util.h"
#include "log.h"

static int read_frame(struct v4l2_camera *cam, int count, int usage)
{
    struct v4l2_buffer buf;
    struct timeval tv1, tv2;
    void * addr;
    size_t len;
    int ret;

    ZAP(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    //Count the time of get one frame
    gettimeofday(&tv1, NULL);
    if(-1 == xioctl(cam->fd, VIDIOC_DQBUF, &buf))
    {
        switch(errno)
        {
            case EAGAIN:
                return -EAGAIN;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                LOGE(DUMP_ERRNO, "dequeue buffer failed\n");
                return ACTION_STOP;
        }
    }
    gettimeofday(&tv2, NULL);

    LOGD("Get one frame take %ld.%03lds\n",
            tv2.tv_sec - tv1.tv_sec - ((tv2.tv_usec < tv1.tv_usec)? 1 : 0),
            (tv2.tv_usec - tv1.tv_usec)/1000 + ((tv2.tv_usec < tv1.tv_usec)? 1000 : 0));

    assert(buf.index < cam->bufq.count);

    addr = cam->bufq.buf[buf.index].addr;
    len = cam->bufq.buf[buf.index].size;

    if (usage & FRAMEUSAGE_DISPLAY) {
        ret = window_update_frame(cam->window, addr, len);
    }
    if (ret == ACTION_STOP)
        return ACTION_STOP;
    if (ret == ACTION_SAVE_PICTURE)
        usage |= FRAMEUSAGE_SAVE;

    if (usage & FRAMEUSAGE_SAVE) {
        if (save_output(addr, len, count, fmt2desc(cam->fmt->fmt.pix.pixelformat)))
            return ACTION_STOP;
    }

    if(-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf)) {
        LOGE(DUMP_ERRNO, "queue buffer failed\n");
        return ACTION_STOP;
    }

    return 0;
}

static int start_capturing(struct v4l2_camera *cam)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for(i = 0; i < cam->bufq.count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf)) {
            LOGE(DUMP_ERRNO, "queue buffer failed\n");
            return -1;
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(cam->fd, VIDIOC_STREAMON, &type)) {
        LOGE(DUMP_ERRNO, "stream on failed\n");
        return -1;
    }
    return 0;
}

static void stop_capturing (struct v4l2_camera *cam)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(cam->fd, VIDIOC_STREAMOFF, &type)) {
        LOGE(DUMP_ERRNO, "stream off failed\n");
    }
}


static void mainloop_noui(struct v4l2_camera *cam)
{
    unsigned int count;
    int ret;
    if (start_capturing(cam))
        return;
    count = 0;
    while(count++ < cam->frame_count)
    {
        /* EAGAIN - continue select loop. */
        while((ret = read_frame(cam, count, FRAMEUSAGE_SAVE)) == EAGAIN);
        if (ret == ACTION_STOP)
            break;
    }
    stop_capturing(cam);
}

static void mainloop_gui(struct v4l2_camera *cam)
{
    int ret;
    start_capturing(cam);
    while (1) {
        while((ret = read_frame(cam, -1, FRAMEUSAGE_DISPLAY)) == EAGAIN);
        if (ret == ACTION_STOP)
            break;
    }
    stop_capturing(cam);
}

static int map_buffer(struct v4l2_camera *cam)
{
    struct v4l2_requestbuffers req;
    int i;

    ZAP(req);

    req.count               = MAX_BUFFER_NUM;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if(-1 == xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
        LOGE(DUMP_ERRNO, "requery buffer failed\n");
        return -1;
    }

    if(req.count < MIN_BUFFER_NUM)
    {
        LOGE(NO_DUMP_ERRNO, "Insufficient buffer memory on %s\n", cam->dev_name);
        return -1;
    }
    cam->bufq.buf = calloc(req.count, sizeof(struct buffer));

    if(!cam->bufq.buf)
    {
        LOGE(NO_DUMP_ERRNO, "Out of memory\n");
        return -1;
    }

    cam->bufq.count = req.count;
    for(i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        ZAP(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl(cam->fd, VIDIOC_QUERYBUF, &buf)) {
            LOGE(DUMP_ERRNO, "query buffer failed\n");
            exit(EXIT_FAILURE);
        }

        cam->bufq.buf[i].size = buf.length;
        cam->bufq.buf[i].addr = mmap(NULL /* start anywhere */,
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                cam->fd, buf.m.offset);

        if(MAP_FAILED == cam->bufq.buf[i].addr) {
            LOGE(DUMP_ERRNO, "mmap failed\n");
            goto out_unmap_buffer;
        }
    }
    return 0;
out_unmap_buffer:
    while(--i >= 0) {
        munmap(cam->bufq.buf[i].addr, cam->bufq.buf[i].size);
    }
out_free_buffer:
    free(cam->bufq.buf);
    return -1;
}

static void unmap_buffer(struct v4l2_camera *cam)
{
    unsigned int i;
    for(i = 0; i < cam->bufq.count; ++i)
        munmap(cam->bufq.buf[i].addr, cam->bufq.buf[i].size);
}

static int open_device(struct v4l2_camera *cam)
{
    struct stat st;

    if(-1 == stat(cam->dev_name, &st))
    {
        LOGE(DUMP_ERRNO, "Cannot identify '%s'\n", cam->dev_name);
        return -1;
    }

    if(!S_ISCHR(st.st_mode))
    {
        LOGE(NO_DUMP_ERRNO, "%s is not char device\n", cam->dev_name);
        return -1;
    }

    cam->fd = open(cam->dev_name, O_RDWR /* required */ , 0);
    if(-1 == cam->fd)
    {
        LOGE(DUMP_ERRNO, "Cannot open '%s'\n", cam->dev_name);
        return -1;
    }
    return 0;
}

static void close_device(struct v4l2_camera *cam)
{
    if(-1 == close(cam->fd)) {
        LOGE(DUMP_ERRNO, "close device failed\n");
    }
    cam->fd = -1;
}

static int init_device(struct v4l2_camera *cam)
{
    struct v4l2_capability cap;

    if (open_device(cam))
        return -1;

    if(-1 == xioctl(cam->fd, VIDIOC_QUERYCAP, &cap))
    {
        LOGE(DUMP_ERRNO, "query cap failed\n");
        goto out_close;
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOGE(NO_DUMP_ERRNO, "%s is no video capture device\n", cam->dev_name);
        goto out_close;
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        LOGE(NO_DUMP_ERRNO, "%s does not support streaming i/o\n", cam->dev_name);
        goto out_close;
    }

    if(-1 == xioctl(cam->fd, VIDIOC_S_FMT, cam->fmt)) {
        LOGE(DUMP_ERRNO, "set format failed\n");
        goto out_close;
    }
    /* Note VIDIOC_S_FMT may change width and height. */
    LOGI("Output image will be %dx%d\n",
            cam->fmt->fmt.pix.width,
            cam->fmt->fmt.pix.height);
    LOGI("Output pixelformat will be:%s\n",
            fmt2desc(cam->fmt->fmt.pix.pixelformat));

    if (map_buffer(cam))
        goto out_close;

    return 0;

out_close:
    close_device(cam);
    return -1;
}

static void deinit_device(struct v4l2_camera *cam)
{
    close_device(cam);
}


#if 0
static void query_format(void)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (xioctl(gc.fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        LOGI("Support pixel format[%d]: %s\n",
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
        } else {
            LOGI("Support control action: %d\n", i);
        }
        i++;
    }
}
#endif

static struct v4l2_camera *alloc_v4l2_camera() {
    struct v4l2_camera * cam = NULL;

    cam = malloc(sizeof(struct v4l2_camera));
    if (!cam) {
        return NULL;
    }
    ZAP(*cam);
    cam->fmt = malloc(sizeof(struct v4l2_format));
    if (!cam->fmt) {
        free(cam);
        return NULL;
    }
    ZAP(*(cam->fmt));

    cam->dev_name = DEFAULT_DEVICE;

    cam->fmt->type                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->fmt->fmt.pix.width         = DEFAULT_IMAGE_WIDTH;
    cam->fmt->fmt.pix.height        = DEFAULT_IMAGE_HEIGHT;
    cam->fmt->fmt.pix.pixelformat   = V4L2_PIX_FMT_YUYV;
    cam->fmt->fmt.pix.field         = V4L2_FIELD_ANY;

    cam->frame_count = DEFAULT_FRAME_COUNT;

    return cam;
}

static void free_v4l2_camera(struct v4l2_camera *cam) {
    if (cam->bufq.buf) {
        free(cam->bufq.buf);
    }
    if (cam->fmt) {
        free(cam->fmt);
    }
    if (cam) {
        free(cam);
    }
}

int main(int argc, char **argv)
{
    int opt;
    struct v4l2_camera *cam = NULL;

    cam = alloc_v4l2_camera();
    if (!cam) {
        LOGE(NO_DUMP_ERRNO, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "gp:w:h:f:n:")) != -1) {
        switch(opt){
            case 'g':
                cam->gui_mode = 1;
                break;
            case 'p':
                cam->dev_name = optarg;
                break;
            case 'w':
                cam->fmt->fmt.pix.width = atoi(optarg);
                break;
            case 'h':
                cam->fmt->fmt.pix.height = atoi(optarg);
                break;
            case 'n':
                cam->frame_count = atoi(optarg);
                if (cam->frame_count <= 0)
                    cam->frame_count = DEFAULT_FRAME_COUNT;
                break;
            case 'f':
                switch (*optarg) {
                    case '1':
                        cam->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
                        break;
                    case '2':
                        cam->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
                        break;
                    case '0': /* default, fall through */
                    default:
                        cam->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
                }
                break;
            default:
                help();
                goto out_free;
        }
    }

    if (cam->gui_mode) {
        cam->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    }

    if (init_device(cam))
        goto out_free;

    if (!cam->gui_mode) {
        mainloop_noui(cam);
    } else {
        cam->window = window_create(cam->fmt->fmt.pix.width, cam->fmt->fmt.pix.height);
        mainloop_gui(cam);
        window_destory(cam->window);
    }

    deinit_device(cam);

out_free:
    free_v4l2_camera(cam);
    return 0;
}
