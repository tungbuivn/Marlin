#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Tai file G-code tu WINDOWS len may in 3D qua WiFi: gui cho PANEL (LCD CYD co HTTP server rieng,
    panel tu day tiep sang ESP3D roi tu gui M22/M21 cho Marlin) - hoac gui THANG cho ESP3D
    (-Target esp3d). Tu dong tim thiet bi, khong phai nhap IP.

.DESCRIPTION
    HAI DUONG GUI (chon bang -Target):

      A) -Target panel  (MAC DINH) - khuyen nghi
         PC --WiFi--> PANEL (HTTP server cua chinh panel, xem hop dong ben duoi)
                      --> panel ghi vao the TF cua panel
                      --> panel POST /files len ESP3D --> ESP3D ghi vao the SD cua Marlin
                      --> panel gui M22 + M21 cho Marlin
         Uu diem: PC chi can o CUNG MANG WiFi voi panel (mang nha), KHONG can doi mang; panel lo
         phan M22/M21; theo doi duoc tien do qua GET /status.
         => KHONG can -InitSd (panel tu gui M22/M21). Neu van truyen -InitSd thi script gui them
            M22/M21 SAU KHI panel bao xong (vo hai, chi de chac).

      B) -Target esp3d  - du phong (duong cu)
         PC --WiFi--> module ESP3D (POST /files) --> the SD cua Marlin.
         Chi chay khi PC NOI DUOC toi ESP3D: hoac PC dang noi AP "Voron2308" cua module
         (192.168.0.1 - mat internet tam thoi), hoac ESP3D da duoc dat AP+STA de no cung vao
         mang nha. CAN gui M22/M21 (ESP3D khong tu lam) => nen dung -InitSd.

    HOP DONG HTTP CUA PANEL (client nay la ban tham chieu; firmware phai khop):
      GET  /status                -> 200 application/json. Nhan dang panel khi co 1 trong:
            {"panel":"TBT", ...}  /  {"device":"TBT-PANEL", ...}  /  chuoi "TBT-PANEL" trong body
            /  hoac JSON co CA truong kieu "state" VA truong kieu "total_bytes|rx_bytes".
            Nen co: state (idle|receiving|posting|done|error), rx_bytes, total_bytes,
                    posted_bytes, file, result/message
      GET  /                      -> trang HTML cho dien thoai/PC gui file; NEN chua chuoi "TBT-PANEL"
      POST /upload?token=<token>  -> multipart/form-data; field text name="name" = ten file dich
            (dung TRUOC), phan file name="file" filename="<ten>"; 200 {"status":"Ok"} | {"status":"Error"}
            Client con gui header  X-Panel-Token: <token>  (nhan ca 2 kieu xac thuc).
      (Co the ep chuoi nhan dang khac bang -PanelMarker "<chuoi>".)

    âš ï¸ VI SAO KHONG DUNG Invoke-WebRequest -Form / MultipartFormDataContent:
      ESP3D chay ESP8266WebServer: Parsing-impl.h::_parseForm cat chuoi Content-Disposition theo dau
      '=' DAU TIEN va LUON bo 1 ky tu ngay sau dau '=':
          argName     = line.substring(nameStart + 2);                       // bo '="'
          argFilename = argName.substring(nameStart + 2, len - 1);           // bo '="' va ky tu cuoi
      => GIA TRI BAT BUOC PHAI CO DAU " bao quanh:
          name="filename"; filename="benchy.gcode"  -> filename = benchy.gcode   DUNG
          name=filename;   filename=benchy.gcode    -> filename = enchy.gcod     SAI (mat 2 ky tu)
      .NET Framework (Windows PowerShell 5.1) sinh Content-Disposition KHONG co dau " cho cac gia tri
      dang token ("name", "file", "benchy.gcode") => dung MultipartFormDataContent la file len the SD
      SAI TEN. Vi vay script nay TU DUNG body multipart bang byte va gui bang TcpClient tho:
      kiem soat tuyet doi framing, Content-Length tu tinh, KHONG chunked, KHONG Expect: 100-continue,
      bao duoc tien do %/KB/s (file 10 MB van muot).
      (ESP32 WebServer dong bo cung cat chuoi y het; ESPAsyncWebServer thi thoang hon - dung body
       nay thi chay duoc voi ca hai, vi trinh duyet cung luon gui co dau ".)

    THU TU TU TIM THIET BI (bo qua khi co -NoDiscover):
      1. -Host (neu co)                          -> nguon: host
      2. CACHE: IP cua lan tim THANH CONG gan nhat        -> nguon: cache
         (%LOCALAPPDATA%\esp3d-upload\last-target.txt, moi dich 1 dong "panel=<ip>" / "esp3d=<ip>")
         => binh thuong chi mat ~0,2-0,5 s, KHONG phai mDNS hay quet lai LAN (17-27 s).
         IP cu / doi IP thi tu dong tim lai o cac buoc duoi roi ghi de. Tat bang -NoCache.
      3. mDNS: TBTPANEL.local, esp3d.local, MYESP.local   -> nguon: mdns
      4. gateway cua card mang dang dung         -> nguon: gateway
         (PC noi vao AP cua module thi gateway CHINH LA module)
      5. quet LAN /24 cua card dang hoat dong, SONG SONG nhieu luong: loc "port 80 mo" truoc,
         roi moi XAC NHAN chu ky thiet bi                     -> nguon: quet
      6. danh sach co dinh: 192.168.0.1, 192.168.4.1, 192.168.1.1, 192.168.1.50, 192.168.0.50
                                                             -> nguon: danh sach
      Moi ung vien timeout ngan (1-1,5 s); buoc quet co tran -ScanBudgetSec.
      Tim duoc thi in:  [PANEL] thay <IP> (nguon: cache|mdns|gateway|quet|danh sach) va luu cache
      (%LOCALAPPDATA%\esp3d-upload\last-target.txt) de lan sau vao thang.

    âš ï¸ CANH BAO MANG (doc truoc khi hoi "sao khong thay may in"):
      * Duong PANEL: panel se vao WiFi STA cua mang nha (TP-Link_31A0, 2.4 GHz) => PC dang o mang nha
        la gui duoc NGAY, khong phai doi mang. Neu panel chua noi duoc WiFi nha thi no chua co IP.
      * Duong ESP3D: module dang phat AP rieng "Voron2308" voi IP TINH 192.168.0.1 (config.h:370-376),
        TRUNG dai LAN nha (router TP-Link thuong cung la 192.168.0.1). Khi PC dang o mang nha thi
        192.168.0.1 chinh la ROUTER, KHONG phai ESP3D => 2 dau khong thay nhau. Phai noi PC vao AP
        "Voron2308" (mat internet tam thoi) HOAC dat ESP3D o che do AP+STA de no cung vao mang nha.
        (DEFAULT_* trong config.h chi la gia tri mac dinh; cau hinh THAT nam trong EEPROM.)

.PARAMETER File
    Duong dan file G-code tren PC (bat buoc).
.PARAMETER Target
    'panel' (mac dinh) = gui len HTTP server cua panel;  'esp3d' = gui thang len module ESP3D.
.PARAMETER Host
    Ep IP/hostname cua thiet bi dich, bo qua buoc tu tim (van XAC NHAN chu ky).
    (Tham so that la -Host; trong code phai dat ten khac vi $Host la bien doc quyen cua PowerShell,
     nen dung [Alias('Host')].)
.PARAMETER Port
    Cong HTTP cua thiet bi dich. Mac dinh 80.
.PARAMETER Name
    Ten file se ghi tren the SD (mac dinh: ten file tren PC).
.PARAMETER Token
    Token xac thuc cua panel (mac dinh TBT-3D-up - cung gia tri PANEL_BT_TOKEN trong panel_config.h).
    Gui ca 2 kieu: ?token=... va header X-Panel-Token.
.PARAMETER PanelMarker
    Chuoi nhan dang panel (mac dinh: nhan nhieu chuoi - xem phan HOP DONG o tren).
.PARAMETER InitSd
    Gui M22 + M21 cho Marlin qua -MarlinPort. Duong esp3d: NEN dung. Duong panel: khong can
    (panel tu gui), nhung neu truyen thi script gui them SAU khi panel bao xong.
.PARAMETER MarlinPort
    Cong serial cua Marlin (USB CDC STM32). Mac dinh COM4, baud 250000, DtrEnable = $true.
.PARAMETER TimeoutSec
    Tran thoi gian cho RIENG lan upload (giay). Mac dinh 300 (file 10 MB nen de >= 120).
.PARAMETER NoDiscover
    KHONG tu tim: chi dung -Host, neu khong co thi dung IP da luu trong cache.
.PARAMETER NoCache
    BO QUA buoc dung IP da luu (cache). Dung khi nghi IP trong cache sai / muon do lai tu dau.
    Cache: %LOCALAPPDATA%\esp3d-upload\last-target.txt, moi dich 1 dong "panel=<ip>" / "esp3d=<ip>".
.PARAMETER ScanThreads
    So luong quet song song khi quet LAN. Mac dinh 40 (nen trong khoang 30-50).
.PARAMETER ScanTimeoutMs
    Timeout TCP cho MOI IP khi quet LAN (ms). Mac dinh 1000.
.PARAMETER ScanBudgetSec
    Tran tong cho buoc quet LAN (giay). Mac dinh 25.
.PARAMETER ProbeTimeoutMs
    Timeout cho moi lan xac nhan chu ky thiet bi (ms). Mac dinh 1500.
.PARAMETER StatusIntervalMs
    Khoang cach giua 2 lan doc GET /status khi theo doi tien do (ms). Mac dinh 1000.
.PARAMETER StatusTimeoutSec
    Tran thoi gian theo doi GET /status sau khi gui xong (giay). Mac dinh 900.
.PARAMETER NoStatus
    Khong theo doi GET /status (chi dua vao phan hoi cua POST /upload).

.EXAMPLE
    .\panel-upload.ps1 -File .\benchy.gcode
    # tu tim PANEL (mDNS -> gateway -> quet LAN -> danh sach) roi gui; panel tu GHI VAO THE SD + M22/M21

.EXAMPLE
    .\panel-upload.ps1 -File big.gcode -Host 192.168.0.174
    # ep IP cua panel

.EXAMPLE
    .\panel-upload.ps1 -File part.gcode -Target esp3d -InitSd -MarlinPort COM4
    # du phong: gui thang len ESP3D (chi khi PC noi AP Voron2308 / ESP3D o AP+STA), roi gui M22/M21

.EXAMPLE
    .\panel-upload.ps1 -File part.gcode -Name PART3.GCO -StatusTimeoutSec 1800
    # doi ten tren the SD + cho panel ghi the lau hon

.NOTES
    MA THOAT: 0 = gui OK (va M21 OK neu co -InitSd)
              2 = loi phia server (HTTP khac 200 / status khac Ok / dut ket noi giua chung)
              3 = KHONG tim thay thiet bi dich
              4 = loi file/tham so (khong thay file, file rong, ten file khong hop le...)

    NGUON DOI CHIEU (da doc truc tiep):
      D:\arduino\ESP3D-direct-sd\esp3d\webinterface.cpp:386   on("/files", HTTP_ANY, handleSDFileList, SPIShareSdCardUpload)
      D:\arduino\ESP3D-direct-sd\esp3d\syncwebserver.cpp:443-696, 983-1131
      D:\arduino\ESP3D-direct-sd\esp3d\command.cpp:1916+      [ESP800] -> "FW version:"
      D:\arduino\ESP3D-direct-sd\esp3d\config.h:34,37,65,89,370-378
      C:\Users\tungbui\.platformio\packages\framework-arduinoespressif32\libraries\WebServer\src\Parsing.cpp
      (ESP8266WebServer 3.1.2 - platform espressif8266@4.2.1 trong platformio.ini)
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$File,
    [ValidateSet('panel', 'esp3d')][string]$Target = 'panel',
    [Alias('Host')][string]$TargetHost,
    [int]$Port = 80,
    [string]$Name,
    [string]$Token = 'TBT-3D-up',
    [string]$PanelMarker,
    [switch]$InitSd,
    [string]$MarlinPort = 'COM4',
    [int]$TimeoutSec = 300,
    [switch]$NoDiscover,
    [switch]$NoCache,
    [int]$ScanThreads = 40,
    [int]$ScanTimeoutMs = 1000,
    [int]$ScanBudgetSec = 25,
    [int]$ProbeTimeoutMs = 1500,
    [int]$StatusIntervalMs = 1000,
    [int]$StatusTimeoutSec = 900,
    [switch]$NoStatus
)

$ErrorActionPreference = 'Continue'

try { Add-Type -AssemblyName System.Net.Http -ErrorAction Stop } catch { }

$CACHE_DIR  = Join-Path $env:LOCALAPPDATA 'esp3d-upload'
$CACHE_FILE = Join-Path $CACHE_DIR 'last-target.txt'

# ---------------------------------------------------------------------------
# CACHE IP (2026-10-14): luu IP cua lan tim THANH CONG gan nhat cho tung dich
#   File: %LOCALAPPDATA%\esp3d-upload\last-target.txt, moi dong 1 dang  "<target>=<ip>"
#         panel=192.168.0.146
#         esp3d=192.168.0.1
#   Lan sau: thu IP da luu TRUOC TIEN (1 lan HTTP ngan ~1,5 s). Dung thi xong ngay (khong mDNS,
#   khong quet LAN 17-27 s); sai (IP doi / thiet bi tat) thi tu tim lai nhu cu roi ghi de.
#   Ban cu (file chi co 1 dong IP) van doc duoc.
#   Tat: -NoCache.
# ---------------------------------------------------------------------------
function Get-CachedIp([string]$tgt) {
    if (-not (Test-Path -LiteralPath $CACHE_FILE)) { return $null }
    $lines = @(Get-Content -LiteralPath $CACHE_FILE -ErrorAction SilentlyContinue)
    foreach ($ln in $lines) {
        $s = "$ln".Trim()
        if (-not $s) { continue }
        if ($s -match '^([A-Za-z0-9_]+)\s*=\s*(\S+)$') {
            if ($matches[1].ToLower() -eq $tgt.ToLower()) { return $matches[2] }
        } elseif ($s -match '^\d{1,3}(\.\d{1,3}){3}$') {
            return $s                      # dinh dang CU: 1 dong IP duy nhat
        }
    }
    return $null
}
function Set-CachedIp([string]$tgt, [string]$ip) {
    try {
        if (-not (Test-Path -LiteralPath $CACHE_DIR)) { [void](New-Item -ItemType Directory -Path $CACHE_DIR -Force) }
        $map = [ordered]@{}
        if (Test-Path -LiteralPath $CACHE_FILE) {
            foreach ($ln in @(Get-Content -LiteralPath $CACHE_FILE -ErrorAction SilentlyContinue)) {
                $s = "$ln".Trim()
                if ($s -match '^([A-Za-z0-9_]+)\s*=\s*(\S+)$') { $map[$matches[1].ToLower()] = $matches[2] }
                elseif ($s -match '^\d{1,3}(\.\d{1,3}){3}$') { $map['panel'] = $s }
            }
        }
        $map[$tgt.ToLower()] = $ip
        $out = @($map.Keys | ForEach-Object { "{0}={1}" -f $_, $map[$_] })
        Set-Content -LiteralPath $CACHE_FILE -Value $out -Encoding ASCII
        Write-Dim ("  (da luu cache {0}={1} vao {2})" -f $tgt.ToLower(), $ip, $CACHE_FILE)
    } catch { Write-Dim ("  (khong ghi duoc cache: {0})" -f $_.Exception.Message) }
}
$FIXED_LIST = @('192.168.0.1', '192.168.4.1', '192.168.1.1', '192.168.1.50', '192.168.0.50')
$PANEL_MARKERS_DEFAULT = @('TBT-PANEL', 'TBTPANEL', 'TBT_PANEL', 'PANEL-TBT', 'TBT-3D-up')
$PANEL_FIELD_NAME = 'file'      # ten field cua phan file khi POST /upload
$ESP3D_FIELD_NAME = 'filename'  # ESP3D khong quan tam ten field (chi can filename="..."), dat cho ro nghia

# ---------------------------------------------------------------------------
# Log (cung van phong voi bt-upload.ps1 / panel-log.ps1)
# ---------------------------------------------------------------------------
function Write-Head([string]$m) { Write-Host $m -ForegroundColor Cyan }
function Write-Step([string]$m) { Write-Host $m -ForegroundColor White }
function Write-Ok  ([string]$m) { Write-Host $m -ForegroundColor Green }
function Write-Warn([string]$m) { Write-Host $m -ForegroundColor Yellow }
function Write-Err ([string]$m) { Write-Host $m -ForegroundColor Red }
function Write-Dim ([string]$m) { Write-Host $m -ForegroundColor DarkGray }
function Write-Note([string]$m) { Write-Host $m -ForegroundColor DarkYellow }

# ---------------------------------------------------------------------------
# Mang
# ---------------------------------------------------------------------------
function Get-ActiveIpv4 {
    $out = @()
    try {
        foreach ($c in (Get-NetIPConfiguration -ErrorAction Stop)) {
            try { if ($c.NetAdapter -and $c.NetAdapter.Status -ne 'Up') { continue } } catch { }
            if (-not $c.IPv4Address) { continue }
            $ip = @($c.IPv4Address)[0].IPAddress
            if (-not $ip -or $ip -like '169.254.*') { continue }
            $pfx = 24
            try { $pfx = [int](@($c.IPv4Address)[0].PrefixLength) } catch { }
            $gw = $null
            try { if ($c.IPv4DefaultGateway) { $gw = @($c.IPv4DefaultGateway)[0].NextHop } } catch { }
            $out += [pscustomobject]@{ Adapter = $c.InterfaceAlias; IP = $ip; Prefix = $pfx; Gateway = $gw }
        }
    } catch { }
    return $out
}

function Get-WlanSsid {
    $ssid = $null
    try {
        foreach ($ln in (netsh wlan show interfaces 2>$null)) {
            if ($ln -match '^\s*SSID\s*:\s*(.+?)\s*$') { $ssid = $Matches[1]; break }
        }
    } catch { }
    return $ssid
}

# ---------------------------------------------------------------------------
# HTTP GET nho (dung HttpClient, TAT proxy de proxy he thong khong an mat IP LAN)
# ---------------------------------------------------------------------------
function Get-ShortError {
    # PowerShell boc loi .NET trong MethodInvocationException => phai bÃ³c den tan cung moi doc duoc.
    param($ErrorRecord)
    $ex = $null
    try { $ex = $ErrorRecord.Exception } catch { }
    while ($ex -and $ex.InnerException) { $ex = $ex.InnerException }
    $msg = ''
    if ($ex) { $msg = "$($ex.Message)" } else { $msg = "$ErrorRecord" }
    if ($msg -match 'canceled|cancelled|timed out|timeout') { $msg = 'het thoi gian cho phan hoi' }
    elseif ($msg -match 'actively refused') { $msg = 'bi tu choi ket noi (khong co gi o port do)' }
    elseif ($msg -match 'Unable to connect|No connection could be made|khong the ket noi') { $msg = 'khong ket noi duoc' }
    elseif ($msg -match 'Name or service not known|host') { $msg = 'khong phan giai duoc ten' }
    if ($msg.Length -gt 90) { $msg = $msg.Substring(0, 90) + '...' }
    return $msg
}

function Invoke-HttpGet {
    param([string]$Ip, [int]$Port = 80, [string]$Path = '/', [int]$TimeoutMs = 1500)
    $res = [pscustomobject]@{ Ok = $false; Status = 0; Body = ''; Header = ''; Error = ''; Ms = 0 }
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $client = $null
    $url = 'http://' + $Ip + ':' + $Port + $Path
    try {
        $handler = New-Object System.Net.Http.HttpClientHandler
        $handler.UseProxy = $false
        $client = New-Object System.Net.Http.HttpClient($handler)
        $client.Timeout = [TimeSpan]::FromMilliseconds($TimeoutMs)
        $r = $client.GetAsync($url).GetAwaiter().GetResult()
        $body = $r.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        $hdr = ''
        try {
            foreach ($h in $r.Headers) { $hdr += ($h.Key + ': ' + ($h.Value -join ',') + "`n") }
            foreach ($h in $r.Content.Headers) { $hdr += ($h.Key + ': ' + ($h.Value -join ',') + "`n") }
        } catch { }
        $res.Ok = $true
        $res.Status = [int]$r.StatusCode
        $res.Body = $body
        $res.Header = $hdr
    } catch {
        $res.Error = Get-ShortError $_
    } finally {
        if ($client) { try { $client.Dispose() } catch { } }
    }
    $sw.Stop()
    $res.Ms = [int]$sw.ElapsedMilliseconds
    return $res
}

# ---------------------------------------------------------------------------
# Doc JSON /status cua panel (chá»‹u duoc nhieu ten truong khac nhau)
# ---------------------------------------------------------------------------
function Get-FieldValue {
    param($Obj, [string[]]$Names)
    if (-not $Obj) { return $null }
    try {
        foreach ($p in $Obj.PSObject.Properties) {
            foreach ($n in $Names) {
                if ($p.Name -eq $n) { return $p.Value }
            }
        }
    } catch { }
    return $null
}

function ConvertFrom-PanelStatus {
    param([string]$Json)
    if (-not $Json) { return $null }
    $o = $null
    try { $o = $Json | ConvertFrom-Json -ErrorAction Stop } catch { return $null }
    if (-not $o) { return $null }
    # Neu la mang JSON thi lay phan tu dau
    if ($o -is [System.Array]) { if ($o.Count -gt 0) { $o = $o[0] } else { return $null } }
    if (-not ($o -is [psobject])) { return $null }

    $state  = Get-FieldValue $o @('state', 'status', 'phase', 'stage')
    $rx     = Get-FieldValue $o @('rx_bytes', 'received', 'received_bytes', 'bytes_received', 'rx')
    $total  = Get-FieldValue $o @('total_bytes', 'total', 'size', 'bytes_total')
    $posted = Get-FieldValue $o @('posted_bytes', 'posted', 'esp3d_bytes', 'sent_bytes', 'uploaded_bytes')
    $result = Get-FieldValue $o @('result', 'message', 'error', 'msg', 'detail', 'last_error')
    $fname  = Get-FieldValue $o @('file', 'filename', 'name', 'current_file')
    $panel  = Get-FieldValue $o @('panel', 'device', 'device_name', 'hostname', 'host')

    $known = ($null -ne $state) -or ($null -ne $rx) -or ($null -ne $total)
    return [pscustomobject]@{
        Raw    = $Json
        Panel  = $panel
        State  = $state
        Rx     = $rx
        Total  = $total
        Posted = $posted
        Result = $result
        File   = $fname
        Known  = $known
    }
}

function Test-PanelSignature {
    # Nhan dien panel: (a) co chuoi nhan dang, (b) JSON co truong panel/device chua 'TBT',
    # (c) JSON co CA truong kieu state VA truong kieu rx/total (schema cua hop dong).
    param([string]$Body, [string]$Header, [string[]]$Markers)
    if (-not $Body) { $Body = '' }
    if (-not $Header) { $Header = '' }
    $all = $Body + "`n" + $Header
    foreach ($m in $Markers) {
        if ($m -and ($all -match [regex]::Escape($m))) { return $true }
    }
    $o = $null
    try { $o = $Body | ConvertFrom-Json -ErrorAction Stop } catch { return $false }
    if (-not $o) { return $false }
    $p = Get-FieldValue $o @('panel', 'device', 'device_name')
    if ($p -and ("$p" -match 'TBT')) { return $true }
    $st = Get-FieldValue $o @('state', 'status', 'phase')
    $rx = Get-FieldValue $o @('rx_bytes', 'received', 'total_bytes', 'total', 'size', 'posted_bytes')
    if ($st -and $rx) { return $true }
    return $false
}

# ---------------------------------------------------------------------------
# Xac nhan thiet bi dich
# ---------------------------------------------------------------------------
function Test-Esp3dAt {
    # DUNG LA ESP3D khi GET /command?plain=[ESP800] chua "FW version:" (khong dung tá»›i the SD).
    param([string]$Ip, [int]$Port = 80, [int]$TimeoutMs = 1500)
    $g = Invoke-HttpGet -Ip $Ip -Port $Port -Path '/command?plain=%5BESP800%5D' -TimeoutMs $TimeoutMs
    $out = [pscustomobject]@{ Esp = $false; Status = $g.Status; Ms = $g.Ms; Error = $g.Error; Info = '' }
    if ($g.Ok -and $g.Body -match 'FW version:') {
        $out.Esp = $true
        $out.Info = (Get-FirstLine $g.Body)
    }
    return $out
}

function Test-PanelAt {
    # DUNG LA PANEL khi /status (hoac /) khop chu ky nhan dang cua hop dong.
    param([string]$Ip, [int]$Port = 80, [int]$TimeoutMs = 1500, [string[]]$Markers = $PANEL_MARKERS_DEFAULT)
    $out = [pscustomobject]@{ Panel = $false; Esp3d = $false; How = ''; Status = $null; Info = ''; Ms = 0 }

    # 1) /status  (u tien - vua nhan dang vua lay duoc tien do)
    $g = Invoke-HttpGet -Ip $Ip -Port $Port -Path '/status' -TimeoutMs $TimeoutMs
    if ($g.Ok -and $g.Status -eq 200 -and (Test-PanelSignature -Body $g.Body -Header $g.Header -Markers $Markers)) {
        $out.Panel = $true
        $out.How = '/status'
        $out.Status = ConvertFrom-PanelStatus $g.Body
        $out.Info = (Get-FirstLine $g.Body)
        $out.Ms = $g.Ms
        return $out
    }
    $statusErr = if ($g.Ok) { "HTTP $($g.Status)" } else { $g.Error }

    # Toi uu: neu /status that bai ngay o tang TCP (khong ai nghe cong) thi KHONG thu tiep
    # ESP800 va '/': khong co HTTP server o do. Nho vay ung vien "chet" chi ton 1 lan timeout.
    if (-not $g.Ok) {
        $out.Info = "/status: $statusErr"
        $out.Ms = $g.Ms
        return $out
    }

    # 2) Khong phai panel thi kiem xem co phai ESP3D khong (de bao dung cho nguoi dung)
    $e = Test-Esp3dAt -Ip $Ip -Port $Port -TimeoutMs $TimeoutMs
    if ($e.Esp) {
        $out.Esp3d = $true
        $out.Info = $e.Info
        $out.Ms = $e.Ms
        return $out
    }

    # 3) GET /  (trang HTML cho dien thoai - du phong nhan dang)
    $h = Invoke-HttpGet -Ip $Ip -Port $Port -Path '/' -TimeoutMs $TimeoutMs
    if ($h.Ok -and $h.Status -eq 200 -and (Test-PanelSignature -Body $h.Body -Header $h.Header -Markers $Markers)) {
        $out.Panel = $true
        $out.How = '/'
        $out.Info = (Get-FirstLine $h.Body)
        $out.Ms = $h.Ms
        return $out
    }
    $out.Info = "/status: $statusErr; /: $(if ($h.Ok) { "HTTP $($h.Status)" } else { $h.Error })"
    $out.Ms = $g.Ms
    return $out
}

function Get-FirstLine([string]$Text) {
    if (-not $Text) { return '' }
    $one = ($Text -split "`r?`n")[0]
    if ($one.Length -gt 140) { $one = $one.Substring(0, 140) + '...' }
    return $one
}

function Format-StatusLine($s, [int64]$ExpectedBytes = 0) {
    if (-not $s) { return '' }
    $parts = @()
    if ($null -ne $s.State -and "$($s.State)" -ne '') { $parts += "state=$($s.State)" }
    if ($null -ne $s.Rx -or $null -ne $s.Total) {
        $rxTxt = if ($null -ne $s.Rx) { "$($s.Rx)" } else { '?' }
        $toTxt = if ($null -ne $s.Total) { "$($s.Total)" } else { '?' }
        # LUU Y firmware: ESP32 core 2.0.17 dat upload.totalSize = 0 o UPLOAD_FILE_START (chi cong don
        # dan trong luc ghi) => panel doc total_bytes o START se luon nhan 0. Khi do dung SO BYTE CUA
        # CHINH CLIENT lam moc tinh % (phan file panel nhan dung bang kich thuoc file ta gui).
        $den = 0
        try { if ($null -ne $s.Total) { $den = [int64]$s.Total } } catch { $den = 0 }
        if ($den -le 0 -and $ExpectedBytes -gt 0) { $den = $ExpectedBytes; $toTxt = "$ExpectedBytes*" }
        $pct = ''
        try {
            $num = 0
            if ($null -ne $s.Rx) { $num = [int64]$s.Rx }
            if ($den -gt 0) { $pct = (' {0}%' -f [int](100 * $num / $den)) }
        } catch { }
        $parts += "rx=$rxTxt/$toTxt$pct"
    }
    if ($null -ne $s.Posted -and "$($s.Posted)" -ne '') { $parts += "posted=$($s.Posted)" }
    if ($null -ne $s.File -and "$($s.File)" -ne '') { $parts += "file=$($s.File)" }
    if ($null -ne $s.Result -and "$($s.Result)" -ne '') { $parts += "result=$($s.Result)" }
    $line = ($parts -join '  ')
    if (-not $line) { $line = (Get-FirstLine $s.Raw) }
    return $line
}

# ---------------------------------------------------------------------------
# Quet LAN song song (runspace pool; worker chi dung .NET, khong can cmdlet nao)
# ---------------------------------------------------------------------------
function Invoke-LanScan {
    param(
        [string[]]$Subnets,
        [string[]]$SelfIps,
        [int]$Port = 80,
        [int]$Threads = 40,
        [int]$TimeoutMs = 1000,
        [int]$BudgetSec = 25
    )
    $result = [pscustomobject]@{ Open = @(); Count = 0; Sec = 0; Aborted = $false; Targets = 0 }

    $targets = New-Object System.Collections.ArrayList
    foreach ($sn in $Subnets) {
        for ($h = 1; $h -le 254; $h++) {
            $ip = "$sn.$h"
            if ($SelfIps -contains $ip) { continue }
            [void]$targets.Add($ip)
        }
    }
    $result.Targets = $targets.Count
    if ($targets.Count -eq 0) { return $result }

    Write-Host ("  {0} IP can thu (tran {1} s, {2} luong)..." -f $targets.Count, $BudgetSec, $Threads)

    $worker = @'
param($ip, $port, $timeoutMs, $sink)
$c = $null
try {
    $c = New-Object System.Net.Sockets.TcpClient
    $iar = $c.BeginConnect($ip, $port, $null, $null)
    if ($iar.AsyncWaitHandle.WaitOne($timeoutMs, $false)) {
        try { $c.EndConnect($iar) } catch { return }
        try { $sink.Enqueue($ip) } catch { }
    }
} catch { } finally { if ($c) { try { $c.Close() } catch { } } }
'@

    $open = New-Object 'System.Collections.Concurrent.ConcurrentQueue[string]'
    $pool = $null
    $jobs = New-Object System.Collections.ArrayList
    try {
        $pool = [runspacefactory]::CreateRunspacePool(1, $Threads)
        $pool.Open()
        foreach ($ip in $targets) {
            $ps = [powershell]::Create()
            $ps.RunspacePool = $pool
            [void]$ps.AddScript($worker)
            [void]$ps.AddArgument($ip)
            [void]$ps.AddArgument($Port)
            [void]$ps.AddArgument($TimeoutMs)
            [void]$ps.AddArgument($open)
            $h = $ps.BeginInvoke()
            [void]$jobs.Add([pscustomobject]@{ PS = $ps; H = $h })
        }

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $lastDone = -1
        while ($true) {
            $done = 0
            foreach ($j in $jobs) { if ($j.H.IsCompleted) { $done++ } }
            if ($done -ne $lastDone) {
                Write-Host ("`r  da kiem tra xong {0}/{1} IP ... {2:N0} s / tran {3} s" -f $done, $jobs.Count, $sw.Elapsed.TotalSeconds, $BudgetSec) -NoNewline -ForegroundColor DarkGray
                $lastDone = $done
            }
            if ($done -ge $jobs.Count) { break }
            if ($sw.Elapsed.TotalSeconds -gt $BudgetSec) { $result.Aborted = $true; break }
            Start-Sleep -Milliseconds 250
        }
        Write-Host ""
        foreach ($j in $jobs) {
            try { if ($j.H.IsCompleted) { [void]$j.PS.EndInvoke($j.H) } else { $j.PS.Stop() } } catch { }
            try { $j.PS.Dispose() } catch { }
        }
        $result.Sec = $sw.Elapsed.TotalSeconds
    } catch {
        Write-Warn ("  loi khi quet: {0}" -f $_.Exception.Message)
    } finally {
        if ($pool) { try { $pool.Close() } catch { } ; try { $pool.Dispose() } catch { } }
    }

    $result.Open = @($open.ToArray() | Sort-Object)
    $result.Count = $result.Open.Count
    return $result
}

# ---------------------------------------------------------------------------
# POST multipart bang socket tho (dung cho CA panel va ESP3D)
# ---------------------------------------------------------------------------
function New-MultipartParts {
    # name="name" (ten file dich) dung TRUOC, roi phan file name="<FieldName>"; filename="<ten>".
    # GIA TRI PHAI CO DAU " - xem phan giai thich o .DESCRIPTION.
    param([string]$FieldName, [string]$SendName)
    $boundary = '----TBT' + [Guid]::NewGuid().ToString('N')
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append('--').Append($boundary).Append("`r`n")
    [void]$sb.Append('Content-Disposition: form-data; name="name"').Append("`r`n`r`n")
    [void]$sb.Append($SendName).Append("`r`n")
    [void]$sb.Append('--').Append($boundary).Append("`r`n")
    [void]$sb.Append('Content-Disposition: form-data; name="').Append($FieldName).Append('"; filename="').Append($SendName).Append('"').Append("`r`n")
    [void]$sb.Append('Content-Type: application/octet-stream').Append("`r`n`r`n")
    return [pscustomobject]@{
        Boundary = $boundary
        Head     = [System.Text.Encoding]::ASCII.GetBytes($sb.ToString())
        Tail     = [System.Text.Encoding]::ASCII.GetBytes("`r`n--" + $boundary + "--`r`n")
    }
}

function Send-FileRaw {
    param(
        [string]$Ip,
        [int]$Port = 80,
        [string]$PathAndQuery = '/upload',
        [string]$LocalPath = '',
        [string]$SendName = '',
        [string]$FieldName = 'file',
        [string]$Token = '',
        [int]$TimeoutSec = 300,
        [int]$ChunkBytes = 32768
    )
    $res = [pscustomobject]@{
        Ok = $false; Status = 0; Body = ''; Error = ''
        Sent = 0; Sec = 0; KBps = 0; Attempt = 0
    }
    $total = [int64](Get-Item -LiteralPath $LocalPath).Length

    for ($attempt = 1; $attempt -le 2; $attempt++) {
        $res.Attempt = $attempt
        if ($attempt -gt 1) {
            Write-Warn "  ... loi ket noi -> THU LAI lan 2 sau 1,5 s"
            Start-Sleep -Milliseconds 1500
        }

        $parts = New-MultipartParts -FieldName $FieldName -SendName $SendName
        $contentLen = [int64]$parts.Head.Length + $total + [int64]$parts.Tail.Length

        $req = "POST {0} HTTP/1.1`r`n" +
               "Host: {1}:{2}`r`n" +
               "Content-Type: multipart/form-data; boundary={3}`r`n" +
               "Content-Length: {4}`r`n" +
               "Connection: close`r`n" +
               "Cache-Control: no-cache`r`n" +
               "{5}" +
               "`r`n"
        $tokenHdr = if ($Token) { "X-Panel-Token: $Token`r`n" } else { '' }
        $req = $req -f $PathAndQuery, $Ip, $Port, $parts.Boundary, $contentLen, $tokenHdr
        if ($req -isnot [string]) { $req = ($req -join '') }
        $reqBytes = [System.Text.Encoding]::ASCII.GetBytes($req)

        $tcp = $null; $ns = $null; $fs = $null
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            if ($attempt -eq 1) {
                Write-Host ("  POST http://{0}:{1}{2}  ({3:N0} byte file, {4} byte multipart)" -f $Ip, $Port, $PathAndQuery, $total, ($contentLen - $total)) -ForegroundColor Cyan
            }

            $tcp = New-Object System.Net.Sockets.TcpClient
            $iar = $tcp.BeginConnect($Ip, $Port, $null, $null)
            if (-not $iar.AsyncWaitHandle.WaitOne(5000, $false)) { throw "het thoi gian ket noi TCP ${Ip}:${Port}" }
            $tcp.EndConnect($iar)
            try { $tcp.NoDelay = $true } catch { }
            $ns = $tcp.GetStream()
            $ns.ReadTimeout = 20000
            $ns.WriteTimeout = 30000

            $ns.Write($reqBytes, 0, $reqBytes.Length)
            $ns.Write($parts.Head, 0, $parts.Head.Length)

            $fs = [System.IO.File]::OpenRead($LocalPath)
            $buf = New-Object byte[] $ChunkBytes
            $sent = [int64]0
            $lastPct = -1
            $lastPrint = [double]0
            while (($n = $fs.Read($buf, 0, $buf.Length)) -gt 0) {
                $ns.Write($buf, 0, $n)
                $sent += $n
                if ($sw.Elapsed.TotalSeconds -gt $TimeoutSec) {
                    throw ("qua thoi gian gui ({0} s) - da gui {1:N0}/{2:N0} byte" -f $TimeoutSec, $sent, $total)
                }
                $pct = [int](100 * $sent / $total)
                if ($pct -ne $lastPct -and ($sw.Elapsed.TotalSeconds - $lastPrint) -ge 0.3) {
                    $sec0 = $sw.Elapsed.TotalSeconds
                    $kbs0 = if ($sec0 -gt 0) { ($sent / 1KB) / $sec0 } else { 0 }
                    Write-Host ("  gui {0,3}%  {1,8:N0}/{2:N0} KB  {3,7:N1} KB/s" -f $pct, ($sent / 1KB), ($total / 1KB), $kbs0) -NoNewline
                    Write-Host "`r" -NoNewline
                    $lastPct = $pct
                    $lastPrint = $sec0
                }
                if ($sent -ge $total) { break }
            }
            $ns.Write($parts.Tail, 0, $parts.Tail.Length)
            $ns.Flush()

            $sec = $sw.Elapsed.TotalSeconds
            $kbs = if ($sec -gt 0) { ($sent / 1KB) / $sec } else { 0 }
            Write-Host ("  da gui {0:N0}/{1:N0} KB trong {2:N1} s ({3:N1} KB/s) - cho tra loi..." -f ($sent / 1KB), ($total / 1KB), $sec, $kbs)
            $res.Sent = $sent
            $res.Sec = $sec
            $res.KBps = $kbs

            $ms = New-Object System.IO.MemoryStream
            $rbuf = New-Object byte[] 4096
            try {
                while ($true) {
                    $r = $ns.Read($rbuf, 0, $rbuf.Length)
                    if ($r -le 0) { break }
                    $ms.Write($rbuf, 0, $r)
                }
            } catch {
                # het ReadTimeout / server dong dot ngot: van dung phan da doc duoc
            }
            $rawResp = [System.Text.Encoding]::UTF8.GetString($ms.ToArray())
            $ms.Dispose()

            $status = 0
            $body = $rawResp
            $idx = $rawResp.IndexOf("`r`n`r`n")
            if ($idx -ge 0) {
                $headTxt = $rawResp.Substring(0, $idx)
                $body = $rawResp.Substring($idx + 4)
                if ($headTxt -match 'HTTP/\d\.\d\s+(\d{3})') { $status = [int]$Matches[1] }
            }
            $res.Status = $status
            $res.Body = $body

            if ($status -eq 0) {
                $res.Error = 'khong nhan duoc phan hoi HTTP (dut ket noi giua chung?)'
                throw $res.Error
            }
            $res.Ok = Test-OkBody -Body $body
            if ($status -ne 200) {
                $res.Error = "server tra HTTP $status"
                return $res                 # loi phia server: KHONG thu lai
            }
            return $res
        } catch {
            $res.Error = Get-ShortError $_
        } finally {
            if ($fs) { try { $fs.Close() } catch { } }
            if ($ns) { try { $ns.Close() } catch { } }
            if ($tcp) { try { $tcp.Close() } catch { } }
            $sw.Stop()
        }
    }
    return $res
}

function Test-OkBody {
    # Phan hoi duoc coi la THANH CONG khi: JSON "status":"Ok"/"Success"/"Queued" hoac chuoi OK.
    param([string]$Body)
    if (-not $Body) { return $false }
    if ($Body -match '"status"\s*:\s*"(ok|success|queued|accepted|done)"') { return $true }
    if ($Body -match '(?i)^\s*(ok|done)\b') { return $true }
    if ($Body -match '"panel"\s*:') { return $true }   # mot so firmware tra ve chinh JSON trang thai
    return $false
}

# ---------------------------------------------------------------------------
# Theo doi GET /status
# ---------------------------------------------------------------------------
function Test-SameFileName {
    param([string]$A, [string]$B)
    if (-not $A -or -not $B) { return $false }
    $a = Split-Path -Leaf $A.Trim()
    $b = Split-Path -Leaf $B.Trim()
    return ($a -eq $b)      # -eq cua PowerShell khong phan biet hoa/thuong
}

function Watch-PanelStatus {
    # Theo doi GET /status cho toi khi panel bao ket thuc (done/error) hoac het tran.
    # Trang thai "tinh" (sticky): panel co the con giu ket qua cua PHIEN TRUOC. Vi vay neu
    # TRUOC KHI GUI panel da o trang thai ket thuc (PreWasTerminal) thi mot trang thai ket thuc
    # GIONG HET snapshot do se bi coi la CU va khong tin - cho toi khi thay phien cua minh
    # (file trung ten, hoac mot state dang chay). Qua StaleGiveUpSec ma van mo ho => ket luan
    # "khong xac nhan duoc" thay vi treo cho het StatusTimeoutSec.
    param(
        [string]$Ip, [int]$Port = 80, [int]$IntervalMs = 1000, [int]$TimeoutSec = 900,
        [string]$SendName = '', [int]$IdleGraceSec = 15, [int]$StaleGiveUpSec = 30,
        [bool]$PreWasTerminal = $false, [string]$PreState = '', [string]$PreFile = '',
        [int64]$ExpectedBytes = 0
    )
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $lastLine = ''
    $sawBusy = $false
    $staleSince = -1.0
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $g = Invoke-HttpGet -Ip $Ip -Port $Port -Path '/status' -TimeoutMs 4000
        if ($g.Ok -and $g.Status -eq 200) {
            $s = ConvertFrom-PanelStatus $g.Body
            $line = Format-StatusLine $s -ExpectedBytes $ExpectedBytes
            if (-not $line) { $line = ('(khong doc duoc JSON) ' + (Get-FirstLine $g.Body)) }
            $shown = ('[{0,5:N0} s] {1}' -f $sw.Elapsed.TotalSeconds, $line)
            if ($shown -ne $lastLine) {
                Write-Host ("`r  " + $shown.PadRight(110)) -NoNewline -ForegroundColor DarkCyan
                $lastLine = $shown
            }

            $stateTxt = ''
            if ($s -and $null -ne $s.State) { $stateTxt = "$($s.State)" }
            $fileTxt = ''
            if ($s -and $null -ne $s.File) { $fileTxt = "$($s.File)" }
            $isTerminal = ($stateTxt -match '(?i)^(done|ok|success|complete|completed|finished|error|failed|fail|abort|cancel|reject)')

            if ($stateTxt -match '(?i)(receiving|posting|busy|uploading|sending|running|saving)') { $sawBusy = $true }
            $sawOurs = $sawBusy -or (Test-SameFileName $fileTxt $SendName)

            # Trang thai ket thuc GIONG HET snapshot truoc khi gui, va chua thay phien cua minh => CU.
            $stale = $false
            if ($isTerminal -and -not $sawOurs -and $PreWasTerminal) {
                if ($stateTxt -eq $PreState -and $fileTxt -eq $PreFile) { $stale = $true }
            }

            if ($isTerminal -and -not $stale) {
                Write-Host ""
                if ($stateTxt -match '(?i)(error|fail|abort|cancel|reject)') {
                    return [pscustomobject]@{ Final = $true; Ok = $false; State = $stateTxt; Info = $s; Sec = $sw.Elapsed.TotalSeconds }
                }
                return [pscustomobject]@{ Final = $true; Ok = $true; State = $stateTxt; Info = $s; Sec = $sw.Elapsed.TotalSeconds }
            }
            if ($stale) {
                if ($staleSince -lt 0) {
                    $staleSince = $sw.Elapsed.TotalSeconds
                    Write-Warn ("`n  /status dang la trang thai CU (state={0}, file={1}) - bo qua, cho phien cua minh..." -f $stateTxt, $fileTxt)
                } elseif (($sw.Elapsed.TotalSeconds - $staleSince) -gt $StaleGiveUpSec) {
                    Write-Host ""
                    Write-Warn ("  /status van la trang thai CU sau {0} s - khong xac nhan duoc ket qua." -f $StaleGiveUpSec)
                    return [pscustomobject]@{ Final = $false; Ok = $true; State = 'stale'; Info = $s; Sec = 0 }
                }
            }

            if (-not $sawOurs -and -not $isTerminal -and -not $stale -and $sw.Elapsed.TotalSeconds -gt $IdleGraceSec) {
                # Khong he thay phien nao chay trong IdleGraceSec => panel khong bao tien do.
                Write-Host ""
                return [pscustomobject]@{ Final = $false; Ok = $true; State = $stateTxt; Info = $s; Sec = 0 }
            }
        } else {
            $errTxt = if ($g.Ok) { "HTTP $($g.Status)" } else { $g.Error }
            Write-Host ("`r  /status: {0} (thu lai...)" -f $errTxt).PadRight(110) -NoNewline -ForegroundColor DarkGray
        }
        Start-Sleep -Milliseconds $IntervalMs
    }
    Write-Host ""
    return [pscustomobject]@{ Final = $false; Ok = $false; State = 'timeout'; Info = $null; Sec = $sw.Elapsed.TotalSeconds }
}

# ---------------------------------------------------------------------------
# M22 + M21 cho Marlin
# ---------------------------------------------------------------------------
function Initialize-MarlinSd {
    param([string]$PortName = 'COM4', [int]$Baud = 250000)
    $portNames = @()
    try { $portNames = @([System.IO.Ports.SerialPort]::GetPortNames()) } catch { }
    if (-not ($portNames -contains $PortName)) {
        Write-Err ("  KHONG thay {0}. Cac cong dang co: {1}" -f $PortName, (($portNames | Sort-Object) -join ', '))
        return $false
    }
    $sp = New-Object System.IO.Ports.SerialPort
    $sp.PortName = $PortName
    $sp.BaudRate = $Baud
    $sp.Parity = [System.IO.Ports.Parity]::None
    $sp.DataBits = 8
    $sp.StopBits = [System.IO.Ports.StopBits]::One
    $sp.NewLine = "`n"
    $sp.ReadTimeout = 200
    $sp.WriteTimeout = 2000
    $sp.DtrEnable = $true      # BAT BUOC voi USB CDC cua STM32 (xem send-gcode.ps1)
    $sp.RtsEnable = $true
    try {
        $sp.Open()
    } catch {
        Write-Err ("  Khong mo duoc {0}: {1}" -f $PortName, $_.Exception.Message)
        Write-Warn "  Goi y: dong Serial Monitor / script khac dang giu cong."
        return $false
    }
    try {
        Write-Host ("  da mo {0} @{1} 8N1 (DTR=1)" -f $PortName, $Baud) -ForegroundColor DarkCyan
        # Mo cong USB CDC lam board RESET -> doi boot xong (giong send-gcode.ps1).
        Start-Sleep -Milliseconds 2500
        try { $sp.DiscardInBuffer() } catch { }
        $okAll = $true
        foreach ($cmd in @('M22', 'M21')) {
            Write-Host ("  >>> {0}" -f $cmd) -ForegroundColor DarkCyan
            $sp.WriteLine($cmd)
            $last = Get-Date
            $all = ''
            while (((Get-Date) - $last).TotalMilliseconds -lt 1500) {
                $s = ''
                try { $s = $sp.ReadExisting() } catch { }
                if ($s.Length -gt 0) { $all += $s; $last = Get-Date } else { Start-Sleep -Milliseconds 60 }
            }
            $lines = @($all -split "`r?`n" | Where-Object { $_.Trim().Length -gt 0 })
            if ($lines.Count -eq 0) {
                Write-Warn "      (khong thay phan hoi - Marlin co the dang ban in hoac cong khong dung)"
                $okAll = $false
            } else {
                foreach ($ln in $lines) { Write-Dim ("      [marlin] " + $ln.TrimEnd()) }
            }
        }
        Start-Sleep -Milliseconds 1000    # ~1 s cho Marlin nhan the
        return $okAll
    } finally {
        try { $sp.Close() } catch { }
        try { $sp.Dispose() } catch { }
    }
}

# ===========================================================================
# 0) Kiem tra file / tham so
# ===========================================================================
$swAll = [System.Diagnostics.Stopwatch]::StartNew()

if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
    Write-Err "KHONG thay file: $File"
    exit 4
}
$fi = Get-Item -LiteralPath $File
if ($fi.Length -le 0) {
    Write-Err "File rong (0 byte): $($fi.FullName)"
    exit 4
}
if ($Port -lt 1 -or $Port -gt 65535) { Write-Err "Port khong hop le: $Port"; exit 4 }
if ($TimeoutSec -lt 5) { $TimeoutSec = 5 }
if ($ScanThreads -lt 1) { $ScanThreads = 1 }
if ($ScanThreads -gt 60) { $ScanThreads = 60 }
if ($StatusIntervalMs -lt 200) { $StatusIntervalMs = 200 }

$sendName = if ($Name) { $Name } else { $fi.Name }
$sendName = Split-Path -Leaf $sendName
if (-not $sendName -or $sendName -match '[\r\n"]') {
    Write-Err "Ten file tren the SD khong hop le (rong hoac chua dau `" / xuong dong): '$sendName'"
    exit 4
}
if ($sendName -notmatch '^[\x20-\x7E]+$') { Write-Warn "CANH BAO: ten file co ky tu khong phai ASCII." }
if ($sendName -notmatch '^[A-Za-z0-9._-]+$') { Write-Warn "CANH BAO: ten file co ky tu ngoai [A-Za-z0-9._-] (duong BT cua panel gioi han 31 ky tu)." }
if ($sendName -notmatch '\.(gcode|gco|g|nc|bin)$') { Write-Warn "CANH BAO: duoi file khong phai .gcode/.gco/.g/.nc/.bin - Marlin co the khong liet ke." }

$markers = if ($PanelMarker) { @($PanelMarker) } else { $PANEL_MARKERS_DEFAULT }
$isPanel = ($Target -eq 'panel')
$size = [int64]$fi.Length

Write-Head "=== Tai file len may in qua WiFi $(if ($isPanel) { '(PANEL -> the SD dung chung voi Marlin)' } else { '(THANG len ESP3D -> the SD Marlin)' }) ==="
Write-Host ("File       : {0}" -f $fi.FullName)
Write-Host ("Kich thuoc : {0:N0} byte = {1:N2} MB" -f $fi.Length, ($fi.Length / 1MB))
Write-Host ("Ten tren SD: {0}" -f $sendName)
if ($isPanel) {
    Write-Host ("Dich       : PANEL :{0}/  (POST /upload?token=..., theo doi GET /status)" -f $Port)
} else {
    Write-Host ("Dich       : ESP3D :{0}/files  (POST multipart)" -f $Port)
}

# --- Boi canh mang ---------------------------------------------------------
$nets = @(Get-ActiveIpv4)
Write-Host ""
Write-Head "--- Mang cua PC ---"
if ($nets.Count -eq 0) {
    Write-Warn "  Khong thay card mang nao dang UP co IPv4."
} else {
    foreach ($n in $nets) {
        $gwTxt = if ($n.Gateway) { $n.Gateway } else { '(khong co gateway)' }
        Write-Host ("  {0,-14} {1}/{2}   gateway {3}" -f $n.Adapter, $n.IP, $n.Prefix, $gwTxt)
    }
}
$ssid = Get-WlanSsid
if ($ssid) {
    Write-Host ("  WiFi SSID  : {0}" -f $ssid)
    if ($isPanel) {
        if ($ssid -eq 'Voron2308') {
            Write-Note "  Panel se vao WiFi STA cua mang nha (TP-Link_31A0). Dang o AP cua ESP3D - kiem tra lai neu khong thay panel."
        } else {
            Write-Ok "  Dang o mang WiFi nha - dung dieu kien de thay PANEL (panel cung vao mang nay)."
        }
    } else {
        if ($ssid -eq 'Voron2308') {
            Write-Ok "  Dang noi AP cua module ESP3D: dung dieu kien cho -Target esp3d."
        } else {
            Write-Note "  CANH BAO: -Target esp3d chi chay khi PC noi AP 'Voron2308' cua module (hoac ESP3D o AP+STA)."
            Write-Note "  Dang o '$ssid' => 192.168.0.1 la ROUTER, khong phai ESP3D."
        }
    }
} else {
    Write-Dim "  (khong doc duoc SSID WiFi - bo qua)"
}
Write-Host ""

# ===========================================================================
# 1) TU TIM THIET BI
# ===========================================================================
$foundIp = $null
$foundFrom = $null
$foundStatus = $null
$esp3dSeen = @()
$tried = New-Object System.Collections.ArrayList
$swDisc = [System.Diagnostics.Stopwatch]::StartNew()

function Try-Candidate {
    param([string]$Ip, [string]$Source = '', [int]$Port = 80, [int]$TimeoutMs = 1500, [switch]$Quiet)
    if (-not $Ip) { return $false }
    if ($script:tried -contains $Ip) {
        if (-not $Quiet) { Write-Dim ("  [bo qua] {0} (da thu va bi loai o buoc truoc)" -f $Ip) }
        return $false
    }
    [void]$script:tried.Add($Ip)

    if ($script:isPanel) {
        $t = Test-PanelAt -Ip $Ip -Port $Port -TimeoutMs $TimeoutMs -Markers $script:markers
        if ($t.Panel) {
            $script:foundIp = $Ip
            $script:foundFrom = $Source
            $script:foundStatus = $t.Status
            $how = if ($t.How) { $t.How } else { '/status' }
            Write-Ok ("  {0} -> DUNG LA PANEL (nhan dang qua {1}, {2} ms)" -f $Ip, $how, $t.Ms)
            if ($t.Info) { Write-Dim ("      {0}" -f $t.Info) }
            return $true
        }
        if ($t.Esp3d) {
            if (-not ($script:esp3dSeen -contains $Ip)) { $script:esp3dSeen += $Ip }
            Write-Warn ("  {0} -> day la ESP3D, KHONG phai panel ({1})" -f $Ip, $t.Info)
            Write-Note  "      => muon gui thang: them  -Target esp3d"
            return $false
        }
        if (-not $Quiet) { Write-Dim ("  {0} -> khong phai panel ({1})" -f $Ip, $t.Info) }
        return $false
    } else {
        $e = Test-Esp3dAt -Ip $Ip -Port $Port -TimeoutMs $TimeoutMs
        if ($e.Esp) {
            $script:foundIp = $Ip
            $script:foundFrom = $Source
            Write-Ok ("  {0} -> DUNG LA ESP3D ({1} ms): {2}" -f $Ip, $e.Ms, $e.Info)
            return $true
        }
        if ($e.Status -gt 0) {
            Write-Warn ("  {0} -> co port {1} nhung KHONG phai ESP3D (HTTP {2}, khong co 'FW version:')" -f $Ip, $Port, $e.Status)
        } elseif (-not $Quiet) {
            Write-Dim ("  {0} -> khong tra loi HTTP ({1})" -f $Ip, $e.Error)
        }
        return $false
    }
}

if ($TargetHost) {
    Write-Step ("[1/6] -Host {0}" -f $TargetHost)
    if (Try-Candidate -Ip $TargetHost -Source 'host' -Port $Port -TimeoutMs $ProbeTimeoutMs) { }
    elseif ($NoDiscover) {
        Write-Err "IP ep (-Host $TargetHost) khong dung chu ky thiet bi dich va dang co -NoDiscover => dung."
        exit 3
    } else {
        Write-Warn "  Tiep tuc tu tim..."
    }
} elseif ($NoDiscover) {
    Write-Step "[1/6] -NoDiscover: chi dung -Host hoac IP da luu trong cache"
    $cachedIp = Get-CachedIp $Target
    if ($cachedIp) {
        Write-Host ("  cache: {0} = {1}" -f $Target.ToLower(), $cachedIp)
        if (Try-Candidate -Ip $cachedIp -Source 'cache' -Port $Port -TimeoutMs $ProbeTimeoutMs) { }
    } else {
        Write-Dim ("  (chua co cache {0} cho dich '{1}')" -f $CACHE_FILE, $Target.ToLower())
    }
}

if (-not $foundIp -and -not $NoDiscover -and -not $NoCache) {
    # ---- 2) CACHE: IP cua lan tim thanh cong gan nhat (nhanh nhat - 1 lan HTTP ngan) ----
    Write-Step ("[2/6] cache IP lan truoc ({0})" -f $CACHE_FILE)
    $cachedIp = Get-CachedIp $Target
    if ($cachedIp) {
        Write-Host ("  thu IP da luu: {0}  (bo qua bang -NoCache)" -f $cachedIp)
        # Timeout ngan: IP cu/chet chi lam mat ~1,5 s roi tu tim lai
        if (Try-Candidate -Ip $cachedIp -Source 'cache' -Port $Port -TimeoutMs ([Math]::Min($ProbeTimeoutMs, 1500))) {
            Write-Ok ("  DUNG IP DA LUU -> khoi phai do lai (nhanh ~{0:N1} s)" -f $swDisc.Elapsed.TotalSeconds)
        } else {
            Write-Dim "  IP da luu khong dung nua (doi IP / thiet bi tat) -> tim lai tu dau."
        }
    } else {
        Write-Dim "  chua co cache (lan dau) - se tim roi luu lai."
    }
}

if (-not $foundIp -and -not $NoDiscover) {
    # ---- 3) mDNS ----
    $mdnsNames = if ($isPanel) {
        @('TBTPANEL.local', 'tbtpanel.local', 'esp3d.local', 'MYESP.local')
    } else {
        @('MYESP.local', 'esp3d.local', 'TBTPANEL.local')
    }
    Write-Step ("[3/6] mDNS ({0})" -f ($mdnsNames -join ', '))
    $mdnsIps = New-Object System.Collections.ArrayList
    foreach ($n in $mdnsNames) {
        try {
            $r = Resolve-DnsName -Name $n -Type A -QuickTimeout -ErrorAction Stop
            $ips = @($r | Where-Object { $_.IPAddress } | ForEach-Object { $_.IPAddress })
            if ($ips.Count -gt 0) {
                Write-Host ("  {0} -> {1}" -f $n, ($ips -join ', '))
                foreach ($i in $ips) { [void]$mdnsIps.Add($i) }
            } else {
                Write-Dim ("  {0} -> khong co ban ghi A" -f $n)
            }
        } catch {
            Write-Dim ("  {0} -> khong phan giai duoc" -f $n)
        }
    }
    foreach ($ip in $mdnsIps) {
        if ($foundIp) { break }
        if (Try-Candidate -Ip $ip -Source 'mdns' -Port $Port -TimeoutMs $ProbeTimeoutMs) { break }
    }
    if (-not $foundIp) { Write-Dim "  mDNS khong cho ket qua dung." }
}

if (-not $foundIp -and -not $NoDiscover) {
    # ---- 3) gateway ----
    Write-Step "[4/6] gateway cua card mang dang dung"
    $gws = @($nets | Where-Object { $_.Gateway } | ForEach-Object { $_.Gateway } | Sort-Object -Unique)
    if ($gws.Count -eq 0) {
        Write-Dim "  khong co gateway nao."
    } else {
        Write-Host ("  thu: {0}" -f ($gws -join ', '))
        foreach ($gw in $gws) {
            if ($foundIp) { break }
            if (Try-Candidate -Ip $gw -Source 'gateway' -Port $Port -TimeoutMs $ProbeTimeoutMs) { break }
        }
        if (-not $foundIp) { Write-Dim "  gateway khong dung chu ky thiet bi dich." }
    }
}

if (-not $foundIp -and -not $NoDiscover) {
    # ---- 4) quet LAN /24 ----
    $subnets = @()
    foreach ($n in $nets) {
        $p = $n.IP.Split('.')
        if ($p.Count -eq 4) { $subnets += ('{0}.{1}.{2}' -f $p[0], $p[1], $p[2]) }
    }
    $subnets = @($subnets | Sort-Object -Unique)

    Write-Step ("[5/6] quet LAN {0} (port {1})" -f (($subnets | ForEach-Object { "$_/24" }) -join ' + '), $Port)
    if ($subnets.Count -eq 0) {
        Write-Dim "  khong xac dinh duoc dai /24 nao."
    } else {
        $selfIps = @($nets | ForEach-Object { $_.IP })
        $scan = Invoke-LanScan -Subnets $subnets -SelfIps $selfIps -Port $Port -Threads $ScanThreads -TimeoutMs $ScanTimeoutMs -BudgetSec $ScanBudgetSec
        $scanTxt = if ($scan.Aborted) { ' (HET TRAN - ket qua co the thieu)' } else { '' }
        Write-Host ("  quet xong sau {0:N1} s - {1} IP mo port {2}{3}." -f $scan.Sec, $scan.Count, $Port, $scanTxt)

        $cands = @($scan.Open)
        if ($cands.Count -gt 12) {
            Write-Dim ("  (co {0} IP mo port {1}; chi xac nhan 12 IP dau de kip thoi gian)" -f $cands.Count, $Port)
            $cands = @($cands[0..11])
        }
        if ($cands.Count -gt 0) {
            Write-Host ("  xac nhan chu ky: {0}" -f ($cands -join ', '))
            foreach ($ip in $cands) {
                if ($foundIp) { break }
                if (Try-Candidate -Ip $ip -Source 'quet' -Port $Port -TimeoutMs $ProbeTimeoutMs) { break }
            }
        }
        if (-not $foundIp) { Write-Dim "  quet LAN khong tim thay thiet bi dich." }
    }
}

if (-not $foundIp -and -not $NoDiscover) {
    # ---- 5) danh sach co dinh ----
    Write-Step ("[6/6] danh sach co dinh ({0})" -f ($FIXED_LIST -join ', '))
    foreach ($ip in $FIXED_LIST) {
        if ($foundIp) { break }
        if (Try-Candidate -Ip $ip -Source 'danh sach' -Port $Port -TimeoutMs $ProbeTimeoutMs) { break }
    }
    if (-not $foundIp) { Write-Dim "  khong ung vien nao dung." }
}

$swDisc.Stop()

if (-not $foundIp) {
    Write-Host ""
    Write-Err ("KHONG TIM THAY {0}." -f $(if ($isPanel) { 'PANEL' } else { 'ESP3D' }))
    Write-Host ("  Da thu {0} dia chi trong {1:N1} s." -f $tried.Count, $swDisc.Elapsed.TotalSeconds)
    if ($tried.Count -gt 0) { Write-Dim ("  {0}" -f (($tried | Select-Object -First 24) -join ', ')) }
    Write-Note "  Kiem tra:"
    if ($isPanel) {
        Write-Note "   1) Panel da noi WiFi nha (TP-Link_31A0) chua? Xem trang STATUS tren panel / log COM7."
        Write-Note "   2) Firmware panel da co HTTP server chua? (nhanh dang lam) - thu: Invoke-WebRequest http://<IP>/status"
        Write-Note "   3) Ep IP:  .\panel-upload.ps1 -File <file> -Host <IP>"
        if ($esp3dSeen.Count -gt 0) {
            Write-Note ("   4) Da thay ESP3D tai: {0} - neu ban muon gui thang thi dung  -Target esp3d" -f ($esp3dSeen -join ', '))
        }
    } else {
        Write-Note "   1) Module co dang bat khong? AP 'Voron2308' co hien trong danh sach WiFi?"
        Write-Note "   2) -Target esp3d chi chay khi PC noi AP cua module (hoac ESP3D o AP+STA)."
        Write-Note "   3) Ep IP:  .\panel-upload.ps1 -File <file> -Target esp3d -Host <IP>"
    }
    exit 3
}

Write-Host ""
Write-Ok ("[{0}] thay {1} (nguon: {2})  [do mat {3:N1} s]" -f $(if ($isPanel) { 'PANEL' } else { 'ESP3D' }), $foundIp, $foundFrom, $swDisc.Elapsed.TotalSeconds)

# Luu IP cho tung dich (panel / esp3d) de lan sau vao thang - xem Get-CachedIp/Set-CachedIp.
# Nguon 'cache' thi khong ghi lai (giong y nguyen gia tri dang co).
if ($foundFrom -ne 'cache') { Set-CachedIp $Target $foundIp }

$sameSubnet = $false
$localIps = @($nets | ForEach-Object { $_.IP })
if ($foundIp -match '^127\.' -or ($localIps -contains $foundIp)) { $sameSubnet = $true }   # loopback / chinh may
foreach ($n in $nets) {
    $p = $n.IP.Split('.')
    if ($p.Count -eq 4 -and $foundIp.StartsWith(('{0}.{1}.{2}.' -f $p[0], $p[1], $p[2]))) { $sameSubnet = $true }
}
if (-not $sameSubnet -and $nets.Count -gt 0) {
    Write-Warn "  CANH BAO: $foundIp KHONG cung dai /24 voi card mang nao cua PC => co the khong ket noi duoc."
}

# ===========================================================================
# 2) GUI FILE
# ===========================================================================
if ($isPanel) {
    # --- Tien phong: doc /status (xem panel co san sang khong + chup lai snapshot de phat hien
    #     "trang thai tinh" cua phien truoc sau khi gui) ---
    $preWasTerminal = $false
    $preState = ''
    $preFile = ''
    if (-not $NoStatus) {
        Write-Host ""
        Write-Head "--- Trang thai panel (GET /status) ---"
        $g0 = Invoke-HttpGet -Ip $foundIp -Port $Port -Path '/status' -TimeoutMs 3000
        if ($g0.Ok -and $g0.Status -eq 200) {
            $s0 = ConvertFrom-PanelStatus $g0.Body
            $line0 = Format-StatusLine $s0 -ExpectedBytes $size
            if ($line0) { Write-Host ("  " + $line0) } else { Write-Host ("  " + (Get-FirstLine $g0.Body)) }
            $tot0 = 0
            try { if ($s0 -and $null -ne $s0.Total) { $tot0 = [int64]$s0.Total } } catch { $tot0 = 0 }
            if ($tot0 -le 0) {
                Write-Dim "  (* panel khong bao total_bytes: ESP32 core 2.0.17 dat upload.totalSize=0 o"
                Write-Dim "     UPLOAD_FILE_START nen firmware doc o do luon nhan 0 - client lay kich thuoc"
                Write-Dim "     file cua chinh no lam moc tinh %.)"
            }
            $st0 = ''
            if ($s0 -and $null -ne $s0.State) { $st0 = "$($s0.State)" }
            if ($s0 -and $null -ne $s0.File) { $preFile = "$($s0.File)" }
            $preState = $st0
            if ($st0 -match '(?i)^(done|ok|success|complete|completed|finished|error|failed|fail|abort|cancel|reject)') {
                $preWasTerminal = $true
                Write-Dim "  (panel dang giu ket qua cua phien TRUOC: state=$st0, file=$preFile)"
            }
            if ($st0 -match '(?i)(receiving|posting|busy|uploading)') {
                Write-Warn "  Panel dang ban (state=$st0). Nen doi cho xong roi chay lai."
            }
        } else {
            $err0 = if ($g0.Ok) { "HTTP $($g0.Status)" } else { $g0.Error }
            Write-Warn ("  /status -> {0} (van thu gui file)" -f $err0)
        }
    }

    $tokenQ = $Token
    try { $tokenQ = [System.Uri]::EscapeDataString($Token) } catch { }
    $nameQ = $sendName
    try { $nameQ = [System.Uri]::EscapeDataString($sendName) } catch { }
    $path = '/upload?token=' + $tokenQ
    if ($nameQ) { $path += '&name=' + $nameQ }

    Write-Host ""
    Write-Head "--- Gui len PANEL ---"
    $up = Send-FileRaw -Ip $foundIp -Port $Port -PathAndQuery $path -LocalPath $fi.FullName `
                       -SendName $sendName -FieldName $PANEL_FIELD_NAME -Token $Token -TimeoutSec $TimeoutSec
} else {
    Write-Host ""
    Write-Head "--- Gui thang len ESP3D ---"
    $up = Send-FileRaw -Ip $foundIp -Port $Port -PathAndQuery '/files' -LocalPath $fi.FullName `
                       -SendName $sendName -FieldName $ESP3D_FIELD_NAME -TimeoutSec $TimeoutSec
}

if ($up.Status -ne 200) {
    Write-Err ("LOI GUI FILE: {0}" -f $up.Error)
    if ($up.Status -gt 0) { Write-Dim ("  HTTP {0}" -f $up.Status) }
    if ($up.Body) { Write-Dim ("  tra loi: {0}" -f ((Get-FirstLine $up.Body))) }
    if ($up.Status -eq 401) {
        Write-Note "  => Panel TU CHOI vi SAI TOKEN. Kiem tra -Token (mac dinh TBT-3D-up) khop"
        Write-Note "     PANEL_HTTP_TOKEN trong lcd_panel/src/panel_config.h."
    } elseif ($up.Status -eq 409) {
        Write-Note "  => Marlin DANG IN nen panel tu choi nhan file (PANEL_HTTP_ALLOW_WHILE_PRINTING=0)."
        Write-Note "     Doi in xong roi chay lai."
    } elseif ($up.Status -eq 403) {
        Write-Note "  => 403: co the ban dang gui vao ROUTER (khong phai panel/ESP3D). Kiem tra lai mang."
    }
    exit 2
}
if ($up.Body) { Write-Dim ("  tra loi: {0}" -f (Get-FirstLine $up.Body)) }
if ($up.Attempt -gt 1) { Write-Warn "  (phai thu lai lan 2 moi duoc)" }

# ===========================================================================
# 3) KET QUA / THEO DOI TIEN DO
# ===========================================================================
$confirmed = Test-OkBody -Body $up.Body
$statusFinal = $null

if ($isPanel) {
    if (-not $NoStatus) {
        Write-Host ""
        Write-Head "--- Theo doi GET /status (panel dang ghi vao the SD) ---"
        $w = Watch-PanelStatus -Ip $foundIp -Port $Port -IntervalMs $StatusIntervalMs -TimeoutSec $StatusTimeoutSec `
                               -SendName $sendName -PreWasTerminal $preWasTerminal -PreState $preState -PreFile $preFile `
                               -ExpectedBytes $size
        $statusFinal = $w
        if ($w.State -eq 'timeout') {
            Write-Warn ("  Het {0} s ma /status chua bao xong. Kiem tra panel (log COM7 / trang STATUS)." -f $StatusTimeoutSec)
        } elseif (-not $w.Final) {
            Write-Dim ("  /status khong co phien dang chay (state={0}) - dua vao phan hoi POST." -f $w.State)
        } else {
            $lineF = Format-StatusLine $w.Info -ExpectedBytes $size
            if ($w.Ok) { Write-Ok ("  PANEL BAO XONG ({0} s): {1}" -f [int]$w.Sec, $lineF) }
            else { Write-Err ("  PANEL BAO LOI: {0}" -f $lineF) }
        }
    } else {
        Write-Dim "  (-NoStatus: khong theo doi /status)"
    }
} else {
    if (-not $confirmed) {
        Write-Warn "  Phan hoi HTTP 200 nhung KHONG thay \"status\":\"Ok\" (co the la 'Upload failed')."
    }
}

# ===========================================================================
# 4) M22 + M21 (tuy chon)
# ===========================================================================
$marlinOk = $null
Write-Host ""
if ($InitSd) {
    Write-Head ("--- Gui M22 + M21 cho Marlin ({0}) ---" -f $MarlinPort)
    if ($isPanel) {
        Write-Dim "  (duong panel: panel DA tu gui M22/M21 sau khi day xong; day la buoc gui them cho chac)"
    }
    $marlinOk = Initialize-MarlinSd -PortName $MarlinPort
    if ($marlinOk) {
        Write-Ok "  Marlin da nhan M22 + M21 => the SD duoc khoi tao lai."
    } else {
        Write-Warn "  Chua chac Marlin da nhan M22/M21. Mo man FILES tren panel (panel tu gui M22/M21)."
    }
} else {
    if ($isPanel) {
        Write-Note "Khong can -InitSd voi duong PANEL: panel TU GUI M22/M21 cho Marlin sau khi day file xong."
        Write-Note "Mo man FILES / REFRESH tren panel de thay file va chon in."
    } else {
        Write-Note "Chua gui M22/M21 (khong co -InitSd) - ESP3D KHONG tu gui, nen file chua in duoc ngay."
        Write-Note "=> HAY MO MAN FILES TREN PANEL (panel tu gui M22/M21) hoac chay lai voi -InitSd -MarlinPort COM4."
    }
}

# ===========================================================================
# 5) KET LUAN + MA THOAT
# ===========================================================================
$swAll.Stop()
Write-Host ""
Write-Head "=== KET QUA ==="
Write-Host ("  File tren the SD : {0}" -f $sendName)
Write-Host ("  Da gui           : {0:N0} byte ({1:N2} MB) trong {2:N1} s ({3:N1} KB/s)" -f $up.Sent, ($up.Sent / 1MB), $up.Sec, $up.KBps)
Write-Host ("  Thiet bi         : {0} {1}:{2} (nguon: {3})" -f $(if ($isPanel) { 'PANEL' } else { 'ESP3D' }), $foundIp, $Port, $foundFrom)
if ($isPanel) {
    $pushTxt = 'chua xac nhan (khong co /status)'
    if ($statusFinal) {
        if ($statusFinal.State -eq 'timeout') { $pushTxt = 'chua xac nhan (het thoi gian theo doi)' }
        elseif ($statusFinal.State -eq 'stale') { $pushTxt = 'chua xac nhan (/status giu ket qua phien truoc)' }
        elseif ($statusFinal.Final -and $statusFinal.Ok) { $pushTxt = 'panel da GHI VAO THE SD: OK' }
        elseif ($statusFinal.Final -and -not $statusFinal.Ok) { $pushTxt = "panel bao loi: $($statusFinal.State)" }
        elseif ($statusFinal.Ok) { $pushTxt = 'panel bao Ok (khong co phien dang chay)' }
    }
    Write-Host ("  Panel -> the SD : {0}" -f $pushTxt)
}
Write-Host ("  Tong thoi gian   : {0:N1} s" -f $swAll.Elapsed.TotalSeconds)

$panelError = ($isPanel -and $statusFinal -and $statusFinal.Final -and -not $statusFinal.Ok)
$unconfirmed = ($isPanel -and -not $NoStatus -and $statusFinal -and ($statusFinal.State -eq 'timeout' -or $statusFinal.State -eq 'stale'))
if ($panelError) {
    Write-Err "  => PANEL BAO LOI khi ghi vao the SD - xem chi tiet o phan theo doi /status."
    exit 2
}
if (-not $isPanel -and -not $confirmed) {
    Write-Err "  => Server tra HTTP 200 nhung KHONG xac nhan \"status\":\"Ok\" - coi nhu THAT BAI."
    exit 2
}
if ($unconfirmed) {
    Write-Warn "  => Da gui xong cho panel nhung CHUA XAC NHAN duoc ket qua ghi vao the SD."
    Write-Warn "     Kiem tra trang STATUS tren panel (hoac log COM7: tools\panel-log.ps1) truoc khi in."
    exit 0
}

Write-Ok   "  => File DA DUOC GUI THANH CONG."
Write-Note "  => Tren panel: mo man FILES (hoac REFRESH) de thay file va chon in."
exit 0
