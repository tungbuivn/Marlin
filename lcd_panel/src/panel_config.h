/**
 * lcd_panel -- one place for pins, board choice and options.
 *
 * The panel is a small MCU (STM32F411CCU6 or STM32F103C6T6) that drives a 2.4" ILI9341
 * SPI TFT. It talks to Marlin over ONE UART with plain G-code (see marlin_link.cpp), so
 * Marlin does not render anything and needs no code changes -- only a spare serial port.
 */
#pragma once

// ---------------------------------------------------------------------------
// Board: pick one in lcd_panel/platformio.ini (-D flags)
//   PANEL_BOARD_F411    STM32F411CCU6  256K/128K  - UI tu ve (khong LVGL)
//   PANEL_BOARD_F103C6  STM32F103C6T6   32K/ 10K  - UI tu ve (khong LVGL)
//   PANEL_BOARD_F407    STM32F407VGT6    1M/128K  - UI LVGL
//   PANEL_BOARD_CYD     ESP32-2432S028R (2.8" ILI9341 + XPT2046) - UI LVGL
// ---------------------------------------------------------------------------
#if !defined(PANEL_BOARD_F411) && !defined(PANEL_BOARD_F103C6) && !defined(PANEL_BOARD_F407) && !defined(PANEL_BOARD_CYD)
  #define PANEL_BOARD_F411        // default: STM32F411CCU6
#endif

#if defined(PANEL_BOARD_CYD)
  #define PANEL_NAME "ESP32-CYD"
#elif defined(PANEL_BOARD_F407)
  #define PANEL_NAME "F407VGT6"
#elif defined(PANEL_BOARD_F103C6)
  #define PANEL_NAME "F103C6"
#else
  #define PANEL_NAME "F411CC"
#endif

// Ten MAY IN hien o man INFO. O che do PUSH, ten that lay tu goi HELLO cua Marlin
// (panel.name). O che do TRAM (PANEL_TRAM_LINK) KHONG co goi HELLO nen dung ten nay
// -> doi cho khop may cua ban, hoac -DPANEL_MACHINE_NAME=\"...\" trong platformio.ini.
#ifndef PANEL_MACHINE_NAME
  #define PANEL_MACHINE_NAME "TBT"
#endif

// UI LVGL: mac dinh BAT cho board du man (F407/CYD), TAT cho F411/F103C6 (UI tu ve).
// Co the ep bang -DPANEL_USE_LVGL=0/1 trong platformio.ini.
#ifndef PANEL_USE_LVGL
  #if defined(PANEL_BOARD_F407) || defined(PANEL_BOARD_CYD)
    #define PANEL_USE_LVGL 1
  #else
    #define PANEL_USE_LVGL 0
  #endif
#endif

// ---------------------------------------------------------------------------
// UART to the mainboard (Marlin). 250000 = the usual "TFT/host" baud.
//
// STM32: `Serial` = USART2 (PA2 = TX, PA3 = RX) qua build flag
//        (-DSERIAL_UART_INSTANCE=2 -DPIN_SERIAL_RX=PA3 -DPIN_SERIAL_TX=PA2).
// ESP32 CYD: `Serial2`, chan do PANEL_UART_* quyet dinh (mac dinh GPIO22 = TX,
//        GPIO35 = RX; GPIO35 chi doc duoc nen hop voi RX, noi vao TX cua Monster8).
// ---------------------------------------------------------------------------
#define PANEL_MARLIN_BAUD     250000

#if defined(PANEL_BOARD_CYD)
  #define PANEL_MARLIN_SERIAL Serial2
  #ifndef PANEL_UART_TX
    #define PANEL_UART_TX 22
  #endif
  #ifndef PANEL_UART_RX
    #define PANEL_UART_RX 35
  #endif
  // ESP32: begin() can ca chan.
  // PHai goi setRxBufferSize TRUOC begin(). Mac dinh cua core Arduino-ESP32 la 256 byte, nhung
  // Marlin co the day mot DOT nhieu frame lien tiep (danh sach file SD, 33 dong setting) va LVGL
  // co the giu vong lap vai chuc ms khi ve man -> 256 byte chi du ~10 ms o 250000 baud
  // (1 byte = 40 us). 1024 byte = ~41 ms do, du an toan. Xem them README muc "buffer serial".
  #ifndef PANEL_RX_BUFFER
    #define PANEL_RX_BUFFER 1024
  #endif
  #define PANEL_LINK_BEGIN(baud) (PANEL_MARLIN_SERIAL.setRxBufferSize(PANEL_RX_BUFFER), PANEL_MARLIN_SERIAL.begin(baud, SERIAL_8N1, PANEL_UART_RX, PANEL_UART_TX))
#else
  #define PANEL_MARLIN_SERIAL Serial
  #define PANEL_LINK_BEGIN(baud) PANEL_MARLIN_SERIAL.begin(baud)
#endif

// ---------------------------------------------------------------------------
// Optional debug console (USB CDC on the F411: -DENABLE_USB_CDC -DPANEL_DEBUG).
// Prints every line sent to / received from Marlin -- very handy while wiring up.
// ---------------------------------------------------------------------------
//#define PANEL_DEBUG
#ifdef PANEL_DEBUG
  #define PANEL_DEBUG_SERIAL SerialUSB
  #define DBG_BEGIN()        PANEL_DEBUG_SERIAL.begin(115200)
  #define DBG(...)           PANEL_DEBUG_SERIAL.print(__VA_ARGS__)
  #define DBGLN(...)         PANEL_DEBUG_SERIAL.println(__VA_ARGS__)
#else
  #define DBG_BEGIN()
  #define DBG(...)
  #define DBGLN(...)
#endif

// ---------------------------------------------------------------------------
// 2.4" ILI9341 SPI TFT  (240x320)
// ---------------------------------------------------------------------------
// 0/2 = portrait 240x320, 1/3 = landscape 320x240.
// UI LVGL (ui_lvgl) chay landscape -> env LVGL truyen -DTFT_ROTATION=1.
// UI tu ve (ui.cpp, env G-code) van thiet ke portrait -> mac dinh 0.
#ifndef TFT_ROTATION
  #define TFT_ROTATION  0
#endif
#define TFT_SPI_SPEED   24000000     // ILI9341 is happy at 24 MHz over short wires
#define TFT_PIN_SCK     PA5
#define TFT_PIN_MISO    PA6          // ILI9341 never sends data; kept free for touch
#define TFT_PIN_MOSI    PA7
#define TFT_PIN_CS      PA4
#define TFT_PIN_DC      PB0
#define TFT_PIN_RST     PB1
#define TFT_PIN_BL      PB10         // backlight; set to -1 if wired straight to 3V3

// ---------------------------------------------------------------------------
// Rotary encoder with push button (the whole input of this panel)
// ---------------------------------------------------------------------------
#define ENC_PIN_A       PB12
#define ENC_PIN_B       PB13
#define ENC_PIN_SW      PB14
#define ENC_REVERSE     1            // flip if turning feels backwards
#define ENC_PULSES_PER_DETENT 4      // quadrature edges per click of a detented encoder
#define ENC_MAX_DETENTS 3            // most detents the UI accepts per frame (fast spin)

// ---------------------------------------------------------------------------
// Optional XPT2046 touch (most 2.4" modules have it). Not used by the UI yet.
// ---------------------------------------------------------------------------
//#define PANEL_TOUCH
#ifdef PANEL_TOUCH
  #define TOUCH_PIN_CS  PB15
#endif

// ---------------------------------------------------------------------------
// CYD: hieu chuan cam ung XPT2046 (man NGANG 320x240)
//
// GIA TRI DUOI DAY la ket qua hieu chuan THUC TE da chay tot tren board, lay tu
// trinh hieu chuan 4 goc trong firmware (menu "Calibrate touch" hoac giu tay luc cap
// dien) -- xem ui_port_touch_calibrate() trong ui_lvgl_port.cpp:
//     cach do: cham -> CA MAN HINH XANH -> keo sat vao goc den khi MAT cam ung;
//     toa do goc = cuc tri (min/max) ghi duoc trong luc keo.
//     [TOUCHCAL] nap tu NVS: swap=0 invX=0 invY=0 X=184..3866 Y=185..3910
//
// Y NGHIA: raw_x la truc NGANG (trai 184 -> phai 3866), raw_y la truc DOC (tren 185 ->
// duoi 3910), ca hai deu TANG theo huong phai/xuong => khong phai dao truc nao.
// (Cach cham 4 goc bang mat truoc day chi ra 332..3645 / 453..3658: hut vao ~150-250
//  don vi o moi mep, chinh vi vay truoc day bi lech o giua man hinh. Cach keo den luc
//  mat cam ung moi lay dung mep vat ly.)
//
// Cach dat ten cua 4 macro (quan trong, de khong nham nua):
//   SWAP_XY = 0 -> X man hinh <- raw_x (dung XMIN/XMAX + INVERT_X)
//                  Y man hinh <- raw_y (dung YMIN/YMAX + INVERT_Y)
//   SWAP_XY = 1 -> X man hinh <- raw_y (dung YMIN/YMAX + INVERT_X)
//                  Y man hinh <- raw_x (dung XMIN/XMAX + INVERT_Y)
//   INVERT_* = 1 khi canh "0" (trai/tren) co gia tri raw LON HON canh doi dien.
//
// LUU Y: day chi la MAC DINH khi NVS (Preferences) chua co gi. Sau khi nguoi dung bam
// "Calibrate touch", gia tri trong NVS se duoc dung thay the (khong can build lai).
// ---------------------------------------------------------------------------
#define PANEL_CYD_TOUCH_SWAP_XY   0
#define PANEL_CYD_TOUCH_INVERT_X  0
#define PANEL_CYD_TOUCH_INVERT_Y  0
#define PANEL_CYD_TOUCH_XMIN      184    // raw_x tai mep TRAI (X man hinh = 0)
#define PANEL_CYD_TOUCH_XMAX      3866   // raw_x tai mep PHAI (X man hinh = 319)
#define PANEL_CYD_TOUCH_YMIN      185    // raw_y tai mep TREN (Y man hinh = 0)
#define PANEL_CYD_TOUCH_YMAX      3910   // raw_y tai mep DUOI (Y man hinh = 239)

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------
#define POLL_TEMP_MS    500          // how often M105 is sent
#define POLL_POS_MS     700          // how often M114 is sent
#define POLL_PROGRESS_MS 2000        // how often M27 is sent while printing
#define LINK_TIMEOUT_MS 4000         // no "ok" for this long -> show LINK LOST
#define UI_REFRESH_MS   150          // redraw period for changed values

// ===========================================================================
// NHAN FILE G-CODE QUA BLUETOOTH (BT SPP) -> the TF cua panel -> POST len ESP3D
// (xem panel_bt_upload.cpp / README muc "BT upload"; CHI co tac dung voi board CYD)
//
// LUONG: dien thoai gui file qua BT -> panel ghi file TAM tren the TF cua chinh panel
// (/btup/<ten>.tmp, khong giu file trong RAM) -> POST multipart/form-data len ESP3D
// (module WiFi cam tren board; no ghi THANG vao the SD cua Marlin qua bus SPI dung chung)
// -> gui M22+M21 cho Marlin de no init lai the SD -> xoa file tam -> bao DONE qua BT.
//
// TAT TINH NANG: -DPANEL_BT_UPLOAD=0 trong platformio.ini (env panel_esp32_cyd)
//                hoac doi 1 -> 0 ngay duoi day.
// ===========================================================================
#if defined(PANEL_BOARD_CYD) && !defined(PANEL_BT_UPLOAD)
  // TAM TAT (theo yeu cau nguoi dung): upload qua WiFi/ESP3D nhanh hon. Bat lai = doi 0 -> 1.
  #define PANEL_BT_UPLOAD 0
#endif

// ===========================================================================
// QUAN LY WiFi DOC LAP (net_wifi.cpp) -- xem README 9.20
//   CYD: mac dinh 1. Khi PANEL_BT_UPLOAD=1 thi BAT BUOC 1 (module BT upload dung CHUNG
//   net_wifi, khong tu goi WiFi.begin/mode/disconnect nua -> khong co 2 noi cung dieu khien WiFi).
//   Board khac: 0 => net_wifi.cpp chi con stub (moi ham tra 0/rong/false) nen nhanh UI van build.
// ===========================================================================
#ifndef PANEL_WIFI_MGMT
  #if defined(PANEL_BOARD_CYD)
    #define PANEL_WIFI_MGMT 1
  #else
    #define PANEL_WIFI_MGMT 0
  #endif
#endif
#if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD && !PANEL_WIFI_MGMT
  #undef  PANEL_WIFI_MGMT
  #define PANEL_WIFI_MGMT 1      // module BT upload PHAI co net_wifi (khong the thieu)
#endif

#if PANEL_WIFI_MGMT
// Tu ket noi lai luc BOOT neu da co cau hinh da luu trong NVS:
//   1 = WiFi chay MOT MINH (PANEL_BT_UPLOAD=0): panel tu vao mang de icon xanh + trang settings.
//   0 = dang bat BT upload: WiFi chi duoc bat o PHA 2 (BT va WiFi KHONG the cung luc - README 9.11).
#ifndef PANEL_WIFI_AUTOSTART
  #if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD
    #define PANEL_WIFI_AUTOSTART 0
  #else
    #define PANEL_WIFI_AUTOSTART 1
  #endif
#endif

// ---------------------------------------------------------------------------
// WiFi + module ESP3D   (TRUOC DAY nam trong khoi PANEL_BT_UPLOAD - da chuyen ra day de
// net_wifi.cpp dung duoc ca khi BT TAT; khoi BT van dung chung cac macro nay.)
//   PANEL_ESP3D_SSID/PASS: AP cua chinh module ESP3D. DANG DUNG (mac dinh trong
//        D:\arduino\ESP3D-direct-sd\esp3d\config.h:370-371):
//            "Voron2308" / "v0r0n24s3cur3"
//        (Neu sau nay doi qua web UI ESP3D thi sua 2 dong duoi. Bo gia tri khac tung gap:
//         AP "ESP32-Access-Point"/"123456789" cua Arduino-ESP32, mang nha "Voron20623"/"v0r0n24s3cur3".)
//        Day chi la gia tri MAC DINH: cau hinh nguoi dung luu trong NVS (Preferences, namespace
//        "wifi") se duoc uu tien (xem net_wifi.cpp + PANEL_WIFI_NVS_NS).
//   PANEL_ESP3D_HOST     : 192.168.0.1 = AP IP STATIC cua module (config.h:376 DEFAULT_IP_VALUE
//                          {192,168,0,1} + DEFAULT_AP_IP_MODE = STATIC_IP_MODE -> config.cpp:954 ghi
//                          vao EP_AP_IP_VALUE), nen IP nay CO DINH. Neu noi duoc host nay thi dung
//                          luon, KHONG do; chi do khi no khong tra loi. De "" neu muon tu do tu dau.
//   !!! CANH BAO TRUNG DAI IP: 192.168.0.0/24 TRUNG voi mang nha cua nguoi dung (router TP-Link
//       cung thuong la 192.168.0.1). Panel chi noi AP cua module nen binh thuong khong sao, nhung
//       neu sau nay panel noi mang nha ma van POST toi 192.168.0.1 thi se gui nham vao ROUTER.
//       Vi vay truoc khi POST LUON phai kiem tra "day co PHAI ESP3D khong" bang cach gui lenh
//       [ESP800] va doi phan hoi chua "FW version:" (KHONG duoc chi kiem "port 80 mo").
//   PANEL_ESP3D_PATH     : endpoint upload cua ESP3D-direct-sd (esp3d/webinterface.cpp:386)
//   PANEL_ESP3D_DIR      : ESP3D-direct-sd LUON ghi vao GOC the SD khi la admin
//                          (AUTHENTICATION_FEATURE tat -> is_authenticated() = LEVEL_ADMIN);
//                          gia tri nay chi de ghi tai lieu/doi chieu.
// ---------------------------------------------------------------------------
#ifndef PANEL_ESP3D_SSID
  #define PANEL_ESP3D_SSID "Voron2308"     // AP cua module ESP3D (config.h:370)
#endif
#ifndef PANEL_ESP3D_PASS
  #define PANEL_ESP3D_PASS "v0r0n24s3cur3" // (config.h:371)
#endif
#ifndef PANEL_ESP3D_HOST
  #define PANEL_ESP3D_HOST "192.168.0.1"   // AP IP static cua module (config.h:376); "" = tu do
#endif
#ifndef PANEL_ESP3D_PORT
  #define PANEL_ESP3D_PORT 80
#endif
#ifndef PANEL_ESP3D_PATH
  #define PANEL_ESP3D_PATH "/files"
#endif
#ifndef PANEL_ESP3D_DIR
  #define PANEL_ESP3D_DIR  "/"
#endif

// ---------------------------------------------------------------------------
// net_wifi: NVS + kich thuoc dem + thoi gian (tat ca chi co tac dung khi PANEL_WIFI_MGMT=1)
// ---------------------------------------------------------------------------
#ifndef PANEL_WIFI_NVS_NS
  #define PANEL_WIFI_NVS_NS   "wifi"    // namespace Preferences (NVS) - rieng, khong dung cua WiFi core
#endif
#ifndef PANEL_WIFI_SSID_MAX
  #define PANEL_WIFI_SSID_MAX 32        // chuan 802.11: SSID toi da 32 ky tu (+1 cho '\0')
#endif
#ifndef PANEL_WIFI_PASS_MAX
  #define PANEL_WIFI_PASS_MAX 64        // WPA2 8..63 ky tu (+1); de 64 de chua ca PSK dang hex
#endif
#ifndef PANEL_WIFI_SCAN_MAX
  #define PANEL_WIFI_SCAN_MAX 16        // so mang LUU LAI sau khi quet (sap xep RSSI manh truoc)
#endif
#ifndef PANEL_WIFI_RETRY_MS
  #define PANEL_WIFI_RETRY_MS 8000u     // thu ket noi lai moi 8 s khi dang mat (khong chan loop)
#endif
#ifndef PANEL_WIFI_MIN_HEAP
  #define PANEL_WIFI_MIN_HEAP  (72u * 1024u)    // heap toi thieu TRUOC khi bat WiFi STA
#endif
#ifndef PANEL_WIFI_MIN_BLOCK
  #define PANEL_WIFI_MIN_BLOCK (32u * 1024u)    // khoi LIEN TUC lon nhat toi thieu cho lwip/WiFi
#endif

// ---------------------------------------------------------------------------
// GIA TRI MAC DINH GOI Y (chi dung khi NVS con TRONG): mang nha cua nguoi dung.
//   Nguoi dung doi duoc tu trang WiFi settings (net_wifi_connect(..., save=true) -> ghi NVS);
//   khi do gia tri o day KHONG con duoc dung nua.
// ---------------------------------------------------------------------------
#ifndef PANEL_WIFI_SSID_DEF
  #define PANEL_WIFI_SSID_DEF "TP-Link_31A0"    // mang nha 2.4 GHz (ESP32 khong co 5 GHz)
#endif
#ifndef PANEL_WIFI_PASS_DEF
  #define PANEL_WIFI_PASS_DEF "89278062"
#endif

// ---------------------------------------------------------------------------
// UPLOAD QUA WiFi: PANEL TU LAM WEB SERVER (net_upload_http.cpp) -- xem README 9.21
//   PC/dien thoai -> WiFi -> http://<panel>/ -> POST /upload -> panel ghi /btup/<ten>.tmp
//   -> panel TU DAY sang ESP3D (dung lai dung pha 2 cua duong BT) -> M22/M21 -> xoa file tam.
//   MAC DINH: 1 voi CYD, nhung TU DONG = 0 khi PANEL_BT_UPLOAD=1 (BT va WiFi KHONG the cung luc:
//   Bluedroid ~100 KB + WiFi ~45 KB khong du cho trong heap). Muon dung WiFi -> tat BT.
// ---------------------------------------------------------------------------
#ifndef PANEL_WIFI_UPLOAD
  #if defined(PANEL_BOARD_CYD)
    #define PANEL_WIFI_UPLOAD 1
  #else
    #define PANEL_WIFI_UPLOAD 0
  #endif
#endif
#if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD && PANEL_WIFI_UPLOAD
  #undef  PANEL_WIFI_UPLOAD
  #define PANEL_WIFI_UPLOAD 0    // BT dang bat => khong the chay web server WiFi cung luc
#endif
#if PANEL_WIFI_UPLOAD && !PANEL_WIFI_MGMT
  #undef  PANEL_WIFI_MGMT
  #define PANEL_WIFI_MGMT 1      // web server can net_wifi (quan ly WiFi)
#endif

#if PANEL_WIFI_UPLOAD
#ifndef PANEL_HTTP_PORT
  #define PANEL_HTTP_PORT 80                // cong web server tren panel
#endif
// Token bao mat (giong duong BT): phai co trong query ?token=... hoac header X-Panel-Token.
// De "" = KHONG kiem (khong khuyen nghi). Mac dinh dung lai token cua duong BT.
#ifndef PANEL_HTTP_TOKEN
  #define PANEL_HTTP_TOKEN "TBT-3D-up"
#endif
// Ten mDNS cua panel: PC/ dien thoai vao http://TBTPANEL.local/ (khong can biet IP).
// ESPmDNS ton ~2 KB DRAM + ~10 KB flash. Dat PANEL_HTTP_USE_MDNS 0 de TAT.
#ifndef PANEL_HTTP_USE_MDNS
  #define PANEL_HTTP_USE_MDNS 1
#endif
#ifndef PANEL_HTTP_MDNS_NAME
  #define PANEL_HTTP_MDNS_NAME "TBTPANEL"
#endif
// Cac macro dung chung voi duong BT (BT block ben duoi cung khai bao qua #ifndef nen khong trung):
//   danh sach IP ung vien ESP3D + nhip theo doi the TF nong (net_esp3d.cpp dung).
#ifndef PANEL_ESP3D_IP_LIST
  #define PANEL_ESP3D_IP_LIST "192.168.4.1,192.168.0.1,192.168.1.1,192.168.1.50,192.168.0.50"
#endif
#ifndef PANEL_SD_CHECK_MS
  #define PANEL_SD_CHECK_MS 1000u    // nhip kiem the con do (chi khi dang ranh)
#endif
#ifndef PANEL_SD_RETRY_MS
  #define PANEL_SD_RETRY_MS 2500u    // nhip thu mount lai khi dang mat the
#endif
// ESP3D o dau so voi panel:
//   0 = ESP3D chay AP+STA (AP "Voron2308" van phat + join them mang nha) => panel POST toi IP LAN
//       cua ESP3D, KHONG phai doi mang. (KHUYEN NGHI - xem README 9.21)
//   1 = ESP3D CHI phat AP: panel phai TAM doi sang AP ESP3D de POST roi quay ve mang nha.
#ifndef PANEL_ESP3D_VIA_AP
  #define PANEL_ESP3D_VIA_AP 0
#endif
// Che do 1: AP cua ESP3D (panel join tam) - dung lai SSID/PASS cua module ESP3D o tren.
#ifndef PANEL_ESP3D_AP_SSID
  #define PANEL_ESP3D_AP_SSID PANEL_ESP3D_SSID
#endif
#ifndef PANEL_ESP3D_AP_PASS
  #define PANEL_ESP3D_AP_PASS PANEL_ESP3D_PASS
#endif
// Che do 0: IP cua ESP3D trong mang nha. "" = tu do (mDNS/gateway/danh sach - nhu duong BT).
#ifndef PANEL_ESP3D_LAN_HOST
  #define PANEL_ESP3D_LAN_HOST ""           // vd "192.168.1.50"; "" = tu do
#endif
// Cho phep nhan file khi Marlin dang in? 0 = tu choi (an toan: ESP3D gianh bus SPI the SD).
#ifndef PANEL_HTTP_ALLOW_WHILE_PRINTING
  #define PANEL_HTTP_ALLOW_WHILE_PRINTING 0
#endif
// Tran kich thuoc 1 file nhan qua HTTP (byte) - chan nguoi dung gui file qua lon (day the TF).
#ifndef PANEL_HTTP_MAX_BYTES
  #define PANEL_HTTP_MAX_BYTES (256u * 1024u * 1024u)   // 256 MB
#endif
// ---- Thoi gian / gioi han cua net_esp3d (day file len ESP3D tu web server) ----
#ifndef PANEL_HTTP_WIFI_WAIT_MS
  #define PANEL_HTTP_WIFI_WAIT_MS 15000u    // cho WiFi ket noi truoc khi POST (che do STA)
#endif
#ifndef PANEL_HTTP_AP_WAIT_MS
  #define PANEL_HTTP_AP_WAIT_MS   12000u    // cho khi doi sang AP cua ESP3D (PANEL_ESP3D_VIA_AP=1)
#endif
#ifndef PANEL_HTTP_PROBE_MS
  #define PANEL_HTTP_PROBE_MS     1500u     // timeout cho moi ung vien IP ESP3D ([ESP800])
#endif
#ifndef PANEL_HTTP_HTTP_TIMEOUT_MS
  #define PANEL_HTTP_HTTP_TIMEOUT_MS 8000u  // timeout moi lan doc/ghi socket
#endif
#ifndef PANEL_HTTP_POST_TIMEOUT_MS
  #define PANEL_HTTP_POST_TIMEOUT_MS 300000u // tran ca phien POST (5 phut)
#endif
#ifndef PANEL_HTTP_POST_RETRY
  #define PANEL_HTTP_POST_RETRY    1        // so lan POST LAI khi loi ket noi
#endif
#ifndef PANEL_HTTP_POST_RETRY_MS
  #define PANEL_HTTP_POST_RETRY_MS 2000u    // cho giua 2 lan POST lai
#endif
#ifndef PANEL_HTTP_REPLY_MAX
  #define PANEL_HTTP_REPLY_MAX (24u * 1024u) // tran doc phan hoi ESP3D
#endif
#ifndef PANEL_HTTP_SDWAIT_MS
  #define PANEL_HTTP_SDWAIT_MS     800u     // cho Marlin xu ly M22/M21 truoc khi xin lai danh sach file
#endif
#ifndef PANEL_HTTP_HEAP_MIN
  #define PANEL_HTTP_HEAP_MIN      (20u * 1024u)  // heap duoi muc nay -> tu choi phien upload moi
#endif

// ---------------------------------------------------------------------------
// The TF cua CYD -- VSPI, BUS RIENG VOI LCD (LCD dung HSPI):
//   tools/tft_setup/User_Setup.h:56-57 "CYD: LCD o HSPI (VSPI de danh cho khe the SD:
//   CS 5 / MOSI 23 / MISO 19 / SCLK 18)" + "#define USE_HSPI_PORT".
//   Cam ung XPT2046 la chan rieng (25/32/39/33/36) -> khong dung bus nay.
//   THE PHAI LA FAT32 (SD cua ESP32/Arduino core 2.0.17 khong mount exFAT).
// ---------------------------------------------------------------------------
#define PANEL_SD_SCK   18
#define PANEL_SD_MISO  19
#define PANEL_SD_MOSI  23
#define PANEL_SD_CS     5
#define PANEL_SD_HZ    20000000      // 20 MHz: an toan cho khe TF cua CYD
#define PANEL_BTUP_DIR "/btup"       // thu muc file tam (Marlin chi liet ke goc the -> khong thay)
#endif  // PANEL_WIFI_UPLOAD
#endif  // PANEL_WIFI_MGMT

#if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD

// ---------------------------------------------------------------------------
// Bluetooth SPP
// ---------------------------------------------------------------------------
#ifndef PANEL_BT_NAME
  #define PANEL_BT_NAME "TBT-PANEL"  // ten thiet bi thay khi dien thoai quet Bluetooth
#endif
// Ma PIN khi dien thoai doi ma luc pair (4..16 ky tu; Bluedroid esp_bt_pin_code_t cho toi 16 byte).
// Dat bang SerialBT.setPin() TRUOC SerialBT.begin() (core doc _pin_len/_pin_code trong begin() -
// BluetoothSerial.cpp:686-688). ⚠️ NEU dien thoai pair bang SSP thi DIEN THOAI KHONG HOI PIN (core
// dat SSP + IO cap) => PIN chi la LOP PHU; lop chinh la TOKEN ben duoi.
#ifndef PANEL_BT_PIN
  #define PANEL_BT_PIN "74196328"
#endif
// ===========================================================================
// BAO MAT - tranh nguoi khac gui file khong mong muon (xem README 9.13)
//   Lop 1 (chinh): TOKEN bat buoc trong dong header. Phien chi duoc chap nhan khi dong DAU la
//       `PUT <token> <ten_file> <so_byte>`; sai/thieu token => ERR sai ma token, KHONG nhan byte
//       nao, khong tao file tam. De "" neu muon TAT lop nay (khong khuyen nghi).
//   Lop 2: PIN khi pair (tren) - chi co tac dung voi thiet bi legacy (khong SSP).
//   Lop 3: sau lan client ket noi thanh cong dau tien => NON-DISCOVERABLE (dien thoai la khong
//       quet thay nua); chi DISCOVERABLE lai khi nguoi dung CHU DONG bat che do nhan file.
//   Lop 4: mac dinh KHONG tu bat che do nhan file (PANEL_BT_UPLOAD_AUTOSTART 0) - UI phai goi
//       panel_bt_upload_set_enabled(true), va tu tat sau PANEL_BT_ENABLE_MIN phut khong co phien.
// ===========================================================================
#ifndef PANEL_BT_TOKEN
  #define PANEL_BT_TOKEN "TBT-3D-up"     // doi tuy y (KHONG de trong neu muon co bao mat)
#endif
// Kieu gui THO (khong header) KHONG the mang token => mac dinh TAT. Bat len la BO lop token.
#ifndef PANEL_BT_ALLOW_RAW
  #define PANEL_BT_ALLOW_RAW 0
#endif
// 1 = TU DONG san sang nhan file tu luc khoi dong (mac dinh - nguoi dung khong phai bam gi; TOKEN
// moi la cong chan that). 0 = phai bat bang panel_bt_upload_set_enabled(true) (nut UI/console).
#ifndef PANEL_BT_UPLOAD_AUTOSTART
  #define PANEL_BT_UPLOAD_AUTOSTART 1
#endif
// Ghi chu: KHONG con cua so "tu tat sau N phut" va KHONG con "an thiet bi sau khi pair" - thiet bi
// LUON DISCOVERABLE de bat ky dien thoai nao cung pair duoc bat cu luc nao (yeu cau nguoi dung);
// bao mat nam o TOKEN + RAW tat + chi nhan khi may khong in.
// SSP "Just Works": core 2.0.17 BAT SSP mac dinh (_enableSSP = true, BluetoothSerial.cpp:896) va dat
// IO capability = ESP_BT_IO_CAP_IO (BluetoothSerial.cpp:694) => nhieu dien thoai hien hop thoai
// doi/so sanh ma 6 so (chinh la "doi ma" nguoi dung gap). Dat = 1 de ghi de IO cap thanh
// ESP_BT_IO_CAP_NONE cho pair KHONG can ma; PIN o tren van duoc dat de dung cho thiet bi legacy.
// LUU Y: Just Works = khong xac thuc nguoi dung => lop bao mat that su la TOKEN.
#ifndef PANEL_BT_JUST_WORKS
  #define PANEL_BT_JUST_WORKS 1
#endif

// ---------------------------------------------------------------------------
// WiFi + module ESP3D: DA CHUYEN len khoi `#if PANEL_WIFI_MGMT` (dau file) de net_wifi.cpp
// dung duoc ca khi PANEL_BT_UPLOAD=0. Khoi BT nay dung chung cac macro do
// (PANEL_ESP3D_SSID/PASS/HOST/PORT/PATH/DIR, PANEL_WIFI_MIN_HEAP/MIN_BLOCK).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// TU DO IP MODULE ESP3D (chi chay khi bat dau PHA 2, ket qua duoc CACHE lai)
//   Thu tu: [PANEL_ESP3D_HOST neu dat] -> [mDNS neu bat] -> [gateway cua mang dang noi]
//           -> [danh sach PANEL_ESP3D_IP_LIST]
//   Nhan dang "dung la ESP3D": gui `GET /command?plain=[ESP800]` va tim chuoi "FW version:"
//   trong phan hoi (command.cpp:1916+ tra ve "FW version:<x> # FW target:..."). Cach nay
//   KHONG dung tay vao the SD cua Marlin (khac voi GET /files) va khong the nham voi router.
// ---------------------------------------------------------------------------
#ifndef PANEL_ESP3D_IP_LIST
  #define PANEL_ESP3D_IP_LIST "192.168.4.1,192.168.0.1,192.168.1.1,192.168.1.50,192.168.0.50"
#endif
#ifndef PANEL_BT_PROBE_MS
  #define PANEL_BT_PROBE_MS       1500      // timeout cho MOI ung vien IP
#endif
#ifndef PANEL_BT_PROBE_TOTAL_MS
  #define PANEL_BT_PROBE_TOTAL_MS 15000     // tran ca qua trinh do
#endif
// mDNS: MAC DINH TAT. Ly do: ten mDNS cua ESP3D KHONG co dinh - esp3d/config.h:34,37 dat
// ESP_DEFAULT_NAME "MYESP" va ESP_HOST_NAME = ESP_DEFAULT_NAME => mac dinh la "MYESP.local"
// (KHONG phai "esp3d.local"), va ten that con doi duoc trong EEPROM (EP_HOSTNAME, sua qua web UI).
// Them nua: ESPmDNS keo theo ~2 KB DRAM bien tinh, ma tran DRAM cua CYD chi con ~2,5 KB (README 9.8).
// Bat lai khi can: -DPANEL_ESP3D_USE_MDNS=1 (se thu lan luot cac ten trong PANEL_ESP3D_MDNS_NAMES).
#ifndef PANEL_ESP3D_USE_MDNS
  #define PANEL_ESP3D_USE_MDNS 0
#endif
#ifndef PANEL_ESP3D_MDNS_NAMES
  #define PANEL_ESP3D_MDNS_NAMES "MYESP,esp3d"
#endif

// ---------------------------------------------------------------------------
// The TF cua CYD -- VSPI, BUS RIENG VOI LCD (LCD dung HSPI):
//   tools/tft_setup/User_Setup.h:56-57 "CYD: LCD o HSPI (VSPI de danh cho khe the SD:
//   CS 5 / MOSI 23 / MISO 19 / SCLK 18)" + "#define USE_HSPI_PORT".
//   Cam ung XPT2046 la chan rieng (25/32/39/33/36) -> khong dung bus nay.
//   THE PHAI LA FAT32 (SD cua ESP32/Arduino core 2.0.17 khong mount exFAT).
// ---------------------------------------------------------------------------
#define PANEL_SD_SCK   18
#define PANEL_SD_MISO  19
#define PANEL_SD_MOSI  23
#define PANEL_SD_CS     5
#define PANEL_SD_HZ    20000000      // 20 MHz: an toan cho khe TF cua CYD
#define PANEL_BTUP_DIR "/btup"       // thu muc file tam (Marlin chi liet ke goc the -> khong thay)

// ---------------------------------------------------------------------------
// Kich thuoc / thoi gian
// ---------------------------------------------------------------------------
#define PANEL_BT_NAME_MAX       31                // do dai ten file toi da (nhan tu BT)
#define PANEL_BT_DEF_NAME       "bt_upload.gco"   // ten mac dinh khi gui kieu RAW (khong header)
// ---------------------------------------------------------------------------
// AN TOAN KHOI DONG - bai hoc tu PANIC THAT (2026-10-14, xem README 9.10):
//   "Guru Meditation ... StoreProhibited EXCVADDR=0x00000000" trong memset cua bta_sys_init:
//   Bluedroid cap phat dong (bta_sys_cb_ptr = osi_malloc) KHONG kiem tra NULL -> neu that bai thi
//   memset(NULL) => panic (khong phai loi tra ve). Vi vay:
//     - PHAI mount duoc the TF truoc (khong co the => TAT tinh nang, KHONG bat BT)
//     - kiem tra heap + KHOI LIEN TUC truoc khi goi SerialBT.begin() (Bluedroid can ~60-70 KB)
//     - KHONG bat WiFi luc boot: truoc day WiFi.begin() chay SONG SONG voi SerialBT.begin()
//       (2 stack radio khoi tao cung luc) va an ~40-50 KB heap ngay truoc khi BT init.
// ---------------------------------------------------------------------------
#ifndef PANEL_BT_MIN_HEAP
  #define PANEL_BT_MIN_HEAP    (110u * 1024u)   // heap toi thieu TRUOC khi SerialBT.begin()
#endif
#ifndef PANEL_BT_MIN_BLOCK
  #define PANEL_BT_MIN_BLOCK   (48u * 1024u)    // khoi LIEN TUC lon nhat toi thieu (chong phan manh)
#endif
// ---------------------------------------------------------------------------
// CHAY TUAN TU BT <-> WiFi (KHONG BAO GIO bat dong thoi) - do THAT tren may (2026-10-14):
//   Bluedroid Classic an ~99,5 KB (heap 124.324 -> 24.840 B ngay sau SerialBT.begin()).
//   Vi pha 1 (nhan BT) va pha 2 (POST WiFi) la NOI TIEP, het pha 1 thi goi SerialBT.end() de TRA
//   ~100 KB, roi moi bat WiFi de POST; xong thi tat WiFi va bat lai BT. SerialBT.end() THUC SU tra
//   bo nho: no goi esp_spp_deinit + esp_bluedroid_disable/deinit + btStop()
//   (BluetoothSerial.cpp:710-751; btStop -> esp_bt_controller_disable/deinit o esp32-hal-bt.c:62-88).
// ---------------------------------------------------------------------------
// PANEL_WIFI_MIN_HEAP / PANEL_WIFI_MIN_BLOCK: da chuyen len khoi `#if PANEL_WIFI_MGMT` (dau file)
// vi net_wifi.cpp (cua an toan RAM truoc khi bat WiFi) dung chung.
#ifndef PANEL_BT_END_MIN_GAIN
  #define PANEL_BT_END_MIN_GAIN (60u * 1024u)   // SerialBT.end() phai tra >= muc nay (chi de CANH BAO)
#endif
// ---------------------------------------------------------------------------
// HOAN INIT RA SAU KHI MAN HINH DA HIEN (nguoi dung phan nan boot lau) - xem README 9.17
//   panel_bt_upload_begin() chi DAT CO; viec that (mount the + bat BT, ~2-3 s) chay trong
//   panel_bt_upload_task() theo TUNG NHIP, co lop phu + khoa UI + dong tien do, va tu goi
//   lv_timer_handler() de ve duoc. Chia nhieu nhip de main loop khong bi doi qua lau
//   (panel coi mat PUSH sau PANEL_LINK_TIMEOUT_MS = 3 s).
// ---------------------------------------------------------------------------
#ifndef PANEL_BT_INIT_DELAY_MS
  #define PANEL_BT_INIT_DELAY_MS 200u           // cho UI on dinh roi moi bat dau init
#endif
#ifndef PANEL_BT_INIT_OK_MS
  #define PANEL_BT_INIT_OK_MS   1200u           // hien "SAN SANG" bao lau roi tu go lop phu
#endif
// The TF: theo doi nong (rut ra / cam vao) - xem README 9.12
#ifndef PANEL_SD_CHECK_MS
  #define PANEL_SD_CHECK_MS    1000u            // nhip kiem the con do (chi khi IDLE/NOTE)
#endif
#ifndef PANEL_SD_RETRY_MS
  #define PANEL_SD_RETRY_MS    2500u            // nhip thu mount lai khi dang mat the
#endif
// ---------------------------------------------------------------------------
// SPP SERVER / SDP RECORD (dien thoai-PC khong thay dich vu SPP - xem README 9.15):
//   Core Arduino dang ky SPP server trong callback ESP_SPP_INIT_EVT va **BO QUA ma tra ve**
//   (BluetoothSerial.cpp:271: `esp_spp_start_srv(...)` khong kiem tra) -> that bai la AM THAM.
//   Vi vay firmware: (1) log khi thay ESP_SPP_INIT_EVT (kem heap/khoi lien tuc) va CANH BAO neu
//   KHONG thay trong PANEL_BT_SPP_WAIT_MS; (2) neu PANEL_BT_SPP_RETRY=1 thi TU GOI LAI
//   esp_spp_start_srv() 1 lan va LOG ma tra ve.
//   LUU Y: esp_spp_start_srv() chi XEP HANG DOI (ESP_OK = da gui yeu cau, KHONG phai da dang ky).
//   Muon biet chac co record hay khong: (a) CORE_DEBUG_LEVEL >= 2 de thay log loi core/Bluedroid,
//   (b) thu tu Windows/dien thoai xem co thay dich vu SPP khong.
#ifndef PANEL_BT_SPP_RETRY
  #define PANEL_BT_SPP_RETRY 1                  // 1 = neu KHONG thay ESP_SPP_START_EVT thi TU DANG KY LAI
#endif
#ifndef PANEL_BT_SPP_WAIT_MS
  #define PANEL_BT_SPP_WAIT_MS 2000u            // cho ESP_SPP_INIT_EVT/START_EVT bao lau truoc khi canh bao
#endif
// Du phong cuoi (README 9.19): neu SPP server VAN khong dang ky duoc sau khi thu lai (dien thoai/PC
// se khong thay dich vu SPP => phai reset panel) thi TU KHOI DONG LAI panel de phien sau chac chan
// noi duoc. 1 = bat (tu restart, khong can nguoi dung bam reset); 0 = chi bao loi trong log.
// Mac dinh 0: chi bat khi do that thay ban va retry KHONG cuu duoc.
#ifndef PANEL_BT_SPP_FAIL_REBOOT
  #define PANEL_BT_SPP_FAIL_REBOOT 0
#endif
// Duong nhan du lieu SPP:
//   1 = custom_data_callback (BluetoothSerial::onData): core goi callback CUA TA truc tiep voi ca
//       goi (BluetoothSerial.cpp:337-338), KHONG qua queue 512 byte va khong day TUNG BYTE
//       (bo luon gioi han 6 KB/s + loi "RX Full! Discarding" - xem README 9.16). => NHANH.
//   0 = duong cu: task btup_rx hut queue bang read() (cham, chi de doi chieu khi can).
#ifndef PANEL_BT_USE_DATA_CB
  #define PANEL_BT_USE_DATA_CB 1
#endif
// WiFi: giu lai bao lau sau lan day cuoi roi tat (0 = tat ngay sau moi lan POST)
#ifndef PANEL_ESP3D_KEEP_MS
  #define PANEL_ESP3D_KEEP_MS  0u               // 0 = tat WiFi ngay sau khi POST xong
#endif
// 2 buffer lam vung dem lam viec lay tu HEAP (khong phai bien tinh - xem panel_bt_upload.cpp):
//   vong dem BT -> loop (PHAI la luy thua cua 2) + khoi ghi xuong the TF.
//   12 KB (thay vi 8 KB) tu khi bat PANEL_BT_USE_DATA_CB: duong callback day CA GOI vao vong dem
//   (khong con 512 byte cua core), nen vong dem phai du rong cho cua so dieu khien luong (script
//   Windows gui toi da WindowKB = 12 KB dang bay). Doi lai: BT-ready heap ~30 KB thay vi ~34 KB,
//   va 2 buffer nay duoc TRA LAI cho WiFi + cho lan SerialBT.begin() thu 2 (xem buf_release()).
//   KHOI GHI 4 KB (thay vi 2 KB): FATFS ton thoi gian cho MOI lan write() -> giam so lan goi.
#define PANEL_BT_RING_BYTES     12288             // vong dem BT -> loop (luy thua cua 2)
#define PANEL_BT_SD_CHUNK       4096              // khoi ghi xuong the TF (4 KB)
#define PANEL_BT_PROG_STEP      4096u             // bao tien do qua BT moi 4 KB (de DIEU KHIEN LUONG)
#define PANEL_BT_IDLE_MS        3000              // kieu RAW: het du lieu 3 s -> ket thuc
#define PANEL_BT_STALL_MS       8000              // kieu header: thieu du lieu 8 s -> loi
#define PANEL_BT_SNIFF_MS       1500              // cho dong header "PUT ..." toi da 1.5 s
#define PANEL_BT_DISCARD_MS     1000              // bo qua byte vun ngay sau phien header
#define PANEL_BT_SDWAIT_MS      800               // cho Marlin xu ly M22/M21 truoc khi xin lai file
#define PANEL_BT_NOTE_MS        6000              // hien DONE/ERR tren man bao lau
#define PANEL_BT_HTTP_TIMEOUT_MS 8000             // timeout moi lan doc/ghi socket
#define PANEL_BT_POST_TIMEOUT_MS 300000           // tran ca phien POST (5 phut)
#define PANEL_BT_POST_RETRY      1                // so lan POST LAI khi loi ket noi (0 = khong)
#define PANEL_BT_POST_RETRY_MS   2000             // cho giua 2 lan POST lai
#define PANEL_BT_WIFI_MS         15000            // cho WiFi ket noi toi da (moi lan POST)
#define PANEL_BT_WIFI_RETRY_MS    8000            // thu ket noi lai moi 8 s khi mat (khong chan loop)
#define PANEL_BT_MAX_REPLY_BYTES (24u * 1024u)    // tran doc phan hoi cua ESP3D
#define PANEL_BT_HEAP_WARN      (60u * 1024u)     // heap duoi muc nay -> CANH BAO trong log 1 Hz
                                                  // (do that: sau khi bat WiFi + BT Classic, heap
                                                  //  con lai rat it - xem README muc BT upload)
#define PANEL_BT_HEAP_MIN       (20u * 1024u)     // heap duoi muc nay -> tu choi phien moi (ERR)

#endif  // PANEL_BT_UPLOAD
