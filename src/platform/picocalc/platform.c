/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "keyboard_matrix.h"
#include "pico/stdlib.h"
#include "pop_port.h"
#include "southbridge.h"
static uint32_t last_poll;
static bool first_poll = true;
void pc_video_init(void);
void PC_Init(void) {
  stdio_init_all();
  sb_init();
  PC_InputReset();
  pc_video_init();
  sb_write_lcd_backlight(180);
  uint8_t columns[8], arrows;
  bool message = false;
  while (!sb_read_keyboard_matrix(columns, &arrows)) {
    if (!message) {
      PC_SetRenderDrawColor(255, 180, 100);
      PC_DrawText(6, 100, "KEYBOARD MATRIX UNAVAILABLE");
      PC_DrawText(6, 120, "CHECK I2C / PICOCALC BIOS");
      message = true;
    }
    sleep_ms(250);
  }
  if (message) {
    PC_SetRenderDrawColor(0, 0, 0);
    PC_RenderFillRect(NULL);
  }
}
void PC_PumpEvents(void) {
  uint32_t now = PC_GetTicks();
  if (!first_poll && (uint32_t)(now - last_poll) < 40)
    return;
  first_poll = false;
  uint8_t columns[8], arrows;
  if (sb_read_keyboard_matrix(columns, &arrows)) {
    pc_keyboard_matrix_feed(columns, arrows);
  } else {
    /* A failed read must never leave movement stuck. Keep queue semantics
       and send releases; retry the physical snapshot on the next poll. */
    static const uint8_t all_up[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    pc_keyboard_matrix_feed(all_up, 255);
  }
  last_poll = now;
}
bool PC_PollEvent(PC_Event *e) {
  if (PC_InputNext(e))
    return true;
  PC_PumpEvents();
  return PC_InputNext(e);
}
uint32_t PC_GetTicks(void) { return to_ms_since_boot(get_absolute_time()); }
uint64_t PC_GetPerformanceCounter(void) { return time_us_64(); }
void PC_Delay(unsigned ms) {
  uint32_t start = PC_GetTicks();
  do {
    PC_PumpEvents();
    if ((uint32_t)(PC_GetTicks() - start) >= ms)
      break;
    sleep_ms(1);
  } while (true);
}
