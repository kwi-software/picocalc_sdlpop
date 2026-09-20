#pragma once
#include "board.h"
#include <stdint.h>
/* First-order error feedback at the 120kHz carrier. Average over five wraps
   preserves sub-LSB PCM detail; quantization error is moved toward the carrier.
   No random dither (silence remains an exactly constant 50 percent duty). */
static inline void pc_pwm_encode(const int16_t *pcm, uint32_t *out,
                                 unsigned frames, uint32_t *residue) {
  uint32_t error = *residue;
  for (unsigned i = 0; i < frames; i++) {
    uint32_t target = (uint32_t)((int32_t)pcm[i] + 32768) * (PC_PWM_WRAP + 1);
    for (unsigned j = 0; j < PC_PWM_OVERSAMPLE; j++) {
      uint32_t v = target + error;
      uint32_t duty = v >> 16;
      error = v & 65535;
      *out++ = duty | (duty << 16);
    }
  }
  *residue = error;
}
