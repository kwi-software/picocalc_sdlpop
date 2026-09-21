/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "pc_media.h"
/* Surfaces wrap caller-owned memory. Indexed ROM sprites stay immutable in flash.
   No implicit allocation, no full-size color conversion or SDL dependency. */
typedef enum { PC_INDEX8 = 1, PC_RGB565 = 2 } PC_PixelFormat;
typedef struct {
    int w, h;
    unsigned pitch;
    PC_PixelFormat format;
    const void *pixels;
    size_t bytes;
    const uint16_t *palette;
    unsigned palette_count;
    int color_key;
    bool writable;
    PC_Rect clip;
} PC_Surface;
typedef enum { PC_BLIT_COPY, PC_BLIT_XOR, PC_BLIT_OR, PC_BLIT_MONO } PC_BlitOp;
bool PC_InitSurface(PC_Surface *s, int w, int h, unsigned pitch, PC_PixelFormat format,
                    const void *pixels, size_t bytes, bool writable);
bool PC_SetSurfacePalette(PC_Surface *s, const uint16_t *palette, unsigned count);
bool PC_SetColorKey(PC_Surface *s, int key); /* -1 disables key; INDEX8 value or RGB565 */
bool PC_SetClipRect(PC_Surface *s, const PC_Rect *clip);
bool PC_FillRect(PC_Surface *s, const PC_Rect *rect, uint16_t value);
/* Destination must be RGB565. dst->w/h are written like SDL_BlitSurface.
   Same-surface overlapping COPY is supported (memmove semantics); other overlap
   is rejected. flip_x operates inside the requested source rect before clipping.
   MONO replaces every non-key source pixel with mono_color. */
bool PC_BlitSurface(const PC_Surface *src, const PC_Rect *source, PC_Surface *dst,
                    PC_Rect *destination, PC_BlitOp op, bool flip_x, uint16_t mono_color);
bool PC_PresentSurface(const PC_Surface *screen, const PC_Rect *dirty, int lcd_y);
uint16_t PC_MapRGB(uint8_t r, uint8_t g, uint8_t b);

/* Track the two game framebuffers without changing ROM surface metadata. */
void PC_TrackTextSurface(unsigned slot, const PC_Surface *surface);
const PC_TextSpan *PC_SurfaceTextRows(const PC_Surface *surface);
void PC_MarkTextRect(const PC_Surface *surface, PC_Rect rect);
void PC_FlipTextRows(const PC_Surface *surface);
