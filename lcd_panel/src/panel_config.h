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
#define TFT_ROTATION    0            // 0/2 = portrait 240x320, 1/3 = landscape 320x240
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
// Behaviour
// ---------------------------------------------------------------------------
#define POLL_TEMP_MS    500          // how often M105 is sent
#define POLL_POS_MS     700          // how often M114 is sent
#define POLL_PROGRESS_MS 2000        // how often M27 is sent while printing
#define LINK_TIMEOUT_MS 4000         // no "ok" for this long -> show LINK LOST
#define UI_REFRESH_MS   150          // redraw period for changed values
