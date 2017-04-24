#include "window.h"

struct window * window_create(int width, int height) {
    struct window *window = NULL;

    window = malloc(sizeof(struct window));

	SDL_Init(SDL_INIT_VIDEO);

	window->sdl_window = SDL_CreateWindow("Tiny Camera",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height, SDL_WINDOW_SHOWN);

    window->sdl_renderer = SDL_CreateRenderer(window->sdl_window, -1, 0);
    window->sdl_texture = SDL_CreateTexture(window->sdl_renderer, SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, width, height);

    return window;
}

void window_update_frame(struct window *window, void *addr, int size) {
    void *pixels;
    static int i = 0;
    int pitch;
	SDL_Event event;
    SDL_LockTexture(window->sdl_texture, NULL, &pixels, &pitch);
    memcpy(pixels, addr, size);
    SDL_UnlockTexture(window->sdl_texture);
    SDL_RenderClear(window->sdl_renderer);
    SDL_RenderCopy(window->sdl_renderer, window->sdl_texture, NULL, NULL);
    SDL_RenderPresent(window->sdl_renderer);
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            exit(0);
        }
    }
}

void window_destory(struct window *window) {
	SDL_DestroyWindow(window->sdl_window);
	SDL_Quit();
    free(window);
}
