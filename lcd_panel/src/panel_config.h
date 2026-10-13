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
  // ESP32: begin() can ca chan
  #define PANEL_LINK_BEGIN(baud) PANEL_MARLIN_SERIAL.begin(baud, SERIAL_8N1, PANEL_UART_RX, PANEL_UART_TX)
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
