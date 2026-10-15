/**
 * ui_page_menu.cpp -- man MENU cua UI LVGL (man hinh CYD): menu doc kieu Marlin, mo bang nut
 * SETTINGS tren man STATUS.
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham/bien dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 *   - Bang MENU_ITEMS[] (ten muc + chi so trang PG_*) va cot gia tri ben phai (mn_val[]).
 *   - cb_cal_touch(): dong "Calibrate touch" phai GIU TAY >= 1.5 s moi chay (chong bam nham).
 *   - s_cal_hold_ms chi dung trong file nay -> giu `static`; s_cal_req duoc ui_lvgl_task()
 *     doc de chay hieu chuan -> KHONG `static`, khai bao `extern` trong ui_lvgl_int.h.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// ===========================================================================
//  MENU -- menu doc kieu Marlin (nut SETTINGS tren man STATUS mo vao day)
//  Moi dong: [ten vang] .......... [gia tri trang]; dong la 1 nut (encoder bam = mo)
// ===========================================================================
#define MENU_ROWS 12
// 1 dong menu: ten + trang se mo (page) HOAC mot hanh dong A_* (act) khi page == MENU_ACT_PAGE.
// 2026-10-15: dong "Motion" khong mo mot TRANG nao ma mo NHOM menu MOTION (giong 12864: Motion la muc
// trong Main menu) -> phai di qua do_action(A_GOTO_MOTION).
#define MENU_ACT_PAGE 0xFF
typedef struct { const char *name; uint8_t page; uint8_t act; } menu_item_t;
static const menu_item_t MENU_ITEMS[MENU_ROWS] = {
  { "Temperature",     PG_TEMP,   0 },
  // 2026-10-15 (yeu cau nguoi dung: "sua not menu motion giong so do menu cua 12864"): dong nay
  // truoc la "Motion (JOG)" mo THANG trang JOG; nay mo menu MOTION (Move Axis/Homing/Z-Align/UBL/
  // Tramming/M48...) - trong do co dong "Move Axis (JOG)" mo lai trang JOG cu.
  { "Motion",          MENU_ACT_PAGE, A_GOTO_MOTION },
  { "Feed/Flow rate",  PG_RATE,   0 },
  { "SD card",         PG_FILES,  0 },
  { "Bed mesh",        PG_MESH,   0 },
  { "Custom G34/SD",   PG_CUSTOM, 0 },
  // 2026-10-15 (yeu cau nguoi dung: "phan configuration toi thay lam giong 12864 tien hon"): dong nay
  // KHONG con mo THANG bang phang nua ma mo CAY menu Configuration (PG_SUBMENU, xem ui_page_submenu.cpp)
  // giong menu Configuration cua Marlin 12864. Bang phang cu van con: "Advanced Settings > All settings".
  { "Configuration",   PG_SUBMENU, 0 },
  { "Tuning (MPC/PID)",PG_MPC,    0 },     // 2026-10-15: 6 tham so MPC (M306) + nut tune, giong Marlin 12864
  { "Tramming wizard", PG_TBT,    0 },
  { "Info",            PG_INFO,   0 },
  { "Endstops (M119)", PG_ENDSTOP,0 },
  // Muc 12: WIFI (mo tu day HOAC bam icon WiFi o header/man STATUS).
  // MENU gio 12 muc + dong "Calibrate touch" = 13 hang x 22 px = 286 > 206 -> thanh day
  // PREV/NEXT xuat hien va phan trang.
  { "WiFi",            PG_WIFI,   0 }
};
static lv_obj_t *mn_val[MENU_ROWS];
static lv_obj_t *mn_val_cal;                 // dong "Calibrate touch" (chi co tren board co touch)
// Dong nguoi dung vua bam: khi BACK ve MENU phai focus + cuon toi dong do (yeu cau nguoi dung
// 2026-10-15: "chua highlight item da trigger open sub-menu"). KHONG luu mang con tro nut: cac dong
// duoc tao LIEN TIEP theo thu tu nen `lv_obj_get_child(trang, i)` chinh la dong thu i (tiet kiem DRAM -
// panel da full, xem ghi chu dram0_0_seg).
static uint8_t s_menu_sel = 255;             // 255 = chua bam dong nao

static void cb_menu_row(lv_event_t *e) {
  // user_data = CHI SO DONG (khong phai chi so trang nua) de nho dong vua mo man khac.
  const uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (i >= MENU_ROWS) return;
  s_menu_sel = i;
  // Dong co the mo 1 TRANG (page) hoac chay 1 HANH DONG (act - vd "Motion" mo menu MOTION).
  if (MENU_ITEMS[i].page == MENU_ACT_PAGE) do_action((int)MENU_ITEMS[i].act);
  else                                     show_page(MENU_ITEMS[i].page);
}

// Focus dong da mo man khac (goi tu page_after_show() cua ui_lvgl.cpp SAU khi trang MENU da hien lai va
// da tra vi tri cuon; focus_mark se ve khung sang cho dong do).
// KHONG cuon theo dong (`lv_obj_scroll_to_view`): vi tri cuon da duoc tra lai dung cho nguoi dung vua
// xem, va dong vua bam luon nam trong cho do -> cuon theo se ghi de cho cuon vua tra lai.
void page_menu_focus_sel(void) {
  if (s_menu_sel >= MENU_ROWS) return;
  lv_obj_t *r = lv_obj_get_child(s_page[PG_MENU], s_menu_sel);   // xem ghi chu: child i = dong thu i
  if (r && s_group) lv_group_focus_obj(r);
}

// "Calibrate touch": chay trinh hieu chuan 4 goc cua port (blocking, tu ve bang TFT_eSPI),
// sau do xoa toan bo man hinh de LVGL ve lai tu dau.
//
// CHONG BAM NHAM: phai GIU TAY >= CAL_TOUCH_HOLD_MS roi NHAC moi vao; 1 lan cham ngan
// (lo bam trong MENU) khong lam gi. Ly do phai chan: trinh hieu chuan la blocking, moi goc
// CHI CAN 1 lan cham la duoc nhan, va no GHI NGAY vao NVS -> bam nham co the lam hong cam ung
// (khi do chi con duong giu tay luc cap dien de vao lai).
#define CAL_TOUCH_HOLD_MS 1500
static uint32_t s_cal_hold_ms = 0;      // >0 = dang giu tay tren dong nay
bool            s_cal_req     = false;  // da giu du -> ui_lvgl_task() se chay hieu chuan

static void cb_cal_touch(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      s_cal_hold_ms = ui_port_millis();
      if (!s_cal_hold_ms) s_cal_hold_ms = 1;               // 0 = "khong giu"
      break;

    case LV_EVENT_RELEASED: {
      const uint32_t held = s_cal_hold_ms ? (ui_port_millis() - s_cal_hold_ms) : 0;
      s_cal_hold_ms = 0;
      if (held >= CAL_TOUCH_HOLD_MS) s_cal_req = true;     // du lau -> cho chay o ui_lvgl_task()
      break;
    }

    // Keo/scroll lam mat diem bam -> coi nhu huy giu
    case LV_EVENT_PRESS_LOST:  s_cal_hold_ms = 0; break;

    default: break;                                        // LV_EVENT_CLICKED (cham ngan) -> bo qua
  }
}

//  MENU la 1 man BINH THUONG: co dong tieu de chung o tren (ten man + LINK + BACK ben phai),
//  cac dong menu do FLEX cua mk_page() tu xep doc (khong can toa do y).
void page_menu_create(lv_obj_t *p) {
  // 10 dong menu + (tren board co touch) 1 dong "Calibrate touch" = 11 dong x 20 px = 220 > 206,
  // nen MENU luon co thanh day de phan trang (xem bot_sync; trang bi thu nho con 192 px -> 184 trong).
  // mk_page() dat pad_row = GAP cho moi man, o day phai de 0 thi moi du (cac dong la o dac lien
  // nhau - giong danh sach menu cua Marlin; dong dang chon duoc to sang + vien xanh).
  lv_obj_set_style_pad_row(p, 0, 0);

  for (uint8_t i = 0; i < MENU_ROWS; i++) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2,
                         cb_menu_row, (void *)(intptr_t)i);   // user_data = CHI SO DONG (xem cb_menu_row)
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, MENU_ITEMS[i].name, MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val[i] = mk_label(r, "", MR_MENUVAL);
    lv_obj_align(mn_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // Dong hieu chuan cam ung: chi hien tren board co touch (CYD).
  // PHAI GIU 1.5s (xem cb_cal_touch) -> 3 event rieng, khong dung CLICKED cua mk_hit.
  if (ui_port_touch_cal_available()) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2, cb_cal_touch, NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, "Calibrate touch (giu 1.5s)", MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val_cal = mk_label(r, "4 goc", MR_MENUVAL);
    lv_obj_align(mn_val_cal, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // KHONG ve nut BACK o day nua: da co nut BACK o goc phai dong tieu de (build_chrome)
}

void page_menu_update(void) {
  if (!mn_val[0]) return;                // trang da bi thu hoi (an toan: xem page_menu_destroy)
  char b[24];
  // gia tri ben phai tung dong (giong cot "value" trong menu Marlin)
  int16_t cur = 0, tgt = 0;
  temp_lookup(CYDLCD_TEMP_E0, &cur, &tgt);
  char a1[12];
  fmt_scaled(a1, sizeof(a1), cur, 100, 0);
  snprintf(b, sizeof(b), "%s C", a1);
  set_text(mn_val[0], b);

  snprintf(b, sizeof(b), "X%u Y%u Z%u",
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0),
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0),
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0));
  set_text(mn_val[1], b);

  snprintf(b, sizeof(b), "%ld/%ld%%",
           (long)(cydlcd_setting(CYDLCD_SET_FEEDRATE_PCT, 10000) / 100),
           (long)(cydlcd_setting(CYDLCD_SET_FLOW_PCT, 10000) / 100));
  set_text(mn_val[2], b);

  // YEU CAU NGUOI DUNG 2026-10-15: "bo phan dem so luong file o menu file" -> dong "SD card" o trang MENU
  // KHONG con hien so file (truoc: "12 file"). Cach Marlin cung vay: muc "Card" trong menu khong co gia tri;
  // the co doc duoc hay khong da co ICON THE o thanh tren, con so luong thi thay ngay khi mo trang.
  // (Bam vao dong nay la mo thang trang FILE SD - xem MENU_ITEMS.)
  set_text(mn_val[3], "");

  if (cydlcd.hello_flags & CYDLCD_HAS_MESH)
    // Them TRANG THAI UBL ON/OFF ngay o dong MENU (yeu cau nguoi dung 2026-10-15: bam UBL ON/OFF xong khong
    // biet da an hay chua) - nhi`n tu MENU cung thay, khong phai vao man BED MESH.
    snprintf(b, sizeof(b), "%ux%u %s", (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny,
             cydlcd.mesh_on ? "ON" : "OFF");
  else snprintf(b, sizeof(b), "n/a");
  set_text(mn_val[4], b);

  set_text(mn_val[5], "G34/M22");
  // So dong thong so da nhan tu Marlin. KHONG con ghi "58/33": tong so 33 la con so CU (Marlin gui 58 dong
  // tu 2026-10-15) nen dong do thanh vo nghia/kho hieu. Man hinh KHONG biet tong so that (Marlin khong gui
  // frame "het danh sach" cho SETTING) -> chi ghi so da nhan.
  snprintf(b, sizeof(b), "%u muc", (unsigned)cydlcd.n_settings);
  set_text(mn_val[6], b);

  // Muc 8 "Tuning (MPC/PID)" (chen 2026-10-15): hien cong suat heater cua MPC (tham so P) de thay ngay
  // gia tri tune hien tai; chua nhan duoc (ban build khong co MPCTEMP) thi ghi "M306".
  {
    const int32_t p = cydlcd_setting(CYDLCD_SET_MPC_P, INT32_MIN);
    if (p == INT32_MIN) set_text(mn_val[7], "M306");
    else {
      char a[12];
      fmt_scaled(a, sizeof(a), p, 1000, 2);
      snprintf(b, sizeof(b), "P%s W", a);
      set_text(mn_val[7], b);
    }
  }

  // Tramming wizard: hien trang thai WIZARD (khong phai chi bao link - chi bao link o THANH TREN).
  // Chua co dong TBT nao / mat link thi ghi ro de biet duong nao chua thong.
  if (!link_ok()) snprintf(b, sizeof(b), "%s", tbt.ever ? "mat link" : "chua bat");
  else if (!tbt.st.active) snprintf(b, sizeof(b), "san sang");
  else snprintf(b, sizeof(b), "%s %s", tbt.st.mode_max ? "MAX" : "MIN", tbt_phase_name(tbt.st.phase));
  set_text(mn_val[8], b);

  // Duong dang dung (PUSH / TRAM / chua co) - KHONG hien lai trang thai link o day,
  // vi chi bao link chi nam o THANH TREN (xem link_ok()).
  snprintf(b, sizeof(b), "%s", tbt.ever ? "TBT" : (cydlcd.linked ? "PUSH" : "-"));
  set_text(mn_val[9], b);

  // Muc "Endstops (M119)": man chi hien ket qua M119 nen o day chi ghi lenh da gui
  set_text(mn_val[10], "M119");
  // Muc "WiFi": hien trang thai ngan (TAT / DANG KET NOI / DA KET NOI / khong ho tro)
  if (!wifi_api_ok()) set_text(mn_val[11], "n/a");
  else {
    static const char *const WST[3] = { "TAT", "dang noi", "DA NOI" };
    const uint8_t ws = wifi_state_now();
    set_text(mn_val[11], WST[ws <= 2 ? ws : 0]);
  }
  // Dong "Calibrate touch": thuong hien nguon gia tri (NVS / mac dinh); khi dang GIU tay thi
  // hien tien do de nguoi dung biet da du 1.5s chua.
  if (s_cal_hold_ms)
    set_text(mn_val_cal, (ui_port_millis() - s_cal_hold_ms) >= CAL_TOUCH_HOLD_MS ? "NHAC TAY" : "dang giu");
  else
    set_text(mn_val_cal, ui_port_touch_cal_is_saved() ? "NVS" : "mac dinh");
}

// Huy man MENU (xem quy uoc o ui_lvgl_int.h): dat NULL cot gia tri tung dong + dong "Calibrate
// touch", va HUY trang thai "dang giu tay" (s_cal_hold_ms) - neu nguoi dung roi man trong luc dang
// giu dong do thi lan mo sau se khong con hien "dang giu" ket lai. Container trang do show_page()
// xoa bang lv_obj_del() nen khong xoa widget o day.
void page_menu_destroy(void) {
  for (uint8_t i = 0; i < MENU_ROWS; i++) mn_val[i] = NULL;
  mn_val_cal   = NULL;
  s_cal_hold_ms = 0;
  // s_menu_sel: GIU NGUYEN (dong nguoi dung da bam) - dung de focus lai khi quay ve MENU.
}

#endif  // CYDLCD_USE_LVGL
