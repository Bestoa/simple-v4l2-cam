#include "camera.h"
#include "window.h"
#include "util.h"
#include "log.h"

static int v4l2_queue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info)
{
    if(xioctl(cam->fd, VIDIOC_QBUF, buffer_info)) {
        LOGE(DUMP_ERROR, "Queue buffer failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

static int v4l2_dequeue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info)
{
    ZAP(*buffer_info);
    buffer_info->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_info->memory = V4L2_MEMORY_MMAP;
    if(xioctl(cam->fd, VIDIOC_DQBUF, buffer_info))
    {
        switch(errno)
        {
            case EAGAIN:
                return -EAGAIN;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                LOGE(DUMP_ERROR, "dequeue buffer failed\n");
                return -EIO;
        }
    }
    return CAMERA_RETURN_SUCCESS;
}

static int v4l2_get_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info, struct buffer *buffer)
{
    // Just get the buffer address and size, don't change it directly.
    assert(buffer_info->index < cam->bufq.count);
    buffer->addr = cam->bufq.buf[buffer_info->index].addr;
    // For compressed format such as MJPEG, it will not use whole buffer.
    if (buffer_info->bytesused)
        buffer->size = buffer_info->bytesused;
    else
        buffer->size = cam->bufq.buf[buffer_info->index].size;
    return CAMERA_RETURN_SUCCESS;
}

static int v4l2_start_capturing(struct v4l2_camera *cam)
{
    unsigned int i;
    enum v4l2_buf_type type;

    LOGI("Stream on\n");
    for(i = 0; i < cam->bufq.count; i++)
    {
        struct v4l2_buffer buffer_info;

        ZAP(buffer_info);
        buffer_info.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_info.memory      = V4L2_MEMORY_MMAP;
        buffer_info.index       = i;
        if (v4l2_queue_buffer(cam, &buffer_info))
            return CAMERA_RETURN_FAILURE;
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(cam->fd, VIDIOC_STREAMON, &type)) {
        LOGE(DUMP_ERROR, "Stream on failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

static void v4l2_stop_capturing (struct v4l2_camera *cam)
{
    enum v4l2_buf_type type;

    LOGI("Strem off\n");
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(cam->fd, VIDIOC_STREAMOFF, &type)) {
        LOGE(DUMP_ERROR, "Stream off failed\n");
    }
}

static int v4l2_request_and_map_buffer(struct v4l2_camera *cam)
{
    struct v4l2_requestbuffers req;
    int i;

    LOGI("Request and map buffer\n");
    ZAP(req);
    req.count               = MAX_BUFFER_NUM;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;
    if(xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
        LOGE(DUMP_ERROR, "Request buffer failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    LOGI("Buffer count: %d\n", req.count);
    if(req.count < MIN_BUFFER_NUM)
    {
        LOGE(DUMP_NONE, "Insufficient buffer memory on %s\n", cam->dev_name);
        return CAMERA_RETURN_FAILURE;
    }
    cam->bufq.buf = calloc(req.count, sizeof(struct buffer));
    if(!cam->bufq.buf)
    {
        LOGE(DUMP_NONE, "Out of memory\n");
        goto out_return_buffer;
    }
    cam->bufq.count = req.count;
    for(i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buffer_info;
        ZAP(buffer_info);
        buffer_info.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_info.memory      = V4L2_MEMORY_MMAP;
        buffer_info.index       = i;
        if(xioctl(cam->fd, VIDIOC_QUERYBUF, &buffer_info)) {
            LOGE(DUMP_ERROR, "Query [%d] buffer failed\n", i);
            goto out_unmap_buffer;
        }
        cam->bufq.buf[i].size = buffer_info.length;
        cam->bufq.buf[i].addr = mmap(NULL /* start anywhere */,
                buffer_info.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                cam->fd, buffer_info.m.offset);
        if(MAP_FAILED == cam->bufq.buf[i].addr) {
            LOGE(DUMP_ERROR, "Mmap failed\n");
            goto out_unmap_buffer;
        }
    }
    return CAMERA_RETURN_SUCCESS;
out_unmap_buffer:
    while(--i >= 0) {
        munmap(cam->bufq.buf[i].addr, cam->bufq.buf[i].size);
    }
out_free_buffer:
    free(cam->bufq.buf);
out_return_buffer:
    req.count = 0;
    if(xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
        LOGE(DUMP_ERROR, "Return buffer failed\n");
    }
    LOGI("Buffer count: %d\n", req.count);
    return CAMERA_RETURN_FAILURE;
}

static void v4l2_return_and_unmap_buffer(struct v4l2_camera *cam)
{
    unsigned int i;
    struct v4l2_requestbuffers req;

    LOGI("Return and unmap buffer\n");
    for(i = 0; i < cam->bufq.count; i++)
        munmap(cam->bufq.buf[i].addr, cam->bufq.buf[i].size);
    ZAP(req);
    req.count               = 0;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;
    if (xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
        LOGE(DUMP_ERROR, "Return buffer failed\n");
    }
    LOGI("Buffer count: %d\n", req.count);
    if (cam->bufq.buf) {
        free(cam->bufq.buf);
    }
}

static int v4l2_open_device(struct v4l2_camera *cam)
{
    struct stat st;

    LOGI("Open device %s\n", cam->dev_name);
    if(-1 == stat(cam->dev_name, &st))
    {
        LOGE(DUMP_ERROR, "Cannot identify '%s'\n", cam->dev_name);
        return CAMERA_RETURN_FAILURE;
    }
    if(!S_ISCHR(st.st_mode))
    {
        LOGE(DUMP_NONE, "%s is not char device\n", cam->dev_name);
        return CAMERA_RETURN_FAILURE;
    }
    cam->fd = open(cam->dev_name, O_RDWR /* required */ , 0);
    if(-1 == cam->fd)
    {
        LOGE(DUMP_ERROR, "Cannot open '%s'\n", cam->dev_name);
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

static void v4l2_close_device(struct v4l2_camera *cam)
{
    LOGI("Close device\n");
    if(-1 == close(cam->fd)) {
        LOGE(DUMP_ERROR, "Close device failed\n");
    }
    cam->fd = -1;
}

static int v4l2_query_cap(struct v4l2_camera *cam)
{
    if(xioctl(cam->fd, VIDIOC_QUERYCAP, &cam->cap))
    {
        LOGE(DUMP_ERROR, "Query cap failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    LOGD("Dump capability:\n");
    LOGD("\tdriver:         %s\n", cam->cap.driver);
    LOGD("\tcard:           %s\n", cam->cap.card);
    LOGD("\tbus info:       %s\n", cam->cap.bus_info);
    LOGD("\tversion:        %u.%u.%u\n", (cam->cap.version >> 16) & 0xFF, (cam->cap.version >> 8) & 0xFF, cam->cap.version & 0xFF);
    LOGD("\tcapability list:\n");
#define DUMP_CAP(x) do { if (x & cam->cap.capabilities) LOGD("\t\t"#x"\n"); } while (0)
    DUMP_CAP(V4L2_CAP_VIDEO_CAPTURE);
    DUMP_CAP(V4L2_CAP_VIDEO_CAPTURE_MPLANE);
    DUMP_CAP(V4L2_CAP_VIDEO_OUTPUT);
    DUMP_CAP(V4L2_CAP_VIDEO_OUTPUT_MPLANE);
    DUMP_CAP(V4L2_CAP_VIDEO_M2M);
    DUMP_CAP(V4L2_CAP_VIDEO_M2M_MPLANE);
    DUMP_CAP(V4L2_CAP_VIDEO_OVERLAY);
    DUMP_CAP(V4L2_CAP_VBI_CAPTURE);
    DUMP_CAP(V4L2_CAP_VBI_OUTPUT);
    DUMP_CAP(V4L2_CAP_SLICED_VBI_CAPTURE);
    DUMP_CAP(V4L2_CAP_SLICED_VBI_OUTPUT);
    DUMP_CAP(V4L2_CAP_RDS_CAPTURE);
    DUMP_CAP(V4L2_CAP_VIDEO_OUTPUT_OVERLAY);
    DUMP_CAP(V4L2_CAP_HW_FREQ_SEEK);
    DUMP_CAP(V4L2_CAP_RDS_OUTPUT);
    DUMP_CAP(V4L2_CAP_TUNER);
    DUMP_CAP(V4L2_CAP_AUDIO);
    DUMP_CAP(V4L2_CAP_RADIO);
    DUMP_CAP(V4L2_CAP_MODULATOR);
    DUMP_CAP(V4L2_CAP_SDR_CAPTURE);
    DUMP_CAP(V4L2_CAP_EXT_PIX_FORMAT);
    DUMP_CAP(V4L2_CAP_SDR_OUTPUT);
    DUMP_CAP(V4L2_CAP_READWRITE);
    DUMP_CAP(V4L2_CAP_ASYNCIO);
    DUMP_CAP(V4L2_CAP_STREAMING);
    DUMP_CAP(V4L2_CAP_TOUCH);
#undef DUMP_CAP
    return CAMERA_RETURN_SUCCESS;
}

static void v4l2_get_output_format(struct v4l2_camera *cam)
{
    if (xioctl(cam->fd, VIDIOC_G_FMT, &cam->fmt))
    {
        LOGE(DUMP_ERROR, "Get format failed\n");
        return;
    }
    LOGI("Output foramt:\n");
    LOGI("\twidth:          %d\n", cam->fmt.fmt.pix.width);
    LOGI("\theight:         %d\n", cam->fmt.fmt.pix.height);
    LOGI("\tpix format      %s\n", fmt2desc(cam->fmt.fmt.pix.pixelformat));
    LOGI("\tbytesperline    %d\n", cam->fmt.fmt.pix.bytesperline);
    LOGI("\tsizeimage       %d\n", cam->fmt.fmt.pix.sizeimage);
    LOGI("\tcolorspace      %d\n", cam->fmt.fmt.pix.colorspace);
}

static int v4l2_set_output_format(struct v4l2_camera *cam)
{
    LOGI("Set format\n");
    if(xioctl(cam->fd, VIDIOC_S_FMT, &cam->fmt)) {
        LOGE(DUMP_ERROR, "set format failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

static void v4l2_query_support_format(struct v4l2_camera *cam)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    LOGD("Query support format:\n");
    while (xioctl(cam->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        LOGD("Support pixel format[%d]: %s\n", fmtdesc.index, fmtdesc.description);
        fmtdesc.index++;
    }
}

static void enumerate_menu(struct v4l2_camera *cam, struct v4l2_queryctrl ctrl, int id)
{
    struct v4l2_querymenu menu;

    LOGD("\tMenu items:\n");
    ZAP(menu);
    menu.id = id;
    for (menu.index = ctrl.minimum; menu.index <= ctrl.maximum; menu.index++) {
        if (!ioctl(cam->fd, VIDIOC_QUERYMENU, &menu)) {
            LOGD("\t\t[%d]", menu.index);
            if (ctrl.type == V4L2_CTRL_TYPE_MENU)
                LOGD("%s\n", menu.name);
            else
                LOGD("%lld\n", menu.value);
        }
    }
}

static void v4l2_query_support_control(struct v4l2_camera *cam)
{
    struct v4l2_queryctrl ctrl;

    ZAP(ctrl);
    ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (!ioctl(cam->fd, VIDIOC_QUERYCTRL, &ctrl)) {
        LOGD("[0x%X]Control %s: min %d, max %d, default value %d, step %d, flags 0x%x\n",
                ctrl.id, ctrl.name, ctrl.minimum, ctrl.maximum,
                ctrl.default_value, ctrl.step, ctrl.flags);
        if (ctrl.type == V4L2_CTRL_TYPE_MENU || ctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU)
            enumerate_menu(cam, ctrl, ctrl.id);

        ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
    if (errno != EINVAL) {
        LOGE(DUMP_ERROR, "Query control failed\n");
    }
}


static struct v4l2_camera *v4l2_alloc_camera_object()
{
    struct v4l2_camera * cam = NULL;

    cam = malloc(sizeof(struct v4l2_camera));
    if (!cam) {
        return NULL;
    }
    ZAP(*cam);
    cam->dev_name = DEFAULT_DEVICE;
    cam->fmt.type                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->fmt.fmt.pix.width         = DEFAULT_IMAGE_WIDTH;
    cam->fmt.fmt.pix.height        = DEFAULT_IMAGE_HEIGHT;
    cam->fmt.fmt.pix.pixelformat   = V4L2_PIX_FMT_YUYV;
    cam->fmt.fmt.pix.field         = V4L2_FIELD_ANY;
    return cam;
}

static void v4l2_free_camera_object(struct v4l2_camera *cam)
{
    if (cam) {
        free(cam);
        cam = NULL;
    }
}

static int v4l2_get_control(struct v4l2_camera *cam, struct v4l2_control *ctrl)
{
    if(xioctl(cam->fd,  VIDIOC_G_CTRL, ctrl)) {
        LOGE(DUMP_ERROR, "Get control failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

static int v4l2_set_control(struct v4l2_camera *cam, struct v4l2_control *ctrl)
{
    if(xioctl(cam->fd,  VIDIOC_S_CTRL, ctrl)) {
        LOGE(DUMP_ERROR, "Set control failed\n");
        return CAMERA_RETURN_FAILURE;
    }
    return CAMERA_RETURN_SUCCESS;
}

//API part
#define STATE_EQ(x) do { \
    if (cam->state != (x)) { \
        LOGE(DUMP_NONE, "Can't do %s in %s state\n", __func__, camera_state_to_string(cam->state));\
        return CAMERA_RETURN_FAILURE; \
    }\
}while(0)

#define STATE_GE(x) do { \
    if (cam->state < (x) || cam->state == CAMERA_STATE_ERROR) { \
        LOGE(DUMP_NONE, "Can't do %s in %s state\n", __func__, camera_state_to_string(cam->state));\
        return CAMERA_RETURN_FAILURE; \
    }\
}while(0)

#define CHECK_RET(x) do { \
    if ((x) != CAMERA_RETURN_SUCCESS) { \
        LOGE(DUMP_NONE, "Set camera state to CAMERA_STATE_ERROR\n");\
        cam->state = CAMERA_STATE_ERROR; \
        return CAMERA_RETURN_FAILURE; \
    }\
}while(0)

struct v4l2_camera *camera_create_object()
{
    struct v4l2_camera *cam = v4l2_alloc_camera_object();
    if (cam)
        cam->state = CAMREA_STATE_INIT;
    return cam;
}
int camera_free_object(struct v4l2_camera *cam)
{
    v4l2_free_camera_object(cam);
    return CAMERA_RETURN_SUCCESS;
}
int camera_dequeue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info)
{
    int ret;
    STATE_EQ(CAMREA_STATE_STREAM_ON);
    ret = v4l2_dequeue_buffer(cam, buffer_info);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_BUFFER_LOCKED;
    return ret;
}
int camera_queue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info)
{
    int ret;
    STATE_EQ(CAMREA_STATE_BUFFER_LOCKED);
    ret = v4l2_queue_buffer(cam, buffer_info);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_STREAM_ON;
    return ret;
}
int camera_start_capturing(struct v4l2_camera *cam)
{
    int ret;
    STATE_EQ(CAMREA_STATE_BUFFER_MAPPED);
    ret = v4l2_start_capturing(cam);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_STREAM_ON;
    return ret;
}
int camera_stop_capturing(struct v4l2_camera *cam)
{
    STATE_EQ(CAMREA_STATE_STREAM_ON);
    v4l2_stop_capturing(cam);
    cam->state = CAMREA_STATE_BUFFER_MAPPED;
    return CAMERA_RETURN_SUCCESS;
}
int camera_get_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info, struct buffer *buffer)
{
    int ret;
    STATE_EQ(CAMREA_STATE_BUFFER_LOCKED);
    ret = v4l2_get_buffer(cam, buffer_info, buffer);
    CHECK_RET(ret);
    return ret;
}
int camera_request_and_map_buffer(struct v4l2_camera *cam)
{
    int ret;
    STATE_EQ(CAMREA_STATE_CONFIGURED);
    ret = v4l2_request_and_map_buffer(cam);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_BUFFER_MAPPED;
    return ret;
}
int camera_return_and_unmap_buffer(struct v4l2_camera *cam)
{
    STATE_EQ(CAMREA_STATE_BUFFER_MAPPED);
    v4l2_return_and_unmap_buffer(cam);
    cam->state = CAMREA_STATE_OPENED;
    return CAMERA_RETURN_SUCCESS;
}
int camera_open_device(struct v4l2_camera *cam)
{
    int ret;
    STATE_EQ(CAMREA_STATE_INIT);
    ret = v4l2_open_device(cam);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_OPENED;
    return ret;
}
int camera_close_device(struct v4l2_camera *cam)
{
    STATE_GE(CAMREA_STATE_OPENED);
    v4l2_close_device(cam);
    cam->state = CAMREA_STATE_INIT;
    return CAMERA_RETURN_SUCCESS;
}
int camera_query_cap(struct v4l2_camera *cam)
{
    int ret;
    STATE_GE(CAMREA_STATE_OPENED);
    ret = v4l2_query_cap(cam);
    return ret;
}
int camera_query_support_control(struct v4l2_camera *cam)
{
    STATE_GE(CAMREA_STATE_OPENED);
    v4l2_query_support_control(cam);
    return CAMERA_RETURN_SUCCESS;
}
int camera_query_support_format(struct v4l2_camera *cam)
{
    STATE_GE(CAMREA_STATE_OPENED);
    v4l2_query_support_format(cam);
    return CAMERA_RETURN_SUCCESS;
}
int camera_get_output_format(struct v4l2_camera *cam)
{
    STATE_GE(CAMREA_STATE_OPENED);
    v4l2_get_output_format(cam);
    return CAMERA_RETURN_SUCCESS;
}
int camera_set_output_format(struct v4l2_camera *cam)
{
    int ret;
    STATE_EQ(CAMREA_STATE_OPENED);
    ret = v4l2_set_output_format(cam);
    CHECK_RET(ret);
    cam->state = CAMREA_STATE_CONFIGURED;
    return ret;
}
int camera_get_control(struct v4l2_camera *cam, struct v4l2_control *ctrl)
{
    int ret;
    STATE_GE(CAMREA_STATE_OPENED);
    ret = v4l2_get_control(cam, ctrl);
    return ret;
}
int camera_set_control(struct v4l2_camera *cam, struct v4l2_control *ctrl)
{
    int ret;
    STATE_GE(CAMREA_STATE_OPENED);
    ret = v4l2_set_control(cam, ctrl);
    return ret;
}
//API part end
