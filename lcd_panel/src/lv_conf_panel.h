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
 *   lv_mem.c) + buffer ve 320x40x2 = 25,6 KB trong ui_lvgl.cpp. Vi vay phai ha pool LVGL
 *   xuong 30 KB (UI nay chay duoc voi 20 KB tren F411 - xem lv_conf.h:31 va README muc 1.1).
 *
 * Cach lam: file nay KHONG copy lv_conf.h (de chi co MOT nguon cau hinh) - no include
 * lv_conf.h roi chi ghi de dung LV_MEM_SIZE. platformio.ini (env panel_esp32_cyd) truyen
 * `-DLV_CONF_PATH=lv_conf_panel.h` thay cho `-DLV_CONF_INCLUDE_SIMPLE`; lv_conf_internal.h
 * (LVGL 8.4, dong 31-35) uu tien LV_CONF_PATH nen moi file LVGL deu lay cau hinh nay.
 *
 * MUON TRA LAI 48 KB: phai tra lai DRAM o cho khac truoc, vi neu khong linker se FAIL. Cach
 * sach nhat (nhung thuoc file ui_lvgl.cpp - nhanh khac dang giu): cap phat buffer ve 25,6 KB
 * tu heap thay vi mang tinh, hoac giam xuong 320x20. Sau do sua LV_MEM_SIZE duoi day ve
 * (48U * 1024U) va build lai.
 */
#pragma once

#include "lv_conf.h"          /* cau hinh goc: nguon duy nhat, khong copy o day */

/* 48 KB -> 30 KB: nhuong 18.432 byte DRAM tinh cho WiFi + Bluetooth Classic. */
#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (30U * 1024U)
