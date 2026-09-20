/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common.h"
#include "game_assets.h"
#include "game_port.h"
#include "pop_game_hooks.h"
#include <strings.h>
#include <time.h>

rgb_type palette[256];
static PC_Surface mirrored;
/* Keep the two large frames out of the small-object arena. Otherwise a
   320x192 -> 320x200 cutscene transition can fail from fragmentation. */
static uint16_t frame_pixels[2][320 * 200];
static bool frame_used[2];
static bool audio_started;
static PC_Blob sound_blobs[58];
static PC_Blob bank;
static uint32_t sound_fence;
static unsigned sound_mask;
static Uint64 timer_due;
static Uint32 (*timer_fn)(Uint32, void *);
static void *timer_arg;

void game_fatal(const char *why) {
  fprintf(stderr, "FATAL: %s (heap %zu / peak %zu)\n", why, game_heap_used(),
          game_heap_peak());
#ifdef PC_GAME_HOST
  exit(1);
#else
  PC_SetRenderDrawColor(0, 0, 0);
  PC_RenderFillRect(NULL);
  PC_SetRenderDrawColor(255, 100, 100);
  PC_DrawText(0, 20, why);
  for (;;)
    PC_Delay(1000);
#endif
}
void quit(int code) {
  (void)code;
  game_fatal("GAME STOPPED");
}
void sdlperror(const char *s) { game_fatal(s); }
void restore_stuff(void) { PC_StopAudio(); }
word prandom(word max) {
  if (!seed_was_init) {
    random_seed = (dword)PC_GetPerformanceCounter();
    seed_was_init = 1;
  }
  random_seed = random_seed * 214013u + 2531011u;
  return (random_seed >> 16) % (max + 1u);
}

static const POP_Asset *directory_asset(dat_type *d, int id, const char *ext) {
  char name[320], base[POP_MAX_PATH];
  snprintf(base, sizeof base, "%s", d->filename);
  char *dot = strrchr(base, '.');
  if (dot)
    *dot = 0;
  snprintf(name, sizeof name, "%s/res%d.%s", base, id, ext);
  return POP_FindAsset(pop_assets, pop_asset_count, name);
}
static PC_Blob find_blob(int id, const char *ext, data_location *where) {
  for (dat_type *d = dat_chain_ptr; d; d = d->next_dat) {
    const POP_Asset *a =
        POP_FindAsset(pop_assets, pop_asset_count, d->filename);
    PC_Blob b;
    if (a && POP_FindDATResource(a->blob, id, &b)) {
      if (where)
        *where = data_DAT;
      return b;
    }
    a = directory_asset(d, id, ext);
    if (a && a->blob.data) {
      if (where)
        *where = data_directory;
      return a->blob;
    }
  }
  if (where)
    *where = data_none;
  return (PC_Blob){0};
}
dat_type *open_dat(const char *name, int optional) {
  (void)optional;
  dat_type *d = calloc(1, sizeof *d);
  snprintf(d->filename, sizeof d->filename, "%s", name);
  d->next_dat = dat_chain_ptr;
  dat_chain_ptr = d;
  return d;
}
void close_dat(dat_type *d) {
  dat_type **p = &dat_chain_ptr;
  while (*p && *p != d)
    p = &(*p)->next_dat;
  if (*p) {
    *p = d->next_dat;
    free(d);
  }
}
void *load_from_opendats_alloc(int id, const char *ext, data_location *where,
                               int *size) {
  PC_Blob b = find_blob(id, ext, where);
  if (size)
    *size = (int)b.size;
  if (!b.data)
    return NULL;
  void *p = malloc(b.size);
  memcpy(p, b.data, b.size);
  return p;
}
int load_from_opendats_to_area(int id, void *dest, int size, const char *ext) {
  PC_Blob b = find_blob(id, ext, NULL);
  if (!b.data) {
    fprintf(stderr, "Missing resource %d.%s\n", id, ext);
    game_fatal("MISSING RESOURCE");
  }
  memcpy(dest, b.data, MIN(b.size, (size_t)size));
  return 0;
}
static PC_Surface *new_surface(int w, int h, PC_PixelFormat fmt) {
  if (w <= 0 || h <= 0 || w > 320 || h > 320)
    game_fatal("BAD SURFACE SIZE");
  PC_Surface *s = calloc(1, sizeof *s);
  unsigned pitch = (unsigned)w * (fmt == PC_RGB565 ? 2 : 1);
  void *p = calloc(h, pitch);
  if (!PC_InitSurface(s, w, h, pitch, fmt, p, (size_t)h * pitch, true))
    game_fatal("SURFACE INIT");
  return s;
}
static PC_Surface *new_frame(int w, int h) {
  if (w != 320 || h <= 0 || h > 200)
    game_fatal("BAD FRAME SIZE");
  for (unsigned i = 0; i < 2; i++)
    if (!frame_used[i]) {
      frame_used[i] = true;
      memset(frame_pixels[i], 0, sizeof frame_pixels[i]);
      PC_Surface *s = malloc(sizeof *s);
      if (!PC_InitSurface(s, w, h, 640, PC_RGB565, frame_pixels[i],
                          sizeof frame_pixels[i], true))
        game_fatal("FRAME INIT");
      return s;
    }
  game_fatal("TOO MANY FRAMES");
  return NULL;
}
void SDL_FreeSurface(SDL_Surface *s) {
  if (!s || s == &mirrored)
    return;
  bool frame = false;
  for (unsigned i = 0; i < 2; i++)
    if (s->pixels == frame_pixels[i]) {
      frame_used[i] = false;
      frame = true;
    }
  if (s->writable && !frame)
    free((void *)s->pixels);
  free((void *)s->palette);
  free(s);
}
void free_surface(surface_type *s) { SDL_FreeSurface(s); }
PC_Surface *game_mirror(PC_Surface *s) {
  mirrored = *s;
  return &mirrored;
}
int SDL_SetPaletteColors(SDL_Surface *s, const SDL_Color *c, int first, int n) {
  if (!s || !s->palette || first < 0 || n < 0 ||
      (unsigned)(first + n) > s->palette_count)
    return -1;
  uint16_t *p = (uint16_t *)s->palette;
  for (int i = 0; i < n; i++)
    p[first + i] = PC_MapRGB(c[i].r, c[i].g, c[i].b);
  return 0;
}
image_type *load_image(int id, dat_pal_type *pal) {
  (void)pal;
  for (dat_type *d = dat_chain_ptr; d; d = d->next_dat) {
    const POP_Asset *a = directory_asset(d, id, "png");
    if (a && a->image) {
      PC_Surface *s = malloc(sizeof *s);
      *s = *a->image;
      uint16_t *p = calloc(MAX(s->palette_count, 16u), sizeof *p);
      memcpy(p, s->palette, s->palette_count * 2);
      s->palette = p;
      s->palette_count = MAX(s->palette_count, 16u);
      p[0] = 0;
      return s;
    }
  }
  return NULL; /* optional, unused sprite IDs legitimately have no image */
}
image_type *decode_image(image_data_type *raw, dat_pal_type *pal) {
  int h = SDL_SwapLE16(raw->height), w = SDL_SwapLE16(raw->width),
      flags = SDL_SwapLE16(raw->flags);
  if (!h || !w)
    return NULL;
  int stride = calc_stride(raw);
  byte *buf = calloc(h, stride);
  decompr_img(buf, raw, h * stride, (flags >> 8) & 15, stride);
  byte *expanded = conv_to_8bpp(buf, w, h, stride, ((flags >> 12) & 7) + 1);
  free(buf);
  PC_Surface *s = new_surface(w, h, PC_INDEX8);
  memcpy((void *)s->pixels, expanded, w * h);
  free(expanded);
  uint16_t *p = calloc(16, 2);
  for (int i = 1; i < 16; i++)
    p[i] =
        PC_MapRGB(pal->vga[i].r << 2, pal->vga[i].g << 2, pal->vga[i].b << 2);
  s->palette = p;
  s->palette_count = 16;
  s->color_key = 0;
  return s;
}
surface_type *make_offscreen_buffer(const rect_type *r) {
  return new_frame(r->right, r->bottom);
}
void set_gr_mode(byte mode) {
  (void)mode;
  onscreen_surface_ = new_frame(320, 200);
  graphics_mode = gmMcgaVga;
  load_font();
}
static POP_Rect rect(const rect_type *r) {
  return (POP_Rect){r->top, r->left, r->bottom, r->right};
}
static uint16_t color565(byte i) {
  return PC_MapRGB(palette[i].r << 2, palette[i].g << 2, palette[i].b << 2);
}
void method_1_blit_rect(surface_type *t, surface_type *s, const rect_type *tr,
                        const rect_type *sr, int blit) {
  if (!POP_CopyRect(t, s, rect(tr), rect(sr), blit != 0))
    game_fatal("COPY RECT");
}
image_type *method_3_blit_mono(image_type *s, int x, int y, int b, byte c) {
  (void)b;
  if (s && !POP_DrawImage(current_target_surface, s, x, y, 0x40, color565(c),
                          s == &mirrored))
    game_fatal("MONO BLIT");
  return s;
}
const rect_type *method_5_rect(const rect_type *r, int b, byte c) {
  (void)b;
  PC_Rect p = POP_ToRect(rect(r));
  if (!PC_FillRect(current_target_surface, &p, color565(c)))
    game_fatal("FILL RECT");
  return r;
}
image_type *method_6_blit_img_to_scr(image_type *s, int x, int y, int blit) {
  if (s && !POP_DrawImage(current_target_surface, s, x, y, blit,
                          color565(blit & 15), s == &mirrored)) {
    fprintf(stderr, "blit=%d\n", blit);
    game_fatal("IMAGE BLIT");
  }
  return s;
}
void rect_to_sdlrect(const rect_type *r, SDL_Rect *p) {
  *p = POP_ToRect(rect(r));
}
peel_type *read_peel_from_screen(const rect_type *r) {
  peel_type *p = calloc(1, sizeof *p);
  p->rect = *r;
  p->peel = new_surface(r->right - r->left, r->bottom - r->top, PC_RGB565);
  rect_type target = {0, 0, r->bottom - r->top, r->right - r->left};
  method_1_blit_rect(p->peel, current_target_surface, &target, r, 0);
  return p;
}
void set_chtab_palette(chtab_type *t, byte *c, int n) {
  if (!t)
    return;
  for (int i = 0; i < t->n_images; i++) {
    PC_Surface *s = t->images[i];
    if (!s)
      continue;
    uint16_t *p = (uint16_t *)s->palette;
    for (int j = 0; j < MIN(n, (int)s->palette_count); j++)
      p[j] = j ? PC_MapRGB(c[j * 3] << 2, c[j * 3 + 1] << 2, c[j * 3 + 2] << 2)
               : 0;
  }
}
void set_bg_attr(int index, int c) {
  if (index || !enable_flash || !offscreen_surface)
    return;
  /* Gameplay offscreen is 192 rows; the persistent HP strip below it must
     survive both the damage flash and the subsequent black restore. */
  PC_Rect area = {0, 0, offscreen_surface->w, offscreen_surface->h};
  PC_FillRect(onscreen_surface_, &area, color565(c));
  PC_Surface source = *offscreen_surface;
  source.color_key = 0;
  if (upside_down)
    flip_screen(offscreen_surface);
  if (!PC_BlitSurface(&source, NULL, onscreen_surface_, NULL, PC_BLIT_COPY,
                      false, 0))
    game_fatal("FLASH BLIT");
  if (upside_down)
    flip_screen(offscreen_surface);
}
static bool widescreen; /* Default 4:3, F1 toggles 320x200 (16:10). */
static bool aspect_dirty;
static int present_left, present_right = 320;
void game_present_columns(int left, int right) {
  if (left >= 0 && right > left && right <= 320) {
    present_left = left;
    present_right = right;
  }
}
void update_screen(void) {
  if (onscreen_surface_) {
    if (aspect_dirty) {
      PC_SetRenderDrawColor(0, 0, 0);
      PC_RenderFillRect(NULL);
      aspect_dirty = false;
      present_left = 0;
      present_right = 320;
    }
    PC_Rect dst = {present_left, widescreen ? 60 : 40, present_right - present_left,
                   widescreen ? 200 : 240};
    PC_UpdateTextureScaledY(&dst, (const uint16_t *)onscreen_surface_->pixels + present_left,
                            onscreen_surface_->pitch, 200);
    PC_RenderPresent();
    present_left = 0;
    present_right = 320;
  }
#ifdef PC_GAME_HOST
  game_host_frame();
#endif
}
SDL_Surface *get_final_surface(void) { return onscreen_surface_; }
int set_joy_mode(void) {
  is_joyst_mode = 0;
  is_keyboard_mode = 1;
  return 0;
}
/* Free keys, translated into the original megahit commands. Physical modifier
   shortcuts still work; movement aliases cannot accidentally trigger Shift+I. */
static int cheat_command(unsigned scan) {
  switch(scan) {
    case SDL_SCANCODE_V:return SDL_SCANCODE_C|WITH_SHIFT;
    case 45:return SDL_SCANCODE_KP_MINUS;
    case 46:return SDL_SCANCODE_KP_PLUS;
    case SDL_SCANCODE_G:return SDL_SCANCODE_I|WITH_SHIFT;
    case SDL_SCANCODE_W:return SDL_SCANCODE_W|WITH_SHIFT;
    case SDL_SCANCODE_B:return SDL_SCANCODE_B|WITH_CTRL;
    case SDL_SCANCODE_D:return SDL_SCANCODE_B|WITH_SHIFT;
    case SDL_SCANCODE_S:return SDL_SCANCODE_S|WITH_SHIFT;
    case SDL_SCANCODE_T:return SDL_SCANCODE_T|WITH_SHIFT;
    case SDL_SCANCODE_L:return SDL_SCANCODE_L|WITH_SHIFT;
    default:return (int)scan;
  }
}
void process_events(void) {
  PC_PumpEvents();
  PC_Event e;
  while (PC_InputNext(&e)) {
    if (e.scancode == SDL_SCANCODE_F1) {
      if (e.type == PC_KEYDOWN && !e.repeat) {
        widescreen = !widescreen;
        aspect_dirty = true;
      }
      continue;
    }
    if(e.scancode==SDL_SCANCODE_F3) {
      if(e.type==PC_KEYDOWN && !e.repeat) {
        cheats_enabled=!cheats_enabled;
        if(current_level>=1 && current_level<=14 && !is_cutscene) {
          display_text_bottom(cheats_enabled ? "MEGAHIT ON" : "MEGAHIT OFF");
          text_time_total=text_time_remaining=24;
        }
      }
      continue;
    }
    POP_ApplyKeyEvent(&e, key_states, sizeof key_states, &last_key_scancode,
                      &last_any_key_scancode);
    if(e.type==PC_KEYDOWN && !e.repeat && !(e.modifiers & (0xc0|0x300))) {
      if(e.scancode==SDL_SCANCODE_I || e.scancode==SDL_SCANCODE_O || e.scancode==SDL_SCANCODE_P)
        last_key_scancode=e.scancode;
      else if(cheats_enabled && e.scancode<224 && e.scancode!=57)
        last_key_scancode=cheat_command(e.scancode);
    }
    if (e.type == PC_KEYDOWN && !e.repeat && e.key >= 32 && e.key < 127) {
      /* Text uses the modifiers captured at key-down, not the later held state.
         Keep scancodes unchanged for movement and cheat shortcuts. */
      last_text_input = e.key;
      if ((e.modifiers & 3) && e.key >= 'a' && e.key <= 'z')
        last_text_input = e.key - 'a' + 'A';
    }
  }
  /* Title/cutscene waits do not redraw each tick. Make F1 visible there too. */
  if (aspect_dirty && onscreen_surface_)
    update_screen();
  if (timer_fn && PC_GetPerformanceCounter() >= timer_due) {
    Uint32 (*fn)(Uint32, void *) = timer_fn;
    timer_fn = NULL;
    fn(0, timer_arg);
  }
}
SDL_TimerID SDL_AddTimer(Uint32 ms, Uint32 (*fn)(Uint32, void *), void *data) {
  timer_fn = fn;
  timer_arg = data;
  timer_due = PC_GetPerformanceCounter() + (Uint64)ms * 1000;
  return 1;
}
void toggle_fullscreen(void) {}
void load_global_options(void) {
  /* Upstream already ORs configurable controls with the original arrows and
     both Shift keys. This preserves overlapping physical/alternative holds. */
  key_action = SDL_SCANCODE_1;
  key_left = SDL_SCANCODE_I;
  key_down = SDL_SCANCODE_O;
  key_right = SDL_SCANCODE_P;
  key_up = SDL_SCANCODE_9;
  key_jump_left = SDL_SCANCODE_8;
  key_jump_right = SDL_SCANCODE_0;
  custom_saved = custom_defaults;
  memset(&fixes_saved, 1, sizeof fixes_saved);
  fixes = &fixes_saved;
  custom = &custom_saved;
  custom->saving_allowed_first_level=1;
  custom->saving_allowed_last_level=14;
  enable_copyprot = 0;
  enable_controller_rumble = 0;
  enable_quicksave = 0;
  enable_replay = 0;
}
void load_mod_options(void) {}
void check_mod_param(void) {}
void load_sound_names(void) {}
void init_digi(void) {
  if (audio_started)
    return;
  PC_MixerInit();
  PC_AudioSpec s = {PC_AUDIO_RATE, PC_AUDIO_FRAMES, PC_MixAudio, NULL};
  if (!PC_OpenAudioDevice(&s))
    game_fatal("AUDIO INIT");
  audio_started = true;
  const POP_Asset *a =
      POP_FindAsset(pop_assets, pop_asset_count, "PRINCE.DAT");
  if (!a || !POP_FindDATResource(a->blob,1,&bank))
    game_fatal("MISSING BANK");
}
sound_buffer_type *load_sound(int index) {
  init_digi();
  if (index < 0 || index >= 58)
    return NULL;
  PC_Blob b = find_blob(index + 10000, "bin", NULL);
  if (b.data && (b.data[0] & 7) != 0) {
    sound_blobs[index] = b;
    return (sound_buffer_type *)b.data;
  }
  return NULL;
}
void free_sound(sound_buffer_type *b) {
  (void)b;
} /* const Flash, whole-program lifetime */
void stop_sounds(void) {
  if (audio_started) {
    while (!PC_StopAudio())
      PC_Delay(1);
  }
  sound_mask = 0;
  sound_fence = 0;
}
void stop_midi(void) {
  while (!PC_StopMIDI())
    PC_Delay(1);
}
void stop_digi(void) {
  while (!PC_StopWAV(0))
    PC_Delay(1);
  while (!PC_StopWAV(1))
    PC_Delay(1);
}
void turn_music_on_off(byte state) {
  enable_music = state;
  if (!state)
    stop_midi();
}
void turn_sound_on_off(byte state) {
  is_sound_on = state;
  if (!state)
    stop_sounds();
}
void play_sound_from_buffer(sound_buffer_type *b) {
  if (!b || !is_sound_on)
    return;
  for (unsigned i = 0; i < 58; i++)
    if (sound_blobs[i].data == (const uint8_t *)b) {
      if ((b->type & 7) == 2 && !enable_music)
        return;
      if (!POP_PlaySoundResource(sound_blobs[i], bank, 0, i, 0))
        game_fatal("SOUND DATA/QUEUE");
      sound_mask = (b->type & 7) == 2 ? 1 : 2;
      while (!PC_AudioFence(&sound_fence))
        PC_Delay(1);
      return;
    }
}
int check_sound_playing(void) {
  if (!sound_mask)
    return 0;
  if (!PC_AudioFenceComplete(sound_fence))
    return 1;
  return sound_mask == 1 ? PC_MusicSequencing()
                         : !!(PC_MixerActive() & sound_mask);
}
dat_type *dat_chain_ptr;
int last_text_input;
