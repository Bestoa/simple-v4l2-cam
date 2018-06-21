#ifdef __HAS_GUI__
#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>

#define WINDOW_DEFAULT_WIDTH    (720)
#define WINDOW_DEFAULT_HEIGHT   (480)

struct window {
    int width;
    int height;
    SDL_Window *sdl_window;
    SDL_Renderer *sdl_renderer;
};

struct window *window_create(int width, int height);
int window_update_frame(struct window *window, void *addr, size_t size, int format);
int window_get_event(struct window *window);
void window_destory(struct window *window);
#endif
#endif
