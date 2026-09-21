cmake_minimum_required(VERSION 3.20)
option(PRINCE_HOST "Build the deterministic host test harness instead of firmware" OFF)
if(NOT PRINCE_HOST)
  # Keep an explicit SDK selection: the VS Code helper assigns this variable.
  set(prince_requested_sdk "${PICO_SDK_PATH}")
  if(NOT prince_requested_sdk AND NOT "$ENV{PICO_SDK_PATH}" STREQUAL "")
    set(prince_requested_sdk "$ENV{PICO_SDK_PATH}")
  endif()
  # Pico VS Code installations provide SDK/toolchain paths and a native picotool.
  # Load this before the SDK import and project(), as in generated Pico projects.
  option(PRINCE_USE_PICO_VSCODE "Use an installed Pico VS Code CMake helper" ON)
  if(PRINCE_USE_PICO_VSCODE)
    if(CMAKE_HOST_WIN32)
      set(prince_sdk_home "$ENV{USERPROFILE}")
    else()
      set(prince_sdk_home "$ENV{HOME}")
    endif()
    set(PRINCE_PICO_VSCODE_FILE "${prince_sdk_home}/.pico-sdk/cmake/pico-vscode.cmake"
      CACHE FILEPATH "Optional Pico VS Code CMake helper")
    if(EXISTS "${PRINCE_PICO_VSCODE_FILE}")
      set(sdkVersion "2.3.1" CACHE STRING "Pico VS Code SDK version")
      set(toolchainVersion "15_2_Rel1" CACHE STRING "Pico VS Code toolchain version")
      set(picotoolVersion "2.3.1" CACHE STRING "Pico VS Code picotool version")
      # USERHOME is part of the helper's interface, not an arbitrary local name.
      set(USERHOME "${prince_sdk_home}")
      include("${PRINCE_PICO_VSCODE_FILE}")
    endif()
  endif()
  set(PICO_BOARD pico2_w CACHE STRING "Target board")
  if(prince_requested_sdk)
    set(PICO_SDK_PATH "${prince_requested_sdk}")
  endif()
  if(PICO_SDK_PATH)
    file(TO_CMAKE_PATH "${PICO_SDK_PATH}" PICO_SDK_PATH)
  endif()
  if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Pico SDK import not found at '${PICO_SDK_PATH}/external/pico_sdk_import.cmake'. Set -DPICO_SDK_PATH to your SDK directory (tested: 2.2.0 and 2.3.1).")
  endif()
  include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
endif()
project(prince_picocalc C CXX ASM)
set(CMAKE_C_STANDARD 11)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PRINCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/PrinceFiles" CACHE PATH "Original PoP DOS DAT directory (1.0/1.1/1.3/1.4)")
set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/src/generated")
set(DAT_GROUPS PRINCE KID VDUNGEON VPALACE GUARD GUARD1 GUARD2 FAT SKEL VIZIER
  SHADOW PV TITLE LEVELS MIDISND1 MIDISND2 DIGISND1 DIGISND2 DIGISND3 IBM_SND1 IBM_SND2)
set(DAT_INPUTS)
foreach(group IN LISTS DAT_GROUPS)
  set(dat "${PRINCE_FILES}/${group}.DAT")
  if(NOT EXISTS "${dat}")
    message(FATAL_ERROR "Required original PoP DOS DAT missing: ${dat}. Obtain the original game data separately and place the required DAT files in PrinceFiles, or set PRINCE_FILES. See README.md.")
  endif()
  list(APPEND DAT_INPUTS "${dat}")
endforeach()
add_custom_command(OUTPUT "${GENERATED}/game_assets.c" "${GENERATED}/game_assets.h" "${GENERATED}/game_assets.json"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pack_dat.py" "${PRINCE_FILES}" "${GENERATED}"
  DEPENDS ${DAT_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pack_dat.py"
  COMMENT "Converting PrinceFiles DATs into immutable asset headers" VERBATIM)
file(GLOB POP_CORE CONFIGURE_DEPENDS src/game/upstream/*.c)
set(GAME_SOURCES ${POP_CORE} src/game/portable.c src/game/adapter.c src/game/game_memory.c
  "${GENERATED}/game_assets.c"
  src/port/status.c src/port/surface.c src/port/input.c src/port/resources.c src/port/assets.c src/port/game_hooks.c src/port/store.c src/port/storage.c
  src/audio/mixer.c src/audio/midi.c src/audio/wave.c src/third_party/dbopl/dbopl.c
  src/platform/picocalc/keyboard_matrix.c)
set(GAME_INCLUDES "${GENERATED}" src/game/compat src/game/upstream src/game src/include
  src/port src/audio src/third_party/dbopl src/platform/picocalc)
if(PRINCE_HOST)
  add_executable(prince ${GAME_SOURCES} src/tests/host.c src/platform/host/flash_store.c src/platform/host/sd_store.c)
  target_include_directories(prince PRIVATE ${GAME_INCLUDES})
  target_compile_definitions(prince PRIVATE PICOCALC PC_GAME_HOST)
  target_compile_options(prince PRIVATE -O1 -g -fwrapv -ffunction-sections -fdata-sections
    "$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration;-Werror=incompatible-pointer-types>")
  option(PRINCE_SANITIZE "ASan and UBSan in host tests" ON)
  if(PRINCE_SANITIZE)
    target_compile_options(prince PRIVATE -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all)
    target_link_options(prince PRIVATE -fsanitize=address,undefined)
  endif()
  target_link_options(prince PRIVATE -Wl,--gc-sections)
  target_link_libraries(prince m)
else()
  pico_sdk_init()
  add_executable(prince_picocalc ${GAME_SOURCES} src/game/main.c
    src/platform/picocalc/platform.c src/platform/picocalc/audio_dma.c
    src/platform/picocalc/video_dma.c src/platform/picocalc/southbridge.c
    src/platform/picocalc/flash_store.c src/platform/picocalc/sdcard.c
    src/port/fat32.c src/port/sd_store.c)
  target_include_directories(prince_picocalc PRIVATE ${GAME_INCLUDES})
  target_compile_definitions(prince_picocalc PRIVATE PICOCALC PICO_STACK_SIZE=0x8000 PICO_CORE1_STACK_SIZE=0x1000)
  target_compile_options(prince_picocalc PRIVATE -O3 -fwrapv -ffunction-sections -fdata-sections
    "$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration;-Werror=incompatible-pointer-types>")
  target_link_options(prince_picocalc PRIVATE -Wl,--gc-sections)
  target_link_libraries(prince_picocalc pico_stdlib pico_multicore hardware_spi hardware_i2c
    hardware_dma hardware_pwm hardware_irq hardware_clocks hardware_flash m)
  pico_set_linker_script(prince_picocalc "${CMAKE_CURRENT_LIST_DIR}/src/game/memmap_game.ld")
  pico_enable_stdio_usb(prince_picocalc 0)
  pico_enable_stdio_uart(prince_picocalc 0)
  pico_add_extra_outputs(prince_picocalc)
endif()
