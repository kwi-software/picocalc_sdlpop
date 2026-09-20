/* Test the actual platform driver against an RGB565 LCD wire decoder.
   Independent expected colors, clipping, pitched rows and CS framing. */
#include "board.h"
#include "mock_sdk.h"
#include "pc_media.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
spi_hw_t mock_spi;
static bool cs = true, dc;
static unsigned command, nparam, param[4], x0, x1, y0, y1, pos, component,
    pixel, bytes;
static uint32_t display[320 * 320];
static unsigned format, frame_bits, busy;
static uint64_t now_ns, high_since, dc_since;
static unsigned windows;
static void bus(uint8_t v) {
  assert(!cs);
  if (!dc) {
    if (command == 0x2c)
      assert(bytes == (x1 - x0 + 1) * (y1 - y0 + 1) * 2 && !component);
    command = v;
    nparam = 0;
    if (v == 0x2c) {
      windows++;
      pos = component = pixel = bytes = 0;
      assert(format == 0x55);
    }
    return;
  }
  if (command == 0x3a)
    format = v;
  if (command == 0x2a || command == 0x2b) {
    assert(nparam < 4);
    param[nparam++] = v;
    if (nparam == 4) {
      unsigned lo = param[0] * 256 + param[1], hi = param[2] * 256 + param[3];
      assert(lo <= hi && hi < 320);
      if (command == 0x2a) {
        x0 = lo;
        x1 = hi;
      } else {
        y0 = lo;
        y1 = hi;
      }
    }
  }
  if (command == 0x2c) {
    bytes++;
    pixel = (pixel << 8) | v;
    if (++component == 2) {
      unsigned x = x0 + pos % (x1 - x0 + 1), y = y0 + pos / (x1 - x0 + 1);
      assert(y <= y1);
      display[y * 320 + x] = pixel;
      component = pixel = 0;
      pos++;
    }
  }
}
void gpio_put(unsigned p, bool v) {
  if (p == PC_LCD_CS) {
    if (cs && !v) {
      assert(now_ns - high_since >= 40);
      assert(now_ns - dc_since >= 40);
    }
    if (v) {
      assert(!busy);
      high_since = now_ns;
    }
    if (!cs && v && command == 0x2c && dc)
      assert(bytes == (x1 - x0 + 1) * (y1 - y0 + 1) * 2 && !component);
    cs = v;
  }
  if (p == PC_LCD_DC) {
    assert(cs);
    dc = v;
    dc_since = now_ns;
  }
}
void gpio_init(unsigned p) { (void)p; }
void gpio_set_dir(unsigned p, bool v) {
  (void)p;
  (void)v;
}
void gpio_set_function(unsigned p, unsigned v) {
  (void)p;
  (void)v;
}
void sleep_ms(unsigned n) { now_ns += (uint64_t)n * 1000000; }
void busy_wait_us(unsigned n) { now_ns += (uint64_t)n * 1000; }
void tight_loop_contents(void) {}
unsigned spi_init(spi_hw_t *s, unsigned n) {
  (void)s;
  return n;
}
void spi_set_format(spi_hw_t *s, unsigned bits, unsigned pol, unsigned phase,
                    unsigned order) {
  (void)s;
  assert(cs && !busy && (bits == 8 || bits == 16) && pol == 0 && phase == 0 &&
         order == 0);
  frame_bits = bits;
}
int spi_write_blocking(spi_hw_t *s, const uint8_t *p, size_t n) {
  (void)s;
  assert(frame_bits == 8);
  for (size_t i = 0; i < n; i++)
    bus(p[i]);
  return (int)n;
}
bool spi_is_readable(spi_hw_t *s) {
  (void)s;
  return false;
}
bool spi_is_busy(spi_hw_t *s) {
  (void)s;
  if (busy) {
    busy--;
    return true;
  }
  return false;
}
spi_hw_t *spi_get_hw(spi_hw_t *s) { return s; }
unsigned spi_get_dreq(spi_hw_t *s, bool tx) {
  (void)s;
  return tx;
}
int dma_claim_unused_channel(bool b) {
  static int next;
  assert(b);
  return next++;
}
dma_channel_config dma_channel_get_default_config(unsigned c) {
  (void)c;
  return (dma_channel_config){0};
}
void channel_config_set_transfer_data_size(dma_channel_config *c, unsigned n) {
  assert(n == DMA_SIZE_16);
  c->size = n;
}
void channel_config_set_read_increment(dma_channel_config *c, bool b) {
  c->inc = b;
}
void channel_config_set_write_increment(dma_channel_config *c, bool b) {
  (void)c;
  assert(!b);
}
void channel_config_set_dreq(dma_channel_config *c, unsigned n) {
  (void)c;
  (void)n;
}
static const volatile uint16_t *source;
static unsigned counts[2];
void dma_channel_configure(unsigned c, const dma_channel_config *cfg,
                           volatile void *dst, const volatile void *src,
                           unsigned n, bool start) {
  assert(c < 2 && !start);
  counts[c] = n;
  if (c == 0) {
    assert(dst == &mock_spi.dr && cfg->inc);
    source = src;
  } else
    assert(src == &mock_spi.dr && !cfg->inc);
}
void dma_start_channel_mask(unsigned mask) {
  assert(mask == 3 && counts[0] == counts[1]);
  assert(frame_bits == 16 && dc && !busy);
  for (unsigned i = 0; i < counts[0]; i++) {
    bus(source[i] >> 8);
    bus(source[i] & 255);
  }
  busy = 2; /* DMA complete does not imply last SPI frame left the pin. */
}
void dma_channel_wait_for_finish_blocking(unsigned c) { (void)c; }
void pc_video_init(void);
int main(void) {
  pc_video_init();
  assert(cs && format == 0x55);
  for (unsigned i = 0; i < 320 * 320; i++)
    assert(display[i] == 0);
  PC_SetRenderDrawColor(255, 0, 0);
  PC_Rect r = {-2, -1, 5, 3};
  PC_RenderFillRect(&r);
  assert(display[0] == 0xf800 && display[322] == 0xf800 && display[3] == 0);
  static uint16_t colors[256 * 256];
  for (unsigned i = 0; i < 65536; i++)
    colors[i] = (uint16_t)i;
  r = (PC_Rect){0, 0, 256, 256};
  PC_UpdateTexture(&r, colors, 512);
  for (unsigned i = 0; i < 65536; i++) {
    assert(display[(i / 256) * 320 + i % 256] == i);
  }
  /* Pitched input, top/left clipping: visible red/green, blue/white. */
  uint16_t pitched[12] = {0,      0, 0, 0,      0,      0xf800,
                          0x07e0, 0, 0, 0x001f, 0xffff, 0};
  r = (PC_Rect){-1, -1, 3, 3};
  PC_UpdateTexture(&r, pitched, 8);
  assert(display[0] == 0xf800 && display[1] == 0x07e0);
  assert(display[320] == 0x001f && display[321] == 0xffff);
  /* Exercise the old failure pattern: back-to-back tiny address windows. */
  PC_SetRenderDrawColor(255, 255, 255);
  for (int y = 0; y < 320; y += 17)
    for (int x = 0; x < 320; x += 13) {
      r = (PC_Rect){x, y, 1, 1};
      PC_RenderFillRect(&r);
      assert(display[y * 320 + x] == 0xffff);
    }
  PC_SetRenderDrawColor(0, 0, 0);
  PC_RenderFillRect(NULL);
  PC_SetRenderDrawColor(255, 255, 255);
  unsigned before = windows;
  PC_DrawText(6, 6, "A1");
  assert(windows - before == 2); /* one window per whole glyph */
  const char *reference[7] = {".###.", "#...#", "#...#", "#####",
                              "#...#", "#...#", "#...#"};
  for (unsigned y = 0; y < 14; y++)
    for (unsigned x = 0; x < 10; x++)
      assert(display[(6 + y) * 320 + 6 + x] ==
             (reference[y / 2][x / 2] == '#' ? 0xffff : 0));
  /* Text still works in bottom status region and clips at the panel edge. */
  PC_DrawText(6, 290, "A");
  PC_DrawText(316, 316, "A");
  assert(display[290 * 320 + 8] == 0xffff &&
         display[316 * 320 + 318] == 0xffff);
  PC_SetRenderDrawColor(0,0,0);
  PC_RenderFillRect(NULL);
  static uint16_t source_frame[200][320];
  for(unsigned y=0;y<200;y++) for(unsigned x=0;x<320;x++) source_frame[y][x]=y*320+x;
  before=windows;
  PC_UpdateTextureScaledY(&(PC_Rect){0,40,320,240}, &source_frame[0][0], 640, 200);
  assert(windows==before+1);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x] == (y>=40 && y<280 ? source_frame[(y-40)*5/6][x] : 0));
  /* Switching to 16:10 must clear rows left over from 4:3. */
  PC_RenderFillRect(NULL);
  before=windows;
  PC_UpdateTextureScaledY(&(PC_Rect){0,60,320,200}, &source_frame[0][0],640,200);
  assert(windows==before+1);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x]==(y>=60 && y<260 ? source_frame[y-60][x] : 0));
  /* Wipe presents only two changed columns with full-frame source pitch. */
  for(unsigned y=0;y<200;y++) {source_frame[y][158]=0xa55a;source_frame[y][159]=0x5aa5;}
  PC_UpdateTextureScaledY(&(PC_Rect){158,60,2,200}, &source_frame[0][158],640,200);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x]==(y>=60 && y<260 ? source_frame[y-60][x] : 0));
  assert(cs && frame_bits == 8);
  puts("PASS: RGB565 65536 colors, 16-bit DMA, CS/DC setup, SPI drain, tiny "
       "windows, text, clipping/pitch, 320x200 to 320x240 in one window");
}
