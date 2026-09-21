#requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([Parameter(Position=0)][ValidateSet('pico2','pico2w')][string]$Board)
$ErrorActionPreference = 'Stop'
try {
    # Prefer an existing interpreter. Reject Windows Store aliases that cannot run.
    $python = $null
    $prefix = @()
    foreach ($candidate in @('python', 'python3', 'py')) {
        $command = Get-Command $candidate -CommandType Application -ErrorAction SilentlyContinue
        if (-not $command -or $command.Source -like "*\WindowsApps\*") { continue }
        $candidatePrefix = @()
        if ($candidate -eq 'py') { $candidatePrefix = @('-3') }
        try {
            & $command.Source @candidatePrefix -c 'import sys, ssl, lzma, zipfile, tarfile; sys.exit(sys.version_info < (3,9))' 2>$null
            if ($LASTEXITCODE -eq 0) { $python = $command.Source; $prefix = $candidatePrefix; break }
        } catch { continue }
    }
    if (-not $python) {
        if (-not [Environment]::Is64BitOperatingSystem) { throw '64-bit Windows is required.' }
        # Embeddable CPython needs no administrator account, installer or pip.
        $pythonDir = Join-Path $PSScriptRoot '.prince-tools/python-3.12.10'
        $python = Join-Path $pythonDir 'python.exe'
        $complete = Join-Path $pythonDir '.complete'
        if (-not (Test-Path -LiteralPath $complete) -or -not (Test-Path -LiteralPath $python)) {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            $zip = Join-Path $PSScriptRoot '.prince-tools/python-3.12.10.zip'
            New-Item -ItemType Directory -Force -Path (Split-Path $zip) | Out-Null
            Write-Host 'Downloading CPython 3.12.10 from python.org...'
            Invoke-WebRequest -UseBasicParsing -Uri 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip' -OutFile "$zip.part"
            Move-Item -LiteralPath "$zip.part" -Destination $zip -Force
            if (Test-Path -LiteralPath $pythonDir) { Remove-Item -LiteralPath $pythonDir -Recurse -Force }
            # Use .NET directly: Microsoft.PowerShell.Archive may not load
            # in Windows Sandbox or a minimal Windows installation.
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $pythonDir)
            if ((Get-AuthenticodeSignature -LiteralPath $python).Status -ne 'Valid') {
                Remove-Item -LiteralPath $pythonDir -Recurse -Force
                throw 'Downloaded Python executable has no valid Authenticode signature.'
            }
            & $python -c 'import ssl, lzma, zipfile, tarfile'
            if ($LASTEXITCODE -ne 0) { throw 'Downloaded Python failed its startup check.' }
            Set-Content -LiteralPath $complete -Value 'complete'
        }
    }
    $builder = Join-Path $PSScriptRoot 'src/tools/build_firmware.py'
    $boardArgs = @()
    if ($Board) { $boardArgs = @($Board.ToLowerInvariant()) }
    & $python @prefix $builder @boardArgs
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE). See the error above." }
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
exit 0
