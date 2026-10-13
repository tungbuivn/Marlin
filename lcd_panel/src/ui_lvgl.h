/**
 * ui_lvgl.h -- UI LVGL 8.3 cua panel (kieu PUSH: Marlin day frame xuong, xem panel_link.h).
 *
 * File nay khai bao 2 thu:
 *   1) UI:   ui_lvgl_begin() / ui_lvgl_task()  -- main.cpp goi (chi khi PANEL_USE_LVGL = 1).
 *   2) PORT: cac ham phu thuoc phan cung (ui_lvgl_port.cpp) ma UI can: init LCD, init input,
 *      millis, va 2 callback ma LVGL goi (flush pixel / doc input).
 *
 * Khi PANEL_USE_LVGL = 0 (F411/F103C6 -- UI tu ve) thi file nay KHONG khai bao gi ca,
 * nen main.cpp cu van build duoc ma khong keo theo LVGL.
 *
 * Man hinh (xem ui_lvgl.cpp): STATUS / TEMP / JOG / FILES / SETTINGS / MESH / CUSTOM / TRAM / INFO.
 */
#pragma once

#include "panel_config.h"

#if PANEL_USE_LVGL

#include <lvgl.h>

// ---------------------------------------------------------------------------
// Kich thuoc LCD: ca 2 board dich (2.4" tren F407/F411, 2.8" tren CYD) deu la
// ILI9341/ST7789 240x320. Port xoay 90 do: CYD dung TFT_eSPI setRotation(1),
// STM32 (tft_ili9341) dung TFT_ROTATION 1 -> man NGANG 320x240.
// Doi 2 so nay thi phai xem lai layout trong ui_lvgl.cpp.
// ---------------------------------------------------------------------------
#define UI_LCD_HOR_RES 320
#define UI_LCD_VER_RES 240
#define UI_LCD_LANDSCAPE 1

// ---- UI chinh (ui_lvgl.cpp) ----
void ui_lvgl_begin(void);
void ui_lvgl_task(void);

// ---- PRESET PREHEAT (PLA/PETG/ABS) -----------------------------------------
// 3 preset do CHINH PANEL luu (NVS namespace "ph") vi ban Marlin nay khong co M145/HAS_PREHEAT.
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

#endif // PANEL_USE_LVGL
