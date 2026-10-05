#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Upload Marlin firmware to MKS Monster8 V2 via ST-Link

.DESCRIPTION
    Builds and uploads the Marlin firmware using PlatformIO.
    MKS Monster8 V2 uses ST-Link protocol by default.

.PARAMETER Environment
    PlatformIO environment to build/upload (default: mks_monster8)

.PARAMETER SkipUpload
    Only build, don't upload (default: false)

.PARAMETER NoPrompt
    Skip confirmation prompt (default: false)

.EXAMPLE
    # Build and upload to default environment
    .\upload-firmware.ps1

    # Build only, no upload
    .\upload-firmware.ps1 -SkipUpload

    # Upload without confirmation
    .\upload-firmware.ps1 -NoPrompt

#>

param(
    [string]$Environment = "mks_monster8",
    [switch]$SkipUpload,
    [switch]$NoPrompt
)

$ErrorActionPreference = "Stop"

# Validate PlatformIO is installed
Write-Host "Checking PlatformIO..." -ForegroundColor Cyan
$pio = Get-Command pio -ErrorAction SilentlyContinue

if (-not $pio) {
    Write-Host "Error: PlatformIO not found in PATH" -ForegroundColor Red
    Write-Host "Make sure Python Scripts directory is in PATH" -ForegroundColor Yellow
    exit 1
}

Write-Host "OK: PlatformIO found at $($pio.Source)" -ForegroundColor Green

# Get project root
$projectRoot = Split-Path -Parent $PSCommandPath

Write-Host ""
Write-Host "===== Marlin Firmware Upload =====" -ForegroundColor Cyan
Write-Host "Project:     $projectRoot"
Write-Host "Environment: $Environment"
Write-Host "Hardware:    MKS Monster8 V2 (STM32F407VGT6)"
Write-Host "Protocol:    ST-Link"
Write-Host "Action:      $(if ($SkipUpload) { 'BUILD ONLY' } else { 'BUILD + UPLOAD' })"
Write-Host ""
Write-Host "Prerequisites:" -ForegroundColor Yellow
Write-Host "  - ST-Link debugger connected via USB"
Write-Host "  - Board power supply ON"
Write-Host "  - No other PlatformIO instances running"
Write-Host ""

if (-not $NoPrompt) {
    $confirm = Read-Host "Continue? (y/n)"
    if ($confirm -ne "y" -and $confirm -ne "Y") {
        Write-Host "Cancelled" -ForegroundColor Yellow
        exit 0
    }
}

Write-Host ""
Write-Host "Building firmware..." -ForegroundColor Cyan

$buildArgs = @("run", "-d", $projectRoot, "-e", $Environment)

if ($SkipUpload) {
    Write-Host "(skipping upload)" -ForegroundColor Gray
}

&pio @buildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Build failed (exit code: $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "SUCCESS: Build completed!" -ForegroundColor Green

$binPath = Join-Path $projectRoot ".pio\build\$Environment\mks_monster8.bin"
if (Test-Path $binPath) {
    $fileSize = (Get-Item $binPath).Length / 1024
    Write-Host ""
    Write-Host "Firmware artifact:" -ForegroundColor Cyan
    Write-Host "  Binary: $binPath"
    Write-Host "  Size: $([math]::Round($fileSize, 2)) KB"
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
if (-not $SkipUpload) {
    Write-Host "  1. Monitor via serial: pio device monitor -b 250000"
    Write-Host "  2. Verify Configuration.h settings"
    Write-Host "  3. Run calibration if needed"
} else {
    Write-Host "  1. If upload failed, use: pio run -d . -e $Environment --target upload"
}

exit 0
