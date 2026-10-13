/**
 * panel_sd_uart.h -- Driver "the SD" cua Marlin khi THE NAM O KHE MAN HINH (CYD),
 *                    du lieu lay qua UART (PANEL_SD_OVER_UART) -- xem lcd_panel/README.md muc 12.
 *
 * VI SAO: thay vi dung MUX SPI (74HC4053) de 2 master tranh nhau bus, o day chi co MOT nguoi
 * giu the: panel (ESP32) mount the va tra loi tung sector 512 B qua chinh UART da co san. Marlin
 * van chay dung ngan xep SD cua no (SdVolume / SdBaseFile / FAT16-32 / liet ke thu muc / M23-M24 /
 * tien do in) vi tat ca chi di qua 2 ham cua driver nay: readBlock()/writeBlock().
 *
 * KHONG can MUX, KHONG can cat duong, KHONG tranh chap bus, panel nap lai firmware luc nao cung duoc.
 * Doi lai: moi thao tac the cua Marlin phu thuoc UART + panel con song.
 *
 * Giai doan 1 (P1): CHI DOC (Marlin chay voi SDCARD_READONLY). writeBlock() tra false.
 */
#pragma once

#include "../inc/MarlinConfigPre.h"

#if ENABLED(PANEL_SD_OVER_UART)

#include "SdInfo.h"             // csd_t (disk_io_driver.h can; Sd2Card.h cung include file nay)
#include "disk_io_driver.h"

class DiskIODriver_PanelUART : public DiskIODriver {
public:
  bool init(const uint8_t sckRateID, const pin_t chipSelectPin) override;
  bool readCSD(csd_t *csd) override { UNUSED(csd); return false; }

  // Marlin KHONG dung 4 ham nay (SdVolume chi goi readBlock/writeBlock) nhung phai co vi la pure virtual.
  bool    readStart(const uint32_t block) override { UNUSED(block); return false; }
  bool    readData(uint8_t *dst) override { UNUSED(dst); return false; }
  bool    readStop() override { return false; }
  bool    writeStart(const uint32_t block, const uint32_t) override { UNUSED(block); return false; }
  bool    writeData(const uint8_t *src) override { UNUSED(src); return false; }
  bool    writeStop() override { return false; }

  bool    readBlock(uint32_t block, uint8_t *dst) override;
  bool    writeBlock(uint32_t blockNumber, const uint8_t *src) override;

  uint32_t cardSize() override { return size_blocks; }   // 0 = khong biet (khong ai dung khi khong co MSC)
  bool     isReady() override { return ready; }
  void     idle() override {}

private:
  bool     ready = false;
  uint32_t size_blocks = 0;
  uint32_t rd_ok = 0, rd_err = 0, wr_err = 0;   // thong ke (xem M115/log chan doan)
};

#endif // PANEL_SD_OVER_UART
