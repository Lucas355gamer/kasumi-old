/* Stub minimo da API libjpeg(-turbo) usada em video.c, so para checagem de tipos. */
#pragma once
#include <stdio.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define JPEG_HEADER_OK 1
typedef unsigned int JDIMENSION;
typedef unsigned char JSAMPLE;
typedef JSAMPLE *JSAMPROW;
typedef JSAMPROW *JSAMPARRAY;
typedef enum { JCS_UNKNOWN, JCS_GRAYSCALE, JCS_RGB, JCS_RGB565 = 16 } J_COLOR_SPACE;
typedef enum { JDCT_ISLOW, JDCT_IFAST, JDCT_FLOAT } J_DCT_METHOD;
typedef enum { JDITHER_NONE, JDITHER_ORDERED, JDITHER_FS } J_DITHER_MODE;
struct jpeg_common_struct;
typedef struct jpeg_common_struct *j_common_ptr;
struct jpeg_error_mgr {
    void (*error_exit)(j_common_ptr cinfo);
    void (*output_message)(j_common_ptr cinfo);
    int msg_code;
};
struct jpeg_common_struct { struct jpeg_error_mgr *err; };
struct jpeg_decompress_struct {
    struct jpeg_error_mgr *err;
    JDIMENSION image_width, image_height;
    J_COLOR_SPACE out_color_space;
    J_DCT_METHOD dct_method;
    boolean do_fancy_upsampling, do_block_smoothing;
    J_DITHER_MODE dither_mode;
    JDIMENSION output_width, output_height, output_scanline;
    int rec_outbuf_height;
};
typedef struct jpeg_decompress_struct *j_decompress_ptr;
struct jpeg_error_mgr *jpeg_std_error(struct jpeg_error_mgr *err);
#define jpeg_create_decompress(c) jpeg_CreateDecompress((c), 62, sizeof(struct jpeg_decompress_struct))
void jpeg_CreateDecompress(j_decompress_ptr cinfo, int version, size_t structsize);
void jpeg_mem_src(j_decompress_ptr cinfo, const unsigned char *inbuffer, unsigned long insize);
int jpeg_read_header(j_decompress_ptr cinfo, boolean require_image);
boolean jpeg_start_decompress(j_decompress_ptr cinfo);
JDIMENSION jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines);
boolean jpeg_finish_decompress(j_decompress_ptr cinfo);
void jpeg_abort_decompress(j_decompress_ptr cinfo);
