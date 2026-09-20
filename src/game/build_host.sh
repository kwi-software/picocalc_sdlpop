#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
cmake -S . -B build-host-game -G Ninja -DPRINCE_HOST=ON -DPRINCE_SANITIZE=ON
cmake --build build-host-game -j4
