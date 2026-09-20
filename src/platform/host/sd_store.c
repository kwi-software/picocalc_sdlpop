/* Game host tests default to flash-only. FAT32 is exercised against a disk image
   by the dedicated SD-storage integration test. */
#include "pc_store_internal.h"
bool pc_sd_record_read(uint8_t out[PC_RECORD_SIZE]) {(void)out;return false;}
bool pc_sd_record_write(const uint8_t record[PC_RECORD_SIZE]) {(void)record;return false;}
