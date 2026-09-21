/* SD-first saves; resets always commit a flash tombstone. Core 0 only. */
#include "pc_store_internal.h"
#include <string.h>
static PC_StoreBackend backend;
PC_StoreBackend PC_StoreLastBackend(void) {return backend;}
static size_t length(unsigned key) {return key==PC_STORE_HOF?PC_STORE_HOF_SIZE:key==PC_STORE_SAVE?PC_STORE_SAVE_SIZE:key==PC_STORE_SETTINGS?PC_STORE_SETTINGS_SIZE:0;}
static unsigned location(unsigned key) {return key==PC_STORE_HOF?16:key==PC_STORE_SAVE?192:208;}
static unsigned token_offset(unsigned key) {return key==PC_STORE_HOF?12:key==PC_STORE_SAVE?200:204;}
/* Spare bytes preserve compatibility with existing score/save records. */
#define RESET_TAG 0x31545352u
static uint32_t reset_id(const uint8_t *r) {
    return pc_record_get32(r+220)==RESET_TAG?pc_record_get32(r+216):0;
}
/* Each key carries the fingerprint of its last flash fallback. SD records
   remember those fingerprints after merging. A changed flash key therefore
   wins even after reboot when the SD generation was temporarily unreadable;
   unchanged flash keys never replace newer SD-only data. */
static bool load(uint8_t out[PC_RECORD_SIZE]) {
    _Alignas(4) uint8_t flash[PC_RECORD_SIZE];
    bool sd_ok=pc_sd_record_read(out),flash_ok=pc_flash_record_read(flash);
    backend=PC_STORE_NONE;
    if(!sd_ok) {
        if(!flash_ok)return false;
        memcpy(out,flash,PC_RECORD_SIZE);backend=PC_STORE_FLASH;return true;
    }
    backend=PC_STORE_SD;
    if(flash_ok && reset_id(flash) && reset_id(out)!=reset_id(flash)) {
        uint32_t seq=pc_record_get32(out+4),fs=pc_record_get32(flash+4);
        if((int32_t)(fs-seq)>0)seq=fs;
        memcpy(out,flash,PC_RECORD_SIZE);
        pc_record_put32(out+4,seq+1);pc_record_seal(out);
        backend=PC_STORE_FLASH;return true;
    }
    if(flash_ok) {
        uint32_t flags=pc_record_get32(out+8),ff=pc_record_get32(flash+8);
        for(unsigned key=0;key<3;key++) {
            unsigned token=token_offset(key);
            if((ff&(1u<<key)) && (!(flags&(1u<<key)) ||
               pc_record_get32(out+token)!=pc_record_get32(flash+token))) {
                memcpy(out+location(key),flash+location(key),length(key));
                memcpy(out+token,flash+token,4);flags|=1u<<key;
                backend=PC_STORE_FLASH;
            }
        }
        if(backend==PC_STORE_FLASH) {
            uint32_t seq=pc_record_get32(out+4),fs=pc_record_get32(flash+4);
            if((int32_t)(fs-seq)>0)seq=fs;
            pc_record_put32(out+4,seq+1);pc_record_put32(out+8,flags);
            pc_record_seal(out);
        }
    }
    return true;
}
bool PC_StoreRead(unsigned key,void *out,size_t bytes) {
    if(!out || !length(key) || bytes!=length(key))return false;
    _Alignas(4) uint8_t record[PC_RECORD_SIZE];
    if(!load(record))return false;
    if(backend==PC_STORE_FLASH && pc_sd_record_write(record))backend=PC_STORE_SD;
    if(!(pc_record_get32(record+8)&(1u<<key)))return false;
    memcpy(out,record+location(key),bytes);return true;
}
bool PC_StoreWrite(unsigned key,const void *data,size_t bytes) {
    if(!data || !length(key) || bytes!=length(key))return false;
    _Alignas(4) uint8_t record[PC_RECORD_SIZE];
    uint32_t seq=0,flags=0;
    if(load(record)) {
        seq=pc_record_get32(record+4);flags=pc_record_get32(record+8);
        if((flags&(1u<<key)) && !memcmp(record+location(key),data,bytes)) {
            if(backend==PC_STORE_FLASH && pc_sd_record_write(record))backend=PC_STORE_SD;
            return true;
        }
    } else {
        memset(record,255,sizeof record);
        pc_record_put32(record+12,0);pc_record_put32(record+200,0);pc_record_put32(record+204,0);
    }
    memcpy(record+location(key),data,bytes);
    pc_record_put32(record+4,seq+1);pc_record_put32(record+8,flags|(1u<<key));
    pc_record_seal(record);
    if(pc_sd_record_write(record)){backend=PC_STORE_SD;return true;}
    /* CRC-derived per-key revision; avoid reusing the previous fingerprint. */
    unsigned token=token_offset(key);
    uint32_t stamp=pc_record_get32(record+252);
    if(stamp==pc_record_get32(record+token))stamp^=0xa5a5a5a5u;
    pc_record_put32(record+token,stamp);pc_record_seal(record);
    if(pc_flash_record_write(record)){backend=PC_STORE_FLASH;return true;}
    backend=PC_STORE_NONE;return false;
}

/* Reset must reach flash even when SD works, otherwise a removed/stale card
   or an older fallback could resurrect deleted keys after a reboot. */
bool PC_StoreReset(void) {
    _Alignas(4) uint8_t record[PC_RECORD_SIZE];
    uint32_t seq=0,id=0;
    if(load(record)){seq=pc_record_get32(record+4);id=reset_id(record);}
    memset(record,0,sizeof record);
    pc_record_put32(record+4,seq+1);
    if(++id==0)id=1;
    pc_record_put32(record+216,id);pc_record_put32(record+220,RESET_TAG);
    pc_record_seal(record);
    if(!pc_flash_record_write(record)){backend=PC_STORE_NONE;return false;}
    backend=pc_sd_record_write(record)?PC_STORE_SD:PC_STORE_FLASH;
    return true;
}
