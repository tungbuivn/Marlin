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
 * Man hinh (xem ui_lvgl.cpp): STATUS / TEMP / JOG / FILES / SETTINGS / MESH / CUSTOM / INFO.
 */
#pragma once

#include "panel_config.h"

#if PANEL_USE_LVGL

#include <lvgl.h>

// ---------------------------------------------------------------------------
// Kich thuoc LCD: ca 2 board dich (2.4" tren F407/F411, 2.8" tren CYD) deu la
// ILI9341 240x320 o rotation 0 (portrait). Doi 2 so nay thi phai xem lai layout
// trong ui_lvgl.cpp va ca TFT_ROTATION / User_Setup.h.
// ---------------------------------------------------------------------------
#define UI_LCD_HOR_RES 240
#define UI_LCD_VER_RES 320

// ---- UI chinh (ui_lvgl.cpp) ----
void ui_lvgl_begin(void);
void ui_lvgl_task(void);

// ---- Port phan cung (ui_lvgl_port.cpp) ----
void     ui_port_display_init(void);        // init LCD (STM32: tft_ili9341, CYD: TFT_eSPI)
void     ui_port_input_init(void);          // init encoder (STM32) hoac chan touch (CYD)
uint32_t ui_port_millis(void);              // dong ho mili giay
bool     ui_port_input_is_encoder(void);    // true: dung encoder -> LV_INDEV_TYPE_ENCODER

// Callback LVGL: UI dang ky 2 ham nay vao lv_disp_drv_t / lv_indev_drv_t
void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px);
void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

#endif // PANEL_USE_LVGL
