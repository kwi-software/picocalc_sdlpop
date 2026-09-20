#!/bin/sh
# Usage: sh src/tests/run_sound_dat_tests.sh /path/to/DATs 1|2
# PCM layout 1: DOS 1.0/1.1; layout 2: DOS 1.3/1.4.
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
${CC:-cc} -std=c11 -O2 -g -fwrapv -fsanitize=address,undefined -fno-sanitize=shift \
  -Isrc/include -Isrc/audio -Isrc/third_party/dbopl \
  src/tests/test_sound_dats.c src/port/resources.c src/audio/mixer.c \
  src/audio/midi.c src/audio/wave.c src/third_party/dbopl/dbopl.c \
  -lm -o build-host/test_sound_dats
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 build-host/test_sound_dats "${1:-PrinceFiles}" "${2:-1}"
