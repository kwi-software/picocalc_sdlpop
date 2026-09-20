/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Blair Leduc
 * Adapted from https://github.com/BlairLeduc/picocalc-text-starter
 * Local PicoCalc game-port changes; see ASSET_SOURCES.md.
 * The full MIT license is in COPYING.PICOCALC at the project root.
 */
//
//  "SouthBridge" functions
//
//  The PicoCalc on-board processor acts as a "southbridge", managing lower-speed functions
//  that provides access to the keyboard, battery, and other peripherals.
//

#include <stdatomic.h>

#include "pico/stdlib.h"
#include "pico/platform.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "southbridge.h"

static bool sb_initialised = false;
volatile atomic_bool sb_i2c_in_use = false; // flag to indicate if I2C bus is in use

//
//  Protect access to the "South Bridge"
//

//  Is the southbridge available?
bool sb_available()
{
    return atomic_load(&sb_i2c_in_use) == false;
}

static size_t sb_write(const uint8_t *src, size_t len)
{
    int result = i2c_write_timeout_us(SB_I2C, SB_ADDR, src, len, false, SB_I2C_TIMEOUT_US * len);
    if (result == PICO_ERROR_GENERIC || result == PICO_ERROR_TIMEOUT)
    {
        // Write error
        return 0;
    }
    return result;
}

static size_t sb_read(uint8_t *dst, size_t len)
{
    int result = i2c_read_timeout_us(SB_I2C, SB_ADDR, dst, len, false, SB_I2C_TIMEOUT_US * len);
    if (result == PICO_ERROR_GENERIC || result == PICO_ERROR_TIMEOUT)
    {
        // Read error
        return 0;
    }
    return result;
}

// Read the keyboard
uint16_t sb_read_keyboard()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_FIF;             // command to check if key is available
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[0] << 8 | buffer[1];
}

uint16_t sb_read_keyboard_state()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_KEY;             // command to read key state
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[0];
}

// Read the battery level from the southbridge
uint8_t sb_read_battery()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_BAT; // command to read battery level
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1];
}

// Read the LCD backlight level
uint8_t sb_read_lcd_backlight()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_BKL; // command to read LCD backlight
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1];
}

// Write the LCD backlight level
uint8_t sb_write_lcd_backlight(uint8_t brightness)
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_BKL | SB_WRITE; // command to write LCD backlight
    buffer[1] = brightness;
    if (sb_write(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1];
}

// Read the keyboard backlight level
uint8_t sb_read_keyboard_backlight()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_BK2; // command to read keyboard backlight
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1];
}

// Write the keyboard backlight level
uint8_t sb_write_keyboard_backlight(uint8_t brightness)
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_BK2 | SB_WRITE; // command to write keyboard backlight
    buffer[1] = brightness;
    if (sb_write(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return 0;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1];
}

bool sb_is_power_off_supported()
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_OFF; // read the power-off register
    if (sb_write(buffer, 1) != 1)
    {
        atomic_store(&sb_i2c_in_use, false);
        return false;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return false;
    }
    atomic_store(&sb_i2c_in_use, false);

    return buffer[1] > 0;
}

bool sb_write_power_off_delay(uint8_t delay_seconds)
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_OFF | SB_WRITE; // command to write power-off delay
    buffer[1] = delay_seconds;
    if (sb_write(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return false;
    }
    atomic_store(&sb_i2c_in_use, false);
    return true;
}

bool sb_reset(uint8_t delay_seconds)
{
    uint8_t buffer[2];

    atomic_store(&sb_i2c_in_use, true);
    buffer[0] = SB_REG_RST | SB_WRITE; // command to reset the PicoCalc
    buffer[1] = delay_seconds;
    if (sb_write(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return false;
    }
    if (sb_read(buffer, 2) != 2)
    {
        atomic_store(&sb_i2c_in_use, false);
        return false;
    }
    atomic_store(&sb_i2c_in_use, false);
    return true;
}

// Initialize the southbridge
void sb_init()
{
    if (sb_initialised)
    {
        return; // already initialized
    }

    i2c_init(SB_I2C, SB_BAUDRATE);
    gpio_set_function(SB_SCL, GPIO_FUNC_I2C);
    gpio_set_function(SB_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(SB_SCL);
    gpio_pull_up(SB_SDA);

    // Set the initialised flag
    sb_initialised = true;
}

/* REG_KEY reports FIFO occupancy, NOT currently pressed keys. Read the actual
   matrix (0x0c) and arrow buttons (0x0d); validate register echoes before use. */
bool sb_read_keyboard_matrix(uint8_t columns[8], uint8_t *arrows) {
    static uint8_t previous_buttons;
    uint8_t cmd = 0x0c, matrix[10], buttons[2] = {0,255};
    atomic_store(&sb_i2c_in_use, true);
    bool ok = sb_write(&cmd, 1) == 1 && sb_read(matrix, sizeof matrix) == sizeof matrix;
    /* The stock keyboard.ino resets js_bits before its scan interval check.
       Therefore one all-up arrow sample can be a transient between scans.
       Three reads roughly 7ms apart cover the firmware's alternating ~10ms
       scan/skip phases at the specified 10kHz bus speed. OR active-low presses. */
    uint8_t combined = 255;
    for (unsigned i=0; ok && i<3; ++i) {
        if (i) sleep_us(3500);
        cmd = 0x0d;
        ok = sb_write(&cmd, 1) == 1 && sb_read(buttons, sizeof buttons) == sizeof buttons;
        ok = ok && buttons[0] == 0x0d;
        combined &= buttons[1];
        /* A pressed arrow proves this is a scan sample, not the all-up gap. */
        if ((combined & 15) != 15) break;
    }
    /* keyboard.ino writes each column with bit 7 initially zero, then updates
       the eight discrete buttons. An I2C interrupt between those steps sees
       false Shift/Ctrl/Alt/0/9/[/] presses. Confirm only NEW button presses
       after the arrow transaction(s), several milliseconds after that window.
       Ordinary matrix keys, releases and already-held keys incur no extra read. */
    uint8_t pressed = 0;
    if (ok && matrix[0] == 0x0c) {
        for (unsigned i=0;i<8;i++) if (!(matrix[i+1] & 128)) pressed |= 1u << i;
        if (pressed & (uint8_t)~previous_buttons) {
            uint8_t confirm[10];
            cmd = 0x0c;
            ok = sb_write(&cmd,1)==1 && sb_read(confirm,sizeof confirm)==sizeof confirm;
            ok = ok && confirm[0]==0x0c;
            if (ok) {
                pressed = 0;
                for (unsigned i=0;i<8;i++) {
                    matrix[i+1] |= confirm[i+1] & 128;
                    if (!(matrix[i+1]&128)) pressed |= 1u << i;
                }
            }
        }
    } else ok = false;
    atomic_store(&sb_i2c_in_use, false);
    if (!ok) { previous_buttons = 0; return false; }
    previous_buttons = pressed;
    for (unsigned i = 0; i < 8; ++i) columns[i] = matrix[i + 1];
    *arrows = combined;
    return true;
}
