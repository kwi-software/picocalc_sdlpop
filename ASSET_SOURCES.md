# Sources, attribution and reproducibility

## Game code

[NagyD/SDLPoP](https://github.com/NagyD/SDLPoP/tree/3c5add5fb7f83d4ceb542823ab66d00146c4271b), commit `3c5add5fb7f83d4ceb542823ab66d00146c4271b`. Imported files are in `src/game/upstream/`; `src/game/upstream-changes.patch` records their changes. `portable.c` adapts routines from seg009. License: GPL-3.0-or-later, see `COPYING` and the original source notices.

## Game data: not distributed

No original DAT files, generated asset arrays or compiled firmware are distributed in this source package. Obtain your own complete English VGA DOS 1.0, 1.1, 1.3 or 1.4 data set and place its DAT files in `PrinceFiles/`, or configure `PRINCE_FILES`. CMake invokes `src/tools/pack_dat.py` to generate immutable assets in the build directory. Neither CMake nor the converter downloads game data.

Prince of Persia and its resources belong to Jordan Mechner and their respective original rights holders. Conversion does not make these assets public domain, and source-code licenses do not grant rights to the original game data.

The font comes from SDLPoP's built-in bitmap font. Music uses PRINCE.DAT resource 1 for the instrument bank; MIDI and PCM resources come from the user's sound DATs.

## PicoCalc driver origins

[Blair Leduc / picocalc-text-starter](https://github.com/BlairLeduc/picocalc-text-starter), inspected at commit `a0fbba7be6fc4b115c1df57e1deed981a067b4cc`. Upstream license: MIT, copyright (c) 2025 Blair Leduc. The complete notice is included in `COPYING.PICOCALC`.

The user-provided SD-card, FAT32 and southbridge files were compared with that upstream snapshot. They retain extensive matching code and have been adapted in:

- `src/platform/picocalc/sdcard.c` and `.h`: SPI SD block access and write timeout/error handling.
- `src/port/fat32.c` and `.h`: filesystem integration; bounds, FAT mirroring, allocation, directory-boundary and error-propagation fixes described in `docs/INTEGRATION.md`.
- `src/platform/picocalc/southbridge.c` and `.h`: keyboard and peripheral I2C access, with port-specific synchronization and input changes.

The LCD work in `src/platform/picocalc/video_dma.c` follows the starter/user-provided LCD initialization reference. The port adds its own 16-bit RGB565 DMA scanout and retains the hardware-tested initialization. The starter's text-display implementation is not imported wholesale. Keyboard/southbridge reference work is credited to Blair Leduc; the separate physical matrix table retains its existing ClockworkPi attribution. The comparison snapshot establishes provenance, not an assertion that every user-supplied file was copied from precisely that commit.

MIT notices are included in adapted source files and the full upstream license is retained. This does not change the license of the SDLPoP-derived game or of other independently licensed components.

## DBOPL

The DBOPL emulator is copyright (C) 2002–2010 The DOSBox Team, licensed GPL-2.0-or-later; see `COPYING.DBOPL` and the original notices in `src/third_party/dbopl/`.

The C implementation was compared with [Chocolate Doom 2.2.0, opl/dbopl.c and opl/dbopl.h](https://github.com/chocolate-doom/chocolate-doom/tree/chocolate-doom-2.2.0/opl), commit `a538c179abe588e79cc45c20dad25706daa826e4`. The retained upstream comments identify DOSBox revision r3635 and describe the C conversion using Chocolate Doom's minus-minus script followed by manual changes. Differences here are the added streaming `Chip__Reset`, portable `offsetof` calculations, and the header guard. Original copyright and licensing comments remain intact. Thanks go to both the DOSBox developers and the Chocolate Doom contributors.

## SDK and loader

The customized linker script is based on Pico SDK 2.2.0; see `docs/PICO-SDK-LICENSE.TXT`. Target: Pico 2W / RP2350, 4 MiB physical flash.

Loader compatibility tests retain three validation functions from [pelrun/uf2loader 2.5](https://github.com/pelrun/uf2loader/tree/5c44a4b64749062b0200507ceeff3ef2b475e288), `ui/uf2.c`, GPL-3.0. Attribution is in `src/tests/loader25_checks.h`; these functions are compiled only into host tests.

The flash-cache correction was checked against Raspberry Pi's [RP2350 boot ROM source](https://github.com/raspberrypi/pico-bootrom-rp2350), `varm_checked_flash.c` and `varm_generic_flash.c`. The port explicitly flushes the ROM flash cache before readback after checked erase/program operations.
