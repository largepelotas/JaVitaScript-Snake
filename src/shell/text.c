/*
 * SDL2_ttf implementation of the text interface.
 *
 * Rendered strings are cached per font. Without a cache every frame would
 * rebuild a texture for the HUD and for every overlay line; that is invisible
 * on the host and wasteful on the Vita, and the strings involved are few and
 * repeat constantly. Eviction is round-robin because the working set (two HUD
 * labels plus one overlay) never approaches the cache size.
 */
#include "text.h"

#include <SDL_ttf.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_CACHE_SLOTS 32
#define TEXT_CACHE_MAX   96 /* longest cacheable string, bytes */
#define TEXT_MAX_LINE    256

typedef struct {
    char          str[TEXT_CACHE_MAX];
    SDL_Color     color;
    SDL_Renderer *ren;
    SDL_Texture  *tex;
    int           w, h;
} CacheSlot;

struct TextFont {
    TTF_Font *font;
    CacheSlot cache[TEXT_CACHE_SLOTS];
    int       next; /* round-robin eviction cursor */
};

static int g_inited;

bool text_init(void)
{
    if (g_inited) {
        return true;
    }
    if (TTF_Init() != 0) {
        return false;
    }
    g_inited = 1;
    return true;
}

void text_shutdown(void)
{
    if (g_inited) {
        TTF_Quit();
        g_inited = 0;
    }
}

TextFont *text_open(const char *path, int px)
{
    TextFont *f = calloc(1, sizeof *f);

    if (!f) {
        SDL_SetError("out of memory");
        return NULL;
    }
    f->font = TTF_OpenFont(path, px);
    if (!f->font) {
        free(f);
        return NULL;
    }
    return f;
}

static void cache_clear(TextFont *f)
{
    int i;

    for (i = 0; i < TEXT_CACHE_SLOTS; i++) {
        if (f->cache[i].tex) {
            SDL_DestroyTexture(f->cache[i].tex);
            f->cache[i].tex = NULL;
        }
        f->cache[i].str[0] = '\0';
    }
}

void text_close(TextFont *f)
{
    if (!f) {
        return;
    }
    cache_clear(f);
    if (f->font) {
        TTF_CloseFont(f->font);
    }
    free(f);
}

int text_line_height(const TextFont *f)
{
    return f ? TTF_FontLineSkip(f->font) : 0;
}

int text_width(TextFont *f, const char *s)
{
    int w = 0, h = 0;

    if (!f || !s || !*s) {
        return 0;
    }
    if (TTF_SizeUTF8(f->font, s, &w, &h) != 0) {
        return 0;
    }
    return w;
}

static bool same_color(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/*
 * Returns a texture for (s, color), either from the cache or freshly built.
 * *owned is set when the caller must destroy it, which happens only for strings
 * too long to cache.
 */
static SDL_Texture *acquire(TextFont *f, SDL_Renderer *r, const char *s,
                            SDL_Color color, int *w, int *h, bool *owned)
{
    SDL_Surface *surf;
    SDL_Texture *tex;
    CacheSlot   *slot;
    size_t       len = strlen(s);
    int          i;

    *owned = false;

    if (len < TEXT_CACHE_MAX) {
        for (i = 0; i < TEXT_CACHE_SLOTS; i++) {
            slot = &f->cache[i];
            if (slot->tex && slot->ren == r && same_color(slot->color, color) &&
                strcmp(slot->str, s) == 0) {
                *w = slot->w;
                *h = slot->h;
                return slot->tex;
            }
        }
    }

    surf = TTF_RenderUTF8_Blended(f->font, s, color);
    if (!surf) {
        return NULL;
    }
    tex = SDL_CreateTextureFromSurface(r, surf);
    *w  = surf->w;
    *h  = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) {
        return NULL;
    }

    if (len >= TEXT_CACHE_MAX) {
        *owned = true;
        return tex;
    }

    slot = &f->cache[f->next];
    f->next = (f->next + 1) % TEXT_CACHE_SLOTS;
    if (slot->tex) {
        SDL_DestroyTexture(slot->tex);
    }
    memcpy(slot->str, s, len + 1);
    slot->color = color;
    slot->ren   = r;
    slot->tex   = tex;
    slot->w     = *w;
    slot->h     = *h;
    return tex;
}

void text_draw(SDL_Renderer *r, TextFont *f, const char *s, int x, int y,
               SDL_Color color)
{
    SDL_Texture *tex;
    SDL_Rect     dst;
    bool         owned;

    if (!r || !f || !s || !*s) {
        return;
    }
    tex = acquire(f, r, s, color, &dst.w, &dst.h, &owned);
    if (!tex) {
        return;
    }
    dst.x = x;
    dst.y = y;
    SDL_RenderCopy(r, tex, NULL, &dst);
    if (owned) {
        SDL_DestroyTexture(tex);
    }
}

void text_draw_center(SDL_Renderer *r, TextFont *f, const char *s, int cx,
                      int y, SDL_Color color)
{
    text_draw(r, f, s, cx - text_width(f, s) / 2, y, color);
}

int text_draw_wrapped(SDL_Renderer *r, TextFont *f, const char *s, int cx,
                      int y, int max_w, SDL_Color color)
{
    char line[TEXT_MAX_LINE];
    char word[TEXT_MAX_LINE];
    int  line_h = text_line_height(f);
    int  drawn  = 0;
    const char *p = s;

    /* r may be NULL: text_draw ignores it and the return value is then a pure
     * measurement, which is how an auto-height dialog box is sized before it is
     * drawn. */
    if (!f || !s) {
        return 0;
    }

    line[0] = '\0';
    while (*p) {
        size_t n = 0;
        char   candidate[TEXT_MAX_LINE * 2];

        while (*p == ' ') {
            p++;
        }
        while (*p && *p != ' ' && n + 1 < sizeof word) {
            word[n++] = *p++;
        }
        word[n] = '\0';
        if (n == 0) {
            break;
        }

        if (line[0] == '\0') {
            SDL_strlcpy(candidate, word, sizeof candidate);
        } else {
            SDL_snprintf(candidate, sizeof candidate, "%s %s", line, word);
        }

        if (line[0] != '\0' && text_width(f, candidate) > max_w) {
            text_draw_center(r, f, line, cx, y + drawn, color);
            drawn += line_h;
            SDL_strlcpy(line, word, sizeof line);
        } else {
            SDL_strlcpy(line, candidate, sizeof line);
        }
    }

    if (line[0] != '\0') {
        text_draw_center(r, f, line, cx, y + drawn, color);
        drawn += line_h;
    }
    return drawn;
}
