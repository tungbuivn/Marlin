/**
 * lv_conf_cydlcd.h -- Cau hinh LVGL RIENG cho env cydlcd_esp32_cyd (board CYD).
 *
 * VI SAO CO FILE NAY (do that, khong phai phong doan):
 *   Board CYD (ESP32, KHONG PSRAM) chi co 124.580 byte DRAM cho BIEN TINH (vung
 *   `dram0_0_seg` trong tools/sdk/esp32/ld/memory.ld:66-67 =
 *   [0x3FFBDB5C, 0x3FFDC200], len = 0x2C200 - 0xDB5C). Khi bat them WiFi
 *   (truoc day bat ca Bluetooth Classic - DA XOA 2026-10-14), SDK mang theo ~38 KB .bss/.data
 *   tinh (libbt 13,0 KB; libfreertos 5,0; liblwip 3,4; libmesh 2,9; libpp 2,9;
 *   libespcoredump 2,2; libnet80211 1,4; libcoexist 1,2; ...) nen linker bao:
 *       "region `dram0_0_seg' overflowed by 28144 bytes".
 *   Trong khi do 2 khoi tinh cua UI da an 73,6 KB: `work_mem_int` = LV_MEM_SIZE (48 KB,
 *   lv_mem.c) + buffer ve 320x40x2 = 25,6 KB trong ui_lvgl.cpp. Vi vay phai ha pool LVGL xuong
 *   24 KB (UI nay chay duoc voi 20 KB tren F411 - xem lv_conf.h:31 va README muc 1.1).
 *   Do that 2026-10-14: pool chi dung ~17,7 KB khi da mo vai man (`lvmem=` trong log 1 Hz).
 *
 * Cach lam: file nay KHONG copy lv_conf.h (de chi co MOT nguon cau hinh) - no include
 * lv_conf.h roi chi ghi de dung LV_MEM_SIZE. platformio.ini (env cydlcd_esp32_cyd) truyen
 * `-DLV_CONF_PATH=lv_conf_cydlcd.h` thay cho `-DLV_CONF_INCLUDE_SIMPLE`; lv_conf_internal.h
 * (LVGL 8.4, dong 31-35) uu tien LV_CONF_PATH nen moi file LVGL deu lay cau hinh nay.
 *
 * MUON TRA LAI 48 KB: phai tra lai DRAM o cho khac truoc, vi neu khong linker se FAIL. Cach
 * sach nhat (nhung thuoc file ui_lvgl.cpp - nhanh khac dang giu): cap phat buffer ve 25,6 KB
 * tu heap thay vi mang tinh, hoac giam xuong 320x20 (nhanh UI DA duoc giao viec nay). Sau do
 * sua LV_MEM_SIZE duoi day len va build lai.
 *
 * LIEN QUAN (README 9.15): `SerialBT.begin()` lam heap tut 124.324 -> 24.840 B (Bluedroid
 * (truoc day con bat ca Bluetooth Classic - DA XOA 2026-10-14) va **thieu RAM**
 * cho WiFi -> cang nhieu DRAM tra ve heap cang tot.
 */
#pragma once

#include "lv_conf.h"          /* cau hinh goc: nguon duy nhat, khong copy o day */

/* 24 KB -> 48 KB (2026-10-14): Bluetooth Classic DA XOA nen DRAM du cho pool 48 KB tro lai.
 *
 * VI SAO PHAI NANG (day la NGUYEN NHAN THAT cua "panel treo im lang" khi mo Tramming wizard):
 *   - LVGL 8.4 bat san `LV_USE_ASSERT_NULL` + `LV_USE_ASSERT_MALLOC` va handler mac dinh la
 *     `#define LV_ASSERT_HANDLER while(1);` (lv_conf_internal.h:741-745) -> khi pool HET,
 *     `lv_mem_alloc()` tra NULL -> assert -> vong lap vo tan: panel dung im, KHONG in log,
 *     khong tra loi console, WiFi/http chet (dung trieu chung da gap).
 *   - Cac trang UI duoc tao LAZY va KHONG bao gio giai phong (show_page -> create_page mot lan),
 *     nen mo dan nhieu trang (MENU -> trang thu 8 la Tramming wizard) la pool 24 KB day.
 * Ngoai viec nang pool, file nay con:
 *   - bat `LV_USE_LOG` + handler assert RIENG (cydlcd_assert.h) de loi IN RA COM7 va TU KHOI DONG
 *     LAI thay vi treo im lang;
 *   - (main.cpp) in muc dung pool LVGL ra log 1 Hz (`lvmem=used/total frag=x%`) de theo doi. */
// 2026-10-17 (don DRAM): 48 KB -> 44 KB.
// SO DO TRUOC KHI HA (do tren ban gia lap, mo LAN LUOT ca 14 trang roi doc `lvmem=`):
//   used = 34.752 B khi mo du trang (2 trang cung ton tai: STATUS giu lai + trang dang mo).
//   40 KB chi con 5,2 KB du (~15%) -> QUA SAT: het pool thi assertion cua LVGL goi handler trong
//   cydlcd_assert.h = TU KHOI DONG LAI CYD -> Marlin mat nguon sector -> CHET MAY IN.
//   44 KB con 9,3 KB du (~21%) -> chap nhan duoc.
#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (44U * 1024U)

/* Log LVGL: chi muc ERROR (assert in ra "Asserted at expression: ...") -> vua du chan doan,
 * khong lam ngap log 1 Hz. `LV_LOG_PRINTF 1` = dung printf (Serial cua ESP32). */
#undef  LV_USE_LOG
#define LV_USE_LOG 1
#undef  LV_LOG_LEVEL
#define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR
#undef  LV_LOG_PRINTF
#define LV_LOG_PRINTF 1

/* Assert -> in ro + khoi dong lai (KHONG dung `while(1)` mac dinh cua LVGL). */
#define LV_ASSERT_HANDLER_INCLUDE "cydlcd_assert.h"
#undef  LV_ASSERT_HANDLER
#define LV_ASSERT_HANDLER cyd_lvgl_assert_halt();

/* Them font 20px cho DONG TRANG THAI cua lop phu (init + luc ghi vao the SD).
 * lv_conf.h (dung chung) chi bat montserrat 14; o day CHI bat dung size can dung, va chi cho
 * env cydlcd_esp32_cyd (env nay truyen -DLV_CONF_PATH=lv_conf_cydlcd.h).
 * Font la `const` -> nam trong FLASH, KHONG ton them DRAM (chi ~30 KB flash; phan vung app
 * huge_app.csv 3 MB dang dung 57,6%). lv_conf_internal.h (LVGL 8.4) dung `#ifndef
 * LV_FONT_MONTSERRAT_20` cho tung font nen dinh nghia o day la du.
 * LUU Y: neu doi size khac (24/28...) thi bat DUNG size do, dung bat thua. */
#undef  LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 1

/* Bang phim + o nhap cho TRANG WIFI (nhap mat khau WiFi - xem ui_page_wifi.cpp).
 * lv_conf.h (dung chung) de 0; o day chi bat cho env cydlcd_esp32_cyd nen env STM32 khong ton them.
 * FLASH: widget lv_keyboard/lv_textarea + bang phim (const, nam trong flash) ~vai KB code.
 * RAM (pool LVGL 24 KB, do dung ~17,7 KB): keyboard + textarea chi duoc TAO khi nguoi dung bam
 * "MAT KHAU..." va XOA NGAY sau khi dong (xem wifi_kb_close) -> khong chiem pool thuong truc. */
#undef  LV_USE_KEYBOARD
#define LV_USE_KEYBOARD 1
#undef  LV_USE_TEXTAREA
#define LV_USE_TEXTAREA 1

/* NHIP VE: tra ve mac dinh cua lv_conf.h (LV_DISP_DEF_REFR_PERIOD 30 ms = 33 Hz, LV_INDEV_DEF_READ_PERIOD 20 ms).
 * 2026-10-15: da thu 16 ms (60 Hz) nhung nguoi dung chot giu 30 Hz cho nhe CPU - xem ui_cmd_task() o ui_lvgl.cpp. */
