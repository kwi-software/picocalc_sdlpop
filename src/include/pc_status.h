/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "pc_media.h"
typedef enum { PC_STATUS_BATTERY, PC_STATUS_LCD, PC_STATUS_KEYBOARD } PC_StatusValue;
/* Platform query: battery percent or backlight 0..255; -1 on read failure. */
int PC_ReadStatusValue(PC_StatusValue value);
/* Write a backlight and return its confirmed 0..255 value, or -1 on failure. */
int PC_WriteBacklight(PC_StatusValue field, uint8_t value);
void PC_StatusAdjustBacklight(PC_StatusValue field, int direction);
/* Core 0 only. Draws directly into LCD rows 0..25, outside the game frames. */
void PC_StatusCycle(void);
void PC_StatusInvalidate(void);
void PC_StatusTick(void);
