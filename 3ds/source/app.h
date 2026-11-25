/* Estado do app compartilhado entre a logica (main.c) e o desenho (screens.c). */
#ifndef KO_APP_H
#define KO_APP_H
#include <stdint.h>
#include "config.h"

#define KO_MAX_PEERS 8

enum { SCR_HOME, SCR_SETTINGS, SCR_CONNECTING, SCR_STREAM, SCR_HELP };

/* botoes da tela de toque durante o jogo */
enum {
    SB_NONE = -1,
    SB_TRIG_L, SB_TRIG_R, SB_L3, SB_R3,
    SB_VIEW, SB_GUIDE, SB_MENU, SB_KEYB, SB_MORE,
    SB_PAD,
    SB_COUNT
};

/* itens do menu durante o jogo */
enum { SM_RESUME, SM_KEYB, SM_TOUCH, SM_GYRO, SM_QUALITY, SM_STATS, SM_DISCONNECT, SM_COUNT };

/* itens dos ajustes */
enum {
    ST_PRESET, ST_AUTO, ST_AUDIO, ST_LAYOUT, ST_TRIG, ST_TOUCH, ST_GYRO, ST_GYRO_SENS,
    ST_GYRO_INV, ST_GYRO_ALT, ST_DEADZONE, ST_THEME, ST_FORGET, ST_COUNT
};

typedef struct {
    char     name[32];
    uint32_t ip;
    uint64_t seen_ms;
    int      online;
} ko_peer_t;

typedef struct {
    ko_config_t cfg;
    int screen;
    int sel, scroll;
    uint64_t now_ms;

    ko_peer_t peers[KO_MAX_PEERS];
    int npeers;

    int wifi;            /* 0..3 */
    uint32_t my_ip;
    int is_new, has_cpp, audio_ok, net_ok;

    char toast[64];
    uint64_t toast_until;

    /* conexao / jogo */
    char host_name[32];
    uint32_t host_ip;
    uint64_t connect_start;
    int pressed;          /* SB_* pressionado no toque */
    int pad_active, pad_x, pad_y, pad_ox, pad_oy;
    int menu_open, menu_sel;
    int show_stats;
    int signal_lost;
    float fps;
    int kbps, loss_pct, decode_ms;
} ko_app_t;

#endif
