/**
 * cydlcd -- screens. Only the fields whose value changed are repainted: a full
 * 240x320 repaint is ~150 KB over SPI, far too slow to do every tick.
 */
#pragma once

#include "tft_ili9341.h"
#include "marlin_link.h"

enum UiMode : uint8_t { MODE_LIST, MODE_JOG, MODE_EDIT };

struct UiState {
  UiMode  mode = MODE_LIST;
  uint8_t sel = 0;              // selected entry of the action list
  uint8_t jog_axis = 0;         // 0 = X, 1 = Y, 2 = Z
  float   edit_value = 0;       // value being edited (MODE_EDIT)
  const char *edit_label = nullptr;
};

extern const uint8_t UI_ACTION_COUNT;
const char *ui_action_label(uint8_t i);

void ui_begin(ILI9341 &tft);
void ui_draw(const CydlcdState &st, const UiState &ui);
void ui_flash(const char *msg);          // one-line notice drawn at the bottom

// helpers shared with main.cpp
void fmtFloat(char *buf, float v, uint8_t decimals);
char *uiPutInt(char *p, int32_t v);      // "M104 S" + uiPutInt(p, 210)
