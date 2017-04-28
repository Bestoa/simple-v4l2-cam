#include "window.h"
#include "log.h"
#include "camera.h"
#include "util.h"

struct window * window_create(int width, int height)
{
    struct window *window = NULL;

    LOGI("Create window\n");

    if (width <= 0 || height <= 0) {
        LOGE(NO_DUMP_ERRNO, "Width or height is invaild.\n");
        goto err_return;
    }

    window = malloc(sizeof(struct window));
    if (window == NULL) {
        LOGE(NO_DUMP_ERRNO, "Out of memory\n");
        goto err_return;
    }

    if (SDL_Init(SDL_INIT_VIDEO)) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        goto free_window;
    }

    window->sdl_window = SDL_CreateWindow("Tiny Camera",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);

    if (window->sdl_window == NULL) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        goto free_window;
    }

    window->sdl_renderer = SDL_CreateRenderer(window->sdl_window, -1, SDL_RENDERER_SOFTWARE);
    if (window->sdl_renderer == NULL) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        goto free_sdl_window;
    }
    window->sdl_texture = SDL_CreateTexture(window->sdl_renderer, SDL_PIXELFORMAT_YUY2,
            SDL_TEXTUREACCESS_STREAMING, width, height);
    if (window->sdl_texture == NULL) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        goto free_sdl_renderer;
    }

    return window;

free_sdl_renderer:
    SDL_DestroyRenderer(window->sdl_renderer);
free_sdl_window:
    SDL_DestroyWindow(window->sdl_window);
free_window:
    free(window);
err_return:
    return NULL;
}

int window_update_frame(struct window *window, void *addr, size_t size)
{
    void *pixels;
    static int i = 0;
    int pitch;
    SDL_Event event;
    struct time_recorder tr;

    if (!window) {
        LOGE(NO_DUMP_ERRNO, "Invaild window\n");
        return ACTION_STOP;
    }
    if (!addr || size <= 0) {
        LOGE(NO_DUMP_ERRNO, "Invaild address or size\n");
        return ACTION_STOP;
    }

    time_recorder_start(&tr);
    if (SDL_LockTexture(window->sdl_texture, NULL, &pixels, &pitch)) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        return ACTION_STOP;
    }
    memcpy(pixels, addr, size);
    SDL_UnlockTexture(window->sdl_texture);
    if (SDL_RenderCopy(window->sdl_renderer, window->sdl_texture, NULL, NULL)) {
        LOGE(NO_DUMP_ERRNO, "%s", SDL_GetError());
        return ACTION_STOP;
    }
    SDL_RenderPresent(window->sdl_renderer);
    time_recorder_end(&tr);
    time_recorder_print_time(&tr, "Display frame");

    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            return ACTION_STOP;
        }
        if (event.type == SDL_KEYUP) {
            if (event.key.keysym.scancode == SDL_SCANCODE_S) {
                LOGD("Save current frame\n");
                return ACTION_SAVE_PICTURE;
            }
        }
    }
    return 0;
}

void window_destory(struct window *window)
{
    LOGI("Destory window\n");
    SDL_DestroyTexture(window->sdl_texture);
    SDL_DestroyRenderer(window->sdl_renderer);
    SDL_DestroyWindow(window->sdl_window);
    SDL_Quit();
    free(window);
}
