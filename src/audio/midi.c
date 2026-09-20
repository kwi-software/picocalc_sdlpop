#include "audio_internal.h"
#include <string.h>
#include <limits.h>
static const uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
#include "note_table.h"
static void reg(Midi *m, unsigned r, unsigned v) { Chip__WriteReg(&m->chip, r, (uint8_t)v); }
static void off(Midi *m, int v) {
    reg(m, 0xb0 + v, m->voices[v].breg & ~32);
    m->voices[v].channel = -1;
}
static void volume(Midi *m, int v) {
    Voice *a = &m->voices[v];
    if (a->channel < 0)
        return;
    unsigned gain =
        (unsigned)a->velocity * m->volume[a->channel] * m->expression[a->channel] / (127 * 127);
    unsigned base = m->bank.data[1 + (unsigned)a->patch * 16 + 9];
    unsigned level = ((base & 63) + 64) * 225 / (gain + 161);
    if (level < 64) level = 64;
    if (level > 127) level = 127;
    reg(m, 0x40 + offsets[v] + 3, (base & 0xc0) | (level - 64));
}
static void pitch(Midi *m, int v) {
    Voice *a = &m->voices[v];
    int c = a->channel;
    if (c < 0)
        return;
    /* Integer interpolation over precomputed Hz*1024, fixed +/-2-semitone bend. */
    int q = (a->note + a->transpose) * 4096 + ((int)m->bend[c] - 8192);
    if (q < 0)
        q = 0;
    if (q > 127 * 4096)
        q = 127 * 4096;
    unsigned n = (unsigned)q >> 12, f = (unsigned)q & 4095;
    uint32_t hz = note_hz[n];
    if (n < 127)
        hz += (uint32_t)(((uint64_t)(note_hz[n + 1] - hz) * f) >> 12);
    unsigned block = 0;
    uint32_t fn = (uint32_t)(((uint64_t)hz * 1024 + 24858) / 49716);
    while (fn > 1023 && block < 7) {
        fn = (fn + 1) / 2;
        block++;
    }
    if (fn > 1023)
        fn = 1023;
    reg(m, 0xa0 + v, fn & 255);
    a->breg = 32 | (block << 2) | (fn >> 8);
    reg(m, 0xb0 + v, a->breg);
}
static void on(Midi *m, unsigned c, unsigned note, unsigned vel) {
    int v = -1;
    for (int i = 1; i <= 9; i++) {
        int candidate = (m->last_voice + i) % 9;
        if (m->voices[candidate].channel < 0) { v = candidate; break; }
    }
    if (v < 0) return; /* SDLPoP skips when all nine voices are occupied. */
    m->last_voice = v;
    off(m, v);
    unsigned p = m->program[c];
    if (p >= m->bank.data[0]) p = 0;
    Voice *a = &m->voices[v];
    *a = (Voice){.channel = (int)c,
                 .note = (int)note,
                 .transpose = m->transpose,
                 .velocity = vel,
                 .patch = p,
                 .down = true};
    unsigned o = offsets[v];
    const unsigned base[5] = {0x20, 0x40, 0x60, 0x80, 0xe0};
    const uint8_t *t = m->bank.data + 1 + p * 16;
    for (unsigned k = 0; k < 5; k++) {
        reg(m, base[k] + o, t[3 + k]);
        reg(m, base[k] + o + 3, t[8 + k]);
    }
    reg(m, 0xc0 + v, t[2]);
    volume(m, v);
    pitch(m, v);
}
static void noteoff(Midi *m, unsigned c, unsigned n) {
    for (int i = 0; i < 9; i++) {
        Voice *a = &m->voices[i];
        if (a->channel == (int)c && a->note == (int)n) {
            a->down = false;
            if (!m->sustain[c])
                off(m, i);
        }
    }
}
static void message(Midi *m, uint8_t s, uint8_t a, uint8_t b) {
    unsigned c = s & 15;
    switch (s >> 4) {
    case 8:
        noteoff(m, c, a);
        break;
    case 9:
        if (b)
            on(m, c, a, b);
        else
            noteoff(m, c, a);
        break;
    case 12:
        m->program[c] = a;
        break;
    case 14:
        m->bend[c] = a | (b << 7);
        for (int i = 0; i < 9; i++)
            if (m->voices[i].channel == (int)c)
                pitch(m, i);
        break;
    case 11:
        if (a == 7)
            m->volume[c] = b;
        if (a == 11)
            m->expression[c] = b;
        if (a == 64)
            m->sustain[c] = (b >= 64);
        if (a == 121) {
            m->volume[c] = 100;
            m->expression[c] = 127;
            m->bend[c] = 8192;
            m->sustain[c] = 0;
        }
        for (int i = 0; i < 9; i++)
            if (m->voices[i].channel == (int)c) {
                if (a == 120) {
                    unsigned o = offsets[i];
                    reg(m, 0x40 + o, 63);
                    reg(m, 0x40 + o + 3, 63);
                    off(m, i);
                    continue;
                }
                if (a == 123)
                    noteoff(m, c, m->voices[i].note);
                if ((a == 64 || a == 121) && !m->sustain[c] && !m->voices[i].down)
                    off(m, i);
                if (a == 7 || a == 11 || a == 121)
                    volume(m, i);
                if (a == 121)
                    pitch(m, i);
            }
        break;
    default:
        break; /* poly/channel pressure ignored */
    }
}
static uint16_t be16(const uint8_t *p) { return (uint16_t)p[0] << 8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)be16(p) << 16 | be16(p + 2); }
static bool vlq(Track *t, uint32_t *out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (t->p == t->end)
            return false;
        uint8_t b = *t->p++;
        v = (v << 7) | (b & 127);
        if (!(b & 128)) {
            *out = v;
            return true;
        }
    }
    return false;
}
static void defaults(Midi *m) {
    reg(m, 1, 32);
    for (int i = 0; i < 9; i++)
        m->voices[i].channel = -1;
    for (int c = 0; c < 16; c++) {
        m->volume[c] = 100;
        m->expression[c] = 127;
        m->bend[c] = 8192;
    }
    m->tempo = 500000;
}
void midi_init(Midi *m) {
    memset(m, 0, sizeof(*m));
    DBOPL_InitTables();
    Chip__Chip(&m->chip);
    Chip__Setup(&m->chip, PC_AUDIO_RATE);
    defaults(m);
}
static void restart(Midi *m) {
    Chip__Reset(&m->chip);
    memset((uint8_t *)m + offsetof(Midi, tracks), 0, sizeof(*m) - offsetof(Midi, tracks));
    defaults(m);
}
static bool event(Midi *m, Track *t) {
    if (t->p == t->end)
        return false;
    uint8_t s = *t->p;
    if (s & 128) {
        t->p++;
        if (s < 0xf0)
            t->running = s;
    } else {
        s = t->running;
        if (!s)
            return false;
    }
    if (s == 0xff) {
        t->running = 0;
        if (t->p == t->end)
            return false;
        uint8_t type = *t->p++;
        uint32_t len;
        if (!vlq(t, &len) || len > (size_t)(t->end - t->p))
            return false;
        if (type == 0x2f) {
            if (len)
                return false;
            t->live = false;
            return true;
        }
        if (type == 0x51) {
            if (len != 3)
                return false;
            uint32_t tempo = (uint32_t)t->p[0] << 16 | (uint32_t)t->p[1] << 8 | t->p[2];
            if (!tempo)
                return false;
            m->tempo = (uint32_t)((uint64_t)tempo * m->tempo_factor / 1000);
            if (!m->tempo)
                m->tempo = 1;
        }
        t->p += len;
    } else if (s == 0xf0 || s == 0xf7) {
        t->running = 0;
        uint32_t len;
        if (!vlq(t, &len) || len > (size_t)(t->end - t->p))
            return false;
        if (m->bank.data && s == 0xf0 && len == 7 && t->p[2] == 0x34 &&
            (t->p[3] == 0 || t->p[3] == 1) && t->p[4] == 0)
            m->transpose = -12 + (int8_t)t->p[5];
        t->p += len;
    } else if (s >= 0x80 && s <= 0xef) {
        unsigned len = ((s >> 4) == 12 || (s >> 4) == 13) ? 1 : 2;
        if ((size_t)(t->end - t->p) < len || t->p[0] > 127 || (len == 2 && t->p[1] > 127))
            return false;
        uint8_t a = *t->p++, b = len == 2 ? *t->p++ : 0;
        message(m, s, a, b);
    } else
        return false;
    uint32_t dt;
    if (!vlq(t, &dt) || UINT64_MAX - t->tick < dt)
        return false;
    t->tick += dt;
    return true;
}
/* Apply all simultaneous events, then schedule next event in sample time.
   Remainder survives tempo changes; bounded event batch prevents pathological MIDI
   monopolising an audio block. A rejected file is silenced, never read out of bounds. */
static bool schedule(Midi *m) {
    unsigned budget = 4096;
    while (m->active) {
        uint64_t next = UINT64_MAX;
        for (unsigned i = 0; i < m->ntracks; i++)
            if (m->tracks[i].live && m->tracks[i].tick < next)
                next = m->tracks[i].tick;
        if (next == UINT64_MAX) {
            for (int i = 0; i < 9; i++)
                off(m, i);
            m->ended = true;
            m->tail = PC_AUDIO_RATE;
            return true;
        }
        if (next > m->tick) {
            uint64_t dt = next - m->tick, scale = (uint64_t)m->tempo * PC_AUDIO_RATE;
            if (dt > (UINT64_MAX - m->remainder) / scale)
                return false;
            uint64_t numerator = dt * scale + m->remainder, den = (uint64_t)m->division * 1000000;
            m->wait = numerator / den;
            m->remainder = numerator % den;
            m->tick = next;
            if (m->wait)
                return true;
        }
        for (unsigned i = 0; i < m->ntracks; i++) {
            Track *t = &m->tracks[i];
            while (t->live && t->tick == m->tick) {
                if (!budget-- || !event(m, t))
                    return false;
            }
        }
    }
    return true;
}
bool midi_open_pop(Midi *m, PC_Blob b, PC_Blob bank, unsigned sound_id) {
    restart(m);
    m->tempo_factor = 1000;
    {
        if (!bank.data || bank.size < 17 || !bank.data[0] || bank.size != 1 + (size_t)bank.data[0] * 16)
            return false;
        m->bank = bank;
        m->transpose = -12;
        for (int c = 0; c < 16; c++) {
            m->program[c] = c;
            m->volume[c] = 127;
        }
        if (sound_id == 53)
            m->tempo_factor = 970;
        if (sound_id == 54)
            m->tempo_factor = 1030;
    }
    if (!b.data || b.size < 14 || memcmp(b.data, "MThd", 4))
        return false;
    uint32_t h = be32(b.data + 4);
    if (h < 6 || h > b.size - 8)
        return false;
    unsigned format = be16(b.data + 8), tracks = be16(b.data + 10);
    m->division = be16(b.data + 12);
    if (format > 1 || !tracks || tracks > MIDI_TRACKS || (format == 0 && tracks != 1) ||
        !m->division || (m->division & 0x8000))
        return false;
    size_t pos = 8 + (size_t)h;
    while (m->ntracks < tracks) {
        if (pos > b.size || b.size - pos < 8)
            return false;
        const uint8_t *c = b.data + pos;
        uint32_t n = be32(c + 4);
        pos += 8;
        if (n > b.size - pos)
            return false;
        if (!memcmp(c, "MTrk", 4)) {
            Track *t = &m->tracks[m->ntracks++];
            *t = (Track){.p = b.data + pos, .end = b.data + pos + n, .live = true};
            uint32_t dt;
            if (!vlq(t, &dt))
                return false;
            t->tick = dt;
        }
        pos += n;
    }
    m->active = true;
    if (!schedule(m)) {
        m->active = false;
        m->bad = true;
        return false;
    }
    return true;
}
void midi_render(Midi *m, int32_t *out, unsigned n) {
    memset(out, 0, n * sizeof(*out));
    unsigned done = 0;
    while (m->active && done < n) {
        if (m->ended) {
            unsigned k = n - done;
            if (k > m->tail)
                k = m->tail;
            Chip__GenerateBlock2(&m->chip, k, out + done);
            done += k;
            m->tail -= k;
            if (!m->tail)
                m->active = false;
            continue;
        }
        if (!m->wait) {
            if (!schedule(m)) {
                m->bad = true;
                m->active = false;
                break;
            }
            continue;
        }
        unsigned k = n - done;
        if (m->wait < k)
            k = (unsigned)m->wait;
        Chip__GenerateBlock2(&m->chip, k, out + done);
        done += k;
        m->wait -= k;
    }
}
