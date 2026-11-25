#ifndef KO_VIDEO_MODULE_H
#define KO_VIDEO_MODULE_H
#include <stdint.h>

/*
 * Decodifica um JPEG 240x400 (ja rotacionado pela ponte) direto no
 * framebuffer RGB565 da tela superior. Retorna 1 se ok.
 * Usa libjpeg-turbo com IDCT rapida, sem suavizacao e sem dithering:
 * e o caminho mais barato possivel para o ARM11 de 268 MHz.
 */
int video_decode_to_fb(const uint8_t *jpg, uint32_t size, uint16_t *fb);

#endif
