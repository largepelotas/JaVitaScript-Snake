/*
 * Phase 0 smoke test (PLAN.md 2.7).
 *
 * Two things are proven here, both of which the whole project depends on:
 *   1. SDL2 initialises, opens a 960x544 window, and clears it for 60 frames.
 *   2. SDL_CreateSoftwareRenderer + SDL_SaveBMP work, which is the headless
 *      verification path from PLAN.md 5.3 -- the only visual feedback loop
 *      this project gets. Better to find it broken now than in Phase 3.
 *
 * A missing display is not a failure: WSLg may be absent, and headless is the
 * primary verification path anyway. In that case we fall back to the dummy
 * video driver and still exercise the software renderer.
 */
#include <SDL.h>
#include <stdio.h>

#define VITA_W 960
#define VITA_H 544

static int save_software_frame(const char *path)
{
    SDL_Surface  *surf;
    SDL_Renderer *ren;
    SDL_Rect      block;

    surf = SDL_CreateRGBSurfaceWithFormat(0, VITA_W, VITA_H, 32,
                                          SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        fprintf(stderr, "CreateRGBSurface: %s\n", SDL_GetError());
        return 0;
    }
    ren = SDL_CreateSoftwareRenderer(surf);
    if (!ren) {
        fprintf(stderr, "CreateSoftwareRenderer: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return 0;
    }

    SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(ren);
    /* One green block, so a wrong-format or all-black save is obvious. */
    SDL_SetRenderDrawColor(ren, 0x33, 0xCC, 0x33, 0xFF);
    block.x = 40; block.y = 40; block.w = 20; block.h = 20;
    SDL_RenderFillRect(ren, &block);
    SDL_RenderPresent(ren);

    if (SDL_SaveBMP(surf, path) != 0) {
        fprintf(stderr, "SDL_SaveBMP: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_FreeSurface(surf);
        return 0;
    }

    SDL_DestroyRenderer(ren);
    SDL_FreeSurface(surf);
    return 1;
}

int main(int argc, char **argv)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    int           frame;
    int           windowed = 1;

    (void)argc; (void)argv;

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init(video) failed: %s -- retrying with dummy driver\n",
               SDL_GetError());
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init dummy also failed: %s\n", SDL_GetError());
            return 1;
        }
        windowed = 0;
    }
    printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());

    win = SDL_CreateWindow("vita-snake smoke", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, VITA_W, VITA_H,
                           SDL_WINDOW_SHOWN);
    if (!win) {
        printf("CreateWindow failed (%s) -- continuing headless\n",
               SDL_GetError());
        windowed = 0;
    }

    if (windowed) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren)
            ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        if (!ren) {
            fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError());
            return 1;
        }
        {
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(ren, &info) == 0)
                printf("renderer: %s\n", info.name);
        }
        for (frame = 0; frame < 60; frame++) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) { /* drain so the WM stays happy */ }
            SDL_SetRenderDrawColor(ren, 0x00, 0x00, (Uint8)(frame * 4), 0xFF);
            SDL_RenderClear(ren);
            SDL_RenderPresent(ren);
        }
        printf("60 frames presented to a window\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    } else {
        printf("no window available (expected without WSLg); "
               "headless path is what matters\n");
    }

    if (!save_software_frame("artifacts/smoke_software.bmp")) {
        fprintf(stderr, "FAIL: software renderer / SaveBMP path is broken\n");
        SDL_Quit();
        return 1;
    }
    printf("wrote artifacts/smoke_software.bmp (%dx%d)\n", VITA_W, VITA_H);

    SDL_Quit();
    printf("PHASE 0 DESKTOP SMOKE: OK\n");
    return 0;
}
