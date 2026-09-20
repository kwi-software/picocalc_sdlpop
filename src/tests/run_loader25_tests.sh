#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror src/tests/test_loader25.c -o build-host/test_loader25
build-host/test_loader25 "${1:-build/prince_picocalc.uf2}"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/tests/mock_flash -Isrc/include src/tests/test_flash_backend.c \
  src/platform/picocalc/flash_store.c -o build-host/test_flash_backend
ASAN_OPTIONS=detect_leaks=0 build-host/test_flash_backend
