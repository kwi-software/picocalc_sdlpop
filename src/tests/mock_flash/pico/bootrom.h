#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Same encodings as Pico SDK 2.2.0 boot/bootrom_constants.h. */
#define CFLASH_ASPACE_LSB 0
#define CFLASH_ASPACE_VALUE_RUNTIME 1
#define CFLASH_SECLEVEL_LSB 8
#define CFLASH_SECLEVEL_VALUE_SECURE 1
#define CFLASH_OP_LSB 16
#define CFLASH_OP_VALUE_ERASE 0
#define CFLASH_OP_VALUE_PROGRAM 1
#define BOOTROM_OK 0
#define ROM_FUNC_FLASH_OP 0x4f46
#define BOOTROM_LOCK_FLASH_OP 1
typedef struct {uint32_t flags;} cflash_flags_t;
typedef int (*rom_flash_op_fn)(cflash_flags_t,uintptr_t,uint32_t,uint8_t *);
void *rom_func_lookup_inline(unsigned key);
bool bootrom_try_acquire_lock(unsigned lock);
void bootrom_release_lock(unsigned lock);

void rom_flash_flush_cache(void);
