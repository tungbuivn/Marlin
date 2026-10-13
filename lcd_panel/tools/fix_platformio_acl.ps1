<#
  fix_platformio_acl.ps1 -- sua ACL hong trong thu muc PlatformIO (~/.platformio).

  VI SAO CAN: tren may nay mot so thu muc trong
      %USERPROFILE%\.platformio\packages\tool-esptoolpy\_contrib\
  (intelhex, ecdsa) bi DENY ACE -> khong doc duoc file, ke ca `Get-Acl`/`icacls` cung bi tu choi.
  Hau qua: esptool.py khong chay duoc => PlatformIO KHONG build duoc env ESP32 (CYD):
      PermissionError: [Errno 13] Permission denied: '...\_contrib\intelhex\__init__.py'
  (env STM32 khong dung esptool nen khong bi anh huong.)

  CACH CHAY (PowerShell voi quyen Administrator):
      powershell -ExecutionPolicy Bypass -File D:\Marlin\lcd_panel\tools\fix_platformio_acl.ps1

  Script lam 4 viec, an toan va chay lai duoc nhieu lan:
      1) quet ~/.platformio (packages, platforms, file o goc) -> liet ke muc KHONG doc duoc
      2) voi tung muc hong: takeown (khong de quy) -> icacls /reset -> grant Full cho
         Administrators + user hien tai, roi lam lai co de quy
      3) quet lai toan bo cay (takeown /R + icacls /reset /T + grant) de chac chan
      4) kiem tra lai cac file quan trong cua esptool va in ket qua

  Khong dung /grant cho Everyone: chi Administrators (S-1-5-32-544) va tai khoan dang nhap.
#>
[CmdletBinding()]
param(
  # Thu muc PlatformIO (mac dinh: %USERPROFILE%\.platformio)
  [string] $Root = (Join-Path $env:USERPROFILE '.platformio'),
  # Bo qua buoc quet lai toan bo cay (nhanh hon, chi sua cac muc hong tim thay)
  [switch] $SkipFullTree
)

$ErrorActionPreference = 'Continue'

function Write-Head($t) { Write-Host ''; Write-Host "=== $t" -ForegroundColor Cyan }
function Write-Ok($t)   { Write-Host "  [OK]   $t" -ForegroundColor Green }
function Write-Bad($t)  { Write-Host "  [HONG] $t" -ForegroundColor Red }
function Write-Info($t) { Write-Host "  $t" -ForegroundColor Gray }

# ---------------------------------------------------------------------------
# 0) Phai chay bang quyen Administrator
# ---------------------------------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  Write-Host 'CAN QUYEN ADMINISTRATOR.' -ForegroundColor Red
  Write-Host 'Mo PowerShell bang "Run as administrator" roi chay lai:'
  Write-Host "  powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`""
  exit 1
}

Write-Head "Sua ACL cho: $Root"
if (-not (Test-Path -LiteralPath $Root)) { Write-Bad "khong thay $Root"; exit 1 }
$me  = "$env:USERDOMAIN\$env:USERNAME"
$grp = 'BUILTIN\Administrators'
Write-Info "user: $me   |   group: $grp"

# ---------------------------------------------------------------------------
# 1) Quet: tim file/thu muc KHONG doc duoc
# ---------------------------------------------------------------------------
function Get-BrokenPaths {
  param([string] $Start)
  $bad   = New-Object System.Collections.Generic.List[string]
  $stack = New-Object System.Collections.Stack
  $stack.Push($Start)
  $n = 0
  while ($stack.Count -gt 0) {
    $d = $stack.Pop()
    $items = $null
    try { $items = [System.IO.Directory]::GetFileSystemEntries($d) }
    catch { $bad.Add($d); continue }          # khong liet ke duoc chinh thu muc nay
    if (-not $items) { continue }
    foreach ($i in $items) {
      $n++
      if ([System.IO.Directory]::Exists($i)) { $stack.Push($i) }
      else {
        try { [void][System.IO.File]::GetAttributes($i) }
        catch { $bad.Add($i) }
      }
    }
  }
  Write-Info "da quet $n muc"
  return $bad
}

Write-Head '1) Quet tim muc khong doc duoc'
$scanRoots = @("$Root\packages", "$Root\platforms") |
             Where-Object { Test-Path -LiteralPath $_ }
$scanRoots += (Get-ChildItem -LiteralPath $Root -File -Force -ErrorAction SilentlyContinue |
               Select-Object -ExpandProperty FullName)

$broken = New-Object System.Collections.Generic.List[string]
foreach ($r in $scanRoots) {
  $b = Get-BrokenPaths -Start $r
  foreach ($x in $b) { $broken.Add($x) }
}
if ($broken.Count -eq 0) { Write-Ok 'khong thay muc nao bi chan' }
else { foreach ($b in $broken) { Write-Bad $b } }

# ---------------------------------------------------------------------------
# 2) Sua tung muc hong (khong de quy truoc, roi de quy sau)
# ---------------------------------------------------------------------------
function Repair-Path {
  param([string] $Path, [switch] $Recurse)
  $r = if ($Recurse) { @('/R','/D','Y') } else { @('/D','Y') }
  & takeown /F "$Path" @r | Out-Null
  & icacls "$Path" /reset | Out-Null
  & icacls "$Path" /grant "${grp}:(OI)(CI)F" | Out-Null
  & icacls "$Path" /grant "${me}:(OI)(CI)F"  | Out-Null
  if ($Recurse) {
    & icacls "$Path" /reset /T /C /Q | Out-Null
    & icacls "$Path" /grant "${grp}:(OI)(CI)F" /T /C /Q | Out-Null
    & icacls "$Path" /grant "${me}:(OI)(CI)F"  /T /C /Q | Out-Null
  }
}

if ($broken.Count -gt 0) {
  Write-Head '2) Sua cac muc hong'
  foreach ($b in $broken) {
    Write-Info "sua: $b"
    Repair-Path -Path $b                 # buoc 1: mo khoa chinh no
    Repair-Path -Path $b -Recurse        # buoc 2: ca cay con
  }
}

# ---------------------------------------------------------------------------
# 3) Quet lai toan bo cay (tuy chon)
# ---------------------------------------------------------------------------
if (-not $SkipFullTree) {
  Write-Head '3) Chuan hoa ACL cho ca cay (takeown /R + icacls /reset /T)'
  foreach ($r in @("$Root\packages", "$Root\platforms")) {
    if (-not (Test-Path -LiteralPath $r)) { continue }
    Write-Info "xu ly: $r  (co the mat 10-60 giay)"
    & takeown /F "$r" /R /D Y 2>&1 | Out-Null
    & icacls "$r" /reset /T /C /Q 2>&1 | Out-Null
    & icacls "$r" /grant "${grp}:(OI)(CI)F" /T /C /Q 2>&1 | Out-Null
    & icacls "$r" /grant "${me}:(OI)(CI)F"  /T /C /Q 2>&1 | Out-Null
  }
  foreach ($f in @("$Root\platforms.lock", "$Root\packages.lock")) {
    if (Test-Path -LiteralPath $f) {
      Write-Info "xu ly file: $f"
      & icacls "$f" /reset 2>&1 | Out-Null
      & icacls "$f" /grant "${grp}:F" 2>&1 | Out-Null
      & icacls "$f" /grant "${me}:F"  2>&1 | Out-Null
    }
  }
}

# ---------------------------------------------------------------------------
# 4) Kiem tra lai
# ---------------------------------------------------------------------------
Write-Head '4) Kiem tra ket qua'
$checks = @(
  "$Root\packages\tool-esptoolpy\esptool.py",
  "$Root\packages\tool-esptoolpy\esptool\__init__.py",
  "$Root\packages\tool-esptoolpy\_contrib\intelhex\__init__.py",
  "$Root\packages\tool-esptoolpy\_contrib\ecdsa\__init__.py",
  "$Root\packages\tool-esptoolpy\_contrib\six.py"
)
$fail = 0
foreach ($c in $checks) {
  if (-not (Test-Path -LiteralPath $c)) { Write-Info "khong co: $c"; continue }
  try {
    $len = (Get-Item -LiteralPath $c -ErrorAction Stop).Length
    Write-Ok "$c  ($len byte)"
  } catch {
    Write-Bad "$c  -- $($_.Exception.Message)"
    $fail++
  }
}

try {
  $v = & python "$Root\packages\tool-esptoolpy\esptool.py" version 2>&1
  Write-Host ''
  Write-Host '  esptool version:' -ForegroundColor Gray
  $v | ForEach-Object { Write-Host "    $_" -ForegroundColor Gray }
} catch { Write-Bad "khong chay duoc esptool.py: $($_.Exception.Message)"; $fail++ }

Write-Host ''
if ($fail -eq 0) {
  Write-Host 'XONG: ACL da duoc sua. Tiep theo chay build/flash binh thuong:' -ForegroundColor Green
  Write-Host '  pio run -d D:\Marlin\lcd_panel -e panel_esp32_cyd -t upload --upload-port COM3' -ForegroundColor Green
} else {
  Write-Host "CON $fail muc van loi - gui lai ket qua nay de xu ly tiep." -ForegroundColor Yellow
}
exit $fail
