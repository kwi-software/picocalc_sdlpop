#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-host
flags="-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined"
${CC:-cc} $flags -Isrc/include -Isrc/platform/picocalc src/tests/test_keyboard_matrix.c src/platform/picocalc/keyboard_matrix.c src/port/input.c -o build-host/test_keyboard_matrix
${CC:-cc} $flags -Isrc/include -Isrc/platform/picocalc src/tests/test_pwm_encode.c -o build-host/test_pwm_encode
ASAN_OPTIONS=detect_leaks=0 build-host/test_keyboard_matrix
ASAN_OPTIONS=detect_leaks=0 build-host/test_pwm_encode
${CC:-cc} $flags -Isrc/tests/mock_keyboard -Isrc/include -Isrc/platform/picocalc src/tests/test_keyboard_protocol.c src/platform/picocalc/southbridge.c src/platform/picocalc/keyboard_matrix.c src/port/input.c -o build-host/test_keyboard_protocol
ASAN_OPTIONS=detect_leaks=0 build-host/test_keyboard_protocol
