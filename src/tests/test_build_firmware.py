#!/usr/bin/env python3
"""Offline bootstrap tests; do not install packages or require original DATs."""
import importlib.util
import io
from pathlib import Path
import struct
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('installer', Path(__file__).parents[1] / 'tools/build_firmware.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


def uf2():
    data = bytearray(512)
    struct.pack_into('<II', data, 0, 0x0A324655, 0x9E5D5157)
    struct.pack_into('<I', data, 508, 0x0AB16F30)
    return bytes(data)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='Prince test with spaces ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_data_check_uses_cmake_list_and_reports_all_missing(self):
        (self.root / 'CMakeLists.txt').write_text('set(DAT_GROUPS PRINCE KID TITLE)')
        (self.root / 'PrinceFiles').mkdir()
        (self.root / 'PrinceFiles/PRINCE.DAT').touch()
        with self.assertRaisesRegex(RuntimeError, r'KID.DAT[\s\S]*TITLE.DAT'):
            b.check_data(self.root)
        for name in ('KID', 'TITLE'): (self.root / 'PrinceFiles' / (name + '.DAT')).touch()
        b.check_data(self.root)

    def test_invalid_uf2_does_not_replace_existing_firmware(self):
        source, output = self.root / 'new.uf2', self.root / 'old.uf2'
        output.write_bytes(b'existing firmware')
        for data in (b'', b'partial', bytes(512), uf2() + bytes(512)):
            source.write_bytes(data)
            with self.assertRaises(RuntimeError): b.publish(source, output)
            self.assertEqual(output.read_bytes(), b'existing firmware')
        source.write_bytes(uf2()); b.publish(source, output)
        self.assertEqual(output.read_bytes(), uf2())

    def test_boards_paths_and_build_failure(self):
        # Record the argv boundary: spaces remain inside individual arguments.
        tools = [self.root / 'tools with spaces' / p for p in ('cmake', 'ninja', 'bin/arm-none-eabi-gcc', 'sdk')]
        configs = {'picotool': self.root / 'picotool', 'pioasm': self.root / 'pioasm'}
        for board, (sdk_board, filename) in b.BOARDS.items():
            output = self.root / ('build-' + board)
            output.mkdir()
            (output / 'prince_picocalc.uf2').write_bytes(uf2())
            with patch.object(b, 'run') as run:
                result = b.build(self.root, board, *tools, configs)
            configure, compile_call = run.call_args_list
            self.assertIn('-DPICO_BOARD=' + sdk_board, configure.args[0])
            self.assertIn('-DPRINCE_USE_PICO_VSCODE=OFF', configure.args[0])
            self.assertIn('-DPython3_EXECUTABLE=' + Path(b.sys.executable).as_posix(), configure.args[0])
            self.assertIn(output, compile_call.args[0])
            self.assertEqual(result, self.root / 'Firmware' / filename)
            self.assertEqual(result.read_bytes(), uf2())
            result.write_bytes(b'keep old firmware')
            with patch.object(b, 'run', side_effect=subprocess.CalledProcessError(1, 'cmake')):
                with self.assertRaises(subprocess.CalledProcessError):
                    b.build(self.root, board, *tools, configs)
            self.assertEqual(result.read_bytes(), b'keep old firmware')

    def test_long_archive_path_and_flat_toolchain_layout(self):
        archive = self.root / 'compiler.zip'
        relative = '/'.join(['headers'] * 35) + '/cxxabi_tweaks.h'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('vendor-toolchain/' + relative, 'header')
            z.writestr('vendor-toolchain/bin/arm-none-eabi-gcc', 'compiler')
        destination = self.root / 'short-cache/arm'
        b.extract(archive, destination, strip_root=True)
        self.assertEqual((destination / relative).read_text(), 'header')
        self.assertTrue((destination / 'bin/arm-none-eabi-gcc').exists())
        self.assertFalse((destination / 'vendor-toolchain').exists())
        self.assertTrue((destination / '.complete').exists())

    def test_reuses_legacy_download_without_network(self):
        old = self.root / '.prince-tools/downloads/compiler.zip'
        old.parent.mkdir(parents=True)
        old.write_bytes(b'existing archive')
        tools = self.root / 'new-cache'
        target = tools / 'downloads/compiler.zip'
        with patch.object(b, 'ROOT', self.root), patch.object(b, 'TOOLS', tools), \
             patch.object(b.urllib.request, 'urlopen', side_effect=AssertionError('No download expected')):
            b.fetch('https://example.invalid/file', target, b.digest(old))
        self.assertEqual(target.read_bytes(), old.read_bytes())

    def test_extended_windows_paths(self):
        self.assertEqual(b.windows_extended_path('C:\\cache\\file'), '\\\\?\\C:\\cache\\file')
        self.assertEqual(b.windows_extended_path('\\\\server\\share\\file'), '\\\\?\\UNC\\server\\share\\file')
        self.assertEqual(b.windows_extended_path('\\\\?\\C:\\cache'), '\\\\?\\C:\\cache')

    def test_tool_lookup_ignores_documentation_and_completions(self):
        # Create misleading names first, as can happen in vendor tar archives.
        (self.root / 'doc/cmake').mkdir(parents=True)
        (self.root / 'share').mkdir()
        (self.root / 'share/cmake').write_text('shell completion')
        (self.root / 'bin').mkdir()
        exe = self.root / 'bin/cmake'
        exe.write_text('executable')
        self.assertEqual(b.find_file(self.root, 'cmake'), exe.resolve())
        exe.unlink()
        (self.root / 'share/cmake').unlink()
        with self.assertRaises(RuntimeError): b.find_file(self.root, 'cmake')

    def test_required_version_reports_loader_error(self):
        error = subprocess.CalledProcessError(127, ['cmake'], output='libexample.so: cannot open shared object file')
        with patch.object(b, 'run', side_effect=error):
            self.assertEqual(b.version('cmake'), ())
            with self.assertRaisesRegex(RuntimeError, 'libexample.so'):
                b.version('cmake', required=True)
        with patch.object(b, 'run', side_effect=PermissionError('Permission denied')):
            with self.assertRaisesRegex(RuntimeError, 'Permission denied'):
                b.version('cmake', required=True)

    def test_archive_traversal_rejected(self):
        archive = self.root / 'bad.zip'
        with zipfile.ZipFile(archive, 'w') as z: z.writestr('../outside', 'bad')
        with self.assertRaises(RuntimeError): b.extract(archive, self.root / 'output')
        self.assertFalse((self.root / 'outside').exists())
        self.assertFalse((self.root / 'output/.complete').exists())

    def test_hash_mismatch_does_not_install_download(self):
        output = self.root / 'download.zip'
        with patch.object(b.sys, 'platform', 'linux'), patch.object(b.urllib.request, 'urlopen', return_value=io.BytesIO(b'bad payload')):
            with self.assertRaisesRegex(RuntimeError, 'SHA-256'):
                b.fetch('https://example.invalid/file', output, '0' * 64)
        self.assertFalse(output.exists())
        self.assertFalse(output.with_name('download.zip.part').exists())

    def test_windows_download_uses_native_tls_and_checks_hash(self):
        target = self.root / 'download with spaces.zip'
        payload = b'checked download'
        sha = b.hashlib.sha256(payload).hexdigest()
        def native_download(args):
            self.assertEqual(args[0], 'powershell.exe')
            self.assertEqual(args[args.index('-Url') + 1], 'https://example.invalid/archive')
            self.assertEqual(Path(args[args.index('-File') + 1]).name, 'download_windows.ps1')
            Path(args[args.index('-OutputPath') + 1]).write_bytes(payload)
        with patch.object(b.sys, 'platform', 'win32'), patch.object(b, 'run', side_effect=native_download), \
             patch.object(b.urllib.request, 'urlopen', side_effect=AssertionError('Python TLS must not be used')):
            b.fetch('https://example.invalid/archive', target, sha)
            self.assertEqual(target.read_bytes(), payload)
            target.unlink()
            with self.assertRaisesRegex(RuntimeError, 'SHA-256'):
                b.fetch('https://example.invalid/archive', target, '0' * 64)
            self.assertFalse(target.exists())
            self.assertFalse(target.with_name(target.name + '.part').exists())

    def test_failed_windows_download_keeps_existing_file(self):
        target = self.root / 'download.zip'
        target.write_bytes(b'old file')
        def fail(args):
            Path(args[args.index('-OutputPath') + 1]).write_bytes(b'partial')
            raise subprocess.CalledProcessError(1, args)
        with patch.object(b.sys, 'platform', 'win32'), patch.object(b, 'run', side_effect=fail):
            with self.assertRaises(subprocess.CalledProcessError):
                b.fetch('https://example.invalid/archive', target, '0' * 64)
        self.assertEqual(target.read_bytes(), b'old file')
        self.assertFalse(target.with_name(target.name + '.part').exists())

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake/Ninja unavailable')
    def test_native_probe_without_host_compiler(self):
        configs = {}
        for tool in ('picotool', 'pioasm'):
            config = self.root / tool
            config.mkdir()
            (config / (tool + 'Config.cmake')).write_text(
                'add_executable(' + tool + ' IMPORTED GLOBAL)\n')
            (config / (tool + 'ConfigVersion.cmake')).write_text(
                'set(PACKAGE_VERSION "2.3.1")\nset(PACKAGE_VERSION_EXACT TRUE)\n'
                'set(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
            configs[tool] = config
        with patch.object(b, 'TOOLS', self.root / 'tools'), patch.dict(b.os.environ, {'CC': '/missing/compiler', 'CXX': '/missing/compiler'}):
            b.check_native_tools(Path(shutil.which('cmake')), Path(shutil.which('ninja')), configs)

    def test_missing_board_prompts_before_dependencies(self):
        with patch.object(b.sys, 'argv', ['builder']), patch('builtins.input', side_effect=['wrong', 'pico2w']) as prompt, \
             patch.object(b, 'check_data', side_effect=RuntimeError('stop before downloads')) as data:
            with self.assertRaisesRegex(RuntimeError, 'stop before downloads'): b.main()
            self.assertEqual(prompt.call_count, 2)
            data.assert_called_once()


if __name__ == '__main__': unittest.main()
