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

  // ---- batch 4 ----
  { UI_T_FILES_SD_BUSY,         "Marlin is reading the SD - will retry" },
  { UI_T_FILES_NO_NAME,         "(no name)" },
  { UI_T_FILES_WIFI_BUSY,       "Receiving file over WiFi - wait" },
  { UI_T_FILES_NONE_SEL,        "Nothing selected" },
  { UI_T_FILES_ITEM_NO_NAME,    "Item has no name - cannot open" },
  { UI_T_FILES_PRINTING,        "Printing - press STOP before printing another file" },
  { UI_T_FILES_NOT_GCODE,       "Not a G-code file (.gcode/.gco)" },
  { UI_T_FILES_SENT_PRINT,      "print command sent" },
  { UI_T_FILES_INIT_BUSY,       "Printing - not re-initializing the card" },
  { UI_T_FILES_DELETING,        "DELETING..." },
  { UI_T_FILES_DIR_DELETED,     "empty folder deleted" },
  { UI_T_FILES_DIR_NOTEMPTY,    "Folder has files - delete them first" },
  { UI_T_FILES_DIR_FAIL,        "Cannot delete folder" },
  { UI_T_FILES_FILE_DELETED,    "file deleted" },
  { UI_T_FILES_DELETING2,       "deleting..." },
  { UI_T_FILES_DEL_BUSY,        "Printing - not deleting files" },
  { UI_T_FILES_DEL_DIR_Q,       "Delete FOLDER?\n%s\n(only when EMPTY)" },
  { UI_T_FILES_DEL_FILE_Q,      "Delete file?\n%s\nCannot be undone!" },
  { UI_T_FILES_INITING,         "INITIALIZING CARD..." },
  { UI_T_FILES_DEL_BTN,         "DELETE" },
  { UI_T_WIFI_NO_AP,            "(no network found)" },
  { UI_T_WIFI_SCAN_BUSY,        "(Marlin is printing - no scan)" },
  { UI_T_WIFI_FORGET,           "FORGET WIFI" },
  { UI_T_WIFI_FORGET_Q,         "Delete saved WiFi config (NVS)?\nYou must enter the password again!" },
  { UI_T_WIFI_PASS_DOTS,        "PASSWORD..." },
  { UI_T_WIFI_PRESS_SCAN,       "(press SCAN to find networks)" },
  { UI_T_WIFI_UNSUPPORTED,      "WiFi: UNSUPPORTED (no module)" },
  { UI_T_WIFI_PASS_NONE,        "PASSWORD: -" },
  { UI_T_WIFI_PASS_SET,         "PASSWORD: **** (%u)" },
  { UI_T_WIFI_PASS_ENTER,       "PASSWORD: (press to enter)" },
  { UI_T_WIFI_PASS_COM7,        "PASSWORD: enter via COM7 (no keyboard)" },
  { UI_T_PRE_DEL_Q,             "Delete material '%s'?" },
  { UI_T_PRE_DEL_TITLE,         "DELETE MATERIAL" },
  { UI_T_PRE_ADD_FULL,          "cannot add (6 already)" },
  { UI_T_MESH_OFF,              "OFF (view normal grid)" },
  { UI_T_MESH_UNSUPPORTED,      "unsupported (CYDLCD_HAS_MESH=0)" },
  { UI_T_SET_INFO_NONE,         "Marlin sent: 0 params" },
  { UI_T_SET_COL_NAME,          "PARAM" },
  { UI_T_SET_COL_VAL,           "VALUE" },
  { UI_T_SET_HINT,              "swipe=scroll | tap=EDIT | %u/%u params" },
  { UI_T_VALUE_NONE,            "(none)" },
  { UI_T_INFO_FONT_FAIL,        "ERR->MS14" },
  { UI_T_INFO_FONT_MONTS,       "FONT: Montserrat (no diacritics)" },
  { UI_T_INFO_YES,              "yes" },
  { UI_T_INFO_NO,               "no" },
  { UI_T_TBT_HINT,              "DONE = corner done | MAX/MIN = switch mode" },
  { UI_T_TBT_RESULT,            "DONE - offset from center (only BACK left)" },
  { UI_T_CF_STOP_TITLE,         "STOP PRINT" },
  { UI_T_CF_STOP_TEXT,          "Really stop the print? (M524)" },
  { UI_T_CF_M502_TEXT,          "Reset RAM settings to DEFAULTS?\nOnly written to EEPROM if you press SAVE after" },

  { UI_T_BOX_SEV_WARN,          "WARNING" },
  { UI_T_BOX_SEV_ERR,           "ERROR" },
  { UI_T_BOX_SEV_FATAL,         "CRITICAL ERROR" },
  { UI_T_BOX_SEV_NOTICE,        "NOTICE" },
  { UI_T_CF_DFU_TEXT,           "Jump into the ROM DFU bootloader?" },

  { UI_T_WIFI_BTN_SCAN,         "SCAN" },
  { UI_T_WIFI_BTN_CONN,         "CONNECT" },
  { UI_T_WIFI_BTN_DISCON,       "LEAVE" },
  { UI_T_WIFI_BTN_FORGET,       "DELETE" },
  { UI_T_WIFI_ST_OFF,           "OFF" },
  { UI_T_WIFI_ST_CONN,          "CONNECTING" },
  { UI_T_WIFI_ST_OK,            "CONNECTED" },
  { UI_T_WIFI_SCAN_FAIL,        "(scan failed)" },
  { UI_T_CAL_CORNER_1,          "TOUCH + DRAG TO THIS CORNER" },
  { UI_T_CAL_CORNER_2,          "THEN RELEASE" },
  { UI_T_CAL_NO_TOUCH,          "NO TOUCH DETECTED" },
  { UI_T_CAL_RETRY,             "RETRY" },
  { UI_T_CAL_READING,           "READING" },
  { UI_T_CAL_DRAG_HOLD,         "drag to the corner then RELEASE" },
  { UI_T_CAL_TOO_SHORT,         "TOO SHORT / NOISY" },
  { UI_T_CAL_TOUCH_AGAIN,       "TOUCH AGAIN" },
  { UI_T_CAL_FAIL_L1,           "calibration failed" },
  { UI_T_CAL_FAIL_L2,           "keeping old values" },
  { UI_T_CAL_SAVED,             "SAVED" },
  { UI_T_CAL_CONTINUE,          "touch to continue" },

  { UI_T_VALUE_CORNERS,         "4 corners" },
  { UI_T_FILES_RECEIVING,       " (receiving)" },
  { UI_T_INFO_WIFI_CONN,        "CONNECTING" },
  { UI_T_INFO_WIFI_OK,          "CONNECTED" },
  { UI_T_TBT_RESTART,           "HOME + PROBE 4 CORNERS" },
  { UI_T_TBT_MAXMIN_Q,          "SWITCH MODE AND RE-PROBE 4 CORNERS?" },
  { UI_T_DLG_CANCEL,            "Cancel" },
  { UI_T_DLG_DELETE,            "Delete" },
  { UI_T_SET_VALUE,             "Value: " },
  { UI_T_SET_STEP,              "Step " },
  { UI_T_WIFI_SCANNING,         "SCANNING NETWORKS..." },

  // ---- endonym (ten ngon ngu viet bang chinh ngon ngu do - KHONG dich) ----
  // Bang EN co ghi 2 dong nay cho DAY DU, du chuoi y het bang VI (xem `--fallback`).
  { UI_T_LANG_VI,             "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t" },
  { UI_T_LANG_EN,             "English" },
  { UI_T_CRIT_POWER,          "POWER OFF, THEN POWER ON" }
};

#define UI_OVR_EN_N ((uint16_t)(sizeof(UI_OVR_EN) / sizeof(UI_OVR_EN[0])))
