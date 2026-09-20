/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pop_assets.h"
#include <string.h>
const POP_Asset *POP_FindAsset(const POP_Asset *table, size_t count, const char *name) {
    if (!table || !name)
        return NULL;
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = strcmp(name, table[mid].name);
        if (!order)
            return &table[mid];
        if (order < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return NULL;
}
