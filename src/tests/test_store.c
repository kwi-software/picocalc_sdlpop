/* Exercise NOR-flash semantics and power loss at every byte of a commit. */
#include "pc_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[PC_STORE_BYTES];
static int cut = -1;
static unsigned programs, erases;
const uint8_t *pc_store_flash(void) { return flash; }
bool pc_store_program(unsigned offset, const uint8_t page[256], bool erase) {
    assert(offset % 256 == 0 && offset <= sizeof flash - 256);
    programs++;
    if (erase) {
        assert(offset % 4096 == 0);
        erases++;
        memset(flash + offset, 255, 4096);
    }
    unsigned n = cut < 0 ? 256 : (unsigned)cut;
    for (unsigned i = 0; i < n; i++) {
        assert((flash[offset+i] & page[i]) == page[i]);
        flash[offset+i] &= page[i];
    }
    return cut < 0;
}
static void read_save(unsigned expected) {
    uint8_t out[8];
    assert(PC_StoreRead(PC_STORE_SAVE, out, sizeof out));
    for (unsigned i = 0; i < sizeof out; i++) assert(out[i] == expected);
}
int main(void) {
    uint8_t hof[PC_STORE_HOF_SIZE], save[8], out[PC_STORE_HOF_SIZE];
    memset(flash, 255, sizeof flash);
    memset(hof, 0x5a, sizeof hof);
    assert(!PC_StoreRead(PC_STORE_HOF, out, sizeof out));
    assert(!PC_StoreWrite(3, save, 8));
    assert(!PC_StoreWrite(PC_STORE_SAVE, save, 7));
    assert(!PC_StoreRead(PC_STORE_SAVE, NULL, 8));
    assert(PC_StoreWrite(PC_STORE_HOF, hof, sizeof hof));
    unsigned before = programs;
    assert(PC_StoreWrite(PC_STORE_HOF, hof, sizeof hof));
    assert(programs == before); /* Unchanged data does not wear flash. */
    for (unsigned i = 1; i <= 200; i++) {
        memset(save, i, sizeof save);
        assert(PC_StoreWrite(PC_STORE_SAVE, save, sizeof save));
        read_save(i);
        assert(PC_StoreRead(PC_STORE_HOF, out, sizeof out));
        assert(!memcmp(hof, out, sizeof hof));
    }
    assert(erases == 13); /* One sector erase per 16 appended records. */
    /* Simulate power interruption before each of 256 programmed bytes,
       both inside a sector and at the boundary requiring an erase. */
    for (unsigned commits = 1; commits <= 32; commits++) {
        memset(flash, 255, sizeof flash);
        for (unsigned i = 1; i <= commits; i++) {
            memset(save, i, sizeof save);
            assert(PC_StoreWrite(PC_STORE_SAVE, save, 8));
        }
        uint8_t snapshot[PC_STORE_BYTES];
        memcpy(snapshot, flash, sizeof flash);
        for (cut = 0; cut < 256; cut++) {
            memcpy(flash, snapshot, sizeof flash);
            memset(save, 99, sizeof save);
            assert(!PC_StoreWrite(PC_STORE_SAVE, save, 8));
            read_save(commits);
            int previous_cut = cut;
            cut = -1;
            assert(PC_StoreWrite(PC_STORE_SAVE, save, 8));
            read_save(99);
            cut = previous_cut;
        }
        cut = -1;
    }
    /* A corrupt newest record falls back to the previous committed one. */
    memset(flash, 255, sizeof flash);
    memset(save, 1, 8); assert(PC_StoreWrite(PC_STORE_SAVE, save, 8));
    memset(save, 2, 8); assert(PC_StoreWrite(PC_STORE_SAVE, save, 8));
    flash[256+192] ^= 1;
    read_save(1);
    memset(save, 3, 8); assert(PC_StoreWrite(PC_STORE_SAVE, save, 8));
    read_save(3);
    /* Settings share the record without replacing scores or game saves. */
    uint8_t settings[8]={1,1,1,0,1,2,192,32}, small[8];
    assert(PC_StoreWrite(PC_STORE_HOF,hof,sizeof hof));
    assert(PC_StoreWrite(PC_STORE_SETTINGS,settings,8));
    read_save(3);
    assert(PC_StoreRead(PC_STORE_SETTINGS,small,8) && !memcmp(small,settings,8));
    uint8_t snapshot[PC_STORE_BYTES];memcpy(snapshot,flash,sizeof flash);
    for(cut=0;cut<256;cut++) {
        memcpy(flash,snapshot,sizeof flash);
        assert(!PC_StoreReset());
        read_save(3);
        assert(PC_StoreRead(PC_STORE_SETTINGS,small,8) && !memcmp(small,settings,8));
        assert(PC_StoreRead(PC_STORE_HOF,out,sizeof out) && !memcmp(out,hof,sizeof out));
    }
    cut=-1;assert(PC_StoreReset());
    assert(!PC_StoreRead(PC_STORE_SETTINGS,small,8));
    assert(!PC_StoreRead(PC_STORE_SAVE,small,8));
    assert(!PC_StoreRead(PC_STORE_HOF,out,sizeof out));
    assert(PC_StoreWrite(PC_STORE_SETTINGS,settings,8));
    assert(!PC_StoreRead(PC_STORE_SAVE,small,8));
    assert(!PC_StoreRead(PC_STORE_HOF,out,sizeof out));
    memset(flash, 255, sizeof flash); /* Provided UF2 resets both sectors. */
    assert(!PC_StoreRead(PC_STORE_SAVE, save, 8));
    assert(!PC_StoreRead(PC_STORE_HOF, out, sizeof out));
    puts("Flash journal: rollover, CRC, independent keys, deduplication and 8192 interrupted commits passed");
}
