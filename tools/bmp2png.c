/*
 * BMP -> PNG converter (PLAN.md 5.3).
 *
 * SDL_SaveBMP is the only image writer in base SDL2, but BMP is awkward to
 * view in most tooling. PLAN.md 5.3 calls for PNG, so the headless pipeline
 * ends here: render to a surface, SDL_SaveBMP, convert, look at the result.
 *
 * This is deliberately a separate host-only tool rather than a link-time
 * dependency of the game: SDL2_image is confirmed present on the host, but
 * its availability for the Vita target is still unverified (PLAN.md 11.2),
 * and nothing in the shipped build should depend on it.
 *
 * Build:  gcc -std=c99 tools/bmp2png.c -o build-host/bmp2png \
 *             $(pkg-config --cflags --libs sdl2 SDL2_image)
 * Usage:  bmp2png in.bmp out.png
 */
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    SDL_Surface *surf;
    int          rc = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <in.bmp> <out.png>\n", argv[0]);
        return 2;
    }

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    surf = SDL_LoadBMP(argv[1]);
    if (!surf) {
        fprintf(stderr, "SDL_LoadBMP(%s): %s\n", argv[1], SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (IMG_SavePNG(surf, argv[2]) != 0) {
        fprintf(stderr, "IMG_SavePNG(%s): %s\n", argv[2], IMG_GetError());
        rc = 1;
    } else {
        printf("%s -> %s (%dx%d)\n", argv[1], argv[2], surf->w, surf->h);
    }

    SDL_FreeSurface(surf);
    SDL_Quit();
    return rc;
}
