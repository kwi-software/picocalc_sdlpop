/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_status.h"
#include "pop_port.h"
#include <stdio.h>
static unsigned mode; /* 0: hidden (startup), 1: battery, 2: battery and lights. */
static bool dirty, battery_sampled, lights_sampled;
static uint32_t battery_time;
static int battery=-1, lcd=-1, keyboard=-1;
void PC_StatusCycle(void) {
    mode=(mode+1)%3;
    dirty=true;
    if(mode==1)battery_sampled=false;
    if(mode==2)lights_sampled=false;
}
void PC_StatusInvalidate(void) { if(mode)dirty=true; }
static void sample(PC_StatusValue field,int *cached) {
    int value=PC_ReadStatusValue(field);
    if(value!=*cached){*cached=value;dirty=true;}
}
/* Event-driven adjustment; re-read before writing to include external changes.
   Keep the LCD readable (minimum 16); the keyboard may be fully switched off. */
void PC_StatusAdjustBacklight(PC_StatusValue field,int direction) {
    if((field!=PC_STATUS_LCD && field!=PC_STATUS_KEYBOARD) || !direction)return;
    int *cached=field==PC_STATUS_LCD?&lcd:&keyboard;
    int value=PC_ReadStatusValue(field);
    if(value>=0) {
        /* Stock BIOS quantizes LCD to 16 and keyboard to 32. A keyboard
           request above 240 wraps to OFF, so its highest safe step is 224. */
        int step=field==PC_STATUS_LCD?16:32;
        int maximum=field==PC_STATUS_LCD?240:224;
        int next=value+(direction>0?step:-step);
        int minimum=field==PC_STATUS_LCD?16:0;
        if(next<minimum)next=minimum;
        if(next>maximum)next=maximum;
        if(next!=value)value=PC_WriteBacklight(field,(uint8_t)next);
    }
    if(value!=*cached && mode==2)dirty=true;
    *cached=value;
}
static int brightness_percent(int value,int maximum) {
    if(value<0)return -1;
    if(value>maximum)value=maximum;
    return (value*100+maximum/2)/maximum;
}
static void percent(char *out,size_t size,const char *label,int value) {
    if(value<0)snprintf(out,size,"%s --%%",label);
    else snprintf(out,size,"%s%3d%%",label,value);
}
void PC_StatusTick(void) {
    if(mode) {
        uint32_t now=PC_GetTicks();
        if(!battery_sampled || (uint32_t)(now-battery_time)>=60000) {
            sample(PC_STATUS_BATTERY,&battery);
            battery_time=now;battery_sampled=true;
        }
        if(mode==2 && !lights_sampled) {
            sample(PC_STATUS_LCD,&lcd);sample(PC_STATUS_KEYBOARD,&keyboard);
            lights_sampled=true;
        }
    }
    if(!dirty)return;
    dirty=false;
    PC_SetRenderDrawColor(0,0,0);
    PC_RenderFillRect(&(PC_Rect){0,0,320,26});
    if(!mode)return;
    PC_SetRenderDrawColor(210,210,210);
    char text[16];
    percent(text,sizeof text,"",battery);
    PC_DrawTextSmall(274,7,text);
    /* Battery outline and terminal, followed by a proportional fill. */
    PC_RenderFillRect(&(PC_Rect){232,7,24,11});
    PC_RenderFillRect(&(PC_Rect){256,10,3,5});
    PC_SetRenderDrawColor(0,0,0);
    PC_RenderFillRect(&(PC_Rect){233,8,22,9});
    if(battery>0) {
        PC_SetRenderDrawColor(battery<=20?255:120,battery<=20?100:220,100);
        PC_RenderFillRect(&(PC_Rect){234,9,(battery*20+99)/100,7});
    }
    if(mode==2) {
        PC_SetRenderDrawColor(210,210,210);
        percent(text,sizeof text,"LCD:",brightness_percent(lcd,240));
        PC_DrawTextSmall(6,7,text);
        percent(text,sizeof text,"KEY:",brightness_percent(keyboard,224));
        PC_DrawTextSmall(114,7,text);
    }
    PC_RenderPresent();
}
