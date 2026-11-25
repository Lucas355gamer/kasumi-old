#ifndef KO_SCREENS_H
#define KO_SCREENS_H
#include "app.h"
#include "gfx.h"

typedef struct { int x, y, w, h; } ko_rect_t;

void scr_home_top(ko_surf_t *s, const ko_app_t *a);
void scr_home_bottom(ko_surf_t *s, const ko_app_t *a);
int  scr_home_hit(int tx, int ty);              /* 0 conectar 1 IP 2 ajustes 3 ajuda, -1 nada */

void scr_settings_top(ko_surf_t *s, const ko_app_t *a);
void scr_settings_bottom(ko_surf_t *s, const ko_app_t *a);
const char *scr_setting_value(const ko_app_t *a, int item, char *buf);

void scr_connecting_top(ko_surf_t *s, const ko_app_t *a);
void scr_connecting_bottom(ko_surf_t *s, const ko_app_t *a);

void scr_help_top(ko_surf_t *s, const ko_app_t *a);
void scr_help_bottom(ko_surf_t *s, const ko_app_t *a);

void scr_stream_bottom(ko_surf_t *s, const ko_app_t *a);
int  scr_stream_hit(const ko_app_t *a, int tx, int ty);    /* SB_* */
ko_rect_t scr_stream_pad_rect(void);
void scr_stream_menu_bottom(ko_surf_t *s, const ko_app_t *a);
int  scr_stream_menu_hit(int tx, int ty);                   /* SM_* ou -1 */
void scr_stream_overlay(ko_surf_t *top, const ko_app_t *a); /* stats/aviso por cima do video */

void scr_toast(ko_surf_t *s, const ko_app_t *a);

int gfx_text_wrap(ko_surf_t *s, const ko_font_t *f, int x, int y, int w, uint16_t c, const char *t);

#endif
