#include "camera.h"
#include "api.h"
#include "util.h"
#include "window.h"
#include "log.h"
#include "demo.h"

static int read_frame(struct v4l2_camera *cam, int count, int usage)
{
    struct v4l2_buffer buffer_info;
    struct buffer buffer;
    struct time_recorder tr;
    int ret;

    //Count the time of get one frame
    time_recorder_start(&tr);
    ret = camera_dequeue_buffer(cam, &buffer_info);
    if (ret == -EAGAIN) return -EAGAIN;
    else if (ret) return ACTION_STOP;
    time_recorder_end(&tr);
    time_recorder_print_time(&tr, "Get frame");
    camera_get_buffer(cam, &buffer_info, &buffer);
    if (usage & FRAMEUSAGE_DISPLAY) {
        ret = window_update_frame((struct window *)cam->priv, buffer.addr, buffer.size);
    }
    if (ret == ACTION_STOP || ret == ACTION_EDIT_CONTROL)
        goto out_queue_buffer;
    if (ret == ACTION_SAVE_PICTURE)
        usage |= FRAMEUSAGE_SAVE;
    if (usage & FRAMEUSAGE_SAVE) {
        if (save_output(buffer.addr, buffer.size, count, fmt2desc(cam->fmt.fmt.pix.pixelformat))){
            ret = ACTION_STOP;
            goto out_queue_buffer;
        }
    }
    ret = camera_queue_buffer(cam, &buffer_info);
    if (ret) return ACTION_STOP;

    return CAMREA_RETURN_SUCCESS;
out_queue_buffer:
    camera_queue_buffer(cam, &buffer_info);
    return ret;
}

static void mainloop_noui(struct v4l2_camera *cam)
{
    int i = 0, ret, count = DEFAULT_FRAME_COUNT;
    if (cam->priv != NULL)
    {
        count = *(int *)cam->priv;
    }
    if (camera_start_capturing(cam))
        return;
    while(i++ < count)
    {
        /* EAGAIN - continue select loop. */
        while((ret = read_frame(cam, i, FRAMEUSAGE_SAVE)) == EAGAIN);
        if (ret == ACTION_STOP)
            break;
    }
    camera_stop_capturing(cam);
}

static void edit_control(struct v4l2_camera *cam)
{
    struct v4l2_control ctrl;
    int cur_level = get_log_level();
    int c;
    set_log_level(DEBUG);
    camera_query_support_control(cam);
    scanf("%d%x%d", &c, &ctrl.id, &ctrl.value);
    if (c == 1)
        camera_set_control(cam, &ctrl);
    else
        camera_get_control(cam, &ctrl);
    LOGI("%d\n", ctrl.value);
    set_log_level(cur_level);
}

static void mainloop(struct v4l2_camera *cam)
{
    int ret;
    camera_start_capturing(cam);
    while (1) {
        while((ret = read_frame(cam, -1, FRAMEUSAGE_DISPLAY)) == EAGAIN);
        if (ret == ACTION_STOP)
            break;
        else if (ret == ACTION_EDIT_CONTROL)
            edit_control(cam);
    }
    camera_stop_capturing(cam);
}

int main(int argc, char **argv)
{
    int opt, has_gui = 0, count;
    struct v4l2_camera *cam = NULL;

    cam = camera_create_object();
    if (!cam) {
        LOGE(DUMP_NONE, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    LOGI("Parsing command line args:\n");
    while ((opt = getopt(argc, argv, "?vgp:w:h:f:n:")) != -1) {
        switch(opt){
            case 'v':
                LOGI("Verbose log\n");
                set_log_level(DEBUG);
                break;
            case 'g':
                LOGI("Gui mode\n");
                has_gui = 1;
                break;
            case 'p':
                cam->dev_name = optarg;
                LOGI("Device path: %s\n", cam->dev_name);
                break;
            case 'w':
                cam->fmt.fmt.pix.width = atoi(optarg);
                LOGI("Width: %d\n", cam->fmt.fmt.pix.width);
                break;
            case 'h':
                cam->fmt.fmt.pix.height = atoi(optarg);
                LOGI("Height: %d\n", cam->fmt.fmt.pix.height);
                break;
            case 'n':
                if ((count = atoi(optarg)) <= 0)
                    count = DEFAULT_FRAME_COUNT;
                cam->priv = &count;
                LOGI("Frame total: %d\n", *(int *)cam->priv);
                break;
            case 'f':
                switch (*optarg) {
                    case '1':
                        cam->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
                        break;
                    case '2':
                        cam->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
                        break;
                    case '0': /* default, fall through */
                    default:
                        cam->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
                }
                LOGI("Format: %d\n", cam->fmt.fmt.pix.pixelformat);
                break;
            case '?':
            default:
                help();
                goto out_free;
        }
    }
    LOGI("Parsing command line args done\n");
    if (has_gui) {
        cam->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    }
    if (camera_open_device(cam))
        goto out_free;
    if (camera_query_cap(cam))
        goto out_close;
    if(!(cam->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOGE(DUMP_NONE, "%s is no video capture device\n", cam->dev_name);
        goto out_close;
    }
    if(!(cam->cap.capabilities & V4L2_CAP_STREAMING))
    {
        LOGE(DUMP_NONE, "%s does not support streaming i/o\n", cam->dev_name);
        goto out_close;
    }
    camera_query_support_control(cam);
    camera_query_support_format(cam);

    if (camera_set_output_format(cam))
        goto out_close;

    /* Note VIDIOC_S_FMT may change width and height. */
    camera_get_output_format(cam);

    if (camera_request_and_map_buffer(cam))
        goto out_close;

    if (!has_gui) {
        mainloop_noui(cam);
    } else {
        cam->priv = window_create(cam->fmt.fmt.pix.width, cam->fmt.fmt.pix.height);
        mainloop(cam);
        window_destory((struct window *)cam->priv);
    }

    camera_return_and_unmap_buffer(cam);

out_close:
    camera_close_device(cam);
out_free:
    camera_free_object(cam);
    return CAMREA_RETURN_SUCCESS;
}
