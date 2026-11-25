#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

const ko_preset_t ko_presets[KO_PRESET_COUNT] = {
    /* calibrado para ~0,6-1,1 Mbps: o Wi-Fi do 3DS comeca a perder pacotes perto de 1,3 Mbps */
    { "Leve",        "8 FPS, Wi-Fi fraco ou longe do roteador (~0,6 Mbps)", 8, 35 },
    { "Equilibrado", "12 FPS, o melhor para a maioria (~1 Mbps)",           12, 45 },
    { "Fluido",      "15 FPS, imagem mais simples (~1,1 Mbps)",             15, 35 },
    { "Nítido",      "10 FPS, textos mais legíveis (~1,1 Mbps)",            10, 65 },
};

void cfg_defaults(ko_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->theme = 0;
    c->preset = KO_PRESET_EQUILIBRADO;
    c->auto_adjust = 1;
    c->audio = 1;
    c->layout = 0;
    c->trig_mode = 0;
    c->gyro = 0;
    c->gyro_sens = 5;
    c->gyro_invert_y = 0;
    c->touch_mode = 0;
    c->deadzone = 12;
}

void ip_to_str(uint32_t ip, char *out)
{
    const uint8_t *b = (const uint8_t *)&ip;
    sprintf(out, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

int ip_from_str(const char *s, uint32_t *ip)
{
    unsigned a, b, c, d;
    char tail;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    uint8_t bytes[4] = { (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d };
    memcpy(ip, bytes, 4);
    return 1;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int cfg_load(ko_config_t *c, const char *path)
{
    cfg_defaults(c);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        int n = atoi(v);
        if (!strcmp(k, "theme")) c->theme = clampi(n, 0, 4);
        else if (!strcmp(k, "preset")) c->preset = clampi(n, 0, KO_PRESET_COUNT - 1);
        else if (!strcmp(k, "auto_adjust")) c->auto_adjust = !!n;
        else if (!strcmp(k, "audio")) c->audio = !!n;
        else if (!strcmp(k, "layout")) c->layout = !!n;
        else if (!strcmp(k, "trig_mode")) c->trig_mode = !!n;
        else if (!strcmp(k, "gyro")) c->gyro = !!n;
        else if (!strcmp(k, "gyro_sens")) c->gyro_sens = clampi(n, 1, 10);
        else if (!strcmp(k, "gyro_invert_y")) c->gyro_invert_y = !!n;
        else if (!strcmp(k, "gyro_alt")) c->gyro_alt = !!n;
        else if (!strcmp(k, "touch_mode")) c->touch_mode = !!n;
        else if (!strcmp(k, "deadzone")) c->deadzone = clampi(n, 0, 40);
        else if (!strcmp(k, "recent") && c->recent_count < KO_MAX_RECENT) {
            /* recent=ip,nome */
            char *comma = strchr(v, ',');
            if (comma) *comma = 0;
            ko_host_t *h = &c->recent[c->recent_count];
            if (ip_from_str(v, &h->ip)) {
                snprintf(h->name, sizeof(h->name), "%s", comma ? comma + 1 : "PC");
                c->recent_count++;
            }
        }
    }
    fclose(f);
    return 1;
}

int cfg_save(const ko_config_t *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "theme=%d\npreset=%d\nauto_adjust=%d\naudio=%d\nlayout=%d\ntrig_mode=%d\n",
            c->theme, c->preset, c->auto_adjust, c->audio, c->layout, c->trig_mode);
    fprintf(f, "gyro=%d\ngyro_sens=%d\ngyro_invert_y=%d\ngyro_alt=%d\ntouch_mode=%d\ndeadzone=%d\n",
            c->gyro, c->gyro_sens, c->gyro_invert_y, c->gyro_alt, c->touch_mode, c->deadzone);
    for (int i = 0; i < c->recent_count; i++) {
        char ip[16];
        ip_to_str(c->recent[i].ip, ip);
        fprintf(f, "recent=%s,%s\n", ip, c->recent[i].name);
    }
    fclose(f);
    return 1;
}

void cfg_add_recent(ko_config_t *c, uint32_t ip, const char *name)
{
    ko_host_t h;
    h.ip = ip;
    snprintf(h.name, sizeof(h.name), "%s", name && *name ? name : "PC");
    /* remove duplicata */
    for (int i = 0; i < c->recent_count; i++)
        if (c->recent[i].ip == ip) {
            memmove(&c->recent[i], &c->recent[i + 1], (size_t)(c->recent_count - i - 1) * sizeof(ko_host_t));
            c->recent_count--;
            break;
        }
    if (c->recent_count == KO_MAX_RECENT) c->recent_count--;
    memmove(&c->recent[1], &c->recent[0], (size_t)c->recent_count * sizeof(ko_host_t));
    c->recent[0] = h;
    c->recent_count++;
}
