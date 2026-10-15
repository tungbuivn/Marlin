/**
 * ui_lang_en.h -- BANG GHI DE TIENG ANH (chi nhung chuoi KHAC tieng Viet).
 *
 * ⚠️ KHONG `#include` file nay o dau khac ngoai `ui_lang.cpp`.
 *
 * VI SAO KHONG dung khoi tao co ten `[ID] = "..."` nhu bang tieng Viet:
 *   GCC cua xtensa (`-std=gnu++11`) CHI nhan khoi tao co ten khi cac chi so DI THEO THU TU TANG va
 *   LIEN MACH (khong co khoang trong) - da thu truc tiep tren compiler that:
 *       { [A0]=0, [A1]="a", [A3]="c" }   -> "sorry, unimplemented: non-trivial designated initializers"
 *       { [A0]=0, [A1]="a", [A2]="b" }   -> OK
 *   Bang tieng Viet la bang GOC day du nen LIEN MACH -> dung `[ID] = "..."` duoc (co ten cho de doc).
 *   Bang EN co tinh CHI GHI MOT SO dong -> se co khoang trong -> phai dung DANH SACH CAP {id, chuoi}
 *   (khong co khoi tao co ten nen khong bi rang buoc thu tu).
 *
 * CHINH SACH (xem chi tiet o ui_lang.h):
 *   - ID nao KHONG co trong danh sach nay thi `ui_t()` lay chuoi TIENG VIET -> dung y nghia "chuoi nay
 *     giong het o 2 ngon ngu" (phan lon ten trong menu Configuration la thuat ngu tieng Anh dung chung).
 *   - Sau MOI batch phai chay: `python tools/check_lang.py --fallback` de soi lai danh sach dung chung,
 *     tranh de lot chu tieng Viet vao ban tieng Anh.
 */
#pragma once
#include "ui_lang.h"

const ui_lang_ovr_t UI_OVR_EN[] = {
  // ---- ngon ngu: ten rieng cua menu Configuration (khac han tieng Viet) ----
  { UI_T_SUB_PRE_MATERIAL,    "Material" },
  { UI_T_SUB_THEM_VAT_LIEU,   "+ Add material" },
  { UI_T_SUB_BED_MESH_LUOI,   "Bed Mesh (grid)" },
  { UI_T_SMG_CAU_HINH,        "CONFIGURATION" },
  { UI_T_SMG_TEMP_ADV,        "TEMP" },

  // ---- ui_chrome.cpp ----
  { UI_T_CHROME_PAGE_OF,      "PAGE %d/%d" },

  // ---- ten man (dong tieu de; giu NGAN vi o tieu de chi rong 84 px) ----
  { UI_T_PT_TEMP,             "TEMP" },
  { UI_T_PT_FILES,            "MEDIA" },
  { UI_T_PT_SETTINGS,         "SETTINGS" },
  { UI_T_PT_MESH,             "MESH" },
  { UI_T_PT_SUBMENU,          "CONFIG" },

  // ---- ui_page_menu.cpp: cot gia tri ----
  { UI_T_MENU_TBT_READY,      "ready" },
  { UI_T_MENU_TBT_NOLINK,     "link lost" },
  { UI_T_MENU_TBT_OFF,        "off" },
  { UI_T_MENU_WIFI_OFF,       "OFF" },
  { UI_T_MENU_WIFI_CONN,      "connecting" },
  { UI_T_MENU_WIFI_OK,        "CONNECTED" },
  { UI_T_VALUE_ITEMS,         "%u items" },
  { UI_T_CAL_DEFAULT,         "default" },
  { UI_T_CAL_RELEASE,         "RELEASE" },
  { UI_T_CAL_HOLDING,         "holding" },
  { UI_T_MENU_CALTOUCH,       "Calibrate touch (hold 1.5s)" },

  // ---- batch 3: cac trang JOG/CUSTOM/RATE/ENDSTOP/STATUS + hop thoai ----
  { UI_T_JOG_AXIS,              "AXIS" },
  { UI_T_JOG_MOVE,              "MOVE (mm)" },
  { UI_T_JOG_HINT,              "press = G91 + G1 <axis><d> F3000 (X/Y) / F600 (Z) + G90" },
  { UI_T_CUST_RUNOUT_ON,        "M412: ON (S1)" },
  { UI_T_CUST_RUNOUT_OFF,       "M412: OFF (S0)" },
  { UI_T_CUST_NOTE_M600,        "M600 = change filament (no long retract)" },
  { UI_T_CUST_NOTE_M412,        "M412 = filament runout sensor ON/OFF" },
  { UI_T_CUST_NOTE_DFU,         "REBOOT DFU = flash firmware over USB" },
  { UI_T_RATE_HINT2,            "sent right after pressing +/-  (see FR/FL on STATUS)" },
  { UI_T_ES_HIT,                "TRIGGERED" },
  { UI_T_ES_OPEN,               "open" },
  { UI_T_ES_INFO,               "M119: %u pins - auto refresh 30 Hz" },
  { UI_T_ST_LINK_ERR,           "LINK ERR (bad bytes)" },
  { UI_T_BOX_AGAIN,             "Are you sure? Press OK again" },
  { UI_T_BOX_TUNE_TITLE,        "TUNE DONE" },
  { UI_T_BOX_TUNE_TXT,          "Marlin finished M303/M306\nRemember SAVE EEPROM if needed" },

  // ---- endonym (ten ngon ngu viet bang chinh ngon ngu do - KHONG dich) ----
  // Bang EN co ghi 2 dong nay cho DAY DU, du chuoi y het bang VI (xem `--fallback`).
  { UI_T_LANG_VI,             "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t" },
  { UI_T_LANG_EN,             "English" }
};

#define UI_OVR_EN_N ((uint16_t)(sizeof(UI_OVR_EN) / sizeof(UI_OVR_EN[0])))
