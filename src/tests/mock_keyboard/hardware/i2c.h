#pragma once
#include "pico/stdlib.h"
#define i2c1 ((void *)1)
int i2c_write_timeout_us(void *,unsigned,const uint8_t *,size_t,bool,unsigned);
int i2c_read_timeout_us(void *,unsigned,uint8_t *,size_t,bool,unsigned);
unsigned i2c_init(void *,unsigned);
