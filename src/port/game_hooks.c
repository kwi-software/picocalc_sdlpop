/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pop_game_hooks.h"
PC_Rect POP_ToRect(POP_Rect r) {
    return (PC_Rect){r.left, r.top, r.right - r.left, r.bottom - r.top};
}
bool POP_CopyRect(PC_Surface *target, const PC_Surface *source, POP_Rect tr, POP_Rect sr,
                  bool transparent) {
    if (!source)
        return false;
    PC_Surface view = *source;
    view.color_key = transparent ? 0 : -1;
    PC_Rect a = POP_ToRect(sr), b = POP_ToRect(tr);
    return PC_BlitSurface(&view, &a, target, &b, PC_BLIT_COPY, false, 0);
}
bool POP_DrawImage(PC_Surface *target, const PC_Surface *image, int x, int y, unsigned blitter,
                   uint16_t mono, bool flip) {
    if (!image)
        return false;
    PC_Surface view = *image;
    PC_BlitOp op = PC_BLIT_COPY;
    switch (blitter) {
    case 0:
        view.color_key = -1;
        break;
    case 2:
    case 0x10:
        view.color_key = 0;
        break; /* matches current SDLPoP SDL backend */
    case 3:
        op = PC_BLIT_XOR;
        view.color_key = -1;
        break;
    case 8:
    case 9:
    case 0x40:
    case 0x46:
    case 0x4c:
        op = PC_BLIT_MONO;
        view.color_key = 0;
        if (blitter == 8)
            mono = 0xffff;
        if (blitter == 9)
            mono = 0;
        break;
    default:
        return false; /* Colored flame/alpha effects require a separate implementation. */
    }
    PC_Rect d = {x, y, 0, 0};
    return PC_BlitSurface(&view, NULL, target, &d, op, flip, mono);
}
bool POP_CapturePeel(const PC_Surface *screen, POP_Rect rect, void *memory, size_t bytes,
                     POP_Peel *peel) {
    if (!peel)
        return false;
    *peel = (POP_Peel){0};
    if (!screen || screen->format != PC_RGB565)
        return false;
    PC_Rect r = POP_ToRect(rect);
    int right = r.x + r.w, bottom = r.y + r.h;
    if (r.x < 0)
        r.x = 0;
    if (r.y < 0)
        r.y = 0;
    if (right > screen->w)
        right = screen->w;
    if (bottom > screen->h)
        bottom = screen->h;
    r.w = right - r.x;
    r.h = bottom - r.y;
    if (r.w <= 0 || r.h <= 0)
        return false;
    if (!PC_InitSurface(&peel->surface, r.w, r.h, (unsigned)r.w * 2, PC_RGB565, memory, bytes,
                        true))
        return false;
    PC_Surface source = *screen;
    source.color_key = -1;
    if (!PC_BlitSurface(&source, &r, &peel->surface, NULL, PC_BLIT_COPY, false, 0))
        return false;
    peel->destination = r;
    peel->valid = true;
    return true;
}
bool POP_RestorePeel(PC_Surface *screen, const POP_Peel *peel) {
    if (!peel || !peel->valid)
        return false;
    PC_Rect r = peel->destination;
    return PC_BlitSurface(&peel->surface, NULL, screen, &r, PC_BLIT_COPY, false, 0);
}
