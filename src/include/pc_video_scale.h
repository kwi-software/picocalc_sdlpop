#pragma once
#include "pc_media.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* RGB565 channel-wise area averaging. Weight is in fifths; rounding must
   happen independently to prevent carries between blue, green and red. */
static inline uint16_t pc_rgb565_area_mix5(uint16_t a, uint16_t b, unsigned weight) {
    unsigned inv = 5 - weight;
    unsigned red = ((a >> 11) * inv + (b >> 11) * weight + 2) / 5;
    unsigned green = (((a >> 5) & 63) * inv + ((b >> 5) & 63) * weight + 2) / 5;
    unsigned blue = ((a & 31) * inv + (b & 31) * weight + 2) / 5;
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

/* One output row; caller validates dimensions, pitch and source storage.
   Filter only the game's 200 -> 240 correction. Other ratios retain nearest
   sampling; 200 -> 200 is an exact copy. No extra framebuffer is allocated. */
static inline void pc_scale_rgb565_row(uint16_t *out, const uint16_t *pixels,
                                      unsigned pitch, unsigned width,
                                      unsigned source_height, unsigned height,
                                      unsigned y, const PC_VideoFilter *filter) {
    unsigned row, weight = 0;
    if (source_height == 200 && height == 240 && (!filter || filter->enabled)) {
        /* Source rows span six units, output rows five. For each six-row
           output group: A, (A+4B)/5, (2B+3C)/5, (3C+2D)/5, (4D+E)/5, E.
           Two rows are exact copies; all others average their covered area. */
        row = (5 * y) / 6;
        unsigned phase = y % 6;
        if (phase > 0 && phase < 5)
            weight = 5 - phase;
    } else {
        row = (uint64_t)y * source_height / height;
    }
    const uint16_t *a = (const uint16_t *)((const uint8_t *)pixels + (size_t)row * pitch);
    if (!weight) {
        memcpy(out, a, (size_t)width * sizeof *out);
        return;
    }
    const uint16_t *b = (const uint16_t *)((const uint8_t *)a + pitch);
    unsigned nearest=(uint64_t)y*source_height/height;
    const uint16_t *sharp=(const uint16_t *)((const uint8_t *)pixels+(size_t)nearest*pitch);
    for (unsigned x = 0; x < width; ++x) {
        bool text=false;
        if(filter && filter->text_rows) {
            unsigned column=filter->source_x+x;
            /* Protect both contributing rows and the nearest row, including
               the edge of a text region where filtering could leave a halo. */
            const unsigned rows[3]={row,row+1,nearest};
            for(unsigned i=0;i<3;i++) {
                PC_TextSpan span=filter->text_rows[rows[i]];
                text |= column>=span.left && column<span.right;
            }
        }
        out[x] = text ? sharp[x] : pc_rgb565_area_mix5(a[x], b[x], weight);
    }
}
