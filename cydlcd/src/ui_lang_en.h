/**
 * ui_lang_en.h -- BANG CHUOI TIENG ANH (ngon ngu thu 2 cua panel).
 *
 * ⚠️ Xem ghi chu dau `ui_lang_vi.h` (khong include o dau khac ngoai `ui_lang.cpp`).
 * ⚠️ TEN MAN (UI_T_PT_*) duoc chon NGAN co y do: o ten man tren dong tieu de co be rong CO DINH
 * `TITLE_W` = 84 px (xem ui_chrome.cpp); ten dai hon se tu cuon trong o nen tranh dat ten dai.
 */
#pragma once
#include "ui_lang.h"

const char *const UI_STR_EN[UI_T_COUNT] = {
  // ⚠️ PHAI co dong cho chi so 0 (xem ghi chu o ui_lang_vi.h): thieu chi so DAU TIEN thi GCC bao
  // "non-trivial designated initializers not supported".
  [UI_T_NONE]               = nullptr,

  // ---- ui_chrome.cpp ----
  [UI_T_CHROME_BACK]        = "BACK",
  [UI_T_CHROME_PREV]        = "PREV",
  [UI_T_CHROME_NEXT]        = "NEXT",
  [UI_T_CHROME_PAGE_OF]     = "PAGE %d/%d",

  // ---- ui_lvgl.cpp: link ----
  [UI_T_LINK_NONE]          = "NO LINK",
  [UI_T_LINK_ERR]           = "LINK ERR",

  // ---- ten man (dong tieu de; giu NGAN) ----
  [UI_T_PT_STATUS]          = "STATUS",
  [UI_T_PT_MENU]            = "MENU",
  [UI_T_PT_RATE]            = "FEED/FLOW",
  [UI_T_PT_TEMP]            = "TEMP",
  [UI_T_PT_JOG]             = "JOG",
  [UI_T_PT_FILES]           = "MEDIA",
  [UI_T_PT_SETTINGS]        = "SETTINGS",
  [UI_T_PT_MESH]            = "MESH",
  [UI_T_PT_CUSTOM]          = "CUSTOM",
  [UI_T_PT_TBT]             = "TRAMMING",
  [UI_T_PT_INFO]            = "INFO",
  [UI_T_PT_ENDSTOP]         = "ENDSTOP",
  [UI_T_PT_WIFI]            = "WIFI",
  [UI_T_PT_SUBMENU]         = "CONFIG",

  // ---- dong MENU (da la tieng Anh san -> giu NGUYEN, tru dong "Calibrate touch") ----
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
  [UI_T_MENU_CALTOUCH]      = "Calibrate touch (hold 1.5s)",

  // ---- cot gia tri cua MENU ----
  [UI_T_MENU_TBT_READY]     = "ready",
  [UI_T_MENU_TBT_NOLINK]    = "link lost",
  [UI_T_MENU_TBT_OFF]       = "off",
  [UI_T_MENU_WIFI_OFF]      = "OFF",
  [UI_T_MENU_WIFI_CONN]     = "connecting",
  [UI_T_MENU_WIFI_OK]       = "CONNECTED",
  [UI_T_VALUE_NA]           = "n/a",
  [UI_T_VALUE_ITEMS]        = "%u items",
  [UI_T_CAL_NVS]            = "NVS",
  [UI_T_CAL_DEFAULT]        = "default",
  [UI_T_CAL_RELEASE]        = "RELEASE",
  [UI_T_CAL_HOLDING]        = "holding",

  // ---- ten ngon ngu (endonym) ----
  [UI_T_LANG_VI]            = "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t",
  [UI_T_LANG_EN]            = "English"
};
