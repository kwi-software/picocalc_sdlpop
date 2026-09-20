#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PC_STORE_HOF 0
#define PC_STORE_SAVE 1
#define PC_STORE_HOF_SIZE 176u
#define PC_STORE_SAVE_SIZE 8u
#define PC_STORE_BYTES 8192u
typedef enum {PC_STORE_NONE, PC_STORE_SD, PC_STORE_FLASH} PC_StoreBackend;
PC_StoreBackend PC_StoreLastBackend(void);
bool PC_StoreRead(unsigned key, void *out, size_t bytes);
bool PC_StoreWrite(unsigned key, const void *data, size_t bytes);
/* Hardware boundary. Read pointer must reflect writable flash, not a constant. */
const uint8_t *pc_store_flash(void);
bool pc_store_program(unsigned offset, const uint8_t page[256], bool erase);
void PC_AudioFlashBegin(void);
void PC_AudioFlashEnd(void);
