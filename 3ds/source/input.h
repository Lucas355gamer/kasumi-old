#ifndef KO_INPUT_H
#define KO_INPUT_H
#include <stdint.h>
#include "config.h"

typedef struct {
    uint32_t buttons;            /* KO_B_* */
    int lx, ly, rx, ry;          /* -32767..32767 */
    int lt, rt;                  /* 0..255 */
    int mdx, mdy;                /* mouse relativo */
    int mbtn;                    /* KO_M_* */
} ko_pad_t;

void input_init(void);
void input_exit(void);
int  input_is_new3ds(void);
int  input_has_cpp(void);        /* Circle Pad Pro / ZL-ZR / C-Stick presentes */
void input_scan(void);           /* chamar uma vez por frame, depois de hidScanInput */
uint32_t input_held(void);       /* hid + irrst combinados */
uint32_t input_down(void);
void input_gyro(int on);

/* monta o estado do controle a partir dos botoes fisicos, analogicos e giroscopio */
void input_build(const ko_config_t *c, ko_pad_t *out);

/* serializa em pacote INPUT (retorna tamanho) */
int  input_pack(const ko_pad_t *pad, uint32_t seq, uint8_t *buf);

#endif
