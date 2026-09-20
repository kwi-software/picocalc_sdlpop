/* Exercise the real 2.5 validation functions without performing flash writes. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define XIP_BASE 0x10000000u
#define FLASH_PAGE_SIZE 256u
#define UF2_MAGIC_START0 0x0a324655u
#define UF2_MAGIC_START1 0x9e5d5157u
#define UF2_MAGIC_END 0x0ab16f30u
#define UF2_FLAG_NOT_MAIN_FLASH 1u
#define UF2_FLAG_FAMILY_ID_PRESENT 0x2000u
#define ABSOLUTE_FAMILY_ID 0xe48bff57u
#define ENABLE_RAM_APPS 0
#define DEBUG_PRINT(...) ((void)0)
struct uf2_block {
    uint32_t magic_start0,magic_start1,flags,target_addr,payload_size,block_no,num_blocks,file_size;
    uint8_t data[476];
    uint32_t magic_end;
};
_Static_assert(sizeof(struct uf2_block)==512,"UF2 block layout");
typedef struct {
    const char *filename;
    uint32_t num_blks,num_blks_read,num_blks_written,family_id;
    bool malformed_uf2;
} prog_state_t;
static prog_state_t s;
/* Pico 2W: physical application starts at +8 KiB; remaining 4088 KiB mapped at XIP_BASE. */
static uintptr_t prog_area_end=XIP_BASE+0x400000-0x2000;
static bool family_valid(uint32_t family) {
    return family==0xe48bff59u || family==0xe48bff5au || family==0xe48bff5bu;
}
#include "loader25_checks.h"
int main(int argc,char **argv) {
    if(argc<2 || argc>3)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    struct uf2_block block;
    while(fread(&block,1,sizeof block,f)==sizeof block) {
        s.num_blks_read++;
        if(s.num_blks_written) {
            if(!check_block(&s,&block))continue;
        } else {
            s.num_blks=block.num_blocks-(s.malformed_uf2?1:0);
            if(!check_1st_block(&block))continue;
        }
        s.num_blks_written++;
    }
    fclose(f);
    bool accepted=s.num_blks && s.num_blks_written==s.num_blks;
    printf("Loader 2.5: %s; accepted %u of %u application blocks\n",accepted?"ACCEPTED":"BAD UF2",s.num_blks_written,s.num_blks);
    return accepted == (argc==2) ? 0 : 1;
}
