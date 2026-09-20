#pragma once
#include "pc_media.h"
#include "dbopl.h"
#define MIDI_TRACKS 16
/* No dynamic allocation, no hardware headers in the engine. */
typedef struct {
    const uint8_t *pcm;
    uint32_t count, index, frac, step;
    uint8_t channels, bits;
    bool active;
} Wave;
bool wave_open(Wave *w, PC_Blob b);
bool wave_open_pcm(Wave *w, PC_Blob b, unsigned rate, unsigned channels, unsigned bits);
int32_t wave_sample(Wave *w);
typedef struct {
    const uint8_t *p, *end;
    uint64_t tick;
    uint8_t running;
    bool live;
} Track;
typedef struct {
    int channel, note, transpose;
    uint8_t velocity, breg, patch;
    bool down;
} Voice;
typedef struct {
    Chip chip;
    Track tracks[MIDI_TRACKS];
    Voice voices[9];
    uint8_t program[16], volume[16], expression[16], sustain[16];
    uint16_t bend[16];
    uint16_t division;
    unsigned ntracks;
    uint32_t tempo, tail;
    uint64_t tick, wait, remainder;
    bool active, bad, ended;
    PC_Blob bank;
    unsigned tempo_factor;
    int transpose, last_voice;
} Midi;
void midi_init(Midi *m);
bool midi_open_pop(Midi *m, PC_Blob b, PC_Blob bank, unsigned sound_id);
void midi_render(Midi *m, int32_t *out, unsigned n);
