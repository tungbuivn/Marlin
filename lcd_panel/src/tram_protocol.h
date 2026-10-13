/**
 * tram_protocol.h -- Giao thuc ASCII cho link TRAMMING (man hinh rieng lcd_panel <-> Marlin).
 *
 * FILE DUNG CHUNG: ban y nguyen nam o
 *      Marlin/src/feature/tram_protocol.h      (Marlin)
 *      lcd_panel/src/tram_protocol.h           (panel)
 *   Sua file nay thi phai COPY Y NGUYEN sang file kia. Header chi dung <stdint.h>/<stdio.h>/
 *   <string.h> (khong Arduino, khong Marlin) nen test duoc tren may tinh:
 *      pwsh lcd_panel/tools/run_host_test.ps1
 *
 * VI SAO KHONG DUNG FRAME NHI PHAN (nhu panel_protocol.h)?
 *   Link tramming chi can vai con so + vai lenh, nen:
 *     - doc duoc bang Serial Monitor -> go loi khong can thiet bi;
 *     - KHONG can ExtUI/PANEL_SERIAL_UI -> giu nguyen MarlinUI + menu 12864 (Tramming Wizard,
 *       Z ALIGN MANUAL...) ma nguoi dung dang dung. Day la ly do chinh cua giao thuc nay.
 *   Xem docs/issues/0001-panel-serial-ui-tat-custom-menu.md.
 *
 * HUONG TRUYEN
 *
 *   panel -> Marlin: 1 ky tu lenh + '\n'
 *        'P' = PROBE   : probe LAI goc dang dung (khong home lai)
 *        'D' = DONE    : goc nay xong -> di goc can chinh ke tiep; het goc -> home + probe 4 goc
 *        'M' = MIN/MAX : doi chien luoc MAX <-> MIN (moc Z-MIN <-> Z-MAX) + luu EEPROM
 *        'R' = RESTART : home + probe lai CA 4 GOC (neu wizard dang khong chay thi BAT DAU)
 *        'B' = BACK    : thoat wizard (stow probe + danh dau Z chua home)   <- mo rong
 *        'Q' = QUERY   : hoi trang thai -> Marlin gui lai NGAY 1 dong TRAM
 *     (ky tu khac / dong rong: bo qua. Nhan ca chu thuong.)
 *
 *   Marlin -> panel: 1 dong trang thai, gui khi trang thai DOI + dinh ky ~5Hz:
 *        TRAM <phase> <mode> <idx> <ref> <d0> <d1> <d2> <d3> <A0> <A1> <A2> <A3> <active> <all_dat> <hint>
 *        phase   : 0 IDLE / 1 HOMING / 2 PROBING / 3 GOTO / 4 POINT / 5 AUTO
 *        mode    : 1 = MAX (moc = Z-MIN, van CCW de HA goc cao) / 0 = MIN (moc = Z-MAX, van CW)
 *        idx     : goc nozzle DANG DUNG (0..3) -> panel ve dau '*'
 *        ref     : moc so sanh (mm, 3 so le)  <- DUNG BANG gia tri man 12864 dang hien
 *        d0..d3  : do lech tung goc so voi moc (mm, 3 so le); "---.---" = goc chua probe duoc
 *        A0..A3  : 1 = nguoi dung da bam DONE o goc do (coi nhu xong)
 *        active  : 1 = wizard dang chay
 *        all_dat : 1 = CA 4 GOC da dat -> man 12864 chi con nut BACK (panel cung vay)
 *        hint    : viec can lam o goc dang dung, 1 token KHONG co khoang trang. Marlin tinh san
 *                  (bam theo dung tramming_turn_cw + TRAM_DONE_MM cua man 12864) de panel khong
 *                  phai tu suy lai chieu van vit:
 *                    "DAT"       goc da dat (hoac ca 4 goc da dat)
 *                    "CW:0.150"  van CW 0.150 mm   (':' thay cho khoang trang)
 *                    "CCW:nhe"   van nhe theo CCW
 *                    "PROBE"     goc nay chua probe duoc -> bam PROBE
 *                    "ERR"       lan probe vua roi bi loi
 *                    "-"         khong co gi
 *   (3 field cuoi - active/all_dat/hint - la phan MO RONG so voi ban toi thieu; parser bo qua
 *    field la khong biet.)
 *
 *   Thu tu 4 goc = dung thu tu TRAMMING_POINT_XY trong Configuration_adv.h:
 *        0 = Back-Right (280,285)   1 = Back-Left (25,285)
 *        2 = Front-Left (25,25)     3 = Front-Right (280,25)
 */
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TRAM_PROTO_VERSION 1

// So goc cua wizard. PHAI bang G35_PROBE_COUNT (= COUNT(TRAMMING_POINT_XY)) phia Marlin.
#define TRAM_PROTO_POINTS  4

// ---------------------------------------------------------------------------
// Lenh panel -> Marlin
// ---------------------------------------------------------------------------
#define TRAM_CMD_PROBE    'P'
#define TRAM_CMD_DONE     'D'
#define TRAM_CMD_MINMAX   'M'
#define TRAM_CMD_RESTART  'R'
#define TRAM_CMD_BACK     'B'
#define TRAM_CMD_QUERY    'Q'

// ---------------------------------------------------------------------------
// phase (khop TR_IDLE..TR_AUTO cua tramming_run.h phia Marlin)
// ---------------------------------------------------------------------------
enum tram_phase_t : uint8_t {
  TRAM_PH_IDLE = 0,
  TRAM_PH_HOMING,
  TRAM_PH_PROBING,
  TRAM_PH_GOTO,
  TRAM_PH_POINT,
  TRAM_PH_AUTO
};

static inline const char *tram_phase_name(const uint8_t phase) {
  switch (phase) {
    case TRAM_PH_IDLE:    return "IDLE";
    case TRAM_PH_HOMING:  return "HOMING";
    case TRAM_PH_PROBING: return "PROBING";
    case TRAM_PH_GOTO:    return "GOTO";
    case TRAM_PH_POINT:   return "POINT";
    case TRAM_PH_AUTO:    return "AUTO";
    default:              return "?";
  }
}

// ---------------------------------------------------------------------------
// Trang thai (ca 2 phia dung chung struct nay)
// ---------------------------------------------------------------------------
typedef struct {
  uint8_t phase;                            // tram_phase_t
  uint8_t mode_max;                         // 1 = MAX, 0 = MIN
  uint8_t idx;                              // goc dang dung (0..TRAM_PROTO_POINTS-1)
  uint8_t active;                           // wizard dang chay
  uint8_t all_dat;                          // ca 4 goc da dat
  float   ref;                              // moc so sanh (mm)
  float   d[TRAM_PROTO_POINTS];             // do lech tung goc (mm)
  uint8_t ok[TRAM_PROTO_POINTS];            // 1 = goc da probe duoc
  uint8_t accepted[TRAM_PROTO_POINTS];      // 1 = da bam DONE
  char    hint[16];                         // viec can lam o goc dang dung (khong co khoang trang)
} tram_status_t;

// ---------------------------------------------------------------------------
// Tien ich so (KHONG dung %f: vua nang, vua khong on dinh giua cac toolchain)
// ---------------------------------------------------------------------------
// mm -> "0.123" / "-12.345" (3 so le, lam tron); |v| qua lon -> "----".
// Tra ve so ky tu da ghi (khong tinh '\0'). `out` phai >= 16 byte.
static inline int tram_fmt_mm(char *out, const float v) {
  long m = (long)(v * 1000.0f + (v >= 0.0f ? 0.5f : -0.5f));
  if (m > 9999999L || m < -9999999L) { memcpy(out, "----", 5); return 4; }

  const bool neg = (m < 0);
  unsigned long a = neg ? (unsigned long)(-m) : (unsigned long)m;
  unsigned long ip = a / 1000UL;
  const unsigned long fp = a % 1000UL;

  char dig[12];
  int dn = 0;
  do { dig[dn++] = (char)('0' + (int)(ip % 10UL)); ip /= 10UL; } while (ip);

  int n = 0;
  if (neg) out[n++] = '-';
  while (dn) out[n++] = dig[--dn];
  out[n++] = '.';
  out[n++] = (char)('0' + (int)(fp / 100UL));
  out[n++] = (char)('0' + (int)((fp / 10UL) % 10UL));
  out[n++] = (char)('0' + (int)(fp % 10UL));
  out[n] = '\0';
  return n;
}

// ---------------------------------------------------------------------------
// Marlin -> panel: tao 1 dong trang thai (co '\n' o cuoi)
// Tra ve so byte da ghi (khong tinh '\0'). `buf` nen >= 128 byte.
// ---------------------------------------------------------------------------
static inline int tram_status_format(char *buf, const int len, const tram_status_t *st) {
  if (len < 8) return 0;
  int n = 0;
  char mm[16];

  #define TRAM_APPEND(...) do { \
    if (n < len - 1) { \
      const int w = snprintf(buf + n, (size_t)(len - n), __VA_ARGS__); \
      if (w > 0) n += (w < (len - n)) ? w : (len - n - 1); \
    } \
  } while (0)

  TRAM_APPEND("TRAM %u %u %u", (unsigned)st->phase, (unsigned)(st->mode_max ? 1U : 0U), (unsigned)st->idx);

  tram_fmt_mm(mm, st->ref);
  TRAM_APPEND(" %s", mm);

  for (uint8_t i = 0; i < TRAM_PROTO_POINTS; i++) {
    if (st->ok[i]) tram_fmt_mm(mm, st->d[i]);
    else           memcpy(mm, "---.---", 8);
    TRAM_APPEND(" %s", mm);
  }

  for (uint8_t i = 0; i < TRAM_PROTO_POINTS; i++)
    TRAM_APPEND(" %u", (unsigned)(st->accepted[i] ? 1U : 0U));

  TRAM_APPEND(" %u %u %s\n", (unsigned)(st->active ? 1U : 0U), (unsigned)(st->all_dat ? 1U : 0U),
              st->hint[0] ? st->hint : "-");

  #undef TRAM_APPEND
  return n;
}

// ---------------------------------------------------------------------------
// Parser (panel dung de doc dong TRAM; host test cung dung)
// ---------------------------------------------------------------------------
// Token ke tiep (bo qua khoang trang). Tra ve NULL khi het dong.
static inline const char *tram_next_tok(const char **p, int *tlen) {
  const char *s = *p;
  while (*s == ' ' || *s == '\t' || *s == '\r') s++;
  if (!*s) { *p = s; *tlen = 0; return NULL; }
  const char *e = s;
  while (*e && *e != ' ' && *e != '\t' && *e != '\r') e++;
  *tlen = (int)(e - s);
  *p = e;
  return s;
}

// "0".."999999" -> 0..999999; khac -> -1
static inline int tram_tok_uint(const char *t, const int l) {
  if (l <= 0 || l > 6) return -1;
  int v = 0;
  for (int i = 0; i < l; i++) {
    if (t[i] < '0' || t[i] > '9') return -1;
    v = v * 10 + (int)(t[i] - '0');
  }
  return v;
}

// "[-]<int>[.<frac>]" -> mm. "*ok = false" khi token la "---.---" (goc chua probe duoc).
static inline float tram_tok_mm(const char *t, const int l, bool *ok) {
  *ok = true;
  if (l >= 3 && t[0] == '-' && t[1] == '-' && t[2] == '-') { *ok = false; return 0.0f; }

  int i = 0;
  bool neg = false;
  if (i < l && (t[i] == '-' || t[i] == '+')) { neg = (t[i] == '-'); i++; }

  long ip = 0;
  int digits = 0;
  while (i < l && t[i] >= '0' && t[i] <= '9') { ip = ip * 10 + (t[i] - '0'); i++; digits++; }

  long fp = 0, fmul = 1;
  int fdig = 0;
  if (i < l && t[i] == '.') {
    i++;
    while (i < l && t[i] >= '0' && t[i] <= '9' && fdig < 3) {
      fp = fp * 10 + (t[i] - '0'); fmul *= 10; i++; fdig++;
    }
  }

  if (!digits) { *ok = false; return 0.0f; }
  const float v = (float)ip + (float)fp / (float)fmul;
  return neg ? -v : v;
}

// Doc 1 dong "TRAM ..." (KHONG can '\n' o cuoi). true = dong hop le.
static inline bool tram_status_parse(const char *line, tram_status_t *st) {
  if (!line || !st) return false;

  const char *p = line;
  int l = 0;
  const char *t = tram_next_tok(&p, &l);
  if (!t || l != 4 || strncmp(t, "TRAM", 4) != 0) return false;

  int v[3];
  for (uint8_t i = 0; i < 3; i++) {
    t = tram_next_tok(&p, &l);
    if (!t) return false;
    v[i] = tram_tok_uint(t, l);
    if (v[i] < 0) return false;
  }
  st->phase    = (uint8_t)v[0];
  st->mode_max = (uint8_t)(v[1] ? 1U : 0U);
  st->idx      = (uint8_t)v[2];
  if (st->idx >= TRAM_PROTO_POINTS) st->idx = 0;

  t = tram_next_tok(&p, &l);
  if (!t) return false;
  bool ok = false;
  st->ref = tram_tok_mm(t, l, &ok);

  for (uint8_t i = 0; i < TRAM_PROTO_POINTS; i++) {
    t = tram_next_tok(&p, &l);
    if (!t) return false;
    st->d[i] = tram_tok_mm(t, l, &ok);
    st->ok[i] = ok ? 1U : 0U;
  }

  for (uint8_t i = 0; i < TRAM_PROTO_POINTS; i++) {
    t = tram_next_tok(&p, &l);
    if (!t) return false;
    const int a = tram_tok_uint(t, l);
    if (a < 0) return false;
    st->accepted[i] = (uint8_t)(a ? 1U : 0U);
  }

  // 3 field cuoi la phan mo rong -> thieu cung duoc
  st->active = 0;
  st->all_dat = 0;
  st->hint[0] = '\0';
  t = tram_next_tok(&p, &l);
  if (t) { const int a = tram_tok_uint(t, l); if (a >= 0) st->active = (uint8_t)(a ? 1U : 0U); }
  t = tram_next_tok(&p, &l);
  if (t) { const int a = tram_tok_uint(t, l); if (a >= 0) st->all_dat = (uint8_t)(a ? 1U : 0U); }
  t = tram_next_tok(&p, &l);
  if (t) {
    uint8_t n = 0;
    while (n < l && n < (uint8_t)(sizeof(st->hint) - 1U)) { st->hint[n] = t[n]; n++; }
    st->hint[n] = '\0';
  }

  return true;
}

// Ky tu lenh panel -> Marlin (tra ve 0 neu khong phai lenh). Nhan ca chu thuong.
static inline char tram_cmd_parse(const char *line) {
  if (!line) return 0;
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  char c = *line;
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  switch (c) {
    case TRAM_CMD_PROBE:
    case TRAM_CMD_DONE:
    case TRAM_CMD_MINMAX:
    case TRAM_CMD_RESTART:
    case TRAM_CMD_BACK:
    case TRAM_CMD_QUERY:
      return c;
    default:
      return 0;
  }
}

// Nhu tram_cmd_parse nhung CHI nhan dong DUNG 1 ky tu lenh (ngoai khoang trang).
// Phia Marlin dung ban nay: neu co byte rac (vi du frame nhi phan lot vao) thi khong the
// giau thanh mot dong dung 1 ky tu -> khong bao gio chay nham lenh.
static inline char tram_cmd_parse_exact(const char *line) {
  if (!line) return 0;
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  char c = *line++;
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  switch (c) {
    case TRAM_CMD_PROBE:
    case TRAM_CMD_DONE:
    case TRAM_CMD_MINMAX:
    case TRAM_CMD_RESTART:
    case TRAM_CMD_BACK:
    case TRAM_CMD_QUERY:
      break;
    default:
      return 0;
  }
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  return (*line == '\0') ? c : 0;
}
