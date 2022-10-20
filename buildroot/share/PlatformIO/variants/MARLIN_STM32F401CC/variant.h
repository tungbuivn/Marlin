/*
 *******************************************************************************
 * Copyright (c) 2011-2021, STMicroelectronics
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#pragma once

/*----------------------------------------------------------------------------
 *        STM32 pins number
 *----------------------------------------------------------------------------*/
#define PA0                     PIN_A0  // NOT USED
#define PA1                     PIN_A1  // HotEnd FAN1 PWM, FAN2 will direct connect to 24v
#define PA2                     PIN_A2  // HotBed PWM
#define PA3                     PIN_A3  // HotEnd thermistor
#define PA4                     PIN_A4  // xxxxxxxxxxx lcd_spi_cs_1
#define PA5                     PIN_A5  // xxxxxxxxxxx lcd_spi_sclk_1
#define PA6                     PIN_A6  // xxxxxxxxxxx lcd_spi_miso_1
#define PA7                     PIN_A7  // xxxxxxxxxxx lcd_spi_mosi_1
#define PA8                     8       // yyyyyyyyyyy I2C3 SCL3 lcd_12864
#define PA9                     9       // PIN_SERIAL_TX usb cable
#define PA10                    10      // PIN_SERIAL_RX usb cable
#define PA11                    11      // XYZ end stop this can be share but e will not, just becausse when x,y trigger then e will stop too
#define PA12                    12      // XYZE enable
#define PA13                    13      // SWDCLK
#define PA14                    14      // SWDIO
#define PA15                    15      // X-Dir
#define PB0                     PIN_A8  // BED thermistor
#define PB1                     PIN_A9
#define PB2                     18      // Y-PULSE
#define PB3                     19      // SDA2 sdcard
#define PB4                     20      // yyyyyyyyyyy I2C3 SDA3 lcd_12864
#define PB5                     21      // Z-DIR
#define PB6                     22      // SCL1 eeprom
#define PB7                     23      // SDA1 eeprom, PIN_WIRE_SDA
#define PB8                     24      // Z-PULSE
#define PB9                     25      // E-DIR
#define PB10                    26      // SCL2 SD card
#define PB12                    27      // E-PULSE
#define PB13                    28      // X-PULSE
#define PB14                    29      // Y-DIR
#define PB15                    30      // xxxxxxxxxx lcd_spi_lcd_a0
#define PC13                    31      // xxxxxxxxxx lcd_spi_lcd_reset
#define PC14                    32      // E-EndStop (filament sensor)
#define PC15                    33
#define PH0                     34      // crysal
#define PH1                     35      // crysal


#define NUM_DIGITAL_PINS        36
#define NUM_ANALOG_INPUTS       10

// On-board LED pin number
#define LED_GREEN               PC13
#ifndef LED_BUILTIN
  #define LED_BUILTIN           LED_GREEN
#endif

// On-board user button
#ifndef USER_BTN
  // User button is not available on all revision of this board
  #define USER_BTN              PA0
#endif

// SPI definitions
#ifndef PIN_SPI_SS
  #define PIN_SPI_SS            PA4
#endif
#ifndef PIN_SPI_SS1
  #define PIN_SPI_SS1           PA4
#endif
#ifndef PIN_SPI_SS2
  #define PIN_SPI_SS2           PB12
#endif
#ifndef PIN_SPI_SS3
  #define PIN_SPI_SS3           PA15
#endif
#ifndef PIN_SPI_MOSI
  #define PIN_SPI_MOSI          PA7
#endif
#ifndef PIN_SPI_MISO
  #define PIN_SPI_MISO          PA6
#endif
#ifndef PIN_SPI_SCK
  #define PIN_SPI_SCK           PA5
#endif

// I2C definitions
#ifndef PIN_WIRE_SDA
  #define PIN_WIRE_SDA          PB7
#endif
#ifndef PIN_WIRE_SCL
  #define PIN_WIRE_SCL          PB6
#endif

// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#ifndef TIMER_TONE
  #define TIMER_TONE            TIM10
#endif
#ifndef TIMER_SERVO
  #define TIMER_SERVO           TIM11
#endif

// UART Definitions
#ifndef SERIAL_UART_INSTANCE
  #define SERIAL_UART_INSTANCE  1
#endif

// Default pin used for generic 'Serial' instance
// Mandatory for Firmata
#ifndef PIN_SERIAL_RX
  #define PIN_SERIAL_RX         PA10
#endif
#ifndef PIN_SERIAL_TX
  #define PIN_SERIAL_TX         PA9
#endif

// #ifndef HSE_VALUE
//   #define HSE_VALUE             25000000U
// #endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#ifdef __cplusplus
  // These serial port names are intended to allow libraries and architecture-neutral
  // sketches to automatically default to the correct port name for a particular type
  // of use.  For example, a GPS module would normally connect to SERIAL_PORT_HARDWARE_OPEN,
  // the first hardware serial port whose RX/TX pins are not dedicated to another use.
  //
  // SERIAL_PORT_MONITOR        Port which normally prints to the Arduino Serial Monitor
  //
  // SERIAL_PORT_USBVIRTUAL     Port which is USB virtual serial
  //
  // SERIAL_PORT_LINUXBRIDGE    Port which connects to a Linux system via Bridge library
  //
  // SERIAL_PORT_HARDWARE       Hardware serial port, physical RX & TX pins.
  //
  // SERIAL_PORT_HARDWARE_OPEN  Hardware serial ports which are open for use.  Their RX & TX
  //                            pins are NOT connected to anything by default.
  #ifndef SERIAL_PORT_MONITOR
    #define SERIAL_PORT_MONITOR   Serial
  #endif
  #ifndef SERIAL_PORT_HARDWARE
    #define SERIAL_PORT_HARDWARE  Serial
  #endif
#endif
