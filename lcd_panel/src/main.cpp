/**
 * lcd_panel -- panel firmware entry point.
 *
 * HAI KIEU, chon bang env trong lcd_panel/platformio.ini (khong phai sua code):
 *
 *   PANEL_USE_LVGL = 0  ("G-code", board nho: F411CC / F103C6)
 *       Panel tu hoi Marlin bang M105 / M114 / M27 (marlin_link.cpp) va tu ve (ui.cpp).
 *       Nhe, chay duoc tren F103C6 32K, nhung khong lay duoc du lieu nang.
 *
 *   PANEL_USE_LVGL = 1  ("PUSH", board lon: F407 / ESP32-CYD, va F411 neu muon)
 *       Marlin chu dong day frame nhi phan xuong (ExtUI panel_serial); panel nhan trong
 *       panel_link.cpp va ve bang LVGL (ui_lvgl.cpp). Panel chi gui G-code cho HANH DONG.
 *       => co du du lieu de ve moi khoi (file SD, bed mesh, PID/steps/mm, prompt...).
 */
#include <Arduino.h>

#include "panel_config.h"

#if PANEL_USE_LVGL

  #include "panel_link.h"
  #include "tram_link.h"          // chan doan: dem dong/byte TRAM nhan duoc
  #include "ui_lvgl.h"

  #if defined(PANEL_CYD_DEBUG)
    #include <esp_system.h>
  #endif

  void setup() {
    #if defined(PANEL_CYD_DEBUG)
      Serial.begin(115200);
      delay(300);
      Serial.println();
      Serial.println("[CYD] ===== setup bat dau (LVGL + push) =====");
      Serial.printf("[CYD] reset reason = %d (1=poweron,3=sw,4=panic,5/6=wdt)\n", (int)esp_reset_reason());
      Serial.printf("[CYD] free heap = %u B\n", (unsigned)ESP.getFreeHeap());
    #endif
    DBG_BEGIN();
    panel_link_begin(PANEL_MARLIN_BAUD);
    #if defined(PANEL_CYD_DEBUG)
      Serial.println("[CYD] da begin Serial2 (GPIO22 TX / GPIO35 RX) @250000");
    #endif
    ui_lvgl_begin();
    #if defined(PANEL_CYD_DEBUG)
      Serial.printf("[CYD] ui_lvgl_begin() xong, free heap = %u B\n", (unsigned)ESP.getFreeHeap());
    #endif
    DBGLN("panel ready (LVGL, push mode)");
  }

  void loop() {
    #if defined(PANEL_CYD_DEBUG)
      static uint32_t dbg_next = 0;
      if (millis() >= dbg_next) {
        dbg_next = millis() + 1000;
        Serial.printf("[CYD] loop OK | link PUSH=%d TRAM=%d rx-active=%d state=%u rx=%u err=%u heap=%u | TRAM dong=%u loi=%u byte=%u cach-day=%ums\n",
                      (int)panel.linked, (int)tram.linked, (int)tram.rx_active, (unsigned)panel.state,
                      (unsigned)panel.rx_frames, (unsigned)panel.rx_errors, (unsigned)ESP.getFreeHeap(),
                      (unsigned)tram.lines, (unsigned)tram.errors, (unsigned)tram.raw_bytes,
                      (unsigned)(millis() - tram.last_rx_ms));
      }
    #endif
    panel_link_task();     // nhan frame tu Marlin + giu PING/lien ket
    ui_lvgl_task();        // LVGL: ve man hinh, xu ly encoder/touch
  }

#else   // ---------- G-code mode (UI tu ve) ----------

  #include "tft_ili9341.h"
  #include "marlin_link.h"
  #include "encoder.h"
  #include "ui.h"

  static ILI9341   tft;
  static MarlinLink link;
  static UiState   ui;

  // -------------------------------------------------------------------------
  // encoder: rotation comes from encoder.cpp (interrupt-driven, validated);
  // the push button is debounced here
  // -------------------------------------------------------------------------
  static bool     btn_down = false, btn_last = false;
  static uint32_t btn_change_ms = 0, btn_down_ms = 0;
  static bool     click = false, long_click = false;

  static void btn_begin() {
    pinMode(ENC_PIN_SW, INPUT_PULLUP);
    btn_last = digitalRead(ENC_PIN_SW) == LOW;
  }

  static void btn_poll() {
    const bool now = digitalRead(ENC_PIN_SW) == LOW;
    const uint32_t ms = millis();
    if (now != btn_last && ms - btn_change_ms > 20) {
      btn_change_ms = ms;
      btn_last = now;
      if (now) { btn_down = true; btn_down_ms = ms; }
      else if (btn_down) {
        btn_down = false;
        if (ms - btn_down_ms >= 700) long_click = true; else click = true;
      }
    }
  }

  // -------------------------------------------------------------------------
  // actions
  // -------------------------------------------------------------------------
  enum {
    ACT_HOME, ACT_PLA, ACT_PETG, ACT_ABS, ACT_COOLDOWN,
    ACT_PAUSE, ACT_RESUME, ACT_STOP, ACT_JOG, ACT_SET_NOZ, ACT_SET_BED, ACT_INFO
  };

  static void preheat(uint16_t nozzle, uint16_t bed, const char *tag) {
    char cmd[20], *p;
    p = cmd; p = (char*)memcpy(p, "M104 S", 6) + 6; p = uiPutInt(p, nozzle); *p = '\0';
    link.send(cmd);
    p = cmd; p = (char*)memcpy(p, "M140 S", 6) + 6; p = uiPutInt(p, bed); *p = '\0';
    link.send(cmd);
    ui_flash(tag);
  }

  static void run_action(uint8_t i) {
    switch (i) {
      case ACT_HOME:     link.send(F("G28"));  ui_flash("HOME ALL"); break;
      case ACT_PLA:      preheat(210, 60,  "PLA 210/60");   break;
      case ACT_PETG:     preheat(240, 80,  "PETG 240/80");  break;
      case ACT_ABS:      preheat(250, 100, "ABS 250/100");  break;
      case ACT_COOLDOWN: link.send(F("M104 S0")); link.send(F("M140 S0")); ui_flash("COOLDOWN"); break;
      case ACT_PAUSE:    link.send(F("M25"));  ui_flash("PAUSE");  break;
      case ACT_RESUME:   link.send(F("M24"));  ui_flash("RESUME"); break;
      case ACT_STOP:     link.send(F("M524")); ui_flash("STOP PRINT"); break;
      case ACT_JOG:      ui.mode = MODE_JOG; ui.jog_axis = 0; break;
      case ACT_SET_NOZ:  ui.mode = MODE_EDIT; ui.edit_label = "NOZZLE"; ui.edit_value = link.state().nozzle_target; break;
      case ACT_SET_BED:  ui.mode = MODE_EDIT; ui.edit_label = "BED";    ui.edit_value = link.state().bed_target; break;
      case ACT_INFO:     link.send(F("M115")); ui_flash("INFO -> console"); break;
    }
  }

  // Jog in relative mode, but always close with G90 so a dropped line cannot leave the
  // machine in relative mode (the link queue holds 4 lines, this uses 3).
  static void jog(int8_t delta, uint16_t step) {
    char cmd[24], *p;
    p = cmd; p = (char*)memcpy(p, "G1 ", 3) + 3;
    *p++ = "XYZ"[ui.jog_axis];
    p = uiPutInt(p, (int32_t)delta * (int32_t)step);
    p = (char*)memcpy(p, " F3000", 6) + 6; *p = '\0';
    if (link.send((const char*)"G91") && link.send(cmd) && link.send((const char*)"G90"))
      ui_flash("JOG");
    else
      ui_flash("link busy");
  }

  // -------------------------------------------------------------------------
  void setup() {
    DBG_BEGIN();

    btn_begin();
    enc_begin();
    tft.begin();
    ui_begin(tft);
    link.begin();

    ui_flash("san sang");
    DBGLN("panel ready");
  }

  void loop() {
    link.loop();
    btn_poll();

    const int8_t enc_delta = enc_detents();
    if (enc_delta) {
      const int8_t d = enc_delta;
      switch (ui.mode) {
        case MODE_LIST:
          ui.sel = (uint8_t)((ui.sel + UI_ACTION_COUNT + d) % UI_ACTION_COUNT);
          break;
        case MODE_JOG:
          jog(d > 0 ? 1 : -1, 1);
          break;
        case MODE_EDIT:
          ui.edit_value += d * 5;
          if (ui.edit_value < 0) ui.edit_value = 0;
          if (ui.edit_value > 300) ui.edit_value = 300;
          break;
      }
    }

    if (click) {
      click = false;
      switch (ui.mode) {
        case MODE_LIST: run_action(ui.sel); break;
        case MODE_JOG:
          if (++ui.jog_axis > 2) { ui.jog_axis = 0; ui.mode = MODE_LIST; }
          break;
        case MODE_EDIT: {
          char cmd[20], *p;
          p = cmd;
          p = (char*)memcpy(p, ui.edit_label && ui.edit_label[0] == 'N' ? "M104 S" : "M140 S", 6) + 6;
          p = uiPutInt(p, (int32_t)ui.edit_value);
          *p = '\0';
          link.send(cmd);
          ui_flash("SET TEMP");
          ui.mode = MODE_LIST;
        } break;
      }
    }

    if (long_click) {
      long_click = false;
      if (ui.mode != MODE_LIST) { ui.mode = MODE_LIST; ui_flash("back"); }
    }

    static uint32_t last_draw = 0;
    if (millis() - last_draw >= UI_REFRESH_MS) {
      last_draw = millis();
      ui_draw(link.state(), ui);
    }
  }

#endif  // PANEL_USE_LVGL
