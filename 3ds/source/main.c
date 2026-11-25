/*
 * Kasumi Old - cliente de streaming para Nintendo 3DS (Old e New).
 *
 * O PC (Kasumi Bridge) captura tela e som de qualquer coisa - GeForce NOW,
 * Xbox Cloud, Steam, emuladores - e manda JPEG 400x240 + audio ADPCM por UDP.
 * O 3DS so decodifica, toca e devolve os botoes. Nada de H.264: o Old 3DS nao
 * tem decodificador de video por hardware e nao aguentaria por software.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "protocol.h"
#include "config.h"
#include "app.h"
#include "gfx.h"
#include "screens.h"
#include "net.h"
#include "reasm.h"
#include "video.h"
#include "audio.h"
#include "input.h"

#define CFG_DIR  "sdmc:/3ds/kasumi-old"
#define CFG_PATH CFG_DIR "/config.ini"

#define CONNECT_TIMEOUT_MS  12000
#define SIGNAL_LOST_MS      3000
#define PEER_EXPIRE_MS      6000

static ko_app_t A;
static ko_reasm_t reasm;
static uint8_t latest[KO_VIDEO_MAX];
static uint32_t latest_size = 0;
static int latest_ready = 0;
static uint8_t pkt[2048];

/* PCs achados por broadcast */
static ko_peer_t found[KO_MAX_PEERS];
static int nfound = 0;

/* ---------------------------------------------------------------- util */

static u64 now_ms(void) { return osGetTime(); }

static void toast(const char *msg)
{
    snprintf(A.toast, sizeof(A.toast), "%s", msg);
    A.toast_until = now_ms() + 1800;
}

static ko_surf_t surf_top(void)
{
    return (ko_surf_t){ (uint16_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 400 };
}

static ko_surf_t surf_bottom(void)
{
    return (ko_surf_t){ (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), 320 };
}

static void present_top(void)
{
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_TOP, false);
}

static void present_bottom(void)
{
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

static void save_cfg(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(CFG_DIR, 0777);
    cfg_save(&A.cfg, CFG_PATH);
}

static void refresh_status(void)
{
    A.my_ip = A.net_ok ? net_local_ip() : 0;
    A.wifi = A.my_ip ? net_wifi_strength() : 0;
    A.has_cpp = input_has_cpp();
}

/* ---------------------------------------------------------------- lista de PCs */

static void rebuild_peers(void)
{
    u64 t = now_ms();
    uint32_t sel_ip = (A.sel < A.npeers) ? A.peers[A.sel].ip : 0;
    A.npeers = 0;
    for (int i = 0; i < nfound && A.npeers < KO_MAX_PEERS; i++)
        if (t - found[i].seen_ms < PEER_EXPIRE_MS) {
            A.peers[A.npeers] = found[i];
            A.peers[A.npeers].online = 1;
            A.npeers++;
        }
    for (int i = 0; i < A.cfg.recent_count && A.npeers < KO_MAX_PEERS; i++) {
        int dup = 0;
        for (int k = 0; k < A.npeers; k++) if (A.peers[k].ip == A.cfg.recent[i].ip) dup = 1;
        if (dup) continue;
        ko_peer_t *p = &A.peers[A.npeers++];
        memset(p, 0, sizeof(*p));
        snprintf(p->name, sizeof(p->name), "%s", A.cfg.recent[i].name);
        p->ip = A.cfg.recent[i].ip;
    }
    /* mantem a selecao no mesmo PC se ele mudou de posicao */
    for (int k = 0; k < A.npeers; k++) if (A.peers[k].ip == sel_ip) A.sel = k;
    if (A.sel >= A.npeers) A.sel = A.npeers ? A.npeers - 1 : 0;
    if (A.sel < A.scroll) A.scroll = A.sel;
    if (A.sel >= A.scroll + 4) A.scroll = A.sel - 3;
}

static void on_announce(uint32_t ip, const uint8_t *name, int len)
{
    char n[32];
    if (len > 31) len = 31;
    memcpy(n, name, (size_t)len);
    n[len] = 0;
    if (!n[0]) strcpy(n, "PC");
    for (int i = 0; i < nfound; i++)
        if (found[i].ip == ip) {
            snprintf(found[i].name, sizeof(found[i].name), "%s", n);
            found[i].seen_ms = now_ms();
            return;
        }
    if (nfound == KO_MAX_PEERS) return;
    snprintf(found[nfound].name, sizeof(found[nfound].name), "%s", n);
    found[nfound].ip = ip;
    found[nfound].seen_ms = now_ms();
    nfound++;
}

/* ---------------------------------------------------------------- conexao */

static void send_hello(void)
{
    uint8_t p[KO_HDR_SIZE + KO_HELLO_SIZE];
    const ko_preset_t *pr = &ko_presets[A.cfg.preset];
    ko_header(p, KO_T_HELLO);
    p[4] = (uint8_t)pr->fps;
    p[5] = (uint8_t)pr->quality;
    p[6] = (uint8_t)(A.cfg.audio && A.audio_ok);
    p[7] = A.cfg.auto_adjust ? KO_HELLO_F_AUTO : 0;
    net_send(A.host_ip, p, sizeof(p));
}

static void send_bye(void)
{
    uint8_t p[KO_HDR_SIZE];
    ko_header(p, KO_T_BYE);
    for (int i = 0; i < 3; i++) net_send(A.host_ip, p, sizeof(p));
}

static void start_connect(uint32_t ip, const char *name)
{
    A.host_ip = ip;
    snprintf(A.host_name, sizeof(A.host_name), "%s", name);
    A.connect_start = now_ms();
    A.screen = SCR_CONNECTING;
    send_hello();
}

static void stream_begin(const uint8_t *name, int len)
{
    if (len > 0) {
        if (len > 31) len = 31;
        memcpy(A.host_name, name, (size_t)len);
        A.host_name[len] = 0;
    }
    cfg_add_recent(&A.cfg, A.host_ip, A.host_name);
    save_cfg();
    ko_reasm_init(&reasm);
    latest_ready = 0;
    latest_size = 0;
    audio_reset();
    input_gyro(A.cfg.gyro);
    A.screen = SCR_STREAM;
    A.menu_open = 0;
    A.signal_lost = 0;
    A.pressed = SB_NONE;
    A.pad_active = 0;
    A.fps = 0;
    A.kbps = 0;
    A.loss_pct = 0;
}

static void stream_end(void)
{
    send_bye();
    input_gyro(0);
    audio_reset();
    A.screen = SCR_HOME;
    A.menu_open = 0;
    A.sel = 0;
    A.scroll = 0;
}

/* ---------------------------------------------------------------- teclados */

static void remote_keyboard(void)
{
    static uint16_t text_id = 1;
    SwkbdState kb;
    char buf[256];
    memset(buf, 0, sizeof(buf));
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 3, 200);
    swkbdSetHintText(&kb, "Texto para digitar no PC");
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "Cancelar", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_MIDDLE, "Digitar", true);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "Digitar+Enter", true);
    swkbdSetValidation(&kb, SWKBD_ANYTHING, 0, 0);
    SwkbdButton b = swkbdInputText(&kb, buf, sizeof(buf));
    if (b != SWKBD_BUTTON_MIDDLE && b != SWKBD_BUTTON_RIGHT) return;

    uint8_t p[KO_HDR_SIZE + 3 + 256];
    int len = (int)strlen(buf);
    ko_header(p, KO_T_TEXT);
    ko_put16(p + 4, text_id++);
    p[6] = (b == SWKBD_BUTTON_RIGHT) ? KO_TEXT_F_ENTER : 0;
    memcpy(p + 7, buf, (size_t)len);
    for (int i = 0; i < 3; i++) net_send(A.host_ip, p, KO_HDR_SIZE + 3 + len);
    toast(b == SWKBD_BUTTON_RIGHT ? "Enviado com Enter" : "Texto enviado");
}

static void manual_ip(void)
{
    SwkbdState kb;
    char buf[20];
    memset(buf, 0, sizeof(buf));
    if (A.cfg.recent_count) ip_to_str(A.cfg.recent[0].ip, buf);
    swkbdInit(&kb, SWKBD_TYPE_NUMPAD, 2, 15);
    swkbdSetNumpadKeys(&kb, L'.', 0);
    swkbdSetHintText(&kb, "IP do PC, ex.: 192.168.0.10");
    swkbdSetInitialText(&kb, buf);
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "Cancelar", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "Conectar", true);
    if (swkbdInputText(&kb, buf, sizeof(buf)) != SWKBD_BUTTON_RIGHT) return;
    uint32_t ip;
    if (!ip_from_str(buf, &ip)) { toast("IP inválido"); return; }
    start_connect(ip, buf);
}

/* ---------------------------------------------------------------- telas de menu */

static void change_setting(int item, int dir)
{
    ko_config_t *c = &A.cfg;
    switch (item) {
    case ST_PRESET:    c->preset = (c->preset + dir + KO_PRESET_COUNT) % KO_PRESET_COUNT; break;
    case ST_AUTO:      c->auto_adjust = !c->auto_adjust; break;
    case ST_AUDIO:     if (A.audio_ok) c->audio = !c->audio; else toast("Falta sd:/3ds/dspfirm.cdc"); break;
    case ST_LAYOUT:    c->layout = !c->layout; break;
    case ST_TRIG:      c->trig_mode = !c->trig_mode; break;
    case ST_TOUCH:     c->touch_mode = !c->touch_mode; break;
    case ST_GYRO:      c->gyro = !c->gyro; break;
    case ST_GYRO_SENS: c->gyro_sens += dir; if (c->gyro_sens < 1) c->gyro_sens = 1; if (c->gyro_sens > 10) c->gyro_sens = 10; break;
    case ST_GYRO_INV:  c->gyro_invert_y = !c->gyro_invert_y; break;
    case ST_GYRO_ALT:  c->gyro_alt = !c->gyro_alt; break;
    case ST_DEADZONE:  c->deadzone += dir * 2; if (c->deadzone < 0) c->deadzone = 0; if (c->deadzone > 40) c->deadzone = 40; break;
    case ST_THEME:     c->theme = (c->theme + dir + KO_THEME_COUNT) % KO_THEME_COUNT; T = &ko_themes[c->theme]; break;
    case ST_FORGET:    break;
    }
}

static void home_action(int which)
{
    switch (which) {
    case 0:
        if (A.npeers) start_connect(A.peers[A.sel].ip, A.peers[A.sel].name);
        else toast("Nenhum PC na lista ainda");
        break;
    case 1: manual_ip(); break;
    case 2: A.screen = SCR_SETTINGS; A.sel = 0; A.scroll = 0; break;
    case 3: A.screen = SCR_HELP; break;
    }
}

static void process_menu_packets(void)
{
    uint32_t from;
    for (int i = 0; i < 32; i++) {
        int n = net_recv(pkt, sizeof(pkt), &from);
        if (n <= 0) break;
        if (!ko_check(pkt, n)) continue;
        if (pkt[2] == KO_T_ANNOUNCE) on_announce(from, pkt + 4, n - 4);
        else if (A.screen == SCR_CONNECTING && from == A.host_ip &&
                 (pkt[2] == KO_T_WELCOME || pkt[2] == KO_T_VIDEO)) {
            stream_begin(pkt[2] == KO_T_WELCOME ? pkt + 4 : NULL, pkt[2] == KO_T_WELCOME ? n - 4 : 0);
            return;
        }
    }
}

/* devolve 1 se deve sair do app */
static int menus_frame(u32 down, int touch_released, int tx, int ty)
{
    static u64 last_discover = 0, last_hello = 0;
    u64 t = now_ms();

    process_menu_packets();
    if (A.screen == SCR_STREAM) return 0;

    switch (A.screen) {
    case SCR_HOME:
        if (A.net_ok && t - last_discover > 2000) { net_discover(); last_discover = t; }
        rebuild_peers();
        if (down & KEY_START) return 1;
        if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) { if (A.sel + 1 < A.npeers) A.sel++; }
        if (down & (KEY_DUP | KEY_CPAD_UP))     { if (A.sel > 0) A.sel--; }
        if (A.sel < A.scroll) A.scroll = A.sel;
        if (A.sel >= A.scroll + 4) A.scroll = A.sel - 3;
        if (down & KEY_A) home_action(0);
        else if (down & KEY_X) home_action(1);
        else if (down & KEY_Y) { nfound = 0; refresh_status(); if (A.net_ok) net_discover(); toast("Buscando PCs..."); }
        else if (down & KEY_SELECT) home_action(2);
        else if (touch_released) { int h = scr_home_hit(tx, ty); if (h >= 0) home_action(h); }
        break;

    case SCR_SETTINGS:
        if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) A.sel = (A.sel + 1) % ST_COUNT;
        if (down & (KEY_DUP | KEY_CPAD_UP))     A.sel = (A.sel + ST_COUNT - 1) % ST_COUNT;
        if (A.sel < A.scroll) A.scroll = A.sel;
        if (A.sel >= A.scroll + 8) A.scroll = A.sel - 7;
        if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT)) change_setting(A.sel, +1);
        if (down & (KEY_DLEFT | KEY_CPAD_LEFT))   change_setting(A.sel, -1);
        if (down & KEY_A) {
            if (A.sel == ST_FORGET) { A.cfg.recent_count = 0; toast("Lista de PCs apagada"); }
            else change_setting(A.sel, +1);
        }
        if ((down & KEY_B) || (touch_released && ty >= 188)) {
            save_cfg();
            A.screen = SCR_HOME; A.sel = 0; A.scroll = 0;
        }
        break;

    case SCR_HELP:
        if ((down & (KEY_B | KEY_A)) || touch_released) A.screen = SCR_HOME;
        break;

    case SCR_CONNECTING:
        if (t - last_hello > 500) { send_hello(); last_hello = t; }
        if (down & KEY_B) { A.screen = SCR_HOME; break; }
        if (t - A.connect_start > CONNECT_TIMEOUT_MS) {
            A.screen = SCR_HOME;
            toast("Sem resposta do PC");
        }
        break;
    }
    return 0;
}

static void draw_menus(void)
{
    ko_surf_t top = surf_top(), bot = surf_bottom();
    switch (A.screen) {
    case SCR_HOME:       scr_home_top(&top, &A);       scr_home_bottom(&bot, &A); break;
    case SCR_SETTINGS:   scr_settings_top(&top, &A);   scr_settings_bottom(&bot, &A); break;
    case SCR_CONNECTING: scr_connecting_top(&top, &A); scr_connecting_bottom(&bot, &A); break;
    case SCR_HELP:       scr_help_top(&top, &A);       scr_help_bottom(&bot, &A); break;
    default: return;
    }
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_TOP, false);
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

/* ---------------------------------------------------------------- jogo */

typedef struct {
    u64 last_rx, last_input, last_stats, last_hello, combo_start, touch_start;
    uint32_t seq, bytes, frames_shown, dec_total_ms, dec_count;
    uint32_t prev_ok, prev_lost;
    int touching, touch_id, touch_moved, last_tx, last_ty;
    int mdx, mdy, click_left, click_right;
    int bottom_dirty;
    u64 toast_drawn_until;
} stream_t;

static stream_t S;

static void stream_reset_state(void)
{
    memset(&S, 0, sizeof(S));
    u64 t = now_ms();
    S.last_rx = t;
    S.last_stats = t;
    S.touch_id = SB_NONE;
    S.bottom_dirty = 1;
}

static void decode_and_show(void)
{
    if (!latest_size) return;
    ko_surf_t top = surf_top();
    u64 t0 = svcGetSystemTick();
    int ok = video_decode_to_fb(latest, latest_size, top.px);
    u64 t1 = svcGetSystemTick();
    if (!ok) return;
    S.dec_total_ms += (uint32_t)((t1 - t0) / CPU_TICKS_PER_MSEC);
    S.dec_count++;
    scr_stream_overlay(&top, &A);
    present_top();
    S.frames_shown++;
}

static void stream_menu_action(int i)
{
    switch (i) {
    case SM_RESUME: A.menu_open = 0; break;
    case SM_KEYB:   A.menu_open = 0; remote_keyboard(); break;
    case SM_TOUCH:  A.cfg.touch_mode = !A.cfg.touch_mode; save_cfg(); break;
    case SM_GYRO:   A.cfg.gyro = !A.cfg.gyro; input_gyro(A.cfg.gyro); save_cfg(); break;
    case SM_QUALITY:
        A.cfg.preset = (A.cfg.preset + 1) % KO_PRESET_COUNT;
        save_cfg();
        send_hello();
        toast(ko_presets[A.cfg.preset].name);
        break;
    case SM_STATS:  A.show_stats = !A.show_stats; break;
    case SM_DISCONNECT: stream_end(); break;
    }
    S.bottom_dirty = 1;
}

static void stream_menu_nav(u32 down)
{
    int i = A.menu_sel;
    if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) i = (i == 0) ? 1 : (i + 2 < SM_COUNT ? i + 2 : i);
    if (down & (KEY_DUP | KEY_CPAD_UP))     i = (i <= 2) ? 0 : i - 2;
    if (down & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
        if (i > 0) i = (i % 2) ? (i + 1 < SM_COUNT ? i + 1 : i) : i - 1;
    }
    if (i != A.menu_sel) { A.menu_sel = i; S.bottom_dirty = 1; }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void stream_touch(u32 held, ko_pad_t *pad)
{
    touchPosition tp;
    int touching = (held & KEY_TOUCH) != 0;
    u64 t = now_ms();
    if (touching) hidTouchRead(&tp);

    if (A.menu_open) {
        if (!touching && S.touching) {
            int h = scr_stream_menu_hit(S.last_tx, S.last_ty);
            if (h >= 0) { A.menu_sel = h; stream_menu_action(h); }
        }
        S.touching = touching;
        if (touching) { S.last_tx = tp.px; S.last_ty = tp.py; }
        return;
    }

    if (touching && !S.touching) {
        S.touch_id = scr_stream_hit(&A, tp.px, tp.py);
        S.touch_start = t;
        S.touch_moved = 0;
        S.last_tx = tp.px; S.last_ty = tp.py;
        if (S.touch_id == SB_PAD) {
            A.pad_active = 1;
            A.pad_ox = A.pad_x = tp.px;
            A.pad_oy = A.pad_y = tp.py;
        } else {
            A.pressed = S.touch_id;
        }
        S.bottom_dirty = 1;
    } else if (touching) {
        int dx = tp.px - S.last_tx, dy = tp.py - S.last_ty;
        if (abs(tp.px - A.pad_ox) > 5 || abs(tp.py - A.pad_oy) > 5) S.touch_moved = 1;
        if (S.touch_id == SB_PAD) {
            if (A.cfg.touch_mode == 1) { S.mdx += dx * 3; S.mdy += dy * 3; }
            if (dx || dy) S.bottom_dirty = 1;
            A.pad_x = tp.px; A.pad_y = tp.py;
        }
        S.last_tx = tp.px; S.last_ty = tp.py;
    } else if (S.touching) {
        /* soltou */
        if (S.touch_id == SB_PAD && A.cfg.touch_mode == 1 && !S.touch_moved) {
            u64 dur = t - S.touch_start;
            if (dur < 250) S.click_left = 3;
            else if (dur > 600) S.click_right = 3;
        }
        if (S.touch_id == SB_KEYB) remote_keyboard();
        if (S.touch_id == SB_MORE) { A.menu_open = 1; A.menu_sel = 0; }
        S.touch_id = SB_NONE;
        A.pressed = SB_NONE;
        A.pad_active = 0;
        S.bottom_dirty = 1;
    }
    S.touching = touching;

    /* botoes de toque segurados */
    int lr_triggers = A.cfg.trig_mode == 1;
    switch (S.touch_id) {
    case SB_TRIG_L: if (lr_triggers) pad->buttons |= KO_B_LB; else pad->lt = 255; break;
    case SB_TRIG_R: if (lr_triggers) pad->buttons |= KO_B_RB; else pad->rt = 255; break;
    case SB_L3:     pad->buttons |= KO_B_LS; break;
    case SB_R3:     pad->buttons |= KO_B_RS; break;
    case SB_VIEW:   pad->buttons |= KO_B_BACK; break;
    case SB_GUIDE:  pad->buttons |= KO_B_GUIDE; break;
    case SB_MENU:   pad->buttons |= KO_B_START; break;
    default: break;
    }
    if (S.touch_id == SB_PAD && A.cfg.touch_mode == 0) {
        pad->rx = clampi(pad->rx + (A.pad_x - A.pad_ox) * 32767 / 40, -32767, 32767);
        pad->ry = clampi(pad->ry - (A.pad_y - A.pad_oy) * 32767 / 40, -32767, 32767);
    }
}

static void stream_frame(u32 held, u32 down)
{
    u64 t = now_ms();
    uint32_t from;

    /* 1) esvazia o socket */
    for (int i = 0; i < 96; i++) {
        int n = net_recv(pkt, sizeof(pkt), &from);
        if (n <= 0) break;
        if (!ko_check(pkt, n) || from != A.host_ip) continue;
        S.last_rx = t;
        S.bytes += (uint32_t)n;
        if (pkt[2] == KO_T_VIDEO) {
            uint32_t sz;
            const uint8_t *f = ko_reasm_push(&reasm, pkt + 4, n - 4, &sz);
            if (f) { memcpy(latest, f, sz); latest_size = sz; latest_ready = 1; }
        } else if (pkt[2] == KO_T_AUDIO) {
            audio_push(pkt + 4, n - 4);
        }
    }

    /* 2) sinal */
    int lost = (t - S.last_rx) > SIGNAL_LOST_MS;
    if (lost != A.signal_lost) {
        A.signal_lost = lost;
        if (lost) decode_and_show();   /* redesenha o ultimo quadro com o aviso */
    }
    if (lost && t - S.last_hello > 1000) { send_hello(); S.last_hello = t; }
    if (lost && (held & KEY_B) && (held & KEY_Y)) { stream_end(); return; }

    /* 3) video: so o quadro mais novo */
    if (latest_ready) { latest_ready = 0; decode_and_show(); }

    /* 4) controles */
    ko_pad_t pad;
    input_build(&A.cfg, &pad);

    int combo = (held & KEY_START) && (held & KEY_SELECT);
    if (combo) {
        pad.buttons &= ~(KO_B_START | KO_B_BACK);
        if (!S.combo_start) S.combo_start = t;
        else if (t - S.combo_start > 800 && !A.menu_open) { A.menu_open = 1; A.menu_sel = 0; S.bottom_dirty = 1; }
    } else {
        S.combo_start = 0;
    }

    stream_touch(held, &pad);
    if (A.screen != SCR_STREAM) return;    /* desconectou pelo menu */

    if (A.menu_open) {
        stream_menu_nav(down);
        if (down & KEY_A) stream_menu_action(A.menu_sel);
        else if (down & KEY_B) { A.menu_open = 0; S.bottom_dirty = 1; }
        if (A.screen != SCR_STREAM) return;
        memset(&pad, 0, sizeof(pad));      /* nada chega ao jogo com o menu aberto */
    }

    if (t - S.last_input >= 8) {
        pad.mdx = S.mdx; pad.mdy = S.mdy;
        S.mdx = S.mdy = 0;
        if (S.click_left)  { pad.mbtn |= KO_M_LEFT;  S.click_left--; }
        if (S.click_right) { pad.mbtn |= KO_M_RIGHT; S.click_right--; }
        uint8_t b[KO_HDR_SIZE + KO_INPUT_SIZE];
        int len = input_pack(&pad, S.seq++, b);
        net_send(A.host_ip, b, len);
        S.last_input = t;
    }

    /* 5) estatisticas a cada segundo (tambem alimentam o ajuste automatico no PC) */
    if (t - S.last_stats >= 1000) {
        uint32_t ok = reasm.frames_ok - S.prev_ok, ls = reasm.frames_lost - S.prev_lost;
        S.prev_ok = reasm.frames_ok; S.prev_lost = reasm.frames_lost;
        float secs = (float)(t - S.last_stats) / 1000.0f;
        A.fps = (float)S.frames_shown / secs;
        A.kbps = (int)((float)S.bytes * 8.0f / 1000.0f / secs);
        A.loss_pct = (ok + ls) ? (int)(ls * 100 / (ok + ls)) : 0;
        A.decode_ms = S.dec_count ? (int)(S.dec_total_ms / S.dec_count) : 0;

        uint8_t p[KO_HDR_SIZE + KO_STATS_SIZE];
        ko_header(p, KO_T_STATS);
        ko_put16(p + 4, (uint16_t)ok);
        ko_put16(p + 6, (uint16_t)ls);
        ko_put16(p + 8, (uint16_t)A.decode_ms);
        ko_put16(p + 10, (uint16_t)audio_take_lost());
        net_send(A.host_ip, p, sizeof(p));

        S.frames_shown = S.bytes = S.dec_total_ms = S.dec_count = 0;
        S.last_stats = t;
        A.wifi = net_wifi_strength();
    }

    /* 6) tela de baixo so quando algo muda */
    int toast_live = A.toast[0] && t <= A.toast_until;
    if (toast_live || S.toast_drawn_until) {
        if (!toast_live) { S.toast_drawn_until = 0; S.bottom_dirty = 1; }
        else if (S.toast_drawn_until != A.toast_until) { S.toast_drawn_until = A.toast_until; S.bottom_dirty = 1; }
    }
    if (S.bottom_dirty) {
        ko_surf_t bot = surf_bottom();
        A.now_ms = t;
        if (A.menu_open) scr_stream_menu_bottom(&bot, &A);
        else scr_stream_bottom(&bot, &A);
        present_bottom();
        S.bottom_dirty = 0;
    }
}

/* ---------------------------------------------------------------- main */

int main(void)
{
    osSetSpeedupEnable(true);   /* New 3DS: 804 MHz + cache L2. No Old 3DS nao faz nada. */
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    /* buffer duplo nas duas telas: toda tela de baixo e redesenhada inteira antes
       da troca, entao nao ha "rasgo" (tearing) enquanto o LCD le a memoria */
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    acInit();

    memset(&A, 0, sizeof(A));
    cfg_load(&A.cfg, CFG_PATH);
    T = &ko_themes[A.cfg.theme];
    A.pressed = SB_NONE;

    input_init();
    A.is_new = input_is_new3ds();
    A.net_ok = net_init();
    A.audio_ok = audio_init();
    refresh_status();

    u64 last_status = 0;
    int prev_touch = 0, tx = 0, ty = 0;
    int prev_screen = -1;

    while (aptMainLoop()) {
        hidScanInput();
        input_scan();
        u32 held = input_held(), down = input_down();
        u64 t = now_ms();
        A.now_ms = t;

        if (A.screen != SCR_STREAM) {
            int touching = (held & KEY_TOUCH) != 0;
            if (touching) { touchPosition tp; hidTouchRead(&tp); tx = tp.px; ty = tp.py; }
            int released = prev_touch && !touching;
            prev_touch = touching;

            if (t - last_status > 1000) { refresh_status(); last_status = t; }
            if (menus_frame(down, released, tx, ty)) break;
            if (A.screen == SCR_STREAM) continue;
            draw_menus();
            gspWaitForVBlank();
        } else {
            if (prev_screen != SCR_STREAM) stream_reset_state();
            stream_frame(held, down);
            if (A.screen != SCR_STREAM) { prev_touch = 0; prev_screen = A.screen; continue; }
            svcSleepThread(1000000LL);   /* 1 ms: deixa o sistema respirar sem perder pacotes */
        }
        prev_screen = A.screen;
    }

    if (A.screen == SCR_STREAM) stream_end();
    save_cfg();
    audio_exit();
    net_exit();
    input_exit();
    acExit();
    gfxExit();
    return 0;
}
