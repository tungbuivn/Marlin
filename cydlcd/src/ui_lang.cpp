/**
 * ui_lang.cpp -- xu ly da ngon ngu (xem ghi chu thiet ke o `ui_lang.h`).
 *
 * File nay CO TINH KHONG phu thuoc LVGL (khong include ui_lvgl*.h) de board nao cung dung duoc; phan
 * "dung lai giao dien sau khi doi ngon ngu" nam o `ui_lang_apply_ui()` trong ui_lvgl.cpp.
 *
 * RAM TON: dung 1 byte (`s_lang`). Ca 2 bang chuoi la `const` -> FLASH.
 */
#include "cydlcd_config.h"
#include "ui_lang.h"

#if defined(CYDLCD_BOARD_CYD)
  #include <Preferences.h>
  #define LANG_NVS_NS  "ui"        // namespace NVS cua man hinh (chung voi cac tuy chon UI khac)
  #define LANG_NVS_KEY "lang"
#endif

// 2 bang chuoi (moi file dinh nghia 1 mang `const` CUA RIENG no -> include o day la duy nhat).
#include "ui_lang_vi.h"
#include "ui_lang_en.h"

// Ngong ngu dang dung. 1 byte -> khong dang ke voi DRAM da day.
static uint8_t s_lang = CYDLCD_LANG_DEFAULT;

static const char *const *lang_tbl(void) {
  return (s_lang == UI_LANG_EN) ? UI_STR_EN : UI_STR_VI;
}

const char *ui_t(ui_text_t id) {
  if (id <= UI_T_NONE || id >= UI_T_COUNT) return "?";
  const char *s = lang_tbl()[id];
  // Thieu ban dich -> roi ve TIENG VIET (ngon ngu goc) chu KHONG tra NULL: LVGL se doc NULL lam vo
  // chuoi/assert. Con thieu ca 2 -> "?" de nhin thay ngay tren man hinh.
  if (!s) s = UI_STR_VI[id];
  return s ? s : "?";
}

uint8_t ui_lang_get(void) { return s_lang; }

const char *ui_lang_code(const uint8_t idx) {
  switch (idx) {
    case UI_LANG_EN: return "en";
    default:         return "vi";
  }
}

// Ten ngon ngu hien cho nguoi dung: dung ENDONYM (chinh ten ngon ngu do viet bang ngon ngu do) nen
// KHONG dich - nho vay nguoi dung dang o ngon ngu nao cung tim thay duong doi lai.
const char *ui_lang_name(const uint8_t idx) {
  switch (idx) {
    case UI_LANG_EN: return UI_STR_EN[UI_T_LANG_EN];
    default:         return UI_STR_VI[UI_T_LANG_VI];
  }
}

void ui_lang_load(void) {
  #if defined(CYDLCD_BOARD_CYD)
    Preferences p;
    if (p.begin(LANG_NVS_NS, true)) {                    // read-only
      const uint8_t v = p.getUChar(LANG_NVS_KEY, (uint8_t)CYDLCD_LANG_DEFAULT);
      p.end();
      if (v < UI_LANG_COUNT) s_lang = v;
    }
  #endif
}

bool ui_lang_set(const uint8_t idx) {
  if (idx >= UI_LANG_COUNT) return false;
  const bool changed = (idx != s_lang);
  s_lang = idx;
  #if defined(CYDLCD_BOARD_CYD)
    Preferences p;
    if (p.begin(LANG_NVS_NS, false)) {                   // read-write
      p.putUChar(LANG_NVS_KEY, idx);
      p.end();
    }
  #endif
  return changed;
}

const char *ui_lang_str(const uint8_t lang, const ui_text_t id) {
  if (id <= UI_T_NONE || id >= UI_T_COUNT) return NULL;
  return (lang == UI_LANG_EN) ? UI_STR_EN[id] : UI_STR_VI[id];
}

uint16_t ui_lang_missing(const uint8_t lang) {
  uint16_t n = 0;
  for (uint16_t i = UI_T_NONE + 1; i < UI_T_COUNT; i++)
    if (!ui_lang_str(lang, (ui_text_t)i)) n++;
  return n;
}
