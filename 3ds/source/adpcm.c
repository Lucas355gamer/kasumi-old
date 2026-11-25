#include "adpcm.h"

static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int8_t index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

int ko_adpcm_decode(const uint8_t *in, int n_samples, int16_t predictor, int step_index, int16_t *out)
{
    int pred = predictor;
    int idx = step_index;
    if (idx < 0) idx = 0;
    if (idx > 88) idx = 88;

    for (int i = 0; i < n_samples; i++) {
        int nib = (i & 1) ? (in[i >> 1] >> 4) : (in[i >> 1] & 0x0F);
        int step = step_table[idx];
        int diff = step >> 3;
        if (nib & 1) diff += step >> 2;
        if (nib & 2) diff += step >> 1;
        if (nib & 4) diff += step;
        if (nib & 8) pred -= diff; else pred += diff;
        if (pred > 32767) pred = 32767;
        if (pred < -32768) pred = -32768;
        idx += index_table[nib];
        if (idx < 0) idx = 0;
        if (idx > 88) idx = 88;
        out[i] = (int16_t)pred;
    }
    return n_samples;
}
