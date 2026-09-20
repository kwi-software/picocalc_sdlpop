/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "pc_surface.h"
typedef struct {
    const char *name;        /* e.g. KID/res400.png or MIDISND1.DAT */
    PC_Blob blob;            /* original bytes for non-PNG entries */
    const PC_Surface *image; /* decoded INDEX8 flash sprite for PNG entries */
} POP_Asset;
const POP_Asset *POP_FindAsset(const POP_Asset *table, size_t count, const char *name);
