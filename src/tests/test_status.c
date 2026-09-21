/* Test polling cadence and direct LCD updates without game framebuffers. */
#include "pc_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static unsigned reads[3],clears,draws;
static int values[3]={75,180,0};
static unsigned writes;
static bool fail_write;
static char battery[16],lcd[16],keyboard[16],notice[26];
uint32_t PC_GetTicks(void){return now;}
int PC_ReadStatusValue(PC_StatusValue f){reads[f]++;return values[f];}
int PC_WriteBacklight(PC_StatusValue f,uint8_t value){
    assert(f==PC_STATUS_LCD || f==PC_STATUS_KEYBOARD);writes++;
    if(fail_write)return -1;
    /* Model stock BIOS rounding, including keyboard wrap-to-off. */
    if(f==PC_STATUS_LCD){value=(value/16)*16;if(value<16)value=16;if(value>240)value=240;}
    else {value=(value/32)*32;if(value>240)value=0;}
    values[f]=value;return value;
}
void PC_SetRenderDrawColor(uint8_t r,uint8_t g,uint8_t b){(void)r;(void)g;(void)b;}
void PC_RenderFillRect(const PC_Rect *r){
    assert(r && r->x>=0 && r->y>=0 && r->x+r->w<=320 && r->y+r->h<=26);
    if(r->w==320){clears++;battery[0]=lcd[0]=keyboard[0]=notice[0]=0;}
}
void PC_DrawText(int x,int y,const char *text){assert(x==6 && y==6 && strlen(text)<sizeof notice);strcpy(notice,text);}
void PC_DrawTextSmall(int x,int y,const char *text){
    assert(y==7 && strlen(text)<16);draws++;
    if(x==274)strcpy(battery,text);
    else if(x==6)strcpy(lcd,text);
    else {assert(x==114);strcpy(keyboard,text);}
}
void PC_RenderPresent(void){}
int main(void){
    PC_StatusTick();assert(!clears && !reads[0]);
    PC_StatusCycle();PC_StatusTick();
    assert(reads[0]==1 && !reads[1] && !reads[2]);
    assert(!strcmp(battery," 75%") && !lcd[0]);
    unsigned before=draws;
    for(now=1;now<60000;now+=11)PC_StatusTick();
    assert(reads[0]==1 && draws==before);
    now=60000;values[0]=74;PC_StatusTick();
    assert(reads[0]==2 && !strcmp(battery," 74%"));
    PC_StatusCycle();PC_StatusTick();
    assert(!strcmp(lcd,"LCD: 75%") && !strcmp(keyboard,"KEY:  0%"));
    before=draws;now+=10000;PC_StatusTick();
    assert(draws==before && reads[1]==1 && reads[2]==1); /* No periodic light reads. */
    PC_StatusAdjustBacklight(PC_STATUS_LCD,-1);PC_StatusTick();
    assert(values[1]==160 && writes==1 && !strcmp(lcd,"LCD: 67%"));
    /* Regression: the old 16-step request from zero was rounded back to zero. */
    PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,1);PC_StatusTick();
    assert(values[2]==32 && writes==2 && !strcmp(keyboard,"KEY: 14%"));
    for(unsigned i=0;i<20;i++)PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,1);
    PC_StatusTick();assert(values[2]==224 && !strcmp(keyboard,"KEY:100%"));
    before=writes;PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,1);assert(writes==before);
    for(unsigned i=0;i<20;i++)PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,-1);
    assert(values[2]==0);
    before=writes;PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,-1);assert(writes==before);
    for(unsigned i=0;i<20;i++)PC_StatusAdjustBacklight(PC_STATUS_LCD,1);
    PC_StatusTick();assert(values[1]==240 && !strcmp(lcd,"LCD:100%"));
    values[1]=20;PC_StatusAdjustBacklight(PC_STATUS_LCD,-1);assert(values[1]==16);
    before=writes;PC_StatusAdjustBacklight(PC_STATUS_LCD,-1);assert(writes==before);
    values[1]=-1;PC_StatusAdjustBacklight(PC_STATUS_LCD,1);PC_StatusTick();
    assert(writes==before && !strcmp(lcd,"LCD: --%"));
    values[1]=100;fail_write=true;PC_StatusAdjustBacklight(PC_STATUS_LCD,1);PC_StatusTick();
    assert(values[1]==100 && !strcmp(lcd,"LCD: --%"));fail_write=false;
    before=reads[1];PC_StatusInvalidate();PC_StatusTick();assert(reads[1]==before);
    PC_StatusCycle();PC_StatusTick();assert(!battery[0] && !lcd[0]);
    now+=100000;PC_StatusTick();assert(reads[1]==before && reads[0]==2);
    before=draws;PC_StatusAdjustBacklight(PC_STATUS_KEYBOARD,1);PC_StatusTick();
    assert(values[2]==32 && draws==before); /* Keys work with the bar hidden. */
    /* Fresh opening, unknown battery and timer wraparound. */
    now=UINT32_MAX-100;values[0]=-1;PC_StatusCycle();PC_StatusTick();
    assert(!strcmp(battery," --%"));
    before=reads[0];now+=59999;PC_StatusTick();assert(reads[0]==before);
    now++;values[0]=100;PC_StatusTick();assert(!strcmp(battery,"100%"));
    PC_StatusSetMode(2);PC_StatusRestoreBacklights(192,32);PC_StatusTick();
    assert(PC_StatusGetMode()==2 && !strcmp(lcd,"LCD: 80%") && !strcmp(keyboard,"KEY: 14%"));
    PC_StatusNotice("SETTINGS SAVED FLASH");PC_StatusTick();
    assert(!strcmp(notice,"SETTINGS SAVED FLASH") && !lcd[0]);
    now+=2999;PC_StatusTick();assert(notice[0]);
    now++;PC_StatusTick();assert(!notice[0] && lcd[0]);
    PC_StatusSetMode(0);PC_StatusNotice("SAVED DATA CLEARED");PC_StatusTick();assert(notice[0]);
    now+=3000;PC_StatusTick();assert(!notice[0] && !lcd[0]);
    puts("PASS: status cycle, percent values, polling limits, event-driven backlights, limits, hidden controls, errors and timer wrap");
}
