# ===========================================================================
#  cydlcd/tools/set_cydlcd_mode.ps1
#
#  Bat / tat che do "man hinh rieng" (ExtUI CYDLCD_SERIAL_UI) trong Marlin.
#
#      pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode on     # bat man hinh CYD
#      pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode off    # tra ve man 12864
#      pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode status # xem dang bat gi
#
#  Vi sao phai doi 5 dong nay: ExtUI chiem chu ky MarlinUI::init_lcd()/update() nen KHONG the
#  vua bat CYDLCD_SERIAL_UI vua de man hinh gan truc tiep (link se bao "multiple definition").
#  ExtUI cung khong dung he thong menu cua MarlinUI => cac menu tuy bien phai chuyen sang man hinh CYD.
# ===========================================================================
param(
  [ValidateSet('on','off','status')]
  [string]$Mode = 'status'
)

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # ...\Marlin (repo root)
if (-not (Test-Path (Join-Path $root 'Marlin\Configuration.h'))) { $root = (Get-Location).Path }

$cfg  = Join-Path $root 'Marlin\Configuration.h'
$cfga = Join-Path $root 'Marlin\Configuration_adv.h'

# name, file, bat khi o che do man hinh CYD?, dong goc (nguyen van)
$items = @(
  @{ Name = 'CYDLCD_SERIAL_UI';   File = $cfg;  On = $true;  Line = '#define CYDLCD_SERIAL_UI' },
  @{ Name = 'MKS_MINI_12864_V3'; File = $cfg;  On = $false; Line = '#define MKS_MINI_12864_V3' },
  @{ Name = 'NEOPIXEL_LED';      File = $cfg;  On = $false; Line = '#define NEOPIXEL_LED' },
  @{ Name = 'CUSTOM_MENU_MAIN';  File = $cfga; On = $false; Line = '#define CUSTOM_MENU_MAIN' },
  @{ Name = 'LED_CONTROL_MENU';  File = $cfga; On = $false; Line = '  #define LED_CONTROL_MENU' }
)

function Get-Line([string]$path, [string]$name) {
  Select-String -Path $path -Pattern ("^\s*(//)?#define\s+" + [regex]::Escape($name) + "\s*$") |
    Select-Object -First 1
}

if ($Mode -eq 'status') {
  foreach ($it in $items) {
    $m = Get-Line $it.File $it.Name
    $state = if (-not $m) { 'KHONG THAY' } elseif ($m.Line -match '^\s*//') { 'tat' } else { 'BAT' }
    "{0,-20} {1}" -f $it.Name, $state
  }
  return
}

$changed = 0
foreach ($it in $items) {
  $m = Get-Line $it.File $it.Name
  if (-not $m) { Write-Warning "Khong tim thay dong '#define $($it.Name)' trong $($it.File)"; continue }
  $indent  = ($m.Line -replace '^(\s*).*$', '$1')
  $wantOn  = if ($Mode -eq 'on') { $it.On } else { -not $it.On }
  $isOn    = ($m.Line -notmatch '^\s*//')
  if ($wantOn -eq $isOn) { continue }
  $new = if ($wantOn) { $indent + $it.Line.TrimStart() } else { $indent + '//' + $it.Line.TrimStart() }
  $raw   = [System.IO.File]::ReadAllText($it.File)
  $origBytes = [System.IO.File]::ReadAllBytes($it.File)
  # Giu NGUYEN kieu xuong dong cua file (repo nay dung LF; Get-Content/WriteAllLines se doi thanh CRLF
  # lam diff hong het file)
  $nl    = if ($raw.Contains("`r`n")) { "`r`n" } else { "`n" }
  $lines = $raw -split "`r?`n"
  $lines[$m.LineNumber - 1] = $new
  # Ghi lai KHONG BOM (PowerShell 5.1 -Encoding UTF8 se them BOM vao dau file)
  $enc = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($it.File, ($lines -join $nl), $enc)

  # --- Kiem tra an toan: file phai con dung UTF-8, dung so dong, khong phinh to ---
  # (mot lan truoc da tung lam hong file config vi Get-Content/Set-Content doc sai encoding)
  $newBytes = [System.IO.File]::ReadAllBytes($it.File)
  $strict = New-Object System.Text.UTF8Encoding($false, $true)
  $okUtf8 = $true
  try { [void]$strict.GetString($newBytes) } catch { $okUtf8 = $false }
  $newLines = ([System.IO.File]::ReadAllText($it.File)) -split "`r?`n"
  $okLines = ($newLines.Count -eq $lines.Count)
  $okSize  = ($newBytes.Length -lt ($origBytes.Length * 1.05 + 64))
  if (-not ($okUtf8 -and $okLines -and $okSize)) {
    [System.IO.File]::WriteAllBytes($it.File, $origBytes)   # tra lai nguyen trang
    throw "Kiem tra an toan THAT BAI khi ghi $($it.File) (utf8=$okUtf8 dong=$okLines size=$okSize) -> da hoan tac, khong doi gi."
  }
  $changed++
  "{0,-20} -> {1}" -f $it.Name, $(if ($wantOn) { 'BAT' } else { 'tat' })
}

if ($changed -eq 0) { "Khong co gi doi (da o dung che do '$Mode')." }
else { "Da doi $changed dong. Build lai: pio run -e mks_monster8" }
