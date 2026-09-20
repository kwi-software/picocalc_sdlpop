#include "board.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pc_media.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pwm_encode.h"
#include "pc_store.h"
#include <assert.h>
#include <stdatomic.h>
/* Pool ownership: FREE -> core1 filling -> READY -> DMA -> FREE.
   Two chained channels always consume immutable buffers. If the producer misses
   its deadline an immutable silence buffer replaces that block. No in-place
   rewriting of a buffer that DMA may already be reading. IRQ lives on core1. */
static uint32_t pool[3][PC_AUDIO_FRAMES * PC_PWM_OVERSAMPLE]
    __attribute__((aligned(4)));
static uint32_t silence[PC_AUDIO_FRAMES * PC_PWM_OVERSAMPLE]
    __attribute__((aligned(4)));
static int16_t pcm[PC_AUDIO_FRAMES];
static int channels[2], playing[2];
static volatile unsigned free_mask, rd, wr;
static uint8_t ready[8];
static PC_AudioSpec audio;
static _Atomic unsigned underruns, max_render_us, started;
static bool opened;
_Static_assert((PC_PWM_WRAP + 1) % 2 == 0, "Exact midpoint required");
#define PWM_MID ((PC_PWM_WRAP + 1) / 2)
#define PWM_WORDS (PC_AUDIO_FRAMES * PC_PWM_OVERSAMPLE)
static int take_ready(void) {
  if (rd == wr)
    return -1;
  int s = ready[rd & 7];
  rd++;
  return s;
}
static void __not_in_flash_func(audio_irq)(void) {
  uint32_t pending = dma_hw->ints1;
  for (unsigned i = 0; i < 2; i++)
    if (pending & (1u << channels[i])) {
      dma_hw->ints1 = 1u << channels[i];
      if (playing[i] >= 0)
        free_mask |= 1u << playing[i];
      int slot = take_ready();
      playing[i] = slot;
      if (slot < 0)
        atomic_fetch_add_explicit(&underruns, 1, memory_order_relaxed);
      dma_channel_set_read_addr(channels[i], slot < 0 ? silence : pool[slot],
                                false);
      dma_channel_set_trans_count(channels[i], PWM_WORDS, false);
    }
  __sev();
  /* Under normal operation the partner is already running via CHAIN_TO.
     Recover if both completion IRQs were delayed for more than a block. */
  if (!dma_channel_is_busy(channels[0]) && !dma_channel_is_busy(channels[1])) {
    atomic_fetch_add_explicit(&underruns, 1, memory_order_relaxed);
    dma_start_channel_mask(1u << channels[0]);
  }
}
static void fill(unsigned slot) {
  uint32_t t = time_us_32();
  audio.callback(audio.userdata, pcm, PC_AUDIO_FRAMES);
  static uint32_t residue = 32768;
  pc_pwm_encode(pcm, pool[slot], PC_AUDIO_FRAMES, &residue);
  unsigned elapsed = time_us_32() - t;
  if (elapsed > atomic_load_explicit(&max_render_us, memory_order_relaxed))
    atomic_store_explicit(&max_render_us, elapsed, memory_order_relaxed);
}
static _Atomic unsigned flash_request, flash_parked;
/* Called by core 1 only, between completed render blocks. Entire parked loop
   and its constants live in RAM while core 0 temporarily disables XIP. */
static void __no_inline_not_in_flash_func(flash_pause)(void) {
  uint32_t interrupts=save_and_disable_interrupts();
  for(unsigned i=0;i<2;i++) dma_channel_set_irq1_enabled(channels[i],false);
  /* Disable both channels before aborting: neither may chain-restart the other. */
  for(unsigned i=0;i<2;i++) hw_clear_bits(&dma_channel_hw_addr(channels[i])->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
  for(unsigned i=0;i<2;i++) dma_channel_abort(channels[i]);
  dma_hw->ints1=(1u<<channels[0])|(1u<<channels[1]);
  pwm_set_both_levels(pwm_gpio_to_slice_num(PC_AUDIO_LEFT),PWM_MID,PWM_MID);
  atomic_store_explicit(&flash_parked,1,memory_order_release);
  __sev();
  while(atomic_load_explicit(&flash_request,memory_order_acquire)) __wfe();
  rd=wr=0;free_mask=7;
  for(unsigned i=0;i<2;i++) {
    playing[i]=-1;
    dma_channel_set_read_addr(channels[i],silence,false);
    dma_channel_set_trans_count(channels[i],PWM_WORDS,false);
    hw_set_bits(&dma_channel_hw_addr(channels[i])->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_set_irq1_enabled(channels[i],true);
  }
  dma_start_channel_mask(1u<<channels[0]);
  atomic_store_explicit(&flash_parked,0,memory_order_release);
  restore_interrupts(interrupts);
  __sev();
}
void PC_AudioFlashBegin(void) {
  if(!opened)return;
  atomic_store_explicit(&flash_request,1,memory_order_release);__sev();
  while(!atomic_load_explicit(&flash_parked,memory_order_acquire)) __wfe();
}
void PC_AudioFlashEnd(void) {
  if(!opened)return;
  atomic_store_explicit(&flash_request,0,memory_order_release);__sev();
  while(atomic_load_explicit(&flash_parked,memory_order_acquire)) __wfe();
}
static void core1_audio(void) {
  unsigned slice = pwm_gpio_to_slice_num(PC_AUDIO_LEFT);
  assert(slice == pwm_gpio_to_slice_num(PC_AUDIO_RIGHT));
  assert(pwm_gpio_to_channel(PC_AUDIO_LEFT) !=
         pwm_gpio_to_channel(PC_AUDIO_RIGHT));
  gpio_set_function(PC_AUDIO_LEFT, GPIO_FUNC_PWM);
  gpio_set_function(PC_AUDIO_RIGHT, GPIO_FUNC_PWM);
  pwm_config c = pwm_get_default_config();
  pwm_config_set_wrap(&c, PC_PWM_WRAP);
  pwm_config_set_clkdiv(&c, 1.f);
  pwm_init(slice, &c, false);
  pwm_set_both_levels(slice, PWM_MID, PWM_MID);
  for (unsigned i = 0; i < PWM_WORDS; i++)
    silence[i] = PWM_MID | (PWM_MID << 16);
  for (unsigned i = 0; i < 3; i++) {
    fill(i);
    ready[wr++ & 7] = i;
  }
  /* Update CC only at PWM wraps. Five identical words per PCM sample give
     exactly 24kHz at 150MHz; asynchronous timer pacing caused varying sample
     lengths at the PWM latch. No fractional divider or extra audio DMA timer.
   */
  assert(clock_get_hz(clk_sys) ==
         PC_AUDIO_RATE * PC_PWM_OVERSAMPLE * (PC_PWM_WRAP + 1));
  channels[0] = dma_claim_unused_channel(true);
  channels[1] = dma_claim_unused_channel(true);
  for (unsigned i = 0; i < 2; i++) {
    playing[i] = take_ready();
    dma_channel_config dc = dma_channel_get_default_config(channels[i]);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pwm_get_dreq(slice));
    channel_config_set_chain_to(&dc, channels[1 - i]);
    channel_config_set_high_priority(&dc, true);
    dma_channel_configure(channels[i], &dc, &pwm_hw->slice[slice].cc,
                          pool[playing[i]], PWM_WORDS, false);
    dma_hw->ints1 = 1u << channels[i];
    dma_channel_set_irq1_enabled(channels[i], true);
  }
  irq_set_exclusive_handler(DMA_IRQ_1, audio_irq);
  irq_set_priority(DMA_IRQ_1, 0x40);
  irq_set_enabled(DMA_IRQ_1, true);
  pwm_set_enabled(slice, true);
  dma_start_channel_mask(1u << channels[0]);
  atomic_store_explicit(&started, 1, memory_order_release);
  for (;;) {
    if(atomic_load_explicit(&flash_request,memory_order_acquire)) flash_pause();
    int slot = -1;
    uint32_t state = save_and_disable_interrupts();
    if (free_mask) {
      slot = __builtin_ctz(free_mask);
      free_mask &= ~(1u << slot);
    }
    restore_interrupts(state);
    if (slot < 0) {
      __wfe();
      continue;
    }
    fill(slot);
    state = save_and_disable_interrupts();
    __dmb();
    ready[wr & 7] = slot;
    wr++;
    restore_interrupts(state);
  }
}
bool PC_OpenAudioDevice(const PC_AudioSpec *spec) {
  if (opened || !spec || !spec->callback || spec->freq != PC_AUDIO_RATE ||
      spec->samples != PC_AUDIO_FRAMES)
    return false;
  unsigned clock = clock_get_hz(clk_sys);
  if (clock != PC_AUDIO_RATE * PC_PWM_OVERSAMPLE * (PC_PWM_WRAP + 1))
    return false;
  if (PC_AUDIO_LEFT > 29 || PC_AUDIO_RIGHT > 29 || PC_AUDIO_LEFT < 0 ||
      PC_AUDIO_RIGHT < 0 ||
      pwm_gpio_to_slice_num(PC_AUDIO_LEFT) !=
          pwm_gpio_to_slice_num(PC_AUDIO_RIGHT) ||
      pwm_gpio_to_channel(PC_AUDIO_LEFT) == pwm_gpio_to_channel(PC_AUDIO_RIGHT))
    return false;
  audio = *spec;
  opened = true;
  multicore_launch_core1(core1_audio);
  while (!atomic_load_explicit(&started, memory_order_acquire))
    tight_loop_contents();
  return true;
}
unsigned PC_AudioUnderruns(void) {
  return atomic_load_explicit(&underruns, memory_order_relaxed);
}
unsigned PC_AudioMaxRenderUs(void) {
  return atomic_load_explicit(&max_render_us, memory_order_relaxed);
}
