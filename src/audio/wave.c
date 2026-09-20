#include "audio_internal.h"
#include <string.h>
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }
bool wave_open(Wave *w, PC_Blob b) {
    *w = (Wave){0};
    if (!b.data || b.size < 12 || memcmp(b.data, "RIFF", 4) || memcmp(b.data + 8, "WAVE", 4))
        return false;
    uint32_t length = le32(b.data + 4);
    if (length < 4 || length > b.size - 8)
        return false;
    size_t end = 8 + (size_t)length, pos = 12;
    uint32_t rate = 0, bytes = 0;
    uint16_t align = 0;
    bool fmt = false, data = false;
    while (pos + 8 <= end) {
        const uint8_t *c = b.data + pos;
        uint32_t n = le32(c + 4);
        pos += 8;
        if (n > end - pos)
            return false;
        if (!memcmp(c, "fmt ", 4)) {
            if (fmt || n < 16 || le16(c + 8) != 1)
                return false;
            unsigned ch = le16(c + 10), bits = le16(c + 22);
            rate = le32(c + 12);
            align = le16(c + 20);
            if ((ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate < 1000 || rate > 192000 ||
                align != ch * (bits / 8))
                return false;
            w->channels = ch;
            w->bits = bits;
            fmt = true;
        } else if (!memcmp(c, "data", 4) && !data) {
            w->pcm = c + 8;
            bytes = n;
            data = true;
        }
        pos += n;
        if (n & 1) {
            if (pos == end)
                return false;
            pos++;
        }
    }
    if (pos != end || !fmt || !data || !bytes || bytes % align)
        return false;
    w->count = bytes / align;
    w->step = (uint32_t)(((uint64_t)rate << 16) / PC_AUDIO_RATE);
    w->active = true;
    return true;
}
static int32_t sample(const Wave *w, uint32_t frame) {
    const uint8_t *p = w->pcm + (size_t)frame * w->channels * (w->bits / 8);
    int32_t a, b;
    if (w->bits == 8) {
        a = ((int)p[0] - 128) * 256;
        b = w->channels == 2 ? ((int)p[1] - 128) * 256 : a;
    } else {
        a = (int16_t)le16(p);
        b = w->channels == 2 ? (int16_t)le16(p + 2) : a;
    }
    return (a + b) / 2;
}
int32_t wave_sample(Wave *w) {
    if (!w->active)
        return 0;
    int32_t a = sample(w, w->index),
            b = sample(w, w->index + 1 < w->count ? w->index + 1 : w->index);
    int32_t out = a + ((b - a) * (int32_t)(w->frac >> 8)) / 256;
    uint32_t f = w->frac + w->step;
    w->frac = f & 65535;
    uint32_t advance = f >> 16;
    if (advance >= w->count - w->index)
        w->active = false;
    else
        w->index += advance;
    return out;
}

bool wave_open_pcm(Wave *w, PC_Blob b, unsigned rate, unsigned channels, unsigned bits) {
    *w = (Wave){0};
    if (!b.data || !b.size || (channels != 1 && channels != 2) || (bits != 8 && bits != 16) ||
        rate < 1000 || rate > 192000)
        return false;
    unsigned frame = channels * (bits / 8);
    if (b.size % frame || b.size / frame > UINT32_MAX)
        return false;
    w->pcm = b.data;
    w->count = (uint32_t)(b.size / frame);
    w->channels = channels;
    w->bits = bits;
    w->step = (uint32_t)(((uint64_t)rate << 16) / PC_AUDIO_RATE);
    w->active = true;
    return true;
}
