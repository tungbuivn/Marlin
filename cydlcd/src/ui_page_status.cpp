/**
 * ui_page_status.cpp -- man STATUS cua UI LVGL (man hinh CYD): bo cuc lay Y NGUYEN toa do tu
 * UI TFT ILI9341 cua Marlin (Marlin/src/lcd/tft/ui_320x240.cpp).
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom: 3 o 64x100 (dau in / ban in / quat) + hang XYZ + FR/FL + nut SD/SETTINGS + thoi gian in
 * + thanh tien do + dong trang thai. status_cell() va cac mang `st_*` chi dung trong file nay
 * nen VAN giu `static`.
 *
 * LUU Y: ui_lvgl_icons.h giu bitmap dang `static` -> CHI duoc include o DUNG file .cpp co dung
 * icon do (include o nhieu file se nhan doi du lieu flash). Sau khi tach man STATUS thi icon
 * 64x64/32x32 duoc dung o file nay va ui_page_rate.cpp (2 icon 32x32).
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include "ui_lvgl_icons.h"          // icon 64x64/32x32 chuyen tu bitmap cua Marlin

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// ===========================================================================
//  STATUS -- TOA DO Y NGUYEN tu Marlin/src/lcd/tft/ui_320x240.cpp
//   3 o nhiet/quat 64x100 tai x = 21/127/234 + hang XYZ + FR/FL + SD + SETTINGS
//   + thoi gian (96,176) + thanh tien do (4,198) + dong trang thai (0,216)
//   Icon 64x64 cua Marlin (bitmap GREYSCALE4) da port that vao ui_lvgl_icons.h.
// ===========================================================================
#define ST_CELLS 3                     // 0 = dau in (E0), 1 = ban in (BED), 2 = quat (FAN)

// ---------------------------------------------------------------------------
//  HANG GIUA cua man STATUS (y 130..194) - bo cuc DA SUA so voi Marlin (2026-10-14) de co cho
//  cho ICON WiFi + chu dBm nam GIUA khoi FR va khoi FL (xem ui_chrome.cpp: ST_WIFI_X):
//
//    Marlin goc : [the SD 0..63, 64x64] [FR 70..149, 80x32] [FL 170..249, 80x32] [SET 256..319]
//    Nay        : [the SD 0..47, 48x48] [FR 54..121, 68x32] |WiFi 128..157 + "-58" 161..185|
//                 [FL 192..259, 68x32] [SET 272..319, 48x48]
//
//  Vi sao phai doi: khe giua FR va FL (Marlin: 150..169 = 20 px) khong du cho icon 30 px + chu
//  dBm (54 px). Nhuong bang cach THU NHO 2 o the (64 -> 48, van >= 44 px cho tay bam) va THU HEP
//  2 khoi FR/FL (80 -> 68; gia tri "100%" = 4 ky tu mono x 8 px = 32 px van nam gon o x = 32..63).
//  Icon the/SETTINGS la bitmap 64x64 co san cua Marlin -> thu bang lv_img_set_zoom() (icon_zoom_to),
//  KHONG cat bot hinh.
// ---------------------------------------------------------------------------
#define ST_TILE_SZ   48                // o the SD / SETTINGS: 48x48 (Marlin: 64x64)
#define ST_TILE_Y    138               // canh giua dai y 130..193: 130 + (64 - 48) / 2
#define ST_SET_X     272               // sat mep phai: 272 + 48 = 320
#define ST_RATE_W    68                // khoi FR/FL: 68 px (Marlin: 80) - icon 32 + gia tri 32
#define ST_FR_X      54                // Marlin: 70
#define ST_FL_X      192               // Marlin: 170

static lv_obj_t *st_cell[ST_CELLS], *st_icon[ST_CELLS];
static lv_obj_t *st_tgt[ST_CELLS], *st_cur[ST_CELLS];
static lv_obj_t *st_axis_val[3], *st_xyz_frame;
static lv_obj_t *st_rate_val[2], *st_rate_icon[2];
static lv_obj_t *st_time, *st_pct, *st_bar, *st_msg;

// Thu nho icon bitmap cua Marlin (64x64) xuong sz x sz.
// KHONG dung lv_img_set_zoom(): LVGL 8.4 chi ho tro transform (zoom/xoay) cho TRUE_COLOR* /
// RGB565A8 (lv_draw_sw_transform.c, ALPHA_4BIT roi vao `default:` -> khong ve gi) ma icon cua
// Marlin la ALPHA_4BIT -> zoom se lam icon BIEN MAT. Vi vay bitmap 48x48 duoc SINH SAN bang
// tools/make_lvgl_icons.py (muc SMALL) va o day chi con chon dung symbol theo kich thuoc o.
static const lv_img_dsc_t *pick_icon(const lv_img_dsc_t *full, const lv_img_dsc_t *small, const lv_coord_t sz) {
  return (sz < full->header.w && small) ? small : full;
}

// 1 o 64x100: [target] / [icon 64x64] / [gia tri]; ca o la 1 nut (bam -> man tuong ung)
// Thu tu tao: nut -> ICON -> 2 nhan, de nhan ve DE LEN icon (giong Marlin: text ve sau image).
static void status_cell(lv_obj_t *p, const uint8_t i, const lv_img_dsc_t *icon, const uint8_t act) {
  lv_obj_t *c = mk_hit(p, MR_CELL_X(i), 0, MR_CELL_W, MR_CELL_H,
                       cb_action, (void *)(intptr_t)act);
  st_cell[i] = c;
  st_icon[i] = mk_icon(c, 0, MR_ICON_Y, icon, MR_COLD);
  st_tgt[i]  = mk_label_mid(c, 0, MR_TGT_Y, MR_CELL_W, "-", MR_COLD);
  st_cur[i]  = mk_label_mid(c, 0, MR_CUR_Y, MR_CELL_W, "-", MR_COLD);
}

void page_status_create(lv_obj_t *p) {
  // ---- 3 o: dau in (E0) / ban in (BED) / quat (FAN) ----
  status_cell(p, 0, &ui_icon_hotend, A_GOTO_TEMP);
  status_cell(p, 1, &ui_icon_bed,    A_GOTO_TEMP);
  status_cell(p, 2, &ui_icon_fan,    A_GOTO_TEMP);
  lv_obj_set_pos(st_icon[2], 0, MR_FAN_ICON_Y);        // Marlin ve icon quat o y=10

  // ---- hang XYZ: khung (4, 103, 312, 24) ----
  // Khung ve rieng (de doi mau theo LINK) + vung bam trong suot dat de len tren
  st_xyz_frame = lv_obj_create(p);
  lv_obj_set_pos(st_xyz_frame, MR_XYZ_X, MR_XYZ_Y);
  lv_obj_set_size(st_xyz_frame, MR_XYZ_W, MR_XYZ_H);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(st_xyz_frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(st_xyz_frame, 1, 0);
  lv_obj_set_style_border_color(st_xyz_frame, MR_AXIS, 0);
  lv_obj_set_style_radius(st_xyz_frame, 0, 0);
  lv_obj_set_style_pad_all(st_xyz_frame, 0, 0);

  lv_obj_t *xyz_hit = mk_hit(p, MR_XYZ_X, MR_XYZ_Y, MR_XYZ_W, MR_XYZ_H,
                             cb_action, (void *)(intptr_t)A_GOTO_JOG);
  static const char *const AX_L[3] = {"X", "Y", "Z"};
  for (uint8_t i = 0; i < 3; i++) {
    lv_obj_t *n = mk_label(xyz_hit, AX_L[i], MR_AXIS);
    lv_obj_set_pos(n, MR_XYZ_LX(i), MR_XYZ_TY);
    st_axis_val[i] = mk_label_right(xyz_hit, MR_XYZ_LX(i), MR_XYZ_TY, MR_XYZ_VW(i),
                                    "-", MR_AXIS);
    // FONT TO HON cho hang X/Y/Z (yeu cau nguoi dung 2026-10-15: "font X,Y,Z o trang status to hon 1 chut").
    // Dung Montserrat 16 (lv_conf.h: LV_FONT_MONTSERRAT_16): line_height 19 px + le tren MR_XYZ_TY=3 = 22
    // <= o 24 px => van vua, khong phai doi bo cuc. Ap cho CA nhan truc (X/Y/Z) LAN gia tri so.
    lv_obj_set_style_text_font(n, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(st_axis_val[i], &lv_font_montserrat_16, 0);
  }

  // ---- FR (54,136,68,32) va FL (192,136,68,32): icon 32x32 + gia tri o x=32,y=6 ----
  // Marlin goc: FR (70,136,80,32) / FL (170,136,80,32). Nay THU HEP 80 -> 68 va doi x de nhuong
  // cho icon WiFi 30 px + chu dBm nam GIUA 2 khoi (2026-10-14, xem ui_chrome.cpp: ST_WIFI_X):
  //      the SD 0..47 | FR 54..121 | WiFi 128..157 + "-58" 161..185 | FL 192..259 | SETTINGS 272..319
  // Gia tri % (4 ky tu mono = 32 px) van nam gon trong khoi 68 px: x = 32..63 <= 67.
  static const lv_img_dsc_t *const RATE_ICON[2] = { &ui_icon_feedrate, &ui_icon_flowrate };
  const lv_coord_t rate_x[2] = {ST_FR_X, ST_FL_X};
  for (uint8_t i = 0; i < 2; i++) {
    lv_obj_t *b = mk_hit(p, rate_x[i], MR_RATE_Y, ST_RATE_W, MR_RATE_H,
                         cb_action, (void *)(intptr_t)A_GOTO_RATE);
    st_rate_icon[i] = mk_icon(b, 0, MR_RATE_ICON_Y, RATE_ICON[i], MR_RATE100);
    st_rate_val[i]  = mk_label(b, "100%", MR_RATE100);
    lv_obj_set_pos(st_rate_val[i], 32, 6);
  }

  // ---- nut SD (0,138,48,48) va nut SETTINGS (272,138,48,48): THU NHO tu 64x64 cua Marlin ----
  // Nhuong 16 px moi ben cho icon WiFi o giua (xem khoi tren). Dung bitmap 48x48 SINH SAN
  // (ui_icon_sd_48 / ui_icon_settings_48, tools/make_lvgl_icons.py) chu KHONG zoom luc chay -
  // LVGL khong transform duoc ALPHA_4BIT (xem pick_icon()). Vung bam 48x48 van >= 44 px cho tay.
  lv_obj_t *sd = mk_hit(p, MR_SD_X, ST_TILE_Y, ST_TILE_SZ, ST_TILE_SZ,
                        cb_action, (void *)(intptr_t)A_GOTO_FILES);
  // Icon THE co SAN cua Marlin: giu nguyen hinh, chi THU NHO + DOI MAU theo trang thai the TF cua
  // MAN HINH CYD (xem status_sd_icon_mark()). Truoc day mau co dinh MR_TIME (khong cho nao khac doi).
  lv_obj_t *sd_icon = mk_icon(sd, 0, 0, pick_icon(&ui_icon_sd, &ui_icon_sd_48, ST_TILE_SZ), MR_TIME);

  lv_obj_t *set = mk_hit(p, ST_SET_X, ST_TILE_Y, ST_TILE_SZ, ST_TILE_SZ,
                         cb_action, (void *)(intptr_t)A_GOTO_MENU);
  mk_icon(set, 0, 0, pick_icon(&ui_icon_settings, &ui_icon_settings_48, ST_TILE_SZ), MR_FRAME);

  // ---- thoi gian in (96,176,128,20) + % (cho trong 224..256) ----
  st_time = mk_label_mid(p, MR_TIME_X, MR_TIME_Y, MR_TIME_W, "-", MR_TIME);
  st_pct  = mk_label(p, "0%", MR_TIME);
  lv_obj_set_pos(st_pct, MR_PCT_X, MR_TIME_Y);

  // ---- thanh tien do (4, 198, 312, 9): khung trang + ruot xanh (nhu Marlin) ----
  st_bar = lv_bar_create(p);
  lv_obj_set_pos(st_bar, MR_BAR_X, MR_BAR_Y);
  lv_obj_set_size(st_bar, MR_BAR_W, MR_BAR_H);
  lv_bar_set_range(st_bar, 0, 100);
  lv_bar_set_value(st_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBG, 0);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(st_bar, 1, 0);
  lv_obj_set_style_border_color(st_bar, MR_FRAME, 0);
  lv_obj_set_style_radius(st_bar, 0, 0);
  lv_obj_set_style_pad_all(st_bar, 1, 0);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBAR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(st_bar, 0, LV_PART_INDICATOR);

  // ---- dong trang thai / ten file (0, 216, 320, 20) ----
  st_msg = mk_label_w(p, "-", MR_MSG, MR_MSG_W);
  lv_obj_set_pos(st_msg, MR_MSG_X, MR_MSG_Y);
  lv_obj_set_style_text_align(st_msg, LV_TEXT_ALIGN_CENTER, 0);

  // ---- 2 icon trang thai: WiFi + chu dBm (nam trong KHE PHAI cua hang nhiet do, xem
  // ui_chrome.cpp: status_icons_create()) + luu lai icon THE co san cua Marlin de to mau theo
  // trang thai the TF cua MAN HINH CYD (xem khoi "Icon trang thai WiFi" o cuoi file).
  // Tao CUOI CUNG de ve de len tren; la con cua trang STATUS nen tu an/hien theo man.
  status_icons_create(p, sd_icon);
}

// Mau theo NHIET DO TUYET DOI (khong con chia theo target).
// YEU CAU NGUOI DUNG 2026-10-15: "khi nhiet do tang dan thi mau cua icon hotend va bed co chuyen dan sang do
// khong? min max hot-end la 10-250; bed 0-60" -> MOC TREN lay dung y (250 / 60), MOC DUOI keo len nhiet do
// phong MR_TEMP_AMB (xem chu thich o ui_lvgl_int.h: 0/10 thap hon nhiet do phong nen ban in nguoi 28.8 do C
// da o 48% dai = mau VANG du heater TAT).
// VI SAO KHONG dung cur/tgt nua: ban cu chia theo TARGET nen CUNG 1 nhiet do (vd 50 do C) se XANH khi target
// 250 nhung DO khi target 60 -> mau khong noi len "dang nong bao nhieu", va target nho (bed 60) thi do ngay
// o 60 do C. Nay mau bam NHIET DO THUC; con "dang cap nguon hay khong" da co icon DAC (co target) / VIEN
// (khong target) lo, nen khong can nhet them vao mau.
// Do thi 3 moc XANH -> VANG (giua dai) -> mau NONG: tron truc tiep xanh->do se di qua mau xam hoi (nhu bi
// "nhat mau") chu khong giong "nong len"; chen moc vang o giua cho ra cam giac nong dan ro rang.
// lv_color_mix(c1, c2, mix): mix = 255 -> c1, mix = 0 -> c2.
lv_color_t temp_ramp(const int16_t cur, const int16_t tmin, const int16_t tmax, const lv_color_t hot) {
  if (cur <= tmin) return MR_COLD;
  if (cur >= tmax) return hot;
  const int32_t u = (int32_t)(cur - tmin) * 255 / (int32_t)(tmax - tmin);   // 0..254: vi tri trong dai
  if (u <= 127) return lv_color_mix(MR_WARM, MR_COLD, (uint8_t)(u * 2));    // nua dau: xanh ... vang
  return lv_color_mix(hot, MR_WARM, (uint8_t)((u - 127) * 2));              // nua sau: vang ... NONG
}

void page_status_update(void) {
  if (!st_msg) return;                    // trang da bi thu hoi (an toan: xem page_status_destroy)
  char b[48], a1[12], a2[12], t1[10], t2[10];
  int16_t cur = 0, tgt = 0;
  const bool homed = (cydlcd.flags & CYDLCD_SF_HOMED) != 0;

  // ---- 3 o: icon + mau theo nhiet do VA theo trang thai CAP NGUON cua heater ----
  // YEU CAU NGUOI DUNG 2026-10-15: "neu marlin cap nguon cho hot end thi co bieu fill dac icon, mau cung
  // thay doi dan tu xanh den do; den luc ngung cap nguon thi chi ve border ma khong fill. Con bed thi...
  // luc cap nguon moi co [bieu tuong khoi]".
  //   - "cap nguon" = co TARGET (tgt > 0): dung y Marlin (man TFT: `targetTemperature > 0 ? imgBedHeated :
  //     imgBed`) va dung khi PID giu nhiet (luc do van con target).
  //   - hotend: co target -> icon DAC (ui_icon_hotend), khong target -> chi VIEN (ui_icon_hotend_line).
  //     Ban VIEN duoc SINH THEM trong tools/make_lvgl_icons.py (bitmap goc cua Marlin la hinh dac).
  //   - bed: co target -> ui_icon_bed_heated (dia + khoi), khong -> ui_icon_bed (chi dia).
  //   - mau: dai mau XANH -> VANG -> DO chay theo NHIET DO THUC (temp_ramp()), moc lay tu MR_TEMP_*_MIN/MAX
  //     (hotend 10-250, bed 0-60). Mau KHONG con phu thuoc "co target hay khong" -> luc nguoi di (heater tat
  //     nhung con 180 do C) icon la VIEN nhung van mau NONG, dung voi thuc te.
  for (uint8_t i = 0; i < ST_CELLS; i++) {
    lv_color_t col = MR_COLD;
    if (i == 2) {                                     // quat: icon theo toc do, chi 1 dong "%"
      uint8_t fp = 0;
      if (cydlcd.n_fans) fp = (uint8_t)(cydlcd.fans[0].pct / 100);
      snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
      set_text(st_cur[i], b);
      set_text(st_tgt[i], "");
      set_img_src(st_icon[i], (fp >= 50) ? &ui_icon_fan_fast : (fp ? &ui_icon_fan_slow : &ui_icon_fan));
    }
    else {                                            // dau in / ban in: so nguyen do C
      temp_lookup(i == 0 ? CYDLCD_TEMP_E0 : CYDLCD_TEMP_BED, &cur, &tgt);
      const bool powered = (tgt > 0);
      if (i == 0) set_img_src(st_icon[i], powered ? &ui_icon_hotend : &ui_icon_hotend_line);
      else        set_img_src(st_icon[i], powered ? &ui_icon_bed_heated : &ui_icon_bed);
      //   - mau: dai mau theo NHIET DO TUYET DOI (temp_ramp()), moc dau/cuoi lay tu MR_TEMP_*_MIN/MAX.
      // LUU Y DON VI: `cur` cua temp_lookup() la 1/100 do C (vd 3102 = 31.02 do C - xem `scale /100` o
      // UI TEMPS) con dai mau tinh bang DO C -> PHAI chia truoc, khong thi 3102 > 250 = luon mau NONG
      // (loi nay da tung xay ra: dau in o nhiet do phong hien DO ngay).
      const int16_t deg = (int16_t)(cur / 100);
      col = (i == 0) ? temp_ramp(deg, MR_TEMP_HOT_MIN, MR_TEMP_HOT_MAX, MR_HOTEND)
                     : temp_ramp(deg, MR_TEMP_BED_MIN, MR_TEMP_BED_MAX, MR_BEDHOT);
      fmt_scaled(a1, sizeof(a1), cur, 100, 0);
      fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
      set_text(st_cur[i], a1);
      set_text(st_tgt[i], a2);
    }
    icon_color(st_icon[i], col);
    set_text_color(st_cur[i], col);
    set_text_color(st_tgt[i], col);
  }

  // ---- XYZ: chua home -> "?" (Marlin nhay "?" mau AXIS_NOT_HOMED) ----
  const int32_t posv[3] = { cydlcd.pos.x, cydlcd.pos.y, cydlcd.pos.z };
  for (uint8_t i = 0; i < 3; i++) {
    if (!homed) {
      set_text(st_axis_val[i], "?");
      set_text_color(st_axis_val[i], MR_NOAXIS);
    }
    else {
      fmt_scaled(a1, sizeof(a1), posv[i], 1000, 2);
      set_text(st_axis_val[i], a1);
      set_text_color(st_axis_val[i], MR_AXIS);
    }
  }
  set_border_color(st_xyz_frame, link_ok() ? MR_AXIS : COL_ERR);

  // ---- FR / FL: xanh la khi 100%, vang khi khac (Marlin) ----
  const uint8_t rate[2] = { (uint8_t)(cydlcd.pos.feed_pct / 100), (uint8_t)(cydlcd.pos.flow_pct / 100) };
  for (uint8_t i = 0; i < 2; i++) {
    const lv_color_t col = (rate[i] == 100) ? MR_RATE100 : MR_RATEALT;
    snprintf(b, sizeof(b), "%u%%", (unsigned)rate[i]);
    set_text(st_rate_val[i], b);
    set_text_color(st_rate_val[i], col);
    icon_color(st_rate_icon[i], col);
  }

  // ---- thoi gian: da in / con lai ----
  fmt_time(t1, sizeof(t1), cydlcd.prog.elapsed_s);
  fmt_time(t2, sizeof(t2), cydlcd.prog.remaining_s);
  if (cydlcd.prog.remaining_s) snprintf(b, sizeof(b), "%s/%s", t1, t2);
  else snprintf(b, sizeof(b), "%s", t1);
  set_text(st_time, b);

  // ---- % + thanh tien do ----
  const uint8_t pct = (cydlcd.progress_pct > 100) ? 100 : cydlcd.progress_pct;
  snprintf(b, sizeof(b), "%u%%", (unsigned)pct);
  set_text(st_pct, b);
  if (lv_bar_get_value(st_bar) != (int32_t)pct) lv_bar_set_value(st_bar, (int32_t)pct, LV_ANIM_OFF);

  // ---- dong trang thai: mat lien ket > "STATE  ten file" ----
  if (!link_ok()) {
    set_text(st_msg, tbt.rx_active ? T(UI_T_ST_LINK_ERR) : T(UI_T_LINK_NONE));
    set_text_color(st_msg, COL_ERR);
  }
  else {
    if (cydlcd.prog.file[0]) snprintf(b, sizeof(b), "%s  %s", state_name(cydlcd.state), cydlcd.prog.file);
    else snprintf(b, sizeof(b), "%s", state_name(cydlcd.state));
    set_text(st_msg, b);
    set_text_color(st_msg, MR_MSG);
  }
}

// Huy man STATUS (xem quy uoc o ui_lvgl_int.h): chi dat NULL con tro widget CUA TRANG - container
// trang do show_page() xoa bang lv_obj_del() nen LVGL xoa luon ca cay con (khong xoa o day).
//
// LUU Y: show_page() KHONG BAO GIO goi ham nay. STATUS duoc GIU LAI suot vong doi firmware:
//   - la trang goc (man hinh dau tien) va la noi MOI nut BACK tro ve;
//   - icon WiFi + chu dBm + icon the SD tren no duoc cap nhat dinh ky (~400 ms) tu ui_lvgl_task()
//     cho MOI trang (wifi_icon_mark/wifi_rssi_mark/status_sd_icon_mark) -> thu hoi se phai quen
//     ca 3 con tro do moi lan.
// Ham co mat cho du bo API (moi trang deu co page_<ten>_destroy) va de neu sau nay thu hoi ca
// STATUS thi khong bo sot con tro nao.
void page_status_destroy(void) {
  for (uint8_t i = 0; i < ST_CELLS; i++) {
    st_cell[i] = NULL; st_icon[i] = NULL; st_tgt[i] = NULL; st_cur[i] = NULL;
  }
  for (uint8_t i = 0; i < 3; i++) st_axis_val[i] = NULL;
  st_xyz_frame = NULL;
  for (uint8_t i = 0; i < 2; i++) { st_rate_val[i] = NULL; st_rate_icon[i] = NULL; }
  st_time = NULL; st_pct = NULL; st_bar = NULL; st_msg = NULL;
  // 2 icon trang thai nam trong kho cua ui_chrome.cpp (cap nhat dinh ky o MOI trang) -> phai quen.
  status_icons_forget();
}

#endif  // CYDLCD_USE_LVGL
