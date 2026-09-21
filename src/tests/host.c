#include "pc_video_scale.h"
#include "pc_status.h"
#include "common.h"
#include "game_port.h"
#include "keyboard_matrix.h"
#include "pc_store.h"
#include <assert.h>
#include <stdio.h>
static uint64_t now;
static bool audio;
static unsigned frames, limit = 400;
static bool scripted;
static int expected_level = -1;
static unsigned game_frames;
static const char *capture;
static const char *scenario;
static bool pending_release;
static unsigned since_level;
static unsigned aspect_clears, wide_frames, classic_frames;
static unsigned filter_stage;
static bool health_tested;
static bool persistence_tested, cheats_tested;
static void verify_cheats(void);
static unsigned partial_frames;
static uint16_t lcd[320*320];
static int seen_level = -1;
static unsigned status_reads,status_texts,status_clears,backlight_writes;
static int status_values[3]={75,180,0};
int PC_ReadStatusValue(PC_StatusValue field) {status_reads++;return status_values[field];}
int PC_WriteBacklight(PC_StatusValue field,uint8_t value) {
  assert(field==PC_STATUS_LCD || field==PC_STATUS_KEYBOARD);
  if(field==PC_STATUS_LCD){value=(value/16)*16;if(value<16)value=16;if(value>240)value=240;}
  else {value=(value/32)*32;if(value>240)value=0;}
  backlight_writes++;status_values[field]=value;return value;
}
void PC_Init(void) { PC_InputReset(); }
void PC_PumpEvents(void) {
  if (pending_release) {
    PC_InputFeed(0xa2, 3);
    PC_InputFeed('l', 3);
    pending_release = false;
  }

  static unsigned step, control_step;
  if (scenario && !strcmp(scenario, "aspect")) {
    static const unsigned times[] = {1000, 1100, 1200, 3000, 3100};
    static const unsigned states[] = {1, 1, 3, 1, 3};
    while (control_step < 5 && now / 1000 >= times[control_step]) {
      PC_InputFeed(0x81, states[control_step]);
      control_step++;
    }
  }
  if (scenario && !strcmp(scenario,"filter")) {
    static const struct {unsigned ms; uint8_t key,state;} keys[]={
      {1000,0xa5,1},{1000,'f',1},{1100,'f',1},{1200,0xa5,3},{1200,'f',3},
      {2000,0x81,1},{2000,0x81,3},
      {3000,0xa5,1},{3000,'f',1},{3100,'f',3},{3100,0xa5,3},
      {4000,0x81,1},{4000,0x81,3},
      {5000,0xa5,1},{5000,'f',1},{5100,'f',1},{5200,'f',3},{5200,0xa5,3}};
    while(control_step<sizeof keys/sizeof keys[0] && now/1000>=keys[control_step].ms) {
      PC_InputFeed(keys[control_step].key,keys[control_step].state);control_step++;
    }
  }
  if (scenario && !strcmp(scenario,"status")) {
    static const struct {unsigned ms;uint8_t key,state;} keys[]={
      {1000,9,1},{1100,9,1},{1200,9,3},
      {2000,9,1},{2000,9,3},{2500,0x81,1},{2500,0x81,3},
      {3000,9,1},{3000,9,3},{3500,0x81,1},{3500,0x81,3}};
    while(control_step<sizeof keys/sizeof keys[0] && now/1000>=keys[control_step].ms) {
      PC_InputFeed(keys[control_step].key,keys[control_step].state);control_step++;
    }
  }
  if (scenario && !strcmp(scenario,"brightness")) {
    static const struct {unsigned ms;uint8_t key,state;} keys[]={
      {1000,']',1},{1100,']',1},{1200,']',3},{2000,'[',1},{2000,'[',3},
      {2500,0xa1,1},{2500,'[',1},{2500,0xa1,3},{2500,'[',3},
      {3000,0xa1,1},{3000,']',1},{3000,']',3},{3000,0xa1,3},
      {3500,9,1},{3500,9,3},{4000,9,1},{4000,9,3},
      {4500,']',1},{4500,']',3},{5000,'[',1},{5000,'[',3},
      {5500,9,1},{5500,9,3}};
    while(control_step<sizeof keys/sizeof keys[0] && now/1000>=keys[control_step].ms) {
      PC_InputFeed(keys[control_step].key,keys[control_step].state);control_step++;
    }
  }
  if (scenario && !strcmp(scenario, "load") && !control_step && now >= 3000000) {
    PC_InputFeed(0x84, 1); PC_InputFeed(0x84, 3); control_step++;
  }
  if (scenario && (!strcmp(scenario, "start") || !strcmp(scenario, "cheats")) && !control_step &&
      now >= 3000000) {
    PC_InputFeed(13, 1);
    PC_InputFeed(13, 3);
    control_step++;
  }
  if (scenario && !strcmp(scenario, "restart") && control_step < 2 &&
      now >= 5000000 + (uint64_t)control_step * 10000000) {
    PC_InputFeed(0xa5, 1);
    PC_InputFeed('a', 1);
    PC_InputFeed('a', 3);
    PC_InputFeed(0xa5, 3);
    control_step++;
  }

  static const struct {
    unsigned ms;
    uint8_t key, state;
  } events[] = {{1000, 0xb7, 1}, {2400, 0xb7, 3}, {2600, 0xb4, 1},
                {3000, 0xb4, 3}, {3200, 0xa2, 1}, {3201, 0xb7, 1},
                {4800, 0xb7, 3}, {4801, 0xa2, 3}, {5000, 0xb5, 1},
                {5500, 0xb5, 3}, {6000, 0xb6, 1}, {6400, 0xb6, 3}};
  if (scripted)
    while (step < sizeof events / sizeof events[0] &&
           now / 1000 >= events[step].ms) {
      PC_InputFeed(events[step].key, events[step].state);
      step++;
    }
}
uint32_t PC_GetTicks(void) { return (uint32_t)(now / 1000); }
uint64_t PC_GetPerformanceCounter(void) { return now; }
void PC_Delay(unsigned ms) {
  now += (uint64_t)ms * 1000;
  if (now > 240000000) {
    fprintf(stderr, "Host watchdog expired\n");
    exit(2);
  }
  if (audio) {
    int16_t buf[256];
    unsigned n = ms * 24;
    while (n) {
      unsigned c = n > 256 ? 256 : n;
      PC_MixAudio(NULL, buf, c);
      n -= c;
    }
  }
}
bool PC_OpenAudioDevice(const PC_AudioSpec *s) {
  (void)s;
  audio = true;
  return true;
}
void PC_UpdateTexture(const PC_Rect *r, const uint16_t *p, unsigned pitch) {
  (void)r;
  (void)p;
  (void)pitch;
}
void PC_SetRenderDrawColor(uint8_t r, uint8_t g, uint8_t b) {
  (void)r;
  (void)g;
  (void)b;
}
void PC_RenderFillRect(const PC_Rect *r) {
  if(r) {
    assert(scenario && (!strcmp(scenario,"status") || !strcmp(scenario,"brightness")));
    assert(r->y>=0 && r->y+r->h<=26);
    if(r->w==320)status_clears++;
    return;
  } /* A full-panel clear is reserved for aspect/filter changes. */
  aspect_clears++;
  memset(lcd,0,sizeof lcd);
}
void PC_DrawText(int x,int y,const char *text) {
  (void)x;(void)y;(void)text;assert(!"Unexpected diagnostic text");
}
void PC_DrawTextSmall(int x, int y, const char *text) {
  (void)x;
  (void)y;
  (void)text;
  assert(scenario && (!strcmp(scenario,"status") || !strcmp(scenario,"brightness")) && y==7);
  status_texts++;
}
void PC_UpdateTextureScaledY(const PC_Rect *r, const uint16_t *p,
                             unsigned pitch, unsigned h, const PC_VideoFilter *filter) {
  assert(r->x >= 0 && r->w > 0 && r->x + r->w <= 320);
  if(r->w < 320) partial_frames++;
  if (r->h == 240) {
    assert(r->y == 40);
    classic_frames++;
  } else {
    assert(r->h == 200 && r->y == 60);
    wide_frames++;
  }
  assert(p && pitch == 640 && h == 200);
  if(scenario && !strcmp(scenario,"filter")) {
    static const unsigned heights[]={240,240,200,240,240};
    static const bool enabled[]={false,true,true,true,false};
    assert(filter && filter_stage<5);
    if(r->h!= (int)heights[filter_stage] || filter->enabled!=enabled[filter_stage]) {
      filter_stage++;
      assert(filter_stage<5 && r->h==(int)heights[filter_stage] && filter->enabled==enabled[filter_stage]);
    }
    assert(!key_states[SDL_SCANCODE_F]);
  }

  for(int y=0;y<r->h;y++)
    pc_scale_rgb565_row(lcd+(r->y+y)*320+r->x,p,pitch,r->w,h,r->h,y,filter);
  if(r->w<320) {
    const uint16_t *all=onscreen_surface_->pixels;
    uint16_t expected[320];
    PC_VideoFilter full=filter?*filter:(PC_VideoFilter){true,NULL,0};full.source_x=0;
    for(int y=0;y<r->h;y++) {
      pc_scale_rgb565_row(expected,all,640,320,h,r->h,y,&full);
      assert(!memcmp(lcd+(r->y+y)*320,expected,sizeof expected));
    }
  }
}
void PC_RenderPresent(void) {}
void game_host_frame(void) {
  if (++frames % 100 == 0)
    fprintf(stderr,
            "frame=%u t=%llu level=%d room=%d kid=%u,%u heap=%zu peak=%zu\n",
            frames, (unsigned long long)now, current_level, drawn_room, Kid.x,
            Kid.y, game_heap_used(), game_heap_peak());
  if (scenario && !strcmp(scenario, "levels") && current_level >= 1 &&
      current_level <= 14 && drawn_room) {
    if (seen_level != current_level) {
      seen_level = current_level;
      since_level = 0;
      fprintf(stderr, "VISITED LEVEL %d\n", seen_level);
    }
    if (++since_level == 10 && current_level < 14) {
      PC_InputFeed(0xa2, 1);
      PC_InputFeed('l', 1);
      pending_release = true;
    }
  }
  if (scenario && !strcmp(scenario, "health") && current_level == 1 &&
      drawn_room && !health_tested) {
    assert(offscreen_surface->h == 192);
    uint16_t saved[320 * 8];
    surface_type *target = current_target_surface;
    current_target_surface = onscreen_surface_;
    for (int hp = 2; hp >= 0; hp--) {
      draw_kid_hp(hp, 3);
      uint16_t *hud = (uint16_t *)onscreen_surface_->pixels + 320 * 192;
      memcpy(saved, hud, sizeof saved);
      set_bg_attr(0, 4);
      assert(!memcmp(saved, hud, sizeof saved));
      set_bg_attr(0, 0);
      assert(!memcmp(saved, hud, sizeof saved));
    }
    draw_kid_hp(hitp_curr, hitp_max);
    current_target_surface = target;
    health_tested = true;
  }
  if (scenario && !strcmp(scenario, "cheats") && current_level == 1 && drawn_room && !cheats_tested) {
    verify_cheats(); cheats_tested = true;
  }
  if (scenario && !strcmp(scenario, "save") && current_level == 1 && drawn_room && !persistence_tested) {
    rem_min=45; rem_tick=321; hitp_beg_lev=7; current_level=6;
    PC_InputFeed(0x82,1); PC_InputFeed(0x82,3); process_events(); process_key(); /* F2 */
    uint8_t saved[8], expected[]={45,0,65,1,6,0,7,0};
    assert(PC_StoreRead(PC_STORE_SAVE,saved,8)); assert(!memcmp(saved,expected,8));
    current_level=1;
    uint8_t score[PC_STORE_HOF_SIZE]={1,0};
    memcpy(score+2,"FLASH TEST",11); score[27]=12; score[29]=123;
    assert(PC_StoreWrite(PC_STORE_HOF,score,sizeof score));
    hof_read(); assert(hof_count==1); hof_write();
    assert(PC_StoreRead(PC_STORE_SAVE,saved,8)); assert(!memcmp(saved,expected,8));
    persistence_tested=true;
  }
  if (scenario && !strcmp(scenario, "load") && current_level == 6 && drawn_room && !persistence_tested) {
    assert(rem_min==45 && rem_tick<=321 && rem_tick>300 && hitp_beg_lev==7);
    assert(hof_count==1); /* Loaded by actual game startup in a new process. */
    uint8_t score[PC_STORE_HOF_SIZE]; hof_write();
    assert(PC_StoreRead(PC_STORE_HOF,score,sizeof score));
    assert(score[0]==1 && !strcmp((char *)score+2,"FLASH TEST") && score[27]==12 && score[29]==123);
    persistence_tested=true;
  }
  PC_Delay(14);
  if (current_level == expected_level && drawn_room)
    game_frames++;
  if ((expected_level < 0 && frames == limit) ||
      (expected_level >= 0 && game_frames == limit)) {
    FILE *f = fopen(capture, "wb");
    if (!f)
      exit(3);
    fprintf(f, "P6\n320 200\n255\n");
    const uint16_t *p = onscreen_surface_->pixels;
    for (int i = 0; i < 64000; i++) {
      uint16_t c = p[i];
      fputc(((c >> 11) & 31) * 255 / 31, f);
      fputc(((c >> 5) & 63) * 255 / 63, f);
      fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
    fprintf(
        stderr, "DONE level=%d room=%d frames=%u peak=%zu audio_errors=%u\n",
        current_level, drawn_room, frames, game_heap_peak(), PC_MixerErrors());
    if (scenario && !strcmp(scenario, "aspect"))
      assert(aspect_clears == 2 && wide_frames && classic_frames);
    else if(scenario && !strcmp(scenario,"filter"))
      assert(aspect_clears==4 && filter_stage==4 && current_level==65535);
    else if(scenario && !strcmp(scenario,"status"))
      assert(aspect_clears==2 && status_reads==3 && status_texts==7 && status_clears==4 && current_level==65535);
    else if(scenario && !strcmp(scenario,"brightness")) {
      assert(!aspect_clears && backlight_writes==6 && status_reads==9 && status_texts==10);
      assert(status_values[1]==176 && status_values[2]==0 && current_level==65535);
      assert(!key_states[SDL_SCANCODE_LEFTBRACKET] && !key_states[SDL_SCANCODE_RIGHTBRACKET]);
    }
    else
      assert(aspect_clears == 0 && wide_frames == 0);
    if(frames>=8000) assert(partial_frames>0);
    if (scenario && !strcmp(scenario, "health"))
      assert(health_tested);
    if (scenario && !strcmp(scenario, "aspect"))
      assert(current_level == 65535);
    if (scenario && (!strcmp(scenario,"save") || !strcmp(scenario,"load"))) assert(persistence_tested);
    if (scenario && !strcmp(scenario,"cheats")) assert(cheats_tested);
    exit(PC_MixerErrors() ? 4 : 0);
  }
}
/* Exercise the actual game's control decoder, not only SDL-shaped events. */
static void verify_controls(void) {
  load_global_options();
  uint8_t cols[8];
  memset(cols, 255, sizeof cols);
  for (unsigned shift = 2; shift <= 3; shift++) {
    cols[shift] = 127;
    pc_keyboard_matrix_feed(cols, 254);
    process_events();
    control_x = control_y = 0;
    read_keyb_control();
    assert(control_shift == CONTROL_HELD && control_x == CONTROL_HELD_RIGHT);
    for (unsigned i = 0; i < 512; i++)
      key_states[i] &= ~KEYSTATE_HELD_NEW;
    cols[shift] = 255;
    pc_keyboard_matrix_feed(cols, 255);
    process_events();
    control_x = control_y = 0;
    read_keyb_control();
    assert(control_x == 0 && control_shift == CONTROL_RELEASED);
  }
  static const struct {
    uint8_t key;
    int x, y, shift;
  } aliases[] = {{'1', 0, 0, CONTROL_HELD},
                 {'i', CONTROL_HELD_LEFT, 0, CONTROL_RELEASED},
                 {'o', 0, CONTROL_HELD_DOWN, CONTROL_RELEASED},
                 {'p', CONTROL_HELD_RIGHT, 0, CONTROL_RELEASED},
                 {'9', 0, CONTROL_HELD_UP, CONTROL_RELEASED},
                 {'8', CONTROL_HELD_LEFT, CONTROL_HELD_UP, CONTROL_RELEASED},
                 {'0', CONTROL_HELD_RIGHT, CONTROL_HELD_UP, CONTROL_RELEASED},
                 {0x85, 0, 0, CONTROL_HELD}};
  for (unsigned j = 0; j < sizeof aliases / sizeof aliases[0]; j++) {
    PC_InputFeed(aliases[j].key, 1);
    process_events();
    control_x = control_y = 0;
    read_keyb_control();
    assert(control_x == aliases[j].x && control_y == aliases[j].y &&
           control_shift == aliases[j].shift);
    for (unsigned i = 0; i < 512; i++)
      key_states[i] &= ~KEYSTATE_HELD_NEW;
    PC_InputFeed(aliases[j].key, 3);
    process_events();
    control_x = control_y = 0;
    read_keyb_control();
    assert(!control_x && !control_y && control_shift == CONTROL_RELEASED);
  }
  /* Alternative slow walk, and release one source while the other stays down.
   */
  PC_InputFeed('1', 1);
  PC_InputFeed(0x85, 1);
  PC_InputFeed('p', 1);
  PC_InputFeed(0xb7, 1);
  process_events();
  control_x = control_y = 0;
  read_keyb_control();
  assert(control_x == CONTROL_HELD_RIGHT && control_shift == CONTROL_HELD);
  for (unsigned i = 0; i < 512; i++)
    key_states[i] &= ~KEYSTATE_HELD_NEW;
  PC_InputFeed('p', 3);
  process_events();
  control_x = 0;
  read_keyb_control();
  assert(control_x == CONTROL_HELD_RIGHT && control_shift == CONTROL_HELD);
  PC_InputFeed('1', 3);
  process_events(); control_x=0; read_keyb_control();
  assert(control_shift==CONTROL_HELD && control_x==CONTROL_HELD_RIGHT);
  PC_InputFeed(0x85, 3);
  PC_InputFeed(0xb7, 3);
  process_events();
  memset(key_states, 0, sizeof key_states);
  last_key_scancode = last_any_key_scancode = last_text_input = 0;
  PC_InputReset();
}
static void press(uint8_t key) {
  PC_InputFeed(key,1); PC_InputFeed(key,3); process_events();
}
static void verify_text_case(void) {
  for (uint8_t shift = 0xa2; shift <= 0xa3; ++shift) {
    for (uint8_t letter = 'a'; letter <= 'z'; ++letter) {
      press(letter); assert(last_text_input == letter);
      PC_InputFeed(shift, 1);
      PC_InputFeed(letter, 1);
      PC_InputFeed(letter, 3);
      PC_InputFeed(shift, 3);
      /* All keys are already released when the queued events are processed. */
      process_events(); assert(last_text_input == letter - 'a' + 'A');
      press(letter); assert(last_text_input == letter);
    }
  }
  press('A'); assert(last_text_input == 'A');
  PC_InputFeed(0xa2, 1); press('7'); assert(last_text_input == '7');
  PC_InputFeed(0xa2, 3); process_events();
  memset(key_states, 0, sizeof key_states);
  PC_InputReset();
  last_key_scancode = last_any_key_scancode = last_text_input = 0;
}
static void verify_cheats(void) {
  assert(!cheats_enabled);
  press(0x83); assert(cheats_enabled);
  static const struct {uint8_t key; int command;} mappings[]={
    {'c',SDL_SCANCODE_C},{'v',SDL_SCANCODE_C|WITH_SHIFT},
    {'-',SDL_SCANCODE_KP_MINUS},{'=',SDL_SCANCODE_KP_PLUS},
    {'r',SDL_SCANCODE_R},{'k',SDL_SCANCODE_K},
    {'g',SDL_SCANCODE_I|WITH_SHIFT},{'w',SDL_SCANCODE_W|WITH_SHIFT},
    {'h',SDL_SCANCODE_H},{'j',SDL_SCANCODE_J},{'u',SDL_SCANCODE_U},{'n',SDL_SCANCODE_N},
    {'b',SDL_SCANCODE_B|WITH_CTRL},{'d',SDL_SCANCODE_B|WITH_SHIFT},
    {'s',SDL_SCANCODE_S|WITH_SHIFT},{'t',SDL_SCANCODE_T|WITH_SHIFT},{'l',SDL_SCANCODE_L|WITH_SHIFT}};
  for (unsigned i=0;i<sizeof mappings/sizeof mappings[0];i++) {
    press(mappings[i].key); assert(last_key_scancode==mappings[i].command);
    last_key_scancode=0;
  }
  int minutes=rem_min;
  press('-'); process_key(); assert(rem_min==minutes-1);
  press('='); process_key(); assert(rem_min==minutes);
  PC_InputFeed(0xa2,1); press('i');
  assert(last_key_scancode==SDL_SCANCODE_I); /* Slow walk must never invert screen. */
  PC_InputFeed(0xa2,3); process_events(); last_key_scancode=0;
  PC_InputFeed(0x83,1); process_events(); assert(!cheats_enabled);
  PC_InputFeed(0x83,2); process_events(); assert(!cheats_enabled);
  PC_InputFeed(0x83,3); process_events();
  press('-'); process_key(); assert(rem_min==minutes);
  last_key_scancode=last_any_key_scancode=last_text_input=0;
}
int main(int argc, char **argv) {
  PC_Init();
  verify_controls();
  verify_text_case();
  scenario = getenv("POP_SCENARIO");
  capture = getenv("POP_CAPTURE");
  if (!capture)
    capture = "build-host-game/frame.ppm";
  if (getenv("POP_FRAMES"))
    limit = atoi(getenv("POP_FRAMES"));
  scripted = getenv("POP_INPUT") != NULL;
  if (getenv("POP_LEVEL_TEST"))
    expected_level = atoi(getenv("POP_LEVEL_TEST"));
  g_argc = argc;
  g_argv = argv;
  pop_main();
  return 0;
}
