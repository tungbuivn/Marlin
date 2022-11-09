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










// #define PA8                     8       // yyyyyyyyyyy I2C3 SCL3 lcd_12864 | encoderB_LCD
// #define PA9                     9       // PIN_SERIAL_TX usb cable
// #define PA10                    10      // PIN_SERIAL_RX usb cable


// #define PA13                    13      // SWDCLK
// #define PA14                    14      // SWDIO





// #define PB4                     20      // yyyyyyyyyyy I2C3 SDA3 lcd_12864 | encoderA_LCD
// #define PB5                     21      // Z-DIR
// #define PB6                     22      // SCL1 eeprom
// #define PB7                     23      // SDA1 eeprom, PIN_WIRE_SDA







// #define PC13                    31      // xxxxxxxxxx lcd_spi_lcd_reset

// #define PC15                    33      // Encoder-click
// #define PH0                     34      // crysal
// #define PH1                     35      // crysal



// =============================================================================================
#ifdef USE_12864_LCD
#else

  #define TFT_SCK_PIN                       PIN_SPI_SCK
  #define TFT_MOSI_PIN                      PIN_SPI_MOSI
  #define TFT_MISO_PIN                      PIN_SPI_MISO
  #define TFT_CS_PIN                        PIN_SPI_SS
  #define TFT_A0_PIN                        PB15 
  #define LCD_RESET_PIN                     PC13
// #define FORCE_SOFT_SPI
#endif
// #define TOUCH_CS_PIN PA2
// #define TOUCH_MOSI_PIN PA0
// #define TOUCH_SCK_PIN PA0
// #define TOUCH_MISO_PIN PA0


#define XYZ_END_STOP                        PA11
#define XYZ_ENABLE                          PA12


//
// Limit Switches
//
#define Z_STOP_PIN                          XYZ_END_STOP
#define X_STOP_PIN                          XYZ_END_STOP
#define Y_STOP_PIN                          XYZ_END_STOP
// #define Z_MIN_PROBE_PIN                     XYZ_END_STOP


//
// Filament Runout Sensor
//
#define FIL_RUNOUT_PIN                      PC14  // "Pulled-high"



//
// Heaters / Fans
//

#define HEATER_0_PIN                        PB1   // HEATER1
#define FAN1_PIN                            PA1   // extruder fan


// 
// Bed
//  
#define HEATER_BED_PIN                    PA2   // HOT BED
// #define FAN_PIN                           PA0   // NO BED FAN, DIRECT connect to 24v


//
// Temperature Sensors
//
#define TEMP_0_PIN                          PA3   // TH1
#define TEMP_BED_PIN                        PB0   // TB1


//
// SD Card
//
// #define ONBOARD_SPI_DEVICE                     1
#define SDSUPPORT
//#define SD_CS_PIN                   PA0   // SDSS
#define ONBOARD_SD_CS_PIN != SD_SS_PIN

//#define BOARD_NO_NATIVE_USB



//
// EEPROM
//
#if NO_EEPROM_SELECTED
  #define IIC_BL24CXX_EEPROM                      // EEPROM on I2C-0
  //#define SDCARD_EEPROM_EMULATION
#endif

#if ENABLED(IIC_BL24CXX_EEPROM)
  #define IIC_EEPROM_SDA                    PB3
  #define IIC_EEPROM_SCL                    PB10
  #define MARLIN_EEPROM_SIZE               0x7D000  // 512K (24C512)
#elif ENABLED(SDCARD_EEPROM_EMULATION)
  #define MARLIN_EEPROM_SIZE               0x800  // 2K
#endif





//
// Steppers
//
#ifndef X_STEP_PIN
  #define X_STEP_PIN                       PE5
#endif
#ifndef X_DIR_PIN
  #define X_DIR_PIN                        PE4
#endif
#define X_ENABLE_PIN                 PE6   // Shared

#ifndef Y_STEP_PIN
  #define Y_STEP_PIN                        PB2
#endif
#ifndef Y_DIR_PIN
  #define Y_DIR_PIN                        PB14
#endif
#define Y_ENABLE_PIN                 XYZ_ENABLE

#ifndef Z_STEP_PIN
  #define Z_STEP_PIN                        PB8
#endif
#ifndef Z_DIR_PIN
  #define Z_DIR_PIN                         PB5
#endif
#define Z_ENABLE_PIN                 XYZ_ENABLE

#ifndef E0_STEP_PIN
  #define E0_STEP_PIN                      PB12
#endif
#ifndef E0_DIR_PIN
  #define E0_DIR_PIN                        PB9
#endif
#define E0_ENABLE_PIN                XYZ_ENABLE





// #define FAN_SOFT_PWM_REQUIRED

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
