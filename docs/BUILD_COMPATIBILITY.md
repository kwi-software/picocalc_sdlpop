# SDK/toolchain compatibility fix

The user reported a 40-byte static RAM overflow with Pico SDK 2.3.1 and Arm GNU Toolchain 15.2.Rel1 on Windows. The earlier firmware was built with SDK 2.2.0 / GCC 13.3.Rel1 and had very little RAM headroom.

The fixed game arena is now 164 KiB instead of 168 KiB. This releases 4096 bytes of static RAM. The 32 KiB core-0 stack, 4 KiB core-1 stack, framebuffers and minimum 2 KiB C-library heap remain unchanged. The arena still has 11,472 bytes above the largest measured host allocation peak (156,464 bytes). These tests do not prove that every room or modified DAT set fits.

CMake applies implicit-function-declaration and incompatible-pointer-types errors only to C sources, so SDK C++ sources no longer receive unsupported warning flags.

## Validation

- Release firmware built under Linux with Pico SDK 2.3.1 (079c6f39023649b154152db30f1d781e884879bc) and Arm GNU Toolchain 15.2.Rel1 / GCC 15.2.1.
- Static RAM including the 2048-byte minimum heap ends at 0x20077028. The core-0 stack begins at 0x20078000, leaving 4056 bytes of headroom. Adding the former 4096 arena bytes reproduces the reported 40-byte overlap arithmetically.
- Eleven ASan/UBSan game scenarios pass with the reduced arena, including levels 1–14, title/attract sequences, input, cheats and persistence. This is not exhaustive gameplay coverage.
- The ELF flash-pause placement check and explicit UF2 flash-reset checks pass.
- Original UF2 Loader 2.5 validation accepts all 6448 application blocks.
- The flash backend model passes direct/relocated addressing and cleanup tests.

The UF2 and map used for those validation runs were built with this configuration; neither is included in the source distribution. Windows execution and physical hardware were not tested here. Separately obtained DOS 1.0 DATs were used; a different asset payload size is not the static RAM linker error, but modified assets may change runtime allocations.

## Windows native picotool discovery

A Pico VS Code installation's optional `pico-vscode.cmake` helper is now included before SDK import and `project()`. It supplies the installed native picotool as well as SDK/toolchain paths. Version defaults match the user's working setup (SDK/picotool 2.3.1, toolchain 15_2_Rel1), are cache-configurable and do not use FORCE. Paths are quoted for spaces. The helper is excluded from host-game builds and can be disabled with `PRINCE_USE_PICO_VSCODE=OFF`.

The reported missing C/C++ compiler was in the native picotool sub-build, not the firmware cross-build. The README includes fresh-cache commands for recovering from this configuration. Firmware reconfiguration/build without the helper was checked under Linux; the Windows helper behavior is based on the user's confirmed working setup and was not executed on Windows here. No game or firmware code changed in this update.

### SDK path follow-up fix

The helper uses `USERHOME` directly to assign SDK, toolchain and picotool paths. The first integration incorrectly used only a project-local variable, so including the helper produced an invalid SDK path even when the environment variable was correct. The project now supplies `USERHOME` before inclusion. It also preserves nonempty explicit CMake SDK paths, then environment SDK paths, ahead of the helper's default. Empty cached paths fall back to the environment. Missing-SDK errors include the actual import path.

Validated with Raspberry Pi's actual `scripts/pico-vscode.cmake` from the pico-vscode repository: default discovery, environment override, explicit path precedence and empty-variable recovery, including simulated Windows profile paths containing spaces. Both SDK and installed picotool paths were asserted. Linux firmware reconfiguration/build passes. These script checks do not execute a Windows compiler.
