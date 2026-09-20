/* LCD initialization/reference work derives from Blair Leduc's MIT-licensed
 * picocalc-text-starter and the user-provided display drivers.
 * Copyright (c) 2025 Blair Leduc (original driver portions).
 * See COPYING.PICOCALC and ASSET_SOURCES.md. DMA scanout is adapted for this port.
 */
#include "board.h"
#include "hardware/dma.h"
#include "hardware/regs/spi.h"
#include "hardware/spi.h"
#include "pc_media.h"
#include "pico/stdlib.h"
#include <limits.h>
#include <string.h>
static int tx, rx;
static uint16_t color, line[PC_LCD_WIDTH], discard;
/* ST7365P RGB565, following the supplied lcd.c/display_backend_pico.c.
   Core 0 owns SPI; core 1 remains dedicated to audio. */
static void select_bus(bool data) {
  /* Explicit CS-high and D/C setup time, independent of optimizer/inlining.
     Supplied working driver requires >=40 ns. One us is conservative and
     paid once per transaction, never per pixel. */
  gpio_put(PC_LCD_DC, data);
  busy_wait_us(1);
  gpio_put(PC_LCD_CS, 0);
}
static void release_bus(void) {
  while (spi_is_busy(PC_LCD_SPI))
    tight_loop_contents();
  gpio_put(PC_LCD_CS, 1);
}
static void cmd(uint8_t c, const uint8_t *p, unsigned n) {
  select_bus(false);
  spi_write_blocking(PC_LCD_SPI, &c, 1);
  release_bus();
  if (n) {
    select_bus(true);
    spi_write_blocking(PC_LCD_SPI, p, n);
    release_bus();
  }
}
static void window(int x, int y, int w, int h) {
  uint8_t a[4] = {x >> 8, x, (x + w - 1) >> 8, x + w - 1},
          b[4] = {y >> 8, y, (y + h - 1) >> 8, y + h - 1};
  cmd(0x2a, a, 4);
  cmd(0x2b, b, 4);
  cmd(0x2c, 0, 0);
  /* Change frame size while CS is HIGH; native uint16_t goes out MSB first. */
  spi_set_format(PC_LCD_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  select_bus(true);
}
static void end_window(void) {
  release_bus();
  spi_set_format(PC_LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}
static void pixels(const uint16_t *p, unsigned n) {
  while (spi_is_readable(PC_LCD_SPI))
    (void)spi_get_hw(PC_LCD_SPI)->dr;
  spi_get_hw(PC_LCD_SPI)->icr = SPI_SSPICR_RORIC_BITS;
  dma_channel_config a = dma_channel_get_default_config(tx),
                     b = dma_channel_get_default_config(rx);
  channel_config_set_transfer_data_size(&a, DMA_SIZE_16);
  channel_config_set_read_increment(&a, true);
  channel_config_set_write_increment(&a, false);
  channel_config_set_dreq(&a, spi_get_dreq(PC_LCD_SPI, true));
  channel_config_set_transfer_data_size(&b, DMA_SIZE_16);
  channel_config_set_read_increment(&b, false);
  channel_config_set_write_increment(&b, false);
  channel_config_set_dreq(&b, spi_get_dreq(PC_LCD_SPI, false));
  dma_channel_configure(rx, &b, &discard, &spi_get_hw(PC_LCD_SPI)->dr, n,
                        false);
  dma_channel_configure(tx, &a, &spi_get_hw(PC_LCD_SPI)->dr, p, n, false);
  dma_start_channel_mask((1u << tx) | (1u << rx));
  dma_channel_wait_for_finish_blocking(tx);
  dma_channel_wait_for_finish_blocking(rx);
  while (spi_is_busy(PC_LCD_SPI))
    tight_loop_contents();
}
void pc_video_init(void) {
  const unsigned pins[] = {PC_LCD_CS, PC_LCD_DC, PC_LCD_RESET};
  for (unsigned i = 0; i < 3; i++) {
    gpio_init(pins[i]);
    gpio_set_dir(pins[i], GPIO_OUT);
    gpio_put(pins[i], 1);
  }
  spi_init(PC_LCD_SPI, PC_LCD_HZ);
  spi_set_format(PC_LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(PC_LCD_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PC_LCD_MOSI, GPIO_FUNC_SPI);
  tx = dma_claim_unused_channel(true);
  rx = dma_claim_unused_channel(true);
  gpio_put(PC_LCD_RESET, 0);
  busy_wait_us(20);
  gpio_put(PC_LCD_RESET, 1);
  sleep_ms(120);
  cmd(0x01, 0, 0);
  sleep_ms(10);
  /* Minimal known-working ST7365P setup from the supplied backend.
     Do not apply the ILI9488 gamma/power/interface register sequence. */
  cmd(0x3a, (const uint8_t[]){0x55}, 1);
  cmd(0x36, (const uint8_t[]){PC_LCD_MADCTL}, 1);
  cmd(0x21, 0, 0);
  cmd(0xb7, (const uint8_t[]){0xc6}, 1);
  cmd(0x11, 0, 0);
  sleep_ms(120);
  PC_SetRenderDrawColor(0, 0, 0);
  PC_RenderFillRect(0);
  cmd(0x29, 0, 0);
  sleep_ms(10);
}
void PC_SetRenderDrawColor(uint8_t r, uint8_t g, uint8_t b) {
  color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
static bool clip(PC_Rect *r) {
  if (r->w <= 0 || r->h <= 0 || r->x >= PC_LCD_WIDTH || r->y >= PC_LCD_HEIGHT)
    return false;
  int64_t right = (int64_t)r->x + r->w, bottom = (int64_t)r->y + r->h;
  if (right <= 0 || bottom <= 0)
    return false;
  if (r->x < 0)
    r->x = 0;
  if (r->y < 0)
    r->y = 0;
  if (right > PC_LCD_WIDTH)
    right = PC_LCD_WIDTH;
  if (bottom > PC_LCD_HEIGHT)
    bottom = PC_LCD_HEIGHT;
  r->w = (int)right - r->x;
  r->h = (int)bottom - r->y;
  return true;
}
void PC_RenderFillRect(const PC_Rect *rect) {
  PC_Rect r = rect ? *rect : (PC_Rect){0, 0, PC_LCD_WIDTH, PC_LCD_HEIGHT};
  if (!clip(&r))
    return;
  window(r.x, r.y, r.w, r.h);
  for (int x = 0; x < r.w; x++)
    line[x] = color;
  for (int y = 0; y < r.h; y++)
    pixels(line, (unsigned)r.w);
  end_window();
}
void PC_UpdateTexture(const PC_Rect *rect, const uint16_t *rgb,
                      unsigned pitch) {
  if (!rect || !rgb || rect->w <= 0 || rect->h <= 0 ||
      (uint64_t)rect->w * 2 > pitch || (pitch & 1))
    return;
  PC_Rect r = *rect;
  if (!clip(&r))
    return;
  /* User must provide the complete source rectangle with at least height*pitch
   * bytes. */
  const uint8_t *row = (const uint8_t *)rgb +
                       (size_t)((int64_t)r.y - rect->y) * pitch +
                       (size_t)((int64_t)r.x - rect->x) * 2;
  window(r.x, r.y, r.w, r.h);
  for (int y = 0; y < r.h; y++) {
    memcpy(line, row, (size_t)r.w * sizeof(uint16_t));
    pixels(line, (unsigned)r.w);
    row += pitch;
  }
  end_window();
}
void PC_UpdateTextureScaledY(const PC_Rect *r, const uint16_t *rgb,
                             unsigned pitch, unsigned source_height) {
  if (!r || !rgb || !source_height || r->x < 0 || r->y < 0 || r->w <= 0 ||
      r->h <= 0 || r->w > 320 || r->h > 320 || r->x > 320 - r->w ||
      r->y > 320 - r->h || pitch < (unsigned)r->w * 2 || (pitch & 1))
    return;
  window(r->x, r->y, r->w, r->h);
  unsigned prev = UINT_MAX;
  for (unsigned y = 0; y < (unsigned)r->h; y++) {
    unsigned src = (uint64_t)y * source_height / (unsigned)r->h;
    if (src != prev)
      memcpy(line, (const uint8_t *)rgb + (size_t)src * pitch, r->w * 2);
    pixels(line, r->w);
    prev = src;
  }
  end_window();
}
void PC_RenderPresent(void) {
} /* Immediate rendering; all DMA complete on return. */
/* Original tiny uppercase 5x7 font: symbols for fatal-error diagnostics. */
static const char glyph_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./";
static const uint8_t glyphs[][7] = {
    {0, 0, 0, 0, 0, 0, 0},        {14, 17, 17, 31, 17, 17, 17},
    {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15},
    {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31}, {17, 27, 21, 21, 17, 17, 17},
    {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30},
    {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},  {17, 17, 10, 4, 4, 4, 4},
    {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2},
    {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14},   {0, 4, 4, 0, 4, 4, 0},
    {0, 0, 0, 31, 0, 0, 0},       {0, 0, 0, 0, 0, 4, 4},
    {1, 2, 2, 4, 8, 8, 16}};
void PC_DrawText(int x, int y, const char *s) {
  int start = x;
  while (*s) {
    unsigned char c = *s++;
    if (c == '\n') {
      x = start;
      y += 18;
      continue;
    }
    if (c >= 'a' && c <= 'z')
      c -= 32;
    const char *g = strchr(glyph_chars, c);
    unsigned idx = g ? (unsigned)(g - glyph_chars) : 0;
    /* One rectangle per glyph: 14x fewer CASET/RASET/RAMWR sequences.
       The 280-byte buffer lives until synchronous PC_UpdateTexture returns. */
    uint16_t glyph[14][10];
    for (int j = 0; j < 7; j++)
      for (int k = 0; k < 5; k++) {
        uint16_t c565 = (glyphs[idx][j] & (16 >> k)) ? color : 0;
        glyph[j * 2][k * 2] = glyph[j * 2][k * 2 + 1] = c565;
        glyph[j * 2 + 1][k * 2] = glyph[j * 2 + 1][k * 2 + 1] = c565;
      }
    PC_Rect r = {x, y, 10, 14};
    PC_UpdateTexture(&r, &glyph[0][0], 20);
    x += 12;
  }
}
