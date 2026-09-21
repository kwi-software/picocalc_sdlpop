#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Isrc/tests/mock -Isrc/include -Isrc/platform/picocalc \
 src/tests/test_video.c src/port/surface.c src/platform/picocalc/video_dma.c -o build-host/test_video
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build-host/test_video
