#ifndef _CAM_TEST_
#define _CAM_TEST_

#define FRAME_NUM	(10)
#define MAX_BUFFER_NUM (8)

#define IMAGE_WIDTH	    (1920)
#define IMAGE_HEIGHT    (1280)

#define ZAP(x) memset (&(x), 0, sizeof (x))

struct buffer {
    void *                  start;
    size_t                  length;
};

#endif
