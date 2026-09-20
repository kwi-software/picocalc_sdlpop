/* GPL-3.0: unmodified validation function excerpt from pelrun/uf2loader ui/uf2.c.
 * Version 2.5, commit 5c44a4b64749062b0200507ceeff3ef2b475e288.
 * Originally from muzkr/hachi, Copyright (c) 2024 muzkr;
 * modified for SD boot by pelrun 2025. See root COPYING.
 * Only host tests include this file; it is not part of the game firmware.
 */
static bool check_generic_block(const struct uf2_block* b)
{
  if (UF2_MAGIC_START0 != b->magic_start0 || UF2_MAGIC_START1 != b->magic_start1  //
      || UF2_MAGIC_END != b->magic_end)
  {
    DEBUG_PRINT("Invalid UF2 magic\n");
    return false;
  }
  if (b->flags & UF2_FLAG_NOT_MAIN_FLASH)
  {
    DEBUG_PRINT("Not for flashing\n");
    return false;
  }
  if (b->target_addr % FLASH_PAGE_SIZE)
  {
    DEBUG_PRINT("Bad alignment\n");
    return false;
  }
  if (b->payload_size != FLASH_PAGE_SIZE)
  {
    DEBUG_PRINT("Incorrect block size\n");
    return false;
  }
  if (b->num_blocks == 0)
  {
    DEBUG_PRINT("Nothing to write\n");
    return false;
  }
  if (b->block_no >= b->num_blocks)
  {
    DEBUG_PRINT("Block count exceeded\n");
    return false;
  }

  // *yes* b->file_size is actually family_id when the UF2_FLAG_FAMILY_ID_PRESENT flag is set

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size == ABSOLUTE_FAMILY_ID &&
      b->block_no == 0 && b->target_addr == 0x10FFFF00)
  {
    // Skip RP2350-E10 workaround block
    DEBUG_PRINT("Skip RP2350-E10 dummy block\n");

    if (b->num_blocks != 2)
    {
      // need to deal with a malformed uf2
      s.malformed_uf2 = true;
    }

    return false;
  }

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && !family_valid(b->file_size))
  {
    DEBUG_PRINT("Not for this platform\n");
    s.family_id = b->file_size;
    return false;
  }
#if ENABLE_RAM_APPS
  if (b->target_addr >= SRAM_BASE && b->target_addr < SRAM_END)
  {
    // This is a no-flash binary, so we're going to immediately reboot into the bootloader to run it
    // convert path completely to short filename as bootloader can't use LFN

    char* sfn_path = get_short_path(s.filename);
    if (sfn_path)
    {
      text_directory_ui_set_status("Launching RAM-only app");
      bl_stage3_command(BOOT_RAM, (uintptr_t)sfn_path);
      reboot();
    }
    return false;
  }
#endif

  if (b->target_addr < XIP_BASE || b->target_addr >= prog_area_end)
  {
    DEBUG_PRINT("Out of bounds %x > %x >= %x\n", XIP_BASE, b->target_addr, prog_area_end);
    return false;
  }

  return true;
}

static bool check_1st_block(const struct uf2_block* b)
{
  if (!check_generic_block(b))
  {
    return false;
  }

  if (b->block_no != (s.malformed_uf2 ? 1 : 0))
  {
    DEBUG_PRINT("First block is missing\n");
    return false;
  }

  return true;
}

static bool check_block(const prog_state_t* s, const struct uf2_block* b)
{
  if (!check_generic_block(b))
  {
    return false;
  }

  if (s->num_blks != b->num_blocks - (s->malformed_uf2 ? 1 : 0))
  {
    return false;
  }
  if (s->num_blks_written != b->block_no - (s->malformed_uf2 ? 1 : 0))
  {
    return false;
  }

  return true;
}
