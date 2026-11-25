/* Remontagem de quadros JPEG fragmentados (codigo portavel, testado no PC). */
#ifndef KO_REASM_H
#define KO_REASM_H
#include <stdint.h>
#include "protocol.h"

#define KO_REASM_SLOTS 2

typedef struct {
    int      active;
    uint16_t frame_id;
    uint8_t  count;
    uint8_t  received;
    uint32_t total;
    uint8_t  have[256 / 8];
    uint8_t  data[KO_VIDEO_MAX];
} ko_slot_t;

typedef struct {
    ko_slot_t slot[KO_REASM_SLOTS];
    int       have_last;
    uint16_t  last_done;      /* ultimo quadro entregue                    */
    uint32_t  frames_ok;
    uint32_t  frames_lost;    /* quadros abandonados incompletos ou pulados */
} ko_reasm_t;

void ko_reasm_init(ko_reasm_t *r);

/*
 * Alimenta um payload VIDEO (sem o cabecalho de 4 bytes).
 * Retorna ponteiro para o JPEG completo e preenche *size quando um quadro
 * fica pronto; o ponteiro vale ate a proxima chamada. Retorna NULL caso contrario.
 */
const uint8_t *ko_reasm_push(ko_reasm_t *r, const uint8_t *p, int len, uint32_t *size);

#endif
