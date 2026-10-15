/**
 * ui_preheat.cpp -- KHO PRESET PREHEAT cua MAN HINH CYD.
 *
 * 2026-10-15 (YEU CAU NGUOI DUNG): "trong phan setting co menu preheat lap lai rat nhieu lan, gom vao
 * 1 menu la preheat, trong preheat cho phep user them bot cac loai vat lieu. Max la 6 vi trang
 * temperature kin roi" -> kho nay doi tu **3 preset CO DINH (PLA/PETG/ABS)** thanh **DANH SACH DONG**
 * toi da `CYDLCD_PRE_MAX` (6) vat lieu, moi vat lieu co TEN do nguoi dung dat (<= 8 ky tu).
 *
 * VI SAO kho nay nam o MAN HINH CYD (khong phai Marlin): ban Marlin nay KHONG co menu MarlinUI 12864
 * nen `HAS_PREHEAT = 0` -> M145 khong duoc bien dich va bang preheat khong nam trong `settings_t`.
 * Bat lai HAS_PREHEAT se doi kich thuoc/CRC EEPROM -> Marlin nap MAC DINH (mat M851/PID/M92/M201/M203/
 * M205/M906 va MESH UBL). Vi vay preset duoc giu trong NVS cua man hinh CYD.
 *
 * NVS:
 *   - namespace "ph2" (BAN MOI): `n` = so vat lieu, `t<i>` = ten (chuoi), `h<i>`/`b<i>`/`f<i>` =
 *     nozzle/bed/fan cua vat lieu thu i.
 *   - namespace "ph" (BAN CU, 3 preset): doc MOT LAN de di tru -> nguoi dung khong mat gia tri da dat
 *     truoc khi nang cap. Sau lan di tru dau tien thi "ph" khong duoc dung nua (giu lai cho ban cu).
 *
 * File nay la NOI DUY NHAT giu mang `s_pre[]`:
 *   - API cong khai (ui_lvgl.h) cho cac trang + console: ui_preheat_count/name/get/set/add/remove/
 *     rename/rev;
 *   - preheat_load()/preheat_save() (khai bao ui_lvgl_int.h) goi tu luc khoi dong / sau moi lan sua.
 *
 * `ui_preheat_rev()` la "so lan doi" - cac trang (MENU CONFIGURATION, NHIET DO) so sanh de biet khi nao
 * phai DUNG LAI noi dung (them/bot vat lieu lam so dong doi, chi doi gia tri thi chi doi chu).
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf (khoa NVS)
#include <string.h>                 // strncpy/strcmp
#include <Arduino.h>                // Serial.printf (WIFILOG)

#if CYDLCD_USE_LVGL

#if defined(CYDLCD_BOARD_CYD)
  #include <Preferences.h>
  #define PRE_NVS_NS      "ph2"     // danh sach dong (ban moi)
  #define PRE_NVS_NS_OLD  "ph"      // 3 preset co dinh (ban cu - chi doc de di tru)
#endif

typedef struct {
  char     name[PRE_NAME_LEN];
  uint16_t h, b;
  uint8_t  f;
} pre_t;

static pre_t    s_pre[CYDLCD_PRE_MAX];
static uint8_t  s_pre_n = 0;
static uint32_t s_pre_rev = 0;      // tang moi khi danh sach/gia tri doi

// 3 vat lieu MAC DINH lan dau (dung y Configuration.h cua may nay: 200/60, 230/60, 240/60, FAN 0).
static const pre_t PRE_SEED[CYDLCD_PRE_SEED] = {
  { "PLA",  200, 60, 0 },
  { "PETG", 230, 60, 0 },
  { "ABS",  240, 60, 0 }
};

// Chan gia tri rac (NVS hong / ghi tu ban khac) va chuoi ten (khong cho ky tu dieu khien).
static void pre_sanitize(pre_t *p) {
  if (p->h > PRE_H_MAX) p->h = PRE_H_MAX;
  if (p->b > PRE_B_MAX) p->b = PRE_B_MAX;
  if (p->f > PRE_F_MAX) p->f = PRE_F_MAX;
  p->name[PRE_NAME_LEN - 1] = '\0';
  for (uint8_t k = 0; k < PRE_NAME_LEN - 1 && p->name[k]; k++)
    if ((uint8_t)p->name[k] < 0x20 || (uint8_t)p->name[k] == 0x7F) p->name[k] = '?';
  if (!p->name[0]) snprintf(p->name, sizeof(p->name), "MAT");
}

void preheat_load(void) {
  bool loaded = false;
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  // ---- 1) Ban MOI (danh sach dong) ----
  if (p.begin(PRE_NVS_NS, true)) {                       // read-only
    const uint8_t n = p.getUChar("n", 0);
    if (n >= 1 && n <= CYDLCD_PRE_MAX) {
      char k[4];
      for (uint8_t i = 0; i < n; i++) {
        snprintf(k, sizeof(k), "t%u", (unsigned)i);
        String t = p.getString(k, "");
        snprintf(s_pre[i].name, sizeof(s_pre[i].name), "%s", t.c_str());
        snprintf(k, sizeof(k), "h%u", (unsigned)i);
        s_pre[i].h = (uint16_t)p.getUShort(k, 200);
        snprintf(k, sizeof(k), "b%u", (unsigned)i);
        s_pre[i].b = (uint16_t)p.getUShort(k, 60);
        snprintf(k, sizeof(k), "f%u", (unsigned)i);
        s_pre[i].f = (uint8_t)p.getUChar(k, 0);
        pre_sanitize(&s_pre[i]);
      }
      s_pre_n = n;
      loaded = true;
    }
    p.end();
  }
  // ---- 2) Di tru tu ban CU (3 preset "ph") ----
  if (!loaded && p.begin(PRE_NVS_NS_OLD, true)) {
    char k[4];
    for (uint8_t i = 0; i < CYDLCD_PRE_SEED; i++) {
      snprintf(s_pre[i].name, sizeof(s_pre[i].name), "%s", PRE_SEED[i].name);
      snprintf(k, sizeof(k), "h%u", (unsigned)i);
      s_pre[i].h = (uint16_t)p.getUShort(k, PRE_SEED[i].h);
      snprintf(k, sizeof(k), "b%u", (unsigned)i);
      s_pre[i].b = (uint16_t)p.getUShort(k, PRE_SEED[i].b);
      snprintf(k, sizeof(k), "f%u", (unsigned)i);
      s_pre[i].f = (uint8_t)p.getUChar(k, PRE_SEED[i].f);
      pre_sanitize(&s_pre[i]);
    }
    s_pre_n = CYDLCD_PRE_SEED;
    loaded = true;
    p.end();
    WIFILOG("[PRE] di tru 3 preset tu NVS cu ('%s') sang danh sach moi ('%s')\n",
            PRE_NVS_NS_OLD, PRE_NVS_NS);
    preheat_save();                                      // ghi ngay sang ban moi
  }
#endif
  // ---- 3) Chua co gi -> mac dinh cua firmware ----
  if (!loaded) {
    for (uint8_t i = 0; i < CYDLCD_PRE_SEED; i++) s_pre[i] = PRE_SEED[i];
    s_pre_n = CYDLCD_PRE_SEED;
  }
  s_pre_rev++;

  WIFILOG("[PRE] %u vat lieu:", (unsigned)s_pre_n);
  for (uint8_t i = 0; i < s_pre_n; i++)
    WIFILOG(" %s %u/%u/%u", s_pre[i].name, (unsigned)s_pre[i].h,
            (unsigned)s_pre[i].b, (unsigned)s_pre[i].f);
  WIFILOG("  (nozzle/bed/fan - sua o CONFIGURATION > Preheat)\n");
}

void preheat_save(void) {
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  if (p.begin(PRE_NVS_NS, false)) {                      // read-write
    char k[4];
    p.putUChar("n", s_pre_n);
    for (uint8_t i = 0; i < s_pre_n; i++) {
      snprintf(k, sizeof(k), "t%u", (unsigned)i); p.putString(k, s_pre[i].name);
      snprintf(k, sizeof(k), "h%u", (unsigned)i); p.putUShort(k, s_pre[i].h);
      snprintf(k, sizeof(k), "b%u", (unsigned)i); p.putUShort(k, s_pre[i].b);
      snprintf(k, sizeof(k), "f%u", (unsigned)i); p.putUChar(k, s_pre[i].f);
    }
    // Xoa cac slot CU con sot lai (sau khi bot vat lieu) de lan sau khong doc nham.
    for (uint8_t i = s_pre_n; i < CYDLCD_PRE_MAX; i++) {
      snprintf(k, sizeof(k), "t%u", (unsigned)i); p.remove(k);
      snprintf(k, sizeof(k), "h%u", (unsigned)i); p.remove(k);
      snprintf(k, sizeof(k), "b%u", (unsigned)i); p.remove(k);
      snprintf(k, sizeof(k), "f%u", (unsigned)i); p.remove(k);
    }
    p.end();
  }
#endif
}

// ===========================================================================
//  API CONG KHAI (khai bao trong ui_lvgl.h)
// ===========================================================================
uint8_t ui_preheat_count(void) { return s_pre_n; }
uint32_t ui_preheat_rev(void)  { return s_pre_rev; }

const char *ui_preheat_name(uint8_t i) { return (i < s_pre_n) ? s_pre[i].name : ""; }

bool ui_preheat_get(uint8_t i, uint16_t *nozzle, uint16_t *bed, uint8_t *fan) {
  if (i >= s_pre_n) return false;
  if (nozzle) *nozzle = s_pre[i].h;
  if (bed)    *bed    = s_pre[i].b;
  if (fan)    *fan    = s_pre[i].f;
  return true;
}

// Sua 1 vat lieu: ghi NVS + bao cho cac trang biet (rev) + cap nhat nhan nut o trang NHIET DO.
bool ui_preheat_set(uint8_t i, uint16_t nozzle, uint16_t bed, uint8_t fan) {
  if (i >= s_pre_n) return false;
  s_pre[i].h = nozzle;
  s_pre[i].b = bed;
  s_pre[i].f = fan;
  pre_sanitize(&s_pre[i]);          // chan tran (300/120/255) - giong M145 cua Marlin
  preheat_save();
  s_pre_rev++;
  tp_pre_sync();                    // nhan nut preset o man NHIET DO (neu trang dang mo)
  WIFILOG("[PRE] dat %s: nozzle=%u bed=%u fan=%u\n", s_pre[i].name,
          (unsigned)s_pre[i].h, (unsigned)s_pre[i].b, (unsigned)s_pre[i].f);
  return true;
}

// Them 1 vat lieu vao CUOI danh sach. `name` rong -> tu dat ten "MAT n". false = danh sach da day (6).
bool ui_preheat_add(const char *name) {
  if (s_pre_n >= CYDLCD_PRE_MAX) {
    WIFILOG("[PRE] khong them duoc: da du %u vat lieu (tran cua trang NHIET DO)\n", (unsigned)CYDLCD_PRE_MAX);
    return false;
  }
  pre_t *p = &s_pre[s_pre_n];
  // Mac dinh cua vat lieu moi = gia tri cua vat lieu CUOI (nguoi dung chi sua vai so) - khong co
  // vat lieu nao thi lay 200/60/0 nhu PLA.
  if (s_pre_n > 0) { p->h = s_pre[s_pre_n - 1].h; p->b = s_pre[s_pre_n - 1].b; p->f = s_pre[s_pre_n - 1].f; }
  else             { p->h = 200; p->b = 60; p->f = 0; }
  if (name && name[0]) snprintf(p->name, sizeof(p->name), "%s", name);
  else                 snprintf(p->name, sizeof(p->name), "MAT %u", (unsigned)(s_pre_n + 1));
  pre_sanitize(p);
  s_pre_n++;
  preheat_save();
  s_pre_rev++;
  tp_pre_sync();
  WIFILOG("[PRE] them '%s' (id %u) -> %u vat lieu\n", p->name, (unsigned)(s_pre_n - 1), (unsigned)s_pre_n);
  return true;
}

// Xoa 1 vat lieu. KHONG cho xoa cai CUOI CUNG (man TEMP phai con it nhat 1 nut).
bool ui_preheat_remove(uint8_t i) {
  if (i >= s_pre_n) return false;
  if (s_pre_n <= 1) {
    WIFILOG("[PRE] khong xoa duoc: phai con it nhat 1 vat lieu\n");
    return false;
  }
  WIFILOG("[PRE] xoa '%s' (id %u)\n", s_pre[i].name, (unsigned)i);
  for (uint8_t k = i; k + 1 < s_pre_n; k++) s_pre[k] = s_pre[k + 1];
  s_pre_n--;
  memset(&s_pre[s_pre_n], 0, sizeof(s_pre[0]));
  preheat_save();
  s_pre_rev++;
  tp_pre_sync();
  return true;
}

bool ui_preheat_rename(uint8_t i, const char *name) {
  if (i >= s_pre_n || !name || !name[0]) return false;
  snprintf(s_pre[i].name, sizeof(s_pre[i].name), "%s", name);
  pre_sanitize(&s_pre[i]);
  preheat_save();
  s_pre_rev++;
  tp_pre_sync();
  WIFILOG("[PRE] doi ten id %u -> '%s'\n", (unsigned)i, s_pre[i].name);
  return true;
}

#endif  // CYDLCD_USE_LVGL
