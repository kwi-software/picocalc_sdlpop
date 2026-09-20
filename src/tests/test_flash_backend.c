#include "pc_store.h"
#include "pico/bootrom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t chip[4*1024*1024];
static unsigned relocated=0x2000, operations, begins, ends;
static bool locked, parked, interrupts_off, deny_lock;
static int fail_operation=-1;
static unsigned flushes;
void rom_flash_flush_cache(void) {assert(parked && interrupts_off);flushes++;}
static int operation(cflash_flags_t flags,uintptr_t address,uint32_t bytes,uint8_t *data) {
    assert(locked && parked && interrupts_off);
    assert((flags.flags&0xffff)==0x101); /* RUNTIME address space, SECURE permission */
    assert(address>=0x101fe000 && address+bytes<=0x10200000);
    unsigned physical=(unsigned)(address-0x10000000)+relocated;
    unsigned op=flags.flags>>16;
    operations++;
    if((int)op==fail_operation)return -4;
    if(op==0) {assert(bytes==4096 && !data && physical%4096==0);memset(chip+physical,255,bytes);}
    else {
        assert(op==1 && bytes==256 && data && (uintptr_t)data%4==0);
        for(unsigned i=0;i<bytes;i++)chip[physical+i]&=data[i];
    }
    return 0;
}
void *rom_func_lookup_inline(unsigned key) {assert(key==ROM_FUNC_FLASH_OP);return (void *)operation;}
bool bootrom_try_acquire_lock(unsigned n) {assert(n==BOOTROM_LOCK_FLASH_OP);if(deny_lock)return false;assert(!locked);return locked=true;}
void bootrom_release_lock(unsigned n) {assert(n==BOOTROM_LOCK_FLASH_OP && locked && !parked);locked=false;}
void PC_AudioFlashBegin(void) {assert(locked && !parked);parked=true;begins++;}
void PC_AudioFlashEnd(void) {assert(!interrupts_off && parked);parked=false;ends++;}
uint32_t save_and_disable_interrupts(void) {assert(parked);interrupts_off=true;return 123;}
void restore_interrupts(uint32_t state) {assert(state==123);interrupts_off=false;}
int main(void) {
    _Alignas(4) uint8_t page[256];memset(page,0x5a,sizeof page);
    assert((uintptr_t)pc_store_flash()==0x101fe000);
    for(unsigned mode=0;mode<2;mode++) {
        relocated=mode?0x2000:0;memset(chip,0xa5,sizeof chip);
        assert(pc_store_program(0,page,true));
        assert(!memcmp(chip+0x1fe000+relocated,page,256));
        assert(chip[0]==0xa5 && chip[0x1fff]==0xa5); /* Loader remains untouched. */
        assert(pc_store_program(4096,page,true));
        assert(!memcmp(chip+0x1ff000+relocated,page,256));
        if(mode)assert(chip[0x1fe000]==0xa5); /* Raw/untranslated address not written. */
    }
    unsigned count=operations;
    assert(!pc_store_program(1,page,false));assert(!pc_store_program(8192,page,false));
    assert(!pc_store_program(256,page,true));assert(!pc_store_program(0,NULL,false));
    assert(operations==count);
    fail_operation=0;assert(!pc_store_program(0,page,true));assert(operations==count+1);
    fail_operation=1;assert(!pc_store_program(0,page,false));
    assert(!locked && !parked && !interrupts_off && begins==ends && flushes==ends);
    deny_lock=true;count=begins;assert(!pc_store_program(0,page,true));assert(begins==count);
    puts("PASS: direct and relocated flash, runtime flags, both journal sectors, error cleanup, bounds and loader preservation");
}
