/* Two-sector append journal. Previous committed record survives interrupted writes. */
#include "pc_store_internal.h"
#include <string.h>
#define MAGIC 0x33504f50u

uint32_t pc_record_get32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
void pc_record_put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++) p[i] = v >> (i * 8);
}
static uint32_t crc(const uint8_t *p) {
    uint32_t v = ~0u;
    for (unsigned i = 0; i < 252; i++) {
        v ^= p[i];
        for (unsigned b = 0; b < 8; b++)
            v = (v >> 1) ^ (0xedb88320u & (0u - (v & 1)));
    }
    return ~v;
}
bool pc_record_valid(const uint8_t p[PC_RECORD_SIZE]) {
    return pc_record_get32(p)==MAGIC && pc_record_get32(p+8)<=3 &&
           crc(p)==pc_record_get32(p+252);
}
void pc_record_seal(uint8_t p[PC_RECORD_SIZE]) {
    pc_record_put32(p,MAGIC); pc_record_put32(p+252,crc(p));
}
static int latest(void) {
    const uint8_t *f = pc_store_flash();
    int best = -1;
    uint32_t seq = 0;
    for (unsigned off = 0; off < PC_STORE_BYTES; off += 256) {
        const uint8_t *p = f + off;
        if (!pc_record_valid(p))
            continue;
        uint32_t n = pc_record_get32(p + 4);
        if (best < 0 || (int32_t)(n - seq) > 0) {
            best = (int)off;
            seq = n;
        }
    }
    return best;
}
bool pc_flash_record_read(uint8_t out[PC_RECORD_SIZE]) {
    int old=latest();
    if(old<0)return false;
    memcpy(out,pc_store_flash()+old,PC_RECORD_SIZE);return true;
}
bool pc_flash_record_write(const uint8_t page[PC_RECORD_SIZE]) {
    if(!pc_record_valid(page))return false;
    int old=latest();
    if(old>=0 && !memcmp(pc_store_flash()+old,page,PC_RECORD_SIZE))return true;
    unsigned next = old < 0 ? 0 : (unsigned)old + 256;
    if (next == PC_STORE_BYTES) next = 0;
    bool erase = (next % 4096) == 0;
    /* A failed write can leave the next page partially programmed. Switch to
       the OTHER sector, preserving the last valid record, before retrying. */
    if (!erase) {
        for (unsigned i = 0; i < 256; i++) {
            if (pc_store_flash()[next + i] != 255) {
                next = ((unsigned)old / 4096 ^ 1u) * 4096;
                erase = true;
                break;
            }
        }
    }
    if (!pc_store_program(next, page, erase)) return false;
    return !memcmp(pc_store_flash() + next, page, 256);
}
