#include <string.h>
#include "gfx.h"

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
const ko_theme_t ko_themes[KO_THEME_COUNT] = {
    /* bg            card            card_hi         accent           accent_dim     text             dim              ok             warn */
    { RGB(14,15,20),  RGB(28,30,39),  RGB(44,47,61),  RGB(240,164,196), RGB(120,78,98), RGB(236,236,240), RGB(138,143,156), RGB(124,214,160), RGB(250,196,92), "Sakura" },
    { RGB(10,16,24),  RGB(20,32,46),  RGB(32,50,70),  RGB(110,190,250), RGB(50,92,130), RGB(230,240,250), RGB(130,150,170), RGB(120,220,180), RGB(250,200,100), "Oceano" },
    { RGB(14,18,14),  RGB(28,36,28),  RGB(42,54,40),  RGB(170,214,120), RGB(84,110,60), RGB(234,240,226), RGB(140,152,132), RGB(150,230,150), RGB(240,200,90), "Matcha" },
    { RGB(0,0,0),     RGB(18,18,18),  RGB(36,36,36),  RGB(255,255,255), RGB(110,110,110),RGB(240,240,240), RGB(130,130,130), RGB(120,220,140), RGB(250,200,90), "OLED" },
    { RGB(242,240,236),RGB(255,255,255),RGB(228,224,218),RGB(196,70,110), RGB(230,180,196),RGB(30,30,36),    RGB(120,116,112), RGB(40,150,90),  RGB(200,120,20), "Washi" },
};
const ko_theme_t *T = &ko_themes[0];

uint16_t ko_blend(uint16_t d, uint16_t s, int a)
{
    if (a >= 16) return s;
    if (a <= 0) return d;
    /* separa canais em 32 bits: G nos bits altos, R/B nos baixos */
    uint32_t dd = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
    uint32_t ss = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
    uint32_t r = (dd + (((ss - dd) * (uint32_t)a) >> 4)) & 0x07E0F81F;
    return (uint16_t)(r | (r >> 16));
}

static inline void clip(ko_surf_t *s, int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > s->w) *w = s->w - *x;
    if (*y + *h > KO_SCREEN_H) *h = KO_SCREEN_H - *y;
}

void gfx_clear(ko_surf_t *s, uint16_t c)
{
    int n = s->w * KO_SCREEN_H;
    for (int i = 0; i < n; i++) s->px[i] = c;
}

void gfx_fill(ko_surf_t *s, int x, int y, int w, int h, uint16_t c)
{
    clip(s, &x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        uint16_t *col = s->px + (x + i) * KO_SCREEN_H + (KO_SCREEN_H - y - h);
        for (int j = 0; j < h; j++) col[j] = c;
    }
}

void gfx_fill_alpha(ko_surf_t *s, int x, int y, int w, int h, uint16_t c, int a16)
{
    clip(s, &x, &y, &w, &h);
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        uint16_t *col = s->px + (x + i) * KO_SCREEN_H + (KO_SCREEN_H - y - h);
        for (int j = 0; j < h; j++) col[j] = ko_blend(col[j], c, a16);
    }
}

static inline void pset_a(ko_surf_t *s, int x, int y, uint16_t c, int a16)
{
    if ((unsigned)x >= (unsigned)s->w || (unsigned)y >= KO_SCREEN_H) return;
    uint16_t *p = s->px + x * KO_SCREEN_H + (KO_SCREEN_H - 1 - y);
    *p = ko_blend(*p, c, a16);
}

/* cobertura (0..16) do pixel (px,py) num canto de raio r, centro em (r,r) */
static int corner_cov(int px, int py, int r)
{
    int cov = 0;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++) {
            int fx = px * 8 + sx * 2 + 1 - r * 8;   /* em 1/8 de pixel */
            int fy = py * 8 + sy * 2 + 1 - r * 8;
            if (fx * fx + fy * fy <= r * r * 64) cov++;
        }
    return cov;
}

void gfx_round(ko_surf_t *s, int x, int y, int w, int h, int r, uint16_t c)
{
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { gfx_fill(s, x, y, w, h, c); return; }
    gfx_fill(s, x + r, y, w - 2 * r, h, c);
    gfx_fill(s, x, y + r, r, h - 2 * r, c);
    gfx_fill(s, x + w - r, y + r, r, h - 2 * r, c);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < r; i++) {
            int a = corner_cov(i, j, r);
            if (!a) continue;
            pset_a(s, x + i, y + j, c, a);
            pset_a(s, x + w - 1 - i, y + j, c, a);
            pset_a(s, x + i, y + h - 1 - j, c, a);
            pset_a(s, x + w - 1 - i, y + h - 1 - j, c, a);
        }
}

void gfx_frame(ko_surf_t *s, int x, int y, int w, int h, int r, uint16_t c)
{
    /* contorno de 2px: arredondado externo menos interno, via mascara simples */
    gfx_fill(s, x + r, y, w - 2 * r, 2, c);
    gfx_fill(s, x + r, y + h - 2, w - 2 * r, 2, c);
    gfx_fill(s, x, y + r, 2, h - 2 * r, c);
    gfx_fill(s, x + w - 2, y + r, 2, h - 2 * r, c);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < r; i++) {
            int outer = corner_cov(i, j, r);
            int inner = (r > 2 && i >= 2 && j >= 2) ? corner_cov(i - 2, j - 2, r - 2) : 0;
            int a = outer - inner;
            if (a <= 0) continue;
            pset_a(s, x + i, y + j, c, a);
            pset_a(s, x + w - 1 - i, y + j, c, a);
            pset_a(s, x + i, y + h - 1 - j, c, a);
            pset_a(s, x + w - 1 - i, y + h - 1 - j, c, a);
        }
}

void gfx_vgrad(ko_surf_t *s, int x, int y, int w, int h, uint16_t top, uint16_t bot)
{
    for (int j = 0; j < h; j++) {
        int a = h > 1 ? (j * 16) / (h - 1) : 0;
        gfx_fill(s, x, y + j, w, 1, ko_blend(top, bot, a));
    }
}

/* ---------------- texto ---------------- */

static int next_cp(const char **p)
{
    const unsigned char *u = (const unsigned char *)*p;
    int cp;
    if (u[0] < 0x80) { cp = u[0]; *p += 1; }
    else if ((u[0] & 0xE0) == 0xC0 && u[1]) { cp = ((u[0] & 0x1F) << 6) | (u[1] & 0x3F); *p += 2; }
    else if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) {
        int uc = ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        *p += 3;
        switch (uc) {               /* simbolos extras da fonte (ver tools/genfont.py) */
        case 0x25C4: return 0x80;   /* ◄ */
        case 0x25BA: return 0x81;   /* ► */
        case 0x25B2: return 0x82;   /* ▲ */
        case 0x25BC: return 0x83;   /* ▼ */
        case 0x22EF: case 0x2026: return 0x84; /* ⋯ … */
        case 0x2039: return 0x85;   /* ‹ */
        case 0x203A: return 0x86;   /* › */
        default: return '?';
        }
    }
    else if ((u[0] & 0xF8) == 0xF0 && u[1] && u[2] && u[3]) { cp = '?'; *p += 4; }
    else { cp = '?'; *p += 1; }
    if (cp > 255 || (cp >= 127 && cp < 160)) cp = '?';
    return cp;
}

int gfx_text_w(const ko_font_t *f, const char *t)
{
    int w = 0;
    while (*t) w += f->glyphs[next_cp(&t)].adv;
    return w;
}

int gfx_text(ko_surf_t *s, const ko_font_t *f, int x, int y, uint16_t c, const char *t)
{
    while (*t) {
        const ko_glyph_t *g = &f->glyphs[next_cp(&t)];
        int gx = x + g->xoff;
        for (int i = 0; i < g->w; i++) {
            int px = gx + i;
            if ((unsigned)px >= (unsigned)s->w) continue;
            uint16_t *col = s->px + px * KO_SCREEN_H;
            for (int j = 0; j < f->height; j++) {
                int py = y + j;
                if ((unsigned)py >= KO_SCREEN_H) continue;
                int n = j * g->w + i;
                int a = (f->data[g->offset + (n >> 1)] >> ((n & 1) * 4)) & 15;
                if (!a) continue;
                uint16_t *p = col + (KO_SCREEN_H - 1 - py);
                *p = ko_blend(*p, c, a + (a > 7));
            }
        }
        x += g->adv;
    }
    return x;
}

void gfx_text_center(ko_surf_t *s, const ko_font_t *f, int cx, int y, uint16_t c, const char *t)
{
    gfx_text(s, f, cx - gfx_text_w(f, t) / 2, y, c, t);
}

void gfx_text_right(ko_surf_t *s, const ko_font_t *f, int rx, int y, uint16_t c, const char *t)
{
    gfx_text(s, f, rx - gfx_text_w(f, t), y, c, t);
}

void gfx_logo(ko_surf_t *s, int x, int y, int k)
{
    /* tres faixas de nevoa, com larguras e opacidades diferentes */
    gfx_round(s, x + 4 * k, y,          22 * k, 4 * k, 2 * k, T->accent);
    gfx_round(s, x,          y + 7 * k, 30 * k, 4 * k, 2 * k, ko_blend(T->accent, T->bg, 6));
    gfx_round(s, x + 9 * k, y + 14 * k, 18 * k, 4 * k, 2 * k, ko_blend(T->accent, T->bg, 10));
}
