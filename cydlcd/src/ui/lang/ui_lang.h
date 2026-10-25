/**
 * ui_lang.h -- DA NGON NGU cho man hinh CYD (kieu Marlin: chuoi `const` nam o FLASH, doi luc chay
 * bang 1 CHI SO).
 *
 * VI SAO LAM KIEU NAY (khong dung lv_i18n / gettext / JSON tren the):
 *   - DRAM TINH cua panel da DAY (`dram0_0_seg`): moi bang ngon ngu phai la `const` -> nam o FLASH.
 *     RAM chi ton DUY NHAT 1 byte (`s_lang`). KHONG copy chuoi vao RAM, KHONG `String`/`std::map`.
 *   - Them 1 ngon ngu = them 1 file `ui_lang_xx.h` + 1 dong trong `UI_LANG_*`; flash con ~1,8 MB nen
 *     chi phi that nho (moi bang ~4 B/chuoi + chuoi).
 *   - Font: `gfx4_font_text` (Segoe UI 16) DA co ASCII + tieng Viet => VI <-> EN KHONG can sinh lai font.
 *     Ngon ngu co chu moi (Thai/CJK...) thi phai sinh lai font va font se phinh rat nhanh -> khong lam.
 *
 * CACH THEM 1 CHUOI MOI (2 buoc, `tools/check_lang.py` se bao neu thieu):
 *   1) Them 1 dong vao `typedef enum` duoi day (NGAY TRUOC `UI_T_COUNT`);
 *   2) Them `[UI_T_...] = "..."` vao `ui_lang_vi.h`.
 *   Dung KHOI TAO CO TEN (`[ID] =`) nen THU TU trong bang KHONG quan trong; o nao thieu se la NULL.
 *
 * ⚠️ CHINH SACH 2 BANG (doi 2026-10-15, batch 2 - ly do o duoi):
 *   - `UI_STR_VI` = **BANG GOC, PHAI DAY DU**. Day la ban dich mac dinh (panel mac dinh tieng Viet) va
 *     cung la BAN DU PHONG: ID nao thieu o bang khac thi lay chuoi o day.
 *   - `UI_STR_EN` = **BANG GHI DE, duoc phep THIEU dong**. ID nao KHONG co trong bang EN thi `ui_t()`
 *     tu lay chuoi tieng Viet -> dung y nghia "chuoi nay giong het o 2 ngon ngu".
 *   LY DO: phan lon ten trong menu Configuration la THUAT NGU TIENG ANH da dung chung cho ca 2 ngon ngu
 *   ("Max X Speed", "Stepper Current", "Activate UBL (G29 A)"...) nen chep lai y nguyen vao bang EN chi
 *   tao ra ~130 dong TRUNG LAP vo nghia. Chi nhung chuoi THAT SU khac moi can ghi vao `ui_lang_en.h`.
 *   DOI LAI (da biet truoc): checker KHONG the tu phat hien "quen dich" - vi vay co lenh
 *   `python tools/check_lang.py --fallback` de IN RA danh sach ID dang dung chung ban VI; MOI BATCH phai
 *   doc danh sach do mot lan de chac chan khong co chu tieng Viet lot vao ban tieng Anh.
 *
 * CHUOI NAO KHONG DICH (giu nguyen trong ca 2 bang):
 *   - G-code, ten frame/khoa NVS, HTML/HTTP, log chan doan `[UI] ...` tren COM8, ten trang cho console
 *     (`PG_KEY[]` - `UI MENU`, `UI ENDSTOP`... phai giong nhau o MOI ngon ngu).
 *   - Ky hieu/icon: "II", "[]", "ON", "OFF", "TBT", "PUSH", "NVS", "MAX", "MIN", don vi ("mm", "C", "%").
 */
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
//  Ngon ngu (chi so = thu tu trong bang). Them ngon ngu: them 1 dong o day + 1 file ui_lang_xx.h.
// ---------------------------------------------------------------------------
enum {
  UI_LANG_VI = 0,
  UI_LANG_EN = 1,
  UI_LANG_COUNT
};

// ---------------------------------------------------------------------------
//  ID chuoi. `UI_T_NONE` = 0 de mang khoi tao bang 0 la NULL het (de phat hien thieu).
// ---------------------------------------------------------------------------
//  Kieu 1 dong cua bang GHI DE (xem ui_lang_en.h: bang EN co tinh chi ghi MOT SO dong nen PHAI dung
//  danh sach cap nay - GCC cua xtensa khong nhan khoi tao co ten khi co khoang trong).
// ---------------------------------------------------------------------------
typedef struct { uint16_t id; const char *s; } ui_lang_ovr_t;

// ---------------------------------------------------------------------------
typedef enum {
  UI_T_NONE = 0,

  // ---- ui_chrome.cpp: dong tieu de + thanh day phan trang ----
  UI_T_CHROME_BACK,               // nut BACK goc phai dong tieu de
  UI_T_CHROME_PREV,               // nut PREV cua thanh day
  UI_T_CHROME_NEXT,               // nut NEXT cua thanh day
  UI_T_CHROME_PAGE_OF,            // "TRANG %d/%d" - CO 2 THAM SO %d

  // ---- ui_lvgl.cpp: chi bao link tren dong tieu de ----
  UI_T_LINK_NONE,                 // "NO LINK"
  UI_T_LINK_ERR,                  // "LINK ERR"

  // ---- ui_lvgl.cpp: TEN MAN hien o dong tieu de (PG_TITLE; PG_KEY cho console giu nguyen) ----
  UI_T_PT_STATUS,                 // STATUS
  UI_T_PT_MENU,                   // MENU
  UI_T_PT_RATE,                   // FEED/FLOW
  UI_T_PT_TEMP,                   // NHIET DO
  UI_T_PT_JOG,                    // JOG
  UI_T_PT_FILES,                  // FILE SD
  UI_T_PT_SETTINGS,               // THONG SO
  UI_T_PT_MESH,                   // BED MESH
  UI_T_PT_CUSTOM,                 // CUSTOM
  UI_T_PT_TBT,                    // TRAMMING
  UI_T_PT_INFO,                   // INFO
  UI_T_PT_ENDSTOP,                // ENDSTOP
  UI_T_PT_WIFI,                   // WIFI
  UI_T_PT_SUBMENU,                // CAU HINH (tieu de trang menu con; ten NHOM van o SM_GROUPS)

  // ---- ui_page_menu.cpp: ten dong MENU ----
  UI_T_MENU_TEMP,                 // "Temperature"
  UI_T_MENU_MOTION,               // "Motion"
  UI_T_MENU_RATE,                 // "Feed/Flow rate"
  UI_T_MENU_FILES,                // "Print from Media"
  UI_T_MENU_MESH,                 // "Bed mesh"
  UI_T_MENU_CUSTOM,               // "Custom/Init SD"
  UI_T_MENU_CONFIG,               // "Configuration"
  UI_T_MENU_TBT,                  // "Tramming wizard"
  UI_T_MENU_INFO,                 // "Info"
  UI_T_MENU_WIFI,                 // "WiFi"
  UI_T_MENU_CALTOUCH,             // "Calibrate touch (giu 1.5s)"

  // ---- ui_page_menu.cpp: cot gia tri ben phai ----
  UI_T_MENU_TBT_READY,            // "san sang"
  UI_T_MENU_TBT_NOLINK,           // "mat link"
  UI_T_MENU_TBT_OFF,              // "chua bat"
  UI_T_MENU_WIFI_OFF,             // "TAT"
  UI_T_MENU_WIFI_CONN,            // "dang noi"
  UI_T_MENU_WIFI_OK,              // "DA NOI"
  UI_T_VALUE_NA,                  // "n/a"
  UI_T_VALUE_ITEMS,               // "%u muc" - CO 1 THAM SO %u
  UI_T_CAL_NVS,                   // "NVS"
  UI_T_CAL_DEFAULT,               // "mac dinh"
  UI_T_CAL_RELEASE,               // "NHAC TAY"
  UI_T_CAL_HOLDING,               // "dang giu"

  // ---- ui_lang.cpp: ten ngon ngu (ENDONYM - co tinh KHONG dich: luon hien o dang goc) ----

  // ---- ui_page_submenu.cpp: ten dong menu con (ID sinh tu chinh chuoi tieng Viet) ----
  UI_T_SUB_ADVANCED_SETTINGS,
  UI_T_SUB_PROBE_Z_OFFSET,
  UI_T_SUB_FADE_HEIGHT,
  UI_T_SUB_CONTROLLER_FAN,
  UI_T_SUB_PREHEAT,
  UI_T_SUB_STORE_SETTINGS_M500,
  UI_T_SUB_LOAD_SETTINGS_M501,
  UI_T_SUB_RESTORE_DEFAULTS_M502,
  UI_T_SUB_ALL_SETTINGS_TABLE,
  UI_T_SUB_MAX_SPEED,
  UI_T_SUB_ACCELERATION,
  UI_T_SUB_JERK,
  UI_T_SUB_PROBE_OFFSETS,
  UI_T_SUB_STEPS_MM,
  UI_T_SUB_TMC_DRIVERS,
  UI_T_SUB_TEMPERATURE,
  UI_T_SUB_FILAMENT,
  UI_T_SUB_TUNE_RUNTIME,
  UI_T_SUB_LANGUAGE,
  UI_T_SUB_HOME_OFFSET_X,
  UI_T_SUB_HOME_OFFSET_Y,
  UI_T_SUB_MAX_X_SPEED,
  UI_T_SUB_MAX_Y_SPEED,
  UI_T_SUB_MAX_Z_SPEED,
  UI_T_SUB_MAX_E_SPEED,
  UI_T_SUB_MIN_VELOCITY,
  UI_T_SUB_MIN_TRAVEL_SPEED,
  UI_T_SUB_ACCEL_PRINT,
  UI_T_SUB_RETRACT_ACCEL,
  UI_T_SUB_TRAVEL_ACCEL,
  UI_T_SUB_MAX_X_ACCEL,
  UI_T_SUB_MAX_Y_ACCEL,
  UI_T_SUB_MAX_Z_ACCEL,
  UI_T_SUB_MAX_E_ACCEL,
  UI_T_SUB_MAX_X_JERK,
  UI_T_SUB_MAX_Y_JERK,
  UI_T_SUB_MAX_Z_JERK,
  UI_T_SUB_MAX_E_JERK,
  UI_T_SUB_JUNCTION_DEV,
  UI_T_SUB_LINEAR_ADV_E,
  UI_T_SUB_PROBE_X_OFFSET,
  UI_T_SUB_PROBE_Y_OFFSET,
  UI_T_SUB_Z_PROBE_WIZARD_M851_Z,
  UI_T_SUB_X_STEPS_MM,
  UI_T_SUB_Y_STEPS_MM,
  UI_T_SUB_Z_STEPS_MM,
  UI_T_SUB_E_STEPS_MM,
  UI_T_SUB_DRIVER_CURRENT,
  UI_T_SUB_STEPPING_MODE,
  UI_T_SUB_X_CURRENT_MA,
  UI_T_SUB_Y_CURRENT_MA,
  UI_T_SUB_Z_CURRENT_MA,
  UI_T_SUB_Z2_CURRENT_MA,
  UI_T_SUB_Z3_CURRENT_MA,
  UI_T_SUB_E0_CURRENT_MA,
  UI_T_SUB_X_STEALTHCHOP,
  UI_T_SUB_Y_STEALTHCHOP,
  UI_T_SUB_Z_STEALTHCHOP,
  UI_T_SUB_Z2_STEALTHCHOP,
  UI_T_SUB_Z3_STEALTHCHOP,
  UI_T_SUB_E0_STEALTHCHOP,
  UI_T_SUB_MPC_HEATER_POWER,
  UI_T_SUB_MPC_BLOCK_HEAT_CAP,
  UI_T_SUB_MPC_SENSOR_RESP,
  UI_T_SUB_MPC_AMBIENT_XFER,
  UI_T_SUB_MPC_AMBIENT_FAN,
  UI_T_SUB_MPC_FILAMENT_HEAT,
  UI_T_SUB_MPC_AUTOTUNE_M306_T,
  UI_T_SUB_BED_PID_P,
  UI_T_SUB_BED_PID_I,
  UI_T_SUB_BED_PID_D,
  UI_T_SUB_BED_PID_AUTOTUNE_M303,
  UI_T_SUB_HOTEND_PID_P,
  UI_T_SUB_HOTEND_PID_I,
  UI_T_SUB_HOTEND_PID_D,
  UI_T_SUB_HOTEND_PID_AUTOTUNE_M303,
  UI_T_SUB_E_IN_MM3_VOLUMETRIC,
  UI_T_SUB_FILAMENT_DIAMETER,
  UI_T_SUB_UNLOAD_LENGTH_MM,
  UI_T_SUB_LOAD_LENGTH_MM,
  UI_T_SUB_RUNOUT_SENSOR,
  UI_T_SUB_FEED,
  UI_T_SUB_FLOW,
  UI_T_SUB_FAN_E0,
  UI_T_SUB_TARGET_E0_C,
  UI_T_SUB_TARGET_BED_C,
  UI_T_SUB_IDLE_SPEED,
  UI_T_SUB_AUTO_MODE,
  UI_T_SUB_ACTIVE_SPEED,
  UI_T_SUB_IDLE_PERIOD_S,
  UI_T_SUB_PRE_MATERIAL,
  UI_T_SUB_THEM_VAT_LIEU,
  UI_T_SUB_MOVE_AXIS_JOG,
  UI_T_SUB_Z_PROBE_WIZARD,
  UI_T_SUB_TRAMMING_WIZARD,
  UI_T_SUB_UNIFIED_BED_LEVELING,
  UI_T_SUB_LEVEL_BED_G29_N,
  UI_T_SUB_DEPLOY_Z_PROBE_M401,
  UI_T_SUB_STOW_Z_PROBE_M402,
  UI_T_SUB_ENDSTOPS_M119,
  UI_T_SUB_M48_PROBE_TEST_G28_O_M48,
  UI_T_SUB_DISABLE_STEPPERS_M84,
  UI_T_SUB_ACTIVATE_UBL_G29_A,
  UI_T_SUB_DEACTIVATE_UBL_G29_D,
  UI_T_SUB_BED_MESH_LUOI,
  UI_T_SUB_MANUALLY_BUILD_MESH,
  UI_T_SUB_FILL_IN_MESH_G29_P3,
  UI_T_SUB_FINE_TUNE_ALL_G29_P4RT,
  UI_T_SUB_SAVE_BED_MESH_G29_S0,
  UI_T_SUB_OUTPUT_UBL_INFO_G29_W,
  UI_T_SUB_OUTPUT_FOR_HOST_G29_T0,
  UI_T_SUB_OUTPUT_FOR_CSV_G29_T1,
  UI_T_SUB_OFF_PRINTER_BACKUP_S_1,
  // ---- ui_page_submenu.cpp: TIEU DE cac nhom (ten nhom hien o dong tieu de) ----
  UI_T_SMG_CAU_HINH,
  UI_T_SMG_ADVANCED,
  UI_T_SMG_MAX_SPEED,
  UI_T_SMG_ACCEL,
  UI_T_SMG_JERK,
  UI_T_SMG_PROBE_OFFSET,
  UI_T_SMG_STEPS_MM,
  UI_T_SMG_TMC_DRIVERS,
  UI_T_SMG_DRIVER_I,
  UI_T_SMG_STEP_MODE,
  UI_T_SMG_TEMP_ADV,
  UI_T_SMG_FILAMENT,
  UI_T_SMG_TUNE,
  UI_T_SMG_CTRL_FAN,
  UI_T_SMG_PREHEAT,
  UI_T_SMG_MOTION,
  UI_T_SMG_UBL,
  UI_T_SMG_LANGUAGE,

  // ---- batch 3: ui_page_jog / ui_page_custom / ui_page_rate / ui_page_endstop / ui_page_status / ui_boxes ----
  UI_T_JOG_AXIS,
  UI_T_JOG_MOVE,
  UI_T_JOG_HINT,
  UI_T_CUST_RUNOUT_ON,
  UI_T_CUST_RUNOUT_OFF,
  UI_T_CUST_NOTE_M600,
  UI_T_CUST_NOTE_M412,
  UI_T_CUST_NOTE_DFU,
  UI_T_RATE_HINT2,
  UI_T_ES_HIT,
  UI_T_ES_OPEN,
  UI_T_ES_INFO,
  UI_T_ST_LINK_ERR,
  UI_T_BOX_AGAIN,
  UI_T_BOX_TUNE_TITLE,
  UI_T_BOX_TUNE_TXT,

  // ---- batch 4: ui_page_files / wifi / preheat / mesh / settings / info / tbt + hop xac nhan ----
  UI_T_FILES_SD_BUSY,
  UI_T_FILES_NO_NAME,
  UI_T_FILES_WIFI_BUSY,
  UI_T_FILES_NONE_SEL,
  UI_T_FILES_ITEM_NO_NAME,
  UI_T_FILES_PRINTING,
  UI_T_FILES_NOT_GCODE,
  UI_T_FILES_SENT_PRINT,
  UI_T_FILES_INIT_BUSY,
  UI_T_FILES_DELETING,
  UI_T_FILES_DIR_DELETED,
  UI_T_FILES_DIR_NOTEMPTY,
  UI_T_FILES_DIR_FAIL,
  UI_T_FILES_FILE_DELETED,
  UI_T_FILES_DELETING2,
  UI_T_FILES_DEL_BUSY,
  UI_T_FILES_DEL_DIR_Q,
  UI_T_FILES_DEL_FILE_Q,
  UI_T_FILES_INITING,
  UI_T_FILES_DEL_BTN,
  UI_T_WIFI_NO_AP,
  UI_T_WIFI_SCAN_BUSY,
  UI_T_WIFI_FORGET,
  UI_T_WIFI_FORGET_Q,
  UI_T_WIFI_PASS_DOTS,
  UI_T_WIFI_PRESS_SCAN,
  UI_T_WIFI_UNSUPPORTED,
  UI_T_WIFI_PASS_NONE,
  UI_T_WIFI_PASS_SET,
  UI_T_WIFI_PASS_ENTER,
  UI_T_WIFI_PASS_COM7,
  UI_T_PRE_DEL_Q,
  UI_T_PRE_DEL_TITLE,
  UI_T_PRE_ADD_FULL,
  UI_T_MESH_OFF,
  UI_T_MESH_UNSUPPORTED,
  UI_T_SET_INFO_NONE,
  UI_T_SET_COL_NAME,
  UI_T_SET_COL_VAL,
  UI_T_SET_HINT,
  UI_T_VALUE_NONE,
  UI_T_INFO_FONT_FAIL,
  UI_T_INFO_FONT_MONTS,
  UI_T_INFO_YES,
  UI_T_INFO_NO,
  UI_T_TBT_HINT,
  UI_T_TBT_RESULT,
  UI_T_CF_STOP_TITLE,
  UI_T_CF_STOP_TEXT,
  UI_T_CF_M502_TEXT,

  UI_T_BOX_SEV_WARN,
  UI_T_BOX_SEV_ERR,
  UI_T_BOX_SEV_FATAL,
  UI_T_BOX_SEV_NOTICE,
  UI_T_CF_DFU_TEXT,

  UI_T_WIFI_BTN_SCAN,
  UI_T_WIFI_BTN_CONN,
  UI_T_WIFI_BTN_DISCON,
  UI_T_WIFI_BTN_FORGET,
  UI_T_WIFI_ST_OFF,
  UI_T_WIFI_ST_CONN,
  UI_T_WIFI_ST_OK,
  UI_T_WIFI_SCAN_FAIL,
  UI_T_CAL_CORNER_1,
  UI_T_CAL_CORNER_2,
  UI_T_CAL_NO_TOUCH,
  UI_T_CAL_RETRY,
  UI_T_CAL_READING,
  UI_T_CAL_DRAG_HOLD,
  UI_T_CAL_TOO_SHORT,
  UI_T_CAL_TOUCH_AGAIN,
  UI_T_CAL_FAIL_L1,
  UI_T_CAL_FAIL_L2,
  UI_T_CAL_SAVED,
  UI_T_CAL_SAVING,                // 2026-10-17: "dang luu" - hop hien ngay khi bam SAVE
  UI_T_CAL_CONTINUE,

  UI_T_VALUE_CORNERS,
  UI_T_FILES_RECEIVING,
  UI_T_INFO_WIFI_CONN,
  UI_T_INFO_WIFI_OK,
  UI_T_TBT_RESTART,
  UI_T_TBT_MAXMIN_Q,          // hoi xac nhan doi MAX/MIN + do lai 4 goc
  UI_T_WIFI_SCANNING,

  // 2026-10-17 (nguoi dung: "cap nhat phan ngon ngu tieng anh, hinh nhu bi lan tieng viet"):
  // 4 chuoi nay truoc day HARD-CODE tieng Viet trong ui_page_preheat.cpp / ui_page_settings.cpp
  // -> o che do EN van hien "Huy" / "Xoa" / "Gia tri:" / "Buoc". Nay dua vao bang ngon ngu.
  // 2026-10-17 (nguoi dung: con 2 chuoi tieng Viet hard-code o trang TRAMMING va SETTINGS):
  UI_T_TBT_NO_DATA,               // "chua co du lieu (CYDLCD_TBT_LINK?)"
  UI_T_TBT_READY_HINT,            // "READY (bam HOME + PROBE)" / "READY (press HOME + PROBE)"
  UI_T_TBT_CORNER,                // "goc" / "corner"
  UI_T_TBT_DONE,                  // "DAT" / "OK" (token DAT Marlin gui len)
  UI_T_SET_RESET_DEFAULT,         // nut "M502 RESET (ve mac dinh)"
  UI_T_DLG_CANCEL,                // nut huy trong hop thoai
  UI_T_DLG_DELETE,                // nut xoa trong hop thoai
  UI_T_SET_VALUE,                 // tien to "Gia tri: " cua lop phu SUA (settings)
  UI_T_SET_STEP,                  // tien to "Buoc " cua lop phu SUA (settings)
  UI_T_LANG_VI,                   // "Tiếng Việt"
  UI_T_LANG_EN,                   // "English"
  // 2026-10-16: dong cuoi cua MAN HINH LOI NANG (chi muc FATAL - xem crit_screen_make() o ui_boxes.cpp).
  UI_T_CRIT_POWER,                // "TAT NGUON ROI BAT LAI"
  UI_T_COUNT
} ui_text_t;

// ---------------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------------
/** Chuoi theo ngon ngu dang chon. KHONG BAO GIO tra NULL (thieu ban dich -> roi ve tieng Viet -> "?"). */
const char *ui_t(ui_text_t id);
#define T(id) ui_t(id)

/** Chi so ngon ngu dang dung (UI_LANG_*). */
uint8_t ui_lang_get(void);

/** Ma ngan ("vi"/"en") va ten endonym ("Tiếng Việt"/"English"). */
const char *ui_lang_code(uint8_t idx);
const char *ui_lang_name(uint8_t idx);

/**
 * Doi ngon ngu: ghi NVS (namespace "ui", khoa "lang"; chi tren board CYDLCD_BOARD_CYD) va cap nhat
 * `s_lang`. KHONG dung lai UI - noi goi phai tu goi `ui_lang_apply_ui()` (o ui_lvgl.cpp) vi file nay
 * khong phu thuoc LVGL. Tra ve true neu chi so hop le va CO doi.
 */
bool ui_lang_set(uint8_t idx);

/** Doc ngon ngu da luu trong NVS luc boot. Chua co -> `CYDLCD_LANG_DEFAULT` (cydlcd_config.h). */
void ui_lang_load(void);

/**
 * CHAN DOAN (lenh `UI LANG?`): chuoi CUA MOT ngon ngu CU THE, va so ID KHONG co chuoi rieng trong bang do.
 * `ui_lang_str()` tra NULL khi ID do khong co ban dich rieng (khac `ui_t()` - ham do tu roi ve tieng Viet).
 * Y NGHIA `ui_lang_missing()` theo chinh sach 2 bang (xem ghi chu dau file):
 *   - voi `UI_LANG_VI`: la LOI (bang goc phai day du);
 *   - voi `UI_LANG_EN`: la HOP LE - so chuoi dung chung ban tieng Viet.
 * Tren PC thi `tools/check_lang.py` (va `--fallback`) kiem tra/in ra TEN ID.
 */
const char *ui_lang_str(uint8_t lang, ui_text_t id);
uint16_t    ui_lang_missing(uint8_t lang);
/** So ID CO chuoi RIENG trong bang cua ngon ngu do (VI: phai = UI_T_COUNT - 1). */
uint16_t    ui_lang_str_count(uint8_t lang);
