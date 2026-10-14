/**
 * cydlcd_assert.h -- Handler assert RIENG cho LVGL tren man hinh CYD.
 *
 * VI SAO CO FILE NAY: LVGL 8.4 bat san `LV_USE_ASSERT_NULL` + `LV_USE_ASSERT_MALLOC`
 * (lv_conf_internal.h:689-708) nhung handler MAC DINH la `while(1);` (dong 741-745).
 * Gap assert (vi du pool LVGL het -> `lv_mem_alloc()` tra NULL) thi panel DUNG IM hoan toan:
 * khong in log, khong tra loi console COM7, WiFi/http chet - khong the chan doan tai cho.
 *
 * Thay vao do: in ro rang ra COM7 roi TU KHOI DONG LAI (self-recovery). Muc dich:
 *   - loi assert la LOI, phai ON AO chu khong duoc im lang (dung tinh than Marlin);
 *   - panel treo vi bat ky ly do gi cung tu hoi phuc sau vai giay (xem them task chan doan
 *     trong main.cpp: `cyd_diag_task()` khoi dong lai khi vong lap dung > 15 s).
 *
 * `cydlcd_assert.cpp` dinh nghia `cyd_lvgl_assert_halt()` voi linkage C (LVGL la C).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** In thong bao assert ra COM7 roi khoi dong lai board (khong bao gio tro ve). */
void cyd_lvgl_assert_halt(void);

#ifdef __cplusplus
}
#endif
