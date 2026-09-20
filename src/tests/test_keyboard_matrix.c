#include "pop_port.h"
#include "keyboard_matrix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t game[512];
static int last, any;
static unsigned drain(void) {
    PC_Event e; unsigned n=0;
    while(PC_InputNext(&e)) {POP_ApplyKeyEvent(&e,game,sizeof game,&last,&any);n++;}
    return n;
}
int main(void) {
    uint8_t cols[8]; memset(cols,255,sizeof cols); PC_InputReset();
    pc_keyboard_matrix_feed(cols,255); assert(!drain());
    /* Left shift + right; firmware FIFO drops right under shift. */
    cols[2]&=127;
    pc_keyboard_matrix_feed(cols,254); assert(drain()==2);
    assert(game[225]==3 && game[79]==3 && last==(79|0x8000));
    for(unsigned i=0;i<10000;i++) pc_keyboard_matrix_feed(cols,254);
    assert(!drain() && !PC_InputOverflows());
    /* Release shift first while right remains pressed: no key identity change. */
    cols[2]=255; pc_keyboard_matrix_feed(cols,254); assert(drain()==1);
    assert(!(game[225]&1) && (game[79]&1));
    pc_keyboard_matrix_feed(cols,255); assert(drain()==1);
    assert(!(game[79]&1));
    /* Right shift and both direction axes, plus Ctrl-A. */
    cols[3]&=127; cols[1]&=127; cols[7]&=~(1u<<4);
    pc_keyboard_matrix_feed(cols,0xf5); assert(drain()==5);
    assert((game[229]&1) && (game[224]&1) && (game[4]&1) && (game[82]&1) && (game[80]&1));
    /* A fresh all-up snapshot repairs missed releases, including after death. */
    memset(cols,255,sizeof cols);pc_keyboard_matrix_feed(cols,255);drain();
    for(unsigned i=0;i<512;i++) assert(!(game[i]&1) && !PC_GetKeyboardState(NULL)[i]);
    /* F1 down once, held, release: only two transitions. */
    cols[4]&=~1u;pc_keyboard_matrix_feed(cols,255);assert(drain()==1 && last==58);
    pc_keyboard_matrix_feed(cols,255);assert(!drain());
    cols[4]=255;pc_keyboard_matrix_feed(cols,255);assert(drain()==1);
    puts("PASS: raw matrix Shift/arrows, Ctrl-A, simultaneous keys, held suppression, release recovery, F1");
}
