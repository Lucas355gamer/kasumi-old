/* Renderiza as telas do app no PC (PPM) para conferir o visual sem um 3DS. */
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "../3ds/source/screens.h"

static uint16_t top_px[400 * 240], bot_px[320 * 240];

static void dump(const char *name, ko_surf_t *top, ko_surf_t *bot)
{
    /* junta as duas telas como no console: superior em cima, inferior centralizada */
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6 400 480 255\n");
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 400; x++) {
            uint16_t c = 0;
            if (y < 240) c = top->px[x * 240 + (239 - y)];
            else if (x >= 40 && x < 360) c = bot->px[(x - 40) * 240 + (239 - (y - 240))];
            unsigned char rgb[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

int main(void)
{
    ko_surf_t top = { top_px, 400 }, bot = { bot_px, 320 };
    ko_app_t a;
    memset(&a, 0, sizeof(a));
    cfg_defaults(&a.cfg);
    a.wifi = 3; a.net_ok = 1; a.audio_ok = 1; a.is_new = 0; a.has_cpp = 0;
    a.my_ip = inet_addr("192.168.0.42");
    a.now_ms = 123456;

    /* home vazia */
    scr_home_top(&top, &a); scr_home_bottom(&bot, &a); dump("out/01_home_vazia.ppm", &top, &bot);

    /* home com PCs */
    strcpy(a.peers[0].name, "PC-da-Sala"); a.peers[0].ip = inet_addr("192.168.0.10"); a.peers[0].online = 1;
    strcpy(a.peers[1].name, "Notebook"); a.peers[1].ip = inet_addr("192.168.0.23"); a.peers[1].online = 0;
    a.npeers = 2;
    scr_home_top(&top, &a); scr_home_bottom(&bot, &a); dump("out/02_home.ppm", &top, &bot);

    /* sem wifi */
    ko_app_t b = a; b.my_ip = 0; b.wifi = 0;
    scr_home_top(&top, &b); scr_home_bottom(&bot, &b); dump("out/03_sem_wifi.ppm", &top, &bot);

    /* ajustes */
    a.screen = SCR_SETTINGS; a.sel = 4; a.scroll = 0;
    scr_settings_top(&top, &a); scr_settings_bottom(&bot, &a); dump("out/04_ajustes.ppm", &top, &bot);
    a.sel = 12; a.scroll = 5;
    scr_settings_top(&top, &a); scr_settings_bottom(&bot, &a); dump("out/05_ajustes_fim.ppm", &top, &bot);

    /* conectando */
    strcpy(a.host_name, "PC-da-Sala"); a.host_ip = a.peers[0].ip; a.connect_start = 118000;
    scr_connecting_top(&top, &a); scr_connecting_bottom(&bot, &a); dump("out/06_conectando.ppm", &top, &bot);

    /* ajuda */
    scr_help_top(&top, &a); scr_help_bottom(&bot, &a); dump("out/07_ajuda.ppm", &top, &bot);

    /* jogo: video falso + painel de toque */
    for (int x = 0; x < 400; x++)
        for (int y = 0; y < 240; y++)
            top_px[x * 240 + (239 - y)] = ko_rgb(40 + x / 4, 60 + y / 3, 120);
    a.show_stats = 1; a.fps = 15; a.kbps = 2100; a.loss_pct = 2; a.decode_ms = 31;
    a.pressed = SB_TRIG_R; a.pad_active = 1; a.pad_ox = 160; a.pad_oy = 118; a.pad_x = 185; a.pad_y = 105;
    a.cfg.gyro = 1;
    scr_stream_overlay(&top, &a); scr_stream_bottom(&bot, &a); dump("out/08_jogo.ppm", &top, &bot);

    a.signal_lost = 1; a.menu_sel = SM_QUALITY; a.pressed = SB_NONE;
    strcpy(a.toast, "Teclado enviado"); a.toast_until = a.now_ms + 1000;
    scr_stream_overlay(&top, &a); scr_stream_menu_bottom(&bot, &a); dump("out/09_menu_jogo.ppm", &top, &bot);

    /* temas */
    for (int t = 1; t < KO_THEME_COUNT; t++) {
        T = &ko_themes[t];
        a.toast[0] = 0;
        char n[64];
        snprintf(n, sizeof(n), "out/1%d_tema_%s.ppm", t, ko_themes[t].name);
        scr_home_top(&top, &a); scr_home_bottom(&bot, &a); dump(n, &top, &bot);
    }
    return 0;
}
