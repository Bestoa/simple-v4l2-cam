#ifndef __WINDOW_TC__
#define __WINDOW_TC__
#include <SDL.h>
struct window {
	SDL_Window *sdl_window;
    SDL_Renderer *sdl_renderer;
    SDL_Texture *sdl_texture;
};

struct window *window_create(int width, int height);
void window_update_frame(struct window *window, void *addr, int size);
void window_destory(struct window *window);
#endif
