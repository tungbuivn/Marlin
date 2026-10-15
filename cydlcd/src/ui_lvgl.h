/**
 * ui_lvgl.h -- UI LVGL 8.3 cua MAN HINH CYD (kieu PUSH: Marlin day frame xuong, xem cydlcd_link.h).
 *
 * File nay khai bao 2 thu:
 *   1) UI:   ui_lvgl_begin() / ui_lvgl_task()  -- main.cpp goi (chi khi CYDLCD_USE_LVGL = 1).
 *   2) PORT: cac ham phu thuoc phan cung (ui_lvgl_port.cpp) ma UI can: init LCD, init input,
 *      millis, va 2 callback ma LVGL goi (flush pixel / doc input).
 *
 * Khi CYDLCD_USE_LVGL = 0 (F411/F103C6 -- UI tu ve) thi file nay KHONG khai bao gi ca,
 * nen main.cpp cu van build duoc ma khong keo theo LVGL.
 *
 * Man hinh (xem ui_page_*.cpp): STATUS / MENU / FEED-FLOW / TEMP / JOG / FILES / SETTINGS / MESH /
 * CUSTOM / TRAMMING / INFO / ENDSTOP / WIFI. Cac phan dung chung nam o ui_widgets.cpp (helper),
 * ui_chrome.cpp (tieu de + thanh day + icon trang thai), ui_boxes.cpp (hop thoai) va
 * ui_preheat.cpp (kho preset preheat).
 */
#pragma once

#include "cydlcd_config.h"

#if CYDLCD_USE_LVGL

#include <lvgl.h>

// ---------------------------------------------------------------------------
// Kich thuoc LCD: ca 2 board dich (2.4" tren F407/F411, 2.8" tren CYD) deu la
// ILI9341/ST7789 240x320. Port xoay 90 do: CYD dung TFT_eSPI setRotation(1),
// STM32 (tft_ili9341) dung TFT_ROTATION 1 -> man NGANG 320x240.
// Doi 2 so nay thi phai xem lai layout trong cac file ui_page_*.cpp.
// ---------------------------------------------------------------------------
#define UI_LCD_HOR_RES 320
#define UI_LCD_VER_RES 240
#define UI_LCD_LANDSCAPE 1

// ---- UI chinh (ui_lvgl.cpp) ----
void ui_lvgl_begin(void);
void ui_lvgl_task(void);

// ---- Chan doan UI qua console COM7 (main.cpp) -------------------------------
// Mo 1 trang UI theo TEN, khop voi tieu de trang (khong phan biet hoa/thuong), vi du "TRAMMING".
// `name` rong hoac "?" -> in danh sach trang + so thu tu (khong mo trang nao).
// Dung de tai hien loi UI (vd trang treo) ma khong can cham tay vao man hinh.
void ui_lvgl_show_page_name(const char *name);
uint8_t ui_lvgl_page_count(void);

// In BO CUC trang dang mo ra console COM7: toa do/kich thuoc tung vat con truc tiep + trang thai
// thanh day (man hinh CYD khong co framebuffer doc nguoc nen khong the chup anh tu PC).
// Dung de kiem tra mot man co bi tran, nut co nam sat day khong, roller cao bao nhieu px.
void ui_lvgl_dump_layout(void);

// Mo 1 hop thoai XAC NHAN rong (action khong lam gi) de KIEM TRA BO CUC hop thoai tu PC:
// hop thong bao phai nam GIUA man hinh. Dung voi lenh `UI TESTBOX` tren COM7.
void ui_lvgl_test_dialog(void);

// CHAN DOAN man FILE: ban su kien CHAM vao 1 dong cua TRANG dang xem (`UI TAP <0..4>`), vao nut
// PREV (`UI TAP P`) / NEXT (`UI TAP N`) / XOA (`UI TAP D`), bam OK hop xac nhan (`UI TAP K`, 2 lan =
// chay lenh that) hoac dem muc con cua thu muc dang chon (`UI TAP C`, khong xoa gi); khong tham so =
// in trang thai danh sach. Di DUNG duong su kien that nen kiem tra duoc tu PC: cham dong nao thi dong
// do co duoc chon + to sang khong, dang chon file nao, va xoa file/thu muc rong co chay khong.
void ui_lvgl_test_tap(const char *arg);

// CHAN DOAN man NHIET DO: ban su kien CHAM vao nut preset preheat thu `arg` (0=PLA, 1=PETG, 2=ABS) -
// lenh `UI PRE <k>` tren COM7. Di dung duong su kien that (cb_temp_preheat) nen kiem tra duoc tu PC:
// nut nay gui `M104 S<nozzle>` + `M140 S<bed>` theo preset luu trong NVS cua man hinh.
void ui_lvgl_test_preheat(const char *arg);

// CHAN DOAN nhiet do: in CA HAI nguon ma man hinh dang co de doi chieu - lenh `UI TEMPS` tren COM7
// (PUSH = frame CYDLCD_F_TEMPS tu Marlin; TRAM = 4 token nhiet do cuoi dong "TBT ..."), kem gia tri
// ma `temp_lookup()` dang dung de HIEN THI. Dung khi "nhiet hien thi khong dung".
void ui_lvgl_dump_temps(void);

// CHAN DOAN dai mau nhiet do: in bang mau ma icon + so cua man STATUS se dung tai tung nhiet do
// (lenh `UI GRAD` tren COM7) - hotend 10..250 va bed 0..60, kem chi so % vi tri trong dai. Tra loi
// "khi nhiet do tang dan thi mau co chuyen dan sang do khong" ma KHONG phai dot nong may.
void ui_lvgl_test_grad(void);

// CHAN DOAN duong PANEL -> MARLIN (lenh `UI HELLO` tren COM7): in `hello_flags` + so tham so/danh sach dang co
// roi xin lai REQ_FULL/SETTINGS/MESH. Dung khi flash lai panel ma `hello_flags` = 0 (trang BED MESH bao "khong
// ho tro", `n_settings` tut) - de biet panel co gui duoc REQUEST sang Marlin hay khong.
void ui_lvgl_test_hello(void);

// CHAN DOAN 2 CHIEU UART (lenh `UI RX` tren COM8): so frame da nhan theo loai + tuoi PONG + vi tri/FR-FL.
// Dung khi "bam nut tren man hinh ma may khong chay": PONG = 0 / tuoi lon nghia la chieu MAN HINH -> MARLIN
// hong (trong khi STATUS van den = chieu xuong van tot). Doi chieu voi `PANEL DIAG` in o COM4.
void ui_lvgl_test_rx(void);

// CHAN DOAN HIEU NANG (lenh `UI PERF` tren COM8): in thoi gian `lv_timer_handler()` moi vong lap (tb/max),
// so lan flush + TONG SO PIXEL da day ra SPI (biet co ve lai ca man khong), thoi gian doc cam ung, va bo nho
// LVGL. Goi 2 lan cach nhau N giay: lan 1 xoa bo dem, lan 2 doc so cua khoang do. Xem ui_lvgl_test_perf().
void ui_lvgl_test_perf(void);

// So lan ui_lvgl_task() chay >= 20 ms KE TU lan goi truoc (do NGAY TRONG ham, doc lap voi dong log o
// main.cpp) + thoi gian lan goi cuoi (us) + tong so lan cham. Dung de doi chieu khi dong log bao cham.
uint32_t ui_lvgl_perf_slow_take(void);
uint32_t ui_lvgl_perf_slow_all(void);
uint32_t ui_lvgl_perf_last_us(void);

// CHAN DOAN trang thai may: in `st`/`fl`/file dang in/tien do + **co cho nhan file upload khong**
// (lenh `UI BUSY` tren COM7, cung in kem trong `UI BOX`). Tra loi "lam sao biet may dang in?":
// upload CHI bi chan khi dang in / tam dung ban in TU THE SD; HOME/PROBE (G35) khong tinh la dang in.
void ui_lvgl_dump_busy(void);

// CHAN DOAN trang BED MESH (lenh `UI MCELL <cot> <hang>` tren COM8): mo hop sua o do Y NHU VUA CHAM vao o
// (khong tu gui M421) - de kiem tra hop sua / nut OK-HUY tu PC khi khong cham duoc man hinh.
void mesh_test_cell(const char *arg);

// CHAN DOAN bang SETTINGS (lenh `UI EDIT <id>` tren COM8): mo lop phu SUA cho dong co id do Y NHU VUA CHAM
// va in ra "Gia tri:" / "Buoc" dang hien (kiem tra dinh dang so tu PC). Xem id bang lenh `UI SET`.
void settings_test_edit(const char *arg);

// CHAN DOAN bang SETTINGS (lenh `UI SET` tren COM8): in tung dong (id, ten, gia tri format, raw) - de kiem
// tra tu PC xem Marlin co day thong so nao va man hinh dang hien gi (bang nam tren LCD, khong doc nguoc duoc).
void page_settings_dump(void);

// CHAN DOAN THONG BAO trang /status (lenh `UI NOTIFY [ms] <text>` tren COM8, ms=0 -> giu mai):
// day 1 thong bao vao kho cua trang /status (KHONG hien hop tren LCD) de kiem tra tu PC; khong tham so ->
// in thong bao dang luu + thoi gian con lai; text rong ("UI NOTIFY 0") -> xoa thong bao dang luu.
void ui_lvgl_test_notify(const char *arg);

// ---- PRESET PREHEAT (PLA/PETG/ABS) -----------------------------------------
// 3 preset do CHINH MAN HINH CYD luu (NVS namespace "ph") vi ban Marlin nay khong co M145/HAS_PREHEAT.
// Hien/sua duoc o trang SETTINGS (9 dong "PLA/PETG/ABS nozzle|bed|fan") va nut tren trang TEMP.
//   i = 0..2 (PLA/PETG/ABS). Gia tri: nozzle/bed = do C, fan = 0..255 (0 = khong dong tới quat).
//   ui_preheat_set() chan gia tri vao khoang hop le, ghi NVS va cap nhat bang thong so.
// Tra false neu i ngoai khoang.
bool ui_preheat_get(uint8_t i, uint16_t *nozzle, uint16_t *bed, uint8_t *fan);
bool ui_preheat_set(uint8_t i, uint16_t nozzle, uint16_t bed, uint8_t fan);
const char *ui_preheat_label(uint8_t i);      // "PLA" / "PETG" / "ABS" ("" neu i sai)

// ---- Port phan cung (ui_lvgl_port.cpp) ----
void     ui_port_display_init(void);        // init LCD (STM32: tft_ili9341, CYD: TFT_eSPI)
void     ui_port_input_init(void);          // init encoder (STM32) hoac chan touch (CYD)
uint32_t ui_port_millis(void);              // dong ho mili giay
bool     ui_port_input_is_encoder(void);    // true: dung encoder -> LV_INDEV_TYPE_ENCODER

// Hieu chuan cam ung 4 goc (CYD). Blocking, tu ve truc tiep bang TFT_eSPI (khong qua LVGL):
// hien ten goc o giua -> khi nhan cam ung thi ca man hinh doi sang XANH -> nguoi dung keo
// sat vao goc do, luc NHAC TAY chinh la luc mat cam ung -> lay cuc tri lam toa do goc.
// STM32 (khong co touch): ham rong.
void     ui_port_touch_calibrate(void);
bool     ui_port_touch_cal_available(void); // true: board co touch va ho tro hieu chuan
bool     ui_port_touch_cal_is_saved(void);  // true: dang dung hieu chuan da luu (NVS)

// Callback LVGL: UI dang ky 2 ham nay vao lv_disp_drv_t / lv_indev_drv_t
void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px);
void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

// ---- DO HIEU NANG (lenh `UI PERF` tren COM8) --------------------------------
// VI SAO CAN: "man hinh chay cham" co the do (a) LVGL ve lai qua nhieu (dien tich lon), (b) SPI cham,
// (c) doc cam ung cham, (d) vong lap bi chan o cho khac. Phai TACH RA do tung phan, khong doan.
// Port do giup 2 con so: so lan flush + TONG SO PIXEL da day ra SPI (+ thoi gian), va so lan doc cam ung
// (+ thoi gian). ui_lvgl_test_perf() in them thoi gian lv_timer_handler() moi vong.
// ui_port_perf_report() IN ra roi XOA bo dem (do theo tung khoang thoi gian).
void ui_port_perf_report(void);

#endif // CYDLCD_USE_LVGL
