/* Test the actual platform driver against an RGB565 LCD wire decoder.
   Independent expected colors, clipping, pitched rows and CS framing. */
#include "board.h"
#include "mock_sdk.h"
#include "pc_surface.h"
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
/* Independent rational reference using the full source/destination geometry. */
static uint16_t reference_scaled(const uint16_t *p, unsigned pitch,
                                 unsigned x, unsigned y) {
  /* Integrate source-pixel coverage in integer units: source=240, output=200.
     This reference uses geometry, independently of the six-phase implementation. */
  unsigned start=y*200, end=start+200, result=0;
  const unsigned shifts[]={0,5,11}, masks[]={31,63,31};
  for(unsigned c=0;c<3;c++) {
    unsigned sum=0;
    for(unsigned row=start/240;row<200 && row*240<end;row++) {
      unsigned lo=start>row*240?start:row*240;
      unsigned hi=end<(row+1)*240?end:(row+1)*240;
      const uint16_t *line=(const uint16_t *)((const uint8_t *)p+row*pitch);
      sum+=((line[x]>>shifts[c])&masks[c])*(hi-lo);
    }
    result|=((sum+100)/200)<<shifts[c];
  }
  return (uint16_t)result;
}
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
  PC_DrawText(6,6,"%");
  assert(display[6*320+6]==0xffff && display[6*320+10]==0 && display[6*320+14]==0xffff);
  before=windows;
  PC_DrawTextSmall(6,24,"L:%");
  assert(windows==before+3);
  /* Native horizontal and vertical stems are both one pixel wide. */
  for(unsigned y=0;y<11;y++)for(unsigned x=0;x<7;x++) {
    assert(display[(24+y)*320+6+x]==((x==0 || y==10)?0xffff:0));
    assert(display[(24+y)*320+16+x]==((x==3 && (y==3 || y==7))?0xffff:0));
  }
  assert(display[24*320+26+1]==0xffff);
  assert(display[34*320+26+5]==0xffff);
  PC_SetRenderDrawColor(0,0,0);
  PC_RenderFillRect(NULL);
  static uint16_t source_frame[200][320];
  for(unsigned y=0;y<200;y++) for(unsigned x=0;x<320;x++) source_frame[y][x]=y*320+x;
  before=windows;
  PC_UpdateTextureScaledY(&(PC_Rect){0,40,320,240}, &source_frame[0][0], 640, 200,NULL);
  assert(windows==before+1);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x] == (y>=40 && y<280 ? reference_scaled(&source_frame[0][0],640,x,y-40) : 0));
  /* A partial title wipe must match a full filtered frame and leave neighbours intact. */
  for(unsigned y=0;y<200;y++) {source_frame[y][158]=y&1 ? 0xf800 : 0x001f;
                             source_frame[y][159]=y&1 ? 0xffff : 0x07e0;}
  PC_UpdateTextureScaledY(&(PC_Rect){158,40,2,240}, &source_frame[0][158],640,200,NULL);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x] == (y>=40 && y<280 ? reference_scaled(&source_frame[0][0],640,x,y-40) : 0));
  /* Narrow pitched rows with guard values; filtering must not cross columns. */
  uint16_t narrow[200][3];
  for(unsigned y=0;y<200;y++) {narrow[y][0]=y&1 ? 0xffff : 0; narrow[y][1]=0xf800; narrow[y][2]=0x07e0;}
  PC_UpdateTextureScaledY(&(PC_Rect){319,40,1,240}, &narrow[0][0],6,200,NULL);
  assert(display[40*320+319]==0 && display[279*320+319]==0xffff);
  assert(display[41*320+319]==0xce59); /* 4/5 white: R=25, G=50, B=25. */
  for(unsigned y=0;y<240;y++) assert(display[(y+40)*320+319]==reference_scaled(&narrow[0][0],6,0,y));
  /* Text metadata follows clipped/overlapping frame copies and opaque erasure. */
  static uint16_t other_frame[200][320];
  PC_Surface src, dst;
  assert(PC_InitSurface(&src,320,200,640,PC_RGB565,source_frame,sizeof source_frame,true));
  assert(PC_InitSurface(&dst,320,200,640,PC_RGB565,other_frame,sizeof other_frame,true));
  PC_TrackTextSurface(0,&src); PC_TrackTextSurface(1,&dst);
  PC_MarkTextRect(&src,(PC_Rect){100,50,20,8});
  PC_Rect target={10,20,0,0}, region={90,50,40,8};
  assert(PC_BlitSurface(&src,&region,&dst,&target,PC_BLIT_COPY,false,0));
  assert(PC_SurfaceTextRows(&dst)[20].left==20 && PC_SurfaceTextRows(&dst)[20].right==40);
  PC_FillRect(&dst,&(PC_Rect){20,20,5,8},0);
  assert(PC_SurfaceTextRows(&dst)[20].left==25);
  target=(PC_Rect){0,21,0,0}; region=(PC_Rect){0,20,320,8};
  assert(PC_BlitSurface(&dst,&region,&dst,&target,PC_BLIT_COPY,false,0));
  assert(PC_SurfaceTextRows(&dst)[28].left==25 && PC_SurfaceTextRows(&dst)[28].right==40);
  PC_FlipTextRows(&dst);
  assert(PC_SurfaceTextRows(&dst)[171].left==25);
  PC_FillRect(&dst,NULL,0);
  for(unsigned y=0;y<200;y++)assert(PC_SurfaceTextRows(&dst)[y].right==0);
  /* Flipped, clipped copies must transform text coordinates with the pixels. */
  PC_SetClipRect(&dst,&(PC_Rect){25,0,10,200});
  target=(PC_Rect){10,20,0,0}; region=(PC_Rect){90,50,40,8};
  assert(PC_BlitSurface(&src,&region,&dst,&target,PC_BLIT_COPY,true,0));
  assert(PC_SurfaceTextRows(&dst)[20].left==25 && PC_SurfaceTextRows(&dst)[20].right==35);
  PC_SetClipRect(&dst,NULL);
  /* Sharp text, filtered neighbours and the one-row filtering halo. */
  PC_VideoFilter filter={true,PC_SurfaceTextRows(&src),0};
  PC_UpdateTextureScaledY(&(PC_Rect){0,40,320,240},&source_frame[0][0],640,200,&filter);
  for(unsigned y=0;y<240;y++) for(unsigned x=0;x<320;x++) {
    unsigned a=y*200/240,b=((y+1)*200-1)/240,near=a;
    bool text=x>=100 && x<120 && ((a>=50 && a<58)||(b>=50 && b<58)||(near>=50 && near<58));
    assert(display[(y+40)*320+x]==(text?source_frame[near][x]:reference_scaled(&source_frame[0][0],640,x,y)));
  }
  /* A narrow title update retains global text coordinates. */
  filter.source_x=105;
  PC_UpdateTextureScaledY(&(PC_Rect){105,40,2,240},&source_frame[0][105],640,200,&filter);
  for(unsigned y=62;y<68;y++)assert(display[(y+40)*320+105]==source_frame[y*200/240][105]);
  filter.source_x=0; filter.enabled=false;
  PC_UpdateTextureScaledY(&(PC_Rect){0,40,320,240},&source_frame[0][0],640,200,&filter);
  for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++)
    assert(display[(y+40)*320+x]==source_frame[y*200/240][x]);
  PC_TrackTextSurface(0,NULL); PC_TrackTextSurface(1,NULL);
  /* Switching to 16:10 must clear rows left over from 4:3. */
  PC_RenderFillRect(NULL);
  before=windows;
  PC_UpdateTextureScaledY(&(PC_Rect){0,60,320,200}, &source_frame[0][0],640,200,NULL);
  assert(windows==before+1);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x]==(y>=60 && y<260 ? source_frame[y-60][x] : 0));
  /* Wipe presents only two changed columns with full-frame source pitch. */
  for(unsigned y=0;y<200;y++) {source_frame[y][158]=0xa55a;source_frame[y][159]=0x5aa5;}
  PC_UpdateTextureScaledY(&(PC_Rect){158,60,2,200}, &source_frame[0][158],640,200,NULL);
  for(unsigned y=0;y<320;y++) for(unsigned x=0;x<320;x++)
    assert(display[y*320+x]==(y>=60 && y<260 ? source_frame[y-60][x] : 0));
  assert(cs && frame_bits == 8);
  puts("PASS: RGB565 65536 colors, 16-bit DMA, CS/DC setup, SPI drain, tiny "
       "windows, text, clipping/pitch, filtered/sharp 4:3, text metadata, exact 16:10, edges and partial updates");
}
