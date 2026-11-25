/*
 * Renderizador 2D por software, direto no framebuffer RGB565 do 3DS.
 * Leve de proposito: no Old 3DS nao usamos GPU (citro2d) para economizar
 * CPU e memoria para o decodificador JPEG.
 *
 * O framebuffer do 3DS e "deitado": 240 pixels por coluna, de baixo p/ cima.
 * pixel(x, y) = px[x * 240 + (239 - y)]
 */
#ifndef KO_GFX_H
#define KO_GFX_H
#include <stdint.h>
#include "fonts.h"

#define KO_SCREEN_H 240

typedef struct {
    uint16_t *px;
    int w;        /* 400 (superior) ou 320 (inferior) */
} ko_surf_t;

typedef struct {
    uint16_t bg, card, card_hi, accent, accent_dim, text, dim, ok, warn;
    const char *name;
} ko_theme_t;

#define KO_THEME_COUNT 5
extern const ko_theme_t ko_themes[KO_THEME_COUNT];
extern const ko_theme_t *T;   /* tema atual */

static inline uint16_t ko_rgb(int r, int g, int b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint16_t ko_blend(uint16_t dst, uint16_t src, int a16); /* a16: 0..16 */

void gfx_clear(ko_surf_t *s, uint16_t c);
void gfx_fill(ko_surf_t *s, int x, int y, int w, int h, uint16_t c);
void gfx_fill_alpha(ko_surf_t *s, int x, int y, int w, int h, uint16_t c, int a16);
void gfx_round(ko_surf_t *s, int x, int y, int w, int h, int r, uint16_t c);
void gfx_frame(ko_surf_t *s, int x, int y, int w, int h, int r, uint16_t c);
void gfx_vgrad(ko_surf_t *s, int x, int y, int w, int h, uint16_t top, uint16_t bot);

int  gfx_text_w(const ko_font_t *f, const char *utf8);
int  gfx_text(ko_surf_t *s, const ko_font_t *f, int x, int y, uint16_t c, const char *utf8);
void gfx_text_center(ko_surf_t *s, const ko_font_t *f, int cx, int y, uint16_t c, const char *utf8);
void gfx_text_right(ko_surf_t *s, const ko_font_t *f, int rx, int y, uint16_t c, const char *utf8);

/* Logo "霞" estilizado: tres faixas de nevoa */
void gfx_logo(ko_surf_t *s, int x, int y, int scale);

#endif
