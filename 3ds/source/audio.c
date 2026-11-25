#include <3ds.h>
#include <string.h>
#include "audio.h"
#include "adpcm.h"
#include "protocol.h"

#define NBUF        12      /* ~512 ms de capacidade total */
#define PREBUFFER   3       /* ~128 ms antes de comecar a tocar */
#define MAX_QUEUED  7       /* acima disso descartamos para nao acumular atraso */
#define CH          0

static int ok = 0;
static ndspWaveBuf wb[NBUF];
static int16_t *pcm = NULL;
static int next = 0;
static int pending = 0;          /* buffers decodificados aguardando pre-buffer */
static int pending_idx[NBUF];
static uint16_t last_seq;
static int have_seq = 0;
static uint32_t lost = 0;

int audio_init(void)
{
    if (R_FAILED(ndspInit())) return 0;
    pcm = (int16_t *)linearAlloc(NBUF * KO_AUDIO_BLOCK * sizeof(int16_t));
    if (!pcm) { ndspExit(); return 0; }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(CH);
    ndspChnSetInterp(CH, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CH, (float)KO_AUDIO_RATE);
    ndspChnSetFormat(CH, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = { 1.0f, 1.0f };
    ndspChnSetMix(CH, mix);

    memset(wb, 0, sizeof(wb));
    for (int i = 0; i < NBUF; i++) {
        wb[i].data_vaddr = pcm + i * KO_AUDIO_BLOCK;
        wb[i].nsamples = KO_AUDIO_BLOCK;
        wb[i].status = NDSP_WBUF_DONE;
    }
    ok = 1;
    return 1;
}

void audio_exit(void)
{
    if (!ok) return;
    ndspChnWaveBufClear(CH);
    ndspExit();
    linearFree(pcm);
    pcm = NULL;
    ok = 0;
}

int audio_available(void) { return ok; }

void audio_reset(void)
{
    if (!ok) return;
    ndspChnWaveBufClear(CH);
    for (int i = 0; i < NBUF; i++) wb[i].status = NDSP_WBUF_DONE;
    pending = 0;
    have_seq = 0;
    next = 0;
}

static int queued_count(void)
{
    int n = 0;
    for (int i = 0; i < NBUF; i++)
        if (wb[i].status == NDSP_WBUF_QUEUED || wb[i].status == NDSP_WBUF_PLAYING) n++;
    return n;
}

static int is_pending(int i)
{
    for (int k = 0; k < pending; k++) if (pending_idx[k] == i) return 1;
    return 0;
}

void audio_push(const uint8_t *p, int len)
{
    if (!ok || len < KO_AUDIO_HDR) return;
    uint16_t seq = ko_get16(p);
    int n = ko_get16(p + 2);
    int16_t pred = (int16_t)ko_get16(p + 4);
    int idx = p[6];
    if (n != KO_AUDIO_BLOCK || len < KO_AUDIO_HDR + n / 2) return;

    if (have_seq) {
        int16_t d = (int16_t)(uint16_t)(seq - last_seq);
        if (d <= 0) return;                 /* atrasado/duplicado */
        if (d > 1) lost += (uint32_t)(d - 1);
    }
    have_seq = 1;
    last_seq = seq;

    int q = queued_count();
    if (q >= MAX_QUEUED) return;            /* evita latencia crescente */

    /* procura buffer livre */
    int slot = -1;
    for (int k = 0; k < NBUF; k++) {
        int i = (next + k) % NBUF;
        if ((wb[i].status == NDSP_WBUF_DONE || wb[i].status == NDSP_WBUF_FREE) && !is_pending(i)) { slot = i; break; }
    }
    if (slot < 0) return;
    next = (slot + 1) % NBUF;

    ko_adpcm_decode(p + KO_AUDIO_HDR, n, pred, idx, (int16_t *)wb[slot].data_vaddr);
    DSP_FlushDataCache(wb[slot].data_vaddr, (u32)(n * sizeof(int16_t)));

    if (q == 0 || pending > 0) {
        /* fila vazia: junta alguns blocos antes de tocar (anti-engasgo) */
        pending_idx[pending++] = slot;
        if (pending >= PREBUFFER) {
            for (int k = 0; k < pending; k++) ndspChnWaveBufAdd(CH, &wb[pending_idx[k]]);
            pending = 0;
        }
        return;
    }
    ndspChnWaveBufAdd(CH, &wb[slot]);
}

uint32_t audio_take_lost(void)
{
    uint32_t l = lost;
    lost = 0;
    return l;
}
