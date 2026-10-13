/**
 * tramming_report.h -- Dong trang thai + lenh cua Bed Tramming Wizard, DUNG CHUNG cho ca 2
 * kenh "man hinh rieng":
 *   - feature/cydlcd_tram_link.cpp : link ASCII rieng tren USART1 (CYDLCD_TRAM_LINK);
 *   - lcd/extui/cydlcd_serial      : kenh PUSH/ExtUI tren USART3 (CYDLCD_SERIAL_UI), frame
 *                                   CYDLCD_F_TRAM / CYDLCD_F_TRAM_CMD (xem cydlcd_protocol.h).
 *
 * VI SAO TACH RA (2026-10-13): logic build_status()/build_hint() + 6 lenh 'P','D','M','R','B','Q'
 * truoc day nam trong cydlcd_tram_link.cpp. Khi them kenh PUSH ma copy y nguyen sang
 * cydlcd_serial.cpp thi 2 noi se lech nhau (format dong, quy tac hint, gate busy...) nen dua
 * vao day lam MOT nguon duy nhat; ca 2 kenh chi con phan truyen/nhan.
 *
 * Format dong "TRAM ..." va y nghia 6 lenh: xem tram_protocol.h.
 * May trang thai (state/hint/worst/...)              : xem tramming_run.h.
 */
#pragma once

#include "../inc/MarlinConfig.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD) && EITHER(CYDLCD_SERIAL_UI, CYDLCD_TRAM_LINK)

  #ifndef TRAM_REPORT_LINE_MAX
    #define TRAM_REPORT_LINE_MAX 128       // du cho 1 dong "TRAM ..." (dai nhat thuc te ~110 byte)
  #endif
  #ifndef TRAM_REPORT_PUSH_MS
    #define TRAM_REPORT_PUSH_MS 200        // chu ky gui trang thai khi khong doi (~5Hz)
  #endif

  // Tao 1 dong "TRAM ...\n" (co ca '\n' o cuoi) vao `buf` (>= TRAM_REPORT_LINE_MAX byte).
  // Tra ve so byte da ghi (khong tinh '\0'), 0 = khong tao duoc.
  int tramming_report_line(char *buf, int len);

  // Xu ly 1 lenh tu man hinh CYD: 'P' probe, 'D' done, 'M' doi MAX/MIN, 'R' restart/begin,
  // 'B' thoat, 'Q' hoi trang thai (nhan ca chu thuong). Tra ve TRUE neu la lenh hop le.
  //   - 'Q' LUON duoc xu ly (chi dat co "gui lai ngay");
  //   - cac lenh HANH DONG bi BO QUA khi tramming_run::busy (dang probe/di chuyen blocking).
  bool tramming_report_cmd(char cmd);

  // Lay co "co lenh -> gui lai trang thai ngay" (do 1 lenh hop le dat) va tu xoa co.
  bool tramming_report_force_take();

#endif
