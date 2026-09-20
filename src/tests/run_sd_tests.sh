#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -D_POSIX_C_SOURCE=200809L -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Isrc/include -Isrc/port -Isrc/platform/picocalc src/tests/test_sd_store.c \
  src/port/fat32.c src/port/sd_store.c src/port/storage.c src/port/store.c -o build-host/test_sd_store
for mode in superfloppy mbr full; do
  python3 src/tests/make_fat32_image.py build-host/sd-test.img "$mode"
  if [ "$mode" = full ]; then
    ASAN_OPTIONS=detect_leaks=0 build-host/test_sd_store build-host/sd-test.img full
  else
    ASAN_OPTIONS=detect_leaks=0 build-host/test_sd_store build-host/sd-test.img
    ASAN_OPTIONS=detect_leaks=0 build-host/test_sd_store build-host/sd-test.img verify
  fi
done
