#ifndef KO_CONFIG_H
#define KO_CONFIG_H
#include <stdint.h>

#define KO_MAX_RECENT 4

typedef struct {
    char     name[32];
    uint32_t ip;          /* ordem de rede (como sin_addr.s_addr) */
} ko_host_t;

typedef struct {
    int theme;
    int preset;           /* KO_PRESET_* */
    int auto_adjust;
    int audio;
    int layout;           /* 0 = posicional Nintendo, 1 = rotulos Xbox */
    int trig_mode;        /* 0 = L/R sao LB/RB, 1 = L/R sao LT/RT */
    int gyro;
    int gyro_sens;        /* 1..10 */
    int gyro_invert_y;
    int gyro_alt;         /* eixo alternativo de giro (se a mira girar errado) */
    int touch_mode;       /* 0 = analogico direito, 1 = mouse */
    int deadzone;         /* 0..40 (em unidades do circle pad) */
    int recent_count;
    ko_host_t recent[KO_MAX_RECENT];
} ko_config_t;

enum { KO_PRESET_LEVE, KO_PRESET_EQUILIBRADO, KO_PRESET_FLUIDO, KO_PRESET_NITIDO, KO_PRESET_COUNT };

typedef struct { const char *name; const char *desc; int fps; int quality; } ko_preset_t;
extern const ko_preset_t ko_presets[KO_PRESET_COUNT];

void cfg_defaults(ko_config_t *c);
int  cfg_load(ko_config_t *c, const char *path);
int  cfg_save(const ko_config_t *c, const char *path);
void cfg_add_recent(ko_config_t *c, uint32_t ip, const char *name);

void ip_to_str(uint32_t ip, char *out);       /* out >= 16 bytes */
int  ip_from_str(const char *s, uint32_t *ip);

#endif
