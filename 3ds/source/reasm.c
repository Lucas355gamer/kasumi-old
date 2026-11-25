#include <string.h>
#include "reasm.h"

static int16_t id_diff(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b); }

void ko_reasm_init(ko_reasm_t *r)
{
    for (int i = 0; i < KO_REASM_SLOTS; i++) r->slot[i].active = 0;
    r->have_last = 0;
    r->last_done = 0;
    r->frames_ok = 0;
    r->frames_lost = 0;
}

static ko_slot_t *find_slot(ko_reasm_t *r, uint16_t id, uint8_t count, uint32_t total)
{
    ko_slot_t *free_s = NULL, *oldest = NULL;
    for (int i = 0; i < KO_REASM_SLOTS; i++) {
        ko_slot_t *s = &r->slot[i];
        if (s->active && s->frame_id == id) {
            if (s->count != count || s->total != total) break; /* inconsistente: reinicia */
            return s;
        }
        if (!s->active) { if (!free_s) free_s = s; }
        else if (!oldest || id_diff(s->frame_id, oldest->frame_id) < 0) oldest = s;
    }

    ko_slot_t *s = NULL;
    for (int i = 0; i < KO_REASM_SLOTS; i++)
        if (r->slot[i].active && r->slot[i].frame_id == id) s = &r->slot[i];
    if (!s) s = free_s;
    if (!s) {
        /* sem espaco: so descarta o mais antigo se o novo for mais recente */
        if (oldest && id_diff(id, oldest->frame_id) < 0) return NULL;
        s = oldest;
    }
    if (!s) return NULL;

    s->active = 1;
    s->frame_id = id;
    s->count = count;
    s->total = total;
    s->received = 0;
    memset(s->have, 0, sizeof(s->have));
    return s;
}

const uint8_t *ko_reasm_push(ko_reasm_t *r, const uint8_t *p, int len, uint32_t *size)
{
    if (len < KO_VIDEO_HDR) return NULL;
    uint16_t id    = ko_get16(p);
    uint8_t  idx   = p[2];
    uint8_t  count = p[3];
    uint32_t total = ko_get32(p + 4);
    const uint8_t *data = p + KO_VIDEO_HDR;
    int dlen = len - KO_VIDEO_HDR;

    if (count == 0 || idx >= count || total == 0 || total > KO_VIDEO_MAX) return NULL;
    uint32_t off = (uint32_t)idx * KO_VIDEO_CHUNK;
    if (off + (uint32_t)dlen > total) return NULL;
    if (idx < count - 1 && dlen != KO_VIDEO_CHUNK) return NULL;
    if (idx == count - 1 && off + (uint32_t)dlen != total) return NULL;
    if ((uint32_t)(count - 1) * KO_VIDEO_CHUNK >= total) return NULL;

    /* quadro velho (ja passamos dele) */
    if (r->have_last && id_diff(id, r->last_done) <= 0) return NULL;

    ko_slot_t *s = find_slot(r, id, count, total);
    if (!s) return NULL;

    uint8_t bit = (uint8_t)(1u << (idx & 7));
    if (!(s->have[idx >> 3] & bit)) {
        s->have[idx >> 3] |= bit;
        memcpy(s->data + off, data, (size_t)dlen);
        s->received++;
    }
    if (s->received != s->count) return NULL;

    /* quadro completo */
    if (r->have_last) {
        int gap = id_diff(id, r->last_done) - 1;
        if (gap > 0) r->frames_lost += (uint32_t)gap;
    }
    r->have_last = 1;
    r->last_done = id;
    r->frames_ok++;

    for (int i = 0; i < KO_REASM_SLOTS; i++) {
        ko_slot_t *o = &r->slot[i];
        if (o->active && id_diff(o->frame_id, id) <= 0) o->active = 0;
    }
    *size = s->total;
    return s->data;
}
