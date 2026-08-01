/*
 * Pixel assertions against a saved frame (PLAN.md 5.3).
 *
 * Looking at the screenshots catches the things only eyes catch - crowding,
 * wrong stacking, unreadable text. It does not catch a playfield that is two
 * pixels off, and it does not catch a regression six commits later. This tool
 * covers that half: it asserts named coordinates against the colors
 * MECHANICS.md 8 and 9 specify, so the layout is checked by the same numbers
 * the renderer was written from.
 *
 * Build:  make build-host/pixel_probe
 * Usage:  pixel_probe frame.bmp "x,y=RRGGBB[:label]" ...
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int probe(SDL_Surface *surf, const char *spec)
{
    char     label[64] = "";
    char     hex[16];
    int      x, y;
    Uint32   pixel;
    Uint8    r, g, b;
    unsigned want;
    char    *colon;
    char     work[128];

    SDL_strlcpy(work, spec, sizeof work);
    colon = strchr(work, ':');
    if (colon) {
        *colon = '\0';
        SDL_strlcpy(label, colon + 1, sizeof label);
    }

    if (sscanf(work, "%d,%d=%15s", &x, &y, hex) != 3) {
        fprintf(stderr, "bad probe '%s', want x,y=RRGGBB[:label]\n", spec);
        return 0;
    }
    if (x < 0 || y < 0 || x >= surf->w || y >= surf->h) {
        fprintf(stderr, "probe %s: (%d,%d) is outside %dx%d\n", spec, x, y,
                surf->w, surf->h);
        return 0;
    }

    want  = (unsigned)strtoul(hex, NULL, 16);
    pixel = *(Uint32 *)((Uint8 *)surf->pixels + (size_t)y * (size_t)surf->pitch +
                        (size_t)x * 4u);
    SDL_GetRGB(pixel, surf->format, &r, &g, &b);

    if (((unsigned)r << 16 | (unsigned)g << 8 | (unsigned)b) != want) {
        printf("  FAIL (%d,%d) is %02X%02X%02X, want %06X  %s\n", x, y, r, g, b,
               want, label);
        return 0;
    }
    printf("  ok   (%3d,%3d) %06X  %s\n", x, y, want, label);
    return 1;
}

int main(int argc, char **argv)
{
    SDL_Surface *raw, *surf;
    int          failures = 0;
    int          i;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <frame.bmp> \"x,y=RRGGBB[:label]\" ...\n",
                argv[0]);
        return 2;
    }

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 2;
    }

    raw = SDL_LoadBMP(argv[1]);
    if (!raw) {
        fprintf(stderr, "SDL_LoadBMP(%s): %s\n", argv[1], SDL_GetError());
        SDL_Quit();
        return 2;
    }
    /* Normalise to one known format so the reader below is not guessing at the
     * BMP's channel order. */
    surf = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    if (!surf) {
        fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    printf("%s\n", argv[1]);
    for (i = 2; i < argc; i++) {
        if (!probe(surf, argv[i])) {
            failures++;
        }
    }

    SDL_FreeSurface(surf);
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
