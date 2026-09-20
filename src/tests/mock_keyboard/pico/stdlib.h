#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define PICO_ERROR_GENERIC (-1)
#define PICO_ERROR_TIMEOUT (-2)
#define GPIO_FUNC_I2C 3
void gpio_set_function(unsigned,unsigned);
void gpio_pull_up(unsigned);
void sleep_us(uint64_t);
