/**
 * tbt_protocol.h -- Giao thuc ASCII cho link TRAMMING (man hinh rieng cydlcd <-> Marlin).
 *
 * FILE DUNG CHUNG: ban y nguyen nam o
 *      Marlin/src/feature/tbt_protocol.h      (Marlin)
 *      cydlcd/src/tbt_protocol.h           (MAN HINH CYD)
 *   Sua file nay thi phai COPY Y NGUYEN sang file kia. Header chi dung <stdint.h>/<stdio.h>/
 *   <string.h> (khong Arduino, khong Marlin) nen test duoc tren may tinh:
 *      pwsh cydlcd/tools/run_host_test.ps1
 *
 * VI SAO KHONG DUNG FRAME NHI PHAN (nhu cydlcd_protocol.h)?
 *   Link tramming chi can vai con so + vai lenh, nen:
 *     - doc duoc bang Serial Monitor -> go loi khong can thiet bi;
 *     - KHONG can ExtUI/CYDLCD_SERIAL_UI -> giu nguyen MarlinUI + menu 12864 (Tramming Wizard,
 *       Z ALIGN MANUAL...) ma nguoi dung dang dung. Day la ly do chinh cua giao thuc nay.
 *   Xem docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md.
 *
 * HUONG TRUYEN
 *
 *   MAN HINH CYD -> Marlin: 1 ky tu lenh + '\n'
 *        'P' = PROBE   : probe LAI goc dang dung (khong home lai)
 *        'D' = DONE    : goc nay xong -> di goc can chinh ke tiep; het goc -> home + probe 4 goc
 *        'M' = MIN/MAX : doi chien luoc MAX <-> MIN (moc Z-MIN <-> Z-MAX) + luu EEPROM
 *        'R' = RESTART : home + probe lai CA 4 GOC (neu wizard dang khong chay thi BAT DAU)
 *        'B' = BACK    : thoat wizard (stow probe + danh dau Z chua home)   <- mo rong
 *        'Q' = QUERY   : hoi trang thai -> Marlin gui lai NGAY 1 dong TRAM
 *     (ky tu khac / dong rong: bo qua. Nhan ca chu thuong.)
 *
 *   Marlin -> MAN HINH CYD: 1 dong trang thai, gui khi trang thai DOI + dinh ky ~5Hz:
 *        TBT <phase> <mode> <idx> <ref> <d0> <d1> <d2> <d3> <A0> <A1> <A2> <A3> <active> <all_dat> <hint>
 *        phase   : 0 IDLE / 1 HOMING / 2 PROBING / 3 GOTO / 4 POINT / 5 AUTO
 *        mode    : 1 = MAX (moc = Z-MIN, van CCW de HA goc cao) / 0 = MIN (moc = Z-MAX, van CW)
 *        idx     : goc nozzle DANG DUNG (0..3) -> MAN HINH CYD ve dau '*'
 *        ref     : moc so sanh (mm, 3 so le)  <- DUNG BANG gia tri man 12864 dang hien
 *        d0..d3  : do lech tung goc so voi moc (mm, 3 so le); "---.---" = goc chua probe duoc
 *        A0..A3  : 1 = nguoi dung da bam DONE o goc do (coi nhu xong)
 *        active  : 1 = wizard dang chay
 *        all_dat : 1 = CA 4 GOC da dat -> man 12864 chi con nut BACK (MAN HINH CYD cung vay)
 *        hint    : viec can lam o goc dang dung, 1 token KHONG co khoang trang. Marlin tinh san
 *                  (bam theo dung tramming_turn_cw + TBT_DONE_MM cua man 12864) de MAN HINH CYD khong
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

#define TBT_PROTO_VERSION 1

// So goc cua wizard. PHAI bang G35_PROBE_COUNT (= COUNT(TRAMMING_POINT_XY)) phia Marlin.
#define TBT_PROTO_POINTS  4

// ---------------------------------------------------------------------------
// Lenh MAN HINH CYD -> Marlin
// ---------------------------------------------------------------------------
#define TBT_CMD_PROBE    'P'
#define TBT_CMD_DONE     'D'
#define TBT_CMD_MINMAX   'M'
#define TBT_CMD_RESTART  'R'
#define TBT_CMD_BACK     'B'
#define TBT_CMD_QUERY    'Q'
// 2026-10-17: bam vao nhan goc tren man hinh CYD -> di chuyen toi goc do.
// Dung 4 ma rieng (chu so) de khong phai gui kem tham so (lenh TBT la 1 byte).
#define TBT_CMD_GOTO0    '0'
#define TBT_CMD_GOTO1    '1'
#define TBT_CMD_GOTO2    '2'
#define TBT_CMD_GOTO3    '3'
// 2026-10-17: bat/tat che do TU DI CHUYEN (nut AUTO tren man CYD).
#define TBT_CMD_AUTO     'A'
// 2026-10-17: doi che do MAX/MIN VA chay lai home + do 4 goc (sau khi nguoi dung xac nhan tren man
// hinh). Tach rieng de Marlin khong phai di chuyen toi goc nao truoc khi restart.
#define TBT_CMD_MODE_RESTART 'N'

// ---------------------------------------------------------------------------
// phase (khop TR_IDLE..TR_AUTO cua tbt_run.h phia Marlin)
// ---------------------------------------------------------------------------
enum tbt_phase_t : uint8_t {
  TBT_PH_IDLE = 0,
  TBT_PH_HOMING,
  TBT_PH_PROBING,
  TBT_PH_GOTO,
  TBT_PH_POINT,
  TBT_PH_AUTO
};

static inline const char *tbt_phase_name(const uint8_t phase) {
  switch (phase) {
    case TBT_PH_IDLE:    return "IDLE";
    case TBT_PH_HOMING:  return "HOMING";
    case TBT_PH_PROBING: return "PROBING";
    case TBT_PH_GOTO:    return "GOTO";
    case TBT_PH_POINT:   return "POINT";
    case TBT_PH_AUTO:    return "AUTO";
    default:              return "?";
  }
}

// ---------------------------------------------------------------------------
// Trang thai (ca 2 phia dung chung struct nay)
// ---------------------------------------------------------------------------
typedef struct {
  uint8_t phase;                            // tbt_phase_t
  uint8_t mode_max;                         // 1 = MAX, 0 = MIN
  uint8_t idx;                              // goc dang dung (0..TBT_PROTO_POINTS-1)
  uint8_t active;                           // wizard dang chay
  uint8_t all_dat;                          // ca 4 goc da dat
  float   ref;                              // moc so sanh (mm)
  float   d[TBT_PROTO_POINTS];             // do lech tung goc (mm)
  uint8_t ok[TBT_PROTO_POINTS];            // 1 = goc da probe duoc
  uint8_t accepted[TBT_PROTO_POINTS];      // 1 = da bam DONE
  char    hint[16];                         // viec can lam o goc dang dung (khong co khoang trang)
  // Nhiet do (2026-10-13): 4 token MO RONG o CUOI dong, don vi = do C * 100 (21050 = 210.50C).
  // Che do TRAM khong co frame PUSH (ExtUI) nen phai gui kem o day, neu khong man MAN HINH CYD khong co
  // nhiet do nao. Phia doc cu (khong biet 4 token nay) van chay binh thuong vi parser bo qua token thua.
  int16_t t_e0, t_bed, t_e0_tgt, t_bed_tgt;
  // 2026-10-17: 1 = che do TU DI CHUYEN dang BAT (nut AUTO). Token MO RONG nua o cuoi dong;
  // ban doc cu bo qua duoc, ban moi thieu token cung mac dinh ON.
  uint8_t auto_on;
} tbt_status_t;

// ---------------------------------------------------------------------------
// Tien ich so (KHONG dung %f: vua nang, vua khong on dinh giua cac toolchain)
// ---------------------------------------------------------------------------
// mm -> "0.123" / "-12.345" (3 so le, lam tron); |v| qua lon -> "----".
// Tra ve so ky tu da ghi (khong tinh '\0'). `out` phai >= 16 byte.
static inline int tbt_fmt_mm(char *out, const float v) {
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
// Marlin -> MAN HINH CYD: tao 1 dong trang thai (co '\n' o cuoi)
// Tra ve so byte da ghi (khong tinh '\0'). `buf` nen >= 128 byte.
// ---------------------------------------------------------------------------
static inline int tbt_status_format(char *buf, const int len, const tbt_status_t *st) {
  if (len < 8) return 0;
  int n = 0;
  char mm[16];

  #define TBT_APPEND(...) do { \
    if (n < len - 1) { \
      const int w = snprintf(buf + n, (size_t)(len - n), __VA_ARGS__); \
      if (w > 0) n += (w < (len - n)) ? w : (len - n - 1); \
    } \
  } while (0)

  TBT_APPEND("TBT %u %u %u", (unsigned)st->phase, (unsigned)(st->mode_max ? 1U : 0U), (unsigned)st->idx);

  tbt_fmt_mm(mm, st->ref);
  TBT_APPEND(" %s", mm);

  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) {
    if (st->ok[i]) tbt_fmt_mm(mm, st->d[i]);
    else           memcpy(mm, "---.---", 8);
    TBT_APPEND(" %s", mm);
  }

  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++)
    TBT_APPEND(" %u", (unsigned)(st->accepted[i] ? 1U : 0U));

  TBT_APPEND(" %u %u %s", (unsigned)(st->active ? 1U : 0U), (unsigned)(st->all_dat ? 1U : 0U),
              st->hint[0] ? st->hint : "-");

  // 4 token nhiet do (do C * 100) - xem chu thich trong tbt_status_t.
  TBT_APPEND(" %d %d %d %d", (int)st->t_e0, (int)st->t_bed, (int)st->t_e0_tgt, (int)st->t_bed_tgt);
  TBT_APPEND(" %u\n", (unsigned)(st->auto_on ? 1U : 0U));

  #undef TBT_APPEND
  return n;
}

// ---------------------------------------------------------------------------
// Parser (MAN HINH CYD dung de doc dong TRAM; host test cung dung)
// ---------------------------------------------------------------------------
// Token ke tiep (bo qua khoang trang). Tra ve NULL khi het dong.
// !!! `\n` PHAI nam trong bo dau phan cach !!!
//   Dong TBT ket thuc bang "... t_e0 t_bed t_e0_tgt t_bed_tgt\n" (xem tbt_status_format) -> token
//   CUOI CUNG dinh lien ky tu '\n'. Ban cu chi coi ' '/'\t'/'\r' la dau phan cach nen token cuoi la
//   "6000\n" -> tbt_tok_uint() gap '\n' -> tra -1 -> tbt_status_parse() `break` -> **t_bed_tgt luon
//   = 0**. Trieu chung da gap tren may that (2026-10-15): man hinh CYD hien "BED <nhiet> / 0 C" du
//   Marlin DA dat target 60 (frame PUSH bao tgt=6000). File nay PHAI GIONG Y NGUYEN
//   cydlcd/src/tbt_protocol.h (xem chu thich dau file).
static inline const char *tbt_next_tok(const char **p, int *tlen) {
  const char *s = *p;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  if (!*s) { *p = s; *tlen = 0; return NULL; }
  const char *e = s;
  while (*e && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n') e++;
  *tlen = (int)(e - s);
  *p = e;
  return s;
}

// "0".."999999" -> 0..999999; khac -> -1
static inline int tbt_tok_uint(const char *t, const int l) {
  if (l <= 0 || l > 6) return -1;
  int v = 0;
  for (int i = 0; i < l; i++) {
    if (t[i] < '0' || t[i] > '9') return -1;
    v = v * 10 + (int)(t[i] - '0');
  }
  return v;
}

// "[-]<int>[.<frac>]" -> mm. "*ok = false" khi token la "---.---" (goc chua probe duoc).
static inline float tbt_tok_mm(const char *t, const int l, bool *ok) {
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

// Doc 1 dong "TBT ..." (KHONG can '\n' o cuoi). true = dong hop le.
static inline bool tbt_status_parse(const char *line, tbt_status_t *st) {
  if (!line || !st) return false;

  const char *p = line;
  int l = 0;
  const char *t = tbt_next_tok(&p, &l);
  if (!t || l != 3 || strncmp(t, "TBT", 3) != 0) return false;

  int v[3];
  for (uint8_t i = 0; i < 3; i++) {
    t = tbt_next_tok(&p, &l);
    if (!t) return false;
    v[i] = tbt_tok_uint(t, l);
    if (v[i] < 0) return false;
  }
  st->phase    = (uint8_t)v[0];
  st->mode_max = (uint8_t)(v[1] ? 1U : 0U);
  st->idx      = (uint8_t)v[2];
  if (st->idx >= TBT_PROTO_POINTS) st->idx = 0;

  t = tbt_next_tok(&p, &l);
  if (!t) return false;
  bool ok = false;
  st->ref = tbt_tok_mm(t, l, &ok);

  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) {
    t = tbt_next_tok(&p, &l);
    if (!t) return false;
    st->d[i] = tbt_tok_mm(t, l, &ok);
    st->ok[i] = ok ? 1U : 0U;
  }

  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) {
    t = tbt_next_tok(&p, &l);
    if (!t) return false;
    const int a = tbt_tok_uint(t, l);
    if (a < 0) return false;
    st->accepted[i] = (uint8_t)(a ? 1U : 0U);
  }

  // 3 field cuoi la phan mo rong -> thieu cung duoc
  st->active = 0;
  st->all_dat = 0;
  st->hint[0] = '\0';
  t = tbt_next_tok(&p, &l);
  if (t) { const int a = tbt_tok_uint(t, l); if (a >= 0) st->active = (uint8_t)(a ? 1U : 0U); }
  t = tbt_next_tok(&p, &l);
  if (t) { const int a = tbt_tok_uint(t, l); if (a >= 0) st->all_dat = (uint8_t)(a ? 1U : 0U); }
  t = tbt_next_tok(&p, &l);
  if (t) {
    uint8_t n = 0;
    while (n < l && n < (uint8_t)(sizeof(st->hint) - 1U)) { st->hint[n] = t[n]; n++; }
    st->hint[n] = '\0';
  }

  // Nhiet do (4 token cuoi, MO RONG 2026-10-13): thieu / sai dinh dang cung khong lam dong loi
  // -> tuong thich nguoc voi ban Marlin cu (khong gui 4 token nay).
  st->t_e0 = st->t_bed = st->t_e0_tgt = st->t_bed_tgt = 0;
  for (uint8_t i = 0; i < 4; i++) {
    t = tbt_next_tok(&p, &l);
    if (!t) break;
    const int a = tbt_tok_uint(t, l);
    if (a < 0) break;
    switch (i) {
      case 0:  st->t_e0      = (int16_t)a; break;
      case 1:  st->t_bed     = (int16_t)a; break;
      case 2:  st->t_e0_tgt  = (int16_t)a; break;
      default: st->t_bed_tgt = (int16_t)a; break;
    }
  }

  // Token AUTO (2026-10-17): thieu -> mac dinh ON (tuong thich voi Marlin cu).
  st->auto_on = 1;
  t = tbt_next_tok(&p, &l);
  if (t) { const int a = tbt_tok_uint(t, l); if (a >= 0) st->auto_on = (uint8_t)(a ? 1U : 0U); }

  return true;
}

// Ky tu lenh MAN HINH CYD -> Marlin (tra ve 0 neu khong phai lenh). Nhan ca chu thuong.
static inline char tbt_cmd_parse(const char *line) {
  if (!line) return 0;
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  char c = *line;
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  switch (c) {
    case TBT_CMD_PROBE:
    case TBT_CMD_DONE:
    case TBT_CMD_MINMAX:
    case TBT_CMD_RESTART:
    case TBT_CMD_BACK:
    case TBT_CMD_QUERY:
    case TBT_CMD_GOTO0:
    case TBT_CMD_GOTO1:
    case TBT_CMD_GOTO2:
    case TBT_CMD_GOTO3:
    case TBT_CMD_AUTO:
    case TBT_CMD_MODE_RESTART:
      return c;
    default:
      return 0;
  }
}

// Nhu tbt_cmd_parse nhung CHI nhan dong DUNG 1 ky tu lenh (ngoai khoang trang).
// Phia Marlin dung ban nay: neu co byte rac (vi du frame nhi phan lot vao) thi khong the
// giau thanh mot dong dung 1 ky tu -> khong bao gio chay nham lenh.
static inline char tbt_cmd_parse_exact(const char *line) {
  if (!line) return 0;
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  char c = *line++;
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  switch (c) {
    case TBT_CMD_PROBE:
    case TBT_CMD_DONE:
    case TBT_CMD_MINMAX:
    case TBT_CMD_RESTART:
    case TBT_CMD_BACK:
    case TBT_CMD_QUERY:
      break;
    // 2026-10-17: lenh MOI phai duoc them o CA HAI ham parse (tbt_cmd_parse va tbt_cmd_parse_exact).
    // Thieu o day thi tbt_report_cmd() tu choi ngay -> bam nut tren CYD khong co tac dung gi.
    case TBT_CMD_GOTO0:
    case TBT_CMD_GOTO1:
    case TBT_CMD_GOTO2:
    case TBT_CMD_GOTO3:
    case TBT_CMD_AUTO:
    case TBT_CMD_MODE_RESTART:
      break;            // ⚠️ BAT BUOC: thieu break thi cac case tren ROI XUONG default -> return 0
    default:
      return 0;
  }
  while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
  return (*line == '\0') ? c : 0;
}
