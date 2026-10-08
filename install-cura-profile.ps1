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
    [switch]$Force,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSCommandPath

Write-Host ""
Write-Host "===== Ap profile Cura: Voron2 300 (MKS Monster8 V2) =====" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Cura phai DONG: no ghi de cac file nay khi thoat
# ---------------------------------------------------------------------------
$curaProcs = Get-Process -Name "Cura","UltiMaker-Cura" -ErrorAction SilentlyContinue
if ($curaProcs -and -not $Force) {
    Write-Host "DUNG LAI: Cura dang chay (PID $($curaProcs.Id -join ', '))." -ForegroundColor Yellow
    Write-Host "  Cura ghi de cac file cau hinh nay khi thoat, nen phai DONG CURA truoc." -ForegroundColor Yellow
    Write-Host "  Dong Cura roi chay lai script, hoac dung -Force neu chac chan." -ForegroundColor Yellow
    exit 2
}
if ($curaProcs) {
    Write-Host "CANH BAO: Cura van dang chay - thay doi co the bi ghi de khi Cura thoat." -ForegroundColor Yellow
    Write-Host ""
}

# ===========================================================================
# CACH 1 (mac dinh): sua definition_changes cua may in dang co
# ===========================================================================
if (-not $AddAsNewPrinter) {

    # Moi file template ap cho mot nhom container.
    #   SubDir   : thu muc con trong config Cura
    #   MatchKey : dong trong file dung de nhan dien container
    #   Merge    : $true = giu lai cac gia tri nguoi dung da dat, chi them key con thieu
    $jobs = @(
        @{ Template = "machine_definition_changes.inst.cfg";  Label = "May in";          SubDir = "definition_changes"; MatchKey = "definition"; Pattern = [regex]::Escape($Definition); Merge = $false },
        @{ Template = "machine_user.inst.cfg";                Label = "May in (user)";   SubDir = "user";               MatchKey = "name";       Pattern = 'Voron2 300_user';       Merge = $true  },
        @{ Template = "extruder_definition_changes.inst.cfg"; Label = "Extruder (gcode)"; SubDir = "definition_changes"; MatchKey = "definition"; Pattern = 'voron2_extruder.*';     Merge = $false },
        @{ Template = "extruder_user.inst.cfg";               Label = "Extruder (user)";  SubDir = "user";               MatchKey = "extruder";   Pattern = 'voron2_extruder.*';     Merge = $true  }
    )

    # Ghep gia tri: giu nguyen file goc, chi them cac key ma file goc chua co
    function Merge-Values([string]$existingRaw, [string]$tplRaw) {
        $tplVals = ($tplRaw -split '(?m)^\[values\]\s*$')[1]
        if (-not $tplVals) { return $existingRaw }
        $existingKeys = [regex]::Matches($existingRaw, '(?m)^([^=\n !]+)[ \t]*=') | ForEach-Object { $_.Groups[1].Value }
        $out = $existingRaw.TrimEnd() + "`n"
        foreach ($line in ($tplVals -split "`n")) {
            if ($line -match '^\s*$') { continue }
            if ($line -match '^\t') { $out += $line + "`n"; continue }   # dong noi cua gia tri nhieu dong
            $k = ($line -split '=', 2)[0].Trim()
            if ($existingKeys -contains $k) { continue }                 # nguoi dung da dat roi -> ton trong
            $out += $line + "`n"
        }
        return $out
    }

    $root = if ($CuraConfigRoot) { $CuraConfigRoot } else { Join-Path $env:APPDATA "cura" }
    if (-not (Test-Path $root)) {
        Write-Host "LOI: khong thay thu muc config Cura: $root" -ForegroundColor Red
        Write-Host "     Cura da chay lan nao chua?" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "Config Cura: $root"
    Write-Host ""

    $versions = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^\d+\.\d+$' } | Select-Object -ExpandProperty Name
    if (-not $versions) {
        Write-Host "LOI: khong thay thu muc phien ban nao trong $root" -ForegroundColor Red
        exit 1
    }

    $done = 0
    foreach ($job in $jobs) {
        $template = Join-Path $repoRoot "cura_profile\$($job.Template)"
        if (-not (Test-Path $template)) {
            Write-Host "CANH BAO: khong thay template $($job.Template) - bo qua" -ForegroundColor Yellow
            continue
        }
        $tplRaw = Get-Content $template -Raw

        foreach ($v in $versions) {
            $dcDir = Join-Path $root "$v\$($job.SubDir)"
            if (-not (Test-Path $dcDir)) { continue }

            $targets = Get-ChildItem "$dcDir\*.inst.cfg" -ErrorAction SilentlyContinue | Where-Object {
                (Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue) -match "(?m)^$($job.MatchKey)\s*=\s*$($job.Pattern)\s*$"
            }
            if (-not $targets) { continue }

            Write-Host "[$($job.Label)] $v : $($targets.Count) container" -ForegroundColor Cyan
            foreach ($t in $targets) {
                # Giu nguyen 'name' / 'definition' / 'extruder' cua file goc: ten file dung dau '+'
                # thay cho dau cach (Voron2+300_settings.inst.cfg) nhung gia tri name lai co dau cach.
                $raw = Get-Content $t.FullName -Raw
                $nameMatch = [regex]::Match($raw, '(?m)^name\s*=\s*(.+?)\s*$')
                $defMatch  = [regex]::Match($raw, '(?m)^definition\s*=\s*(.+?)\s*$')
                $extMatch  = [regex]::Match($raw, '(?m)^extruder\s*=\s*(.+?)\s*$')
                $machineName = if ($nameMatch.Success) { $nameMatch.Groups[1].Value } else { $t.Name -replace '\.inst\.cfg$', '' }
                $machineDef  = if ($defMatch.Success)  { $defMatch.Groups[1].Value }  else { $Definition }
                $machineExt  = if ($extMatch.Success)  { $extMatch.Groups[1].Value }  else { '' }
                $content = $tplRaw.Replace('@NAME@', $machineName).Replace('@DEFINITION@', $machineDef).Replace('@EXTRUDER@', $machineExt)

                if ($job.Merge) { $content = Merge-Values $raw $content }

                if ($WhatIf) {
                    Write-Host "  [WhatIf] se ghi $(if ($job.Merge) {'(merge)'} else {'(de)'}) -> $($t.Name)" -ForegroundColor DarkGray
                    $done++
                    continue
                }

                $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
                Copy-Item $t.FullName "$($t.FullName).bak-$stamp" -Force
                [System.IO.File]::WriteAllText($t.FullName, $content, (New-Object System.Text.UTF8Encoding($false)))
                Write-Host "  OK -> $($t.Name)" -ForegroundColor Green
                $done++
            }
        }
    }

    # ------------------------------------------------------------------ #
    # Script hau xu ly (PostProcessingPlugin) -> <config>\<ver>\scripts\
    # Cura chi doc thu muc nay luc khoi dong, nen phai MO LAI CURA moi thay.
    # ------------------------------------------------------------------ #
    $scriptSrc = Join-Path $repoRoot "cura_profile\scripts"
    if (Test-Path $scriptSrc) {
        $pyFiles = Get-ChildItem "$scriptSrc\*.py" -ErrorAction SilentlyContinue
        foreach ($v in $versions) {
            $dstDir = Join-Path $root "$v\scripts"
            foreach ($src in $pyFiles) {
                $dst = Join-Path $dstDir $src.Name
                if ($WhatIf) {
                    Write-Host "[Script] $v : [WhatIf] se copy -> $($src.Name)" -ForegroundColor DarkGray
                    $done++
                    continue
                }
                if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
                Copy-Item $src.FullName $dst -Force
                Write-Host "[Script] $v : OK -> scripts\$($src.Name)" -ForegroundColor Green
                $done++
            }
        }
    }

    # ------------------------------------------------------------------ #
    # Setting DANG LUU cua post-processing script (nam trong machine instance)
    #
    # Khi bat mot script, Cura luu TOAN BO setting cua no vao khoi
    # `post_processing_scripts` trong machine_instances\*.global.cfg. Gia tri dang
    # luu do DE LEN `default_value` trong file .py, nen copy .py la CHUA DU.
    # Da gap that: ClampFeeds.py doi 300->150 / 500->2000 nhung Cura van dung so cu.
    # ------------------------------------------------------------------ #
    $ppTool = Join-Path $repoRoot "cura_profile\fix-pp-settings.py"
    if (Test-Path $ppTool) {
        $py = Get-Command python -ErrorAction SilentlyContinue
        if (-not $py) {
            $candidate = Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"
            if (Test-Path $candidate) { $py = Get-Item $candidate }
        }
        if ($py) {
            $ppArgs = @($ppTool, "--root", $root)
            if ($WhatIf) { $ppArgs += "--what-if" }
            $prevEAP = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            & $py.Source @ppArgs
            $ErrorActionPreference = $prevEAP
            $done++
        } else {
            Write-Host "[PP] Bo qua dong bo setting da luu: khong thay python" -ForegroundColor Yellow
        }
    }

    Write-Host ""
    Write-Host "===== KET QUA =====" -ForegroundColor Cyan
    if ($done -gt 0) {
        Write-Host "Da ap dung cho $done muc." -ForegroundColor Green
        Write-Host ""
        Write-Host "Buoc tiep theo:" -ForegroundColor Cyan
        Write-Host "  1. MO CURA LAI."
        Write-Host "  2. Manage printers > Machine settings > tab Printer:"
        Write-Host "     'Origin at center' TAT, ban 305x305, Start/End G-code la ban Marlin."
        Write-Host "  3. tab Extruder 1: 'Extruder Start G-code' = duong purge,"
        Write-Host "     'Extruder End G-code' = retract."
        exit 0
    } else {
        Write-Host "Khong tim thay container nao de sua." -ForegroundColor Yellow
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
