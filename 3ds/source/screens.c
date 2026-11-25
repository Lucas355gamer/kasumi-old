#include <stdio.h>
#include <string.h>
#include "screens.h"

#define TOP_W 400
#define BOT_W 320

/* ---------------------------------------------------------------- util */

int gfx_text_wrap(ko_surf_t *s, const ko_font_t *f, int x, int y, int w, uint16_t c, const char *t)
{
    char line[240], word[80];
    int lh = f->height + 1, cy = y;
    line[0] = 0;
    while (1) {
        int n = 0;
        while (*t == ' ') t++;
        while (*t && *t != ' ' && *t != '\n' && n < (int)sizeof(word) - 1) word[n++] = *t++;
        word[n] = 0;

        if (n) {
            char test[240];
            snprintf(test, sizeof(test), "%s%s%s", line, line[0] ? " " : "", word);
            if (gfx_text_w(f, test) > w && line[0]) {
                if (s) gfx_text(s, f, x, cy, c, line);
                cy += lh;
                snprintf(line, sizeof(line), "%s", word);
            } else {
                snprintf(line, sizeof(line), "%s", test);
            }
        }
        if (*t == '\n' || !*t) {
            if (line[0] || *t == '\n') {
                if (s && line[0]) gfx_text(s, f, x, cy, c, line);
                cy += lh;
            }
            line[0] = 0;
            if (!*t) break;
            t++;
        }
    }
    return cy - y;
}

static int hit(ko_rect_t r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

static void wifi_icon(ko_surf_t *s, int x, int y, int lvl)
{
    for (int i = 0; i < 3; i++) {
        int h = 4 + i * 4;
        gfx_round(s, x + i * 6, y + 12 - h, 4, h, 1, i < lvl ? T->text : T->card_hi);
    }
}

static void button(ko_surf_t *s, ko_rect_t r, const char *label, const char *sub, int hot, int pressed)
{
    uint16_t bg = pressed ? T->accent : hot ? T->card_hi : T->card;
    uint16_t fg = pressed ? T->bg : T->text;
    gfx_round(s, r.x, r.y, r.w, r.h, 10, bg);
    if (hot && !pressed) gfx_frame(s, r.x, r.y, r.w, r.h, 10, T->accent);
    if (sub) {
        int th = font_body.height + 2 + font_small.height;
        int ty = r.y + (r.h - th) / 2;
        gfx_text_center(s, &font_body, r.x + r.w / 2, ty, fg, label);
        gfx_text_center(s, &font_small, r.x + r.w / 2, ty + font_body.height + 2, pressed ? T->bg : T->dim, sub);
    } else {
        gfx_text_center(s, &font_body, r.x + r.w / 2, r.y + (r.h - font_body.height) / 2, fg, label);
    }
}

static void header(ko_surf_t *s, const ko_app_t *a, const char *title, const char *sub)
{
    gfx_vgrad(s, 0, 0, TOP_W, 56, ko_blend(T->bg, T->accent, 2), T->bg);
    gfx_logo(s, 16, 17, 1);
    gfx_text(s, &font_title, 56, 8, T->text, title);
    gfx_text(s, &font_small, 57, 34, T->dim, sub);
    wifi_icon(s, 362, 12, a->wifi);
    gfx_text_right(s, &font_small, 388, 30, T->dim, a->is_new ? "New 3DS" : "Old 3DS");
}

static void footer(ko_surf_t *s, const char *hints)
{
    gfx_fill(s, 0, 218, TOP_W, 22, T->card);
    gfx_text_center(s, &font_small, TOP_W / 2, 222, T->dim, hints);
}

void scr_toast(ko_surf_t *s, const ko_app_t *a)
{
    if (!a->toast[0] || a->now_ms > a->toast_until) return;
    int w = gfx_text_w(&font_body, a->toast) + 28;
    int x = (s->w - w) / 2;
    gfx_round(s, x, 186, w, 28, 14, T->accent);
    gfx_text_center(s, &font_body, s->w / 2, 186 + (28 - font_body.height) / 2, T->bg, a->toast);
}

/* ---------------------------------------------------------------- home */

void scr_home_top(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    header(s, a, "Kasumi Old", "Nuvem e PC no seu 3DS de qualquer geração");

    if (!a->net_ok || !a->my_ip) {
        gfx_round(s, 20, 76, 360, 110, 12, T->card);
        gfx_text_center(s, &font_body, 200, 96, T->warn, "Sem Wi-Fi");
        gfx_text_wrap(s, &font_small, 40, 122, 320, T->dim,
                      "Ligue o Wi-Fi do 3DS (botão ou chave lateral) e conecte numa rede de 2,4 GHz. "
                      "O PC com o Kasumi Bridge precisa estar na mesma rede.");
        footer(s, "Y: Tentar de novo  ·  SELECT: Ajustes  ·  START: Sair");
        return;
    }

    gfx_text(s, &font_small, 20, 62, T->dim, a->npeers ? "SEUS PCs" : "PROCURANDO PCs NA REDE...");

    if (!a->npeers) {
        gfx_round(s, 20, 82, 360, 104, 12, T->card);
        int dots = (int)((a->now_ms / 400) % 4);
        char buf[8] = "";
        for (int i = 0; i < dots; i++) strcat(buf, ".");
        gfx_text_center(s, &font_body, 200, 98, T->text, "Nenhum PC encontrado ainda");
        gfx_text_center(s, &font_body, 200 + gfx_text_w(&font_body, "Nenhum PC encontrado ainda") / 2 + 8, 98, T->accent, buf);
        gfx_text_wrap(s, &font_small, 40, 124, 320, T->dim,
                      "Abra o Kasumi Bridge no PC. Ele aparece aqui sozinho. "
                      "Se não aparecer, aperte X e digite o IP mostrado na janela do Bridge.");
    }

    int first = a->scroll < a->npeers ? a->scroll : 0;
    for (int i = 0; i < 4 && first + i < a->npeers; i++) {
        const ko_peer_t *p = &a->peers[first + i];
        int y = 80 + i * 34;
        int sel = (first + i) == a->sel;
        gfx_round(s, 20, y, 360, 30, 9, sel ? T->card_hi : T->card);
        if (sel) gfx_round(s, 20, y + 6, 4, 18, 2, T->accent);
        gfx_round(s, 34, y + 11, 8, 8, 4, p->online ? T->ok : T->dim);
        gfx_text(s, &font_body, 50, y + 6, T->text, p->name);
        char ip[16];
        ip_to_str(p->ip, ip);
        gfx_text_right(s, &font_small, 368, y + 4, T->dim, ip);
        gfx_text_right(s, &font_small, 368, y + 16, p->online ? T->ok : T->dim, p->online ? "online" : "recente");
    }
    if (a->npeers > 4) {
        char more[32];
        snprintf(more, sizeof(more), "%d de %d", a->sel + 1, a->npeers);
        gfx_text_right(s, &font_small, 380, 62, T->dim, more);
    }
    footer(s, "A: Conectar  ·  X: Digitar IP  ·  Y: Buscar  ·  SELECT: Ajustes");
}

static const ko_rect_t home_btn[4] = {
    { 10, 10, 145, 92 }, { 165, 10, 145, 92 }, { 10, 110, 145, 92 }, { 165, 110, 145, 92 },
};

int scr_home_hit(int tx, int ty)
{
    for (int i = 0; i < 4; i++) if (hit(home_btn[i], tx, ty)) return i;
    return -1;
}

void scr_home_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    const char *peer = (a->npeers && a->sel < a->npeers) ? a->peers[a->sel].name : "nenhum PC";
    button(s, home_btn[0], "Conectar", peer, a->npeers > 0, 0);
    button(s, home_btn[1], "Digitar IP", "conexão manual", 0, 0);
    button(s, home_btn[2], "Ajustes", ko_presets[a->cfg.preset].name, 0, 0);
    button(s, home_btn[3], "Como usar", "passo a passo", 0, 0);

    char line[96], ip[16];
    if (a->my_ip) ip_to_str(a->my_ip, ip); else strcpy(ip, "sem rede");
    snprintf(line, sizeof(line), "3DS: %s  ·  Áudio %s  ·  %s", ip,
             a->audio_ok ? "ok" : "off", a->has_cpp ? "ZL/ZR ok" : "toque p/ LT/RT");
    gfx_text_center(s, &font_small, BOT_W / 2, 216, T->dim, line);
    scr_toast(s, a);
}

/* ---------------------------------------------------------------- ajustes */

static const char *setting_names[ST_COUNT] = {
    "Qualidade", "Ajuste automático", "Áudio", "Botões A/B/X/Y", "L e R",
    "Tela de toque", "Giroscópio (mira)", "Sensibilidade do giro", "Inverter giro vertical",
    "Eixo do giro", "Zona morta do analógico", "Tema", "Esquecer PCs recentes",
};

static const char *setting_help[ST_COUNT] = {
    "Leve: para Wi-Fi fraco. Equilibrado: o melhor para a maioria. Fluido: mais quadros, imagem mais simples. Nítido: textos legíveis, menos quadros. Com o ajuste automático, o PC nunca passa do preset escolhido.",
    "Ligado: o PC baixa ou sobe a qualidade sozinho conforme a perda de pacotes e o tempo de decodificação do 3DS.",
    "Som do PC no 3DS (24 kHz). Precisa do arquivo dspfirm.cdc em sd:/3ds (gere com o DSP1).",
    "Posicional: o botão de baixo (B) vira o A do Xbox, como em jogos de Switch. Rótulos: A é A, B é B.",
    "No Old 3DS sem Circle Pad Pro não existem ZL/ZR. Escolha se L/R são os ombros (LB/RB) ou os gatilhos (LT/RT). O que sobrar fica na tela de toque.",
    "Analógico direito: arraste para mirar/olhar. Mouse: arraste move o cursor do PC, toque rápido = clique, toque longo = clique direito.",
    "Use o giroscópio para mirar, como no Kasumi original. Funciona no Old 3DS também.",
    "Quanto maior, mais o personagem gira para o mesmo movimento do console.",
    "Inverte o movimento vertical do giroscópio.",
    "Padrão: girar o console para os lados (como volante de mira). Alternativo: inclinar o console para os lados.",
    "Ignora pequenos desvios do Circle Pad. Aumente se o personagem andar sozinho.",
    "Cores da interface. A tela do jogo não muda.",
    "Remove a lista de PCs usados recentemente. Aperte A para confirmar.",
};

static const char *onoff(int v) { return v ? "Ligado" : "Desligado"; }

const char *scr_setting_value(const ko_app_t *a, int item, char *buf)
{
    const ko_config_t *c = &a->cfg;
    switch (item) {
    case ST_PRESET:    return ko_presets[c->preset].name;
    case ST_AUTO:      return onoff(c->auto_adjust);
    case ST_AUDIO:     return a->audio_ok ? onoff(c->audio) : "Sem dspfirm";
    case ST_LAYOUT:    return c->layout ? "Rótulos (Xbox)" : "Posicional";
    case ST_TRIG:      return c->trig_mode ? "Gatilhos LT/RT" : "Ombros LB/RB";
    case ST_TOUCH:     return c->touch_mode ? "Mouse" : "Analógico direito";
    case ST_GYRO:      return onoff(c->gyro);
    case ST_GYRO_SENS: sprintf(buf, "%d", c->gyro_sens); return buf;
    case ST_GYRO_INV:  return onoff(c->gyro_invert_y);
    case ST_GYRO_ALT:  return c->gyro_alt ? "Inclinar" : "Girar";
    case ST_DEADZONE:  sprintf(buf, "%d", c->deadzone); return buf;
    case ST_THEME:     return ko_themes[c->theme].name;
    case ST_FORGET:    sprintf(buf, "%d salvos", c->recent_count); return buf;
    }
    return "";
}

void scr_settings_top(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    header(s, a, "Ajustes", "Salvos automaticamente ao sair");
    const int rows = 8;
    for (int i = 0; i < rows && a->scroll + i < ST_COUNT; i++) {
        int it = a->scroll + i;
        int y = 60 + i * 19;
        int sel = it == a->sel;
        if (sel) {
            gfx_round(s, 12, y, 376, 18, 6, T->card_hi);
            gfx_round(s, 12, y + 4, 3, 10, 1, T->accent);
        }
        gfx_text(s, &font_body, 24, y + 1, sel ? T->text : T->dim, setting_names[it]);
        char buf[24];
        const char *v = scr_setting_value(a, it, buf);
        if (sel && it != ST_FORGET) {
            char tmp[48];
            snprintf(tmp, sizeof(tmp), "‹ %s ›", v);
            gfx_text_right(s, &font_body, 380, y + 1, T->accent, tmp);
        } else {
            gfx_text_right(s, &font_body, 380, y + 1, sel ? T->accent : T->text, v);
        }
    }
    /* barra de rolagem */
    int track = rows * 19;
    int th = track * rows / ST_COUNT;
    int ty = 60 + (track - th) * a->scroll / (ST_COUNT - rows);
    gfx_round(s, 393, ty, 3, th, 1, T->card_hi);
    footer(s, "◄ ► Mudar  ·  ▲ ▼ Escolher  ·  B: Voltar");
}

void scr_settings_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    gfx_round(s, 10, 10, 300, 168, 12, T->card);
    gfx_text(s, &font_body, 24, 22, T->accent, setting_names[a->sel]);
    gfx_text_wrap(s, &font_small, 24, 46, 272, T->text, setting_help[a->sel]);
    if (a->sel == ST_PRESET) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Agora: %s", ko_presets[a->cfg.preset].desc);
        gfx_text_wrap(s, &font_small, 24, 140, 272, T->dim, buf);
    }
    ko_rect_t back = { 10, 188, 300, 42 };
    button(s, back, "Salvar e voltar", NULL, 0, 0);
    scr_toast(s, a);
}

/* ---------------------------------------------------------------- conectando */

static void spinner(ko_surf_t *s, int cx, int cy, uint64_t t)
{
    static const signed char pts[8][2] = { {0,-14},{10,-10},{14,0},{10,10},{0,14},{-10,10},{-14,0},{-10,-10} };
    int head = (int)((t / 90) % 8);
    for (int i = 0; i < 8; i++) {
        int age = (head - i + 8) % 8;
        int a16 = 16 - age * 2;
        gfx_round(s, cx + pts[i][0] - 3, cy + pts[i][1] - 3, 6, 6, 3, ko_blend(T->bg, T->accent, a16));
    }
}

void scr_connecting_top(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    gfx_logo(s, 170, 42, 2);
    spinner(s, 200, 120, a->now_ms);
    char buf[64], ip[16];
    snprintf(buf, sizeof(buf), "Conectando a %s", a->host_name);
    gfx_text_center(s, &font_body, 200, 150, T->text, buf);
    ip_to_str(a->host_ip, ip);
    gfx_text_center(s, &font_small, 200, 170, T->dim, ip);
    if (a->now_ms - a->connect_start > 4000)
        gfx_text_center(s, &font_small, 200, 190, T->warn, "Demorando... o Bridge está aberto e liberado no firewall?");
    footer(s, "B: Cancelar");
}

static const char *tips[] = {
    "Segure START + SELECT (ou toque em ⋯) para abrir o menu durante o jogo.",
    "No Old 3DS, LT/RT ficam na tela de toque. Dá para trocar em Ajustes > L e R.",
    "Wi-Fi fraco? Use a qualidade Leve e fique perto do roteador.",
    "GeForce NOW e Xbox Cloud: abra no navegador do PC em tela cheia.",
    "O teclado da tela de toque digita direto no PC: ótimo para logins.",
};

void scr_connecting_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    gfx_round(s, 10, 60, 300, 110, 12, T->card);
    gfx_text(s, &font_small, 26, 74, T->accent, "DICA");
    int n = (int)(sizeof(tips) / sizeof(tips[0]));
    gfx_text_wrap(s, &font_body, 26, 94, 268, T->text, tips[(a->connect_start / 1000) % n]);
}

/* ---------------------------------------------------------------- ajuda */

void scr_help_top(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    header(s, a, "Como usar", "Três passos e você está jogando");
    static const char *steps[3][2] = {
        { "1. No PC", "Rode o Kasumi Bridge (pasta pc). Ele captura a tela e o som e cria um controle de Xbox virtual." },
        { "2. No PC, abra o jogo", "GeForce NOW, Xbox Cloud, Steam, emulador... qualquer coisa em tela cheia." },
        { "3. No 3DS", "Escolha o PC na lista e aperte A. Mesma rede Wi-Fi de 2,4 GHz." },
    };
    int y = 60;
    for (int i = 0; i < 3; i++) {
        gfx_round(s, 16, y, 368, 51, 10, T->card);
        gfx_text(s, &font_body, 28, y + 3, T->accent, steps[i][0]);
        gfx_text_wrap(s, &font_small, 28, y + 20, 346, T->text, steps[i][1]);
        y += 53;
    }
    footer(s, "B: Voltar");
}

void scr_help_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    gfx_round(s, 10, 10, 300, 220, 12, T->card);
    gfx_text(s, &font_body, 24, 20, T->accent, "Controles no jogo");
    gfx_text_wrap(s, &font_small, 24, 44, 276, T->text,
                  "Circle Pad: analógico esquerdo\n"
                  "C-Stick / toque / giroscópio: analógico direito\n"
                  "L/R: ombros ou gatilhos (veja Ajustes)\n"
                  "START: Menu  ·  SELECT: View\n"
                  "Toque: LT, RT, L3, R3, Guide, teclado\n"
                  "START + SELECT (segure): menu do Kasumi\n\n"
                  "Fechar a tampa pausa; ao abrir, reconecta sozinho.");
    (void)a;
}

/* ---------------------------------------------------------------- jogo */

static const ko_rect_t sb_rect[SB_COUNT] = {
    [SB_TRIG_L] = { 4, 4, 76, 40 },   [SB_TRIG_R] = { 240, 4, 76, 40 },
    [SB_L3]     = { 84, 4, 74, 40 },  [SB_R3]     = { 162, 4, 74, 40 },
    [SB_VIEW]   = { 4, 196, 60, 40 }, [SB_GUIDE]  = { 67, 196, 60, 40 },
    [SB_MENU]   = { 130, 196, 60, 40 }, [SB_KEYB] = { 193, 196, 60, 40 },
    [SB_MORE]   = { 256, 196, 60, 40 },
    [SB_PAD]    = { 4, 48, 312, 144 },
};

ko_rect_t scr_stream_pad_rect(void) { return sb_rect[SB_PAD]; }

int scr_stream_hit(const ko_app_t *a, int tx, int ty)
{
    (void)a;
    for (int i = 0; i < SB_COUNT; i++) if (hit(sb_rect[i], tx, ty)) return i;
    return SB_NONE;
}

void scr_stream_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    const int lr_are_triggers = a->cfg.trig_mode == 1;
    const char *labels[SB_COUNT] = {
        [SB_TRIG_L] = lr_are_triggers ? "LB" : "LT", [SB_TRIG_R] = lr_are_triggers ? "RB" : "RT",
        [SB_L3] = "L3", [SB_R3] = "R3",
        [SB_VIEW] = "View", [SB_GUIDE] = "Guide", [SB_MENU] = "Menu", [SB_KEYB] = "Teclado", [SB_MORE] = "⋯",
    };
    for (int i = 0; i < SB_COUNT; i++) {
        if (i == SB_PAD) continue;
        if (a->has_cpp && (i == SB_TRIG_L || i == SB_TRIG_R) && !lr_are_triggers) {
            /* com ZL/ZR fisicos os gatilhos na tela ainda funcionam, mas ficam discretos */
        }
        uint16_t bg = a->pressed == i ? T->accent : T->card;
        gfx_round(s, sb_rect[i].x, sb_rect[i].y, sb_rect[i].w, sb_rect[i].h, 9, bg);
        gfx_text_center(s, &font_body, sb_rect[i].x + sb_rect[i].w / 2,
                        sb_rect[i].y + (sb_rect[i].h - font_body.height) / 2,
                        a->pressed == i ? T->bg : T->text, labels[i]);
    }

    ko_rect_t p = sb_rect[SB_PAD];
    gfx_round(s, p.x, p.y, p.w, p.h, 12, ko_blend(T->bg, T->card, 10));
    gfx_frame(s, p.x, p.y, p.w, p.h, 12, T->card_hi);
    const char *mode = a->cfg.touch_mode ? "Mouse: arraste · toque = clique" : "Analógico direito: arraste";
    gfx_text_center(s, &font_small, p.x + p.w / 2, p.y + p.h - 18, T->dim, mode);

    int cx = p.x + p.w / 2, cy = p.y + p.h / 2 - 6;
    if (a->cfg.touch_mode == 0) {
        gfx_frame(s, cx - 30, cy - 30, 60, 60, 30, T->card_hi);
        int kx = cx, ky = cy;
        if (a->pad_active) {
            kx = cx + (a->pad_x - a->pad_ox) * 30 / 40;
            ky = cy + (a->pad_y - a->pad_oy) * 30 / 40;
            if (kx < cx - 30) kx = cx - 30;
            if (kx > cx + 30) kx = cx + 30;
            if (ky < cy - 30) ky = cy - 30;
            if (ky > cy + 30) ky = cy + 30;
        }
        gfx_round(s, kx - 12, ky - 12, 24, 24, 12, a->pad_active ? T->accent : T->card_hi);
    } else {
        gfx_fill(s, cx - 10, cy, 21, 1, T->card_hi);
        gfx_fill(s, cx, cy - 10, 1, 21, T->card_hi);
        if (a->pad_active) gfx_round(s, a->pad_x - 6, a->pad_y - 6, 12, 12, 6, T->accent);
    }
    if (a->cfg.gyro) gfx_text(s, &font_small, p.x + 10, p.y + 8, T->accent, "giroscópio ligado");
    scr_toast(s, a);
}

static const ko_rect_t sm_rect[SM_COUNT] = {
    [SM_RESUME] = { 10, 10, 300, 40 },
    [SM_KEYB]   = { 10, 56, 147, 40 },  [SM_TOUCH]   = { 163, 56, 147, 40 },
    [SM_GYRO]   = { 10, 102, 147, 40 }, [SM_QUALITY] = { 163, 102, 147, 40 },
    [SM_STATS]  = { 10, 148, 147, 40 }, [SM_DISCONNECT] = { 163, 148, 147, 40 },
};

int scr_stream_menu_hit(int tx, int ty)
{
    for (int i = 0; i < SM_COUNT; i++) if (hit(sm_rect[i], tx, ty)) return i;
    return -1;
}

void scr_stream_menu_bottom(ko_surf_t *s, const ko_app_t *a)
{
    gfx_clear(s, T->bg);
    char q[40];
    snprintf(q, sizeof(q), "%s", ko_presets[a->cfg.preset].name);
    const char *lab[SM_COUNT] = { "Voltar ao jogo", "Teclado", "Toque", "Giroscópio", "Qualidade", "Estatísticas", "Desconectar" };
    const char *sub[SM_COUNT] = {
        NULL, "digitar no PC", a->cfg.touch_mode ? "mouse" : "analógico", a->cfg.gyro ? "ligado" : "desligado",
        q, a->show_stats ? "visíveis" : "ocultas", "voltar ao início",
    };
    for (int i = 0; i < SM_COUNT; i++) button(s, sm_rect[i], lab[i], sub[i], a->menu_sel == i, 0);
    gfx_text_center(s, &font_small, BOT_W / 2, 204, T->dim, "▲▼◄► + A, ou toque  ·  B: voltar ao jogo");
    scr_toast(s, a);
}

void scr_stream_overlay(ko_surf_t *top, const ko_app_t *a)
{
    if (a->show_stats) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f fps  ·  %d kbps  ·  perda %d%%  ·  dec %d ms",
                 (double)a->fps, a->kbps, a->loss_pct, a->decode_ms);
        int w = gfx_text_w(&font_small, buf) + 12;
        gfx_fill_alpha(top, 4, 4, w, 18, T->bg, 11);
        gfx_text(top, &font_small, 10, 6, T->text, buf);
    }
    if (a->signal_lost) {
        gfx_fill_alpha(top, 90, 92, 220, 56, T->bg, 13);
        gfx_text_center(top, &font_body, 200, 100, T->warn, "Sem sinal do PC");
        gfx_text_center(top, &font_small, 200, 122, T->dim, "reconectando... (B + Y: sair)");
    }
}
