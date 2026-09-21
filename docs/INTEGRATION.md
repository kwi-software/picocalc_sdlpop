# Architecture and storage

Base: SDLPoP commit `3c5add5fb7f83d4ceb542823ab66d00146c4271b`. `src/game/upstream-changes.patch` records changes to imported core files relative to that commit's `src/`. `portable.c` adapts portable seg009 routines; `adapter.c` replaces desktop graphics, audio, events and resource access. The SDL2 header provides the subset needed by this game, not general SDL2 compatibility.

## Graphics and assets

Original DATs must be provided by the user; none are included. CMake decodes DAT graphics once at build time. For the previously tested DOS 1.0 set, the 840 asset entries include 21 DAT blobs and prepared surfaces, with 1,345,280 bytes of pixel/palette/binary payload. The generated JSON contains input hashes. No PNG library or game-sprite DAT decompression is needed at runtime. Internal names such as `KID/res401.png` are lookup keys, not actual PNG files. Text uses the original built-in bitmap font.

Sprite pixels stay in flash. Small mutable palettes and surface metadata use the 164 KiB coalescing game allocator on core 0. Two fixed 320x200 RGB565 buffers occupy 256,000 bytes and avoid fragmentation when transitioning between 192- and 200-line surfaces.

The confirmed ST7365P path uses COLMOD 0x55, 75 MHz SPI and 16-bit DMA: two bytes per pixel. Output is 320x240 at y=40 for 4:3 or 320x200 at y=60 for 16:10. The existing 640-byte scanline buffer handles channel-wise area-weighted RGB565 scaling from 200 to 240 rows. Each five source rows A–E produce A, (A+4B)/5, (2B+3C)/5, (3C+2D)/5, (4D+E)/5, E. Integer channel rounding prevents cross-channel carries; two of every six output rows use an exact copy. The 16:10 path copies pixels exactly. The shared implementation is in `src/include/pc_video_scale.h`; it uses integer arithmetic and no additional framebuffer. The hardware driver and host model both use it. LCD wire tests cover an independent pixel-coverage reference, border rows, narrow pitched input and partial updates. Hardware appearance and rendering time still require measurement. Title wipes transfer only newly revealed columns, including catch-up/final strips. F1 and a Ctrl+F filter change force a complete redraw. Ctrl+F is consumed without changing the setting in 16:10; key repeats do not toggle it again. The vertical filter is disabled at startup and its preference is retained across aspect changes. Damage flashes stay within the 192-line gameplay area, preserving the health display in rows 192–199.

Text bypasses filtering using two fixed tables of 200 horizontal intervals (1600 bytes plus two pointers). Font glyphs and title/story lettering resources mark the destination rows when drawn. Rectangle copies propagate these intervals, opaque writes trim or clear them, and vertical flips reverse them. The tables are keyed by framebuffer address, so temporary surface views share the metadata without changing immutable asset structures. Each row stores one conservative interval: gaps between text fragments, and interior cuts, may also remain unfiltered until a larger redraw replaces the interval. Both contributing source rows are checked to avoid a blurred halo at the text boundary. Partial title updates use absolute source-column coordinates. No extra image buffer or heap allocation is required.

## Cores, audio and input

Core 0 runs the game, input, graphics and persistence. Core 1 generates DBOPL music using PRINCE.DAT's original instrument bank and mixes PCM/WAV. The mixer supports one MIDI stream plus two sample channels; the game adapter retains original sound priorities. Commands cross a SPSC queue with a fence.

PCM resources auto-detect the 7-byte DOS 1.0/1.1 header or 9-byte DOS 1.3/1.4 header (excluding the type byte). Candidate sample lengths are validated; if both layouts appear valid, a unique exact resource-length match is required. No PCM copy or additional audio buffer is allocated.

The 24 kHz mixer drives 120 kHz PWM at a 150 MHz system clock and TOP=1249. Five DMA words per PCM sample and error-feedback quantization preserve fractional PWM levels. Two chained DMA channels consume three buffers of 128 samples, plus an immutable silence buffer. The audio IRQ runs on core 1; LCD DMA is synchronous on core 0.

The southbridge is read through its physical matrix (0x0c) and arrow register (0x0d), not the character FIFO. Modifiers are applied first. Newly pressed discrete bit-7 buttons require confirmation because of transient BIOS scan states. Holding a key does not generate repeated press events; I2C failures release held inputs. F3 is intercepted before the game decoder. I/O/P remain movement keys even with physical Shift.

Original game timers remain intact.

## Persistence interfaces

- `src/port/storage.c`: SD-first selection, per-key fallback reconciliation and public APIs.
- `src/port/store.c`: record codec/CRC and two-sector flash journal.
- `src/port/sd_store.c`: alternating FAT32 snapshot files and readback verification.
- `src/port/fat32.c`: supplied FAT32 implementation with targeted fixes.
- `src/platform/picocalc/sdcard.c`: SPI0/card-detect hardware driver.
- `src/platform/picocalc/flash_store.c`: RP2350 checked ROM flash operations.

Both media use 256-byte snapshots: format magic at offset 0, sequence at 4, valid-key mask at 8, high-score fallback token at 12, 176-byte score payload at 16, 8-byte game save at 192, game-save fallback token at 200, and CRC32 over bytes 0–251 at 252. Numeric fields are little endian. The underlying record size/magic and payload positions remain compatible with the earlier flash journal.

Each new flash fallback gives the changed key a CRC-derived revision fingerprint. SD snapshots remember the last incorporated fingerprint for each key. If an unreadable card prevented its sequence number from being observed, the new flash key can still be recognized when the card returns. Only changed keys are merged; an unchanged old flash high-score payload does not replace a newer SD-only score. Sequence numbers select the latest valid record within each medium. CRC and fingerprints detect accidental damage/revisions, not malicious tampering.

Writes try SD first. Only a failed SD write causes a flash commit. Reads inspect flash metadata to detect outstanding fallback updates, then prefer/reconcile onto SD. Identical payloads are not rewritten unnecessarily. A successful save reports its storage backend. A failed migration does not destroy the readable flash fallback.

## SD layout and driver changes

The game creates `/Prince/STATE0.BIN` and `/Prince/STATE1.BIN`. Each file contains a complete 256-byte record in its own allocated cluster. The older/invalid slot is written while the latest valid slot is retained. The new file is closed, reopened, read and compared before success is reported. Unexpected directories, read-only files or oversized files at these paths are rejected rather than deleted. The game never formats the card.

The FAT32 driver supports 512-byte sectors, SDSC/SDHC, FAT32 MBR partitions and FAT32 volumes without an MBR. Unsupported formats fall back to flash. All filesystem work and card detection are synchronous on core 0; the supplied periodic unmount callback was removed to avoid interrupt-time mutation of active filesystem state. Each storage operation remounts to recover from card swaps. This adds initialization latency during saves/loads, not during normal frames. SPI0 uses GPIO16/17/18/19 and active-low detect GPIO22; SPI1 remains dedicated to the LCD.

Targeted fixes to the supplied drivers:

- Honor the SD programming-busy timeout using elapsed time; timeout now reports failure.
- Recognize a valid FAT32 boot sector before interpreting executable boot bytes as an MBR partition table.
- Subtract reserved sectors when calculating the data region; validate root cluster and FAT capacity.
- Keep all mirrored FAT copies consistent. Active-only/non-mirrored FAT volumes are explicitly rejected.
- Bound cluster accesses and wrap the free-cluster search around a stale FSInfo hint.
- Honor read-only attributes when writing.
- Propagate directory read failures instead of reporting them as missing files.
- Correct LFN/short-name placement when a directory entry spans clusters.
- Avoid reading beyond filename strings when filling LFN padding.
- Remove hardware/timer includes from the portable FAT32 layer and make its header self-contained.

These changes are present only in project copies; uploaded originals are unchanged. This remains a small FAT32 implementation, not a transactional filesystem. Alternating records cannot guarantee card metadata integrity if power is removed during directory/FAT updates or if the card's internal controller loses writes.

## Flash layout and cache correction

Pico 2W has 4 MiB physical flash. UF2 Loader 2.5 reserves the first 8 KiB physically, maps the application at XIP_BASE, and provides a smaller virtual application range. Firmware and fallback data stay within the application's first 2 MiB:

| Application offset | Runtime XIP address | Purpose |
|---|---|---|
| 0x000000–0x1fdfff | starting at 0x10000000 | Firmware/assets, linker checked |
| 0x1fe000–0x1fefff | 0x101fe000 | Journal sector A |
| 0x1ff000–0x1fffff | 0x101ff000 | Journal sector B |

The journal appends one complete record per 256-byte flash page. After 16 pages it erases the other sector, leaving the previous valid record intact. CRC rejects incomplete writes. A partially programmed next page causes a retry in the other sector. Duplicate snapshots do not consume another page.

For flash operations, core 1 parks at an audio-block boundary in an explicitly non-inlined SRAM function. Both audio DMA channels are disabled before aborting, preventing them from restarting one another. PWM holds its midpoint. Core 0 acquires the ROM flash lock, waits for core 1, disables its interrupts, and calls `ROM_FUNC_FLASH_OP` with **RUNTIME** address translation and Secure partition permissions. This uses the same checked ROM operation as SDK `rom_flash_op`, with the existing audio handshake instead of FIFO multicore lockout. Errors are returned; raw physical-offset erase/program calls are not used.

**After erase/program, the firmware explicitly calls `rom_flash_flush_cache()` before resuming interrupts/audio and verifying the record.** RP2350 checked ROM writes do not invalidate previously cached XIP bytes. The previous implementation omitted this step, allowing readback to see old data and report `SAVE FAILED` even after a write. This software defect is corrected; hardware confirmation remains required.

The linker's `.persistent_reset` section contains 8192 bytes of 0xff at the journal addresses and prevents firmware overlap. UF2 reinstall explicitly resets both flash sectors, including reinstalling the same version. SD files are outside the UF2 and survive. Flash tools that omit the reset section may behave differently.

Loader reference: [pelrun/uf2loader 2.5](https://github.com/pelrun/uf2loader/tree/5c44a4b64749062b0200507ceeff3ef2b475e288). The original three validation functions are included only in host tests. They reject the old out-of-partition UF2 and accept the corrected layout. The standard RP2350-E10 dummy block is retained and skipped correctly by the loader.

## RAM and verification limits

Core 0 has a reserved 32 KiB stack; core 1 has 4 KiB. The two framebuffers and the 164 KiB game arena are already counted in `.bss`. At least 2 KiB is reserved for the C-library heap. FAT32 uses fixed sector, FSInfo and LFN buffers; storage records are stack-local. The arena was reduced from 168 to 164 KiB to provide 4 KiB more headroom for SDK/compiler differences. Changes must still be checked against the linker map and assertions.

Inspect the generated linker map for exact section sizes in your build. The largest measured 64-bit host game-arena peak is 156,464 of 167,936 bytes. This is not an ARM stack measurement or a complete-room coverage guarantee. Hardware checks of SD behavior, flash fallback, power cycles, stack high-water marks and audio underruns remain necessary.


## Optional status bar

Tab cycles hidden -> battery -> battery and backlight percentages -> hidden. Startup is hidden. The portable `src/port/status.c` module draws directly into LCD rows 0–25, entirely above the 4:3 (y=40) and 16:10 (y=60) game images. It uses the existing immediate drawing/DMA path, not either game framebuffer. The extra persistent state is about 24 bytes; formatting uses a 16-byte stack buffer and the existing text renderer. The status renderer uses a dedicated native 7x11 ROM font with one-pixel stems and a 10-pixel advance, matching the 11-pixel battery outline. No row/column scaling is performed, avoiding uneven stroke thickness. The glyph table occupies 220 bytes in flash. Its temporary single-glyph buffer is 154 bytes on the stack; no framebuffer is added. Diagnostic text retains its original 10x14 glyphs.

Battery polling occurs on opening and every 60 seconds while visible. LCD and keyboard brightness are read on entering the expanded mode; there is no periodic brightness polling. Bracket presses decrease/increase keyboard brightness; Alt selects LCD brightness. Each distinct press steps by 32 for the keyboard (0–224) and 16 for the LCD (16–240). Stock BIOS rounds keyboard writes down to multiples of 32: the former 16-step request from zero was always rounded back to zero. Keyboard values above 240 after rounding wrap to zero, so 224 is the highest safe step. LCD writes round to multiples of 16 and clamp to 16–240. The readback remains authoritative. See the stock [backlight implementation](https://github.com/clockworkpi/PicoCalc/blob/f91519806d4b2e0a62c4638a9f695cd5162c5479/Code/picocalc_keyboard/backlight.ino) and [step constants](https://github.com/clockworkpi/PicoCalc/blob/f91519806d4b2e0a62c4638a9f695cd5162c5479/Code/picocalc_keyboard/conf_app.h). The controls work in all bar modes and are consumed before game input processing. Only changed values, mode changes, or a full LCD clear trigger redraws. Hidden mode performs no automatic telemetry reads. An explicit brightness adjustment reads the current register, writes the new value if needed, and confirms the actual register value. Read/write failures invalidate the cached value rather than displaying an unconfirmed setting. The adapter services the bar during input processing as well as frame presentation, so title and pause waits do not require an extra game redraw. All status work runs on core 0; it does not allocate memory or modify the core 1 audio path.

`PC_ReadStatusValue` separates the hardware queries from the UI. Checked southbridge reads use registers 0x0b (battery), 0x05 (LCD), and 0x0a (keyboard), and report errors separately from valid zero values. Battery bit 7 is a charging flag and is masked before displaying bits 0–6 as a percentage; invalid percentages are rejected. Backlight readbacks are converted to rounded percentages relative to LCD maximum 240 and keyboard maximum 224; both maxima display 100%. Out-of-range positive readings are capped at 100%, while failures remain `--%`. Labels are `LCD:` and `KEY:`. The charging flag itself is not displayed. The register interpretation follows [Blair Leduc's southbridge documentation](https://github.com/BlairLeduc/picocalc-text-starter/blob/a0fbba7be6fc4b115c1df57e1deed981a067b4cc/docs/southbridge.md) and battery command implementation. Error readings show `--%`. Battery errors retry at the next minute; brightness errors retry on another adjustment or when reopening the expanded view. Changes made outside these controls are picked up on reopening the expanded view or adjusting that backlight.

Status unit tests cover battery polling intervals, absence of periodic brightness polling, hidden controls, range limits, changed-only redraws, read/write failures and tick wraparound. Backlight test doubles model stock BIOS quantization and wraparound; regression tests cover keyboard startup from zero and repeated increases at maximum. The game harness exercises Tab repeats, all three modes, title input isolation and redraw after aspect changes, plus bracket/Alt input and repeat suppression. Device-reported accuracy and actual I2C/rendering latency still require hardware testing.
