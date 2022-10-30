/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
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

#include "../../platforms.h"

#ifdef HAL_STM32

#include "../../../inc/MarlinConfig.h"

#if HAS_SPI_TFT

#include "tft_spi.h"
#include "pinconfig.h"
#ifdef USE_ADAFRUIT_SPI

#include <Adafruit_ST7789.h>

Adafruit_ST7789 TFT_ST7789_Adafruit::AdafruitSPI(TFT_CS_PIN, TFT_A0_PIN, TFT_RESET_PIN);
int yline;
int xline;
int wline;
int dataMode;
void TFT_ST7789_Adafruit::Init()
{

#if PIN_EXISTS(TFT_RESET)
  OUT_WRITE(TFT_RESET_PIN, HIGH);
  delay(100);
#endif
  OUT_WRITE(TFT_A0_PIN, HIGH);
  OUT_WRITE(TFT_CS_PIN, HIGH);

  AdafruitSPI.init(TFT_HEIGHT, TFT_WIDTH);

  AdafruitSPI.invertDisplay(false);
  AdafruitSPI.setRotation(1);
  AdafruitSPI.fillScreen(ST77XX_BLACK);
}
void TFT_ST7789_Adafruit::DataTransferBegin(uint16_t DataWidth)
{

  dataMode = DataWidth;
  AdafruitSPI.startWrite();
}
void TFT_ST7789_Adafruit::DataTransferEnd()
{

  AdafruitSPI.endWrite();
}
void TFT_ST7789_Adafruit::DataTransferAbort()
{
}

void TFT_ST7789_Adafruit::Transmit(uint16_t Data)
{

  if (dataMode == DATASIZE_8BIT)
  {
    AdafruitSPI.spiWrite(Data);
  }
  else
  {
    AdafruitSPI.SPI_WRITE16(Data);
  }
}
void TFT_ST7789_Adafruit::TransmitDMA(uint32_t MemoryIncrease, uint16_t *Data, uint16_t Count)
{

  DataTransferBegin();
  uint16_t *cp = Data;
  if (MemoryIncrease == DMA_MINC_DISABLE)
  {
    AdafruitSPI.writeColor(Data[0], Count);
  }
  else
  {

    while (Count--)
    {
      AdafruitSPI.SPI_WRITE16(*cp++);
    }
  }
  DataTransferEnd();
}
bool TFT_ST7789_Adafruit::isBusy()
{
  return false;
}
uint32_t TFT_ST7789_Adafruit::GetID()
{
  return 0x8552;
}

#endif
#endif
#endif