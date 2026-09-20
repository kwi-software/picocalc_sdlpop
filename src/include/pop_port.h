/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "pc_surface.h"
/* SDL2 USB-HID scancodes used by SDLPoP. Not a replacement SDL.h. */
enum {
    POP_SCAN_RETURN = 40,
    POP_SCAN_ESCAPE = 41,
    POP_SCAN_SPACE = 44,
    POP_SCAN_F1 = 58,
    POP_SCAN_F2 = 59,
    POP_SCAN_F3 = 60,
    POP_SCAN_RIGHT = 79,
    POP_SCAN_LEFT = 80,
    POP_SCAN_DOWN = 81,
    POP_SCAN_UP = 82,
    POP_SCAN_LCTRL = 224,
    POP_SCAN_LSHIFT = 225,
    POP_SCAN_LALT = 226,
    POP_SCAN_RSHIFT = 229
};
void PC_InputReset(void);
void PC_InputFeed(uint8_t raw_code, unsigned state); /* SB: 1 down,2 hold,3 up */
bool PC_InputNext(PC_Event *event);
const uint8_t *PC_GetKeyboardState(unsigned *count); /* 512 bytes, scancodes */
unsigned PC_InputOverflows(void);
void PC_PumpEvents(void); /* hardware on core0 */
uint32_t PC_GetTicks(void);
uint64_t PC_GetPerformanceCounter(void); /* microseconds */
/* Apply an event to SDLPoP's key_states (HELD=1, HELD_NEW=2).
   Menu/fullscreen/text/controller handling remains the game's responsibility. */
void POP_ApplyKeyEvent(const PC_Event *event, uint8_t *key_states, size_t count, int *last_key,
                       int *last_any_key);
/* Raw sound_buffer_type resource bytes, INCLUDING type byte, before conversion.
   version=1: DOS 1.0/1.1, version=2: 1.3/1.4; 0 autodetect, ambiguity rejected.
   bank: PRINCE resource 1, count byte followed by 16-byte instrument records. */
bool POP_PlaySoundResource(PC_Blob sound, PC_Blob bank, unsigned slot, unsigned sound_id,
                           unsigned version);
bool POP_FindDATResource(PC_Blob dat, uint16_t id, PC_Blob *resource);
