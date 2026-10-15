/**
 * ui_page_submenu.cpp -- man MENU CON cua UI LVGL (man hinh CYD): menu "Configuration" kieu 12864.
 *
 * YEU CAU NGUOI DUNG 2026-10-15: "phan configuration toi thay lam giong 12864 tien hon" -> thay vi
 * MOT BANG PHANG 75 dong (trang SETTINGS cu, van con nguyen o muc "All settings"), thong so duoc xep
 * thanh CAY nhieu cap giong menu Configuration cua Marlin 12864 (xem docs/12864-menu-map.md):
 *
 *   CAU HINH (root)
 *     |- Advanced Settings >                (item DAU TIEN - yeu cau nguoi dung)
 *     |    |- Max Speed >      Max X/Y/Z/E Speed, Min Velocity, Min Travel Speed
 *     |    |- Acceleration >   Accel print/retract/travel + Max X/Y/Z/E Accel
 *     |    |- Jerk >           Max X/Y/Z/E Jerk + Junction Dev + Linear Adv
 *     |    |- Probe Offsets >  Probe X/Y/Z + Z Probe Wizard (M851 Z)
 *     |    |- Steps/mm >       X/Y/Z/E
 *     |    |- TMC Drivers >    Driver Current > (X/Y/Z/Z2/Z3/E0), Stepping Mode > (StealthChop)
 *     |    |- Temperature >    MPC P/C/R/A/F/H + MPC Autotune, Bed PID, Bed PID Autotune, Hotend PID
 *     |    |- Filament >       Filament D, E in mm3, Load/Unload Length, Runout Sensor
 *     |    |- Tune (runtime) > Feed %, Flow %, Fan %, Target E0, Target bed
 *     |    |- Home Offset X/Y
 *     |- Probe Z Offset                     (dong SUA duoc)
 *     |- Fade Height                        (M420 Z - ngay DUOI Probe Z Offset; 2026-10-15 gom ve 1 cho)
 *     |- Controller Fan >                   (M710 I/A/S/D)
 *     |- Preheat >                          (danh sach vat lieu, toi da 6, luu NVS)
 *     |- Store Settings (M500) / Load Settings (M501) / Restore Defaults (M502)
 *     |- All settings (table)               (mo lai TRANG SETTINGS bang phang cu)
 *
 * GHI CHU 2026-10-15: "Fade Height" truoc day xuat hien o 3 nhom (Advanced / Motion / UBL) vi 12864 cung
 * de nhu vay; theo yeu cau nguoi dung no duoc GO khoi ca 3 va chi giu MOT dong o root (duoi Probe Z
 * Offset) de khoi phai di tim.
 *
 * VI SAO CHI 1 TRANG (PG_SUBMENU) CHO CA CAY:
 *   - Moi trang LVGL ton ~5-10 KB pool (xem ghi chu page_release() o ui_lvgl.cpp); 17 nhom = 17 trang
 *     la het pool 48 KB => panel treo. O day NOI DUNG cua trang do `s_sm_g` quyet dinh, doi nhom thi
 *     dung lai noi dung (page_rebuild), nen bo nho chi con trang dang mo nhu cu.
 *   - Bang SM_GROUPS/SM_ITEMS la `const` -> nam o FLASH (DRAM cua panel da full).
 *
 * DONG `SM_VAL` KHONG tu format/gui gia tri: no tro vao bang SET_ROWS[] cua ui_page_settings.cpp theo
 * ID thong so (CYDLCD_SET_*) -> dung lai nguyen lop phu SUA (-/+/OK + 4 muc buoc) va se_send_mcode().
 * Nho vay them 1 dong vao cay chi la: them 1 dong SET_ROWS + 1 dong SM_ITEMS.
 *
 * DONG NAO KHONG CO DU LIEU THI TU AN (settings_has_value): Marlin chi gui thong so ma ban build CO
 * (MPC khi co MPCTEMP, Z2/Z3 khi co driver, StealthChop khi driver ho tro, M710 khi USE_CONTROLLER_FAN)
 * -> cay giong 12864 nhung khong bao gio hien dong trong.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // sscanf / snprintf
#include <stdlib.h>                 // malloc / free (KHONG dung pool LVGL: xem ghi chu o sm_ctx_t)
#include <Arduino.h>                // Serial.printf (lenh chan doan `UI CFG`)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  Kieu dong / kieu nhom
// ===========================================================================
#define SM_SUB 0     // arg = chi so NHOM con (mo tiep)
#define SM_VAL 1     // arg = ID thong so (CYDLCD_SET_*) -> cham vao mo lop phu SUA
#define SM_ACT 2     // arg = hanh dong A_* (do_action)
// 2026-10-15: dong VAT LIEU PREHEAT (arg = chi so slot trong kho preheat). Khac SM_VAL: gia tri
// khong nam trong bang SET_ROWS ma lay tu ui_preheat_*() (NVS "ph2"), va cham vao mo HOP SUA
// 3-trong-1 (nozzle/bed/fan cung 1 hang) chu khong phai lop phu sua 1 so.
#define SM_PRE 3

// SM_PARENT_NONE (255 = nhom GOC) nam trong ui_lvgl_int.h vi do_action() cua ui_lvgl.cpp cung dung.

// Chi so cac nhom (dung lam `parent` va lam tham so SM_S/SMG_*). Thu tu o day chi de doc.
enum {
  SMG_ROOT = 0,     // Configuration
  SMG_ADV,          // Advanced Settings
  SMG_SPEED,        //   Max Speed
  SMG_ACCEL,        //   Acceleration
  SMG_JERK,         //   Jerk
  SMG_PROBE,        //   Probe Offsets
  SMG_STEPS,        //   Steps/mm
  SMG_TMC,          //   TMC Drivers
  SMG_TMC_CUR,      //     Driver Current
  SMG_STEPMODE,     //     Stepping Mode
  SMG_TEMP,         //   Temperature (MPC/PID)
  SMG_FILAMENT,     //   Filament
  SMG_TUNE,         //   Tune (runtime)
  SMG_CTRLFAN,      // Controller Fan (ngang cap Advanced Settings)
  SMG_PREHEAT,      // Preheat (DANH SACH vat lieu do nguoi dung them/bot - toi da 6)
  // 2026-10-15: menu MOTION giong so do 12864 (docs/12864-menu-map.md, nut m20 "Motion"). Day la nhom
  // NGANG CAP CONFIGURATION (parent = SM_PARENT_NONE -> BACK ve MENU, giong 12864: Motion nam trong Main).
  SMG_MOTION,
  SMG_UBL,          //   Unified Bed Leveling >
  SMG_COUNT
};

typedef struct { const char *name; uint8_t kind; uint16_t arg; } sm_item_t;
typedef struct { const char *title; uint8_t parent; const sm_item_t *items; uint8_t n; } sm_group_t;

// Cac macro cho bang duong gon va khong the go sai thu tu tham so:
#define SM_S(txt, g)  { txt, SM_SUB, (uint16_t)(g) }
#define SM_V(txt, id) { txt, SM_VAL, (uint16_t)(id) }
#define SM_A(txt, a)  { txt, SM_ACT, (uint16_t)(a) }
// Dong vat lieu preheat: `txt` chi la TEN TAM (hien luc moi tao trang, ngay sau do
// page_submenu_update() ghi ten THAT tu kho preheat vao).
#define SM_P(slot)    { "Vat lieu", SM_PRE, (uint16_t)(slot) }

// ===========================================================================
//  CAY MENU (thu tu dong y nhu menu Configuration cua 12864)
// ===========================================================================
static const sm_item_t SM_I_ROOT[] = {
  // 2026-10-15 (YEU CAU NGUOI DUNG): "advance setting trong configuration la item dau tien" + "fade height
  // go bo [o cac cho khac] va cho vao configuration, ngay ben duoi z probe offset".
  SM_S("Advanced Settings",         SMG_ADV),
  SM_V("Probe Z Offset",            CYDLCD_SET_PROBE_OFFSET_Z),   // 12864: SUBMENU Z Probe Offset
  SM_V("Fade Height",               CYDLCD_SET_FADE_HEIGHT),      // M420 Z (chi hien khi co ENABLE_LEVELING_FADE_HEIGHT)
  SM_S("Controller Fan",            SMG_CTRLFAN),
  // 2026-10-15 (YEU CAU NGUOI DUNG: "gom vao 1 menu la preheat, cho phep them bot vat lieu"): 3 dong
  // "Preheat PLA/PETG/ABS Settings" cu -> MOT dong "Preheat" (danh sach dong, toi da 6).
  SM_S("Preheat",                   SMG_PREHEAT),
  SM_A("Store Settings (M500)",     A_SAVE),
  SM_A("Load Settings (M501)",      A_LOAD_SETTINGS),
  SM_A("Restore Defaults (M502)",   A_EEPROM_RESET),              // xac nhan 2 buoc (xem do_action)
  SM_A("All settings (table)",      A_GOTO_SETTINGS)              // bang phang cu (giu nguyen chuc nang)
};

static const sm_item_t SM_I_ADV[] = {
  SM_S("Max Speed",                 SMG_SPEED),
  SM_S("Acceleration",              SMG_ACCEL),
  SM_S("Jerk",                      SMG_JERK),
  SM_S("Probe Offsets",             SMG_PROBE),
  SM_S("Steps/mm",                  SMG_STEPS),
  SM_S("TMC Drivers",               SMG_TMC),
  SM_S("Temperature",               SMG_TEMP),
  SM_S("Filament",                  SMG_FILAMENT),
  SM_S("Tune (runtime)",            SMG_TUNE),
  // 2026-10-15: "Fade Height" DA CHUYEN ra CONFIGURATION (ngay duoi "Probe Z Offset") theo yeu cau
  // nguoi dung - truoc day dong nay nam o 3 noi (Advanced / UBL / Motion) nen bi lap.
  SM_V("Home Offset X",             CYDLCD_SET_HOME_OFFSET_X),
  SM_V("Home Offset Y",             CYDLCD_SET_HOME_OFFSET_Y)
};

static const sm_item_t SM_I_SPEED[] = {
  SM_V("Max X Speed",               CYDLCD_SET_MAXFEED_X),
  SM_V("Max Y Speed",               CYDLCD_SET_MAXFEED_Y),
  SM_V("Max Z Speed",               CYDLCD_SET_MAXFEED_Z),
  SM_V("Max E Speed",               CYDLCD_SET_MAXFEED_E),
  SM_V("Min Velocity",              CYDLCD_SET_MIN_FEEDRATE),        // M205 S
  SM_V("Min Travel Speed",          CYDLCD_SET_MIN_TRAVEL_FEEDRATE)  // M205 T
};

static const sm_item_t SM_I_ACCEL[] = {
  SM_V("Accel (print)",             CYDLCD_SET_MAXACCEL_PRINT),      // M204 P
  SM_V("Retract Accel",             CYDLCD_SET_MAXACCEL_RETRACT),    // M204 R
  SM_V("Travel Accel",              CYDLCD_SET_MAXACCEL_TRAVEL),     // M204 T
  SM_V("Max X Accel",               CYDLCD_SET_MAXACCEL_X),
  SM_V("Max Y Accel",               CYDLCD_SET_MAXACCEL_Y),
  SM_V("Max Z Accel",               CYDLCD_SET_MAXACCEL_Z),
  SM_V("Max E Accel",               CYDLCD_SET_MAXACCEL_E)
};

static const sm_item_t SM_I_JERK[] = {
  SM_V("Max X Jerk",                CYDLCD_SET_JERK_X),
  SM_V("Max Y Jerk",                CYDLCD_SET_JERK_Y),
  SM_V("Max Z Jerk",                CYDLCD_SET_JERK_Z),
  SM_V("Max E Jerk",                CYDLCD_SET_JERK_E),
  SM_V("Junction Dev",              CYDLCD_SET_JUNCTION_DEV),        // M205 J (CLASSIC_JERK tat)
  SM_V("Linear Adv E",              CYDLCD_SET_LINEAR_ADV_E)         // M900 K
};

static const sm_item_t SM_I_PROBE[] = {
  SM_V("Probe X Offset",            CYDLCD_SET_PROBE_OFFSET_X),
  SM_V("Probe Y Offset",            CYDLCD_SET_PROBE_OFFSET_Y),
  SM_V("Probe Z Offset",            CYDLCD_SET_PROBE_OFFSET_Z),
  SM_A("Z Probe Wizard (M851 Z)",   A_EDIT_PROBE_Z)                  // mo lop phu sua M851 Z
};

static const sm_item_t SM_I_STEPS[] = {
  SM_V("X Steps/mm",                CYDLCD_SET_STEPS_MM_X),
  SM_V("Y Steps/mm",                CYDLCD_SET_STEPS_MM_Y),
  SM_V("Z Steps/mm",                CYDLCD_SET_STEPS_MM_Z),
  SM_V("E Steps/mm",                CYDLCD_SET_STEPS_MM_E)
};

static const sm_item_t SM_I_TMC[] = {
  SM_S("Driver Current",            SMG_TMC_CUR),
  SM_S("Stepping Mode",             SMG_STEPMODE)
};

static const sm_item_t SM_I_TMC_CUR[] = {
  SM_V("X Current (mA)",            CYDLCD_SET_TMC_CURRENT_X),       // M906 X
  SM_V("Y Current (mA)",            CYDLCD_SET_TMC_CURRENT_Y),
  SM_V("Z Current (mA)",            CYDLCD_SET_TMC_CURRENT_Z),
  SM_V("Z2 Current (mA)",           CYDLCD_SET_TMC_CURRENT_Z2),      // may co 3 driver Z
  SM_V("Z3 Current (mA)",           CYDLCD_SET_TMC_CURRENT_Z3),
  SM_V("E0 Current (mA)",           CYDLCD_SET_TMC_CURRENT_E)
};

static const sm_item_t SM_I_STEPMODE[] = {
  SM_V("X StealthChop",             CYDLCD_SET_STEALTH_X),           // M569 S0/S1 X
  SM_V("Y StealthChop",             CYDLCD_SET_STEALTH_Y),
  SM_V("Z StealthChop",             CYDLCD_SET_STEALTH_Z),
  SM_V("Z2 StealthChop",            CYDLCD_SET_STEALTH_Z2),
  SM_V("Z3 StealthChop",            CYDLCD_SET_STEALTH_Z3),
  SM_V("E0 StealthChop",            CYDLCD_SET_STEALTH_E)
};

static const sm_item_t SM_I_TEMP[] = {
  SM_V("MPC Heater Power",          CYDLCD_SET_MPC_P),               // M306 P
  SM_V("MPC Block Heat Cap",        CYDLCD_SET_MPC_C),               // M306 C
  SM_V("MPC Sensor Resp",           CYDLCD_SET_MPC_R),               // M306 R
  SM_V("MPC Ambient Xfer",          CYDLCD_SET_MPC_A),               // M306 A
  SM_V("MPC Ambient + fan",         CYDLCD_SET_MPC_F),               // M306 F
  SM_V("MPC Filament Heat",         CYDLCD_SET_MPC_H),               // M306 H
  SM_A("MPC Autotune (M306 T)",     A_MPC_TUNE),
  SM_V("Bed PID-P",                 CYDLCD_SET_PID_BED_KP),          // M304 P
  SM_V("Bed PID-I",                 CYDLCD_SET_PID_BED_KI),
  SM_V("Bed PID-D",                 CYDLCD_SET_PID_BED_KD),
  SM_A("Bed PID Autotune (M303)",   A_PID_BED),
  SM_V("Hotend PID-P",              CYDLCD_SET_PID_E_KP),            // M301 P (E0)
  SM_V("Hotend PID-I",              CYDLCD_SET_PID_E_KI),
  SM_V("Hotend PID-D",              CYDLCD_SET_PID_E_KD)
};

static const sm_item_t SM_I_FILAMENT[] = {
  SM_V("E in mm3 (volumetric)",     CYDLCD_SET_FILAMENT_VOL),        // M200 S0/S1
  SM_V("Filament Diameter",         CYDLCD_SET_FILAMENT_DIA),        // M200 D
  SM_V("Unload Length (mm)",        CYDLCD_SET_FC_UNLOAD),           // M603 U
  SM_V("Load Length (mm)",          CYDLCD_SET_FC_LOAD),             // M603 L
  SM_V("Runout Sensor",             CYDLCD_SET_RUNOUT_EN)            // M412 S0/S1
};

static const sm_item_t SM_I_TUNE[] = {
  SM_V("Feed %",                    CYDLCD_SET_FEEDRATE_PCT),        // M220
  SM_V("Flow %",                    CYDLCD_SET_FLOW_PCT),            // M221
  SM_V("Fan E0 %",                  CYDLCD_SET_FAN_PCT_E0),          // M106
  SM_V("Target E0 C",               CYDLCD_SET_TARGET_TEMP_E0),      // M104
  SM_V("Target bed C",              CYDLCD_SET_TARGET_TEMP_BED)      // M140
};

static const sm_item_t SM_I_CTRLFAN[] = {
  SM_V("Idle Speed",                CYDLCD_SET_CTRLFAN_IDLE),        // M710 I
  SM_V("Auto Mode",                 CYDLCD_SET_CTRLFAN_AUTO),        // M710 A
  SM_V("Active Speed",              CYDLCD_SET_CTRLFAN_ACTIVE),      // M710 S
  SM_V("Idle Period (s)",           CYDLCD_SET_CTRLFAN_DURATION)     // M710 D
};

// PREHEAT: danh sach vat lieu do NGUOI DUNG them/bot (kho o ui_preheat.cpp, NVS "ph2").
// 6 dong SM_P slot 0..5: dong nao co vat lieu (slot < ui_preheat_count()) thi hien, ten + "h/b/f" lay
// tu kho (page_submenu_update ghi vao nhan), CHAM VAO = mo HOP SUA 3-trong-1 (xem ui_page_preheat.cpp) -
// khong phai di qua menu con nua. Dong cuoi de THEM vat lieu (ban phim dat ten; da du 6 thi bao loi).
static const sm_item_t SM_I_PREHEAT[] = {
  SM_P(0), SM_P(1), SM_P(2), SM_P(3), SM_P(4), SM_P(5),
  SM_A("+ Them vat lieu",           A_PRE_ADD)
};

// ===========================================================================
//  MENU MOTION (2026-10-15) - giong menu "Motion" cua 12864 (docs/12864-menu-map.md, nut m20)
// ===========================================================================
// Thu tu dong bam theo so do 12864: Move Axis -> (Homing) -> Auto Z-Align -> (Z Align Manual) ->
// Deploy/Stow Z-Probe -> Z Probe Wizard -> Tramming Wizard -> (Worst Point) -> Unified Bed Leveling ->
// Level Bed -> Bed Leveling -> Fade Height -> M48 Probe Test -> Disable Steppers.
// (Fade Height 2026-10-15 da chuyen sang Configuration, xem ghi chu o SM_I_ROOT.)
// 2026-10-15 (tiep): them "Endstops (M119)" (dong cuoi truoc "Disable Steppers") - lay tu MENU chinh.
// KHONG lam 4 muc cua 12864:
//   - "Homing >": YEU CAU NGUOI DUNG 2026-10-15 ("an menu Homing vi da co het trong JOG roi") - trang
//     JOG da co HOME ALL + home tung truc + M84 nen menu con nay la thua;
//   - "Z Align Manual" (man rieng cua 12864), "Worst Point = MAX/MIN" (da co trong trang TRAMMING),
//     "Bed Leveling" (G29 P1..P4 - da co nguyen trang BED MESH voi cac nut UBL).
static const sm_item_t SM_I_MOTION[] = {
  SM_A("Move Axis (JOG)",           A_GOTO_JOG),        // 12864: "Move Axis >" (chon truc + buoc)
  // 2026-10-15: BO dong "Auto Z-Align (G34)" - YEU CAU NGUOI DUNG "tam an va cam chuc nang G34 do thiet
  // ke truc Z cung, ke ca GCODE cung tat" (xem CYDLCD_NO_G34 trong Marlin/Configuration.h).
  SM_A("Z Probe Wizard",            A_EDIT_PROBE_Z),    // mo lop phu sua M851 Z (nhu 12864)
  SM_A("Tramming Wizard",           A_GOTO_TBT),
  SM_S("Unified Bed Leveling",      SMG_UBL),
  SM_A("Level Bed (G29 N)",         A_G29_LEVEL),
  SM_A("Deploy Z-Probe (M401)",     A_PROBE_DEPLOY),
  SM_A("Stow Z-Probe (M402)",       A_PROBE_STOW),
  // 2026-10-15 (YEU CAU NGUOI DUNG: "cho hien thi endstop (m119) thi hay cho vao motion" + "di chuyen
  // menu 'endstop' xuong ngay sau 'stow probe'"): dong nay o MENU chinh da BO, chuyen vao day DUNG SAU
  // "Stow Z-Probe (M402)" -> mo trang ENDSTOP (doc tin hieu M119 30 lan/giay, KHONG con nut REFRESH;
  // ra khoi man la NGUNG xin frame - xem page_endstop_poll o ui_lvgl.cpp/ui_page_endstop.cpp).
  SM_A("Endstops (M119)",           A_GOTO_ENDSTOP),
  // 2026-10-15: BO dong "Fade Height" (12864 co) - da chuyen sang CONFIGURATION > ngay duoi Probe Z Offset.
  SM_A("M48 Probe Test (G28 O+M48)", A_M48),
  SM_A("Disable Steppers (M84)",    A_STEPPERS_OFF)
};

static const sm_item_t SM_I_UBL[] = {
  SM_A("Activate UBL (G29 A)",      A_G29_ON),
  SM_A("Deactivate UBL (G29 D)",    A_G29_OFF),
  SM_A("Bed Mesh (luoi)",           A_GOTO_MESH),       // trang BED MESH: luoi + G29 P1/P3/P4/SAVE
  SM_A("Manually Build Mesh",       A_G29_UBL_MANUAL),  // G29 I999 + G29 P2BT0 (12864)
  SM_A("Fill-in Mesh (G29 P3)",     A_G29_FILL),
  SM_A("Fine Tune All (G29 P4RT)",  A_G29_FT),
  SM_A("Save Bed Mesh (G29 S0)",    A_G29_SAVE),
  SM_A("Output UBL Info (G29 W)",   A_G29_INFO),
  SM_A("Output for Host (G29 T0)",  A_UBL_T0),
  SM_A("Output for CSV (G29 T1)",   A_UBL_T1),
  SM_A("Off Printer Backup (S-1)",  A_UBL_S1)
  // 2026-10-15: BO dong "Fade Height" (12864 co trong nhom UBL) - da chuyen sang CONFIGURATION.
};

#define SM_N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

// Thu tu trong bang KHONG quan trong (moi nhom tro bang ten SMG_*), nhung de doc thi xep theo cay.
static const sm_group_t SM_GROUPS[SMG_COUNT] = {
  [SMG_ROOT]      = { "CAU HINH",     SM_PARENT_NONE, SM_I_ROOT,      SM_N(SM_I_ROOT) },
  [SMG_ADV]       = { "ADVANCED",     SMG_ROOT,       SM_I_ADV,       SM_N(SM_I_ADV) },
  [SMG_SPEED]     = { "MAX SPEED",    SMG_ADV,        SM_I_SPEED,     SM_N(SM_I_SPEED) },
  [SMG_ACCEL]     = { "ACCEL",        SMG_ADV,        SM_I_ACCEL,     SM_N(SM_I_ACCEL) },
  [SMG_JERK]      = { "JERK",         SMG_ADV,        SM_I_JERK,      SM_N(SM_I_JERK) },
  [SMG_PROBE]     = { "PROBE OFFSET", SMG_ADV,        SM_I_PROBE,     SM_N(SM_I_PROBE) },
  [SMG_STEPS]     = { "STEPS/MM",     SMG_ADV,        SM_I_STEPS,     SM_N(SM_I_STEPS) },
  [SMG_TMC]       = { "TMC DRIVERS",  SMG_ADV,        SM_I_TMC,       SM_N(SM_I_TMC) },
  [SMG_TMC_CUR]   = { "DRIVER I",     SMG_TMC,        SM_I_TMC_CUR,   SM_N(SM_I_TMC_CUR) },
  [SMG_STEPMODE]  = { "STEP MODE",    SMG_TMC,        SM_I_STEPMODE,  SM_N(SM_I_STEPMODE) },
  [SMG_TEMP]      = { "TEMP (ADV)",   SMG_ADV,        SM_I_TEMP,      SM_N(SM_I_TEMP) },
  [SMG_FILAMENT]  = { "FILAMENT",     SMG_ADV,        SM_I_FILAMENT,  SM_N(SM_I_FILAMENT) },
  [SMG_TUNE]      = { "TUNE",         SMG_ADV,        SM_I_TUNE,      SM_N(SM_I_TUNE) },
  [SMG_CTRLFAN]   = { "CTRL FAN",     SMG_ROOT,       SM_I_CTRLFAN,   SM_N(SM_I_CTRLFAN) },
  [SMG_PREHEAT]   = { "PREHEAT",      SMG_ROOT,       SM_I_PREHEAT,   SM_N(SM_I_PREHEAT) },
  [SMG_MOTION]    = { "MOTION",       SM_PARENT_NONE, SM_I_MOTION,    SM_N(SM_I_MOTION) },
  [SMG_UBL]       = { "UBL",          SMG_MOTION,     SM_I_UBL,       SM_N(SM_I_UBL) }
};

// ===========================================================================
//  Trang thai cua trang
// ===========================================================================
// So dong TOI DA ve duoc: nhom lon nhat (Temperature) co 14 dong => 16 la du; thanh day PREV/NEXT
// (bot_sync) lo phan trang khi noi dung cao hon 206 px (9 dong dau hien cung luc).
#define SM_ROWS 16

// Cap phat bang malloc() (heap ESP32) chu KHONG bang lv_mem_alloc(): pool LVGL chi 48 KB va dang
// dung cho widget cac trang (xem page_release o ui_lvgl.cpp) - mang con tro nay khong phai widget.
typedef struct {
  lv_obj_t *hit[SM_ROWS];      // NUT cua tung dong (de FOCUS lai dong nguoi dung vua dung)
  lv_obj_t *nm[SM_ROWS];       // nhan TEN dong (chi SM_PRE moi phai ghi lai: ten vat lieu la dong)
  lv_obj_t *val[SM_ROWS];      // nhan COT GIA TRI cua tung dong dang ve
  uint8_t   vis[SM_ROWS];      // chi so THAT trong SM_GROUPS[g].items[] cua dong dang ve
  uint8_t   n;                 // so dong dang ve
  uint8_t   g;                 // nhom dang ve (chup lai luc tao)
} sm_ctx_t;

static sm_ctx_t *s_sm = NULL;
// Nhom dang mo: PHAI nam ngoai s_sm vi submenu_title()/submenu_parent() duoc goi CA KHI trang chua
// duoc tao (show_page dat tieu de cho trang MOI) va page_submenu_destroy() xoa s_sm.
static uint8_t s_sm_g = SMG_ROOT;
// VI TRI CUON cua TUNG NHOM (yeu cau nguoi dung 2026-10-15: "menu co phan trang, khi bam back chua quay
// ve trang truoc do"). Ca cay dung CHUNG 1 trang nen khong the luu theo trang: moi nhom co danh sach dai
// ngan khac nhau -> phai luu theo NHOM. uint16_t cho tiet kiem DRAM (noi dung 1 nhom chi vai tram px).
static uint16_t s_sm_scroll[SMG_COUNT];
// DONG (chi so item) can FOCUS khi nhom hien lai: la dong vua mo nhom con (do_action A_BACK quay lui
// hoac bam 1 dong SM_SUB). 255 = khong to dong nao.
static uint8_t s_sm_hi = 255;

// ===========================================================================
//  Loc dong khong co du lieu (de quy qua cac nhom con)
// ===========================================================================
static bool sm_group_visible(const uint8_t g);      // forward: sm_item_visible() goi xuong nhom con

static bool sm_item_visible(const sm_item_t *it) {
  switch (it->kind) {
    case SM_SUB: return sm_group_visible((uint8_t)it->arg);
    case SM_VAL: return settings_has_value(it->arg);
    case SM_PRE: return it->arg < ui_preheat_count();   // slot chua co vat lieu -> KHONG ve dong do
    default:     return true;      // SM_ACT: lenh luon chay duoc, khong phu thuoc du lieu Marlin
  }
}

// true = nhom con co it nhat 1 dong hien duoc (nhom RONG thi dong menu tro toi no bi an luon).
static bool sm_group_visible(const uint8_t g) {
  if (g >= SMG_COUNT) return false;
  const sm_group_t *G = &SM_GROUPS[g];
  for (uint8_t i = 0; i < G->n; i++)
    if (sm_item_visible(&G->items[i])) return true;
  return false;
}

// ===========================================================================
//  Ve / cap nhat man
// ===========================================================================
static void cb_sm_row(lv_event_t *e) {
  if (!s_sm) return;
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k >= s_sm->n || s_sm->g >= SMG_COUNT) return;
  const uint8_t it_idx = s_sm->vis[k];
  // NHO DONG VUA BAM (item trong NHOM dang mo): khi BACK quay lai nhom nay (tu trang JOG/BED MESH/...)
  // thi dong do duoc FOCUS + to sang - yeu cau nguoi dung 2026-10-15 ("chon Motion, Move Axis (JOG),
  // bam Back le ra phai highlight 'Move Axis (JOG)'").
  s_sm_hi = it_idx;
  const sm_item_t *it = &SM_GROUPS[s_sm->g].items[it_idx];
  switch (it->kind) {
    case SM_SUB: submenu_open((uint8_t)it->arg); break;   // vao nhom con (dung lai noi dung trang nay)
    case SM_VAL: ov_open_id(it->arg); break;              // mo lop phu SUA cua dong thong so do
    // Dong VAT LIEU PREHEAT: mo HOP SUA 3-trong-1 (nozzle/bed/fan cung 1 hang) - khong vao menu con.
    case SM_PRE: preheat_edit_open((uint8_t)it->arg); break;
    case SM_ACT: do_action((int)it->arg); break;          // lenh M500/M501/M502/mo trang bang...
    default: break;
  }
}

void page_submenu_create(lv_obj_t *p) {
  // Cac dong la o DAC lien nhau (giong danh sach menu cua Marlin / man MENU cua man hinh).
  lv_obj_set_style_pad_row(p, 0, 0);

  if (!s_sm) s_sm = (sm_ctx_t *)malloc(sizeof(sm_ctx_t));
  if (!s_sm) return;                       // het RAM: trang trong (khong crash) - xem ghi chu o sm_ctx_t

  if (s_sm_g >= SMG_COUNT) s_sm_g = SMG_ROOT;
  s_sm->g = s_sm_g;
  s_sm->n = 0;

  const sm_group_t *G = &SM_GROUPS[s_sm->g];
  for (uint8_t i = 0; i < G->n && s_sm->n < SM_ROWS; i++) {
    const sm_item_t *it = &G->items[i];
    if (!sm_item_visible(it)) continue;    // dong khong co du lieu -> KHONG ve (khong hien dong rong)

    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2,
                         cb_sm_row, (void *)(intptr_t)s_sm->n);
    s_sm->hit[s_sm->n] = r;      // de submenu_focus_parent() focus lai dong vua mo nhom con
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    // Dong SM_PRE lay TEN tu kho preheat (khong phai it->name) -> giu con tro nhan de update ghi lai.
    s_sm->nm[s_sm->n] = mk_label(r, it->name, MR_MENU);
    lv_obj_align(s_sm->nm[s_sm->n], LV_ALIGN_LEFT_MID, 4, 0);
    // Cot phai: nhom con -> ICON chevron (font ICON, KHONG phai chu ">"), dong sua duoc -> gia tri.
    // YEU CAU NGUOI DUNG 2026-10-15: "viec hien thi menu co submenu voi '>' qua xau, hay ve icon font
    // dep chut" + "doi voi cac icon khong phai chu thi tach ra cho khac" -> dung FONT ICON rieng
    // (gfx4_font_icons, Material Icons) chu khong dung ky tu chu: U+E008 = chevron_right.
    const bool is_sub = (it->kind == SM_SUB);
    s_sm->val[s_sm->n] = mk_label(r, is_sub ? "\xEE\x80\x88" : "", is_sub ? COL_ACCENT : MR_MENUVAL);
    if (is_sub) lv_obj_set_style_text_font(s_sm->val[s_sm->n], gfx4_font_icons_get(), 0);
    lv_obj_align(s_sm->val[s_sm->n], LV_ALIGN_RIGHT_MID, -6, 0);
    s_sm->vis[s_sm->n] = i;
    s_sm->n++;
  }

  page_submenu_update();                   // dien gia tri ngay (khong doi 400 ms dau tien)
}

void page_submenu_update(void) {
  if (!s_sm || s_sm->g >= SMG_COUNT) return;

  // So thong so Marlin gui DOI (moi ket noi lai / vua sua) -> co the co dong VUA hien duoc: dung lai
  // noi dung de loc lai (giong trang SETTINGS). Danh sach PREHEAT cung vay: them/bot vat lieu lam SO
  // DONG doi -> so sanh ui_preheat_rev() (xem ui_preheat.cpp).
  //
  // !!! CHI dung lai khi trang DA HIEN (s_page_act == PG_SUBMENU) !!!
  // LOI THAT 2026-10-15 (panel panic "Guru Meditation ... LoadProhibited" ngay khi bam `UI CFG 0`):
  // page_submenu_create() goi ham nay o CUOI, ma luc do create_page() (tu show_page) CHUA dat
  // s_page_act = PG_SUBMENU -> page_rebuild() thay "trang KHONG mo" nen no THU HOI trang vua tao roi
  // return NGAY (khong tao lai, xem page_rebuild) => s_page[PG_SUBMENU] = NULL => create_page() goi
  // tiep lv_obj_add_flag(NULL) -> panic. Lan tao dau da ve du dong theo du lieu hien tai nen bo qua o
  // day la dung; cac lan sau (nhip 400 ms) s_page_act da la PG_SUBMENU -> dung lai binh thuong.
  static uint8_t  last_n = 255;
  static uint32_t last_pre = 0;
  if (last_n != cydlcd.n_settings || last_pre != ui_preheat_rev()) {
    last_n = cydlcd.n_settings;
    last_pre = ui_preheat_rev();
    if (s_sm->n && s_page_act == PG_SUBMENU) { page_rebuild(PG_SUBMENU); return; }
  }

  const sm_group_t *G = &SM_GROUPS[s_sm->g];
  char b[24];
  for (uint8_t k = 0; k < s_sm->n; k++) {
    const sm_item_t *it = &G->items[s_sm->vis[k]];
    if (it->kind == SM_VAL && s_sm->val[k]) {
      // set_text() chi ghi khi chu DOI -> khong nhay, khong day SPI vo ich.
      if (settings_fmt_cur(b, sizeof(b), it->arg)) set_text(s_sm->val[k], b);
    }
    else if (it->kind == SM_PRE) {
      // Ten vat lieu o cot TRAI + "nozzle/bed/fan" o cot PHAI (ca 3 thuoc tinh tren 1 dong).
      uint16_t h = 0, bed = 0;
      uint8_t  f = 0;
      if (ui_preheat_get((uint8_t)it->arg, &h, &bed, &f)) {
        if (s_sm->nm[k]) set_text(s_sm->nm[k], ui_preheat_name((uint8_t)it->arg));
        snprintf(b, sizeof(b), "%u/%u/%u", (unsigned)h, (unsigned)bed, (unsigned)f);
        if (s_sm->val[k]) set_text(s_sm->val[k], b);
      }
    }
  }
}

void page_submenu_destroy(void) {
  if (s_sm) { free(s_sm); s_sm = NULL; }
}

// ===========================================================================
//  Dieu huong (goi tu do_action / show_page)
// ===========================================================================
// Mo 1 nhom. `keep_hi` = GIU `s_sm_hi` da dat truoc (dung khi quay lai tu NGAN XEP BACK: luc do khong
// con quan he cha/con de suy ra dong can to sang, phai dung dong da ghi trong ngan xep).
static void submenu_open_ex(uint8_t g, const bool keep_hi) {
  if (g >= SMG_COUNT) g = SMG_ROOT;

  // NHO CHO CUA NHOM DANG XEM + DONG VUA DUNG (yeu cau nguoi dung 2026-10-15):
  //   - luu `scroll_top` cua nhom dang mo de lan quay lai dung cho (moi nhom 1 vi tri rieng);
  //   - tim DONG trong NHOM DICH co arg == nhom dang mo: do chinh la dong nguoi dung vua bam (di xuong)
  //     hoac dong vua quay lui (BACK len nhom cha) -> se duoc FOCUS + to sang khi nhom dich hien lai.
  if (!keep_hi) {
    s_sm_hi = 255;
    if (s_sm_g < SMG_COUNT) {
      const sm_group_t *G = &SM_GROUPS[g];
      for (uint8_t i = 0; i < G->n; i++)
        if (G->items[i].kind == SM_SUB && (uint8_t)G->items[i].arg == s_sm_g) { s_sm_hi = i; break; }
    }
  }
  // Luu vi tri cuon cua nhom dang xem TRUOC khi doi nhom (trang con dang hien nen doc duoc).
  if (s_sm_g < SMG_COUNT) submenu_save_scroll();

  s_sm_g = g;
  // Dang o TRONG trang nay (bam 1 dong nhom con): chi dung lai noi dung, KHONG tao trang moi
  // (tao trang moi se ton them bo nho LVGL va page cu van nam do).
  if (s_page_act == PG_SUBMENU && s_page[PG_SUBMENU]) { page_rebuild(PG_SUBMENU); return; }

  show_page(PG_SUBMENU);
  // show_page() dua trang ve NHOM GOC khi MO trang nay tu mot trang KHAC (dung y Marlin: bam tu MENU
  // thi vao tu dau) - nhung o day NGUOI GOI DA CHI DINH nhom (vd lenh chan doan `UI CFG 14`), nen dat
  // lai nhom da yeu cau roi dung lai noi dung. LOI THAT 2026-10-15: `UI CFG 14` in ra nhom 0.
  if (s_sm_g != g) {
    s_sm_g = g;
    page_rebuild(PG_SUBMENU);
  }
}

void submenu_open(uint8_t g) { submenu_open_ex(g, false); }

// Quay lai 1 nhom tu NGAN XEP BACK (xem back_pop/do_action A_BACK o ui_lvgl.cpp) + TO SANG dong `hi`
// (dong nguoi dung da bam de roi khoi nhom do - vd "Move Axis (JOG)" khi BACK tu trang JOG).
void submenu_open_restore(uint8_t g, uint8_t hi) {
  s_sm_hi = hi;
  submenu_open_ex(g, true);
}

uint8_t submenu_group(void)     { return s_sm_g; }
uint8_t submenu_last_item(void) { return s_sm_hi; }

// Mo menu MOTION (muc "Motion" trong MENU - xem ui_page_menu.cpp; 12864 de Motion ngang cap
// Configuration trong Main menu nen nhom nay co parent = SM_PARENT_NONE -> BACK tu day ve MENU).
void submenu_open_motion(void) {
  submenu_open(SMG_MOTION);
}

// Luu vi tri cuon cua NHOM dang mo (goi tu show_page TRUOC khi thu hoi trang MENU CON).
void submenu_save_scroll(void) {
  if (s_sm_g < SMG_COUNT && s_page[PG_SUBMENU])
    s_sm_scroll[s_sm_g] = (uint16_t)lv_obj_get_scroll_top(s_page[PG_SUBMENU]);
}

// Vi tri cuon da luu cua NHOM dang mo (page_after_show tra lai sau khi trang da hien).
lv_coord_t submenu_scroll_y(void) {
  return (s_sm_g < SMG_COUNT) ? (lv_coord_t)s_sm_scroll[s_sm_g] : 0;
}

// Focus DONG da mo nhom con (goi tu page_after_show SAU khi trang MENU CON da hien lai VA da tra lai vi
// tri cuon). KHONG goi `lv_obj_scroll_to_view()`: dong do chinh la dong nguoi dung vua bam (luon nam
// trong vung nhin cua cho cuon da luu) nen chi can FOCUS - neu cuon theo dong thi se ghi de vi tri cuon
// vua tra lai (gap that 2026-10-15: nhom ADVANCED dang o scroll 92 px, quay lui thi bi keo ve 0).
// Chi to sang MOT lan: sau do xoa `s_sm_hi` de lan ve sau (tu man khac) khong focus lai dong cu.
void submenu_focus_parent(void) {
  if (!s_sm || s_sm_hi >= SMG_COUNT) return;
  for (uint8_t k = 0; k < s_sm->n; k++) {
    if (s_sm->vis[k] != s_sm_hi || !s_sm->hit[k]) continue;
    if (s_group) lv_group_focus_obj(s_sm->hit[k]);
    break;
  }
  s_sm_hi = 255;
}

uint8_t submenu_parent(void) {
  return (s_sm_g < SMG_COUNT) ? SM_GROUPS[s_sm_g].parent : SM_PARENT_NONE;
}

const char *submenu_title(void) {
  return (s_sm_g < SMG_COUNT) ? SM_GROUPS[s_sm_g].title : "CAU HINH";
}

void submenu_reset_root(void) {
  s_sm_g = SMG_ROOT;
}

// ===========================================================================
//  CHAN DOAN: `UI CFG` (in ca cay) / `UI CFG <nhom>` (mo nhom do roi in cac dong dang hien)
//  VI SAO CAN: khong the cham tay vao LCD tu PC - day la cach DUY NHAT kiem tra cay menu + viec loc
//  dong (dong nao dang an vi Marlin khong gui) va gia tri tung dong.
// ===========================================================================
void submenu_dump(const char *arg) {
  int g = -1;
  if (arg && *arg) sscanf(arg, "%d", &g);

  if (g >= 0) {
    if (g >= (int)SMG_COUNT) {
      Serial.printf("[UI] CFG: chi co nhom 0..%u\n", (unsigned)(SMG_COUNT - 1));
      return;
    }
    submenu_open((uint8_t)g);
  }

  char b[20];
  Serial.printf("[UI] CFG: nhom dang mo = %u (%s), cha = %u | Marlin gui %u thong so\n",
                (unsigned)s_sm_g, submenu_title(), (unsigned)submenu_parent(),
                (unsigned)cydlcd.n_settings);

  const bool one = (g >= 0);
  for (uint8_t i = 0; i < SMG_COUNT; i++) {
    const sm_group_t *G = &SM_GROUPS[i];
    if (one && i != s_sm_g) continue;
    // Do sau = so lan di len cha (chi de thut le cho de doc).
    uint8_t depth = 0;
    for (uint8_t p = G->parent; p != SM_PARENT_NONE && p < SMG_COUNT && depth < SMG_COUNT; p = SM_GROUPS[p].parent)
      depth++;
    uint8_t vis = 0;
    for (uint8_t k = 0; k < G->n; k++) if (sm_item_visible(&G->items[k])) vis++;
    Serial.printf("[UI] CFG %*s[%u] %-13s cha=%-3u dong=%u/%u%s\n",
                  (int)(2 * depth), "", (unsigned)i, G->title, (unsigned)G->parent,
                  (unsigned)vis, (unsigned)G->n, vis ? "" : "  (RONG -> dong menu tro toi no bi an)");
    if (!one) continue;
    for (uint8_t k = 0; k < G->n; k++) {
      const sm_item_t *it = &G->items[k];
      if (!sm_item_visible(it)) continue;
      if (it->kind == SM_VAL) {
        const bool ok = settings_fmt_cur(b, sizeof(b), it->arg);
        Serial.printf("[UI] CFG     %-24s VAL id=%-3u = %s\n", it->name, (unsigned)it->arg,
                      ok ? b : "(khong co gia tri)");
      }
      else if (it->kind == SM_PRE) {
        // Dong VAT LIEU PREHEAT: ten + 3 thuoc tinh tren cung 1 dong (giong cach hien tren LCD).
        uint16_t h = 0, bed = 0;
        uint8_t  f = 0;
        if (ui_preheat_get((uint8_t)it->arg, &h, &bed, &f)) {
          snprintf(b, sizeof(b), "%u/%u/%u", (unsigned)h, (unsigned)bed, (unsigned)f);
          Serial.printf("[UI] CFG     %-24s PRE slot=%u = %s\n", ui_preheat_name((uint8_t)it->arg),
                        (unsigned)it->arg, b);
        }
        else Serial.printf("[UI] CFG     %-24s PRE slot=%u = (chua co vat lieu)\n",
                           "-", (unsigned)it->arg);
      }
      else Serial.printf("[UI] CFG     %-24s %s %u\n", it->name,
                         it->kind == SM_SUB ? "SUB ->" : "ACT", (unsigned)it->arg);
    }
  }
  Serial.printf("[UI] CFG: SM_ROWS=%u (so dong ve duoc cua 1 nhom)\n", (unsigned)SM_ROWS);
}

#endif  // CYDLCD_USE_LVGL
