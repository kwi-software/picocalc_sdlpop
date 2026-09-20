/* Include at the END of SDLPoP config.h for the PicoCalc game port.
   Disabling these features does not itself remove every SDL/POSIX dependency. */
#pragma once
#ifdef PICOCALC
#undef USE_ALPHA
#undef USE_FADE
#undef USE_LIGHTING
#undef USE_SCREENSHOT
#undef USE_MENU
#undef USE_COLORED_TORCHES
#undef USE_QUICKSAVE
#undef USE_QUICKLOAD_PENALTY
#undef USE_REPLAY
#undef USE_FAST_FORWARD
#undef USE_DARK_TRANSITION
#undef USE_DEBUG_CHEATS
#undef USE_COMPAT_TIMER
#undef USE_AUTO_INPUT_MODE
#endif
