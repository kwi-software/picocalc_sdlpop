/* Exercise every PCM/MIDI resource through the actual decoder and mixer. */
#include "pop_port.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static PC_Blob file_blob(const char *dir, const char *name) {
    char path[1024];
    assert(snprintf(path,sizeof path,"%s/%s.DAT",dir,name)>0);
    FILE *f=fopen(path,"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); long n=ftell(f); assert(n>0); rewind(f);
    uint8_t *p=malloc((size_t)n); assert(p);
    assert(fread(p,1,(size_t)n,f)==(size_t)n); fclose(f);
    return (PC_Blob){p,(size_t)n};
}
int main(int argc,char **argv) {
    assert(argc==3);
    unsigned version=(unsigned)atoi(argv[2]); assert(version==1 || version==2);
    PC_Blob prince=file_blob(argv[1],"PRINCE"),bank;
    assert(POP_FindDATResource(prince,1,&bank));
    const char *groups[]={"DIGISND1","DIGISND2","DIGISND3","MIDISND1","MIDISND2"};
    unsigned tested=0; int16_t out[128];
    for(unsigned g=0;g<5;g++) {
        PC_Blob dat=file_blob(argv[1],groups[g]);
        for(unsigned id=10000;id<10058;id++) {
            PC_Blob sound;
            if(!POP_FindDATResource(dat,id,&sound))continue;
            if((sound.data[0]&7)!=1 && (sound.data[0]&7)!=2)continue;
            unsigned expected_blocks=0; uint32_t expected_hash=0;
            for(unsigned pass=0;pass<2;pass++) {
            PC_MixerInit();
            assert(POP_PlaySoundResource(sound,bank,0,id-10000,pass ? 0 : version));
            unsigned blocks=0; uint32_t hash=2166136261u;
            do {
                PC_MixAudio(NULL,out,128);
                assert(!PC_MixerErrors());
                for(unsigned i=0;i<128;i++)hash=(hash^(uint16_t)out[i])*16777619u;
                assert(++blocks<24000u*180u/128u);
            } while(PC_MixerActive());
            if(!pass){expected_blocks=blocks;expected_hash=hash;}
            else {assert(blocks==expected_blocks);assert(hash==expected_hash);}
            }
            tested++;
        }
        free((void *)dat.data);
    }
    free((void *)prince.data);
    printf("PASS: %u PCM/MIDI resources decoded to completion; auto/explicit output identical\n",tested);
}
