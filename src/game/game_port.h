#pragma once
#include "pc_surface.h"
PC_Surface *game_mirror(PC_Surface *s);
void game_fatal(const char *why);
void game_host_frame(void);
extern dat_type *dat_chain_ptr;
extern rgb_type palette[256];
extern int last_text_input;
int calc_stride(image_data_type *raw);
void decompr_img(byte *dest, const image_data_type *source, int size,
                 int method, int stride);
byte *conv_to_8bpp(byte *data, int w, int h, int stride, int depth);
void load_font(void);
