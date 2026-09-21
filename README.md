# Prince of Persia / SDLPoP for PicoCalc with Pico 2 / Pico 2 W

A PicoCalc game port based on SDLPoP commit `3c5add5fb7f83d4ceb542823ab66d00146c4271b`, with RGB565 LCD output, DMA audio, DBOPL music and SD/flash persistence.

**This is a source-only project. Original DAT files, generated game assets and prebuilt firmware are not included.** You must obtain the original game data yourself and compile the firmware.

## Prepare the game data

Use a complete English VGA DOS DAT set from Prince of Persia **1.0, 1.1, 1.3 or 1.4**. These sets have passed automated game, audio and firmware-build checks. Do not mix files from different releases. Other releases and modified data sets have not been validated.

1. Obtain the original DAT files from your own copy of the game or another source you are entitled to use.
2. Create `PrinceFiles` in the project root if it does not exist. The directory contains its own [file checklist](PrinceFiles/README.md).
3. Copy the DAT files directly into that folder, keeping their uppercase filenames. Do not put them in an extra subfolder. The DOS executable is not needed.

The build requires these files:

```text
PRINCE.DAT   KID.DAT      VDUNGEON.DAT  VPALACE.DAT
GUARD.DAT    GUARD1.DAT   GUARD2.DAT    FAT.DAT
SKEL.DAT     VIZIER.DAT   SHADOW.DAT    PV.DAT
TITLE.DAT    LEVELS.DAT   MIDISND1.DAT  MIDISND2.DAT
DIGISND1.DAT DIGISND2.DAT DIGISND3.DAT  IBM_SND1.DAT
IBM_SND2.DAT
```

Additional EGA/CGA/MT-32 files may stay in the folder but are not embedded. The build never downloads game data. Original game assets retain their original ownership and are not covered by this project's source-code licenses.

## Build

Requirements: PicoCalc with a Pico 2 or Pico 2 W, **Pico SDK** (tested: 2.2.0 and 2.3.1), Arm GNU Toolchain (tested: 13.3.Rel1 and 15.2.Rel1), CMake 3.20 or newer, Ninja and Python 3. Asset generation uses only the Python standard library.

### Automatic dependency setup and build

First place the required DAT files in `PrinceFiles`. Then run one of these commands from the project directory:

**Windows (PowerShell 5.1 or newer):**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 pico2w
# For the non-wireless board:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 pico2
```

Alternatively, double-click `build_pico2.cmd` or `build_pico2w.cmd`, or run the matching CMD file from Command Prompt. Each wrapper selects its board and locates `build.ps1` relative to itself, including paths with spaces. On failure it pauses so the error remains visible and returns the PowerShell exit code.

The execution-policy option applies only to that process; managed policies may still prohibit scripts.

**Linux:**

```sh
bash build.sh pico2w
# For the non-wireless board:
bash build.sh pico2
```

Omit the board argument to choose interactively. The scripts accept `pico2` and `pico2w`; the latter is translated to the SDK's `pico2_w` identifier.

| Argument | Output |
| --- | --- |
| `pico2` | `Firmware/Prince_Pico2.uf2` |
| `pico2w` | `Firmware/Prince_Pico2w.uf2` |

The scripts check Python, CMake, Ninja, the Arm compiler/newlib, Pico SDK and native Pico tools. Suitable PATH tools and compatible Pico VS Code installations are reused. Missing build tools are downloaded into `%LOCALAPPDATA%\PrinceTools` on Windows or `.prince-tools` in this project on Linux; system PATH and existing SDK installations are not changed. SDK/picotool are pinned to 2.3.1. The fallback downloads use Arm GNU 13.3.Rel1, CMake 3.31.6 and Ninja 1.12.1 (1.13.1 on Linux Arm64). Downloads come from [Raspberry Pi](https://github.com/raspberrypi/pico-sdk-tools/releases), [Kitware](https://github.com/Kitware/CMake/releases), [Ninja](https://github.com/ninja-build/ninja/releases), [Arm](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) and [Python](https://www.python.org/downloads/release/python-31210/). GitHub-provided SHA-256 digests are checked when present.

Windows can download a local embeddable Python 3.12.10 if no Python 3.9+ is available; it needs no administrator account. Linux installs missing Python and CA certificates through apt, dnf, pacman or zypper, using sudo when necessary. Run the Linux script as your normal user. Supported download hosts are 64-bit Windows and glibc-based Linux x86_64/aarch64; other systems can use the manual build below. On Windows Arm64 the downloaded Windows tools require x64 emulation. On older Linux installations, vendor tools may require newer shared libraries; an execution failure is reported before the firmware build where possible.

The firmware does not use USB stdio, Wi-Fi or Bluetooth, so the downloaded SDK archive needs none of those optional submodules. Prebuilt `picotool` and `pioasm` are checked through CMake before configuring the game; no native Windows C/C++ compiler is needed for them.

Each board uses its own `build-pico2` or `build-pico2w` directory. A fresh CMake configuration prevents stale SDK/board settings while retaining compiled outputs where possible. The UF2 is checked and copied to `Firmware` only after a successful build; a failed build leaves any previous output there untouched. The scripts build the firmware but do not flash the device. Downloads are cached for later runs. Internet access is required for missing dependencies; Downloads honor `HTTPS_PROXY`/`HTTP_PROXY`. On Windows, dependency downloads use PowerShell/.NET and Windows certificate-chain validation; without an explicit proxy variable they use the Windows default proxy settings. Certificate verification stays enabled. If Windows also rejects a certificate, fix the Windows trust chain or install the legitimate proxy CA through your administrator. If a corporate proxy blocks downloads, prepare the dependencies on an allowed connection or use the manual setup.

Missing DAT files are reported before downloading the main toolchain. No game data is downloaded. To remove cached downloads and tools, delete `%LOCALAPPDATA%\PrinceTools` on Windows or `.prince-tools` on Linux; the scripts recreate the cache as needed. Set `PRINCE_TOOLS_PATH` to override the build-tool cache location, preferably a short local path. Windows uses extended paths while extracting archives and removes the redundant outer Arm toolchain directory to keep compiler paths short. Completed downloads in an older project-local `.prince-tools/downloads` directory are reused automatically; keep that directory until the next run finishes. The small Windows Python bootstrap still lives in the project-local `.prince-tools` directory. Both `.prince-tools` and `Firmware` are ignored by Git.

### Select the board

The port does not use Wi-Fi, Bluetooth or the CYW43 library, so the Pico 2 without wireless is also an intended target. Select the matching SDK board when configuring:

| Installed board | CMake option |
| --- | --- |
| Pico 2 W | `-DPICO_BOARD=pico2_w` (default) |
| Pico 2 | `-DPICO_BOARD=pico2` |

The commands below use `pico2_w`; replace it with `pico2` for the non-wireless board. When switching boards, use a new build directory or configure with `--fresh` (CMake 3.24+) and reapply your options. Physical validation to date concerns the Pico 2 W; the Pico 2 has not yet been tested on hardware for this port.

### Windows with the Raspberry Pi Pico VS Code installation

From the project root in Command Prompt, after populating `PrinceFiles`:

```bat
"%USERPROFILE%\.pico-sdk\cmake\v4.3.4\bin\cmake.exe" -S . -B build -G Ninja -DPICO_BOARD=pico2_w -DCMAKE_BUILD_TYPE=Release
"%USERPROFILE%\.pico-sdk\cmake\v4.3.4\bin\cmake.exe" --build build --target prince_picocalc -j4
```

Adjust the CMake version in the command to your installation. The optional `%USERPROFILE%/.pico-sdk/cmake/pico-vscode.cmake` helper is loaded before SDK import, with its required `USERHOME` variable. Defaults are SDK/picotool 2.3.1 and toolchain 15_2_Rel1; override `sdkVersion`, `picotoolVersion` and `toolchainVersion` with `-D` as needed. This locates the installed native picotool instead of trying to compile it with a missing Windows host compiler.

An explicit `-DPICO_SDK_PATH=...` takes precedence over the `PICO_SDK_PATH` environment variable, which takes precedence over the helper default. To recover from an old CMake configuration, add `--fresh` to the configure command (CMake 3.24+). Reapply any custom `-D` options, including `PRINCE_FILES`.

### Linux or a manually installed SDK

Make the Arm toolchain, CMake, Ninja and Python available in PATH, then run:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -G Ninja -DPICO_BOARD=pico2_w -DCMAKE_BUILD_TYPE=Release -DPRINCE_USE_PICO_VSCODE=OFF
cmake --build build --target prince_picocalc -j4
```

Install the SDK's required submodules. Use a compatible native picotool installation, or provide a native C/C++ compiler if the SDK builds picotool from source. The native picotool must run on your computer; `arm-none-eabi-gcc` is for the firmware.

The output is **`build/prince_picocalc.uf2`**. The linker map is generated alongside it. Neither is distributed with this project.

CMake converts your DAT files into immutable C/header assets under `build/src/generated/`. Changed DAT files trigger regeneration. Missing or invalid input stops the build. Use `-DPRINCE_FILES=/absolute/path/to/DATs` to choose another data directory. PCM sound formats are detected automatically.

The game arena reserves 164 KiB; core 0 has a 32 KiB stack and core 1 has 4 KiB. See `docs/BUILD_COMPATIBILITY.md` for the SDK/compiler memory checks.

## Install the firmware you built

For **PicoCalc UF2 Loader 2.5**, copy the matching `Firmware/Prince_Pico2.uf2` or `Firmware/Prince_Pico2w.uf2` (manual build: `build/prince_picocalc.uf2`) to `/pico2-apps/` on the SD card and select it in the loader. Alternatively, use BOOTSEL and copy the UF2 to the Pico's USB mass-storage drive.

To resume the installed game, power on normally or select its loader entry in **square brackets**. Selecting the UF2 file again reinstalls it and clears the flash fallback. **SD saves and scores survive firmware installation.**

All game graphics, levels and sounds are embedded when compiling. Do not copy DAT files to the PicoCalc SD card for runtime loading: the game uses that card only for settings, scores and the saved game.

## Controls

| Key | Action |
|---|---|
| Enter | Start the game |
| F1 | Toggle 4:3 / 16:10; default 4:3 |
| Tab | Cycle status bar: hidden (default), battery, battery + display/keyboard brightness |
| `[` / `]` | Decrease / increase keyboard backlight |
| `Alt+[` / `Alt+]` | Decrease / increase display backlight |
| Ctrl+F | Toggle vertical filtering in 4:3; off by default, ignored in 16:10 |
| F2 | Save game, preferring SD |
| Alt+F2 | Save current settings, preferring SD |
| Alt+Delete | Clear saved settings, high scores and game save; restore defaults |
| F3 | Toggle Megahit cheats; off by default |
| F4 | Load saved game, including from the title screen |
| Shift, 1 or F5 | Action: grab, pick up items, fight, walk carefully |
| Left / Right or I / P | Move |
| Up or 9 | Jump / climb |
| Down or O | Crouch / drop |
| 8 / 0 | Up+Left / Up+Right |
| Shift / 1 / F5 + Left/Right or I/P | Walk carefully |
| Esc | Pause; another key resumes |
| Space | Show remaining time |
| Ctrl+A | Restart level |
| Ctrl+R | Return to title |
| Ctrl+S | Toggle sound |
| Ctrl+G / Ctrl+L | Save / load, in addition to F2/F4 |

The 4:3 mode offers optional vertical area-weighted filtering to reduce uneven steps on diagonal edges. Text regions, including title lettering and the minutes display, use unfiltered scaling for readability. Ctrl+F disables/enables the filter for the remaining picture in 4:3. This setting survives aspect-ratio changes and can be saved with Alt+F2; without saved settings it starts disabled; Ctrl+F has no effect in 16:10. The 16:10 mode remains pixel-exact. LCD transfers remain two-byte RGB565.

The optional status bar occupies the upper LCD margin without enlarging the game framebuffers. Battery charge is read when shown and every 60 seconds thereafter; brightness is read when entering the expanded view and after adjustments, with no periodic brightness polling. The bracket keys work even while the bar is hidden. Each press changes keyboard brightness by 32 and display brightness by 16, matching stock BIOS steps. The keyboard range is 0–224 and the display range is 16–240. These caps prevent BIOS rounding or wraparound from turning the keyboard light off. Brightness percentages are relative to each usable maximum: keyboard 224 and display 240 both show 100%. The native 11-pixel-high status text uses the labels `LCD:` and `KEY:` and matches the battery icon height. Values are displayed as percentages; unavailable readings show `--%`. Hidden status does not automatically poll the device, and unchanged values do not cause redraws.

There is no help overlay. Tab, F1, F3 and Ctrl+F do not skip the title sequence. F5 and 1 are independent action inputs, not text modifiers. Releasing one action key does not cancel another held action key.

### Cheat keys after F3

| Key | Cheat |
|---|---|
| L | Next level, retaining the original level-skip time adjustment |
| R | Revive after death |
| K | Kill the guard, except a skeleton |
| S | Restore one health point |
| T | Increase maximum health |
| = / - | Add / subtract one minute |
| W | Feather fall |
| G | Toggle upside-down view |
| D | Toggle blind mode |
| H / J | View the room to the left / right |
| U / N | View the room above / below |
| B | Return the view to the prince's room |
| C | Show room and direct neighbors |
| V | Show additional neighboring rooms |

Room-view cheats move the view, not the prince. F3 disables the additional mappings again. Original Ctrl shortcuts remain available. SDLPoP's optional debug timer displays are disabled.

High-score names: hold either physical Shift key for uppercase letters; release it for lowercase.

## SD-first storage

The game creates `/Prince` automatically on a supported, writable FAT32 card. It alternates between:

- `/Prince/STATE0.BIN`
- `/Prince/STATE1.BIN`

Each checksummed snapshot contains the settings, six-entry high-score table and saved game. Keep both files when making a backup. They are the port's binary snapshot format, not standalone DOS PRINCE.SAV/PRINCE.HOF files.

Successful saves report **GAME SAVED (SD)** or **GAME SAVED (FLASH)**. For ordinary saves, flash is written only when SD saving fails: for example, no card, unsupported filesystem, read/write failure, read-only file, or insufficient space. FAT32 with an MBR or without a partition table is supported; GPT, exFAT, FAT16 and active-only/non-mirrored FAT configurations are not supported by this driver.

Flash fallback changes are reconciled back onto SD when the card becomes usable. Settings, scores and game saves have separate fallback revision markers, so an older flash score does not overwrite a newer SD-only score when a game save falls back to flash. Valid snapshots are checked before use and verified after writing. Alternating files protect the previous snapshot against an incomplete data write; FAT32 metadata and the card controller are not transactional. Do not remove the card or power during a save.

The implementation adapts Blair Leduc's `sdcard.c/.h` and `fat32.c/.h`, with fixes documented in `docs/INTEGRATION.md`. SPI0 uses GPIO16–19; card detect is active-low GPIO22. The LCD remains on SPI1. Storage runs on core 0; no FAT operations run in an interrupt callback.

### Saved-game behavior

F2 saves in levels 1–14. It preserves the original game's level-start save semantics: level, remaining time and health capacity at the start of that level. **Loading restarts the saved level; it does not restore the exact room, position or animation frame.** There is one save slot. Titles and cutscenes are not saved. The high-score table is saved when the original game updates it.

The flash fallback survives reset and power-off. Reinstalling this UF2 clears only the flash journal; SD files remain intact. Use **Alt+Delete** to clear settings, scores and the saved game together. This writes an empty snapshot and a reset marker to flash, even when SD works, then updates SD. It is a logical reset, not a secure erase or deletion of the snapshot files. If the card is absent or unwritable, the message **CLEARED - SD SYNC PENDING** is shown; old data on that card is ignored and replaced on the next successful storage access. Keep the reset marker in flash until synchronization completes: reinstalling the UF2 first removes that protection. The running level continues; its stored save is removed.

### Saved settings

Press **Alt+F2** to save the current aspect ratio, filter preference, sound on/off, cheats on/off, status-bar mode (hidden/battery/full), LCD backlight and keyboard backlight. Settings load automatically before the title sequence. Saving is explicit; later changes remain temporary until Alt+F2 is pressed again. A short message reports **SETTINGS SAVED SD**, **SETTINGS SAVED FLASH** or a failure. Backlight read failures prevent saving an incomplete settings record.

The filter preference is retained in 16:10 but only affects 4:3. Defaults are 4:3, filter off, sound on, cheats off and status bar hidden. Alt+Delete immediately restores those defaults and sets LCD backlight to raw 176 (73%) and keyboard backlight to off. Without saved settings, startup leaves the existing hardware backlight levels unchanged. Older snapshots without a settings entry remain readable.

## Source layout

| Path | Contents |
|---|---|
| `PrinceFiles/` | Your own original DOS DAT files and a file checklist; only the checklist is tracked by Git |
| `src/game/` | Game adapter, entry point, allocator and linker script |
| `src/game/upstream/` | Adapted SDLPoP core |
| `src/game/compat/` | Minimal SDL2 source compatibility layer |
| `src/include/` | Shared public and storage interface headers |
| `src/port/` | Portable graphics, input, assets, FAT32 and storage policy |
| `src/audio/` | MIDI/OPL2 mixer and PCM/WAV playback |
| `src/platform/picocalc/` | LCD, keyboard, SD SPI, audio DMA and flash hardware |
| `src/platform/host/` | Host test backends |
| `src/third_party/dbopl/` | OPL2 emulation |
| `src/tools/` | DAT converter |
| `src/tests/` | Game, driver, codec, loader and persistence tests |
| `docs/` | Integration notes, build compatibility and Pico SDK license |

All supplied C/header files are under `src/`. Generated asset C/header files go under `<build>/src/generated/`, not into the source tree.

## Tests

Run asset-dependent tests only after providing your own DAT files:

```sh
python3 src/tests/test_dat.py
sh src/tests/run_store_tests.sh
sh src/tests/run_sd_tests.sh
sh src/tests/run_hardware_logic_tests.sh
sh src/tests/run_video_tests.sh
sh src/tests/run_status_tests.sh
python3 src/tests/run_game_tests.py
sh src/tests/run_sound_dat_tests.sh /absolute/path/to/DATs 1
python3 src/tests/check_firmware.py build/prince_picocalc.elf
sh src/tests/run_loader25_tests.sh build/prince_picocalc.uf2
```

For the sound test, use layout `1` for DOS 1.0/1.1 and `2` for 1.3/1.4. Host tests require GCC, CMake and Ninja. For host-game tests with an external DAT folder, first configure `build-host-game` with `-DPRINCE_HOST=ON -DPRINCE_FILES=/absolute/path/to/DATs`. The DAT parser unit test uses `PrinceFiles`. ELF inspection also needs `arm-none-eabi-nm` and `arm-none-eabi-objdump` in PATH. SD tests use disposable disk images, not physical drives.

The host-game suite includes a 150,000-frame title/demo soak test and compares heap usage at matching title phases after warm-up. It takes longer than the short control tests.

Automated checks do not constitute a complete playthrough or hardware validation of every feature. Desktop menus, replays, arbitrary-position quicksaves and smooth palette fades are not included.

## Authors

PicoCalc port maintained by Karl Wintermann.

## Acknowledgements

- Thank you to **Jordan Mechner and the original Prince of Persia development team** for a wonderful game that remains a joy to play.
- Thank you to the **[SDLPoP developers](https://github.com/NagyD/SDLPoP)** for their outstanding reverse-engineering work and continued development, which made this port possible.
- Thank you to **[Blair Leduc](https://github.com/BlairLeduc/picocalc-text-starter)** for `picocalc-text-starter`, the source/reference for LCD, keyboard/southbridge, SD-card and FAT32 driver work used in this port.
- Thank you to the **[DOSBox developers](https://www.dosbox.com/)** for the DBOPL emulator, and to the **[Chocolate Doom developers](https://github.com/chocolate-doom/chocolate-doom/)** for its C port. The code here was compared with Chocolate Doom 2.2.0 and includes local streaming-reset and portability changes.
- Thank you to **ChatGPT** for the extensive hands-on assistance with implementation, debugging, testing and documentation throughout this port.

## Licenses and provenance

See `COPYING` for the SDLPoP-derived GPL-3.0-or-later game code, `COPYING.DBOPL` for DBOPL's GPL-2.0-or-later license, and `COPYING.PICOCALC` for Blair Leduc's MIT license. Original copyright notices are retained. Component details and local modifications are recorded in `ASSET_SOURCES.md`; original game data are not redistributed.
