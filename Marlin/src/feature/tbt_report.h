/**
 * tbt_report.h -- Dong trang thai + lenh cua Bed Tramming Wizard, DUNG CHUNG cho ca 2
 * kenh "man hinh rieng":
 *   - feature/cydlcd_tbt_link.cpp : link ASCII rieng tren USART1 (CYDLCD_TBT_LINK);
 *   - lcd/extui/cydlcd_serial      : kenh PUSH/ExtUI tren USART3 (CYDLCD_SERIAL_UI), frame
 *                                   CYDLCD_F_TBT / CYDLCD_F_TBT_CMD (xem cydlcd_protocol.h).
 *
 * VI SAO TACH RA (2026-10-13): logic build_status()/build_hint() + 6 lenh 'P','D','M','R','B','Q'
 * truoc day nam trong cydlcd_tbt_link.cpp. Khi them kenh PUSH ma copy y nguyen sang
 * cydlcd_serial.cpp thi 2 noi se lech nhau (format dong, quy tac hint, gate busy...) nen dua
 * vao day lam MOT nguon duy nhat; ca 2 kenh chi con phan truyen/nhan.
 *
 * Format dong "TBT ..." va y nghia 6 lenh: xem tbt_protocol.h.
 * May trang thai (state/hint/worst/...)              : xem tbt_run.h.
 */
#pragma once

#include "../inc/MarlinConfig.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD) && EITHER(CYDLCD_SERIAL_UI, CYDLCD_TBT_LINK)

  #ifndef TBT_REPORT_LINE_MAX
    #define TBT_REPORT_LINE_MAX 128       // du cho 1 dong "TBT ..." (dai nhat thuc te ~110 byte)
  #endif
  #ifndef TBT_REPORT_PUSH_MS
    #define TBT_REPORT_PUSH_MS 200        // chu ky gui trang thai khi khong doi (~5Hz)
  #endif

  // Tao 1 dong "TBT ...\n" (co ca '\n' o cuoi) vao `buf` (>= TBT_REPORT_LINE_MAX byte).
  // Tra ve so byte da ghi (khong tinh '\0'), 0 = khong tao duoc.
  int tbt_report_line(char *buf, int len);

  // Xu ly 1 lenh tu man hinh CYD: 'P' probe, 'D' done, 'M' doi MAX/MIN, 'R' restart/begin,
  // 'B' thoat, 'Q' hoi trang thai (nhan ca chu thuong). Tra ve TRUE neu la lenh hop le.
  //   - 'Q' LUON duoc xu ly (chi dat co "gui lai ngay");
  //   - cac lenh HANH DONG bi BO QUA khi tbt_run::busy (dang probe/di chuyen blocking).
  bool tbt_report_cmd(char cmd);

  // Lay co "co lenh -> gui lai trang thai ngay" (do 1 lenh hop le dat) va tu xoa co.
  bool tbt_report_force_take();

#endif
