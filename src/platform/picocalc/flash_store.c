#include "pc_store.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"

/* Runtime (XIP) address inside the application's first 2 MiB. The UF2 Loader
   relocates the app physically; never use this as a raw flash chip offset. */
#define STORE_ADDRESS (XIP_BASE + 0x1fe000u)
_Static_assert(PC_STORE_BYTES == 8192, "Linker journal size must match");

/* Explicit UF2 pages reset saved data when this application is reinstalled. */
__attribute__((used, section(".persistent_reset")))
const uint8_t pc_store_reset_image[PC_STORE_BYTES] = {[0 ... PC_STORE_BYTES-1] = 255};

const uint8_t *pc_store_flash(void) {
    return (const uint8_t *)STORE_ADDRESS;
}
bool pc_store_program(unsigned offset, const uint8_t page[256], bool erase) {
    if (!page || offset > PC_STORE_BYTES - 256 || offset % 256 ||
        (erase && offset % 4096)) return false;

    /* Same ROM operation and lock as SDK rom_flash_op(), with our own core-1
       RAM parking handshake instead of the SDK's multicore FIFO lockout.
       Resolve the ROM pointer BEFORE XIP can be disabled. The ROM operation
       restores XIP before returning; no application instruction runs while
       XIP is disabled. RUNTIME translation also enforces partition bounds. */
    rom_flash_op_fn op = (rom_flash_op_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_OP);
    if (!op || !bootrom_try_acquire_lock(BOOTROM_LOCK_FLASH_OP)) return false;

    const uint32_t common =
        (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
        (CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB);
    PC_AudioFlashBegin();
    uint32_t interrupts = save_and_disable_interrupts();
    int result = BOOTROM_OK;
    if (erase) {
        cflash_flags_t flags = {common | (CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB)};
        result = op(flags, STORE_ADDRESS + offset, 4096, NULL);
    }
    if (result == BOOTROM_OK) {
        cflash_flags_t flags = {common | (CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB)};
        result = op(flags, STORE_ADDRESS + offset, 256, (uint8_t *)page);
    }
    /* Checked ROM writes do not invalidate previously cached XIP data. */
    rom_flash_flush_cache();
    restore_interrupts(interrupts);
    PC_AudioFlashEnd();
    bootrom_release_lock(BOOTROM_LOCK_FLASH_OP);
    return result == BOOTROM_OK;
}
