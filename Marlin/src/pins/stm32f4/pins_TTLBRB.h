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


#include "env_validate.h"


#ifndef BOARD_INFO_NAME
  #define BOARD_INFO_NAME      "TTL Voron 2.4"
#endif
#ifndef DEFAULT_MACHINE_NAME
  #define DEFAULT_MACHINE_NAME "TTL-BRB v0.1"
#endif
#ifndef BOARD_WEBSITE_URL
  #define BOARD_WEBSITE_URL "https://oshwlab.com/tungbuivn/voron-3d-printer-board"
#endif

#define X_MIN_PIN                           PC13
// #define X_MAX_PIN                           PA15
#define Y_MIN_PIN                           PB3
// #define Y_MAX_PIN                           PD12
#define Z_MIN_PIN                           PD14
// #define Z_MAX_PIN                           PD15

#define X_STEP_PIN                          PE5
#define X_DIR_PIN                           PE4
#define X_ENABLE_PIN                        PE6

#define Y_STEP_PIN                          PB7
#define Y_DIR_PIN                           PB6
#define Y_ENABLE_PIN                        PB5

#define Z_STEP_PIN                          PD15
#define Z_DIR_PIN                           PC7
#define Z_ENABLE_PIN                        PC6

#define Z2_STEP_PIN                          PB1
#define Z2_DIR_PIN                           PE7
#define Z2_ENABLE_PIN                        Z_ENABLE_PIN

#define E0_STEP_PIN                         PC4
#define E0_DIR_PIN                          PC5
#define E0_ENABLE_PIN                       PA7
// default setting current for each motor is 800ma
// Now if using small motor for extruder then need modify E0_CURRENT in configuration_adv.h (usually small motor has max current 0.6)


//
// All step motor using same setting about current, so we only need to
//
#define TMC_BAUD_RATE                     19200
// #define X_SERIAL_TX_PIN                  PD5
  // #define X_SERIAL_RX_PIN                  PD6

#define X_HARDWARE_SERIAL               MSerial2
#define X_SLAVE_ADDRESS                    0b00  // ms2 | ms1
#define Y_HARDWARE_SERIAL               MSerial2
// #define Y_SERIAL_TX_PIN                  PD5
  // #define Y_SERIAL_RX_PIN                  PD6

#define Y_SLAVE_ADDRESS                    0b01
#define E0_HARDWARE_SERIAL              MSerial2
// #define E0_SERIAL_TX_PIN                  PD5
  // #define E0_SERIAL_RX_PIN                  PD6

#define E0_SLAVE_ADDRESS                   0b10

#define Z_HARDWARE_SERIAL               MSerial3
#define Z_SLAVE_ADDRESS                    0b00
#define Z2_HARDWARE_SERIAL              MSerial3
#define Z2_SLAVE_ADDRESS                   0b10






//
// Temperature Sensors
//
#define TEMP_0_PIN                          PC0   // T0
#define TEMP_BED_PIN                        PC1   // TB


//
// Sensor probe on/off
//
// #define SOL1_PIN                            PE8 // turn on/off capacitive probe, using to deploy, stow probe
#define PROBE_ENABLE_PIN                    PE8

// servo pin
#define SERVO0                              PA6

//
// Heaters / Fans
//
#define HEATER_0_PIN                        PA2   // Heater0
#define HEATER_BED_PIN                      PA1   // Hotbed


#define FAN_PIN                             PE9  // Layer fan
#define FAN1_PIN                           PE10  // Hotend fan
#define FAN2_PIN                           PE11  // Electric Board Fan

#define FIL_RUNOUT_PIN                     PD10


//
// TFT LCD
//
#define TFT_A0_PIN                          PD9
#define TFT_CS_PIN                   PIN_SPI_SS
#define TFT_SCK_PIN                 PIN_SPI_SCK
#define TFT_MOSI_PIN               PIN_SPI_MOSI
#define TFT_MISO_PIN               PIN_SPI_MISO
#define TFT_RESET_PIN                       PD8

//
// Rotary Encoder
//
#define BTN_ENC                            PE15
#define BTN_EN1                            PE13
#define BTN_EN2                            PE14

// External I2C EEPROM
#if  NO_EEPROM_SELECTED
  #define I2C_EEPROM
  #define MARLIN_EEPROM_SIZE              0x80000  // 512K
  #define I2C_SCL_PIN                       PB8
  #define I2C_SDA_PIN                       PB9
  #undef NO_EEPROM_SELECTED
#endif



//
// Onboard SD support
//
// #ifndef SDCARD_CONNECTION
//   #define SDCARD_CONNECTION              ONBOARD
// #endif

// #if SD_CONNECTION_IS(ONBOARD)
  // #define SDIO_SUPPORT                            // Use SDIO for onboard SD
#if DISABLED(SDIO_SUPPORT)
  #define SOFTWARE_SPI                          // Use soft SPI for onboard SD
  #define SDSS                            PC11
  #define SD_SCK_PIN                      PC12
  #define SD_MISO_PIN                     PC8
  #define SD_MOSI_PIN                     PD2
#endif






// #endif
