/* Compile the actual Southbridge driver; replay firmware scan-window snapshots. */
#include "southbridge.h"
#include "keyboard_matrix.h"
#include "pop_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t first[8], second[8], arrows=255, reg;
static unsigned matrix_reads, arrow_reads;
static bool fail_confirm;
int i2c_write_timeout_us(void *bus,unsigned addr,const uint8_t *p,size_t n,bool stop,unsigned timeout) {
    (void)bus;(void)stop;(void)timeout;assert(addr==0x1f && n==1);reg=p[0];return n;
}
int i2c_read_timeout_us(void *bus,unsigned addr,uint8_t *p,size_t n,bool stop,unsigned timeout) {
    (void)bus;(void)addr;(void)stop;(void)timeout;
    if(reg==0x0c) {
        assert(n==10);matrix_reads++;
        if(fail_confirm && matrix_reads==2)return -2;
        p[0]=reg;memcpy(p+1,matrix_reads==1?first:second,8);p[9]=255;
    } else {assert(reg==0x0d && n==2);arrow_reads++;p[0]=reg;p[1]=arrows;}
    return n;
}
unsigned i2c_init(void *bus,unsigned rate){(void)bus;return rate;}
void gpio_set_function(unsigned a,unsigned b){(void)a;(void)b;}
void gpio_pull_up(unsigned p){(void)p;}
void sleep_us(uint64_t n){assert(n==3500);}
static void read_snapshot(unsigned expected_reads) {
    uint8_t out[8], dirs;
    matrix_reads=arrow_reads=0;
    assert(sb_read_keyboard_matrix(out,&dirs));
    assert(matrix_reads==expected_reads);
    pc_keyboard_matrix_feed(out,dirs);
}
int main(void) {
    PC_Event e;PC_InputReset();
    memset(first,255,8);memset(second,255,8);
    /* Each partial scan pattern must leave the title's input queue empty. */
    for(unsigned pattern=1;pattern<256;pattern++) {
        for(unsigned c=0;c<8;c++)first[c]=(pattern&(1u<<c))?127:255;
        read_snapshot(2);assert(!PC_InputNext(&e));
    }
    /* True Shift and its hold/release still work; only new presses cost a reread. */
    memset(first,255,8);first[2]=second[2]=127;
    read_snapshot(2);assert(PC_InputNext(&e)&&e.scancode==225&&e.type==PC_KEYDOWN);
    read_snapshot(1);assert(!PC_InputNext(&e));
    first[2]=second[2]=255;
    read_snapshot(1);assert(PC_InputNext(&e)&&e.type==PC_KEYUP);
    /* Row-seven numeric key plus an ordinary letter, with a real arrow. */
    first[5]=second[5]=127;first[7]=second[7]=251;arrows=254;
    read_snapshot(2);assert(arrow_reads==1);
    assert(PC_GetKeyboardState(NULL)[38] && PC_GetKeyboardState(NULL)[30] && PC_GetKeyboardState(NULL)[79]);
    /* An I2C error during confirmation may not become an accepted snapshot. */
    first[2]=second[2]=127;matrix_reads=0;fail_confirm=true;
    uint8_t out[8], dirs;assert(!sb_read_keyboard_matrix(out,&dirs));
    puts("PASS: 255 partial-scan ghost patterns suppressed, real button press/hold/release, ordinary matrix keys, I2C failure");
}
