/**
 * tbt_run.h -- MAY TRANG THAI cua Bed Tramming Wizard, KHONG phu thuoc MarlinUI.
 *
 * VI SAO TACH RA:
 *   Truoc 2026-10-13 toan bo thuat toan nam trong lcd/menu/menu_tramming.cpp, nen chi man 12864
 *   moi dung duoc. Nay thuat toan nam o day de CA HAI duong cung dung chung MOT trang thai:
 *     - MarlinUI (menu 12864): menu_tramming.cpp = lop UI mong (ve + nhan encoder);
 *     - Man hinh rieng cydlcd: feature/cydlcd_tbt_link.cpp (link ASCII tren USART3).
 *   => KHONG dung ExtUI/CYDLCD_SERIAL_UI, nen menu 12864 (Tramming Wizard, Z ALIGN MANUAL...)
 *      van nguyen ven. Xem docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md.
 *
 * THUAT TOAN KHONG DOI so voi ban cu trong menu_tramming.cpp (moc Z-MIN/Z-MAX chot 1 lan,
 * nguong auto-DAT 0.01mm, tu di tiep khi DAT, DONE = goc nay xong, BACK = thoat, ca 4 DAT ->
 * hien delta so voi tam + chi con BACK). Xem khoi chu thich dai trong tbt_run.cpp.
 */
#pragma once

#include "../inc/MarlinConfig.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD)

#include "tramming.h"

// Trang thai (giong nguyen TR_* cu trong menu_tramming.cpp)
enum TramPhase : uint8_t {
  TR_IDLE = 0,   // dang cho nguoi dung van vit
  TR_HOMING,     // dang cho G28 Z xong
  TR_PROBE,      // dang probe 4 goc
  TR_GOTO,       // dang di toi goc lech nhat
  TR_POINT,      // dang di toi goc CAN CHINH ke tiep (nut DONE / tu dong sau khi DAT)
  TR_AUTO        // goc vua probe DA DAT -> TU DI TIEP (goc can chinh ke tiep, hoac home+probe lai)
};

// Ket qua lan PROBE gan nhat
enum TramProbeState : uint8_t {
  PR_NONE = 0,   // chua probe lai o goc nay -> hien huong van (CW/CCW + so mm)
  PR_RESULT,     // da probe lai -> hien TARGET con lai (z cuoi) + huong van
  PR_FAILED
};

// Nguong "auto DAT": |delta| <= so nay (mm) thi coi nhu goc do da xong.
// 0.01mm = muc nhieu that cua probe (do lai 20 lan lech 7-10um), duoi muc nay van nua la vo nghia.
#define TBT_DONE_MM 0.02f

namespace tbt_run {

  // -------------------------------------------------------------------------
  // Hook cho UI (menu_tramming dat; link serial thi de NULL -> khong lam gi)
  // -------------------------------------------------------------------------
  extern void (*refresh_hook)();       // tuong duong ui.refresh() trong code cu
  extern void (*exit_hook)();          // thoat wizard (menu: goto_previous_screen_no_defer)
  extern void (*enter_idle_hook)();    // vua chuyen sang TR_IDLE (menu: dat lai btn_sel)
  inline void refresh() { if (refresh_hook) refresh_hook(); }

  // -------------------------------------------------------------------------
  // Trang thai (ca 2 UI doc)
  // -------------------------------------------------------------------------
  extern uint8_t state;                // TramPhase
  extern uint8_t probe_state;          // TramProbeState
  extern uint8_t worst_index;          // goc nozzle dang dung
  extern uint8_t step_index;           // goc dang probe trong vong do 4 goc
  extern bool    all_dat;              // CA 4 GOC da dat -> chi con nut BACK
  extern bool    busy;                 // dang probe / di chuyen (blocking) -> khong nhan gi
  extern bool    active;               // wizard da bat (tu begin/restart toi exit)
  extern bool    ui_attached;          // man 12864 dang mo wizard -> MENU la driver, khong phai cydlcd

  inline uint8_t count() { return G35_PROBE_COUNT; }

  float   delta(const uint8_t i);      // z_delta[i] (mm, moc vat ly = Z-home)
  bool    ok(const uint8_t i);         // da probe duoc chua
  bool    accepted(const uint8_t i);   // nguoi dung da bam DONE o goc nay
  bool    ref_set();                   // da chot moc chua
  float   ref_min();                   // MOC Z-MIN da chot
  float   ref_max();                   // MOC Z-MAX da chot
  // MOC dung de QUYET DINH (MAX -> Z-MAX, MIN -> Z-MIN). KHAC `display_ref()` = Z-home (chi de hien thi).
  float   ref();                       // MOC dang dung (MAX -> Z-MAX, MIN -> Z-MIN)
  float   display_ref();               // LUON = base_home_pos(Z_AXIS): so hien thi luon la r_i = z_i - Z-home
  bool    need_adjust(const uint8_t i);
  bool    any_need_adjust();
  bool    next_need_adjust();          // doi worst_index; false = het goc

  // -------------------------------------------------------------------------
  // Hanh dong (dung chung cho menu 12864 VA link serial)
  // -------------------------------------------------------------------------
  void begin();                        // vao wizard: home TAT CA truc -> probe 4 goc
  void exit();                         // thoat: stow probe + danh dau Z chua home
  void step();                         // 1 buoc cua may trang thai (giu nguyen thu tu cu)
  void probe_here();                   // PROBE: probe lai goc dang dung
  void goto_corner(const uint8_t i);   // bam nhan goc tren man hinh CYD -> di chuyen toi goc do
  void done();                         // DONE: goc nay xong -> goc ke tiep / home+probe lai
  void toggle_minmax();                // doi chien luoc MAX <-> MIN (chi trong phien) roi di goc lech nhat
  void restart();                      // home lai Z + probe lai CA 4 GOC (moc moi)
  void goto_worst();                   // di nozzle toi goc lech nhat

}

// ---------------------------------------------------------------------------
// Diem vao CU (giu nguyen ten de lcd/menu/menu_motion.cpp khong phai sua):
//   "WORST POINT = MAX" trong menu Motion -> doi chien luoc MAX <-> MIN (chi trong phien,
//   KHONG luu EEPROM: mo lai may -> ve mac dinh MAX).
//   (Chi doi chien luoc, KHONG di chuyen nozzle - khac nut MAX<->MIN trong wizard.)
// ---------------------------------------------------------------------------
void tramming_worst_toggle();

#endif // ASSISTED_TRAMMING_WIZARD
