#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/include src/tests/test_store.c src/port/store.c src/port/storage.c src/platform/host/sd_store.c -o build-host/test_store
ASAN_OPTIONS=detect_leaks=0 build-host/test_store
