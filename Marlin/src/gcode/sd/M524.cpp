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

#include "../../inc/MarlinConfig.h"

#if ENABLED(SDSUPPORT)

#include "../gcode.h"
#include "../../MarlinCore.h"            // wait_for_heatup / wait_for_user
#include "../../sd/cardreader.h"
#include "../queue.h"                    // queue.clear() (bo lenh da nap tu file)
#include "../../module/motion.h"         // quickstop_stepper()
#include "../../module/temperature.h"    // thermalManager.disable_all_heaters()

#if ENABLED(DWIN_LCD_PROUI)
  #include "../../lcd/marlinui.h"
#endif

/**
 * M524: Abort the current SD print job (started with M24)
 *
 * SUA DOI CHO BAN NAY (2026-10-15) - theo yeu cau: "khi gui M524 thi NGUNG FEED noi dung file
 * dang in, coi nhu DA DOC XONG". Truoc day M524 chi dat co abort roi doi vong `loop()` xu ly
 * (MarlinCore.cpp:1680) => 2 van de gap that:
 *   1. Neu Marlin dang o trong mot vong CHO (M109/M190 cho nong - vd PSU tat nen ban khong nong
 *      duoc) thi `loop()` KHONG BAO GIO chay -> co abort khong duoc xu ly -> bam STOP khong dung
 *      duoc ban in, phai reset board. Nay dat luon `wait_for_heatup = false` de vong cho THOAT.
 *   2. Cac lenh DA NAP TU FILE (di chuyen, gia nhiet...) van nam trong hang doi va se CHAY TIEP
 *      sau khi "dung" -> may van dong/nung them. Nay XOA hang doi (coi nhu da doc xong file).
 *   3. (2026-10-15, nguoi dung bao "huy in xong van thay hotend dang nong") TAT GIA NHIET + DUNG
 *      CHUYEN DONG NGAY tai day. Truoc day 2 viec do chi nam trong `abortSDPrinting()`
 *      (MarlinCore.cpp:363) va chi chay o vong `loop()` (MarlinCore.cpp:1680) -> neu Marlin dang o
 *      trong 1 lenh DAI (vd `G4` dwell) hoac vua moi thoat vong cho nong thi bi TRE; trong luc tre,
 *      NHIET DO MUC TIEU tu file (vd 250 C) VAN CON -> hotend van nung. Xem cydlcd_serial.cpp (ban
 *      sao y nguyen cho lenh M524 di tu MAN HINH CYD, xu ly khong qua hang doi).
 */
void GcodeSuite::M524() {

  #if ENABLED(DWIN_LCD_PROUI)

    ui.abort_print();

  #else

    wait_for_heatup = false;                     // thoat vong cho nong (M109/M190/M303)
    TERN_(HAS_RESUME_CONTINUE, wait_for_user = false);

    if (IS_SD_PRINTING()) {
      card.flag.abort_sd_printing = true;        // IS_SD_PRINTING() = false -> NGUNG FEED ngay
      card.abortFilePrintSoon();                 // ... va de MarlinCore goi abortSDPrinting()
    }
    else if (card.isMounted())
      card.closefile();

    queue.clear();                               // bo cac lenh da nap tu file (chua chay)
    quickstop_stepper();                         // dung moi chuyen dong dang xep
    IF_DISABLED(SD_ABORT_NO_COOLDOWN, thermalManager.disable_all_heaters());

  #endif

}

#endif // SDSUPPORT
