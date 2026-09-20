/* Host-only persistence backend. Set PRINCE_STORE_PATH for cross-process tests. */
#include "pc_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t flash[PC_STORE_BYTES];
static bool initialized;
const uint8_t *pc_store_flash(void) {
    if(!initialized) {
        memset(flash,255,sizeof flash);initialized=true;
        const char *path=getenv("PRINCE_STORE_PATH");
        if(path){FILE *f=fopen(path,"rb");if(f){if(fread(flash,1,sizeof flash,f)!=sizeof flash)memset(flash,255,sizeof flash);fclose(f);}}
    }
    return flash;
}
bool pc_store_program(unsigned offset,const uint8_t page[256],bool erase) {
    (void)pc_store_flash();
    if(offset%256 || offset>PC_STORE_BYTES-256 || (erase && offset%4096))return false;
    if(erase)memset(flash+offset,255,4096);
    for(unsigned i=0;i<256;i++) {if((flash[offset+i]&page[i])!=page[i])return false;flash[offset+i]&=page[i];}
    const char *path=getenv("PRINCE_STORE_PATH");
    if(path){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(flash,1,sizeof flash,f)==sizeof flash;return fclose(f)==0 && ok;}
    return true;
}
