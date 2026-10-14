/**
 * cydlcd -- MAN HINH CYD firmware entry point.
 *
 * HAI KIEU, chon bang env trong cydlcd/platformio.ini (khong phai sua code):
 *
 *   CYDLCD_USE_LVGL = 0  ("G-code", board nho: F411CC / F103C6)
 *       MAN HINH CYD tu hoi Marlin bang M105 / M114 / M27 (marlin_link.cpp) va tu ve (ui.cpp).
 *       Nhe, chay duoc tren F103C6 32K, nhung khong lay duoc du lieu nang.
 *
 *   CYDLCD_USE_LVGL = 1  ("PUSH", board lon: F407 / ESP32-CYD, va F411 neu muon)
 *       Marlin chu dong day frame nhi phan xuong (ExtUI cydlcd_serial); MAN HINH CYD nhan trong
 *       cydlcd_link.cpp va ve bang LVGL (ui_lvgl.cpp). MAN HINH CYD chi gui G-code cho HANH DONG.
 *       => co du du lieu de ve moi khoi (file SD, bed mesh, PID/steps/mm, prompt...).
 */
#include <Arduino.h>

#include "cydlcd_config.h"

#if CYDLCD_USE_LVGL

  #include "cydlcd_link.h"
  #include "cydlcd_port.h"        // cydlcd_millis() (log 1 Hz: tuoi cua dong TBT cuoi cung)
  #include "tbt_link.h"          // chan doan: dem dong/byte TBT nhan duoc
  #include "ui_lvgl.h"
  #include "net_wifi.h"           // quan ly WiFi DOC LAP voi BT (trang WiFi settings + net_wifi_state)
  #include "net_upload_http.h"    // web server tren cydlcd: PC/dien thoai gui file qua WiFi
  #include "cyd_sd.h"             // chan doan: cyd_sd_write_test() (lenh `UI WTEST`)
  #include <lvgl.h>              // lv_mem_monitor(): theo doi pool LVGL trong log 1 Hz

  #if defined(CYDLCD_CYD_DEBUG)
    #include <esp_system.h>
  #endif

  // Chan doan treo vong lap (2026-10-14 - panel tung dung im khong in log, khong tra loi console):
  //  - `s_stage` / `s_hung_s` nam trong RTC RAM (KHONG bi xoa boi reset mem/WDT, chi mat khi cat dien)
  //    -> lan khoi dong sau biet vong lap truoc dung o chang nao va da treo bao lau (in o setup()).
  //  - Task `cyd_diag_task()` (1 Hz, tren core 0) theo doi `s_loop_n`: vong lap khong chay > 15 s thi
  //    in ro chang dang dung roi TU KHOI DONG LAI -> panel khong bao gio "chet im" mai mai nua.
  //  - `s_ms[..]` do thoi gian tung chang o vong lap truoc, in o dong log 1 Hz (thay chang nao cham).
  RTC_NOINIT_ATTR uint32_t s_stage;      // 0 = het vong, 1 = console, 2 = link, 3 = http, 4 = ui
  RTC_NOINIT_ATTR uint32_t s_hung_s;     // so giay vong lap bi dung o lan treo truoc
  static uint32_t s_ms[5];               // thoi gian (ms) tung chang o vong lap truoc
  static volatile uint32_t s_loop_n = 0; // so vong lap da chay (task chan doan doc)
  static volatile uint32_t s_loop_ms = 0;// millis() o dau vong lap hien tai

  #if defined(CYDLCD_CYD_DEBUG)
    // Task chan doan: khong dung vong lap chinh nen van chay duoc khi vong lap bi chan.
    static void cyd_diag_task(void *) {
      uint32_t last_n = 0, stall = 0;
      bool exempt_logged = false;
      for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (s_loop_n != last_n) { last_n = s_loop_n; stall = 0; s_hung_s = 0; exempt_logged = false; continue; }
        // ---------------------------------------------------------------------------
        // MIEN TRU KHI DANG NHAN FILE UPLOAD (WiFi).
        // VI SAO: `server.handleClient()` cua WebServer (ESP32 core) xu ly CA BODY cua POST trong 1
        // lan goi (DONG BO) -> vong lap chinh DUNG HOP LE suot phien upload. Do dai ti le voi kich
        // thuoc file: ~13 s cho 2 MB, ~30 s cho 4,3 MB. Neu khong mien tru thi watchdog 15 s tuong
        // "treo" va TU KHOI DONG LAI giua phien -> PC thay "ket noi bi dut giua chung" va `SD.write()`
        // bao "ghi the TF loi" (loi THAT 2026-10-15: gui file 4,3 MB chet o ~46%, file 2 MB thi lot).
        // Trong luc upload thi KHONG the la treo that (moi buoc deu co tien do tu client) -> bo qua.
        // ---------------------------------------------------------------------------
        if (net_upload_active()) {
          // MIEN TRU CO DIEU KIEN: chi khi upload CON TIEN TRIEN (co du lieu moi trong 30 s).
          // Neu dung y nguyen (khong xet tien trien) thi khi upload bi ket giua duong (driver SD ket),
          // watchdog KHONG BAO GIO cuu -> panel TREO VINH VIEN, phai rut dien (da gap that 2026-10-15).
          const uint32_t idle_ms = (uint32_t)(millis() - net_upload_last_activity_ms());
          if (idle_ms < 30000u) {
            if (!exempt_logged) {
              Serial.println("[CYD] dang nhan file WiFi -> MIEN TRU watchdog (vong lap dung la binh thuong)");
              exempt_logged = true;
            }
            stall = 0; s_hung_s = 0;
            continue;
          }
          if (stall == 2 || stall == 5)
            Serial.printf("[CYD] CANH BAO: upload KHONG tien trien %lu s (chang %u) - se tu khoi dong lai\n",
                          (unsigned long)stall, (unsigned)s_stage);
        }
        stall++;
        s_hung_s = stall;                          // RTC: lan sau boot biet da treo bao lau
        if (stall == 2 || stall == 5 || stall == 10)
          Serial.printf("[CYD] CANH BAO: vong lap KHONG chay %lu s (dang o chang %u)\n",
                        (unsigned long)stall, (unsigned)s_stage);
        if (stall >= 15) {                         // treo qua lau -> tu phuc hoi
          Serial.printf("[CYD] TREO > 15 s o chang %u -> TU KHOI DONG LAI\n", (unsigned)s_stage);
          Serial.flush();
          esp_restart();
        }
      }
    }
  #endif

  void setup() {
    #if defined(CYDLCD_CYD_DEBUG)
      Serial.begin(115200);
      delay(300);
      Serial.println();
      Serial.println("[CYD] ===== setup bat dau (LVGL + push) =====");
      Serial.printf("[CYD] reset reason = %d (1=poweron,3=sw,4=panic,5/6=wdt)\n", (int)esp_reset_reason());
      // Chang cua vong lap TRUOC khi reset: RTC RAM khong bi xoa boi reset mem/WDT (chi mat khi cat dien).
      // 0 = het vong lap (khong treo), 1 = console COM7, 2 = link Marlin, 3 = web server WiFi, 4 = UI LVGL.
      Serial.printf("[CYD] vong lap truoc dung o chang = %u (0=het vong,1=console,2=link,3=http,4=ui)\n",
                    (unsigned)s_stage);
      if (s_hung_s) Serial.printf("[CYD] lan truoc vong lap bi dung %u s (da tu khoi dong lai)\n", (unsigned)s_hung_s);
      Serial.printf("[CYD] free heap = %u B\n", (unsigned)ESP.getFreeHeap());
      // Task chan doan treo: 1 Hz tren core 0 (core 1 la task loop cua Arduino).
      xTaskCreatePinnedToCore(cyd_diag_task, "cyd_diag", 4096, nullptr, 1, nullptr, 0);
    #endif
    s_stage = 0;
    s_hung_s = 0;
    DBG_BEGIN();
    cydlcd_link_begin(CYDLCD_MARLIN_BAUD);
    #if defined(CYDLCD_CYD_DEBUG)
      Serial.println("[CYD] da begin Serial2 (GPIO22 TX / GPIO35 RX) @250000");
    #endif
    ui_lvgl_begin();
    #if defined(CYDLCD_CYD_DEBUG)
      Serial.printf("[CYD] ui_lvgl_begin() xong, free heap = %u B\n", (unsigned)ESP.getFreeHeap());
    #endif
    // WiFi: quan ly ket noi (net_wifi.cpp) + web server nhan file (net_upload_http.cpp).
    // Khong lam gi neu tinh nang tuong ung = 0.
    net_wifi_begin();          // doc NVS + (CYDLCD_WIFI_AUTOSTART) vao mang nha - KHONG chan
    net_upload_http_begin();   // the TF + WebServer + mDNS (rong neu CYDLCD_WIFI_UPLOAD = 0)
    DBGLN("cydlcd ready (LVGL, push mode)");
  }


  void loop() {
    s_loop_n++;
    s_loop_ms = millis();
    static char s_forward_pending[40];     // lenh G-code nhan tu COM7 (console), cho gui sang Marlin
    #if defined(CYDLCD_CYD_DEBUG)
      static uint32_t dbg_next = 0;
      if (millis() >= dbg_next) {
        dbg_next = millis() + 1000;
        // rx/err la 2 CO bool trong cydlcd_data_t (cydlcd_link.h) -> in ra 0/1 (ep (int) de dung
        // voi ca bool lan uint16_t neu nhanh khac doi kieu).
        // t=<ms> = millis(): doc dong DAU TIEN de biet tu reset den luc loop() chay la bao lau
        // (sau khi hoan init BT ra khoi setup() - xem README 9.17).
        // Phan sau dau "|" chi co y nghia o che do TBT (link ASCII tramming): dem dong/byte de biet
        // day noi SONG hay CHET va sai khung hay khong - xem bang chan doan o cydlcd/README.md muc
        // "link PUSH/TBT". `cach-day` = ms tu dong TBT cuoi cung (lon dan = Marlin im).
        Serial.printf("[CYD] loop OK | t=%lu ms | link PUSH=%d TBT=%d rx=%d err=%d heap=%u"
                      " | TBT dong=%lu loi=%lu byte=%lu cach-day=%lums\n",
                      (unsigned long)millis(),
                      (int)cydlcd.linked, (int)tbt.linked, (int)cydlcd.rx_frames,
                      (int)cydlcd.rx_errors, (unsigned)ESP.getFreeHeap(),
                      (unsigned long)tbt.lines, (unsigned long)tbt.errors,
                      (unsigned long)tbt.raw_bytes,
                      (unsigned long)(cydlcd_millis() - tbt.last_rx_ms));
        // Thoi gian tung chang cua vong lap TRUOC (ms) - de thay chang nao bat dau cham/chan.
        // Thu tu: 1 = console COM7, 2 = link Marlin, 3 = web server WiFi, 4 = UI LVGL.
        Serial.printf("[CYD] chang: con=%lu link=%lu http=%lu ui=%lu ms | dang o chang %u\n",
                      (unsigned long)s_ms[1], (unsigned long)s_ms[2], (unsigned long)s_ms[3],
                      (unsigned long)s_ms[4], (unsigned)s_stage);
        // Pool LVGL: `used/total` + do phan manh. Chi so QUAN TRONG: pool day 100% =>
        // `lv_mem_alloc()` tra NULL => LVGL assert (truoc 2026-10-14 handler la `while(1)`
        // nen panel TREO IM LANG - xem ghi chu o lv_conf_cydlcd.h).
        {
          lv_mem_monitor_t lm;
          lv_mem_monitor(&lm);
          Serial.printf("[CYD] lvmem: %lu/%lu B (frag %u%%, max free %lu B)\n",
                        (unsigned long)(lm.total_size - lm.free_size), (unsigned long)lm.total_size,
                        (unsigned)lm.frag_pct, (unsigned long)lm.free_biggest_size);
        }
        net_upload_http_log();     // 1 dong: the TF + WiFi + phien upload qua web (rong neu tat)
      }
    #endif

    // Gui lenh G-code/M-code nhan duoc tu CONG USB CUA MAN HINH CYD (COM7) sang Marlin, bang DUNG duong ma
    // cac nut bam tren UI dung. Gui o nhip 500ms cho chac (2026-10-13) - xem cydlcd/README.md.
    static uint32_t fwd_next = 0;
    if (s_forward_pending[0] && millis() >= fwd_next) {
      fwd_next = millis() + 500;
      // Log 1 dong: de biet lenh go tu COM7 CO duoc gui sang Marlin hay khong (chan doan
      // "go lenh tren console ma may khong chay" - xem cydlcd/README.md bang lenh console).
      Serial.printf("[FWD] -> Marlin: %s\n", s_forward_pending);
      cydlcd_link_send_gcode(s_forward_pending);
      s_forward_pending[0] = '\0';
    }
    s_stage = 2;
    uint32_t t_stage = millis();
    cydlcd_link_task();     // nhan frame tu Marlin + giu PING/lien ket
    s_ms[2] = millis() - t_stage;
    s_stage = 3; t_stage = millis();
    net_upload_http_task();// web server (nhan file qua WiFi) -> ghi vao the SD cua man hinh
    s_ms[3] = millis() - t_stage;
    s_stage = 4; t_stage = millis();
    ui_lvgl_task();        // LVGL: ve man hinh, xu ly encoder/touch
    s_ms[4] = millis() - t_stage;
    s_stage = 1;           // con lai trong vong lap: doc console COM7

    // CONSOLE G-CODE QUA CONG USB CUA MAN HINH CYD (tinh nang, 2026-10-13): nhan tung dong G-code/M-code
    // tu COM7 roi chuyen tiep sang Marlin bang DUNG duong ma cac nut bam tren UI dung
    // (cydlcd_link_send_gcode) -> go lenh tu PC y nhu bam nut tren LCD.
    // Vi du: mo COM7 115200 (hoac dung serial-console.ps1), go "G28", "G35", "M220 S100",
    // "G91" -> "G1 X10 F600" -> "G90". Chuan bi o day, gui o nhip 500ms cho chac (xem dau loop()).
    // CHAP NHAN CA '\r' VA '\n' lam ket thuc dong: nhieu terminal (PuTTY/Arduino) gui chi CR,
    // neu chi nhan '\n' thi nhieu lenh bi don thanh 1 dong (da gap that 2026-10-13).
    static char s_fwd[40];
    static uint8_t s_fwd_len = 0;
    while (Serial.available()) {
      const char ch = (char)Serial.read();
      if (ch == '\r' || ch == '\n') {
        s_fwd[s_fwd_len] = '\0';
        if (s_fwd_len) {
          // LENH RIENG CUA MAN HINH (khong gui sang Marlin) - xem bang trong cydlcd/README.md.
#if defined(CYDLCD_BOARD_CYD)
          // (WIFI/PRE chi co tren board CYD: can WiFi + NVS + heap cua ESP32)
          if (strncmp(s_fwd, "WIFI", 4) == 0) {
            // LENH RIENG CUA MAN HINH CYD (khong gui sang Marlin) - thu WiFi/trang settings khong can UI:
            //   WIFI?                          -> in trang thai + IP + RSSI + ssid da luu
            //   WIFI SCAN                      -> quet mang (~2-4 s, CHAN) + liet ke kem RSSI
            //   WIFI CONNECT <ssid> <pass> [S] -> ket noi (co S = luu vao NVS de tu ket noi lai)
            //   WIFI OFF                       -> tat WiFi        WIFI FORGET -> xoa cau hinh da luu
            const char *arg = s_fwd + 4;
            while (*arg == ' ') arg++;
            if (strncmp(arg, "SCAN", 4) == 0) {
              // Quet KHONG CHAN + bom link trong luc cho: chung minh vong lap van chay (van tra loi
              // Marlin tung sector the SD) trong suot thoi gian quet mang.
              Serial.println("[WIFI] dang quet (khong chan vong lap)...");
              int n = -1;
              if (net_wifi_scan_begin() == 0) {
                const uint32_t t0 = millis();
                uint32_t last = 0;
                while ((n = net_wifi_scan_poll()) == -1 && (millis() - t0) < 30000u) {
                  cydlcd_link_task();          // <-- van phuc vu Marlin trong luc quet
                  net_upload_http_task();     // <-- van phuc vu web server
                  if (millis() - last > 1000u) { last = millis(); Serial.printf("[WIFI] ... dang quet (%lu ms)\n", (unsigned long)(millis() - t0)); }
                  delay(5);
                }
              }
              for (int i = 0; i < n; i++)
                Serial.printf("[WIFI]  %2d) %-32s %d dBm\n", i + 1,
                              net_wifi_scan_ssid(i) ? net_wifi_scan_ssid(i) : "?",
                              (int)net_wifi_scan_rssi(i));
              Serial.printf("[WIFI] quet xong: %d mang\n", n);
            } else if (strncmp(arg, "ASYNC", 5) == 0) {
              // CHAN DOAN: quet async voi muc "bom" khac nhau de tim tac vu pha phien quet.
              //   WIFI ASYNC 0 = chi poll (khong bom gi)
              //   WIFI ASYNC 1 = + cydlcd_link_task() (UART + tra loi the SD)
              //   WIFI ASYNC 2 = + net_upload_http_task() (web server + poll WiFi)  [giong vong lap that]
              const int mode = (arg[5] == ' ') ? atoi(arg + 6) : 0;
              Serial.printf("[WIFI] ASYNC che do bom = %d\n", mode);
              int n = -1;
              if (net_wifi_scan_begin() == 0) {
                const uint32_t t0 = millis();
                while ((n = net_wifi_scan_poll()) == -1 && (millis() - t0) < 30000u) {
                  if (mode >= 1) cydlcd_link_task();
                  if (mode >= 2) net_upload_http_task();
                  delay(5);
                }
              }
              Serial.printf("[WIFI] ASYNC mode=%d -> %d mang\n", mode, n);
            } else if (strncmp(arg, "CONNECT", 7) == 0) {
              const char *p1 = arg + 7;
              while (*p1 == ' ') p1++;
              char ssid[33], pass[65];
              size_t a = 0;
              while (*p1 && *p1 != ' ' && a < sizeof(ssid) - 1u) ssid[a++] = *p1++;
              ssid[a] = '\0';
              while (*p1 == ' ') p1++;
              size_t b = 0;
              while (*p1 && *p1 != ' ' && b < sizeof(pass) - 1u) pass[b++] = *p1++;
              pass[b] = '\0';
              const bool save = (strchr(p1, 'S') != nullptr);
              const bool ok = net_wifi_connect(ssid, pass, save);
              Serial.printf("[WIFI] connect \"%s\" (luu NVS=%d) -> %s; doi ket noi...\n",
                            ssid, (int)save, ok ? "da bat dau" : "TU CHOI");
            } else if (strncmp(arg, "OFF", 3) == 0) {
              net_wifi_disconnect(false);
              Serial.println("[WIFI] da tat");
            } else if (strncmp(arg, "FORGET", 6) == 0) {
              net_wifi_disconnect(true);
              Serial.println("[WIFI] da xoa cau hinh da luu");
            } else {
              Serial.printf("[WIFI] state=%u ssid=\"%s\" ip=%s rssi=%d saved=\"%s\" heap=%u B\n",
                            (unsigned)net_wifi_state(), net_wifi_ssid(), net_wifi_ip(),
                            (int)net_wifi_rssi(), net_wifi_saved_ssid(),
                            (unsigned)ESP.getFreeHeap());
            }
          } else if (strncmp(s_fwd, "PRE", 3) == 0) {
            // PRESET PREHEAT cua MAN HINH (kho rieng trong NVS namespace "ph" - xem ui_preheat.cpp).
            // LUU Y: lenh nay KHONG gui sang Marlin, chi doi gia tri preset cua man hinh.
            //   PRE?                               -> in ca 3 preset
            //   PRE <i 0..2> <nozzle> <bed> [fan]  -> dat + luu NVS (0=PLA, 1=PETG, 2=ABS)
            const char *a2 = s_fwd + 3;
            while (*a2 == ' ') a2++;
            if (*a2 == '?') {
              for (uint8_t i = 0; i < 3; i++) {
                uint16_t pn = 0, pb = 0; uint8_t pf = 0;
                if (ui_preheat_get(i, &pn, &pb, &pf))
                  Serial.printf("[PRE] %u) %-4s nozzle=%u C  bed=%u C  fan=%u\n",
                                (unsigned)i, ui_preheat_label(i), (unsigned)pn, (unsigned)pb, (unsigned)pf);
              }
            } else {
              int iv = 0, nv = 0, bv = 0, fv = 0;
              const int got = sscanf(a2, "%d %d %d %d", &iv, &nv, &bv, &fv);
              if (got >= 3 && iv >= 0 && iv < 3) {
                const bool ok = ui_preheat_set((uint8_t)iv, (uint16_t)(nv < 0 ? 0 : nv),
                                               (uint16_t)(bv < 0 ? 0 : bv), (uint8_t)(fv < 0 ? 0 : fv));
                Serial.printf("[PRE] dat preset %d (%s): nozzle=%d bed=%d fan=%d -> %s\n",
                              iv, ui_preheat_label((uint8_t)iv), nv, bv, fv, ok ? "OK" : "LOI");
              } else {
                Serial.println("[PRE] dung: PRE?   hoac   PRE <i 0..2> <nozzle> <bed> [fan]");
              }
            }
          } else if (strncmp(s_fwd, "UI", 2) == 0 && (s_fwd[2] == '\0' || s_fwd[2] == ' ' || s_fwd[2] == '?')) {
            // CHAN DOAN UI: mo 1 trang theo ten (`UI TRAMMING`, `UI SETTINGS`...) hoac in danh sach
            // trang (`UI` / `UI ?`). Dung de TAI HIEN loi UI tu PC, khong phai cham tay vao man hinh.
            const char *a3 = s_fwd + 2;
            while (*a3 == ' ') a3++;
            // `UI BOX` = in bo cuc trang dang mo (toa do/kich thuoc tung vat + thanh day + lop
            // phu/hop thoai) - man hinh CYD khong co framebuffer doc nguoc nen day la cach DUY NHAT
            // kiem tra bo cuc tu PC. `UI TESTBOX` = mo 1 hop thoai rong de kiem tra canh giua.
            // `UI TAP <dong>` = ban su kien CHAM vao 1 dong cua man FILE (kiem tra duong chon file).
            // `UI PRE <k>` = bam nut preset preheat PLA/PETG/ABS cua man NHIET DO.
            if (strcasecmp(a3, "BOX") == 0)          ui_lvgl_dump_layout();
            else if (strcasecmp(a3, "TESTBOX") == 0) ui_lvgl_test_dialog();
            else if (strncasecmp(a3, "TAP", 3) == 0) ui_lvgl_test_tap(a3 + 3);
            else if (strncasecmp(a3, "PRE", 3) == 0) ui_lvgl_test_preheat(a3 + 3);
            else if (strcasecmp(a3, "TEMPS") == 0)   ui_lvgl_dump_temps();
            else if (strcasecmp(a3, "BUSY") == 0)    ui_lvgl_dump_busy();
            else if (strcasecmp(a3, "SET") == 0)     page_settings_dump();
            // `UI MCELL <cot> <hang>` = mo hop sua 1 o mesh Y NHU VUA CHAM (kiem tra che do EDIT tu PC).
            else if (strncasecmp(a3, "MCELL", 5) == 0) mesh_test_cell(a3 + 5);
            // `UI NOTIFY [ms] <text>` = day 1 THONG BAO vao trang /status (KHONG hien hop tren LCD nua) de
            // kiem tra tu PC; khong tham so -> in dang luu; chi co so -> xoa. Xem ui_lvgl_test_notify().
            else if (strncasecmp(a3, "NOTIFY", 6) == 0) ui_lvgl_test_notify(a3 + 6);
            else if (strncasecmp(a3, "WTEST", 5) == 0) {
              // `UI WTEST [KB]` = ghi thu the (khong qua WiFi) de tach nguyen nhan loi ghi the.
              unsigned kb = 4096;
              if (a3[5]) sscanf(a3 + 5, "%u", &kb);
              cyd_sd_write_test(kb);
            }
            else                                     ui_lvgl_show_page_name(a3);
          } else
#endif
          {
            snprintf(s_forward_pending, sizeof(s_forward_pending), "%s", s_fwd);
          }
        }
        s_fwd_len = 0;
        continue;
      }
      if (s_fwd_len < sizeof(s_fwd) - 1) s_fwd[s_fwd_len++] = ch;
      else s_fwd_len = 0;
    }
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
    DBGLN("cydlcd ready");
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

#endif  // CYDLCD_USE_LVGL
