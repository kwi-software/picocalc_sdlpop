/* Deliberately limited source adapter for the pinned SDLPoP game core.
   Not an SDL binary ABI and not a general SDL implementation. */
#pragma once
#include "pop_port.h"
#include <stdint.h>
typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef PC_Surface SDL_Surface;
typedef PC_Rect SDL_Rect;
typedef struct {
  Uint8 r, g, b, a;
} SDL_Color;
typedef void SDL_Window;
typedef void SDL_Renderer;
typedef void SDL_Texture;
typedef void SDL_Haptic;
typedef void SDL_Joystick;
typedef void SDL_GameController;
typedef void SDL_RWops;
typedef int SDL_TimerID;
typedef struct {
  Uint8 major, minor, patch;
} SDL_version;
#define SDL_COMPILE_TIME_ASSERT(name, x) _Static_assert(x, #name)
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN
#define SDL_VERSION_ATLEAST(a, b, c) 0
#define SDL_SwapLE16(x) ((uint16_t)(x))
#define SDL_SwapLE32(x) ((uint32_t)(x))
#define SDL_NUM_SCANCODES 512
#define SDL_TRUE 1
#define SDL_FALSE 0
#define SDL_Delay PC_Delay
#define SDL_GetKeyboardState PC_GetKeyboardState
#define SDL_GetPerformanceCounter PC_GetPerformanceCounter
static inline Uint64 SDL_GetPerformanceFrequency(void) { return 1000000; }
static inline int SDL_LockSurface(SDL_Surface *s) {
  (void)s;
  return 0;
}
static inline void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }
static inline int SDL_SetColorKey(SDL_Surface *s, int on, Uint32 key) {
  return PC_SetColorKey(s, on ? (int)key : -1) ? 0 : -1;
}
static inline int SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *r) {
  return PC_SetClipRect(s, r);
}
void SDL_FreeSurface(SDL_Surface *s);
int SDL_SetPaletteColors(SDL_Surface *s, const SDL_Color *c, int first, int n);
static inline void SDL_SetWindowTitle(void *w, const char *t) {
  (void)w;
  (void)t;
}
static inline void SDL_GetVersion(SDL_version *v) {
  *v = (SDL_version){0, 0, 0};
}
#define SDL_VERSION(v) SDL_GetVersion(v)
static inline void SDL_SetTextInputRect(const SDL_Rect *r) { (void)r; }
static inline void SDL_StartTextInput(void) {}
static inline void SDL_StopTextInput(void) {}
static inline int SDL_HapticRumblePlay(void *p, float s, Uint32 n) {
  (void)p;
  (void)s;
  (void)n;
  return -1;
}
SDL_TimerID SDL_AddTimer(Uint32 ms, Uint32 (*fn)(Uint32, void *), void *data);
enum {
  SDL_CONTROLLER_AXIS_LEFTX,
  SDL_CONTROLLER_AXIS_LEFTY,
  SDL_CONTROLLER_AXIS_RIGHTX,
  SDL_CONTROLLER_AXIS_RIGHTY,
  SDL_CONTROLLER_AXIS_TRIGGERLEFT,
  SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};

enum {
  SDL_SCANCODE_A = 4,
  SDL_SCANCODE_B = 5,
  SDL_SCANCODE_C = 6,
  SDL_SCANCODE_D = 7,
  SDL_SCANCODE_E = 8,
  SDL_SCANCODE_F = 9,
  SDL_SCANCODE_G = 10,
  SDL_SCANCODE_H = 11,
  SDL_SCANCODE_I = 12,
  SDL_SCANCODE_J = 13,
  SDL_SCANCODE_K = 14,
  SDL_SCANCODE_L = 15,
  SDL_SCANCODE_M = 16,
  SDL_SCANCODE_N = 17,
  SDL_SCANCODE_O = 18,
  SDL_SCANCODE_P = 19,
  SDL_SCANCODE_Q = 20,
  SDL_SCANCODE_R = 21,
  SDL_SCANCODE_S = 22,
  SDL_SCANCODE_T = 23,
  SDL_SCANCODE_U = 24,
  SDL_SCANCODE_V = 25,
  SDL_SCANCODE_W = 26,
  SDL_SCANCODE_X = 27,
  SDL_SCANCODE_Y = 28,
  SDL_SCANCODE_Z = 29,
  SDL_SCANCODE_1 = 30,
  SDL_SCANCODE_8 = 37,
  SDL_SCANCODE_9 = 38,
  SDL_SCANCODE_0 = 39,
  SDL_SCANCODE_RETURN = 40,
  SDL_SCANCODE_ESCAPE = 41,
  SDL_SCANCODE_BACKSPACE = 42,
  SDL_SCANCODE_TAB = 43,
  SDL_SCANCODE_SPACE = 44,
  SDL_SCANCODE_LEFTBRACKET = 47,
  SDL_SCANCODE_RIGHTBRACKET = 48,
  SDL_SCANCODE_HOME = 74,
  SDL_SCANCODE_PAGEUP = 75,
  SDL_SCANCODE_DELETE = 76,
  SDL_SCANCODE_RIGHT = 79,
  SDL_SCANCODE_LEFT = 80,
  SDL_SCANCODE_DOWN = 81,
  SDL_SCANCODE_UP = 82,
  SDL_SCANCODE_CLEAR = 156,
  SDL_SCANCODE_LSHIFT = 225,
  SDL_SCANCODE_RSHIFT = 229,
  SDL_SCANCODE_KP_MINUS = 86,
  SDL_SCANCODE_KP_PLUS = 87,
  SDL_SCANCODE_F1 = 58,
  SDL_SCANCODE_F2 = 59,
  SDL_SCANCODE_F3 = 60,
  SDL_SCANCODE_F4 = 61,
  SDL_SCANCODE_F5 = 62,
  SDL_SCANCODE_F6 = 63,
  SDL_SCANCODE_F7 = 64,
  SDL_SCANCODE_F8 = 65,
  SDL_SCANCODE_F9 = 66,
  SDL_SCANCODE_F10 = 67,
  SDL_SCANCODE_F11 = 68,
  SDL_SCANCODE_F12 = 69,
  SDL_SCANCODE_KP_1 = 89,
  SDL_SCANCODE_KP_2 = 90,
  SDL_SCANCODE_KP_3 = 91,
  SDL_SCANCODE_KP_4 = 92,
  SDL_SCANCODE_KP_5 = 93,
  SDL_SCANCODE_KP_6 = 94,
  SDL_SCANCODE_KP_7 = 95,
  SDL_SCANCODE_KP_8 = 96,
  SDL_SCANCODE_KP_9 = 97
};
