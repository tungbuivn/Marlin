/**
 * cydlcd -- screens (implementation).
 *
 * Screen map (240x320 portrait, 8x8 font, 8 px per char; 2x font = 16 px per char):
 *   y   4   header: link status
 *   y  18   NOZZLE / BED temperature, 2x
 *   y  40   labels
 *   y  56   X / Y / Z position (14 px rows)
 *   y  98   separator
 *   y 102   progress bar
 *   y 122   action list (12 rows x 14 px) -- or the jog / editor MAN HINH CYD
 *   y 296   separator
 *   y 302   message line
 *
 * Only the fields whose value changed are repainted: a full 240x320 repaint is ~150 KB
 * over SPI, far too slow to do every tick.
 */
#include "ui.h"

static ILI9341 *tft = nullptr;

// ---------------------------------------------------------------------------
// action list (keep in sync with main.cpp)
// ---------------------------------------------------------------------------
static const char * const action_labels[] = {
  "HOME ALL", "PREHEAT PLA", "PREHEAT PETG", "PREHEAT ABS", "COOLDOWN",
  "PAUSE", "RESUME", "STOP PRINT", "JOG", "SET NOZZLE", "SET BED", "INFO"
};
const uint8_t UI_ACTION_COUNT = sizeof(action_labels) / sizeof(action_labels[0]);
const char *ui_action_label(uint8_t i) { return i < UI_ACTION_COUNT ? action_labels[i] : "?"; }

#define LIST_Y   122
#define LIST_DY  14

// ---------------------------------------------------------------------------
// Tiny string helpers. No snprintf/printf anywhere: on the 32 KB F103C6 build the
// printf machinery alone is several KB, and everything here is a fixed-shape line.
// ---------------------------------------------------------------------------
static char *putStr(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *putInt(char *p, int32_t v) {
  if (v < 0) { *p++ = '-'; v = -v; }
  char tmp[12];
  int8_t n = 0;
  if (v == 0) tmp[n++] = '0';
  while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
  while (n) *p++ = tmp[--n];
  return p;
}

char *uiPutInt(char *p, int32_t v) { return putInt(p, v); }

// "label" left-aligned in `width` columns (truncating), NUL terminated
static void putLabel(char *buf, const char *s, uint8_t width) {
  char *p = buf;
  uint8_t n = 0;
  while (*s && n < width) { *p++ = *s++; n++; }
  while (n++ < width) *p++ = ' ';
  *p = '\0';
}

// Integer-only float formatting (no libc float printer).
// Callers pass a buffer of at least 12 bytes; the widest output is "-300.000" + NUL.
// ---------------------------------------------------------------------------
void fmtFloat(char *buf, float v, uint8_t decimals) {
  const bool neg = v < 0;
  if (neg) v = -v;

  const int32_t div = decimals == 3 ? 1000 : decimals == 2 ? 100 : 10;
  const int32_t scaled = (int32_t)(v * (float)div + 0.5f);
  int32_t ip = scaled / div, fp = scaled % div;

  char *p = buf;
  if (neg) *p++ = '-';

  char tmp[12];
  int8_t n = 0;
  if (ip == 0) tmp[n++] = '0';
  while (ip > 0) { tmp[n++] = (char)('0' + (ip % 10)); ip /= 10; }
  while (n) *p++ = tmp[--n];

  if (decimals) {
    *p++ = '.';
    if (decimals == 3) *p++ = (char)('0' + (fp / 100) % 10);
    if (decimals >= 2) *p++ = (char)('0' + (fp / 10) % 10);
    *p++ = (char)('0' + fp % 10);
  }
  *p = '\0';
}

// right-aligned float in `width` columns, e.g. "    12.34"
static void putFloatRight(char *buf, float v, uint8_t decimals, uint8_t width) {
  char num[12];
  fmtFloat(num, v, decimals);
  const uint8_t len = (uint8_t)strlen(num);
  char *p = buf;
  for (uint8_t i = len; i < width; i++) *p++ = ' ';
  *p = '\0';
  putStr(p, num);
}

// ---------------------------------------------------------------------------
// cached values: only redraw what changed
// ---------------------------------------------------------------------------
struct Cache {
  float nozzle = -1, nozzle_t = -1, bed = -1, bed_t = -1;
  float x = -999, y = -999, z = -999;
  int8_t progress = -1;
  bool link = false;
  UiMode mode = MODE_LIST;
  uint8_t sel = 255, jog_axis = 255;
  float edit = -1;
  char msg[34] = "";
  bool msg_pending = true;
};
static Cache c;

static const char *flash_msg = nullptr;
static uint32_t flash_until = 0;

// ---------------------------------------------------------------------------
static void drawHeader(const CydlcdState &st) {
  if (st.link_ok == c.link) return;
  char line[24];
  putLabel(line, st.link_ok ? "cydlcd " CYDLCD_NAME "  OK" : "cydlcd " CYDLCD_NAME "  NO LINK", 20);
  tft->print(line, 4, 4, st.link_ok ? TFT_GREEN : TFT_RED, TFT_BLACK, 1);
  c.link = st.link_ok;
}

static void drawTemps(const CydlcdState &st) {
  char b[16], line[16];
  if (st.nozzle != c.nozzle || st.nozzle_target != c.nozzle_t) {
    char *p = putInt(b, (int32_t)(st.nozzle + 0.5f));
    *p++ = '/'; p = putInt(p, (int32_t)(st.nozzle_target + 0.5f)); *p = '\0';
    putLabel(line, b, 7);
    tft->print(line, 8, 18, TFT_ORANGE, TFT_BLACK, 2);
    c.nozzle = st.nozzle; c.nozzle_t = st.nozzle_target;
  }
  if (st.bed != c.bed || st.bed_target != c.bed_t) {
    char *p = putInt(b, (int32_t)(st.bed + 0.5f));
    *p++ = '/'; p = putInt(p, (int32_t)(st.bed_target + 0.5f)); *p = '\0';
    putLabel(line, b, 7);
    tft->print(line, 124, 18, TFT_CYAN, TFT_BLACK, 2);
    c.bed = st.bed; c.bed_t = st.bed_target;
  }
}

static void drawPos(const CydlcdState &st, bool force) {
  if (!force && st.x == c.x && st.y == c.y && st.z == c.z) return;
  char line[24];
  const float v[3] = { st.x, st.y, st.z };
  for (uint8_t i = 0; i < 3; i++) {
    line[0] = "XYZ"[i];
    line[1] = ' ';
    putFloatRight(line + 2, v[i], 2, 9);
    line[11] = '\0';
    tft->print(line, 8, 56 + i * 14, TFT_WHITE, TFT_BLACK, 1);
  }
  c.x = st.x; c.y = st.y; c.z = st.z;
}

static void drawProgress(const CydlcdState &st, bool force) {
  if (!force && st.progress == c.progress) return;
  tft->progressBar(8, 102, 224, 16, st.progress, TFT_GREEN, TFT_DKGREY, TFT_GREY);
  char b[8];
  char *p = b;
  if (st.progress < 100) *p++ = ' ';
  if (st.progress < 10)  *p++ = ' ';
  p = putInt(p, st.progress);
  *p++ = '%'; *p = '\0';
  tft->print(b, 96, 106, TFT_WHITE, TFT_DKGREY, 1);
  c.progress = st.progress;
}

static void drawList(const UiState &ui) {
  char line[24];
  for (uint8_t i = 0; i < UI_ACTION_COUNT; i++) {
    const bool sel = (i == ui.sel);
    line[0] = sel ? '>' : ' ';
    putLabel(line + 1, ui_action_label(i), 18);
    tft->print(line, 4, LIST_Y + i * LIST_DY, sel ? TFT_YELLOW : TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK, 1);
  }
}

static void drawJog(const CydlcdState &st, const UiState &ui) {
  char line[24], b[12];
  const float v = ui.jog_axis == 0 ? st.x : ui.jog_axis == 1 ? st.y : st.z;
  fmtFloat(b, v, 2);
  line[0] = 'J'; line[1] = 'O'; line[2] = 'G'; line[3] = ' ';
  line[4] = "XYZ"[ui.jog_axis];
  line[5] = ' ';
  putStr(line + 6, b);
  tft->print(line, 8, LIST_Y + 8, TFT_YELLOW, TFT_BLACK, 2);
  putLabel(line, "turn  = +-1 mm", 18);
  tft->print(line, 8, LIST_Y + 34, TFT_GREY, TFT_BLACK, 1);
  putLabel(line, "click = next", 18);
  tft->print(line, 8, LIST_Y + 48, TFT_GREY, TFT_BLACK, 1);
}

static void drawEdit(const UiState &ui) {
  char line[24], b[12];
  putLabel(line, ui.edit_label ? ui.edit_label : "SET", 18);
  tft->print(line, 8, LIST_Y + 8, TFT_GREY, TFT_BLACK, 1);
  fmtFloat(b, ui.edit_value, 0);
  tft->print(b, 8, LIST_Y + 24, TFT_ORANGE, TFT_BLACK, 2);
  putLabel(line, "turn=+-5 click=OK", 18);
  tft->print(line, 8, LIST_Y + 50, TFT_GREY, TFT_BLACK, 1);
}

static void clearScreen() { tft->fillRect(0, LIST_Y - 2, tft->width(), 176, TFT_BLACK); }

// ---------------------------------------------------------------------------
void ui_begin(ILI9341 &t) {
  tft = &t;
  tft->fillScreen(TFT_BLACK);

  tft->print("cydlcd " CYDLCD_NAME, 4, 4, TFT_GREEN, TFT_BLACK, 1);
  tft->print("NOZZLE", 8, 40, TFT_GREY, TFT_BLACK, 1);
  tft->print("BED", 124, 40, TFT_GREY, TFT_BLACK, 1);
  tft->drawFastHLine(0, 98, tft->width(), TFT_DKGREY);
  tft->drawFastHLine(0, 296, tft->width(), TFT_DKGREY);

  c = Cache();
  c.mode = MODE_JOG;                    // force the first ui_draw() to paint the cydlcd
}

void ui_draw(const CydlcdState &st, const UiState &ui) {
  if (!tft) return;

  drawHeader(st);
  drawTemps(st);
  drawPos(st, false);
  drawProgress(st, false);

  if (ui.mode != c.mode || ui.sel != c.sel || ui.jog_axis != c.jog_axis || ui.edit_value != c.edit) {
    clearScreen();
    switch (ui.mode) {
      case MODE_JOG:  drawJog(st, ui); break;
      case MODE_EDIT: drawEdit(ui); break;
      default:        drawList(ui); break;
    }
    c.mode = ui.mode; c.sel = ui.sel; c.jog_axis = ui.jog_axis; c.edit = ui.edit_value;
  }
  else if (ui.mode == MODE_JOG) {
    drawJog(st, ui);                    // the position changes while jogging
  }

  // bottom message line
  const char *msg = (flash_msg && millis() < flash_until) ? flash_msg : st.message;
  if (c.msg_pending || strncmp(msg, c.msg, sizeof(c.msg) - 1) != 0) {
    char line[36];
    putLabel(line, msg, 33);
    tft->print(line, 4, 302, TFT_YELLOW, TFT_BLACK, 1);
    strncpy(c.msg, msg, sizeof(c.msg) - 1);
    c.msg[sizeof(c.msg) - 1] = '\0';
    c.msg_pending = false;
  }
}

void ui_flash(const char *msg) {
  flash_msg = msg;
  flash_until = millis() + 2500;
}
