#ifndef __TINY_CAMERA_API_
#define __TINY_CAMERA_API_
#include <linux/videodev2.h>

struct v4l2_camera *camera_create_object();
int camera_free_object(struct v4l2_camera *cam);
int camera_dequeue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info);
int camera_queue_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info);
int camera_start_capturing(struct v4l2_camera *cam);
int camera_stop_capturing(struct v4l2_camera *cam);
int camera_get_buffer(struct v4l2_camera *cam, struct v4l2_buffer *buffer_info, struct buffer *buffer);
int camera_request_and_map_buffer(struct v4l2_camera *cam);
int camera_return_and_unmap_buffer(struct v4l2_camera *cam);
int camera_open_device(struct v4l2_camera *cam);
int camera_close_device(struct v4l2_camera *cam);
int camera_query_cap(struct v4l2_camera *cam);
int camera_query_support_control(struct v4l2_camera *cam);
int camera_query_support_format(struct v4l2_camera *cam);
int camera_get_output_format(struct v4l2_camera *cam);
int camera_set_output_format(struct v4l2_camera *cam);
int camera_get_control(struct v4l2_camera *cam, struct v4l2_control *ctrl);
int camera_set_control(struct v4l2_camera *cam, struct v4l2_control *ctrl);
#endif
