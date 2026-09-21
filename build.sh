#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Bootstrap Python only; the shared builder manages all other tools locally.
set -euo pipefail
prince_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != pico2 && "$1" != pico2w && "$1" != --help ) ]]; then
    echo 'Usage: bash build.sh [pico2|pico2w]' >&2; exit 2
fi
if [[ ${1:-} == --help ]]; then
    echo 'Usage: bash build.sh [pico2|pico2w] (asks if omitted)'; exit 0
fi
if ! command -v python3 >/dev/null || ! python3 -c 'import sys, ssl, lzma, zipfile, tarfile; sys.exit(sys.version_info < (3,9))'; then
    echo 'Python 3.9+ is required. Installing Python and CA certificates with the system package manager.'
    prince_sudo=()
    if [[ $(id -u) != 0 ]]; then
        command -v sudo >/dev/null || { echo 'Install Python 3.9+ first, or run with sudo.' >&2; exit 1; }
        prince_sudo=(sudo)
    fi
    if command -v apt-get >/dev/null; then
        "${prince_sudo[@]}" apt-get update
        "${prince_sudo[@]}" apt-get install -y python3 ca-certificates
    elif command -v dnf >/dev/null; then
        "${prince_sudo[@]}" dnf install -y python3 ca-certificates
    elif command -v pacman >/dev/null; then
        "${prince_sudo[@]}" pacman -S --needed --noconfirm python ca-certificates
    elif command -v zypper >/dev/null; then
        "${prince_sudo[@]}" zypper --non-interactive install python3 ca-certificates
    else
        echo 'No supported package manager. Install Python 3.9+ and CA certificates, then retry.' >&2; exit 1
    fi
fi
exec python3 "$prince_root/src/tools/build_firmware.py" "$@"
