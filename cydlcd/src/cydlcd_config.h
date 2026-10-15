/**
 * cydlcd -- one place for pins, board choice and options.
 *
 * The MAN HINH CYD is a small MCU (STM32F411CCU6 or STM32F103C6T6) that drives a 2.4" ILI9341
 * SPI TFT. It talks to Marlin over ONE UART with plain G-code (see marlin_link.cpp), so
 * Marlin does not render anything and needs no code changes -- only a spare serial port.
 */
#pragma once

// ---------------------------------------------------------------------------
// Board: pick one in cydlcd/platformio.ini (-D flags)
//   CYDLCD_BOARD_F411    STM32F411CCU6  256K/128K  - UI tu ve (khong LVGL)
//   CYDLCD_BOARD_F103C6  STM32F103C6T6   32K/ 10K  - UI tu ve (khong LVGL)
//   CYDLCD_BOARD_F407    STM32F407VGT6    1M/128K  - UI LVGL
//   CYDLCD_BOARD_CYD     ESP32-2432S028R "CYD" (2.8" ST7789 240x320 + XPT2046) - UI LVGL
// ---------------------------------------------------------------------------
#if !defined(CYDLCD_BOARD_F411) && !defined(CYDLCD_BOARD_F103C6) && !defined(CYDLCD_BOARD_F407) && !defined(CYDLCD_BOARD_CYD)
  #define CYDLCD_BOARD_F411        // default: STM32F411CCU6
#endif

#if defined(CYDLCD_BOARD_CYD)
  #define CYDLCD_NAME "ESP32-CYD"
#elif defined(CYDLCD_BOARD_F407)
  #define CYDLCD_NAME "F407VGT6"
#elif defined(CYDLCD_BOARD_F103C6)
  #define CYDLCD_NAME "F103C6"
#else
  #define CYDLCD_NAME "F411CC"
#endif

// Ten MAY IN hien o man INFO. O che do PUSH, ten that lay tu goi HELLO cua Marlin
// (cydlcd.name). O che do TRAM (CYDLCD_TBT_LINK) KHONG co goi HELLO nen dung ten nay
// -> doi cho khop may cua ban, hoac -DCYDLCD_MACHINE_NAME=\"...\" trong platformio.ini.
#ifndef CYDLCD_MACHINE_NAME
  #define CYDLCD_MACHINE_NAME "TBT"
#endif

// UI LVGL: mac dinh BAT cho board du man (F407/CYD), TAT cho F411/F103C6 (UI tu ve).
// Co the ep bang -DCYDLCD_USE_LVGL=0/1 trong platformio.ini.
#ifndef CYDLCD_USE_LVGL
  #if defined(CYDLCD_BOARD_F407) || defined(CYDLCD_BOARD_CYD)
    #define CYDLCD_USE_LVGL 1
  #else
    #define CYDLCD_USE_LVGL 0
  #endif
#endif

// ---------------------------------------------------------------------------
// UART to the mainboard (Marlin). 250000 = the usual "TFT/host" baud.
//
// STM32: `Serial` = USART2 (PA2 = TX, PA3 = RX) qua build flag
//        (-DSERIAL_UART_INSTANCE=2 -DPIN_SERIAL_RX=PA3 -DPIN_SERIAL_TX=PA2).
// ESP32 CYD: `Serial2`, chan do CYDLCD_UART_* quyet dinh (mac dinh GPIO22 = TX,
//        GPIO35 = RX; GPIO35 chi doc duoc nen hop voi RX, noi vao TX cua Monster8).
// ---------------------------------------------------------------------------
#define CYDLCD_MARLIN_BAUD     250000

#if defined(CYDLCD_BOARD_CYD)
  #define CYDLCD_MARLIN_SERIAL Serial2
  #ifndef CYDLCD_UART_TX
    #define CYDLCD_UART_TX 22
  #endif
  #ifndef CYDLCD_UART_RX
    #define CYDLCD_UART_RX 35
  #endif
  // ESP32: begin() can ca chan.
  // PHai goi setRxBufferSize TRUOC begin(). Mac dinh cua core Arduino-ESP32 la 256 byte, nhung
  // Marlin co the day mot DOT nhieu frame lien tiep (danh sach file SD, 33 dong setting) va LVGL
  // co the giu vong lap vai chuc ms khi ve man -> 256 byte chi du ~10 ms o 250000 baud
  // (1 byte = 40 us). 1024 byte = ~41 ms do, du an toan. Xem them README muc "buffer serial".
  #ifndef CYDLCD_RX_BUFFER
    #define CYDLCD_RX_BUFFER 1024
  #endif
  #define CYDLCD_LINK_BEGIN(baud) (CYDLCD_MARLIN_SERIAL.setRxBufferSize(CYDLCD_RX_BUFFER), CYDLCD_MARLIN_SERIAL.begin(baud, SERIAL_8N1, CYDLCD_UART_RX, CYDLCD_UART_TX))
#else
  #define CYDLCD_MARLIN_SERIAL Serial
  #define CYDLCD_LINK_BEGIN(baud) CYDLCD_MARLIN_SERIAL.begin(baud)
#endif

// ---------------------------------------------------------------------------
// Optional debug console (USB CDC on the F411: -DENABLE_USB_CDC -DCYDLCD_DEBUG).
// Prints every line sent to / received from Marlin -- very handy while wiring up.
// ---------------------------------------------------------------------------
//#define CYDLCD_DEBUG
#ifdef CYDLCD_DEBUG
  #define CYDLCD_DEBUG_SERIAL SerialUSB
  #define DBG_BEGIN()        CYDLCD_DEBUG_SERIAL.begin(115200)
  #define DBG(...)           CYDLCD_DEBUG_SERIAL.print(__VA_ARGS__)
  #define DBGLN(...)         CYDLCD_DEBUG_SERIAL.println(__VA_ARGS__)
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
// Rotary encoder with push button (the whole input of this MAN HINH CYD)
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
//#define CYDLCD_TOUCH
#ifdef CYDLCD_TOUCH
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
#define CYDLCD_CYD_TOUCH_SWAP_XY   0
#define CYDLCD_CYD_TOUCH_INVERT_X  0
#define CYDLCD_CYD_TOUCH_INVERT_Y  0
#define CYDLCD_CYD_TOUCH_XMIN      184    // raw_x tai mep TRAI (X man hinh = 0)
#define CYDLCD_CYD_TOUCH_XMAX      3866   // raw_x tai mep PHAI (X man hinh = 319)
#define CYDLCD_CYD_TOUCH_YMIN      185    // raw_y tai mep TREN (Y man hinh = 0)
#define CYDLCD_CYD_TOUCH_YMAX      3910   // raw_y tai mep DUOI (Y man hinh = 239)

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------
#define POLL_TEMP_MS    500          // how often M105 is sent
#define POLL_POS_MS     700          // how often M114 is sent
#define POLL_PROGRESS_MS 2000        // how often M27 is sent while printing
#define LINK_TIMEOUT_MS 4000         // no "ok" for this long -> show LINK LOST
// UI_REFRESH_MS = nhip ve lai cac gia tri co the doi (man nao cung chi ve khi chu THAT SU doi ->
// `set_text()` so sanh truoc, xem ui_widgets.cpp).
// 2026-10-15: thu 150 -> 90 de man ENDSTOP doc 10 lan/giay, nhung sau do man ENDSTOP co DUONG RIENG
// (30 lan/giay, xem `page_endstop_poll()` trong ui_page_endstop.cpp + ui_lvgl.cpp) nen tra lai 150 cho
// cac man khac nhu cu (do lai: man ENDSTOP van 30 lan/giay, cac man khac khong bi ve lai nhieu hon).
#define UI_REFRESH_MS   150          // redraw period for changed values

// ===========================================================================
// QUAN LY WiFi (net_wifi.cpp) -- xem README 9.20
//   CYD: mac dinh 1 (co module WiFi). Board khac: 0 => net_wifi.cpp chi con stub
//   (moi ham tra 0/rong/false) nen nhanh UI van build binh thuong.
// ===========================================================================
#ifndef CYDLCD_WIFI_MGMT
  #if defined(CYDLCD_BOARD_CYD)
    #define CYDLCD_WIFI_MGMT 1
  #else
    #define CYDLCD_WIFI_MGMT 0
  #endif
#endif

#if CYDLCD_WIFI_MGMT
// Tu ket noi lai luc BOOT neu da co cau hinh da luu trong NVS:
//   1 = man hinh tu vao mang de icon WiFi xanh + trang WiFi settings dung duoc ngay.
//   0 = khong tu ket noi (phai bam KET NOI tren trang WiFi).
#ifndef CYDLCD_WIFI_AUTOSTART
  #define CYDLCD_WIFI_AUTOSTART 1
#endif

// ---------------------------------------------------------------------------
// net_wifi: NVS + kich thuoc dem + thoi gian (tat ca chi co tac dung khi CYDLCD_WIFI_MGMT=1)
// ---------------------------------------------------------------------------
#ifndef CYDLCD_WIFI_NVS_NS
  #define CYDLCD_WIFI_NVS_NS   "wifi"    // namespace Preferences (NVS) - rieng, khong dung cua WiFi core
#endif
#ifndef CYDLCD_WIFI_SSID_MAX
  #define CYDLCD_WIFI_SSID_MAX 32        // chuan 802.11: SSID toi da 32 ky tu (+1 cho '\0')
#endif
#ifndef CYDLCD_WIFI_PASS_MAX
  #define CYDLCD_WIFI_PASS_MAX 64        // WPA2 8..63 ky tu (+1); de 64 de chua ca PSK dang hex
#endif
#ifndef CYDLCD_WIFI_SCAN_MAX
  #define CYDLCD_WIFI_SCAN_MAX 16        // so mang LUU LAI sau khi quet (sap xep RSSI manh truoc)
#endif
#ifndef CYDLCD_WIFI_RETRY_MS
  #define CYDLCD_WIFI_RETRY_MS 8000u     // thu ket noi lai moi 8 s khi dang mat (khong chan loop)
#endif
#ifndef CYDLCD_WIFI_MIN_HEAP
  #define CYDLCD_WIFI_MIN_HEAP  (72u * 1024u)    // heap toi thieu TRUOC khi bat WiFi STA
#endif
#ifndef CYDLCD_WIFI_MIN_BLOCK
  #define CYDLCD_WIFI_MIN_BLOCK (32u * 1024u)    // khoi LIEN TUC lon nhat toi thieu cho lwip/WiFi
#endif

// ---------------------------------------------------------------------------
// GIA TRI MAC DINH GOI Y (chi dung khi NVS con TRONG): mang nha cua nguoi dung.
//   Nguoi dung doi duoc tu trang WiFi settings (net_wifi_connect(..., save=true) -> ghi NVS);
//   khi do gia tri o day KHONG con duoc dung nua.
// ---------------------------------------------------------------------------
#ifndef CYDLCD_WIFI_SSID_DEF
  #define CYDLCD_WIFI_SSID_DEF "TP-Link_31A0"    // mang nha 2.4 GHz (ESP32 khong co 5 GHz)
#endif
#ifndef CYDLCD_WIFI_PASS_DEF
  #define CYDLCD_WIFI_PASS_DEF "89278062"
#endif

// ---------------------------------------------------------------------------
// UPLOAD QUA WiFi: MAN HINH CYD TU LAM WEB SERVER (net_upload_http.cpp) -- xem README 9.21 + muc 11
//   PC/dien thoai -> WiFi -> http://<ip-man-hinh>/ -> POST /upload -> MAN HINH CYD ghi /cydlcd_tmp/<ten>.tmp
//   Sau do MAN HINH CYD lam 1 trong 2 viec (tuy co CYDLCD_SD_SHARED, xem khoi ngay duoi):
//     - CYDLCD_SD_SHARED 1 (MAC DINH, 2026-10-14): doi ten file tam -> /<ten> tren THE SD DUNG CHUNG
//       voi Marlin (mux 74HC4053) -> tra bus + M22/M21. khong can mang, khong can module ESP3D.
//     - CYDLCD_SD_SHARED 0: (kieu cu) POST sang module ESP3D -- DA XOA 2026-10-14.
//   MAC DINH: 1 voi CYD (khong con phu thuoc tinh nang upload nao khac):
//   Bluedroid ~100 KB + WiFi ~45 KB khong du cho trong heap). Muon dung WiFi -> tat BT.
// ---------------------------------------------------------------------------
#ifndef CYDLCD_WIFI_UPLOAD
  #if defined(CYDLCD_BOARD_CYD)
    #define CYDLCD_WIFI_UPLOAD 1
  #else
    #define CYDLCD_WIFI_UPLOAD 0
  #endif
#endif
#if CYDLCD_WIFI_UPLOAD && !CYDLCD_WIFI_MGMT
  #undef  CYDLCD_WIFI_MGMT
  #define CYDLCD_WIFI_MGMT 1      // web server can net_wifi (quan ly WiFi)
#endif

// ---------------------------------------------------------------------------
// THE SD DUNG CHUNG voi Marlin qua MUX SPI (74HC4053) -- [DUONG LUI] thay cho kieu cu: POST file sang module ESP3D (DA XOA 2026-10-14)
//
//   SO DO: the TF nam o khe CYD. Hai master (ESP32 cua MAN HINH CYD + STM32 cua Marlin) noi vao
//   MUX; chan chon (A/B/C cua 4053 noi chung) do MAN HINH CYD dieu khien bang CYDLCD_SD_MUX_PIN, va
//   DUOC KEO XUONG GND bang dien tro 10k => muc MAC DINH (MAN HINH CYD reset / mat dien / chan chon
//   bo troi) = MARLIN. Nho vay Marlin khong bao gio bi MAN HINH CYD chiem bus ngoai y muon.
//   - 4053 co 3 cong tac: SCK, MOSI, CS. MISO dung CHUNG truc tiep (chi the lai driver MISO).
//   - MAN HINH CYD CHI gianh bus khi: Marlin KHONG in, va chi trong luc nhan file ghi vao the.
//   - Sau khi tra bus: MAN HINH CYD gui M22 + M21 de Marlin mount lai the roi xin lai danh sach file.
//
//   CYDLCD_SD_SHARED 0 = kieu cu (POST file sang module ESP3D) -- DA XOA, khong con dung duoc.
//   CYDLCD_SD_MUX_PIN: chan GPIO dieu khien chan chon cua 4053 (A/B/C noi chung).
//     CYD: GPIO27 = chan TRONG o header CN1 (GND/IO22/IO27/3.3V) -> KHUYEN NGHI.
//     Cach khac: dung chinh chan DEN NEN (IO21, chan thu 4 cua P3 - xem canh bao ben duoi).
//     Cac chan da dung: 22/35 = UART Marlin, 18/19/23/5 = SPI the TF, 12/13/14/15/2 = LCD,
//     25/32/33/36/39 = cam ung, 4/16/17 = LED RGB, 34 = cam bien anh sang, 26 = loa.
// ---------------------------------------------------------------------------
// [2026-10-14 - DANG DUNG] THE SD NAM O KHE MAN HINH, MARLIN LAY SECTOR QUA UART
//   (quyet dinh cua nguoi dung: "implement kieu sdcard moi ma lay data usart")
//
//   Uu diem: KHONG can MUX SPI, khong cat duong, khong tranh chap bus, khong dien tro, MAN HINH CYD nap
//   lai firmware luc nao cung duoc. Marlin van chay dung ngan xep SD cua no (FAT, liet ke thu muc,
//   M23/M24, tien do in...) vi SdVolume chi goi readBlock()/writeBlock() -> xem
//   Marlin/src/sd/cydlcd_sd_uart.* va Marlin/Configuration_adv.h (CYDLCD_SD_OVER_UART, SDCARD_READONLY).
//
//   Nhuoc diem: moi thao tac the cua Marlin phu thuoc UART + MAN HINH CYD con song (MAN HINH CYD chet = Marlin
//   khong doc duoc file). Doi lai MAN HINH CYD LA NGUOI DUY NHAT giu the nen phan mem MAN HINH CYD don gian hon:
//   mount 1 lan luc boot, giu luon, chi tra loi sector.
//
//   CYDLCD_SD_UART 1  -> MAN HINH CYD lam "may chu the SD" (proto >= 3), va CYDLCD_SD_SHARED bi TAT.
//   CYDLCD_SD_UART 0  -> quay lai che do cu (MUX 4053; kieu POST sang ESP3D da xoa).
// ---------------------------------------------------------------------------
#if defined(CYDLCD_BOARD_CYD) && !defined(CYDLCD_SD_UART)
  #define CYDLCD_SD_UART 1
#endif
#if CYDLCD_SD_UART && defined(CYDLCD_SD_SHARED) && CYDLCD_SD_SHARED
  #undef  CYDLCD_SD_SHARED
  #define CYDLCD_SD_SHARED 0        // 2 che do loai tru nhau
#endif

// ---------------------------------------------------------------------------
// THE SD DUNG CHUNG voi Marlin qua MUX SPI (74HC4053) -- [DUONG LUI], hien KHONG dung nua
// vi da chon CYDLCD_SD_UART (the thuoc han man hinh). Thay cho kieu cu: POST file sang module
// ESP3D -- kieu do DA XOA 2026-10-14 (cung voi duong upload qua Bluetooth).
//
//   SO DO: the TF nam o khe CYD. Hai master (ESP32 cua MAN HINH CYD + STM32 cua Marlin) noi vao
//   MUX; chan chon (A/B/C cua 4053 noi chung) do MAN HINH CYD dieu khien bang CYDLCD_SD_MUX_PIN, va
//   DUOC KEO XUONG GND bang dien tro 10k => muc MAC DINH (MAN HINH CYD reset / mat dien / chan chon
//   bo troi) = MARLIN. Nho vay Marlin khong bao gio bi MAN HINH CYD chiem bus ngoai y muon.
//   - 4053 co 3 cong tac: SCK, MOSI, CS. MISO dung CHUNG truc tiep (chi the lai driver MISO).
//   - MAN HINH CYD CHI gianh bus khi: Marlin KHONG in, va chi trong luc nhan file ghi vao the.
//   - Sau khi tra bus: MAN HINH CYD gui M22 + M21 de Marlin mount lai the roi xin lai danh sach file.
//
//   CYDLCD_SD_SHARED 0 = kieu cu (POST file sang module ESP3D) -- DA XOA, khong con dung duoc.
//   CYDLCD_SD_MUX_PIN: chan GPIO dieu khien chan chon cua 4053 (A/B/C noi chung).
//     CYD: GPIO27 = chan TRONG o header CN1 (GND/IO22/IO27/3.3V) -> KHUYEN NGHI.
//     Cach khac: dung chinh chan DEN NEN (IO21, chan thu 4 cua P3 - xem canh bao ben duoi).
//     Cac chan da dung: 22/35 = UART Marlin, 18/19/23/5 = SPI the TF, 12/13/14/15/2 = LCD,
//     25/32/33/36/39 = cam ung, 4/16/17 = LED RGB, 34 = cam bien anh sang, 26 = loa.
// ---------------------------------------------------------------------------
#if defined(CYDLCD_BOARD_CYD) && !defined(CYDLCD_SD_SHARED)
  #define CYDLCD_SD_SHARED 0
#endif
#if CYDLCD_SD_SHARED
  #ifndef CYDLCD_SD_MUX_PIN
    #define CYDLCD_SD_MUX_PIN 27
  #endif
  // Muc logic cua chan chon: HIGH = the thuoc MAN HINH CYD (ESP32), LOW = the thuoc MARLIN.
  #ifndef CYDLCD_SD_MUX_CYDLCD_LEVEL
    #define CYDLCD_SD_MUX_CYDLCD_LEVEL HIGH
  #endif
  #ifndef CYDLCD_SD_MUX_MARLIN_LEVEL
    #define CYDLCD_SD_MUX_MARLIN_LEVEL LOW
  #endif
  // Thoi gian cho MUX + the on dinh sau khi doi master (4053 tron trong vai chuc ns; 3 ms la
  // qua du cho ca truong hop the dang o trang thai ngu).
  #ifndef CYDLCD_SD_MUX_SETTLE_MS
    #define CYDLCD_SD_MUX_SETTLE_MS 3u
  #endif
#endif

// ---------------------------------------------------------------------------
// DEN NEN man hinh CYD: FIRMWARE dieu khien truc tiep
//   tools/tft_setup_cyd/User_Setup.h da dat `TFT_BL -1` (TFT_eSPI khong keo chan nay nua) de:
//     (1) board CYD co 2 ban: den nen o GPIO21 (ban ST7789 pho bien) hoac GPIO27 (ban khac);
//     (2) chan den nen la ung vien cho CHAN CHON MUX SPI the SD (xem khoi ngay tren).
//   CYDLCD_BL_PIN -1 = den nen DAU THANG 3V3 (luon sang) => chan do coi nhu TRONG, dung lam chan
//   chon MUX duoc (khi do phai them -DCYDLCD_BL_PIN=-1 vao build_flags).
// ---------------------------------------------------------------------------
#if defined(CYDLCD_BOARD_CYD) && !defined(CYDLCD_BL_PIN)
  #define CYDLCD_BL_PIN 21
#endif
#ifndef CYDLCD_BL_ON_LEVEL
  #define CYDLCD_BL_ON_LEVEL HIGH
#endif
#if defined(CYDLCD_SD_SHARED) && CYDLCD_SD_SHARED && defined(CYDLCD_BL_PIN) && (CYDLCD_BL_PIN >= 0) && \
    defined(CYDLCD_SD_MUX_PIN) && (CYDLCD_SD_MUX_PIN == CYDLCD_BL_PIN)
  #error "CYDLCD_SD_MUX_PIN trung CYDLCD_BL_PIN: chan chon MUX doi muc luc chay => man hinh se TAT/NHAY moi lan cydlcd tra bus cho Marlin. Hay dau den nen thang 3V3 roi them -DCYDLCD_BL_PIN=-1 vao build_flags (xem cydlcd/README.md muc 11.3)."
#endif

#if CYDLCD_WIFI_UPLOAD
#ifndef CYDLCD_HTTP_PORT
  #define CYDLCD_HTTP_PORT 80                // cong web server tren man hinh
#endif
// Token bao mat: phai co trong query ?token=... hoac header X-CYDLCD-Token.
// De "" = KHONG kiem (khong khuyen nghi).
#ifndef CYDLCD_HTTP_TOKEN
  #define CYDLCD_HTTP_TOKEN "TBT-3D-up"
#endif
// Ten mDNS cua MAN HINH CYD: PC/ dien thoai vao http://TBTCYDLCD.local/ (khong can biet IP).
// ESPmDNS ton ~2 KB DRAM + ~10 KB flash. Dat CYDLCD_HTTP_USE_MDNS 0 de TAT.
#ifndef CYDLCD_HTTP_USE_MDNS
  #define CYDLCD_HTTP_USE_MDNS 1
#endif
#ifndef CYDLCD_HTTP_MDNS_NAME
  #define CYDLCD_HTTP_MDNS_NAME "TBTCYDLCD"
#endif
// Nhip theo doi the TF nong (cyd_sd.cpp dung).
#ifndef CYDLCD_SD_CHECK_MS
  #define CYDLCD_SD_CHECK_MS 1000u    // nhip kiem the con do (chi khi dang ranh)
#endif
#ifndef CYDLCD_SD_RETRY_MS
  #define CYDLCD_SD_RETRY_MS 2500u    // nhip thu mount lai khi dang mat the
#endif
#ifndef CYDLCD_SD_CLEANUP_MS
  // Nhip DON FILE TAM dinh ky (yeu cau nguoi dung 2026-10-15): 30 phut/lan, xoa file .tmp trong
  // /cydlcd_tmp cu hon 30 phut (rac cua lan gui bi loi/ngat/reboot). Chi chay khi dang ranh (khong co
  // phien upload). Xem cyd_sd_task()/clean_dir().
  #define CYDLCD_SD_CLEANUP_MS (30u * 60u * 1000u)
#endif
#ifndef CYDLCD_WIFI_REFRESH_MS
  // Nhip doc day du trang thai WiFi (WiFi.status/localIP/RSSI - RSSI phai hoi driver, ~300 us/lan).
  // VI SAO khong doc moi vong lap: do bang `UI PERF` 2026-10-15 -> `net_wifi_task()` an ~300 us MOI vong
  // (~40% CPU khi vong lap chay ~300 lan/s) trong khi icon WiFi + chu dBm chi cap nhat 400 ms/lan.
  #define CYDLCD_WIFI_REFRESH_MS 500u
#endif
#ifndef CYDLCD_SD_FREE_MS
  // Nhip doc DUNG LUONG CON TRONG cua the (`SD.totalBytes()/usedBytes()` = FATFS `f_getfree`, quet bang FAT:
  // do duoc ~20 ms moi lan tren the 16 GB) - chi de IN RA LOG 1 Hz, nen khong can doc moi giay.
  #define CYDLCD_SD_FREE_MS 10000u
#endif
#ifndef CYDLCD_HTTP_POLL_MS
  // Nhip goi `server.handleClient()`: ~50 us MOI lan goi (hoi lwIP xem co ket noi moi) nen goi moi vong lap
  // (5000 lan/s) la 27% CPU vo ich. Trang web chi vai request/giay; phien upload khong bi anh huong vi
  // WebServer xu ly CA body trong 1 lan goi.
  #define CYDLCD_HTTP_POLL_MS 10u
#endif
// ---------------------------------------------------------------------------
// BAO MAT TEN FILE (yeu cau nguoi dung 2026-10-15: "ten file tren server can fix bao mat, chi cho phep
// a-z0-9._"): whitelist ky tu cho ten file gui qua WEB (upload + xoa).
//   Co ban LUON cho phep: a-z, A-Z, 0-9, '.', '_'
//     - giu A-Z vi the dang co ten chu HOA (Body11.gcode, SoftSPI.txt, V300_bot.gcode...);
//   `CYDLCD_NAME_ALLOW_EXTRA` = ky tu THEM ngoai co ban:
//     - mac dinh "-" vi cac file dang co tren the dung dau gach (V300_case-esp-2432s208_no-text.gcode);
//     - MUON CHAT DUNG nhu yeu cau (chi a-z0-9._) thi doi thanh "" (mot dong duy nhat o day).
// Ngoai whitelist, ten con bi chan: rong, > 64 ky tu, bat dau bang '.', chua "..", va bi bo het duong dan.
// LUU Y: day la lop chan o BIEN GIOI WEB. Duong xoa cua man FILES tren LCD (ten lay tu danh sach cua
// Marlin) khong bi gioi han nay, neu khong se khong xoa duoc cac file ten la nhu "V300_[a]_xy_...gc".
#ifndef CYDLCD_NAME_ALLOW_EXTRA
  #define CYDLCD_NAME_ALLOW_EXTRA "-"
#endif
// ---------------------------------------------------------------------------
// THONG BAO tren TRANG /status (yeu cau nguoi dung 2026-10-15): hop thoai "thong bao" cua Marlin (co nut OK
// + tu dong an tren LCD) KHONG hien tren man hinh nua, chi hien o trang /status va luu lai message LAN CUOI;
// qua `CYDLCD_MSG_TTL_MS` thi tu bien mat (dat 0 = giu mai).
//   CYDLCD_MSG_BOX_ON_LCD 0 = khong hien hop thong bao tren LCD (mac dinh, theo yeu cau);
//                         1 = quay lai hanh vi cu (hien hop OK + tu an sau CYDLCD_MSG_TTL_MS tren LCD).
// ---------------------------------------------------------------------------
#ifndef CYDLCD_MSG_BOX_ON_LCD
  #define CYDLCD_MSG_BOX_ON_LCD 0
#endif
#ifndef CYDLCD_MSG_TTL_MS
  #define CYDLCD_MSG_TTL_MS 6000u        // thoi gian tu an cua thong bao o trang /status (0 = giu mai)
#endif
#ifndef CYDLCD_NOTIFY_LEN
  #define CYDLCD_NOTIFY_LEN 96           // do dai toi da cua 1 thong bao (byte)
#endif
// Cho phep nhan file khi Marlin dang in? 0 = tu choi (an toan: 2 nguoi ghi 1 FAT).
#ifndef CYDLCD_HTTP_ALLOW_WHILE_PRINTING
  #define CYDLCD_HTTP_ALLOW_WHILE_PRINTING 0
#endif
// Tran kich thuoc 1 file nhan qua HTTP (byte) - chan nguoi dung gui file qua lon (day the TF).
#ifndef CYDLCD_HTTP_MAX_BYTES
  #define CYDLCD_HTTP_MAX_BYTES (256u * 1024u * 1024u)   // 256 MB
#endif
// Cho Marlin xu ly M22/M21 truoc khi xin lai danh sach file (cyd_sd.cpp dung).
#ifndef CYDLCD_HTTP_SDWAIT_MS
  #define CYDLCD_HTTP_SDWAIT_MS     800u
#endif

// ---------------------------------------------------------------------------
// The TF cua CYD -- VSPI, BUS RIENG VOI LCD (LCD dung HSPI):
//   tools/tft_setup_cyd/User_Setup.h:56-57 "CYD: LCD o HSPI (VSPI de danh cho khe the SD:
//   CS 5 / MOSI 23 / MISO 19 / SCLK 18)" + "#define USE_HSPI_PORT".
//   Cam ung XPT2046 la chan rieng (25/32/39/33/36) -> khong dung bus nay.
//   THE PHAI LA FAT32 (SD cua ESP32/Arduino core 2.0.17 khong mount exFAT).
// ---------------------------------------------------------------------------
#define CYDLCD_SD_SCK   18
#define CYDLCD_SD_MISO  19
#define CYDLCD_SD_MOSI  23
#define CYDLCD_SD_CS     5
#define CYDLCD_SD_HZ     20000000     // 20 MHz = MAX (theo yeu cau nguoi dung 2026-10-15: "spi clock de max
                                      // di"): ghi 4,5 MB chi mat ~5-8 s thay vi ~30 s o 4 MHz -> CUA SO
                                      // RUI RO ngan hon nhieu (loi the giua phien ~1/2 lan o 4 MHz).
                                      // `sd_mount()` van tu ha 20 -> 10 -> 4 MHz neu the khong chiu duoc.
                                      // (loi that 2026-10-15: `SD.write()` tra ve thieu sau ~1,7-2 MB, moi
                                      //  lan ghi 1436 B mat 50-170 ms; cung the do CO lan ghi tron 4,3 MB
                                      //  => the/khe the bi margin, khong hong han). 4 MHz = cham hon
                                      //  (~50-90 KB/s) nhung doc sector cho Marlin van du
                                      //  (512 B @4 MHz ~1 ms, van nho hon chi phi khung UART).
                                      // Muon thu lai toc do cao: doi so nay roi chay lai upload file lon.
#define CYDLCD_UPLOAD_TMP_DIR "/cydlcd_tmp"       // thu muc file tam (Marlin chi liet ke goc the -> khong thay)

// Cac THU MUC HE THONG khong hien tren man FILE SD (danh sach TEN, ngan cach bang dau phay; so sanh
// khong phan biet hoa/thuong). Loc NGAY O PHIA MAN HINH (cydlcd_link.cpp: is_hidden_dir) nen khong
// phai nap lai Marlin:
//   cydlcd_tmp = thu muc file tam cua CHINH MAN HINH (cyd_sd.cpp tu tao khi nhan file qua WiFi);
//   BTUP       = thu muc cu cua duong upload qua BLUETOOTH (da xoa khoi firmware 2026-10-14).
// Trong 2 thu muc nay khong co gi de in/xoa -> an di cho danh sach gon.
#ifndef CYDLCD_HIDE_DIRS
  #define CYDLCD_HIDE_DIRS "cydlcd_tmp,BTUP"
#endif
#endif  // CYDLCD_WIFI_UPLOAD
#endif  // CYDLCD_WIFI_MGMT
