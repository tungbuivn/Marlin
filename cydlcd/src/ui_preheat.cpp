/**
 * ui_preheat.cpp -- KHO PRESET PREHEAT (PLA/PETG/ABS) cua MAN HINH CYD.
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * VI SAO kho nay nam o MAN HINH CYD (khong phai Marlin): ban Marlin nay KHONG co menu
 * MarlinUI 12864 nen `HAS_PREHEAT = 0` -> M145 khong duoc bien dich va bang preheat khong nam
 * trong `settings_t`. Bat lai HAS_PREHEAT se doi kich thuoc/CRC EEPROM -> Marlin nap MAC DINH
 * (mat M851/PID/M92/M201/M203/M205/M906 va MESH UBL). Vi vay preset duoc giu trong NVS cua man
 * hinh CYD (namespace "ph") va hien/sua duoc o trang SETTINGS nho "id cuc bo" 0x40xx.
 *
 * File nay la NOI DUY NHAT giu mang `s_pre_h/s_pre_b/s_pre_f`:
 *   - ui_preheat_get()/ui_preheat_set()/ui_preheat_label() la API CONG KHAI (ui_lvgl.h);
 *   - preheat_load()/preheat_save()/preheat_inject() duoc ui_page_settings.cpp (trang SETTINGS) + luc
 *     khoi dong) va chinh file nay goi -> khai bao trong ui_lvgl_int.h;
 *   - trang TEMP (ui_page_temp.cpp) doc qua API cong khai, KHONG cham thang mang.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf (khoa NVS "h0"/"b0"/"f0")
#include <Arduino.h>                // Serial.printf (WIFILOG)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  PREHEAT presets (PLA/PETG/ABS) -- MAN HINH CYD TU LUU (NVS), khong phai Marlin (2026-10-14)
//
//  VI SAO KHONG DUNG M145 CUA MARLIN: ban nay KHONG co menu MarlinUI 12864 nen
//  `HAS_PREHEAT = 0` (Conditionals_post.h: `HAS_TEMPERATURE && ANY(HAS_MARLINUI_MENU, ...)`) =>
//    - `M145` KHONG duoc bien dich (gui lenh do se la "Unknown command"),
//    - bang preheat khong nam trong `settings_t` (khong co trong EEPROM).
//  Bat lai HAS_PREHEAT thi `settings_t` DOI KICH THUOC -> version/CRC EEPROM doi -> Marlin nap
//  MAC DINH => MAT M851 (Z offset), PID, M92/M201/M203/M205/M906 va MESH UBL dang luu. Vi vay preset
//  duoc giu trong NVS CUA MAN HINH CYD (namespace "ph") va van hien/sua duoc o trang SETTINGS nho "id cuc
//  bo" 0x40xx (Marlin chi dung id 1..~50 nen khong trung).
//
//  Nut PLA/PETG/ABS o man TEMP dung CHINH cac gia tri nay (khong con hardcode 200/230/240).
//  Duong ghi NVS chi co tren CYD (ESP32 co Preferences) - board khac: chi giu trong RAM.
// ===========================================================================
// CYDLCD_PRE_COUNT / CYDLCD_PRE_ID_BASE / PRE_H_MAX / PRE_B_MAX / PRE_F_MAX va comment gioi han
// ngay tren da chuyen sang ui_lvgl_int.h (dung cheo file: ui_page_settings.cpp - trang SETTINGS - va
// ui_page_temp.cpp). Kho du lieu `s_pre_*` VAN chi nam trong file nay.

static const char *const PRE_LABEL[CYDLCD_PRE_COUNT] = { "PLA", "PETG", "ABS" };
// Mac dinh = dung Configuration.h cua may nay (PREHEAT_1/2/3: 200/60, 230/60, 240/60; FAN_SPEED 0)
static const uint16_t PRE_H_DEF[CYDLCD_PRE_COUNT] = { 200, 230, 240 };
static const uint16_t PRE_B_DEF[CYDLCD_PRE_COUNT] = {  60,  60,  60 };
static const uint8_t  PRE_F_DEF[CYDLCD_PRE_COUNT] = {   0,   0,   0 };

static uint16_t s_pre_h[CYDLCD_PRE_COUNT] = { 200, 230, 240 };
static uint16_t s_pre_b[CYDLCD_PRE_COUNT] = {  60,  60,  60 };
static uint8_t  s_pre_f[CYDLCD_PRE_COUNT] = {   0,   0,   0 };


#if defined(CYDLCD_BOARD_CYD)
  #include <Preferences.h>
  #define PRE_NVS_NS "ph"
#endif

void preheat_load(void) {
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  if (p.begin(PRE_NVS_NS, true)) {                   // read-only
    char k[4];
    for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
      snprintf(k, sizeof(k), "h%u", (unsigned)i);
      s_pre_h[i] = (uint16_t)p.getUShort(k, PRE_H_DEF[i]);
      snprintf(k, sizeof(k), "b%u", (unsigned)i);
      s_pre_b[i] = (uint16_t)p.getUShort(k, PRE_B_DEF[i]);
      snprintf(k, sizeof(k), "f%u", (unsigned)i);
      s_pre_f[i] = (uint8_t)p.getUChar(k, PRE_F_DEF[i]);
    }
    p.end();
  }
#endif
  // Chan gia tri rac (NVS hong / ghi tu ban khac)
  for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
    if (s_pre_h[i] > PRE_H_MAX) s_pre_h[i] = PRE_H_DEF[i];
    if (s_pre_b[i] > PRE_B_MAX) s_pre_b[i] = PRE_B_DEF[i];
    if (s_pre_f[i] > PRE_F_MAX) s_pre_f[i] = PRE_F_DEF[i];
  }
  WIFILOG("[PRE] preset: %s %u/%u/%u | %s %u/%u/%u | %s %u/%u/%u  (nozzle/bed/fan, sua o SETTINGS)\n",
          PRE_LABEL[0], (unsigned)s_pre_h[0], (unsigned)s_pre_b[0], (unsigned)s_pre_f[0],
          PRE_LABEL[1], (unsigned)s_pre_h[1], (unsigned)s_pre_b[1], (unsigned)s_pre_f[1],
          PRE_LABEL[2], (unsigned)s_pre_h[2], (unsigned)s_pre_b[2], (unsigned)s_pre_f[2]);
}

void preheat_save(void) {
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  if (p.begin(PRE_NVS_NS, false)) {                  // read-write
    char k[4];
    for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
      snprintf(k, sizeof(k), "h%u", (unsigned)i); p.putUShort(k, s_pre_h[i]);
      snprintf(k, sizeof(k), "b%u", (unsigned)i); p.putUShort(k, s_pre_b[i]);
      snprintf(k, sizeof(k), "f%u", (unsigned)i); p.putUChar(k, s_pre_f[i]);
    }
    p.end();
    WIFILOG("[PRE] da luu 3 preset preheat vao NVS (%s)\n", PRE_NVS_NS);
  }
#endif
}

// Bom 9 gia tri vao bang `cydlcd.settings[]` de trang SETTINGS hien/sua duoc (xem cydlcd_setting_set).
// Goi lai duoc nhieu lan (idempotent) - Marlin KHONG bao gio gui cac id nay.
void preheat_inject(void) {
  const uint8_t before = cydlcd.n_settings;
  for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
    const uint16_t base = (uint16_t)(CYDLCD_PRE_ID_BASE + 1 + i * 3);
    cydlcd_setting_set(base,     (int32_t)s_pre_h[i] * 100);   // scale 100 (do C)
    cydlcd_setting_set(base + 1, (int32_t)s_pre_b[i] * 100);
    cydlcd_setting_set(base + 2, (int32_t)s_pre_f[i]);         // scale 1  (0..255)
  }
  // Chi in khi bang THAT SU duoc them dong (tranh spam: ham nay chay moi lan cap nhat trang).
  if (cydlcd.n_settings != before)
    WIFILOG("[PRE] da them 9 dong preset vao bang SETTINGS (%u -> %u dong)\n",
            (unsigned)before, (unsigned)cydlcd.n_settings);
}
// ===========================================================================
//  API PRESET PREHEAT (khai bao trong ui_lvgl.h) - cho console COM7 va test
//  Kho du lieu nam trong khoi "PREHEAT presets" o phan SETTINGS (NVS namespace "ph").
// ===========================================================================
const char *ui_preheat_label(uint8_t i) { return (i < CYDLCD_PRE_COUNT) ? PRE_LABEL[i] : ""; }

bool ui_preheat_get(uint8_t i, uint16_t *nozzle, uint16_t *bed, uint8_t *fan) {
  if (i >= CYDLCD_PRE_COUNT) return false;
  if (nozzle) *nozzle = s_pre_h[i];
  if (bed)    *bed    = s_pre_b[i];
  if (fan)    *fan    = s_pre_f[i];
  return true;
}

bool ui_preheat_set(uint8_t i, uint16_t nozzle, uint16_t bed, uint8_t fan) {
  if (i >= CYDLCD_PRE_COUNT) return false;
  if (nozzle > PRE_H_MAX) nozzle = PRE_H_MAX;
  if (bed    > PRE_B_MAX) bed    = PRE_B_MAX;
  if (fan    > PRE_F_MAX) fan    = PRE_F_MAX;
  s_pre_h[i] = nozzle;
  s_pre_b[i] = bed;
  s_pre_f[i] = fan;
  preheat_save();          // NVS (CYD) - board khac chi giu RAM
  preheat_inject();        // cap nhat bang thong so de trang SETTINGS thay ngay
  tp_pre_sync();           // + nhan nut PLA/PETG/ABS o man TEMP
  WIFILOG("[PRE] dat %s: nozzle=%u bed=%u fan=%u\n", PRE_LABEL[i],
          (unsigned)s_pre_h[i], (unsigned)s_pre_b[i], (unsigned)s_pre_f[i]);
  return true;
}

#endif  // CYDLCD_USE_LVGL
