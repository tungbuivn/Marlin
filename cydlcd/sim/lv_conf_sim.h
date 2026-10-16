/**
 * lv_conf_sim.h -- cau hinh LVGL cho ban GIA LAP UI tren PC (cydlcd/sim).
 *
 * KHONG copy lv_conf.h: include ban that roi CHI ghi de nhung gi khac tren PC:
 *   - LV_MEM_SIZE: PC khong thieu RAM nhu ESP32 (48 KB trong lv_conf_cydlcd.h) -> nang len 256 KB
 *     de mo duoc NHIEU trang cung luc (moi trang UI duoc tao LAZY va khong bao gio giai phong).
 *   - Khong bat log/assert dac biet: tren PC thi `LV_USE_ASSERT_*` cua lv_conf.h la du (in ra stdout).
 *
 * Truyen vao build bang `-DLV_CONF_PATH=lv_conf_sim.h -Isim` (xem tools/run_sim.ps1).
 */
#pragma once

#include "lv_conf.h"

#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (256U * 1024U)

// Font 20 px cho DONG TRANG THAI cua lop phu upload: board CYD bat no trong lv_conf_cydlcd.h, con
// `lv_conf.h` dung chung (cho STM32) thi khong. Ban gia lap mo phong MAN HINH THAT (co WiFi/upload)
// nen bat o day - xem ghi chu o ui_overlay.cpp (OV_FONT_BIG).
#undef  LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 1
