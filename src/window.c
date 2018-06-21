#ifdef __HAS_GUI__
#include <SDL_image.h>

#include "window.h"
#include "log.h"
#include "camera.h"
#include "util.h"
#include "demo.h"

struct window * window_create(int width, int height)
{
    struct window *window = NULL;

    LOGI("Create window\n");

    if (width <= 0 || height <= 0) {
        LOGE(DUMP_NONE, "Width or height is invaild.\n");
        goto err_return;
    }

    window = malloc(sizeof(struct window));
    if (window == NULL) {
        LOGE(DUMP_NONE, "Out of memory\n");
        goto err_return;
    }

    if (SDL_Init(SDL_INIT_VIDEO)) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        goto free_window;
    }

    window->sdl_window = SDL_CreateWindow("Tiny Camera",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);

    if (window->sdl_window == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        goto free_window;
    }

    window->sdl_renderer = SDL_CreateRenderer(window->sdl_window, -1, SDL_RENDERER_SOFTWARE);
    if (window->sdl_renderer == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        goto free_sdl_window;
    }

    window->width = width;
    window->height = height;

    return window;

free_sdl_window:
    SDL_DestroyWindow(window->sdl_window);
free_window:
    free(window);
err_return:
    return NULL;
}

static int draw_yuyv(struct window *window, void *addr, size_t size)
{
    void *pixels;
    int pitch, ret = CAMERA_RETURN_SUCCESS;
    struct SDL_Texture *texture = NULL;
    texture = SDL_CreateTexture(window->sdl_renderer, SDL_PIXELFORMAT_YUY2,
            SDL_TEXTUREACCESS_STREAMING, window->width, window->height);
    if (texture == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        return CAMERA_RETURN_FAILURE;
    }
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        ret = CAMERA_RETURN_FAILURE;
        goto out;
    }
    memcpy(pixels, addr, size);
    SDL_UnlockTexture(texture);
    if (SDL_RenderCopy(window->sdl_renderer, texture, NULL, NULL)) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        ret = CAMERA_RETURN_FAILURE;
        goto out;
    }
    SDL_RenderPresent(window->sdl_renderer);
out:
    SDL_DestroyTexture(texture);
    return ret;
}

static int draw_mjpeg(struct window *window, void *addr, size_t size)
{
    int ret = CAMERA_RETURN_SUCCESS;
    SDL_RWops *rw = NULL;
    SDL_Surface *image = NULL;
    struct SDL_Texture *texture = NULL;

    IMG_Init(IMG_INIT_JPG);

    rw = SDL_RWFromMem(addr, size);
    if (rw == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        return CAMERA_RETURN_FAILURE;
    }
    image = IMG_Load_RW(rw, 0);
    if (image == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        ret = CAMERA_RETURN_FAILURE;
        goto out;
    }
    texture = SDL_CreateTextureFromSurface(window->sdl_renderer, image);
    if (texture == NULL) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        ret = CAMERA_RETURN_FAILURE;
        goto out;
    }
    if (SDL_RenderCopy(window->sdl_renderer, texture, NULL, NULL)) {
        LOGE(DUMP_NONE, "%s", SDL_GetError());
        ret = CAMERA_RETURN_FAILURE;
        goto out;
    }
    SDL_RenderPresent(window->sdl_renderer);
out:
    IMG_Quit();

    if (texture != NULL)
        SDL_DestroyTexture(texture);
    if (rw != NULL)
        SDL_RWclose(rw);
    if (image != NULL)
        SDL_FreeSurface(image);
    return ret;
}

int window_update_frame(struct window *window, void *addr, size_t size, int format)
{
    int ret;
    struct time_recorder tr;

    if (!window) {
        LOGE(DUMP_NONE, "Invaild window\n");
        return CAMERA_RETURN_FAILURE;
    }
    if (!addr || size <= 0) {
        LOGE(DUMP_NONE, "Invaild address or size\n");
        return CAMERA_RETURN_FAILURE;
    }

    time_recorder_start(&tr);
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
            ret = draw_yuyv(window, addr, size);
            break;
        case V4L2_PIX_FMT_MJPEG:
            ret = draw_mjpeg(window, addr, size);
            break;
        default:
            ret = CAMERA_RETURN_FAILURE;
    }
    time_recorder_end(&tr);
    time_recorder_print_time(&tr, "Display frame");

    return ret;
}

int window_get_event(struct window * window)
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            LOGI("Exit.\n");
            return ACTION_STOP;
        }
        if (event.type == SDL_KEYUP) {
            if (event.key.keysym.scancode == SDL_SCANCODE_S) {
                LOGD("Save current frame\n");
                return ACTION_SAVE_PICTURE;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_E) {
                LOGI("Edit control\n");
                return ACTION_EDIT_CONTROL;
            }
        }
    }
    return ACTION_NONE;
}

void window_destory(struct window *window)
{
    LOGI("Destory window\n");
    SDL_DestroyRenderer(window->sdl_renderer);
    SDL_DestroyWindow(window->sdl_window);
    SDL_Quit();
    free(window);
}
#endif
