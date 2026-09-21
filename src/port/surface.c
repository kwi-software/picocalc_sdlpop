/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_surface.h"
#include <string.h>
#include <stdint.h>
uint16_t PC_MapRGB(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}
static PC_Rect intersection(PC_Rect a, PC_Rect b) {
    int64_t x = a.x > b.x ? a.x : b.x, y = a.y > b.y ? a.y : b.y;
    int64_t r = (int64_t)a.x + a.w, br = (int64_t)b.x + b.w, bot = (int64_t)a.y + a.h,
            bb = (int64_t)b.y + b.h;
    if (br < r)
        r = br;
    if (bb < bot)
        bot = bb;
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0 || r <= x || bot <= y)
        return (PC_Rect){0};
    return (PC_Rect){(int)x, (int)y, (int)(r - x), (int)(bot - y)};
}
/* Two 200-row interval tables: 1600 bytes, no heap allocations. */
static struct { const void *pixels; PC_TextSpan rows[200]; } text_frames[2];
static PC_TextSpan *text_rows(const PC_Surface *s) {
    if(s) for(unsigned i=0;i<2;i++)
        if(s->pixels && s->pixels==text_frames[i].pixels)return text_frames[i].rows;
    return NULL;
}
void PC_TrackTextSurface(unsigned slot,const PC_Surface *s) {
    if(slot>=2)return;
    text_frames[slot].pixels=s?s->pixels:NULL;
    memset(text_frames[slot].rows,0,sizeof text_frames[slot].rows);
}
const PC_TextSpan *PC_SurfaceTextRows(const PC_Surface *s) {return text_rows(s);}
static void add_span(PC_TextSpan *s,int left,int right) {
    if(right<=left)return;
    if(s->right<=s->left)*s=(PC_TextSpan){left,right};
    else {if(left<s->left)s->left=left;if(right>s->right)s->right=right;}
}
static void clear_span(PC_TextSpan *s,int left,int right) {
    if(left<=s->left && right>=s->right)*s=(PC_TextSpan){0};
    else if(left<=s->left && right>s->left)s->left=right;
    else if(left<s->right && right>=s->right)s->right=left;
    /* An interior cut cannot be represented by one interval: keep the union.
       This conservatively keeps the gap sharp until the next larger redraw. */
}
void PC_MarkTextRect(const PC_Surface *s,PC_Rect r) {
    PC_TextSpan *rows=text_rows(s);if(!rows)return;
    r=intersection(r,s->clip);r=intersection(r,(PC_Rect){0,0,s->w,s->h<200?s->h:200});
    for(int y=r.y;y<r.y+r.h;y++)add_span(&rows[y],r.x,r.x+r.w);
}
void PC_FlipTextRows(const PC_Surface *s) {
    PC_TextSpan *rows=text_rows(s);if(!rows || s->h>200)return;
    for(int y=0;y<s->h/2;y++){PC_TextSpan t=rows[y];rows[y]=rows[s->h-1-y];rows[s->h-1-y]=t;}
}
static void copy_text_rows(const PC_Surface *s,PC_Surface *d,PC_Rect a,PC_Rect out,
                           int sx,int sy,bool flip,bool opaque) {
    PC_TextSpan *dst=text_rows(d),*src=text_rows(s);if(!dst)return;
    int first=0,last=out.h,step=1;
    if(src==dst && out.y>sy){first=out.h-1;last=-1;step=-1;}
    for(int j=first;j!=last;j+=step) {
        PC_TextSpan mark={0};
        if(src && sy+j>=0 && sy+j<200)mark=src[sy+j];
        int left=mark.left,right=mark.right;
        if(flip){left=a.x+a.w-mark.right;right=a.x+a.w-mark.left;}
        else {left-=sx;right-=sx;}
        if(flip){left-=sx;right-=sx;}
        if(left<0)left=0;
        if(right>out.w)right=out.w;
        int y=out.y+j;if(y<0 || y>=200)continue;
        if(opaque)clear_span(&dst[y],out.x,out.x+out.w);
        if(mark.right>mark.left && right>left)add_span(&dst[y],out.x+left,out.x+right);
    }
}
bool PC_InitSurface(PC_Surface *s, int w, int h, unsigned pitch, PC_PixelFormat fmt,
                    const void *pixels, size_t bytes, bool writable) {
    if (!s || !pixels || w <= 0 || h <= 0 || w > 32767 || h > 32767 ||
        (fmt != PC_RGB565 && fmt != PC_INDEX8))
        return false;
    if ((unsigned)w * (unsigned)fmt > pitch || pitch > SIZE_MAX / (unsigned)h ||
        (size_t)pitch * h > bytes)
        return false;
    if (fmt == PC_RGB565 && (((uintptr_t)pixels & 1) || (pitch & 1)))
        return false;
    *s = (PC_Surface){w, h, pitch, fmt, pixels, bytes, NULL, 0, -1, writable, {0, 0, w, h}};
    return true;
}
bool PC_SetSurfacePalette(PC_Surface *s, const uint16_t *pal, unsigned n) {
    if (!s || s->format != PC_INDEX8 || !pal || !n || n > 256)
        return false;
    s->palette = pal;
    s->palette_count = n;
    return true;
}
bool PC_SetColorKey(PC_Surface *s, int key) {
    if (!s || key < -1 || key > (s->format == PC_INDEX8 ? 255 : 65535))
        return false;
    s->color_key = key;
    return true;
}
bool PC_SetClipRect(PC_Surface *s, const PC_Rect *r) {
    if (!s)
        return false;
    s->clip = intersection((PC_Rect){0, 0, s->w, s->h}, r ? *r : (PC_Rect){0, 0, s->w, s->h});
    return s->clip.w > 0;
}
bool PC_FillRect(PC_Surface *s, const PC_Rect *r, uint16_t value) {
    if (!s || !s->writable || (s->format == PC_INDEX8 && value > 255))
        return false;
    PC_Rect a = intersection(r ? *r : (PC_Rect){0, 0, s->w, s->h}, s->clip);
    PC_TextSpan *marks=text_rows(s);
    for (int y = a.y; y < a.y + a.h; y++) {
        if(marks && y<200)clear_span(&marks[y],a.x,a.x+a.w);
        uint8_t *row = (uint8_t *)(uintptr_t)s->pixels + (size_t)y * s->pitch;
        if (s->format == PC_INDEX8)
            memset(row + a.x, (uint8_t)value, a.w);
        else {
            uint16_t *d = (uint16_t *)row + a.x;
            for (int x = 0; x < a.w; x++)
                d[x] = value;
        }
    }
    return true;
}
bool PC_BlitSurface(const PC_Surface *s, const PC_Rect *source, PC_Surface *d, PC_Rect *dest,
                    PC_BlitOp op, bool flip, uint16_t mono) {
    if (!s || !d || !d->writable || d->format != PC_RGB565 || op > PC_BLIT_MONO)
        return false;
    if (s->format == PC_INDEX8 && (!s->palette || !s->palette_count))
        return false;
    PC_Rect a = source ? *source : (PC_Rect){0, 0, s->w, s->h};
    PC_Rect b = {dest ? dest->x : 0, dest ? dest->y : 0, a.w, a.h};
    if (a.w <= 0 || a.h <= 0) {
        if (dest)
            dest->w = dest->h = 0;
        return true;
    }
    /* Clipping in offset space preserves the destination origin when source is clipped. */
    int64_t lo_x = 0, lo_y = 0, hi_x = a.w, hi_y = a.h;
#define LOWER(v, expr)                                                                             \
    do {                                                                                           \
        int64_t z = (expr);                                                                        \
        if (z > (v))                                                                               \
            (v) = z;                                                                               \
    } while (0)
#define UPPER(v, expr)                                                                             \
    do {                                                                                           \
        int64_t z = (expr);                                                                        \
        if (z < (v))                                                                               \
            (v) = z;                                                                               \
    } while (0)
    LOWER(lo_x, (int64_t)d->clip.x - b.x);
    UPPER(hi_x, (int64_t)d->clip.x + d->clip.w - b.x);
    LOWER(lo_y, (int64_t)d->clip.y - b.y);
    UPPER(hi_y, (int64_t)d->clip.y + d->clip.h - b.y);
    LOWER(lo_y, -(int64_t)a.y);
    UPPER(hi_y, (int64_t)s->h - a.y);
    if (!flip) {
        LOWER(lo_x, -(int64_t)a.x);
        UPPER(hi_x, (int64_t)s->w - a.x);
    } else {
        LOWER(lo_x, (int64_t)a.x + a.w - s->w);
        UPPER(hi_x, (int64_t)a.x + a.w);
    }
#undef LOWER
#undef UPPER
    if (hi_x <= lo_x || hi_y <= lo_y) {
        if (dest)
            dest->w = dest->h = 0;
        return true;
    }
    PC_Rect out = {(int)(b.x + lo_x), (int)(b.y + lo_y), (int)(hi_x - lo_x), (int)(hi_y - lo_y)};
    uintptr_t sp = (uintptr_t)s->pixels, dp = (uintptr_t)d->pixels;
    bool overlap = (sp <= dp ? dp - sp < s->bytes : sp - dp < d->bytes);
    if (overlap) {
        if (sp != dp || s->pitch != d->pitch || s->format != PC_RGB565 || op != PC_BLIT_COPY ||
            flip || s->color_key >= 0)
            return false;
        int sy = (int)(a.y + lo_y), sx = (int)(a.x + lo_x);
        int start = 0, end = out.h, step = 1;
        if (out.y > sy) {
            start = out.h - 1;
            end = -1;
            step = -1;
        }
        for (int j = start; j != end; j += step)
            memmove((uint8_t *)dp + (size_t)(out.y + j) * d->pitch + out.x * 2,
                    (const uint8_t *)sp + (size_t)(sy + j) * s->pitch + sx * 2, (size_t)out.w * 2);
    } else {
        for (int y = 0; y < out.h; y++) {
            const uint8_t *row = (const uint8_t *)s->pixels + (size_t)(a.y + lo_y + y) * s->pitch;
            uint16_t *dst = (uint16_t *)((uint8_t *)dp + (size_t)(out.y + y) * d->pitch) + out.x;
            if (s->format == PC_RGB565 && s->color_key < 0 && op == PC_BLIT_COPY && !flip) {
                memcpy(dst, row + (a.x + lo_x) * 2, (size_t)out.w * 2);
                continue;
            }
            for (int x = 0; x < out.w; x++) {
                int sx = (int)(flip ? (int64_t)a.x + a.w - 1 - lo_x - x : (int64_t)a.x + lo_x + x);
                unsigned raw = s->format == PC_INDEX8 ? row[sx] : ((const uint16_t *)row)[sx];
                if ((int)raw == s->color_key)
                    continue;
                if (s->format == PC_INDEX8 && raw >= s->palette_count)
                    return false;
                uint16_t v = s->format == PC_INDEX8 ? s->palette[raw] : (uint16_t)raw;
                switch (op) {
                case PC_BLIT_COPY:
                    dst[x] = v;
                    break;
                case PC_BLIT_XOR:
                    dst[x] ^= v;
                    break;
                case PC_BLIT_OR:
                    dst[x] |= v;
                    break;
                case PC_BLIT_MONO:
                    dst[x] = mono;
                    break;
                }
            }
        }
    }
    copy_text_rows(s,d,a,out,flip?(int)lo_x:(int)(a.x+lo_x),(int)(a.y+lo_y),
                   flip,s->color_key<0 && op==PC_BLIT_COPY);
    if (dest)
        *dest = out;
    return true;
}
bool PC_PresentSurface(const PC_Surface *s, const PC_Rect *dirty, int lcd_y) {
    if (!s || s->format != PC_RGB565 || s->w != 320 || s->h != 200 || lcd_y < 0 || lcd_y > 120)
        return false;
    PC_Rect r = intersection(dirty ? *dirty : (PC_Rect){0, 0, 320, 200}, (PC_Rect){0, 0, 320, 200});
    if (!r.w)
        return true;
    const uint16_t *p =
        (const uint16_t *)((const uint8_t *)s->pixels + (size_t)r.y * s->pitch) + r.x;
    r.y += lcd_y;
    PC_UpdateTexture(&r, p, s->pitch);
    PC_RenderPresent();
    return true;
}
