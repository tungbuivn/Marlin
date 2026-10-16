/**
 * cydlcd_assert.cpp -- Cai dat handler assert cua LVGL (xem cydlcd_assert.h).
 *
 * Duoc LVGL goi qua `LV_ASSERT_HANDLER` (dinh nghia trong lv_conf_cydlcd.h). Dong thoi gian:
 * in thong bao + flush Serial roi `esp_restart()` -> board chay lai tu dau (nhanh, khong treo may).
 */
#include "cydlcd_assert.h"
#include "cydlcd_config.h"

// Chi board CYD (ESP32) dung lv_conf_cydlcd.h -> chi o do moi co LV_ASSERT_HANDLER tro vao day.
// Cac env STM32 (khong LVGL hoac LVGL rieng) van bien dich file nay thanh TU rong.
#if defined(CYDLCD_BOARD_CYD)

#include <Arduino.h>
#include <esp_system.h>

extern "C" void cyd_lvgl_assert_halt(void) {
  // Ghi y nguyen: day la loi LAP TRINH (khong phai loi nguoi dung) - phai thay trong log COM7.
  Serial.println();
  Serial.println("[CYD] !!! LVGL ASSERT -> KHOI DONG LAI (xem dong 'Asserted at expression' ngay tren)");
  Serial.println("[CYD] Goi y: pool LVGL het (LV_MEM_SIZE), con tro NULL, hoac loi logic trang UI.");
  Serial.flush();
  delay(100);                       // du thoi gian day het log ra USB
  esp_restart();
  for (;;) { }                      // khong bao gio toi day
}

#endif // CYDLCD_BOARD_CYD
