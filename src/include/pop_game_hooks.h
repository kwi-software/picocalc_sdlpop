/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "pop_port.h"
/* Field order matches SDLPoP rect_type, but this remains an independent C type.
   Convert members explicitly; do not type-pun an upstream struct pointer. */
typedef struct {
    int16_t top, left, bottom, right;
} POP_Rect;
typedef struct {
    PC_Surface surface;
    PC_Rect destination;
    bool valid;
} POP_Peel;
PC_Rect POP_ToRect(POP_Rect r);
bool POP_CopyRect(PC_Surface *target, const PC_Surface *source, POP_Rect target_rect,
                  POP_Rect source_rect, bool transparent);
bool POP_DrawImage(PC_Surface *target, const PC_Surface *image, int x, int y, unsigned blitter,
                   uint16_t mono_color, bool flip);
bool POP_CapturePeel(const PC_Surface *screen, POP_Rect rect, void *memory, size_t bytes,
                     POP_Peel *peel);
bool POP_RestorePeel(PC_Surface *screen, const POP_Peel *peel);
