/**
 * lv_conf_panel.h -- Cau hinh LVGL RIENG cho env panel_esp32_cyd (board CYD).
 *
 * VI SAO CO FILE NAY (do that, khong phai phong doan):
 *   Board CYD (ESP32, KHONG PSRAM) chi co 124.580 byte DRAM cho BIEN TINH (vung
 *   `dram0_0_seg` trong tools/sdk/esp32/ld/memory.ld:66-67 =
 *   [0x3FFBDB5C, 0x3FFDC200], len = 0x2C200 - 0xDB5C). Khi bat them WiFi + Bluetooth
 *   Classic (module nhan file BT -> ESP3D), cac thu vien SDK mang theo ~38 KB .bss/.data
 *   tinh (libbt 13,0 KB; libfreertos 5,0; liblwip 3,4; libmesh 2,9; libpp 2,9;
 *   libespcoredump 2,2; libnet80211 1,4; libcoexist 1,2; ...) nen linker bao:
 *       "region `dram0_0_seg' overflowed by 28144 bytes".
 *   Trong khi do 2 khoi tinh cua UI da an 73,6 KB: `work_mem_int` = LV_MEM_SIZE (48 KB,
 *   lv_mem.c) + buffer ve 320x40x2 = 25,6 KB trong ui_lvgl.cpp. Vi vay phai ha pool LVGL xuong
 *   24 KB (UI nay chay duoc voi 20 KB tren F411 - xem lv_conf.h:31 va README muc 1.1).
 *   Do that 2026-10-14: pool chi dung ~17,7 KB khi da mo vai man (`lvmem=` trong log 1 Hz).
 *
 * Cach lam: file nay KHONG copy lv_conf.h (de chi co MOT nguon cau hinh) - no include
 * lv_conf.h roi chi ghi de dung LV_MEM_SIZE. platformio.ini (env panel_esp32_cyd) truyen
 * `-DLV_CONF_PATH=lv_conf_panel.h` thay cho `-DLV_CONF_INCLUDE_SIMPLE`; lv_conf_internal.h
 * (LVGL 8.4, dong 31-35) uu tien LV_CONF_PATH nen moi file LVGL deu lay cau hinh nay.
 *
 * MUON TRA LAI 48 KB: phai tra lai DRAM o cho khac truoc, vi neu khong linker se FAIL. Cach
 * sach nhat (nhung thuoc file ui_lvgl.cpp - nhanh khac dang giu): cap phat buffer ve 25,6 KB
 * tu heap thay vi mang tinh, hoac giam xuong 320x20 (nhanh UI DA duoc giao viec nay). Sau do
 * sua LV_MEM_SIZE duoi day len va build lai.
 *
 * LIEN QUAN (README 9.15): `SerialBT.begin()` lam heap tut 124.324 -> 24.840 B (Bluedroid
 * Classic an ~99,5 KB) va **SPP server/SDP record co the dang ky THAT BAI vi thieu RAM**
 * (dien thoai/PC khong thay dich vu SPP) -> cang nhieu DRAM tra ve heap cang tot.
 */
#pragma once

#include "lv_conf.h"          /* cau hinh goc: nguon duy nhat, khong copy o day */

/* 48 KB -> 24 KB: nhuong 24.576 byte DRAM tinh cho WiFi + Bluetooth Classic + SPP/SDP. */
#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (24U * 1024U)

/* Them font 20px cho DONG TRANG THAI cua lop phu (init + PHA 2 - day len ESP3D).
 * lv_conf.h (dung chung) chi bat montserrat 14; o day CHI bat dung size can dung, va chi cho
 * env panel_esp32_cyd (env nay truyen -DLV_CONF_PATH=lv_conf_panel.h).
 * Font la `const` -> nam trong FLASH, KHONG ton them DRAM (chi ~30 KB flash; phan vung app
 * huge_app.csv 3 MB dang dung 57,6%). lv_conf_internal.h (LVGL 8.4) dung `#ifndef
 * LV_FONT_MONTSERRAT_20` cho tung font nen dinh nghia o day la du.
 * LUU Y: neu doi size khac (24/28...) thi bat DUNG size do, dung bat thua. */
#undef  LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 1

/* Bang phim + o nhap cho TRANG WIFI (nhap mat khau WiFi - xem ui_lvgl.cpp).
 * lv_conf.h (dung chung) de 0; o day chi bat cho env panel_esp32_cyd nen env STM32 khong ton them.
 * FLASH: widget lv_keyboard/lv_textarea + bang phim (const, nam trong flash) ~vai KB code.
 * RAM (pool LVGL 24 KB, do dung ~17,7 KB): keyboard + textarea chi duoc TAO khi nguoi dung bam
 * "MAT KHAU..." va XOA NGAY sau khi dong (xem wifi_kb_close) -> khong chiem pool thuong truc. */
#undef  LV_USE_KEYBOARD
#define LV_USE_KEYBOARD 1
#undef  LV_USE_TEXTAREA
#define LV_USE_TEXTAREA 1
