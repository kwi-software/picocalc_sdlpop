#include "pwm_encode.h"
#include "pc_media.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    assert(PC_AUDIO_RATE * PC_PWM_OVERSAMPLE * (PC_PWM_WRAP+1)==150000000);
    uint32_t error=32768, words[PC_PWM_OVERSAMPLE];
    int64_t wanted=0, actual=0;
    for(int v=-32768;v<=32767;v++) {
        int16_t pcm=v;
        pc_pwm_encode(&pcm,words,1,&error);
        for(unsigned j=0;j<PC_PWM_OVERSAMPLE;j++) {
            unsigned duty=words[j]&65535;
            assert((words[j]>>16)==duty && duty<=PC_PWM_WRAP+1);
            wanted+=(int64_t)(v+32768)*(PC_PWM_WRAP+1);
            actual+=(int64_t)duty*65536;
            assert(llabs(actual-wanted)<=32768);
        }
    }
    int16_t zero=0;
    for(unsigned n=0;n<1000;n++) {
        pc_pwm_encode(&zero,words,1,&error);
        for(unsigned j=0;j<PC_PWM_OVERSAMPLE;j++) assert((words[j]&65535)==625);
    }
    puts("PASS: all 65536 PCM values, bounded noise-shaper error, dual mono, constant silent PWM, exact 24kHz pacing");
}
