#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Ap profile may in (Voron2 300 / MKS Monster8 V2) vao Cura da cai tren may.

.DESCRIPTION
    Co 2 cach, mac dinh la cach 1 (KHONG can quyen Admin):

    1) (mac dinh) Sua may in "Voron2 300" dang co san trong Cura.
       Cura luu moi thay doi thong so may vao
         %APPDATA%\cura\<version>\definition_changes\<TenMay>_settings.inst.cfg
       Script ghi de file do bang cura_profile\machine_definition_changes.inst.cfg.
       Cach nay giu nguyen variant / quality / material / platform ma may in dang dung,
       nen khong lam hong gi trong Cura.

    2) -AddAsNewPrinter: them mot may in MOI tu cura_profile\voron21_300_mks_monster8.def.json.
       Ghi vao Program Files CAN QUYEN ADMIN. Neu khong co quyen, script chi ghi duoc vao
       thu muc config nguoi dung. Luu y: variant cua Voron duoc gan voi definition
       "voron2_300", nen may in moi se khong co san variant - phai tu tao.

    LUON DONG CURA TRUOC KHI CHAY. Cura ghi de file cau hinh khi thoat.

.PARAMETER AddAsNewPrinter
    Them may in moi thay vi sua may in "Voron2 300" dang co.

.PARAMETER CuraConfigRoot
    Thu muc config cua Cura (mac dinh %APPDATA%\cura).

.PARAMETER Definition
    Chi sua cac may in dung definition nay (mac dinh voron2_300).

.PARAMETER WhatIf
    Chi in ra se lam gi, khong ghi.

.EXAMPLE
    .\install-cura-profile.ps1
    .\install-cura-profile.ps1 -WhatIf
    .\install-cura-profile.ps1 -AddAsNewPrinter
#>

param(
    [switch]$AddAsNewPrinter,
    [string]$CuraConfigRoot,
    [string]$Definition = "voron2_300",
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSCommandPath

Write-Host ""
Write-Host "===== Ap profile Cura: Voron2 300 (MKS Monster8 V2) =====" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Canh bao neu Cura dang chay - Cura se ghi de file khi thoat
# ---------------------------------------------------------------------------
$curaProcs = Get-Process -Name "Cura","UltiMaker Cura" -ErrorAction SilentlyContinue
if ($curaProcs) {
    Write-Host "CANH BAO: Cura dang chay (PID $($curaProcs.Id -join ', '))." -ForegroundColor Yellow
    Write-Host "         Hay DONG CURA hoan toan roi chay lai, neu khong thay doi se bi ghi de." -ForegroundColor Yellow
    if (-not $WhatIf) {
        $ans = Read-Host "Van tiep tuc? (y/n)"
        if ($ans -ne 'y' -and $ans -ne 'Y') { Write-Host "Da huy."; exit 0 }
    }
    Write-Host ""
}

# ===========================================================================
# CACH 1 (mac dinh): sua definition_changes cua may in dang co
# ===========================================================================
if (-not $AddAsNewPrinter) {

    $template = Join-Path $repoRoot "cura_profile\machine_definition_changes.inst.cfg"
    if (-not (Test-Path $template)) {
        Write-Host "LOI: khong thay template: $template" -ForegroundColor Red
        exit 1
    }

    $root = if ($CuraConfigRoot) { $CuraConfigRoot } else { Join-Path $env:APPDATA "cura" }
    if (-not (Test-Path $root)) {
        Write-Host "LOI: khong thay thu muc config Cura: $root" -ForegroundColor Red
        Write-Host "     Cura da chay lan nao chua?" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "Config Cura: $root"
    Write-Host "Definition:  $Definition"
    Write-Host ""

    $versions = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^\d+\.\d+$' } | Select-Object -ExpandProperty Name
    if (-not $versions) {
        Write-Host "LOI: khong thay thu muc phien ban nao trong $root" -ForegroundColor Red
        exit 1
    }

    $done = 0
    foreach ($v in $versions) {
        $dcDir = Join-Path $root "$v\definition_changes"
        if (-not (Test-Path $dcDir)) { continue }

        # Tim file cau hinh cua may in dung definition nay
        $targets = Get-ChildItem "$dcDir\*.inst.cfg" -ErrorAction SilentlyContinue | Where-Object {
            (Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue) -match "(?m)^definition\s*=\s*$([regex]::Escape($Definition))\s*$"
        }

        if (-not $targets) {
            Write-Host "[$v] khong co may in nao dung definition '$Definition' - bo qua" -ForegroundColor DarkGray
            continue
        }

        foreach ($t in $targets) {
            # Giu nguyen 'name = ...' cua file goc: ten file dung dau '+' thay cho dau cach
            # (Voron2+300_settings.inst.cfg) nhung gia tri name lai co dau cach
            # (name = Voron2 300_settings). Suy tu ten file se sai.
            $raw = Get-Content $t.FullName -Raw
            $nameMatch = [regex]::Match($raw, '(?m)^name\s*=\s*(.+?)\s*$')
            $machineName = if ($nameMatch.Success) { $nameMatch.Groups[1].Value } else { $t.Name -replace '\.inst\.cfg$', '' }
            $content = (Get-Content $template -Raw).Replace('@NAME@', $machineName)

            if ($WhatIf) {
                Write-Host "  [WhatIf] se ghi de -> $($t.FullName)" -ForegroundColor DarkGray
                $done++
                continue
            }

            $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
            Copy-Item $t.FullName "$($t.FullName).bak-$stamp" -Force
            [System.IO.File]::WriteAllText($t.FullName, $content, (New-Object System.Text.UTF8Encoding($false)))
            Write-Host "  OK -> $($t.FullName)" -ForegroundColor Green
            Write-Host "       (da backup: $($t.Name).bak-$stamp)" -ForegroundColor DarkGray
            $done++
        }
    }

    Write-Host ""
    Write-Host "===== KET QUA =====" -ForegroundColor Cyan
    if ($done -gt 0) {
        Write-Host "Da ap dung cho $done may in." -ForegroundColor Green
        Write-Host ""
        Write-Host "Buoc tiep theo:" -ForegroundColor Cyan
        Write-Host "  1. MO CURA LAI."
        Write-Host "  2. Che do Prepare: kiem tra khay in hien dung 305 x 305."
        Write-Host "  3. Printer > Manage printers > Machine settings: kiem tra"
        Write-Host "     'Origin at center' DA TAT, va Start/End G-code dung ban Marlin."
        exit 0
    } else {
        Write-Host "Khong tim thay may in nao de sua." -ForegroundColor Yellow
        Write-Host "Trong Cura: Settings > Printer > Add Printer > Non-Ultimaker > Voron2 300,"
        Write-Host "roi chay lai script nay." -ForegroundColor Yellow
        exit 1
    }
}

# ===========================================================================
# CACH 2: them may in moi tu .def.json
# ===========================================================================
$sourceFile = Join-Path $repoRoot "cura_profile\voron21_300_mks_monster8.def.json"
if (-not (Test-Path $sourceFile)) {
    Write-Host "LOI: khong thay $sourceFile" -ForegroundColor Red
    exit 1
}

Write-Host "Them may in moi tu: $sourceFile"
Write-Host ""

function Find-CuraInstalls {
    $found = New-Object System.Collections.Generic.List[string]
    foreach ($root in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall')) {
        Get-ChildItem $root -ErrorAction SilentlyContinue | ForEach-Object {
            $props = Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue
            if ($props.DisplayName -match 'Cura' -and $props.InstallLocation -and (Test-Path $props.InstallLocation)) {
                $found.Add($props.InstallLocation)
            }
        }
    }
    foreach ($pattern in @(
        "$env:ProgramFiles\UltiMaker Cura *",
        "$env:ProgramFiles\Ultimaker Cura *",
        "${env:ProgramFiles(x86)}\UltiMaker Cura *",
        "$env:LOCALAPPDATA\Programs\UltiMaker Cura *")) {
        Get-Item $pattern -ErrorAction SilentlyContinue | ForEach-Object { $found.Add($_.FullName) }
    }
    $seen = @{}; $result = New-Object System.Collections.Generic.List[string]
    foreach ($p in $found) {
        $full = (Resolve-Path $p -ErrorAction SilentlyContinue).Path
        if ($full -and -not $seen.ContainsKey($full)) { $seen[$full] = $true; $result.Add($full) }
    }
    return $result
}

$okCount = 0
foreach ($install in (Find-CuraInstalls)) {
    $defDir = $null
    foreach ($c in @("$install\share\cura\resources\definitions", "$install\resources\definitions")) {
        if (Test-Path $c) { $defDir = $c; break }
    }
    Write-Host "Cura: $install"
    if (-not $defDir) { Write-Host "  Bo qua: khong thay definitions" -ForegroundColor DarkGray; continue }

    $dest = Join-Path $defDir (Split-Path -Leaf $sourceFile)
    if ($WhatIf) { Write-Host "  [WhatIf] -> $dest" -ForegroundColor DarkGray; $okCount++; continue }
    try {
        Copy-Item $sourceFile $dest -Force
        Write-Host "  OK -> $dest" -ForegroundColor Green
        $okCount++
    } catch {
        Write-Host "  THAT BAI (can Admin): $dest" -ForegroundColor Yellow
        Write-Host "     -> Mo PowerShell 'Run as administrator' roi chay lai." -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "===== KET QUA =====" -ForegroundColor Cyan
if ($okCount -gt 0) {
    Write-Host "Da cai $okCount noi. MO LAI CURA roi vao Settings > Printer > Add Printer." -ForegroundColor Green
    exit 0
} else {
    Write-Host "Khong cai duoc (can quyen Admin)." -ForegroundColor Red
    exit 1
}
