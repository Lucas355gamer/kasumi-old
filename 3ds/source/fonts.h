/* Gerado por tools/genfont.py - nao editar. Fonte: Inter (SIL Open Font License). */
#ifndef KO_FONTS_H
#define KO_FONTS_H
#include <stdint.h>
typedef struct { uint32_t offset; uint8_t w; int8_t xoff; uint8_t adv; } ko_glyph_t;
typedef struct { uint8_t height; uint8_t ascent; const ko_glyph_t *glyphs; const uint8_t *data; } ko_font_t;
extern const ko_font_t font_small;
extern const ko_font_t font_body;
extern const ko_font_t font_title;
#endif
