#requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-or-later
# Keep Windows certificate-chain validation, including Windows root discovery.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][Uri]$Url,
    [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$client = $null
try {
    if ($Url.Scheme -ne 'https') { throw 'Downloads require HTTPS.' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $client = New-Object Net.WebClient
    $client.Headers.Add('User-Agent', 'Prince-PicoCalc-builder')
    # Otherwise WebClient uses the Windows default proxy settings.
    $proxyAddress = $env:HTTPS_PROXY
    if (-not $proxyAddress) { $proxyAddress = $env:HTTP_PROXY }
    if ($proxyAddress) { $client.Proxy = New-Object Net.WebProxy($proxyAddress) }
    $client.DownloadFile($Url, $OutputPath)
} catch {
    Write-Error ("Windows download failed: " + $_.Exception.Message) -ErrorAction Continue
    exit 1
} finally {
    if ($client) { $client.Dispose() }
}
exit 0
