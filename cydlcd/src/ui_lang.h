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
 * CACH THEM 1 CHUOI MOI (3 buoc, `tools/check_lang.py` se bao neu thieu):
 *   1) Them 1 dong vao `typedef enum` duoi day (NGAY TRUOC `UI_T_COUNT`);
 *   2) Them `[UI_T_...] = "..."` vao `ui_lang_vi.h`;
 *   3) Them `[UI_T_...] = "..."` vao `ui_lang_en.h`.
 *   Dung KHOI TAO CO TEN (`[ID] =`) nen THU TU trong 2 bang KHONG quan trong; o nao thieu se la NULL
 *   (checker bao loi, va luc chay `ui_t()` tu roi ve tieng Viet).
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
  UI_T_LANG_VI,                   // "Tiếng Việt"
  UI_T_LANG_EN,                   // "English"

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
 * CHAN DOAN (lenh `UI LANG?`): chuoi CUA MOT ngon ngu CU THE, va so ID THIEU trong bang do.
 * `ui_lang_str()` tra NULL khi ID do thieu ban dich (khac `ui_t()` - ham do tu roi ve tieng Viet).
 * Dung de kiem tra "bang nao con thieu dong nao" ngay tren may; tren PC thi `tools/check_lang.py`
 * in ra TEN ID (UI_T_...) chu khong chi so.
 */
const char *ui_lang_str(uint8_t lang, ui_text_t id);
uint16_t    ui_lang_missing(uint8_t lang);
