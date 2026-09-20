/* Actual FAT32 + routing code on a sector-backed disk, with injectable failures. */
#include "pc_store_internal.h"
#include "sdcard.h"
#include "fat32.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FILE *disk;
static bool present=true,read_fail,write_fail;
static unsigned flash_writes;
static uint8_t flash[PC_STORE_BYTES];
void sd_init(void) {}
bool sd_card_present(void) {return present;}
sd_error_t sd_card_init(void) {return present?SD_OK:SD_ERROR_NO_CARD;}
sd_error_t sd_read_block(uint32_t block,uint8_t *buf) {
    if(!present || read_fail)return SD_ERROR_READ_FAILED;
    if(fseek(disk,(long)block*512,SEEK_SET))return SD_ERROR_READ_FAILED;
    return fread(buf,1,512,disk)==512?SD_OK:SD_ERROR_READ_FAILED;
}
sd_error_t sd_write_block(uint32_t block,const uint8_t *buf) {
    if(!present || write_fail)return SD_ERROR_WRITE_FAILED;
    if(fseek(disk,(long)block*512,SEEK_SET))return SD_ERROR_WRITE_FAILED;
    bool ok=fwrite(buf,1,512,disk)==512;fflush(disk);return ok?SD_OK:SD_ERROR_WRITE_FAILED;
}
const uint8_t *pc_store_flash(void) {return flash;}
bool pc_store_program(unsigned off,const uint8_t page[256],bool erase) {
    assert(off%256==0 && off<=sizeof flash-256);
    if(erase){assert(off%4096==0);memset(flash+off,255,4096);}
    for(unsigned i=0;i<256;i++)flash[off+i]&=page[i];
    flash_writes++;return true;
}
static void expect(unsigned key,const void *expected,size_t size) {
    uint8_t result[PC_STORE_HOF_SIZE];assert(PC_StoreRead(key,result,size));assert(!memcmp(result,expected,size));
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);disk=fopen(argv[1],"r+b");assert(disk);memset(flash,255,sizeof flash);
    uint8_t score[PC_STORE_HOF_SIZE],save[8],out[8];memset(score,0x42,sizeof score);memset(save,1,sizeof save);
    if(argc==3 && !strcmp(argv[2],"verify")) {
        memset(score,0x43,sizeof score);memset(save,4,8);
        expect(PC_STORE_SAVE,save,8);expect(PC_STORE_HOF,score,sizeof score);
        assert(PC_StoreLastBackend()==PC_STORE_SD && !flash_writes);
        fclose(disk);puts("PASS: SD scores and save survive a new process with empty flash");return 0;
    }
    assert(PC_StoreWrite(PC_STORE_HOF,score,sizeof score));
    if(argc==3){assert(PC_StoreLastBackend()==PC_STORE_FLASH && flash_writes);fclose(disk);puts("PASS: full FAT32 falls back to flash");return 0;}
    assert(PC_StoreLastBackend()==PC_STORE_SD && !flash_writes);
    assert(PC_StoreWrite(PC_STORE_SAVE,save,8));expect(PC_STORE_HOF,score,sizeof score);expect(PC_STORE_SAVE,save,8);
    assert(!flash_writes);
    /* Read-only/unwritable card: newest record goes to flash. */
    write_fail=true;memset(save,2,8);assert(PC_StoreWrite(PC_STORE_SAVE,save,8));assert(PC_StoreLastBackend()==PC_STORE_FLASH && flash_writes==1);
    expect(PC_STORE_SAVE,save,8);assert(PC_StoreLastBackend()==PC_STORE_FLASH);
    present=false;expect(PC_STORE_SAVE,save,8);expect(PC_STORE_HOF,score,sizeof score);
    /* Card comes back: automatically move the newer fallback onto SD. */
    present=true;write_fail=false;expect(PC_STORE_SAVE,save,8);assert(PC_StoreLastBackend()==PC_STORE_SD && flash_writes==1);
    memset(score,0x43,sizeof score);assert(PC_StoreWrite(PC_STORE_HOF,score,sizeof score));
    assert(PC_StoreLastBackend()==PC_STORE_SD && flash_writes==1);
    write_fail=true;memset(save,9,8);assert(PC_StoreWrite(PC_STORE_SAVE,save,8));
    write_fail=false;expect(PC_STORE_SAVE,save,8);expect(PC_STORE_HOF,score,sizeof score);
    memset(flash,255,sizeof flash);fat32_unmount();expect(PC_STORE_SAVE,save,8);expect(PC_STORE_HOF,score,sizeof score);
    /* Read errors must also fall back, and a missing SD must not block saving. */
    read_fail=true;memset(save,3,8);assert(PC_StoreWrite(PC_STORE_SAVE,save,8));assert(PC_StoreLastBackend()==PC_STORE_FLASH);
    read_fail=false;
    /* Recovery must work even if flash was empty during an SD read failure. */
    present=false;expect(PC_STORE_SAVE,save,8);memset(save,4,8);assert(PC_StoreWrite(PC_STORE_SAVE,save,8));
    present=true;
    /* Inspect the on-card files through the real FAT32 implementation. */
    assert(fat32_is_ready());fat32_file_t file;assert(fat32_open(&file,"/Prince/STATE0.BIN")==FAT32_OK);assert(file.file_size==256);fat32_close(&file);
    assert(fat32_open(&file,"/Prince/STATE1.BIN")==FAT32_OK);assert(file.file_size==256);fat32_close(&file);
    assert(PC_StoreRead(PC_STORE_SAVE,out,8));assert(!memcmp(out,save,8));
    expect(PC_STORE_HOF,score,sizeof score);
    assert(PC_StoreLastBackend()==PC_STORE_SD);
    fclose(disk);puts("PASS: FAT32 directory creation across clusters, SD-first scores/save, write/read errors, card removal, flash fallback and recovery");
}
