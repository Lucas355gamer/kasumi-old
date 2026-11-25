#ifndef KO_AUDIO_H
#define KO_AUDIO_H
#include <stdint.h>

int  audio_init(void);   /* 0 se o DSP nao estiver disponivel (falta dspfirm.cdc) */
void audio_exit(void);
int  audio_available(void);
void audio_reset(void);  /* limpa a fila (inicio/fim de sessao) */
/* payload AUDIO sem o cabecalho de 4 bytes */
void audio_push(const uint8_t *p, int len);
uint32_t audio_take_lost(void);

#endif
