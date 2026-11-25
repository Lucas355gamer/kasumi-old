#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "video.h"
#include "protocol.h"

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
} ko_jerr_t;

static void on_error(j_common_ptr cinfo)
{
    ko_jerr_t *e = (ko_jerr_t *)cinfo->err;
    longjmp(e->jb, 1);
}

static void on_message(j_common_ptr cinfo) { (void)cinfo; }

static struct jpeg_decompress_struct dec;
static ko_jerr_t jerr;
static int ready = 0;

int video_decode_to_fb(const uint8_t *jpg, uint32_t size, uint16_t *fb)
{
    if (!ready) {
        dec.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = on_error;
        jerr.pub.output_message = on_message;
        jpeg_create_decompress(&dec);
        ready = 1;
    }

    if (setjmp(jerr.jb)) {
        jpeg_abort_decompress(&dec);
        return 0;
    }

    jpeg_mem_src(&dec, (unsigned char *)jpg, (unsigned long)size);
    if (jpeg_read_header(&dec, TRUE) != JPEG_HEADER_OK) {
        jpeg_abort_decompress(&dec);
        return 0;
    }
    if (dec.image_width != KO_VIDEO_W || dec.image_height != KO_VIDEO_H) {
        jpeg_abort_decompress(&dec);
        return 0;
    }

    dec.out_color_space = JCS_RGB565;
    dec.dct_method = JDCT_IFAST;
    dec.do_fancy_upsampling = FALSE;
    dec.do_block_smoothing = FALSE;
    dec.dither_mode = JDITHER_NONE;

    jpeg_start_decompress(&dec);
    while (dec.output_scanline < dec.output_height) {
        JSAMPROW rows[4];
        int n = (int)dec.rec_outbuf_height;
        if (n > 4) n = 4;
        for (int i = 0; i < n; i++) {
            unsigned line = dec.output_scanline + (unsigned)i;
            if (line >= dec.output_height) line = dec.output_height - 1;
            rows[i] = (JSAMPROW)(fb + line * KO_VIDEO_W);
        }
        jpeg_read_scanlines(&dec, rows, (JDIMENSION)n);
    }
    jpeg_finish_decompress(&dec);
    return 1;
}
