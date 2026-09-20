#include "audio_internal.h"
#include <stdatomic.h>
#include <string.h>
typedef struct {
  unsigned kind, slot;
  PC_Blob blob, bank;
  unsigned rate, channels, bits, sound_id;
  uint32_t fence;
} Command;
static Command queue[16];
static _Atomic unsigned head, tail, errors, active, sequencing;
static _Atomic uint32_t completed_fence;
static uint32_t next_fence;
static Midi midi;
static Wave waves[2];
static int32_t synth[PC_AUDIO_FRAMES];
void PC_MixerInit(void) {
  atomic_store(&head, 0);
  atomic_store(&tail, 0);
  atomic_store(&errors, 0);
  atomic_store(&active, 0);
  atomic_store(&sequencing, 0);
  atomic_store(&completed_fence, 0);
  next_fence = 0;
  memset(waves, 0, sizeof(waves));
  midi_init(&midi);
}
static bool send(Command c) {
  unsigned h = atomic_load_explicit(&head, memory_order_relaxed),
           next = (h + 1) & 15;
  if (next == atomic_load_explicit(&tail, memory_order_acquire))
    return false;
  queue[h] = c;
  atomic_store_explicit(&head, next, memory_order_release);
  return true;
}
bool PC_PlayWAV(unsigned slot, PC_Blob b) {
  return slot < 2 && send((Command){.kind = 2, .slot = slot, .blob = b});
}
bool PC_StopAudio(void) { return send((Command){.kind = 3}); }
bool PC_PlayPCM(unsigned slot, PC_Blob b, unsigned rate, unsigned channels,
                unsigned bits) {
  return slot < 2 && send((Command){.kind = 4,
                                    .slot = slot,
                                    .blob = b,
                                    .rate = rate,
                                    .channels = channels,
                                    .bits = bits});
}
bool PC_PlayPoPMIDI(PC_Blob b, PC_Blob bank, unsigned sound_id) {
  return send(
      (Command){.kind = 5, .blob = b, .bank = bank, .sound_id = sound_id});
}
bool PC_StopMIDI(void) { return send((Command){.kind = 6}); }
bool PC_StopWAV(unsigned slot) {
  return slot < 2 && send((Command){.kind = 7, .slot = slot});
}
bool PC_MusicSequencing(void) {
  return atomic_load_explicit(&sequencing, memory_order_relaxed) != 0;
}
bool PC_AudioFence(uint32_t *ticket) {
  if (!ticket)
    return false;
  uint32_t next = next_fence + 1;
  if (!next)
    next = 1;
  if (!send((Command){.kind = 8, .fence = next}))
    return false;
  next_fence = next;
  *ticket = next;
  return true;
}
bool PC_AudioFenceComplete(uint32_t ticket) {
  if (!ticket)
    return false;
  return (int32_t)(atomic_load_explicit(&completed_fence,
                                        memory_order_acquire) -
                   ticket) >= 0;
}
unsigned PC_MixerErrors(void) {
  return atomic_load_explicit(&errors, memory_order_relaxed);
}
unsigned PC_MixerActive(void) {
  return atomic_load_explicit(&active, memory_order_relaxed);
}
void PC_MixAudio(void *unused, int16_t *out, unsigned frames) {
  (void)unused;
  uint32_t fence = 0;
  unsigned t = atomic_load_explicit(&tail, memory_order_relaxed),
           h = atomic_load_explicit(&head, memory_order_acquire);
  while (t != h) {
    Command c = queue[t];
    bool ok = true;
    if (c.kind == 2)
      ok = wave_open(&waves[c.slot], c.blob);
    if (c.kind == 3) {
      midi.active = false;
      waves[0].active = waves[1].active = false;
    }
    if (c.kind == 4)
      ok = wave_open_pcm(&waves[c.slot], c.blob, c.rate, c.channels, c.bits);
    if (c.kind == 5)
      ok = midi_open_pop(&midi, c.blob, c.bank, c.sound_id);
    if (c.kind == 6)
      midi.active = false;
    if (c.kind == 7)
      waves[c.slot].active = false;
    if (c.kind == 8)
      fence = c.fence;
    if (!ok)
      atomic_fetch_add_explicit(&errors, 1, memory_order_relaxed);
    t = (t + 1) & 15;
    atomic_store_explicit(&tail, t, memory_order_release);
  }
  while (frames) {
    unsigned n = frames > PC_AUDIO_FRAMES ? PC_AUDIO_FRAMES : frames;
    bool bad = midi.bad;
    midi_render(&midi, synth, n);
    if (!bad && midi.bad)
      atomic_fetch_add_explicit(&errors, 1, memory_order_relaxed);
    for (unsigned i = 0; i < n; i++) {
      int32_t s = synth[i] + wave_sample(&waves[0]) + wave_sample(&waves[1]);
      if (s > 32767)
        s = 32767;
      if (s < -32768)
        s = -32768;
      *out++ = (int16_t)s;
    }
    frames -= n;
  }
  atomic_store_explicit(&sequencing, midi.active && !midi.ended,
                        memory_order_relaxed);
  atomic_store_explicit(&active,
                        (midi.active ? 1 : 0) | (waves[0].active ? 2 : 0) |
                            (waves[1].active ? 4 : 0),
                        memory_order_relaxed);
  if (fence)
    atomic_store_explicit(&completed_fence, fence, memory_order_release);
}
