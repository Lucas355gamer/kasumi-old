/*
 * Kasumi Old - protocolo de rede entre o 3DS e a ponte do PC.
 *
 * Tudo trafega por UDP, little-endian, numa unica porta (KO_PORT).
 * Cabecalho comum de 4 bytes:  'K' 'O' tipo versao
 *
 * Este arquivo e compartilhado com os testes no PC; a ponte em Python
 * (pc/kasumi_bridge.py) implementa exatamente os mesmos formatos.
 */
#ifndef KO_PROTOCOL_H
#define KO_PROTOCOL_H

#include <stdint.h>

#define KO_PORT            39770
#define KO_MAGIC0          'K'
#define KO_MAGIC1          'O'
#define KO_VERSION         1
#define KO_HDR_SIZE        4

/* ---- 3DS -> PC ---- */
#define KO_T_DISCOVER      0x01  /* broadcast, sem payload                  */
#define KO_T_HELLO         0x10  /* pede inicio do stream                   */
#define KO_T_INPUT         0x11  /* estado do controle (tambem e heartbeat) */
#define KO_T_STATS         0x12  /* estatisticas p/ ajuste automatico       */
#define KO_T_TEXT          0x13  /* texto UTF-8 do teclado remoto           */
#define KO_T_BYE           0x14  /* fim da sessao                           */

/* ---- PC -> 3DS ---- */
#define KO_T_ANNOUNCE      0x02  /* resposta ao DISCOVER: nome do PC        */
#define KO_T_VIDEO         0x20  /* fragmento de quadro JPEG                */
#define KO_T_AUDIO         0x21  /* bloco de audio IMA ADPCM mono           */
#define KO_T_WELCOME       0x22  /* resposta ao HELLO                       */

/*
 * HELLO payload (4 bytes):
 *   u8 fps alvo (5..30)   u8 qualidade JPEG (20..90)
 *   u8 audio (0/1)        u8 flags (bit0 = ajuste automatico)
 */
#define KO_HELLO_SIZE      4
#define KO_HELLO_F_AUTO    0x01

/*
 * VIDEO payload:
 *   u16 frame_id  u8 chunk_idx  u8 chunk_count  u32 total_size  bytes...
 * O JPEG ja vem rotacionado 90 graus (240 de largura x 400 de altura) para
 * cair direto no layout do framebuffer do 3DS, sem copia nem rotacao.
 */
#define KO_VIDEO_HDR       8
#define KO_VIDEO_CHUNK     1100   /* bytes de dados por fragmento */
#define KO_VIDEO_MAX       (64 * 1024)
#define KO_VIDEO_W         240    /* largura do JPEG (= altura da tela) */
#define KO_VIDEO_H         400    /* altura do JPEG  (= largura da tela) */

/*
 * AUDIO payload:
 *   u16 seq  u16 n_samples  s16 predictor  u8 step_index  u8 reservado
 *   n_samples/2 bytes de IMA ADPCM (nibble baixo primeiro)
 */
#define KO_AUDIO_HDR       8
#define KO_AUDIO_RATE      24000
#define KO_AUDIO_BLOCK     1024   /* amostras por pacote (~42,7 ms) */

/*
 * INPUT payload (28 bytes):
 *   u32 seq  u32 botoes (KO_B_*)
 *   s16 lx ly rx ry  (-32767..32767, y positivo = para cima)
 *   u8 lt  u8 rt     (0..255)
 *   s16 mouse_dx  s16 mouse_dy  u8 mouse_botoes  u8 flags
 *   u16 reservado
 */
#define KO_INPUT_SIZE      28

#define KO_B_A             (1u << 0)
#define KO_B_B             (1u << 1)
#define KO_B_X             (1u << 2)
#define KO_B_Y             (1u << 3)
#define KO_B_LB            (1u << 4)
#define KO_B_RB            (1u << 5)
#define KO_B_BACK          (1u << 6)
#define KO_B_START         (1u << 7)
#define KO_B_GUIDE         (1u << 8)
#define KO_B_LS            (1u << 9)
#define KO_B_RS            (1u << 10)
#define KO_B_UP            (1u << 11)
#define KO_B_DOWN          (1u << 12)
#define KO_B_LEFT          (1u << 13)
#define KO_B_RIGHT         (1u << 14)

#define KO_M_LEFT          0x01
#define KO_M_RIGHT         0x02

/*
 * TEXT payload: u16 id  u8 flags (bit0 = apertar Enter no fim)  texto UTF-8
 * Enviado 3x por causa do UDP; a ponte ignora ids repetidos.
 *
 * ANNOUNCE / WELCOME payload: nome do PC em UTF-8 (ate 31 bytes).
 */
#define KO_TEXT_F_ENTER    0x01

/*
 * STATS payload (8 bytes):
 *   u16 quadros_ok  u16 quadros_perdidos  u16 ms_medio_decodificacao
 *   u16 pacotes_audio_perdidos      (contagens desde o ultimo STATS)
 */
#define KO_STATS_SIZE      8

/* ---- helpers little-endian (sem depender de alinhamento) ---- */
static inline void ko_put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void ko_put32(uint8_t *p, uint32_t v) { ko_put16(p, (uint16_t)v); ko_put16(p + 2, (uint16_t)(v >> 16)); }
static inline uint16_t ko_get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t ko_get32(const uint8_t *p) { return (uint32_t)ko_get16(p) | ((uint32_t)ko_get16(p + 2) << 16); }

static inline void ko_header(uint8_t *p, uint8_t type)
{
    p[0] = KO_MAGIC0; p[1] = KO_MAGIC1; p[2] = type; p[3] = KO_VERSION;
}

static inline int ko_check(const uint8_t *p, int len)
{
    return len >= KO_HDR_SIZE && p[0] == KO_MAGIC0 && p[1] == KO_MAGIC1 && p[3] == KO_VERSION;
}

#endif
