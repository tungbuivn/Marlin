/**
 * cydlcd_sd_uart.cpp -- xem cydlcd_sd_uart.h.
 *
 * Luong 1 lan doc sector (512 B):
 *   readBlock(block, dst)
 *     -> ExtUI::cydlcd_serial_sd_read(block, dst, CYDLCD_SD_TIMEOUT_MS)   [chan toi da ~25 ms]
 *          gui CYDLCD_F_SDREQ {op=READ, sector}
 *          cho 4 frame CYDLCD_F_SDDATA (moi frame 128 B) ghep vao dst
 *     -> true/false
 *
 * Neu that bai: tra false -> SdVolume/SdBaseFile bao loi -> Marlin dung in "SD read error"
 * (KHONG treo may, vi vong lap cho co tran thoi gian).
 */
#include "../inc/MarlinConfig.h"

#if ENABLED(CYDLCD_SD_OVER_UART)

#include "cydlcd_sd_uart.h"
#include "../lcd/extui/cydlcd_serial/cydlcd_serial.h"

bool DiskIODriver_CydlcdUART::init(const uint8_t sckRateID, const pin_t chipSelectPin) {
  UNUSED(sckRateID);
  UNUSED(chipSelectPin);          // khong co chan CS nao ca: the do cydlcd giu
  ready = false;
  size_blocks = 0;
  if (!ExtUI::cydlcd_serial_sd_init()) {
    SERIAL_ECHOLNPGM("PANEL SD: khong doc duoc sector 0 qua UART (cydlcd chua san sang?)");
    return false;
  }
  ready = true;
  // Kich thuoc that (chi de bao cao; SdVolume tu doc BPB nen khong bat buoc):
  // FAT32/FAT16: tong so sector nam trong boot sector -> doc lai o day cho gon.
  uint8_t bs[CYDLCD_SD_SECTOR];
  if (ExtUI::cydlcd_serial_sd_read(0, bs, CYDLCD_SD_TIMEOUT_MS)) {
    const uint16_t tot16 = (uint16_t)(bs[19] | ((uint16_t)bs[20] << 8));
    const uint32_t tot32 = (uint32_t)bs[32] | ((uint32_t)bs[33] << 8) |
                           ((uint32_t)bs[34] << 16) | ((uint32_t)bs[35] << 24);
    size_blocks = tot16 ? tot16 : tot32;
  }
  SERIAL_ECHOLNPGM("PANEL SD: the SD qua UART san sang");
  return true;
}

bool DiskIODriver_CydlcdUART::readBlock(uint32_t block, uint8_t *dst) {
  if (!ready || !dst) return false;
  if (ExtUI::cydlcd_serial_sd_read(block, dst, CYDLCD_SD_TIMEOUT_MS)) { rd_ok++; return true; }
  rd_err++;
  SERIAL_ECHOLNPGM("PANEL SD: doc sector ", block, " THAT BAI");
  return false;
}

bool DiskIODriver_CydlcdUART::writeBlock(uint32_t blockNumber, const uint8_t *src) {
  if (!ready || !src) return false;
  // P3: ghi sector qua UART (man hinh CYD ghi xuong the bang SD.writeRAW).
  // Dung cho M28/M29 (ghi log ra the), M30 (xoa file), POWER_LOSS_RECOVERY...
  if (ExtUI::cydlcd_serial_sd_write(blockNumber, src, CYDLCD_SD_TIMEOUT_MS)) { wr_ok++; return true; }
  wr_err++;
  SERIAL_ECHOLNPGM("PANEL SD: ghi sector ", blockNumber, " THAT BAI");
  return false;
}

#endif // CYDLCD_SD_OVER_UART
