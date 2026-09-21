#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local dependency bootstrap and firmware build; never downloads game data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
def tools_directory():
    configured = os.environ.get('PRINCE_TOOLS_PATH')
    if configured:
        return Path(configured).expanduser().resolve()
    if os.name == 'nt':
        return Path(os.environ.get('LOCALAPPDATA', str(Path.home() / 'AppData/Local'))) / 'PrinceTools'
    return ROOT / '.prince-tools'


TOOLS = tools_directory()
SDK_VERSION = '2.3.1'
TOOLS_TAG = 'v2.3.1-0'
CMAKE_VERSION = '3.31.6'
NINJA_VERSION = '1.12.1'
ARM_VERSION = '13.3.rel1'
BOARDS = {'pico2': ('pico2', 'Prince_Pico2.uf2'),
          'pico2w': ('pico2_w', 'Prince_Pico2w.uf2')}


def run(args, capture=False, **kwargs):
    args = [str(a) for a in args]
    if not capture:
        print('+ ' + subprocess.list2cmdline(args), flush=True)
    return subprocess.run(args, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.STDOUT if capture else None, **kwargs)


def version(exe, option='--version', required=False):
    try:
        result = run([exe, option], capture=True).stdout
        match = re.search(r'(\d+)\.(\d+)(?:\.(\d+))?', result)
        if match:
            return tuple(int(x or 0) for x in match.groups())
        if required:
            raise RuntimeError(f'Cannot read version from {exe}:\n{result.strip()}')
        return ()
    except (OSError, subprocess.CalledProcessError) as error:
        if required:
            details = error.output if isinstance(error, subprocess.CalledProcessError) else str(error)
            raise RuntimeError(f'Cannot run {exe} {option}:\n{details}') from error
        return ()


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def fetch(url, target, sha256=None):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_file() and (not sha256 or digest(target) == sha256):
        return target
    # Reuse archives from earlier project-local installations after moving the
    # Windows cache. Never reuse incomplete .part files.
    legacy = ROOT / '.prince-tools/downloads' / target.name
    if target.parent == TOOLS / 'downloads' and legacy != target and legacy.is_file():
        if not sha256 or digest(legacy) == sha256:
            print('Reusing previous download: ' + legacy.name, flush=True)
            staged = target.with_name(target.name + '.part')
            shutil.copyfile(legacy, staged)
            staged.replace(target)
            return target
    print('Downloading ' + url, flush=True)
    request = urllib.request.Request(url, headers={'User-Agent': 'Prince-PicoCalc-builder'})
    partial = target.with_name(target.name + '.part')
    try:
        if sys.platform == 'win32':
            # Embedded CPython/OpenSSL may not build the same certificate chain
            # as Windows. Use Windows TLS validation, never an insecure context.
            run(['powershell.exe', '-NoProfile', '-NonInteractive',
                 '-ExecutionPolicy', 'Bypass', '-File',
                 ROOT / 'src/tools/download_windows.ps1',
                 '-Url', url, '-OutputPath', partial])
        else:
            with urllib.request.urlopen(request, timeout=60) as response, partial.open('wb') as output:
                shutil.copyfileobj(response, output, 1024 * 1024)
        if sha256 and digest(partial) != sha256:
            raise RuntimeError('SHA-256 mismatch: ' + target.name)
        partial.replace(target)
    finally:
        partial.unlink(missing_ok=True)
    return target


def release_asset(repo, tag, filename):
    metadata = fetch(f'https://api.github.com/repos/{repo}/releases/tags/{tag}',
                     TOOLS / 'downloads' / (repo.replace('/', '-') + '-' + tag + '.json'))
    assets = json.loads(metadata.read_text(encoding='utf-8'))['assets']
    asset = next((a for a in assets if a['name'] == filename), None)
    if asset is None:
        raise RuntimeError(f'Release {repo}/{tag} has no asset {filename}.')
    sha = asset.get('digest') or ''
    return fetch(asset['browser_download_url'], TOOLS / 'downloads' / filename,
                 sha.removeprefix('sha256:') if sha.startswith('sha256:') else None)


def windows_extended_path(value):
    if value.startswith('\\\\?\\'): return value
    if value.startswith('\\\\'): return '\\\\?\\UNC\\' + value[2:]
    return '\\\\?\\' + value


def filesystem_path(path):
    # Prefix only filesystem operations, not paths passed to CMake/GCC.
    value = str(path.absolute())
    return Path(windows_extended_path(value) if os.name == 'nt' else value)


def extract(archive, destination, strip_root=False):
    """Stage complete archives; reject traversal and unsafe link destinations."""
    original_destination = destination
    destination = filesystem_path(destination)
    marker = destination / '.complete'
    if marker.is_file():
        return original_destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='unpack-', dir=destination.parent) as temp:
        temp = filesystem_path(Path(temp)).resolve()
        def contained(name):
            candidate = (temp / name).resolve()
            if candidate != temp and temp not in candidate.parents:
                raise RuntimeError('Unsafe archive path: ' + name)
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as z:
                for item in z.infolist():
                    contained(item.filename)
                    if (item.external_attr >> 16) & 0o170000 == 0o120000:
                        raise RuntimeError('Unexpected ZIP symlink: ' + item.filename)
                z.extractall(temp)
        else:
            with tarfile.open(archive) as t:
                for item in t.getmembers():
                    contained(item.name)
                    if item.issym(): contained(str(Path(item.name).parent / item.linkname))
                    elif item.islnk(): contained(item.linkname)
                    elif not (item.isfile() or item.isdir()):
                        raise RuntimeError('Unsupported archive entry: ' + item.name)
                # Trusted vendor archives, checked paths; preserves compiler symlinks.
                t.extractall(temp, **({'filter': 'data'} if hasattr(tarfile, 'data_filter') else {}))
        source = temp
        if strip_root:
            entries = list(temp.iterdir())
            if len(entries) != 1 or not entries[0].is_dir() or entries[0].is_symlink():
                raise RuntimeError('Expected one top-level directory in ' + archive.name)
            source = entries[0]
        if destination.exists(): shutil.rmtree(destination)
        shutil.copytree(source, destination, symlinks=True)
    marker.write_text('complete\n')
    return original_destination


def find_file(folder, name):
    # Archives also contain documentation directories and shell completions
    # named 'cmake'. Select regular files, preferring the actual bin directory.
    paths = sorted((p for p in folder.rglob(name) if p.is_file()),
                   key=lambda p: (p.parent.name != 'bin', len(p.parts), str(p)))
    if not paths:
        raise RuntimeError(f'{name} not found under {folder}')
    result = paths[0]
    if os.name != 'nt' and not name.endswith('.cmake'):
        result.chmod(result.stat().st_mode | 0o111)
    return result.resolve()


def host():
    machine = platform.machine().lower()
    if sys.platform == 'win32' and machine in ('amd64', 'x86_64', 'arm64'):
        return 'windows', 'x86_64'  # Windows on Arm uses x64 emulation.
    if sys.platform.startswith('linux') and machine in ('x86_64', 'amd64', 'aarch64', 'arm64'):
        return 'linux', 'aarch64' if machine in ('aarch64', 'arm64') else 'x86_64'
    raise RuntimeError('Supported hosts: Windows 64-bit, Linux x86_64 or aarch64 (glibc).')


def candidate_tools(name, folder):
    suffix = '.exe' if os.name == 'nt' else ''
    existing = shutil.which(name)
    if existing: yield Path(existing)
    pico = Path.home() / '.pico-sdk' / folder
    if pico.exists():
        yield from sorted(pico.glob('*/bin/' + name + suffix), reverse=True)


def cmake_tool(system, arch):
    for candidate in candidate_tools('cmake', 'cmake'):
        if version(candidate) >= (3, 24, 0): return candidate.resolve()
    name = f'cmake-{CMAKE_VERSION}-' + ('windows-x86_64.zip' if system == 'windows' else f'linux-{arch}.tar.gz')
    directory = extract(release_asset('Kitware/CMake', 'v' + CMAKE_VERSION, name), TOOLS / ('cmake-' + system + '-' + arch))
    result = find_file(directory, 'cmake.exe' if system == 'windows' else 'cmake')
    if version(result, required=True) < (3, 24, 0):
        raise RuntimeError(f'CMake 3.24+ is required; found an older version at {result}.')
    return result


def ninja_tool(system, arch):
    for candidate in candidate_tools('ninja', 'ninja'):
        if version(candidate) >= (1, 10, 0): return candidate.resolve()
    if system == 'linux' and arch == 'aarch64':
        # Ninja 1.12.1 only ships x86_64 Linux; use the Arm-capable 1.13 release.
        tag, name = 'v1.13.1', 'ninja-linux-aarch64.zip'
    else:
        tag, name = 'v' + NINJA_VERSION, 'ninja-win.zip' if system == 'windows' else 'ninja-linux.zip'
    directory = extract(release_asset('ninja-build/ninja', tag, name), TOOLS / ('ninja-' + tag + '-' + system + '-' + arch))
    result = find_file(directory, 'ninja.exe' if system == 'windows' else 'ninja')
    if version(result) < (1, 10, 0): raise RuntimeError('Downloaded Ninja cannot run on this host.')
    return result


def arm_tool(system, arch):
    candidates = []
    configured = os.environ.get('PICO_TOOLCHAIN_PATH')
    if configured:
        folder = Path(configured)
        candidates += [folder / 'bin' / ('arm-none-eabi-gcc.exe' if system == 'windows' else 'arm-none-eabi-gcc'),
                       folder / ('arm-none-eabi-gcc.exe' if system == 'windows' else 'arm-none-eabi-gcc')]
    candidates += list(candidate_tools('arm-none-eabi-gcc', 'toolchain'))
    def usable(candidate):
        # Confirm an actual C++ compiler and newlib, not just a GCC executable.
        if version(candidate, '-dumpfullversion') < (13, 0, 0): return False
        cxx = candidate.with_name(candidate.name.replace('gcc', 'g++'))
        if not cxx.is_file(): return False
        try:
            return Path(run([candidate, '-print-file-name=libc.a'], capture=True).stdout.strip()).is_file()
        except (OSError, subprocess.CalledProcessError): return False
    for candidate in candidates:
        if usable(candidate): return candidate.resolve()
    platform_name = 'mingw-w64-i686' if system == 'windows' else arch
    name = f'arm-gnu-toolchain-{ARM_VERSION}-{platform_name}-arm-none-eabi.' + ('zip' if system == 'windows' else 'tar.xz')
    url = f'https://developer.arm.com/-/media/Files/downloads/gnu/{ARM_VERSION}/binrel/{name}'
    archive = fetch(url, TOOLS / 'downloads' / name)
    directory = extract(archive, TOOLS / ('arm-flat-' + ARM_VERSION + '-' + system + '-' + arch), strip_root=True)
    result = find_file(directory, 'arm-none-eabi-gcc.exe' if system == 'windows' else 'arm-none-eabi-gcc')
    if not usable(result): raise RuntimeError('Arm toolchain cannot run, or newlib/C++ is missing.')
    return result


def sdk():
    candidates = [Path.home() / '.pico-sdk' / 'sdk' / SDK_VERSION]
    configured = os.environ.get('PICO_SDK_PATH')
    if configured: candidates.insert(0, Path(configured))
    def valid(path):
        version_file = path / 'pico_sdk_version.cmake'
        if not version_file.is_file() or not (path / 'external/pico_sdk_import.cmake').is_file(): return False
        text = version_file.read_text()
        return all(re.search(r'PICO_SDK_VERSION_' + key + r'\s+' + value + r'\s*\)', text)
                   for key, value in zip(('MAJOR', 'MINOR', 'REVISION'), SDK_VERSION.split('.')))
    for path in candidates:
        if valid(path): return path.resolve()
    url = f'https://github.com/raspberrypi/pico-sdk/archive/refs/tags/{SDK_VERSION}.tar.gz'
    directory = extract(fetch(url, TOOLS / 'downloads' / f'pico-sdk-{SDK_VERSION}.tar.gz'), TOOLS / ('sdk-' + SDK_VERSION))
    path = directory / ('pico-sdk-' + SDK_VERSION)
    if not valid(path): raise RuntimeError('Incomplete or unexpected Pico SDK archive.')
    # USB stdio, networking and Bluetooth are disabled; no external submodules
    # are required by this firmware. pioasm and picotool are supplied prebuilt.
    return path.resolve()


def pico_tools(system, arch):
    suffix = 'x64-win.zip' if system == 'windows' else f'{arch}-lin.tar.gz'
    base = TOOLS / ('pico-tools-' + SDK_VERSION + '-' + system + '-' + arch)
    configs = {}
    for package, tool in [('picotool', 'picotool'), ('pico-sdk-tools', 'pioasm')]:
        installed = Path.home() / '.pico-sdk' / ('picotool' if tool == 'picotool' else 'tools') / SDK_VERSION
        if installed.is_dir():
            candidates = list(installed.rglob(tool + 'Config.cmake')) + list(installed.rglob(tool + '-config.cmake'))
            try:
                exe = find_file(installed, tool + ('.exe' if system == 'windows' else ''))
                if tool == 'picotool': run([exe, 'version', SDK_VERSION], capture=True)
                if candidates:
                    configs[tool] = candidates[0].parent.resolve()
                    continue
            except (OSError, RuntimeError, subprocess.CalledProcessError):
                pass
        directory = extract(release_asset('raspberrypi/pico-sdk-tools', TOOLS_TAG,
                                         f'{package}-{SDK_VERSION}-{suffix}'), base / package)
        exe = find_file(directory, tool + ('.exe' if system == 'windows' else ''))
        # picotool's version command also checks DLL/shared-library availability.
        if tool == 'picotool':
            run([exe, 'version', SDK_VERSION], capture=True)
        candidates = list(directory.rglob(tool + 'Config.cmake')) + list(directory.rglob(tool + '-config.cmake'))
        if not candidates:
            raise RuntimeError(f'{tool} package has no CMake config; cannot use this release layout.')
        configs[tool] = candidates[0].parent.resolve()
    return configs


def check_native_tools(cmake, ninja, configs):
    # A CONFIG-only probe must succeed before the SDK can attempt source builds.
    probe = TOOLS / 'native-probe'
    probe.mkdir(parents=True, exist_ok=True)
    (probe / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.24)\nproject(PrinceNativeTools NONE)\n'
        'find_package(picotool ' + SDK_VERSION + ' EXACT CONFIG REQUIRED)\n'
        'find_package(pioasm CONFIG REQUIRED)\n'
        'if(NOT TARGET picotool OR NOT TARGET pioasm)\n'
        '  message(FATAL_ERROR "Native Pico tool targets missing")\nendif()\n')
    run([cmake, '--fresh', '-S', probe, '-B', probe / 'build', '-G', 'Ninja',
         '-DCMAKE_MAKE_PROGRAM=' + ninja.as_posix(),
         '-Dpicotool_DIR=' + configs['picotool'].as_posix(),
         '-Dpioasm_DIR=' + configs['pioasm'].as_posix()])


def check_data(root):
    cmake = (root / 'CMakeLists.txt').read_text()
    groups = re.search(r'set\(DAT_GROUPS\s+([^)]*)\)', cmake)
    if not groups: raise RuntimeError('DAT file list not found in CMakeLists.txt.')
    missing = [group + '.DAT' for group in groups[1].split()
               if not (root / 'PrinceFiles' / (group + '.DAT')).is_file()]
    if missing:
        raise RuntimeError('Place these original files in PrinceFiles first (no game data is downloaded):\n  ' + '\n  '.join(missing))


def publish(source, destination):
    """Never publish an empty/truncated firmware or replace good output on failure."""
    size = source.stat().st_size
    if not size or size % 512: raise RuntimeError('Invalid UF2 size: ' + str(source))
    with source.open('rb') as f:
        for block in iter(lambda: f.read(512), b''):
            if struct.unpack_from('<II', block) != (0x0A324655, 0x9E5D5157) or struct.unpack_from('<I', block, 508)[0] != 0x0AB16F30:
                raise RuntimeError('Invalid UF2 block: ' + str(source))
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix('.uf2.tmp')
    shutil.copyfile(source, temporary)
    temporary.replace(destination)


def build(root, board, cmake, ninja, compiler, sdk_path, configs):
    sdk_board, filename = BOARDS[board]
    output = root / ('build-' + board)
    env = dict(os.environ)
    env['PATH'] = os.pathsep.join([str(compiler.parent), str(cmake.parent), str(ninja.parent), env.get('PATH', '')])
    # Explicit arguments avoid IDE helper overrides and stale board/tool caches.
    run([cmake, '--fresh', '-S', root, '-B', output, '-G', 'Ninja',
         '-DPRINCE_HOST=OFF', '-DPRINCE_USE_PICO_VSCODE=OFF', '-DCMAKE_BUILD_TYPE=Release',
         '-DPICO_PLATFORM=rp2350-arm-s', '-DPICO_BOARD=' + sdk_board,
         '-DPICO_SDK_PATH=' + sdk_path.as_posix(),
         '-DPICO_TOOLCHAIN_PATH=' + compiler.parent.parent.as_posix(),
         '-DCMAKE_MAKE_PROGRAM=' + ninja.as_posix(),
         '-DPython3_EXECUTABLE=' + Path(sys.executable).as_posix(),
         '-DPRINCE_FILES=' + (root / 'PrinceFiles').as_posix(),
         '-Dpicotool_DIR=' + configs['picotool'].as_posix(),
         '-Dpioasm_DIR=' + configs['pioasm'].as_posix()], env=env)
    run([cmake, '--build', output, '--target', 'prince_picocalc', '--parallel', str(min(os.cpu_count() or 2, 4))], env=env)
    result = root / 'Firmware' / filename
    publish(output / 'prince_picocalc.uf2', result)
    print('\nFirmware ready: ' + str(result), flush=True)
    print('Copy it to your PicoCalc loader SD card or install using BOOTSEL.')
    return result


def main():
    if sys.version_info < (3, 9): raise RuntimeError("Python 3.9 or newer is required.")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('board', nargs='?', choices=BOARDS)
    args = parser.parse_args()
    if not args.board:
        while args.board not in BOARDS:
            try: args.board = input('Board type (pico2 / pico2w): ').strip().lower()
            except EOFError: raise RuntimeError('No interactive input. Pass pico2 or pico2w as a parameter.')
    check_data(ROOT)  # Fail before downloading hundreds of MB without assets.
    system, arch = host()
    TOOLS.mkdir(parents=True, exist_ok=True)
    cmake, ninja = cmake_tool(system, arch), ninja_tool(system, arch)
    compiler, sdk_path = arm_tool(system, arch), sdk()
    configs = pico_tools(system, arch)
    check_native_tools(cmake, ninja, configs)
    build(ROOT, args.board, cmake, ninja, compiler, sdk_path, configs)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError, tarfile.TarError, zipfile.BadZipFile) as error:
        print('\nERROR: ' + str(error), file=sys.stderr)
        print('Fix the reported dependency/download/build error and rerun. Existing firmware is kept.', file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print('\nCancelled.', file=sys.stderr)
        sys.exit(130)
