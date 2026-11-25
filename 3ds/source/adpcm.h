#ifndef KO_ADPCM_H
#define KO_ADPCM_H
#include <stdint.h>

/* Decodifica IMA ADPCM mono (nibble baixo primeiro). Retorna amostras escritas. */
int ko_adpcm_decode(const uint8_t *in, int n_samples, int16_t predictor, int step_index, int16_t *out);

#endif
