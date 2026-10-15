/**
 * ui_lang_vi.h -- BANG CHUOI TIENG VIET (ngon ngu MAC DINH cua panel).
 *
 * ⚠️ KHONG `#include` file nay o dau khac ngoai `ui_lang.cpp` (no dinh nghia 1 mang `const` CUA RIENG
 * file do; include o nhieu noi se tao nhieu ban -> ton flash).
 * ⚠️ CHUOI O DAY PHAI GIONG Y CHUOI DANG HIEN TREN MAY (2026-10-15 chuyen sang i18n): chi DOI CACH LAY
 * (qua `T(id)`), KHONG doi chu - de viec chuyen doi khong lam thay doi giao dien tieng Viet.
 * ⚠️ Dung KHOI TAO CO TEN `[UI_T_...] = "..."` -> thu tu khong quan trong, thieu dong nao thi
 * `tools/check_lang.py` bao loi.
 */
#pragma once
#include "ui_lang.h"

const char *const UI_STR_VI[UI_T_COUNT] = {
  // ⚠️ PHAI co dong cho chi so 0: GCC (xtensa, -std=gnu++11) bao "non-trivial designated initializers
  // not supported" khi khoi tao CO TEN ma THIEU chi so DAU TIEN. Giu NULL o day cung dung y nghia
  // "UI_T_NONE = khong co chuoi".
  [UI_T_NONE]               = nullptr,

  // ---- ui_chrome.cpp ----
  [UI_T_CHROME_BACK]        = "BACK",
  [UI_T_CHROME_PREV]        = "PREV",
  [UI_T_CHROME_NEXT]        = "NEXT",
  [UI_T_CHROME_PAGE_OF]     = "TRANG %d/%d",

  // ---- ui_lvgl.cpp: link ----
  [UI_T_LINK_NONE]          = "NO LINK",
  [UI_T_LINK_ERR]           = "LINK ERR",

  // ---- ten man (dong tieu de) ----
  [UI_T_PT_STATUS]          = "STATUS",
  [UI_T_PT_MENU]            = "MENU",
  [UI_T_PT_RATE]            = "FEED/FLOW",
  [UI_T_PT_TEMP]            = "NHIET DO",
  [UI_T_PT_JOG]             = "JOG",
  [UI_T_PT_FILES]           = "FILE SD",
  [UI_T_PT_SETTINGS]        = "THONG SO",
  [UI_T_PT_MESH]            = "BED MESH",
  [UI_T_PT_CUSTOM]          = "CUSTOM",
  [UI_T_PT_TBT]             = "TRAMMING",
  [UI_T_PT_INFO]            = "INFO",
  [UI_T_PT_ENDSTOP]         = "ENDSTOP",
  [UI_T_PT_WIFI]            = "WIFI",
  [UI_T_PT_SUBMENU]         = "CAU HINH",

  // ---- dong MENU ----
  [UI_T_MENU_TEMP]          = "Temperature",
  [UI_T_MENU_MOTION]        = "Motion",
  [UI_T_MENU_RATE]          = "Feed/Flow rate",
  [UI_T_MENU_FILES]         = "Print from Media",
  [UI_T_MENU_MESH]          = "Bed mesh",
  [UI_T_MENU_CUSTOM]        = "Custom/Init SD",
  [UI_T_MENU_CONFIG]        = "Configuration",
  [UI_T_MENU_TBT]           = "Tramming wizard",
  [UI_T_MENU_INFO]          = "Info",
  [UI_T_MENU_WIFI]          = "WiFi",
  [UI_T_MENU_CALTOUCH]      = "Calibrate touch (giu 1.5s)",

  // ---- cot gia tri cua MENU ----
  [UI_T_MENU_TBT_READY]     = "san sang",
  [UI_T_MENU_TBT_NOLINK]    = "mat link",
  [UI_T_MENU_TBT_OFF]       = "chua bat",
  [UI_T_MENU_WIFI_OFF]      = "TAT",
  [UI_T_MENU_WIFI_CONN]     = "dang noi",
  [UI_T_MENU_WIFI_OK]       = "DA NOI",
  [UI_T_VALUE_NA]           = "n/a",
  [UI_T_VALUE_ITEMS]        = "%u muc",
  [UI_T_CAL_NVS]            = "NVS",
  [UI_T_CAL_DEFAULT]        = "mac dinh",
  [UI_T_CAL_RELEASE]        = "NHAC TAY",
  [UI_T_CAL_HOLDING]        = "dang giu",

  // ---- ten ngon ngu (endonym) ----
  // "Tiếng Việt" viet bang MA UTF-8 (\x..) de file nguon giu duoc ASCII thuan - dung quy uoc cua
  // project (xem ghi chu o ui_lvgl.cpp). Font Segoe UI 16 co du dau tieng Viet (dai vn1..vn4).
  [UI_T_LANG_VI]            = "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t",
  [UI_T_LANG_EN]            = "English"
};
