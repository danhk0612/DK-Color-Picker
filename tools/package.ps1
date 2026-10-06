param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

$exe = Resolve-Path $ExePath
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$out = Join-Path $root $OutputDir
$stage = Join-Path $out "DKColorPicker-win-x64"
$zip = Join-Path $out "DKColorPicker-win-x64.zip"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }

New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item $exe (Join-Path $stage "DKColorPicker.exe")
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE")
Copy-Item (Join-Path $root "README.md") (Join-Path $stage "README.md")

New-Item -ItemType Directory -Path $out -Force | Out-Null
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal

$exeInfo = Get-Item (Join-Path $stage "DKColorPicker.exe")
$zipInfo = Get-Item $zip
Write-Host "EXE: $($exeInfo.FullName) ($($exeInfo.Length) bytes)"
Write-Host "ZIP: $($zipInfo.FullName) ($($zipInfo.Length) bytes)"
