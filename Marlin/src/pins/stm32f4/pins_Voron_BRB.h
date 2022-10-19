/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2022 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once
/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2022 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

// #include "variant_BLACKPILL_F401CC.h"
/**
 * Creality V24S1_301 (STM32F103RE / STM32F103RC) board pin assignments as found on Ender 3 S1.
 * Also supports the STM32F4 version of the board with identical pin mapping.
 */

#include "env_validate.h"


#ifndef BOARD_INFO_NAME
  #define BOARD_INFO_NAME      "Custom Voron 2.4"
#endif
#ifndef DEFAULT_MACHINE_NAME
  #define DEFAULT_MACHINE_NAME "Voron 2.4 v0.1"
#endif


#define TFT_SCK_PIN PA5
#define TFT_MOSI_PIN PA7
#define TFT_MISO_PIN PA6
#define TFT_CS_PIN PA4

#define TFT_A0_PIN PA3
#define LCD_RESET_PIN PA10

// #define TOUCH_CS_PIN PA2
// #define TOUCH_MOSI_PIN PA0
// #define TOUCH_SCK_PIN PA0
// #define TOUCH_MISO_PIN PA0

#define FORCE_SOFT_SPI
// reset A10


//
// Limit Switches
//
#define Z_STOP_PIN                          PA0

#ifndef Z_MIN_PROBE_PIN
  #define Z_MIN_PROBE_PIN                   PA0  // BLTouch IN
#endif

//
// Filament Runout Sensor
//
#ifndef FIL_RUNOUT_PIN
  #define FIL_RUNOUT_PIN                    PA0  // "Pulled-high"
#endif

//
// Heaters / Fans
//
#define HEATER_BED_PIN                      PA0   // HOT BED
#define FAN1_PIN                            PA0   // extruder fan

//
// SD Card
//
#define ONBOARD_SPI_DEVICE                     1
#define ONBOARD_SD_CS_PIN                   PA0   // SDSS

//#define BOARD_NO_NATIVE_USB



//
// EEPROM
//
#if NO_EEPROM_SELECTED
  #define IIC_BL24CXX_EEPROM                      // EEPROM on I2C-0
  //#define SDCARD_EEPROM_EMULATION
#endif

#if ENABLED(IIC_BL24CXX_EEPROM)
  #define IIC_EEPROM_SDA                    PA11
  #define IIC_EEPROM_SCL                    PA12
  #define MARLIN_EEPROM_SIZE               0x800  // 512K (24C512)
#elif ENABLED(SDCARD_EEPROM_EMULATION)
  #define MARLIN_EEPROM_SIZE               0x800  // 2K
#endif


//
// Limit Switches
//
#ifndef X_STOP_PIN
  #define X_STOP_PIN                        PA0
#endif
#ifndef Y_STOP_PIN
  #define Y_STOP_PIN                        PA0
#endif
#ifndef Z_STOP_PIN
  #define Z_STOP_PIN                        PA7
#endif

#ifndef Z_MIN_PROBE_PIN
  #define Z_MIN_PROBE_PIN                   PB1   // BLTouch IN
#endif

//
// Filament Runout Sensor
//
#ifndef FIL_RUNOUT_PIN
  #define FIL_RUNOUT_PIN                    PA4   // "Pulled-high"
#endif

//
// Steppers
//
#ifndef X_STEP_PIN
  #define X_STEP_PIN                        PA0
#endif
#ifndef X_DIR_PIN
  #define X_DIR_PIN                         PB0
#endif
#define X_ENABLE_PIN                        PA0   // Shared

#ifndef Y_STEP_PIN
  #define Y_STEP_PIN                        PB8
#endif
#ifndef Y_DIR_PIN
  #define Y_DIR_PIN                         PB0
#endif
#define Y_ENABLE_PIN                X_ENABLE_PIN

#ifndef Z_STEP_PIN
  #define Z_STEP_PIN                        PB0
#endif
#ifndef Z_DIR_PIN
  #define Z_DIR_PIN                         PB0
#endif
#define Z_ENABLE_PIN                X_ENABLE_PIN

#ifndef E0_STEP_PIN
  #define E0_STEP_PIN                       PB0
#endif
#ifndef E0_DIR_PIN
  #define E0_DIR_PIN                        PB0
#endif
#define E0_ENABLE_PIN               X_ENABLE_PIN


//
// Temperature Sensors
//
#define TEMP_0_PIN                          PA0   // TH1
#define TEMP_BED_PIN                        PA0   // TB1

//
// Heaters / Fans
//
#ifndef HEATER_0_PIN
  #define HEATER_0_PIN                      PA0   // HEATER1
#endif
#ifndef HEATER_BED_PIN
  #define HEATER_BED_PIN                    PA2   // HOT BED
#endif
#ifndef FAN_PIN
  #define FAN_PIN                           PA0   // FAN
#endif
#define FAN_SOFT_PWM_REQUIRED

// //
// // SD Card
// //
// #define SD_DETECT_PIN                       PC7
// #define SDCARD_CONNECTION ONBOARD
// #define SDIO_SUPPORT
// #define NO_SD_HOST_DRIVE                          // This board's SD is only seen by the printer


// #if ENABLED(CR10_STOCKDISPLAY)

//   #define LCD_PINS_RS                EXP3_07_PIN
//   #define LCD_PINS_ENABLE            EXP3_08_PIN
//   #define LCD_PINS_D4                EXP3_06_PIN

//   #define BTN_ENC                    EXP3_02_PIN
//   #define BTN_EN1                    EXP3_03_PIN
//   #define BTN_EN2                    EXP3_05_PIN

//   #ifndef HAS_PIN_27_BOARD
//     #define BEEPER_PIN               EXP3_01_PIN
//   #endif

// #elif ANY(HAS_DWIN_E3V2, IS_DWIN_MARLINUI, DWIN_VET6_CREALITY_LCD)

//   #define BTN_ENC                    EXP3_05_PIN
//   #define BTN_EN1                    EXP3_08_PIN
//   #define BTN_EN2                    EXP3_07_PIN

//   #ifndef BEEPER_PIN
//     #define BEEPER_PIN               EXP3_06_PIN
//   #endif

// #elif ENABLED(FYSETC_MINI_12864_2_1)

//   #ifndef NO_CONTROLLER_CUSTOM_WIRING_WARNING
//     #error "CAUTION! FYSETC_MINI_12864_2_1 and clones require wiring modifications. See 'pins_CREALITY_V4.h' for details. Define NO_CONTROLLER_CUSTOM_WIRING_WARNING to suppress this warning."
//   #endif

//   #if SD_CONNECTION_IS(LCD)
//     #error "The LCD SD Card is not connected with this configuration."
//   #endif

//   /**
//    *
//    *                 Board (RET6 12864 LCD)              Display
//    *                 ------                               ------
//    *  (EN1)    PC6  | 1  2 | PB2  (BTN_ENC)           5V |10  9 | GND
//    *  (LCD_CS) PB10 | 3  4 | PB11 (LCD RESET)         -- | 8  7 | --
//    *  (LCD_A0) PB14   5  6 | PB13 (EN2)           (DIN)  | 6  5   (LCD RESET)
//    *  (LCD_SCK)PB12 | 7  8 | PB15 (MOSI)        (LCD_A0) | 4  3 | (LCD_CS)
//    *            GND | 9 10 | 5V                (BTN_ENC) | 2  1 | --
//    *                 ------                               ------
//    *                  EXP1                                 EXP1
//    *
//    *                                                      ------
//    *                -----                             -- |10  9 | --
//    *                | 1 | VCC                    (RESET) | 8  7 | --
//    *                | 2 | PA13 (DIN)             (MOSI)  | 6  5   (EN2)
//    *                | 3 | PA14                        -- | 4  3 | (EN1)
//    *                | 4 | GND                   (LCD_SCK)| 2  1 | --
//    *                -----                                 ------
//    *              Debug port                               EXP2
//    *
//    * Needs custom cable. Connect EN2-EN2, LCD_CS-LCD_CS and so on.
//    * Debug port is just above EXP1. You need to add pins.
//    *
//    */

//   #define BTN_ENC                    EXP3_02_PIN
//   #define BTN_EN1                    EXP3_01_PIN
//   #define BTN_EN2                    EXP3_06_PIN
//   #define BEEPER_PIN                        -1

//   #define DOGLCD_CS                  EXP3_03_PIN
//   #define DOGLCD_A0                  EXP3_05_PIN
//   #define DOGLCD_SCK                 EXP3_07_PIN
//   #define DOGLCD_MOSI                EXP3_08_PIN
//   #define LCD_RESET_PIN              EXP3_04_PIN

//   #define FORCE_SOFT_SPI
//   #define LCD_BACKLIGHT_PIN                 -1
//   #define NEOPIXEL_PIN                      PA13

// #endif
