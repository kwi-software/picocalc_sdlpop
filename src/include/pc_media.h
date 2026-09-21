#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define PC_AUDIO_RATE 24000u
#define PC_AUDIO_FRAMES 128u
#define PC_KEY_F1 0x81
#define PC_KEY_F2 0x82
#define PC_KEY_F3 0x83
typedef struct {
    int x, y, w, h;
} PC_Rect;
enum { PC_KEYDOWN = 1, PC_KEYUP = 2, PC_INPUT_RESET = 3 };
typedef struct {
    uint8_t key, type, repeat;
    uint16_t scancode, modifiers; /* SDL KMOD_SHIFT=3, CTRL=0xc0, ALT=0x300 */
} PC_Event;
typedef struct {
    const uint8_t *data;
    size_t size;
} PC_Blob;
typedef void (*PC_AudioCallback)(void *userdata, int16_t *mono, unsigned frames);
typedef struct {
    unsigned freq, samples;
    PC_AudioCallback callback;
    void *userdata;
} PC_AudioSpec;
/* Platform calls: core 0 only. Buffers passed to rendering remain owned by caller.
   Rendering completes synchronously; DMA transfers pixels without a framebuffer. */
void PC_Init(void);
bool PC_PollEvent(PC_Event *event);
void PC_SetRenderDrawColor(uint8_t r, uint8_t g, uint8_t b);
void PC_RenderFillRect(const PC_Rect *rect);
void PC_UpdateTexture(const PC_Rect *rect, const uint16_t *rgb565, unsigned pitch_bytes);
/* One conservative text interval per source row; right is exclusive. */
typedef struct { uint16_t left, right; } PC_TextSpan;
typedef struct {
    bool enabled;
    const PC_TextSpan *text_rows;
    unsigned source_x;
} PC_VideoFilter;
/* Vertical scaling: area-weighted RGB565 filtering for 200 -> 240, exact copy at 1:1,
   nearest sampling otherwise. NULL filter enables filtering without text
   exclusions. text_rows, if supplied, has 200 entries; source_x is the absolute
   column of the first supplied pixel. Full rectangle must be within LCD bounds. */
void PC_UpdateTextureScaledY(const PC_Rect *rect, const uint16_t *rgb565,
                            unsigned pitch_bytes, unsigned source_height,
                            const PC_VideoFilter *filter);
void PC_RenderPresent(void);
void PC_DrawText(int x, int y, const char *text);
bool PC_OpenAudioDevice(const PC_AudioSpec *spec); /* launches core 1 once */
unsigned PC_AudioUnderruns(void);
unsigned PC_AudioMaxRenderUs(void);
void PC_Delay(unsigned ms);
/* Engine: init before opening audio; commands are SPSC core0 -> core1.
   Blob memory must remain valid (const flash arrays recommended). */
void PC_MixerInit(void);
bool PC_PlayWAV(unsigned slot, PC_Blob blob); /* slots 0 and 1, restart independently */
bool PC_StopAudio(void);
void PC_MixAudio(void *unused, int16_t *out, unsigned frames);
unsigned PC_MixerErrors(void);
unsigned PC_MixerActive(void); /* bit0 MIDI incl release, bit1 WAV0, bit2 WAV1 */

/* Extra entry points for SDLPoP. PoP bank must outlive playback. */
bool PC_PlayPoPMIDI(PC_Blob midi, PC_Blob prince_bank, unsigned sound_id);
bool PC_PlayPCM(unsigned slot, PC_Blob pcm, unsigned rate, unsigned channels, unsigned bits);
bool PC_StopMIDI(void);
bool PC_StopWAV(unsigned slot);
bool PC_MusicSequencing(void); /* excludes the final OPL release tail */

/* Queue a fence after accepted stop commands before freeing old source buffers.
   Tickets valid only until PC_MixerInit; poll from core0, never in the callback. */
bool PC_AudioFence(uint32_t *ticket);
bool PC_AudioFenceComplete(uint32_t ticket);
