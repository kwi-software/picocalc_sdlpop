#pragma once
/* PicoCalc default wiring. Audio GPIO26/27 are PWM A/B of one slice.
   Keep this pair on the same slice if porting the packed stereo DMA backend. */
#define PC_AUDIO_LEFT 26
#define PC_AUDIO_RIGHT 27
#define PC_LCD_SPI spi1
#define PC_LCD_SCK 10
#define PC_LCD_MOSI 11
#define PC_LCD_CS 13
#define PC_LCD_DC 14
#define PC_LCD_RESET 15
#define PC_LCD_HZ 75000000u
#define PC_LCD_MADCTL 0x48
#define PC_LCD_WIDTH 320
#define PC_LCD_HEIGHT 320
#define PC_PWM_WRAP 1249u
#define PC_PWM_OVERSAMPLE 5u
