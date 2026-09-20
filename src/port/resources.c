/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pop_port.h"
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }
bool POP_FindDATResource(PC_Blob dat, uint16_t id, PC_Blob *out) {
    if (!out)
        return false;
    *out = (PC_Blob){0};
    if (!dat.data || dat.size < 8)
        return false;
    size_t off = le32(dat.data), len = le16(dat.data + 4);
    if (off < 6 || off > dat.size || len < 2 || len > dat.size - off)
        return false;
    const uint8_t *table = dat.data + off;
    unsigned count = le16(table);
    if (count > (len - 2) / 8)
        return false;
    for (unsigned i = 0; i < count; i++) {
        const uint8_t *r = table + 2 + i * 8;
        size_t pos = le32(r + 2), size = le16(r + 6);
        if (pos < 6 || pos >= dat.size || size > dat.size - pos - 1)
            return false;
        if (le16(r) == id) {
            *out = (PC_Blob){dat.data + pos + 1, size};
            return true;
        } /* skip checksum byte like upstream */
    }
    return false;
}

bool POP_PlaySoundResource(PC_Blob b, PC_Blob bank, unsigned slot, unsigned sound_id,
                           unsigned version) {
    if (!b.data || b.size < 2 || slot >= 2)
        return false;
    unsigned type = b.data[0] & 7;
    if (type == 2)
        return PC_PlayPoPMIDI((PC_Blob){b.data + 1, b.size - 1}, bank, sound_id);
    if (type != 1)
        return false; /* speaker, OGG and converted desktop pointers are not PCM blobs */
    const uint8_t *d = b.data + 1;
    size_t size = b.size - 1;
    /* Validate both layouts before auto-detection: an 8 in sample_count's
       low byte must not be mistaken for the newer sample_size field. */
    bool old = size >= 7 && d[6] == 8 && le16(d + 2) > 0 && le16(d + 2) <= size - 7;
    bool newer = size >= 9 && d[2] == 8 && le16(d + 3) > 0 && le16(d + 3) <= size - 9;
    if (!version) {
        if (old && newer) {
            /* Prefer the layout that consumes the complete resource. */
            old = le16(d + 2) == size - 7;
            newer = le16(d + 3) == size - 9;
        }
        if (old == newer)
            return false;
        version = old ? 1 : 2;
    }
    if ((version == 1 && !old) || (version == 2 && !newer) || (version != 1 && version != 2))
        return false;
    unsigned hdr = version == 1 ? 7 : 9, count = le16(d + (version == 1 ? 2 : 3)), rate = le16(d);
    if (!count || count > size - hdr || rate < 1000)
        return false;
    return PC_PlayPCM(slot, (PC_Blob){d + hdr, count}, rate, 1, 8);
}
