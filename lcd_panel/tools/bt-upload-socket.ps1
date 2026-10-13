#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Gui file G-code tu WINDOWS sang panel CYD qua Bluetooth SPP bang SOCKET RFCOMM TRUC TIEP
    (Winsock AF_BTH + BTHPROTO_RFCOMM) - KHONG can cong COM, KHONG can quyen admin.

.DESCRIPTION
    Panel CYD (firmware lcd_panel) nhan file qua Bluetooth SPP, ghi file tam tren the TF cua chinh
    panel, roi TAT BT + bat WiFi de POST multipart/form-data len module ESP3D (module ghi thang vao
    the SD cua Marlin), cuoi cung gui M22+M21 cho Marlin va bao ket qua qua BT.

    Script nay la ban SOCKET cua tools/bt-upload.ps1. Ly do ton tai: rat nhieu may Windows KHONG
    tao cong COM SPP cho thiet bi (Control Panel bao "The device you selected does not have a serial
    port service running") => System.IO.Ports.SerialPort khong dung duoc. O day ta tu mo ket noi
    RFCOMM qua ws2_32.dll:
        WSAStartup(0x202) -> socket(AF_BTH=32, SOCK_STREAM=1, BTHPROTO_RFCOMM=3)
        -> connect(SOCKADDR_BTH{ addressFamily=32, btAddr=<MAC dao byte>, serviceClassId=SPP,
                                  port=0 hoac port=<kenh do SDP> })
        -> send()/recv()  (byte[] thuan, khong qua SerialPort)
    => KHONG can cong COM, KHONG can admin. Neu ban VAN muon dung cong COM thi script cu
       (tools/bt-upload.ps1) van con nguyen.

    CU PHAP HEADER (panel nhan 3 dang, TAT CA deu phai co token):
        PUT <token> <ten_file> <so_byte>   <- mac dinh (script tu tinh so byte)
        PUT <token> <ten_file> *           <- dung khi co -NoSize (panel ket thuc khi im 3 giay)
    Token nam trong lcd_panel/src/panel_config.h (PANEL_BT_TOKEN, mac dinh "TBT-3D-up").

    DIEN BIEN MOT PHIEN (dung nhu panel_bt_upload.cpp):
        PC  -> PUT ...            panel -> OK <ten> <so_byte>
        PC  -> <noi dung file>    panel -> PROG <n> moi 64 KB
                                  panel -> OK <ten> <n>   (da nhan du, TRUOC khi tat BT)
                                  panel -> TAT BT (socket DUT la BINH THUONG, khong phai loi)
                                  panel -> bat WiFi, POST len ESP3D, M22/M21, bat lai BT
        PC  -> (tu noi lai)       panel -> DONE <n>  hoac  ERR <ly do>   (ket qua duoc LUU LAI va
                                            gui cho client MOI => script tu noi lai vai lan)
    Vi vay script nay KHONG coi "socket dut sau khi gui xong" la loi: no cho vai giay roi noi lai
    (toi da -ReconnectSec giay) de nhan DONE/ERR.

.PARAMETER File
    Duong dan file G-code tren PC (bat buoc).
.PARAMETER Token
    Token trong panel_config.h (PANEL_BT_TOKEN). Mac dinh: TBT-3D-up.
.PARAMETER Name
    Ten file se ghi tren the SD cua Marlin. Mac dinh: ten file tren PC.
.PARAMETER NoSize
    Gui "PUT <token> <ten> *" (khong biet truoc so byte; panel ket thuc khi im 3 giay).
.PARAMETER ChunkKB
    Gui theo khoi bao nhieu KB (1 KB = 1024 byte). Mac dinh 0 = KHONG dung, lay -ChunkBytes.
    Neu ban truyen -ChunkKB > 0 thi no duoc uu tien hon -ChunkBytes.
.PARAMETER ChunkBytes
    Gui theo khoi bao nhieu BYTE. Mac dinh **256**.
    ⚠️ VI SAO NHO: core Arduino chi co queue nhan **512 byte** (`BluetoothSerial.cpp:43 RX_QUEUE_SIZE
    512`) va callback SPP day TUNG BYTE vao queue voi timeout 0; queue day thi core VUT phan duoi cua
    goi (log `[E][BluetoothSerial.cpp:342] RX Full! Discarding N bytes`) - mat du lieu AM THAM (vong
    dem cua panel van bao drop=0). Khoi 1 KB => goi SPP ~990 byte > 512 => rat de mat duoi goi.
    Do that: khoi 1 KB + nhip 4 KB/s => 35 lan "RX Full" va phai bo phien o 56 KB; khoi 256 B +
    nhip 6 KB/s => **0 lan "RX Full"** va gui du 63.472 B (xem README 9.14).
.PARAMETER WindowKB
    Cua so byte CHUA DUOC PANEL XAC NHAN toi da (dieu khien luong theo dong "PROG <n>" cua panel).
    Mac dinh 8. ⚠️ PHAI LON HON buoc PROG cua panel (buoc 4 KB -> cua so >= 8 KB), neu khong se
    gui het cua so roi ngoi doi mot dong PROG khong bao gio toi (deadlock).
.PARAMETER PaceKBps
    Nhip gui trung binh toi da, KB/s (leaky bucket). Mac dinh **6**. DAY LA LOP QUAN TRONG NHAT: core
    Arduino chi co queue nhan ~512 byte day theo TUNG BYTE voi timeout 0, queue day la VUT phan duoi
    cua goi (log `[E][BluetoothSerial.cpp:342] RX Full! Discarding N bytes`) - gui giap la thi mat
    du lieu du nhip trung binh cham. Dat 0 de TAT gioi han nhip (chi nen lam khi panel da co queue
    nhan lon / PROG moi 1 KB).
.PARAMETER ProgTimeoutMs
    Cho toi da bao lau de panel xac nhan them byte (PROG nhay len) truoc khi bao loi dung han.
    Mac dinh 4000 (phai < PANEL_BT_STALL_MS = 8000 cua panel).
.PARAMETER ProgProbeMs
    Neu trong bao lau dau ma KHONG thay dong "PROG" nao thi coi la firmware CU (PROG moi 64 KB)
    va chi con dung nhip deu. Mac dinh 1200 (ngan, de khong dung gui lam panel bi "stall").
.PARAMETER TimeoutSec
    Tong thoi gian toi da cho pha 1 (gui du lieu). Mac dinh 600.
.PARAMETER ReconnectSec
    Sau khi gui xong, con cho + tu noi lai toi da bao nhieu giay de nhan DONE/ERR. Mac dinh 90.
.PARAMETER Mac
    Ghi de MAC cua panel, dang 48:AF:F3:70:B1:FA (hoac 48AFF370B1FA).
.PARAMETER Channel
    Ep kenh RFCOMM (bo qua SDP). Duong NHANH NHAT va chac nhat: -Channel 1 (panel CYD dung kenh 1).
.PARAMETER PrimaryTimeoutMs
    Timeout cho ket noi kenh CHINH (kenh 1 / kenh do -Channel ep / kenh do SDP tim thay). Mac dinh 4000.
.PARAMETER ChannelTimeoutMs
    Timeout cho moi kenh khi QUET 2..ScanMaxChannel. Mac dinh 1200.
.PARAMETER ScanMaxChannel
    Quet kenh RFCOMM 2..N khi kenh 1 khong noi duoc. Mac dinh 30 (0 = tat quet).
.PARAMETER ForceSdp
    Bo qua cach 1 (port=0), dung ngay WSALookupServiceBegin/Next de lay kenh.
.PARAMETER ConnectTimeoutMs
    Timeout cho mot lan connect() (mac dinh 12000).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File D:\Marlin\lcd_panel\tools\bt-upload-socket.ps1 -File d:\0in\V300_Part3_proj.gcode

.EXAMPLE
    .\bt-upload-socket.ps1 -File .\benchy.gco -Name B2.gco -Mac 48:AF:F3:70:B1:FA

.NOTES
    MA THOAT: 0 = thay DONE | 2 = thay ERR | 3 = khong noi duoc | 4 = het thoi gian cho DONE/ERR
              1 = loi dau vao (thieu file, MAC sai...)

    ===========================================================================
    DO THAT TREN MAY NAY (Windows + Intel BT, panel TBT-PANEL 48:AF:F3:70:B1:FA):
    ===========================================================================
    1) MAC -> btAddr: dung CHINH chuoi MAC doc nhu so hex binh thuong
          48:AF:F3:70:B1:FA -> btAddr = 0x48AFF370B1FA
       (KHONG dao byte nhu nhieu tai lieu noi. Bang chung: WSALookupServiceNext voi NS_BTH +
        LUP_CONTAINERS tra ve SOCKADDR_BTH cua chinh TBT-PANEL; doc ULONGLONG tai offset 2 duoc
        0x48AFF370B1FA - khop InstanceId BTHENUM\DEV_48AFF370B1FA. Dao byte => connect() luon
        HET THOI GIAN vi Windows di tim mot dia chi khong ton tai.)
    2) SOCKADDR_BTH PACK = 1, tong 30 byte (ws2bth.h boc trong <pshpack1.h>):
           +0 addressFamily (2) | +2 btAddr (8, little-endian) | +10 serviceClassId GUID (16)
           | +26 port (4, gia tri 0..30)
       Dung layout 40 byte (pack 8, port o +32) => connect() tra 10049 WSAEADDRNOTAVAIL.
    3) WSALookupServiceBegin phai co lpszContext = chuoi do WSAAddressToString() tao ra
       (vi du "(48:AF:F3:70:B1:FA)"), KHONG phai chuoi MAC tho; sai thi tra 10022 WSAEINVAL.
    4) Tren may nay:
         - port = 0 + serviceClassId SPP ("cach 1" cua de bai): connect() HET THOI GIAN
           (Windows khong tu tra duoc kenh tu serviceClassId).
         - SDP (WSALookupServiceBegin/Next + UUID SPP): 10108 WSASERVICE_NOT_FOUND voi ca
           LUP_FLUSHCACHE => Windows KHONG doc duoc SDP record SPP cua panel. Dieu nay khop voi
           viec Control Panel bao "The device you selected does not have a serial port service
           running" va viec Windows khong tao cong COM SPP.
         - QUET KENH: **kenh RFCOMM 1 NOI DUOC** (Bluedroid SPP mac dinh = kenh 1): ~0,9 s lan dau
           (nguoi), ~0,1 s khi da co link.
       => Thu tu uu tien cua script: -Channel n -> SDP -> QUET KENH 1..30 -> port=0 (cuoi cung).
          Muon thu "port = 0" truoc nhu de bai thi them -Port0First.

    PAIR TREN WINDOWS (lam 1 lan): Settings -> Bluetooth & devices -> Add device -> Bluetooth ->
        chon "TBT-PANEL". Neu duoc hoi ma PIN: 74196328 (PANEL_BT_PIN trong panel_config.h).
        Sau khi pair: KHONG can cong COM (day chinh la ly do co script nay).

    ===========================================================================
    TRANG THAI DO TREN PANEL THAT (firmware dang chay, thang 10/2026):
    ===========================================================================
    Transport + giao thuc DA CHAY: noi duoc RFCOMM kenh 1, gui dong header, panel tra loi NGAY
    qua socket. NHUNG panel TU CHOI phien vi RAM cua chinh panel:
        [panel] ERR heap thap (16484 B)                        <- panel tra qua BT
        [BTUP] heap thap (16484 B) -> tu choi phien moi        <- log COM7 cua panel
    Ly do (panel_bt_upload.cpp, st_idle): tu choi khi ESP.getFreeHeap() < PANEL_BT_HEAP_MIN
    = 20 KB (panel_config.h). Do that tren may nay:
        heap luc ranh (BT bat, chua co client) = 21.552 B
        heap khi CO client SPP ket noi        = 16.476 B   (Bluedroid cap them ~5 KB)
        => thieu ~4 KB so voi nguong 20 KB.
    Da thu MOI cach tu phia Windows - KHONG cach nao lam tang heap cua panel:
        - MTU RFCOMM 128 / 672 / 990 (SO_BTH_MTU): heap panel 16.460 / 16.448 / 16.476 B (khong doi)
        - port=0, SDP, quet kenh: chi anh huong buoc NOI, khong lien quan heap.
    => Muon upload duoc phai sua phia FIRMWARE PANEL (ngoai pham vi script nay):
       ha PANEL_BT_HEAP_MIN (vi du 14 KB) hoac tra them ~4 KB heap (xem README muc 9.8 / 9.15).
       Script nay VAN dung de chan doan: no noi duoc, gui header duoc va in dung dong ERR cua panel.

    GOP Y: theo doi log panel qua cong serial USB (vi du COM7 @115200, DtrEnable=$true) de doi chieu
        cac dong [BTUP] (bt=.. st=.. rx=.. ring=.. drop=.. KB/s, "TAT BT", "DONE").
        Co san: lcd_panel/tools/panel-log.ps1 -Seconds 120
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$File,
    [string]$Token = 'TBT-3D-up',
    [string]$Name,
    [switch]$NoSize,
    [int]$ChunkKB = 0,
    [int]$ChunkBytes = 256,
    [int]$TimeoutSec = 600,
    [int]$ReconnectSec = 90,
    [string]$Mac,
    [int]$Channel = -1,
    [switch]$ForceSdp,
    [switch]$Port0First,
    [int]$WindowKB = 8,
    [int]$PaceKBps = 6,
    [int]$ProgTimeoutMs = 4000,
    [int]$ProgProbeMs = 1200,
    [int]$ConnectTimeoutMs = 12000,
    [int]$PrimaryTimeoutMs = 4000,
    [int]$ChannelTimeoutMs = 1200,
    [int]$ScanMaxChannel = 30
)

$ErrorActionPreference = 'Continue'

# ===========================================================================
# 0) Kiem tra dau vao
# ===========================================================================
if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
    Write-Host "KHONG thay file: $File" -ForegroundColor Red
    exit 1
}
$fi = Get-Item -LiteralPath $File
if ($fi.Length -le 0) {
    Write-Host "File rong (0 byte): $File" -ForegroundColor Red
    exit 1
}
if ([string]::IsNullOrWhiteSpace($Token)) {
    Write-Host "Token rong - phai dung dung PANEL_BT_TOKEN trong panel_config.h" -ForegroundColor Red
    exit 1
}
if ($TimeoutSec -lt 10) { $TimeoutSec = 10 }
if ($ReconnectSec -lt 5) { $ReconnectSec = 5 }
if ($WindowKB -lt 1) { $WindowKB = 1 }

$sendName = if ($Name) { $Name } else { $fi.Name }
$size = [int64]$fi.Length
# Kich thuoc khoi: -ChunkKB (neu > 0) duoc uu tien; khong thi dung -ChunkBytes (mac dinh 256 B).
$chunkBytes = if ($ChunkKB -gt 0) { $ChunkKB * 1024 } else { $ChunkBytes }

# ===========================================================================
# 1) Tim MAC cua panel (tu dong neu khong co -Mac)
# ===========================================================================
function Normalize-Mac([string]$s) {
    if (-not $s) { return $null }
    $h = ($s -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
    if ($h.Length -ne 12) { return $null }
    return $h
}
function Format-Mac([string]$hex12) {
    return (($hex12 -split '(.{2})' | Where-Object { $_ -ne '' }) -join ':')
}

$macHex = Normalize-Mac $Mac
if ($Mac -and -not $macHex) {
    Write-Host "MAC khong hop le: '$Mac' (can 12 chu so hex, vi du 48:AF:F3:70:B1:FA)" -ForegroundColor Red
    exit 1
}
$macSource = 'tham so -Mac'
if (-not $macHex) {
    $dev = $null
    try {
        $dev = Get-PnpDevice -Class Bluetooth -ErrorAction Stop |
               Where-Object { $_.FriendlyName -match 'TBT-PANEL' -and $_.InstanceId -match 'DEV_([0-9A-Fa-f]{12})' } |
               Select-Object -First 1
    } catch { }
    if ($dev) {
        [void]($dev.InstanceId -match 'DEV_([0-9A-Fa-f]{12})')
        $macHex = $Matches[1].ToUpperInvariant()
        $macSource = "tu dong (Get-PnpDevice: $($dev.FriendlyName))"
    }
}
if (-not $macHex) {
    Write-Host "KHONG tim duoc MAC cua TBT-PANEL." -ForegroundColor Red
    Write-Host "Cac thiet bi Bluetooth dang thay:" -ForegroundColor Yellow
    try {
        Get-PnpDevice -Class Bluetooth -ErrorAction Stop |
            Where-Object { $_.InstanceId -match 'DEV_' } |
            ForEach-Object { Write-Host ("  {0,-22} {1}" -f $_.FriendlyName, $_.InstanceId) }
    } catch { }
    Write-Host "Goi y: pair 'TBT-PANEL' truoc, hoac chay lai voi -Mac 48:AF:F3:70:B1:FA" -ForegroundColor Yellow
    exit 3
}

# btAddr = chuoi MAC doc nhu so hex binh thuong (xem MakeAddr trong C# ben duoi)
$btAddr = [Convert]::ToUInt64($macHex, 16)

Write-Host "=== Gui file qua Bluetooth RFCOMM socket (khong can cong COM) ===" -ForegroundColor Cyan
Write-Host ("File   : {0} ({1} byte = {2:N1} KB)" -f $fi.FullName, $size, ($size / 1KB))
Write-Host ("Gui ten: {0}" -f $sendName)
Write-Host ("Header : PUT {0} {1} {2}" -f $Token, $sendName, $(if ($NoSize) { '*' } else { $size }))
Write-Host ("Panel  : {0}  (MAC {1}, btAddr=0x{2:X12}) [{3}]" -f 'TBT-PANEL', (Format-Mac $macHex), $btAddr, $macSource)
$chunkText = if ($ChunkBytes -gt 0) { "$ChunkBytes byte" } else { "$ChunkKB KB" }
Write-Host ("Khoi   : {0} | nhip <= {1} KB/s | timeout gui {2} s | cho DONE toi da {3} s" -f $chunkText, $PaceKBps, $TimeoutSec, $ReconnectSec)
Write-Host ""

# ===========================================================================
# 2) C# inline: goi ws2_32.dll (AF_BTH / RFCOMM) - khong dung System.IO.Ports
# ===========================================================================
$csSource = @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class BthSpp
{
    public const int AF_BTH          = 32;      // winsock2.h / ws2bth.h
    public const int SOCK_STREAM     = 1;
    public const int BTHPROTO_RFCOMM = 3;
    public const int SOL_SOCKET      = 0xFFFF;
    public const int SO_SNDTIMEO     = 0x1005;
    public const int SO_RCVTIMEO     = 0x1006;
    public const int NS_BTH          = 16;
    public const int LUP_RETURN_ADDR = 0x00000100;
    public const int LUP_FLUSHCACHE  = 0x00001000;
    public const int WSAETIMEDOUT    = 10060;
    public const int WSAEWOULDBLOCK  = 10035;

    public const string SPP_UUID = "00001101-0000-1000-8000-00805F9B34FB";

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WSAQUERYSET
    {
        public int dwSize;
        [MarshalAs(UnmanagedType.LPWStr)] public string lpszServiceInstanceName;
        public IntPtr lpServiceClassId;
        public IntPtr lpVersion;
        [MarshalAs(UnmanagedType.LPWStr)] public string lpszComment;
        public int dwNameSpace;
        public IntPtr lpNSProviderId;
        [MarshalAs(UnmanagedType.LPWStr)] public string lpszContext;
        public int dwNumberOfProtocols;
        public IntPtr lpafpProtocols;
        [MarshalAs(UnmanagedType.LPWStr)] public string lpszQueryString;
        public int dwNumberOfCsAddrs;
        public IntPtr lpcsaBuffer;
        public int dwOutputFlags;
        public IntPtr lpBlob;
    }

    [DllImport("ws2_32.dll")] static extern int WSAStartup(ushort ver, byte[] data);
    [DllImport("ws2_32.dll")] static extern int WSACleanup();
    [DllImport("ws2_32.dll")] static extern int WSAGetLastError();
    [DllImport("ws2_32.dll")] static extern IntPtr socket(int af, int type, int proto);
    [DllImport("ws2_32.dll")] static extern int connect(IntPtr s, byte[] name, int namelen);
    [DllImport("ws2_32.dll")] static extern int send(IntPtr s, byte[] buf, int len, int flags);
    [DllImport("ws2_32.dll")] static extern int recv(IntPtr s, byte[] buf, int len, int flags);
    [DllImport("ws2_32.dll")] static extern int closesocket(IntPtr s);
    [DllImport("ws2_32.dll")] static extern int setsockopt(IntPtr s, int level, int opt, ref int val, int len);
    [DllImport("ws2_32.dll")] static extern int getsockopt(IntPtr s, int level, int opt, byte[] val, ref int len);
    [DllImport("ws2_32.dll")] static extern int ioctlsocket(IntPtr s, int cmd, ref int argp);
    [DllImport("ws2_32.dll")] static extern int select(int nfds, IntPtr rfds, IntPtr wfds, IntPtr efds, ref TIMEVAL tv);
    [DllImport("ws2_32.dll", CharSet = CharSet.Unicode)]
    static extern int WSAAddressToString(byte[] lpsaAddress, int dwAddressLength,
                                         IntPtr lpProtocolInfo, StringBuilder lpszAddressString,
                                         ref int lpdwAddressStringLength);
    [DllImport("ws2_32.dll", CharSet = CharSet.Unicode)]
    static extern int WSALookupServiceBegin(ref WSAQUERYSET qs, int flags, out IntPtr h);
    [DllImport("ws2_32.dll")] static extern int WSALookupServiceNext(IntPtr h, int flags, ref int len, IntPtr buf);
    [DllImport("ws2_32.dll")] static extern int WSALookupServiceEnd(IntPtr h);

    static IntPtr _s = new IntPtr(-1);
    static bool   _wsa = false;

    public static string ErrName(int code)
    {
        string m;
        try { m = new Win32Exception(code).Message; } catch { m = "khong ro"; }
        return string.Format("{0} (0x{1:X8}) {2}", code, code, m);
    }

    public static bool Startup(out string msg)
    {
        msg = "";
        if (_wsa) return true;
        byte[] d = new byte[512];
        int r = WSAStartup(0x0202, d);
        if (r != 0) { msg = "WSAStartup loi " + ErrName(r); return false; }
        _wsa = true;
        return true;
    }

    public static void Cleanup()
    {
        Close();
        if (_wsa) { WSACleanup(); _wsa = false; }
    }

    public static int NewSocket(out string msg)
    {
        msg = "";
        Close();
        IntPtr h = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (h == new IntPtr(-1)) { int e = WSAGetLastError(); msg = ErrName(e); return e; }
        _s = h;
        return 0;
    }

    public static int SetTimeout(int sendMs, int recvMs)
    {
        int a = sendMs, b = recvMs;
        if (setsockopt(_s, SOL_SOCKET, SO_SNDTIMEO, ref a, 4) != 0) return WSAGetLastError();
        if (setsockopt(_s, SOL_SOCKET, SO_RCVTIMEO, ref b, 4) != 0) return WSAGetLastError();
        return 0;
    }

    public static void Close()
    {
        if (_s != new IntPtr(-1)) { closesocket(_s); _s = new IntPtr(-1); }
    }

    // SOCKADDR_BTH nam trong ws2bth.h GIUA <pshpack1.h> ... <poppack.h> => PACK = 1, tong 30 byte:
    //   +0  USHORT addressFamily (=32)
    //   +2  BTH_ADDR btAddr (8 byte; byte DAU TIEN cua chuoi MAC nam o byte THAP nhat)
    //   +10 GUID serviceClassId (SPP)  - dung khi port = 0 (Windows tu tra kenh qua SDP)
    //   +26 ULONG port (0..30; 0 = de Windows tu tra kenh tu serviceClassId)
    public const int SOCKADDR_BTH_LEN = 30;
    public const int BTADDR_OFF = 2;
    public const int GUID_OFF   = 10;
    public const int PORT_OFF   = 26;

    public static byte[] MakeAddr(string macHex12, int port)
    {
        // ⚠️ THU TU BYTE (do THAT tren may nay, 2026):
        //   btAddr = CHINH chuoi MAC doc nhu so hex binh thuong: 48:AF:F3:70:B1:FA -> 0x48AFF370B1FA
        //   (KHONG dao byte!). Trong SOCKADDR_BTH (pack=1) gia tri do duoc ghi little-endian, nen
        //   6 byte dau tien tai offset 2 se la FA B1 70 F3 AF 48.
        //   Bang chung: WSALookupServiceNext(NS_BTH + LUP_CONTAINERS) tra ve chinh SOCKADDR_BTH cua
        //   TBT-PANEL; doc ULONGLONG tai offset 2 (little-endian) duoc 0x48AFF370B1FA - dung bang
        //   phan InstanceId BTHENUM\DEV_48AFF370B1FA. Thu dao byte (0xFAB170F3AF48) thi connect()
        //   luon HET THOI GIAN (Windows di tim mot thiet bi khong ton tai).
        ulong addr = Convert.ToUInt64(macHex12, 16);
        byte[] a = new byte[SOCKADDR_BTH_LEN];
        a[0] = (byte)(AF_BTH & 0xFF);
        a[1] = (byte)((AF_BTH >> 8) & 0xFF);
        for (int i = 0; i < 8; i++) a[BTADDR_OFF + i] = (byte)((addr >> (8 * i)) & 0xFF);
        byte[] g = new Guid(SPP_UUID).ToByteArray();     // dung layout GUID cua Windows
        Array.Copy(g, 0, a, GUID_OFF, 16);
        a[PORT_OFF]     = (byte)(port & 0xFF);
        a[PORT_OFF + 1] = (byte)((port >> 8) & 0xFF);
        a[PORT_OFF + 2] = (byte)((port >> 16) & 0xFF);
        a[PORT_OFF + 3] = (byte)((port >> 24) & 0xFF);
        return a;
    }

    public static string DumpAddr(string macHex12, int port)
    {
        byte[] a = MakeAddr(macHex12, port);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < a.Length; i++) sb.Append(a[i].ToString("X2"));
        return sb.ToString();
    }

    // Chuoi dia chi do WSAAddressToString tra ve - MSDN yeu cau lpszContext cua
    // WSALookupServiceBegin phai la chuoi NAY (khong phai "48AFF370B1FA" tho).
    public static string AddrToString(string macHex12)
    {
        byte[] a = MakeAddr(macHex12, 0);
        int len = 512;
        StringBuilder sb = new StringBuilder(len);
        int r = WSAAddressToString(a, a.Length, IntPtr.Zero, sb, ref len);
        if (r != 0) return null;
        return sb.ToString();
    }

    public static int Connect(string macHex12, int port, out string msg)
    {
        msg = "";
        byte[] a = MakeAddr(macHex12, port);
        if (connect(_s, a, a.Length) != 0) { int e = WSAGetLastError(); msg = ErrName(e); return e; }
        return 0;
    }

    public static int QueryStructSize() { return Marshal.SizeOf(typeof(WSAQUERYSET)); }

    // ---- connect() co timeout (non-blocking + select) - dung cho quet kenh RFCOMM ----
    // Tra ve 0 = noi duoc; >0 = ma loi Winsock; WSAETIMEDOUT (10060) = het thoi gian cho.
    public const int FIONBIO  = unchecked((int)0x8004667E);
    public const int SO_ERROR = 0x1007;
    public const int WSAENOTCONN = 10057;

    [StructLayout(LayoutKind.Sequential)]
    struct TIMEVAL { public int tv_sec; public int tv_usec; }

    public static int ConnectTimeout(string macHex12, int port, int timeoutMs, out string msg)
    {
        msg = "";
        int r = NewSocket(out msg);
        if (r != 0) return r;
        int nb = 1;
        ioctlsocket(_s, FIONBIO, ref nb);
        byte[] a = MakeAddr(macHex12, port);
        if (connect(_s, a, a.Length) == 0) { RestoreBlocking(); return 0; }
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK) { msg = ErrName(e); return e; }

        IntPtr fds = Marshal.AllocHGlobal(520);
        try
        {
            for (int i = 0; i < 520; i++) Marshal.WriteByte(fds, i, 0);
            Marshal.WriteInt32(fds, 0, 1);                       // fd_count = 1
            if (IntPtr.Size == 8) Marshal.WriteIntPtr(fds, 8, _s);
            else Marshal.WriteInt32(fds, 4, _s.ToInt32());
            TIMEVAL tv;
            tv.tv_sec  = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            int n = select(0, IntPtr.Zero, fds, IntPtr.Zero, ref tv);
            if (n == 0) { msg = string.Format("connect() het thoi gian {0} ms (khong tra loi)", timeoutMs); return WSAETIMEDOUT; }
            if (n < 0) { int e2 = WSAGetLastError(); msg = "select(): " + ErrName(e2); return e2; }
            int l = 4;
            byte[] ob = new byte[4];
            getsockopt(_s, SOL_SOCKET, SO_ERROR, ob, ref l);
            int serr = BitConverter.ToInt32(ob, 0);
            if (serr != 0) { msg = ErrName(serr); return serr; }
            RestoreBlocking();
            return 0;                                            // noi duoc!
        }
        finally { Marshal.FreeHGlobal(fds); }
    }

    static void RestoreBlocking()
    {
        int zero = 0;
        ioctlsocket(_s, FIONBIO, ref zero);                      // tra socket ve blocking
    }

    // Gui het 'len' byte dau cua buf (tu lo phan send() ngan).
    public static int SendAll(byte[] buf, int len, out string msg)
    {
        msg = "";
        int off = 0;
        while (off < len)
        {
            byte[] part;
            if (off == 0) { part = buf; }
            else { part = new byte[len - off]; Array.Copy(buf, off, part, 0, len - off); }
            int n = send(_s, part, len - off, 0);
            if (n <= 0) { int e = WSAGetLastError(); msg = ErrName(e); return e; }
            off += n;
        }
        return 0;
    }

    // >0 = so byte nhan duoc | 0 = khong co du lieu (timeout) | -1 = socket dong/loi
    public static int RecvSome(byte[] buf, out string msg)
    {
        msg = "";
        int n = recv(_s, buf, buf.Length, 0);
        if (n > 0) return n;
        if (n == 0) { msg = "socket dong (recv tra ve 0 - BT ben panel da tat)"; return -1; }
        int e = WSAGetLastError();
        if (e == WSAETIMEDOUT || e == WSAEWOULDBLOCK) return 0;
        msg = ErrName(e);
        return -1;
    }

    // ---- Cach 2: SDP (WSALookupServiceBegin/Next, NS_BTH + UUID SPP + LUP_RETURN_ADDR) ----
    // Tra ve 0 va channel = kenh RFCOMM; -1 = khong tim thay (msg co ly do).
    public static int LookupChannel(string macHex12, bool useContext, bool flushCache,
                                    out int channel, out string msg)
    {
        channel = -1;
        msg = "";
        bool x64 = (IntPtr.Size == 8);
        int offNumCsAddrs   = x64 ? 88 : 44;
        int offCsaBuffer    = x64 ? 96 : 48;
        int csaSize         = x64 ? 40 : 24;      // CSADDR_INFO (winsock2.h, pack 8)
        int offRemoteAddr   = x64 ? 16 : 8;       // CSADDR_INFO.RemoteAddr

        ulong want = Convert.ToUInt64(macHex12, 16);     // cung quy uoc voi MakeAddr()

        IntPtr pGuid = Marshal.AllocHGlobal(16);
        IntPtr buf   = Marshal.AllocHGlobal(0x4000);
        IntPtr h     = IntPtr.Zero;
        try
        {
            Marshal.Copy(new Guid(SPP_UUID).ToByteArray(), 0, pGuid, 16);
            WSAQUERYSET qs = new WSAQUERYSET();
            qs.dwSize = Marshal.SizeOf(typeof(WSAQUERYSET));
            qs.dwNameSpace = NS_BTH;
            qs.lpServiceClassId = pGuid;
            // MSDN: lpszContext = dia chi thiet bi dang CHUOI do WSAAddressToString tao ra
            // (khong phai "48AFF370B1FA" tho - chuoi sai => WSALookupServiceBegin tra 10022).
            qs.lpszContext = useContext ? AddrToString(macHex12) : null;
            qs.dwNumberOfCsAddrs = 0;
            int flags = LUP_RETURN_ADDR;
            if (flushCache) flags |= LUP_FLUSHCACHE;         // MSDN khuyen nghi bat co nay

            int r = WSALookupServiceBegin(ref qs, flags, out h);
            if (r != 0) { int e = WSAGetLastError(); msg = "WSALookupServiceBegin: " + ErrName(e); return -1; }

            for (;;)
            {
                int len = 0x4000;
                int rr = WSALookupServiceNext(h, flags, ref len, buf);
                if (rr != 0)
                {
                    int e = WSAGetLastError();
                    if (e == 10110 || e == 10102) break;      // WSA_E_NO_MORE / WSAENOMORE
                    if (e == 10108) { msg = "WSASERVICE_NOT_FOUND (khong co SPP tren thiet bi)"; return -1; }
                    if (e == 122) { msg = "buffer SDP qua nho"; return -1; }   // ERROR_INSUFFICIENT_BUFFER
                    msg = "WSALookupServiceNext: " + ErrName(e);
                    return -1;
                }
                int nCs = 0;
                try { nCs = Marshal.ReadInt32(buf, offNumCsAddrs); } catch { nCs = 0; }
                IntPtr pCsa = IntPtr.Zero;
                try { pCsa = Marshal.ReadIntPtr(buf, offCsaBuffer); } catch { pCsa = IntPtr.Zero; }
                for (int i = 0; i < nCs && pCsa != IntPtr.Zero; i++)
                {
                    IntPtr blk = new IntPtr(pCsa.ToInt64() + i * csaSize);
                    IntPtr pAddr = Marshal.ReadIntPtr(blk, offRemoteAddr);
                    int aLen = Marshal.ReadInt32(blk, offRemoteAddr + IntPtr.Size);
                    if (pAddr == IntPtr.Zero || aLen < SOCKADDR_BTH_LEN) continue;
                    ushort fam = (ushort)Marshal.ReadInt16(pAddr, 0);
                    if (fam != AF_BTH) continue;
                    ulong got = (ulong)Marshal.ReadInt64(pAddr, BTADDR_OFF);
                    int port = Marshal.ReadInt32(pAddr, PORT_OFF);
                    if (useContext == false && got != want) continue;    // loc dung thiet bi
                    if (port <= 0 || port > 30) continue;
                    channel = port;
                    return 0;
                }
            }
            if (channel < 0) msg = "SDP khong tra ve kenh RFCOMM nao cho thiet bi nay";
            return -1;
        }
        finally
        {
            if (h != IntPtr.Zero) WSALookupServiceEnd(h);
            Marshal.FreeHGlobal(pGuid);
            Marshal.FreeHGlobal(buf);
        }
    }
}
'@

if (-not ('BthSpp' -as [type])) {
    try {
        Add-Type -TypeDefinition $csSource -Language CSharp -ErrorAction Stop
    } catch {
        Write-Host "Khong bien dich duoc C# inline: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

$wsaMsg = ''
if (-not [BthSpp]::Startup([ref]$wsaMsg)) {
    Write-Host "Khoi tao Winsock that bai: $wsaMsg" -ForegroundColor Red
    exit 3
}
Write-Host ("Winsock: WSAStartup(0x202) OK | SPP UUID {0}" -f [BthSpp]::SPP_UUID) -ForegroundColor DarkGray

# ===========================================================================
# 3) Trang thai phien + ham doc phan hoi panel
# ===========================================================================
$script:PanelLines = New-Object System.Collections.ArrayList
$script:RxText     = ''
$script:Done       = $false
$script:ErrFlag    = $false
$script:ErrLine    = ''
$script:DoneLine   = ''
$script:SocketDead = $false
$script:Sent       = [int64]0
$script:Reconnects = 0
$script:SawSession = $false      # da thay "OK <ten> ..." / "PROG" cua phien nay chua
$script:Phase      = 'connect'   # connect | header | send | wait | reconnect (de gan nhan cho tung dong)
$script:LastProg   = [int64]0    # so byte panel XAC NHAN da nhan/ghi xong (dong "PROG <n>")
$script:ProgSeen   = $false      # da nhan duoc PROG nao chua
$script:FlowError  = $false      # panel khong xac nhan byte nao trong ProgTimeoutMs

# Giai thich cac dong ERR cua panel (panel_bt_upload.cpp) - in cho de hieu, khong phai suy dien.
function Explain-Err([string]$line) {
    $t = $line
    if ($t -match 'heap thap \(([0-9]+) B\)') {
        $hb = $Matches[1]
        return @(
            "  => CUA AN TOAN RAM cua panel: ESP.getFreeHeap() = $hb B < PANEL_BT_HEAP_MIN = 20 KB",
            "     (panel_config.h:416). Panel TU CHOI MO phien vi thieu RAM, KHONG phai loi script/Windows.",
            "     Do that: heap panel luc ranh (BT bat, chua co client) ~21,5 KB; khi CO client SPP ~16,5 KB",
            "     (Bluedroid cap ~5 KB cho ket noi) => thieu ~4 KB. Ban firmware moi (LV_MEM_SIZE 24 KB +",
            "     buffer ve nho lai) tra them ~18,7 KB DRAM => heap sau khi bat BT ~43 KB => het loi nay.",
            "     Xem README muc 9.8 / 9.15. Script khong can sua gi cho viec nay."
        )
    }
    if ($t -match 'sai ma token')        { return @("  => Token khong khop PANEL_BT_TOKEN trong panel_config.h (dung -Token ...).") }
    if ($t -match 'dang in')             { return @("  => Marlin dang IN - panel chi nhan file khi may ranh.") }
    if ($t -match 'the TF chua san sang'){ return @("  => The TF cua CHINH PANEL chua mount duoc (cam lai the FAT32 roi doi ~2,5 s).") }
    if ($t -match 'chua bat che do')     { return @("  => Che do nhan file dang KHOA (go 'BTUP ON' o console COM7 cua panel).") }
    if ($t -match 'thieu RAM cho WiFi')  { return @("  => Pha 2 (WiFi/ESP3D) thieu RAM: panel da bat lai BT, file tam bi xoa.") }
    if ($t -match 'ESP3D')               { return @("  => Module ESP3D loi/khong tra loi (kiem tra AP 'Voron2308' + IP module).") }
    if ($t -match 'BT mat ket noi giua pha 1') { return @("  => Ket noi BT dut GIUA luc dang gui => panel xoa file tam (khong de file thieu).") }
    if ($t -match 'thieu du lieu')       { return @("  => Panel nhan thieu byte so voi header (mang BT cham/nghen) => thu lai voi -ChunkKB nho hon.") }
    if ($t -match 'chi nhan')            { return @("  => So byte nhan duoc khac header => file khong duoc ghi.") }
    if ($t -match 'header')              { return @("  => Dong header sai cu phap: 'PUT <token> <ten_file> <so_byte>'.") }
    return @()
}

function Show-PanelLine([string]$line) {
    [void]$script:PanelLines.Add($line)
    $col = 'Gray'
    if ($line -like 'OK*' -or $line -like 'PROG*') { $script:SawSession = $true }
    if ($line -match '^PROG\s+([0-9]+)') {
        # PROG <n> = so byte panel DA NHAN (da lay ra khoi vong dem BT) => tin hieu DIEU KHIEN LUONG.
        $script:LastProg = [int64]$Matches[1]
        $script:ProgSeen = $true
    }
    if ($line -like 'ERR*')  { $col = 'Red';    $script:ErrFlag = $true;  $script:ErrLine = $line }
    if ($line -like 'DONE*') { $col = 'Green';  $script:Done = $true;     $script:DoneLine = $line }
    if ($line -like 'OK*')   { $col = 'Cyan' }
    if ($line -like 'PROG*') { $col = 'DarkCyan' }

    # Nhan cho tung dong: phan hoi TRUC TIEP cua phien nay hay KET QUA PHIEN TRUOC duoc gui lai?
    $tag = ''
    if ($line -like 'ERR*' -or $line -like 'DONE*') {
        if ($script:Phase -eq 'reconnect') {
            $tag = '  <- panel gui lai KET QUA cua phien vua roi (luc POST thi BT da tat nen app khong nhan duoc ngay)'
        } elseif ($script:Phase -eq 'header' -and -not $script:SawSession) {
            $tag = '  <- phan hoi NGAY sau dong header: ERR mo phien, HOAC ket qua phien TRUOC neu panel con luu'
        } else {
            $tag = '  <- phan hoi TRUC TIEP cua phien nay'
        }
    }
    Write-Host ("  [panel] {0}" -f $line) -ForegroundColor $col
    if ($tag) { Write-Host $tag -ForegroundColor DarkYellow }
    if ($line -like 'ERR*') {
        foreach ($l in (Explain-Err $line)) { Write-Host $l -ForegroundColor DarkYellow }
    }
}

function Receive-Panel([int]$ms) {
    $buf = New-Object byte[] 4096
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalMilliseconds -lt $ms) {
        if ($script:SocketDead) { return }
        $msg = ''
        $n = [BthSpp]::RecvSome($buf, [ref]$msg)
        if ($n -gt 0) {
            $script:RxText += [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
            while ($script:RxText.Contains("`n")) {
                $i = $script:RxText.IndexOf("`n")
                $line = $script:RxText.Substring(0, $i).TrimEnd("`r")
                $script:RxText = $script:RxText.Substring($i + 1)
                if ($line.Length -gt 0) { Show-PanelLine $line }
            }
        } elseif ($n -lt 0) {
            $script:SocketDead = $true
            Write-Host ("  [socket] DUT: {0}" -f $msg) -ForegroundColor DarkYellow
            return
        } else {
            Start-Sleep -Milliseconds 20
        }
        if ($script:Done -or $script:ErrFlag) { return }
    }
}

# ===========================================================================
# 4) Noi toi panel: cach 1 = port 0 (Windows tu tra kenh qua SDP tu serviceClassId)
#                   cach 2 = WSALookupServiceBegin/Next (NS_BTH + UUID SPP) -> connect(kenh)
#                   cach 3 = quet kenh RFCOMM 1..N (khong can SDP)
# ===========================================================================
function Open-Socket([int]$chan, [int]$timeoutMs) {
    $msg = ''
    $r = [BthSpp]::ConnectTimeout($macHex, $chan, $timeoutMs, [ref]$msg)
    if ($r -ne 0) {
        return @{ ok = $false; code = $r; msg = $msg; how = (Get-HowText $chan); chan = $chan }
    }
    [void][BthSpp]::SetTimeout(10000, 200)
    return @{ ok = $true; code = 0; msg = ''; how = (Get-HowText $chan); chan = $chan }
}
function Get-HowText([int]$chan) {
    if ($chan -le 0) { return 'cach 1: port=0 + serviceClassId SPP (Windows tu tra kenh qua SDP)' }
    return "kenh RFCOMM $chan"
}
function Find-SppComPort {
    # Neu Windows CO tao cong COM SPP cho TBT-PANEL thi tra ve ten cong (duong du phong).
    $c = $null
    try {
        $c = Get-PnpDevice -Class Ports -ErrorAction Stop |
             Where-Object { $_.Status -eq 'OK' -and ($_.FriendlyName -match 'TBT-PANEL' -or $_.FriendlyName -match 'Bluetooth') } |
             ForEach-Object { if ($_.FriendlyName -match '\((COM\d+)\)') { $Matches[1] } } |
             Select-Object -First 1
    } catch { }
    return $c
}

Write-Host ("Chan doan: WSAQUERYSET size = {0} byte (x64 chuan = 120) | SOCKADDR_BTH = {1} byte (pack=1, chuan = 30), port o offset {2})" -f `
    [BthSpp]::QueryStructSize(), [BthSpp]::SOCKADDR_BTH_LEN, [BthSpp]::PORT_OFF) -ForegroundColor DarkGray
$ctxStr = [BthSpp]::AddrToString($macHex)
Write-Host ("Chan doan: sockaddr = {0}" -f [BthSpp]::DumpAddr($macHex, 0)) -ForegroundColor DarkGray
Write-Host ("Chan doan: lpszContext (WSAAddressToString) = '{0}'" -f $ctxStr) -ForegroundColor DarkGray

Write-Host "Dang mo ket noi RFCOMM toi panel..." -ForegroundColor Cyan
$conn = $null
$lastFail = @{ code = 0; msg = '' }
$triedPort0 = $false
$sdpChannels = New-Object System.Collections.ArrayList

# THU TU UU TIEN (do THAT tren may nay - xem phan NOTES):
#   1) KENH 1 TRUOC TIEN, timeout DAI (PrimaryTimeoutMs ~4 s): Bluedroid SPP mac dinh la kenh 1 va
#      do duoc la noi duoc o kenh 1 (~0,9 s lan dau). Day la duong CHINH.
#   2) SDP (WSALookupServiceBegin/Next) chi la TOI UU: Windows co the MAI khong thay record SPP cua
#      panel (10108 WSASERVICE_NOT_FOUND) => KHONG duoc de SDP chan duong, that bai thi di tiep.
#   3) QUET kenh 2..N voi timeout NGAN (ChannelTimeoutMs ~1,2 s).
#   4) port = 0 + serviceClassId ("cach 1" cua de bai) o CUOI, vi do tren may nay: HET THOI GIAN.
#      Dung -Port0First neu muon thu no truoc.

function Try-Channel([int]$ch, [int]$timeoutMs, [string]$why) {
    Write-Host ("  thu kenh {0} (timeout {1} ms): {2}" -f $ch, $timeoutMs, $why) -ForegroundColor DarkGray
    $r = Open-Socket $ch $timeoutMs
    if ($r.ok) { Write-Host ("    -> NOI DUOC o kenh RFCOMM {0}" -f $ch) -ForegroundColor Green }
    else       { Write-Host ("    -> that bai: {0}" -f $r.msg) -ForegroundColor Yellow }
    return $r
}

if ($Channel -ge 0) {
    Write-Host ("Kenh RFCOMM ep bang tham so -Channel: {0}" -f $Channel) -ForegroundColor Yellow
    $res = Try-Channel $Channel $PrimaryTimeoutMs 'kenh do nguoi dung ep'
    if ($res.ok) { $conn = $res } else { $lastFail = $res }
} else {
    # ---- port = 0 (chi khi duoc yeu cau thu truoc) ----
    if ($Port0First -and -not $ForceSdp) {
        $triedPort0 = $true
        Write-Host ("  thu {0} (timeout {1} ms) ..." -f (Get-HowText 0), $ConnectTimeoutMs) -ForegroundColor DarkGray
        $res = Open-Socket 0 $ConnectTimeoutMs
        if ($res.ok) { $conn = $res } else { $lastFail = $res; Write-Host ("    -> that bai: {0}" -f $res.msg) -ForegroundColor Yellow }
    }
    # ---- 1) KENH 1 TRUOC TIEN (timeout dai) ----
    if (-not $conn) {
        $res = Try-Channel 1 $PrimaryTimeoutMs 'KENH 1 - Bluedroid SPP mac dinh (thu truoc tien)'
        if ($res.ok) { $conn = $res } else { $lastFail = $res }
    }
    # ---- 2) SDP - chi la toi uu, khong chan duong ----
    if (-not $conn) {
        Write-Host "  SDP (chi de toi uu; that bai thi khong sao)..." -ForegroundColor DarkGray
        $flushList = if ($ForceSdp) { @($true, $false) } else { @($false) }
        foreach ($useCtx in @($true, $false)) {
            foreach ($flush in $flushList) {
                $ch = -1; $msg = ''
                $r = [BthSpp]::LookupChannel($macHex, $useCtx, $flush, [ref]$ch, [ref]$msg)
                if ($r -eq 0) {
                    Write-Host ("    -> SDP OK: kenh RFCOMM {0} (lpszContext={1}, flush={2})" -f $ch, $useCtx, $flush) -ForegroundColor Green
                    if (-not $sdpChannels.Contains($ch)) { [void]$sdpChannels.Add($ch) }
                } else {
                    Write-Host ("    -> SDP khong tra ve kenh: {0}" -f $msg) -ForegroundColor DarkGray
                }
            }
        }
        foreach ($ch in $sdpChannels) {
            if ($ch -eq 1) { continue }        # kenh 1 da thu o tren
            $res = Try-Channel $ch $PrimaryTimeoutMs 'kenh do SDP tim thay'
            if ($res.ok) { $conn = $res; break }
            $lastFail = $res
        }
    }
    # ---- 3) QUET 2..ScanMaxChannel (timeout ngan) ----
    if (-not $conn -and $ScanMaxChannel -ge 2) {
        Write-Host ("  Quet kenh RFCOMM 2..{0} (moi kenh toi da {1} ms)..." -f $ScanMaxChannel, $ChannelTimeoutMs) -ForegroundColor Cyan
        for ($ch = 2; $ch -le $ScanMaxChannel; $ch++) {
            if ($sdpChannels.Contains($ch)) { continue }
            Write-Host ("`r    thu kenh {0,2}/{1} ...   " -f $ch, $ScanMaxChannel) -NoNewline
            $res = Open-Socket $ch $ChannelTimeoutMs
            if ($res.ok) {
                Write-Host ""
                Write-Host ("    -> NOI DUOC o kenh RFCOMM {0}" -f $ch) -ForegroundColor Green
                $conn = $res
                break
            }
            if ($res.code -ne 10060) { $lastFail = $res }     # giu loi that (khac timeout)
        }
        if (-not $conn) { Write-Host "" }
    }
    # ---- 4) port = 0 (cach 1 cua de bai, thu cuoi) ----
    if (-not $conn -and -not $Port0First -and -not $ForceSdp) {
        $triedPort0 = $true
        Write-Host ("  thu {0} (timeout {1} ms) ..." -f (Get-HowText 0), $ConnectTimeoutMs) -ForegroundColor DarkGray
        $res = Open-Socket 0 $ConnectTimeoutMs
        if ($res.ok) { $conn = $res } else { $lastFail = $res; Write-Host ("    -> that bai: {0}" -f $res.msg) -ForegroundColor Yellow }
    }
}

if (-not $conn) {
    Write-Host ""
    Write-Host "KHONG NOI DUOC toi TBT-PANEL ($(Format-Mac $macHex))." -ForegroundColor Red
    if ($lastFail.msg) { Write-Host ("  Loi Winsock cuoi cung: {0}" -f $lastFail.msg) -ForegroundColor Red }
    Write-Host "  Goi y:" -ForegroundColor Yellow
    Write-Host "    - Kiem tra panel dang BAT, the TF san sang, da pair, khong app khac dang giu BT." -ForegroundColor Yellow
    Write-Host "    - Panel CYD (Bluedroid SPP) thuong o KENH 1: thu -Channel 1 (noi ngay, khong can SDP)." -ForegroundColor Yellow
    Write-Host "    - Neu SDP bao 'WSASERVICE_NOT_FOUND' (10108): Windows KHONG doc duoc SDP record SPP" -ForegroundColor Yellow
    Write-Host "      cua panel (Control Panel cung bao 'does not have a serial port service running')." -ForegroundColor Yellow
    Write-Host "      Do la ly do phai quet kenh / dung -Channel; KHONG anh huong viec gui file." -ForegroundColor Yellow
    Write-Host "    - Neu moi kenh deu HET THOI GIAN: kiem tra trong Windows da 'Connected'/'Paired' chua," -ForegroundColor Yellow
    Write-Host "      va thu Remove device TBT-PANEL roi pair lai (khong hoi ma PIN)." -ForegroundColor Yellow

    # Duong du phong: neu Windows CO tao cong COM SPP thi goi lai script cong COM.
    $comp = Find-SppComPort
    if ($comp) {
        Write-Host ""
        Write-Host ("THAY CONG COM SPP: {0} -> chuyen sang tools/bt-upload.ps1 (duong du phong cong COM)" -f $comp) -ForegroundColor Yellow
        [BthSpp]::Cleanup()
        $oldArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'bt-upload.ps1'),
                     '-Port', $comp, '-Token', $Token, '-File', $fi.FullName, '-Name', $sendName,
                     '-ChunkKB', "$ChunkKB", '-TimeoutSec', "$TimeoutSec")
        if ($NoSize) { $oldArgs += '-NoSize' }
        & powershell @oldArgs
        exit $LASTEXITCODE
    }
    Write-Host "  (Khong thay cong COM SPP nao de lam duong du phong.)" -ForegroundColor DarkGray
    [BthSpp]::Cleanup()
    exit 3
}
Write-Host ("DA NOI (RFCOMM): {0}" -f $conn.how) -ForegroundColor Green
Write-Host ""
$connectChan = $conn.chan        # kenh da noi duoc - dung lai khi phai noi lai o pha 2

# ===========================================================================
# 5) Gui header + noi dung file
# ===========================================================================
function Send-Bytes([byte[]]$data, [int]$len) {
    $msg = ''
    $r = [BthSpp]::SendAll($data, $len, [ref]$msg)
    if ($r -ne 0) {
        $script:SocketDead = $true
        Write-Host ("`n  [socket] loi send(): {0}" -f $msg) -ForegroundColor Red
        return $false
    }
    return $true
}

[void][BthSpp]::SetTimeout(10000, 1)      # trong luc gui: recv timeout ngan cho khoi chan
$header = if ($NoSize) { "PUT $Token $sendName *`r`n" } else { "PUT $Token $sendName $size`r`n" }
$hb = [System.Text.Encoding]::ASCII.GetBytes($header)
$script:Phase = 'header'
[void](Send-Bytes $hb $hb.Length)
Write-Host "Da gui dong header, cho panel tra loi..." -ForegroundColor Cyan
Receive-Panel 2500
if ($script:ErrFlag) {
    Write-Host "Panel TU CHOI MO PHIEN (dong ERR ngay tren la phan hoi truc tiep sau header)." -ForegroundColor Red
    [BthSpp]::Cleanup()
    exit 2
}
if ($script:SocketDead) {
    Write-Host "Socket dut ngay sau khi gui header." -ForegroundColor Red
    [BthSpp]::Cleanup()
    exit 3
}

$fs = [System.IO.File]::OpenRead($fi.FullName)
$buf = New-Object byte[] $chunkBytes
$swTotal = [System.Diagnostics.Stopwatch]::StartNew()
$swRate  = [System.Diagnostics.Stopwatch]::StartNew()
$lastPct = -1
$kbsNow = 0.0

# ---------------------------------------------------------------------------
# DIEU KHIEN LUONG (2 lop, dung dong thoi):
#
#  A) NHIP GUI (-PaceKBps, mac dinh 8): gui theo kieu "leaky bucket" - sau moi khoi phai doi cho
#     nhip trung binh <= PaceKBps. DAY LA LOP QUAN TRONG NHAT, vi nguyen nhan mat du lieu THAT SU
#     nam o core Arduino (do bang log CORE_DEBUG_LEVEL=2):
#         [E][BluetoothSerial.cpp:342] esp_spp_cb(): RX Full! Discarding 605 bytes
#     Callback SPP cua Bluedroid day TUNG BYTE vao mot FreeRTOS queue chi ~512 byte voi timeout 0
#     (BluetoothSerial.cpp:339-345); neu queue day thi core VUT LUON phan duoi cua goi (toi ~990 B)
#     va KHONG ai bao - `drop=` cua panel van la 0. Vi vay gui giap la (burst) la mat du lieu, du
#     nhip trung binh cham; phai gui nho va deu.
#
#  B) CUA SO THEO "PROG <n>" (-WindowKB, mac dinh 8): PROG <n> = so byte panel DA NHAN (lay ra khoi
#     vong dem 4 KB cua no). Giu so byte CHUA DUOC XAC NHAN <= WindowKB.
#     - Firmware MOI (PROG moi 1-4 KB): vua chong tran vong dem vua tu thich nghi theo toc do panel.
#       ⚠️ WindowKB PHAI LON HON buoc PROG cua panel, neu khong se DEADLOCK (gui het cua so roi ngoi
#       doi mot dong PROG khong bao gio toi). Vi du buoc PROG 4 KB -> WindowKB >= 8.
#     - Firmware CU (PROG moi 64 KB): khong thay PROG nao trong -ProgProbeMs dau => tu lui ve chi con
#       lop A (nhip deu) va CANH BAO.
#     - Cho PROG qua -ProgTimeoutMs => dung han + bao loi ro (khong doan mo).
# ---------------------------------------------------------------------------
$windowBytes = [int64]($WindowKB * 1KB)
$progMode    = $null           # $null = chua biet | $true = dung PROG | $false = chi nhip deu
$waitedTotal = 0.0
$paceLimited = [double]0.0
Write-Host ("Dang gui du lieu... (khoi {0} KB, nhip <= {1} KB/s, cua so chua xac nhan {2} KB, cho PROG toi da {3} ms)" -f `
    $ChunkKB, $PaceKBps, $WindowKB, $ProgTimeoutMs) -ForegroundColor Cyan
$script:Phase = 'send'
try {
    while (-not $script:Done -and -not $script:ErrFlag -and -not $script:SocketDead) {
        $want = [int][Math]::Min([int64]$chunkBytes, ($size - $script:Sent))
        if ($want -le 0) { break }
        $n = $fs.Read($buf, 0, $want)
        if ($n -le 0) { break }
        if (-not (Send-Bytes $buf $n)) { break }
        $script:Sent += $n

        Receive-Panel 1                                  # doc xen ke OK/PROG/ERR

        # ---- lop A: giu nhip <= PaceKBps (vua nghi vua hut socket) ----
        if ($PaceKBps -gt 0) {
            $targetSec = ($script:Sent / 1KB) / $PaceKBps
            $sleepSec  = $targetSec - $swTotal.Elapsed.TotalSeconds
            if ($sleepSec -gt 0.002) {
                $swPace = [System.Diagnostics.Stopwatch]::StartNew()
                while ($swPace.Elapsed.TotalSeconds -lt $sleepSec -and
                       -not $script:Done -and -not $script:ErrFlag -and -not $script:SocketDead) {
                    Receive-Panel 25
                }
                $paceLimited += $swPace.Elapsed.TotalSeconds
            }
        }

        # ---- lop B: cua so theo PROG ----
        if ($progMode -eq $true) {
            if (($script:Sent - $script:LastProg) -ge $windowBytes) {
                $sw = [System.Diagnostics.Stopwatch]::StartNew()
                while ((($script:Sent - $script:LastProg) -ge $windowBytes) -and
                       $sw.Elapsed.TotalMilliseconds -lt $ProgTimeoutMs -and
                       -not $script:Done -and -not $script:ErrFlag -and -not $script:SocketDead) {
                    Receive-Panel 200
                }
                $waitedTotal += $sw.Elapsed.TotalSeconds
                if ((($script:Sent - $script:LastProg) -ge $windowBytes) -and
                    -not $script:Done -and -not $script:ErrFlag -and -not $script:SocketDead) {
                    Write-Host ""
                    Write-Host ("LOI DIEU KHIEN LUONG: panel khong xac nhan byte nao trong {0} ms " -f $ProgTimeoutMs) -ForegroundColor Red
                    Write-Host ("     (da gui {0:N0} B, panel moi xac nhan {1:N0} B -> con {2:N0} B chua xac nhan)" -f `
                        $script:Sent, $script:LastProg, ($script:Sent - $script:LastProg)) -ForegroundColor Red
                    Write-Host "     Panel co the da nghen/tat BT. Xem log COM7 ([BTUP] ring=.. drop=..)." -ForegroundColor Red
                    $script:FlowError = $true
                    break
                }
            }
        }
        elseif ($progMode -eq $null) {
            if ($script:Sent -ge $windowBytes) {
                $sw = [System.Diagnostics.Stopwatch]::StartNew()
                while (-not $script:ProgSeen -and $sw.Elapsed.TotalMilliseconds -lt $ProgProbeMs -and
                       -not $script:Done -and -not $script:ErrFlag -and -not $script:SocketDead) {
                    Receive-Panel 200
                }
                if ($script:ProgSeen) {
                    $progMode = $true
                    Write-Host ("`n  [luong] firmware MOI: dung PROG lam phong tin hieu (cua so {0} KB)" -f $WindowKB) -ForegroundColor Green
                } else {
                    $progMode = $false
                    Write-Host ""
                    Write-Host ("  [luong] CANH BAO: khong thay 'PROG' nao trong {0} ms dau => firmware CU (PROG moi 64 KB)." -f $ProgProbeMs) -ForegroundColor Yellow
                    Write-Host ("          Chi con lop NHIP DEU {0} KB/s (khong co phan hoi tien do tu panel)." -f $PaceKBps) -ForegroundColor Yellow
                    Write-Host "          Neu van mat du lieu: giam -PaceKBps (vi du 5) hoac -ChunkKB 1." -ForegroundColor Yellow
                    Write-Host "          Cach chua that: firmware panel gui 'PROG' moi 1-4 KB (README 9.3)." -ForegroundColor Yellow
                }
            }
        }

        $pct = [int](100 * $script:Sent / $size)
        if ($swRate.Elapsed.TotalMilliseconds -ge 500) {
            $kbsNow = ($script:Sent / 1KB) / $swTotal.Elapsed.TotalSeconds
            $swRate.Restart()
        }
        if ($pct -ne $lastPct) {
            $mode = if ($progMode -eq $true) { 'PROG' } elseif ($progMode -eq $false) { 'nhip' } else { 'do...' }
            Write-Host ("`r  gui {0,3}%  {1,7:N0}/{2:N0} KB  prog={3,7:N0} B  {4,6:N1} KB/s  [{5}]   " -f `
                $pct, ($script:Sent / 1KB), ($size / 1KB), $script:LastProg, $kbsNow, $mode) -NoNewline
            $lastPct = $pct
        }
        if ($swTotal.Elapsed.TotalSeconds -gt $TimeoutSec) {
            Write-Host "`nHET THOI GIAN gui du lieu ($TimeoutSec s)." -ForegroundColor Red
            break
        }
    }
} finally {
    $fs.Close()
    if ($lastPct -ge 0) { Write-Host "" }
}
$sendSec = $swTotal.Elapsed.TotalSeconds
$avgKbs = if ($sendSec -gt 0) { ($script:Sent / 1KB) / $sendSec } else { 0 }
Write-Host ("Da gui {0:N0}/{1:N0} byte ({2:N1} KB) trong {3:N1} s  =>  trung binh {4:N1} KB/s" -f `
    $script:Sent, $size, ($script:Sent / 1KB), $sendSec, $avgKbs) -ForegroundColor Cyan
Write-Host ("Panel xac nhan (PROG cuoi): {0:N0} B [che do: {1}] | gioi han nhip: {2:N1} s | cho PROG: {3:N1} s" -f `
    $script:LastProg, $(if ($progMode -eq $true) { 'PROG' } elseif ($progMode -eq $false) { 'nhip deu (firmware cu)' } else { 'khong ro' }), $paceLimited, $waitedTotal)
if ($script:FlowError) {
    Write-Host ""
    Write-Host "DUNG som vi panel khong xac nhan tien do. File tam tren panel (neu co) se bi xoa." -ForegroundColor Red
    [BthSpp]::Cleanup()
    exit 4
}
if ($script:ErrFlag) {
    Write-Host ""
    Write-Host ("Panel bao loi trong luc gui: {0}" -f $script:ErrLine) -ForegroundColor Red
    foreach ($l in (Explain-Err $script:ErrLine)) { Write-Host $l -ForegroundColor Yellow }
    [BthSpp]::Cleanup()
    exit 2
}

# ===========================================================================
# 6) Cho ket qua: panel TAT BT de bat WiFi (socket dut la BINH THUONG)
#    => tu noi lai vai lan de nhan DONE/ERR da duoc panel luu lai.
# ===========================================================================
Write-Host ""
Write-Host "Cho panel POST len ESP3D + M22/M21 (se tu noi lai neu BT dut)..." -ForegroundColor Cyan
[void][BthSpp]::SetTimeout(8000, 250)

# Panel TAT BT o pha 2 => socket dut la BINH THUONG. Panel bat lai BT va GUI LAI ket qua da luu cho
# client MOI => phai tu noi lai. Noi lai DUNG KENH da noi duoc (thuong la 1) - KHONG quet lai tu dau.
$script:Phase = 'wait'
$waitDeadline = [DateTime]::Now.AddSeconds($ReconnectSec)
$nextTry = [DateTime]::Now.AddSeconds(3)
$connected = -not $script:SocketDead
$lastReconnectErr = ''
$reconnectChans = @($connectChan)
if ($connectChan -ne 1) { $reconnectChans += 1 }
while (-not $script:Done -and -not $script:ErrFlag -and [DateTime]::Now -lt $waitDeadline) {
    if ($connected -and -not $script:SocketDead) {
        Receive-Panel 500
        continue
    }
    if ($script:SocketDead -and $connected) {
        $connected = $false
        Write-Host "  (socket dut: panel TAT BT de bat WiFi - binh thuong; cho roi NOI LAI)" -ForegroundColor DarkYellow
    }
    if ([DateTime]::Now -lt $nextTry) { Start-Sleep -Milliseconds 200; continue }
    $nextTry = [DateTime]::Now.AddSeconds(3)

    $res = $null
    foreach ($ch in $reconnectChans) {
        $res = Open-Socket $ch $PrimaryTimeoutMs
        if ($res.ok) { break }
    }
    if (-not $res.ok) {
        if ($res.msg -ne $lastReconnectErr) {
            $lastReconnectErr = $res.msg
            Write-Host ("  noi lai: chua duoc ({0}) - thu tiep moi 3 s..." -f $res.msg) -ForegroundColor DarkGray
        }
        continue
    }
    $script:Reconnects++
    $script:SocketDead = $false
    $connected = $true
    $script:Phase = 'reconnect'
    Write-Host ("  DA NOI LAI lan {0} (kenh {1}) - cho panel gui ket qua da luu (DONE/ERR)..." -f $script:Reconnects, $res.chan) -ForegroundColor Green
    Receive-Panel 2500
    $script:Phase = 'wait'
}

# ===========================================================================
# 7) Ket luan + ma thoat
# ===========================================================================
Write-Host ""
Write-Host "---- Cac dong panel da tra loi ----" -ForegroundColor Cyan
if ($script:PanelLines.Count -eq 0) {
    Write-Host "  (khong nhan duoc dong nao)" -ForegroundColor Yellow
} else {
    foreach ($l in $script:PanelLines) { Write-Host ("  {0}" -f $l) }
}
Write-Host ("Thoi gian gui: {0:N1} s | trung binh {1:N1} KB/s | so lan noi lai: {2}" -f $sendSec, $avgKbs, $script:Reconnects)
[BthSpp]::Cleanup()

if ($script:Done) {
    Write-Host "XONG: panel da POST file len ESP3D va init lai the SD cua Marlin (M22/M21)." -ForegroundColor Green
    Write-Host "Mo man FILES tren panel (hoac bam REFRESH) de thay file moi." -ForegroundColor Green
    exit 0
}
if ($script:ErrFlag) {
    Write-Host "================================================================" -ForegroundColor Red
    Write-Host ("LOI TU PANEL: {0}" -f $script:ErrLine) -ForegroundColor Red
    foreach ($l in (Explain-Err $script:ErrLine)) { Write-Host $l -ForegroundColor Yellow }
    if ($script:Sent -le 0) {
        Write-Host "  (Panel tu choi NGAY sau dong header -> CHUA gui byte noi dung nao.)" -ForegroundColor Yellow
    } else {
        Write-Host ("  (Da gui {0:N0}/{1:N0} byte truoc khi panel bao loi; file tam tren panel da bi xoa.)" -f $script:Sent, $size) -ForegroundColor Yellow
    }
    Write-Host "================================================================" -ForegroundColor Red
    exit 2
}
Write-Host ("Het thoi gian cho ({0} s) ma chua thay DONE/ERR." -f $ReconnectSec) -ForegroundColor Yellow
Write-Host "Kiem tra log COM cua panel ([BTUP] ... POST/DONE) de biet dang o buoc nao." -ForegroundColor Yellow
exit 4
