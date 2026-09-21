#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Isrc/include src/tests/test_status.c src/port/status.c -o build-host/test_status
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build-host/test_status
