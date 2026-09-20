#pragma once
#include "pc_store.h"
#define PC_RECORD_SIZE 256u
uint32_t pc_record_get32(const uint8_t *p);
void pc_record_put32(uint8_t *p, uint32_t v);
bool pc_record_valid(const uint8_t p[PC_RECORD_SIZE]);
void pc_record_seal(uint8_t p[PC_RECORD_SIZE]);
bool pc_flash_record_read(uint8_t out[PC_RECORD_SIZE]);
bool pc_flash_record_write(const uint8_t record[PC_RECORD_SIZE]);
bool pc_sd_record_read(uint8_t out[PC_RECORD_SIZE]);
bool pc_sd_record_write(const uint8_t record[PC_RECORD_SIZE]);
