/**
 * panel_serial.cpp -- ExtUI "panel_serial": day du lieu xuong man hinh rieng qua UART.
 *
 *  - Marlin -> panel: frame nhi phan (panel_protocol.h), day dinh ky + ngay khi co su kien.
 *  - panel -> Marlin: lenh G-code (chi de HANH DONG) + vai yeu cau (REQUEST) + PING.
 *
 * UART: PANEL_SERIAL = MSerial3 (USART3, PD8/PD9 tren MKS Monster8 V2). Marlin KHONG
 * dien giai duong nay nhu G-code (khong dung SERIAL_PORT_2) nen frame nhi phan di qua
 * nguyen ven; lenh G-code tu panel duoc dua vao hang doi bang queue.enqueue_one_now().
 */
#include "../../../inc/MarlinConfigPre.h"

#if BOTH(PANEL_SERIAL_UI, EXTENSIBLE_UI)

#include "panel_serial.h"
#include "../ui_api.h"
#include "../../../gcode/queue.h"
#if ENABLED(ASSISTED_TRAMMING_WIZARD)
  #include "../../../feature/tramming_report.h"   // dong "TRAM ..." + 6 lenh (dung chung panel_tram_link)
  #include "../../../feature/tramming_run.h"      // may trang thai Bed Tramming Wizard
#endif
#if ENABLED(SDSUPPORT)
  #include "../../../sd/cardreader.h"
#endif
#include "../../../MarlinCore.h"                    // printer_busy()
#include "../../../module/planner.h"                // planner.settings / finish_and_disable()
#include "../../../module/temperature.h"            // thermalManager.disable_all_heaters()
#if HAS_TRINAMIC_CONFIG
  #include "../../../module/stepper/indirection.h" // stepperX/Y/Z/E0.val_mA
#endif
#if HAS_FILAMENT_SENSOR
  #include "../../../feature/runout.h"             // runout.enabled
#endif

using namespace ExtUI;

namespace {

  // ---------------------------------------------------------------- TX ----
  // Bo tao/giai frame nam trong panel_protocol.h -> dung CHUNG voi firmware panel
  // (va duoc kiem bang chuong trinh test chay tren may tinh: lcd_panel/tools/).
  panel_tx_t tx;
  uint16_t tx_sent = 0, tx_dropped = 0;

  FORCE_INLINE void tx_begin(const panel_frame_t type) { panel_tx_begin(&tx, (uint8_t)type); }
  FORCE_INLINE void tx_u8(const uint8_t v)   { panel_tx_u8(&tx, v); }
  FORCE_INLINE void tx_u16(const uint16_t v) { panel_tx_u16(&tx, v); }
  FORCE_INLINE void tx_i16(const int16_t v)  { panel_tx_i16(&tx, v); }
  FORCE_INLINE void tx_u32(const uint32_t v) { panel_tx_u32(&tx, v); }
  FORCE_INLINE void tx_i32(const int32_t v)  { panel_tx_i32(&tx, v); }
  FORCE_INLINE void tx_str(const char *s, const uint8_t maxlen = PANEL_STR_MAX) { panel_tx_str(&tx, s, maxlen); }

  // ---- Hang doi TX (paced TX queue) ----
  // VI SAO CAN: TX ring buffer cua core STM32 (HardwareSerial, MarlinSerial thua ke) chi co
  // SERIAL_TX_BUFFER_SIZE = 64 byte (framework-arduinoststm32/cores/arduino/HardwareSerial.h
  // dong 43-58; da nang len 256 bang build_flags trong ini/features.ini, key PANEL_SERIAL_UI)
  // trong khi UART chay 250000 baud = 40 us/byte -> 64 byte can ~2.6 ms moi ra het duong truyen.
  // Cac cho goi LIEN TIEP thi nhanh hon UART rat nhieu:
  //   - push_settings(): 33 frame x 12 byte (6 overhead + 2 id + 4 i32) = 396 byte (~16 ms);
  //   - push_files():    moi file 1 frame (~60 byte) + 1 frame ket thuc 0xFF;
  //   - push_mesh():     7 hang luoi, moi hang 1 frame.
  // Truoc day tx_send() ghi thang ra UART va BO frame ngay khi availableForWrite() < do dai
  // frame => phan lon 33 dong setting bi mat (panel chi hien vai dong co gia tri) va co the mat
  // luon frame KET THUC danh sach file (panel khong bao gio biet danh sach da xong).
  // Tu day: tx_send() chi XEP HANG (khong ghi ra UART), tx_drain() o dau onIdle() day dan ra
  // UART theo dung toc do that cua UART -> khong mat frame nao.
  uint8_t txq_buf[PANEL_TXQ_SLOTS][PANEL_MAX_FRAME];  // moi slot giu NGUYEN 1 frame
  uint8_t txq_len[PANEL_TXQ_SLOTS];                   // do dai that cua frame trong slot
  uint8_t txq_off[PANEL_TXQ_SLOTS];                   // so byte da ghi ra (>0 chi khi buffer < frame)
  uint8_t txq_head = 0, txq_tail = 0, txq_count = 0;

  void tx_send() {
    const uint8_t total = panel_tx_finish(&tx);
    if (txq_count >= PANEL_TXQ_SLOTS) {
      // Hang doi day = UART that su khong theo kip, luc do moi bo frame.
      // (Loi cu: so availableForWrite() voi CA frame nen 25/33 dong setting bi bo ngay.)
      tx_dropped++;
      return;
    }
    for (uint8_t i = 0; i < total; i++) txq_buf[txq_tail][i] = tx.buf[i];
    txq_len[txq_tail] = total;
    txq_off[txq_tail] = 0;
    txq_tail = (uint8_t)((txq_tail + 1) % PANEL_TXQ_SLOTS);
    txq_count++;
  }

  // Day hang doi ra UART. TUYET DOI khong cho: HardwareSerial::write() cua core STM32 quay cho
  // (while) khi TX buffer day, nen chi ghi khi buffer con du cho. Khong while-cho, khong delay,
  // khong ghi blocking -> goi duoc tu onIdle() (main loop, KHONG phai trong ngat).
  static void tx_drain() {
    while (txq_count) {
      const uint8_t left = (uint8_t)(txq_len[txq_head] - txq_off[txq_head]);
      const int space = PANEL_SERIAL.availableForWrite();
      if (space <= 0) break;                                          // TX buffer day -> de lan sau
      if (space >= (int)left) {                                       // du cho CA frame
        PANEL_SERIAL.write(txq_buf[txq_head] + txq_off[txq_head], left);
        txq_off[txq_head] = 0;
        txq_head = (uint8_t)((txq_head + 1) % PANEL_TXQ_SLOTS);
        txq_count--;
        tx_sent++;
      }
      else {
        // Chi xay ra khi TX buffer nho hon 1 frame (khong xay ra voi SERIAL_TX_BUFFER_SIZE=256
        // >= PANEL_MAX_FRAME=198): ghi phan vua cho, phan con lai ghi o lan onIdle sau => khong
        // bao gio ket hang doi va khong bao gio block.
        PANEL_SERIAL.write(txq_buf[txq_head] + txq_off[txq_head], (size_t)space);
        txq_off[txq_head] = (uint8_t)(txq_off[txq_head] + (uint8_t)space);
      }
    }
  }

  // ------------------------------------------------- SD qua UART (proto 3) ----
  // Marlin KHONG tu doc the SPI nua (the nam o khe man hinh): moi sector 512 B duoc hoi panel qua
  // chinh UART nay. Xem panel_serial.h + lcd_panel/README.md muc 12.
  #if ENABLED(PANEL_SD_OVER_UART)
    void handle_frame();               // dinh nghia o duoi file (can cho sd_flush_deferred)
    uint8_t *s_sd_dst = nullptr;       // noi ghep 4 manh 128 B vao (do panel_serial_sd_read dat)
    uint8_t  s_sd_have = 0;            // bitmask: da nhan duoc manh nao
    uint8_t  s_sd_err  = 0;            // PANEL_SD_ST_* (khac OK = loi)
    bool     s_sd_done = false;        // du 4 manh
    bool     s_sd_busy = false;        // dang cho sector -> hoan cac frame mang LENH lai
    bool     s_sd_present = false;     // panel bao co the (tu lan panel_serial_sd_init)
    volatile uint8_t s_wr_status = 0xFF;   // ket qua lenh GHI (PANEL_F_SDACK): 0xFF = chua co
    volatile uint32_t s_wr_sector = 0;

    // Ghi THANG ra UART, khong qua hang doi TX: chi dung cho yeu cau SD (5 byte). Co tran thoi gian
    // + nuoi watchdog nen khong bao gio treo vinh vien du UART bi ket.
    void tx_write_now(const uint8_t *b, const uint16_t n) {
      uint16_t sent = 0;
      const millis_t t0 = millis();
      while (sent < n) {
        const int space = PANEL_SERIAL.availableForWrite();
        if (space > 0) {
          uint16_t k = (uint16_t)(n - sent);
          if (k > (uint16_t)space) k = (uint16_t)space;
          PANEL_SERIAL.write(b + sent, k);
          sent = (uint16_t)(sent + k);
        }
        else if (ELAPSED(millis(), t0 + 1000)) break;
        else hal.watchdog_refresh();
      }
    }

    // Cac frame mang LENH tu panel (G-code / REQUEST / TRAM_CMD / ANSWER) KHONG duoc xu ly ngay trong
    // luc dang cho sector: chung co the enqueue G-code, chay M500 hay nhay DFU... -> HOAN lai, onIdle
    // xu ly sau. Payload cua chung rat ngan (<= 48 byte) nen chi ton vai chuc byte RAM.
    struct sd_defer_t { uint8_t type, len, payload[PANEL_STR_MAX + 8]; };
    sd_defer_t s_defer[2];
    uint8_t    s_defer_n = 0;

    FORCE_INLINE bool is_cmd_frame(const uint8_t t) {
      return t == PANEL_F_GCODE || t == PANEL_F_REQUEST || t == PANEL_F_ANSWER || t == PANEL_F_TRAM_CMD;
    }
    // (sd_take_chunk / sd_defer / sd_flush_deferred nam SAU khai bao `rx` o duoi - chung dung rx.)
  #endif  // PANEL_SD_OVER_UART

  FORCE_INLINE int32_t scaled(const float v, const float s) {
    return (int32_t)(v * s + (v < 0 ? -0.5f : 0.5f));
  }

  // ------------------------------------------------------- trang thai ----
  bool flag_homing = false, flag_leveling = false, flag_killed = false, mesh_dirty = false;
  bool flag_homed = false, flag_steppers = false;

  uint8_t state_bits() {
    uint8_t f = 0;
    if (flag_homed)    f |= PANEL_SF_HOMED;
    if (flag_steppers) f |= PANEL_SF_STEPPERS;
    #if ENABLED(SDSUPPORT)
      if (isMediaInserted())     f |= PANEL_SF_MEDIA;
      if (isPrintingFromMedia()) f |= PANEL_SF_SD_PRINTING;
    #endif
    if (isPrintingPaused())    f |= PANEL_SF_PAUSED;
    if (flag_leveling)         f |= PANEL_SF_LEVELING;
    #if BOTH(HAS_LEVELING, HAS_MESH)
      if (getMeshValid()) f |= PANEL_SF_MESH_VALID;
    #endif
    #if HAS_FILAMENT_SENSOR
      if (getFilamentRunoutState()) f |= PANEL_SF_RUNOUT;
    #endif
    return f;
  }

  uint8_t state_idle() {
    if (flag_killed)            return PANEL_ST_KILLED;
    if (flag_homing)            return PANEL_ST_HOMING;
    if (flag_leveling)          return PANEL_ST_LEVELING;
    if (isPrintingPaused())     return PANEL_ST_PAUSED;
    if (isPrinting())           return PANEL_ST_PRINTING;
    TERN_(HAS_FILAMENT_SENSOR, if (getFilamentRunoutState()) return PANEL_ST_RUNOUT);
    return PANEL_ST_IDLE;
  }

  // ---------------------------------------------------------- frames ----
  uint8_t hello_flags() {
    uint8_t f = 0;
    #if ENABLED(SDSUPPORT)
      f |= PANEL_HAS_SD;
    #endif
    #if HAS_MESH
      f |= PANEL_HAS_MESH;
    #endif
    #if HAS_BED_PROBE
      f |= PANEL_HAS_PROBE;
    #endif
    #if HAS_FILAMENT_SENSOR
      f |= PANEL_HAS_FILAMENT;
    #endif
    #if HAS_HEATED_BED
      f |= PANEL_HAS_BED;
    #endif
    #if HAS_HEATED_CHAMBER
      f |= PANEL_HAS_CHAMBER;
    #endif
    #if ENABLED(POWER_LOSS_RECOVERY)
      f |= PANEL_HAS_POWERLOSS;
    #endif
    #if HAS_TRINAMIC_CONFIG
      f |= PANEL_HAS_TMC;
    #endif
    return f;
  }

  void push_hello() {
    tx_begin(PANEL_F_HELLO);
    tx_u16(PANEL_PROTO_VERSION);
    tx_u8(PANEL_PROTO_VERSION);
    tx_u8(hello_flags());
    #if HAS_HOTEND
      tx_u8(HOTENDS);
    #else
      tx_u8(0);
    #endif
    tx_u8(FAN_COUNT > 4 ? 4 : FAN_COUNT);
    // Ten hien tren panel: uu tien CUSTOM_MACHINE_NAME (ten MAY, vd "TBT"), khong co thi lay ten firmware.
    #ifdef CUSTOM_MACHINE_NAME
      tx_str(CUSTOM_MACHINE_NAME);
    #else
      tx_str((const char*)getFirmwareName_str());
    #endif
    // Proto >= 2: them 1 str = SHORT_BUILD_VERSION (vd "2.1.2"). Panel doc kieu "neu con byte thi doc"
    // nen panel ban cu (chi doc den het `name`) van chay binh thuong.
    tx_str(SHORT_BUILD_VERSION);
    tx_send();
  }

  void push_status() {
    tx_begin(PANEL_F_STATUS);
    tx_u8(state_idle());
    tx_u8(state_bits());
    tx_u8(getProgress_percent());
    tx_send();
  }

  void push_temps() {
    tx_begin(PANEL_F_TEMPS);
    tx_u8(0);                                              // cho trong: so luong (dien sau)
    uint8_t n = 0;
    #if HOTENDS
      tx_u8((uint8_t)(PANEL_TEMP_E0 + getActiveTool()));
      tx_i16((int16_t)scaled(getActualTemp_celsius(getActiveTool()), PANEL_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(getActiveTool()), PANEL_TEMP_SCALE));
      n++;
    #endif
    #if HAS_HEATED_BED
      tx_u8(PANEL_TEMP_BED);
      tx_i16((int16_t)scaled(getActualTemp_celsius(BED), PANEL_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(BED), PANEL_TEMP_SCALE));
      n++;
    #endif
    #if HAS_HEATED_CHAMBER
      tx_u8(PANEL_TEMP_CHAMBER);
      tx_i16((int16_t)scaled(getActualTemp_celsius(CHAMBER), PANEL_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(CHAMBER), PANEL_TEMP_SCALE));
      n++;
    #endif
    tx.buf[5] = n;                                         // dien so luong vao byte dau payload
    tx_send();
  }

  void push_position() {
    tx_begin(PANEL_F_POSITION);
    tx_i32(scaled(getAxisPosition_mm(X), PANEL_POS_SCALE));
    tx_i32(scaled(getAxisPosition_mm(Y), PANEL_POS_SCALE));
    tx_i32(scaled(getAxisPosition_mm(Z), PANEL_POS_SCALE));
    #if HAS_EXTRUDERS
      tx_i32(scaled(getAxisPosition_mm(getActiveTool()), PANEL_POS_SCALE));
    #else
      tx_i32(0);
    #endif
    tx_u16(0);                                             // feed mm/s*10: ExtUI khong co API -> 0
    tx_i16((int16_t)scaled(getFeedrate_percent(), PANEL_PCT_SCALE));
    #if HAS_EXTRUDERS
      tx_i16((int16_t)scaled((float)getFlow_percent(E0), PANEL_PCT_SCALE));
    #else
      tx_i16(0);
    #endif
    tx_send();
  }

  #if ENABLED(ASSISTED_TRAMMING_WIZARD)
    // ------------------------------------------------- Bed Tramming Wizard ----
    // Man TRAMMING cua panel (thay cho che do PANEL_TRAM_LINK cu): gui NGUYEN 1 dong ASCII
    // "TRAM ...\n" (tram_protocol.h) lam payload str cua PANEL_F_TRAM, ~5Hz. Panel gui lenh
    // hanh dong bang PANEL_F_TRAM_CMD ('P','D','M','R','B','Q').
    char tram_line[TRAM_REPORT_LINE_MAX];
    millis_t next_tram_ms = 0;

    void push_tram() {
      const int n = tramming_report_line(tram_line, (int)sizeof(tram_line));
      if (n <= 0) return;
      tx_begin(PANEL_F_TRAM);
      tx_str(tram_line, TRAM_REPORT_LINE_MAX);   // str: u8 len + n byte (co ca '\n' cuoi dong)
      // Dong TRAM ~70-75 byte -> di qua CHINH hang doi TX (tx_send) chu khong con ghi blocking
      // nhu tx_send_wait() cu: hang doi giu nguyen ca frame, tx_drain() day ra theo toc do UART
      // (250000 baud = 40 us/byte) nen dong dai bao nhieu cung khong bi cat va khong chan vong lap.
      tx_send();
    }
  #endif

  void push_progress() {
    tx_begin(PANEL_F_PROGRESS);
    tx_u8(getProgress_percent());
    tx_u32(getProgress_seconds_elapsed());
    #if ENABLED(SHOW_REMAINING_TIME)
      tx_u32(getProgress_seconds_remaining());
    #else
      tx_u32(0);                                   // SHOW_REMAINING_TIME dang tat
    #endif
    #if ENABLED(SDSUPPORT)
      tx_str(isPrintingFromMedia() ? (const char*)card.longest_filename() : (const char*)"");
    #else
      tx_str("");
    #endif
    tx_send();
  }

  void push_fans() {
    tx_begin(PANEL_F_FANS);
    const uint8_t n = FAN_COUNT > 4 ? 4 : FAN_COUNT;
    tx_u8(n);
    for (uint8_t i = 0; i < n; i++) {
      tx_u8(i);
      tx_i16((int16_t)scaled(getTargetFan_percent((fan_t)i), PANEL_PCT_SCALE));
    }
    tx_send();
  }

  // Trang thai endstop/probe kieu M119 (frame PANEL_F_ENDSTOPS).
  // Doc y nhu Marlin: module/endstops.cpp report_states() -- READ(PIN) != PIN_ENDSTOP_INVERTING.
  // LUU Y may nay: Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN (Configuration.h) => chan Z_MIN chinh la
  // dau probe, nen bao no voi id PANEL_ES_PROBE (0) va KHONG bao trung id PANEL_ES_Z_MIN (3).
  void push_endstops() {
    tx_begin(PANEL_F_ENDSTOPS);
    tx_u8(0);                                              // cho trong: so luong (dien sau)
    uint8_t n = 0;
    #if HAS_Z_MIN
      #if ENABLED(Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN)
        tx_u8(PANEL_ES_PROBE);
      #else
        tx_u8(PANEL_ES_Z_MIN);
      #endif
      tx_u8(READ(Z_MIN_PIN) != Z_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_X_MIN
      tx_u8(PANEL_ES_X_MIN);
      tx_u8(READ(X_MIN_PIN) != X_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_Y_MIN
      tx_u8(PANEL_ES_Y_MIN);
      tx_u8(READ(Y_MIN_PIN) != Y_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_X_MAX
      tx_u8(PANEL_ES_X_MAX);
      tx_u8(READ(X_MAX_PIN) != X_MAX_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_Y_MAX
      tx_u8(PANEL_ES_Y_MAX);
      tx_u8(READ(Y_MAX_PIN) != Y_MAX_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    tx.buf[5] = n;                                         // dien so luong vao byte dau payload
    tx_send();
  }

  // ---- Bang SETTING: gui DAN tung dong, khong bao gio burst ----
  // 33 dong x 12 byte (6 overhead + 2 id + 4 i32) = 396 byte; o 250000 baud phai mat ~16 ms
  // moi ra het duong truyen, trong khi vong lap sinh ra ca 33 dong chi mat vai chuc us.
  // Vi vay: push_settings() chi BAT DAU (hoac bat dau LAI) viec liet ke, con settings_pump()
  // (goi tu tx_pump moi lan onIdle) gui DUNG 1 dong. push_setting() dem buoc di qua danh sach
  // va chi gui dong thu `settings_target` cua lan pump hien tai -> khong dong nao bi mat.
  bool    settings_listing = false;
  uint8_t settings_sent = 0, settings_step = 0, settings_target = 0;

  void push_setting(const panel_setting_t id, const int32_t v) {
    if (settings_step++ != settings_target) return;   // chua toi luot -> chi dem buoc, khong gui
    tx_begin(PANEL_F_SETTING);
    tx_u16((uint16_t)id);
    tx_i32(v);
    tx_send();
    settings_sent++;
  }

  void push_settings() {                              // (ten cu) BAT DAU liet ke lai tu dau
    settings_sent = settings_step = settings_target = 0;
    settings_listing = true;
  }

  static void push_settings_body() {
    push_setting(PANEL_SET_Z_OFFSET, scaled(getZOffset_mm(), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_FEEDRATE_PCT, scaled(getFeedrate_percent(), PANEL_PCT_SCALE));
    #if HAS_EXTRUDERS
      push_setting(PANEL_SET_FLOW_PCT, scaled((float)getFlow_percent(E0), PANEL_PCT_SCALE));
      push_setting(PANEL_SET_STEPS_MM_E, scaled(getAxisSteps_per_mm(E0), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_MAXFEED_E, scaled(getAxisMaxFeedrate_mm_s(E0), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_MAXACCEL_E, scaled(getAxisMaxAcceleration_mm_s2(E0), PANEL_VAL_SCALE));
      #if ENABLED(CLASSIC_JERK)
        push_setting(PANEL_SET_JERK_E, scaled(getAxisMaxJerk_mm_s(E0), PANEL_VAL_SCALE));
      #endif
      #if ENABLED(LIN_ADVANCE)
        push_setting(PANEL_SET_LINEAR_ADV_E, scaled(getLinearAdvance_mm_mm_s(E0), PANEL_VAL_SCALE));
      #endif
      push_setting(PANEL_SET_TARGET_TEMP_E0, scaled(getTargetTemp_celsius(E0), PANEL_TEMP_SCALE));
      push_setting(PANEL_SET_FAN_PCT_E0, scaled(getTargetFan_percent(FAN0), PANEL_PCT_SCALE));
      #if ENABLED(PIDTEMP)
        push_setting(PANEL_SET_PID_E_KP, scaled(getPID_Kp(E0), PANEL_VAL_SCALE));
        push_setting(PANEL_SET_PID_E_KI, scaled(getPID_Ki(E0), PANEL_VAL_SCALE));
        push_setting(PANEL_SET_PID_E_KD, scaled(getPID_Kd(E0), PANEL_VAL_SCALE));
      #endif
    #endif
    push_setting(PANEL_SET_STEPS_MM_X, scaled(getAxisSteps_per_mm(X), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_STEPS_MM_Y, scaled(getAxisSteps_per_mm(Y), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_STEPS_MM_Z, scaled(getAxisSteps_per_mm(Z), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXFEED_X, scaled(getAxisMaxFeedrate_mm_s(X), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXFEED_Y, scaled(getAxisMaxFeedrate_mm_s(Y), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXFEED_Z, scaled(getAxisMaxFeedrate_mm_s(Z), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXACCEL_X, scaled(getAxisMaxAcceleration_mm_s2(X), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXACCEL_Y, scaled(getAxisMaxAcceleration_mm_s2(Y), PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXACCEL_Z, scaled(getAxisMaxAcceleration_mm_s2(Z), PANEL_VAL_SCALE));
    #if ENABLED(CLASSIC_JERK)
      push_setting(PANEL_SET_JERK_X, scaled(getAxisMaxJerk_mm_s(X), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_JERK_Y, scaled(getAxisMaxJerk_mm_s(Y), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_JERK_Z, scaled(getAxisMaxJerk_mm_s(Z), PANEL_VAL_SCALE));
    #elif HAS_JUNCTION_DEVIATION
      push_setting(PANEL_SET_JUNCTION_DEV, scaled(getJunctionDeviation_mm(), PANEL_VAL_SCALE));
    #endif
    #if HAS_HEATED_BED
      push_setting(PANEL_SET_TARGET_TEMP_BED, scaled(getTargetTemp_celsius(BED), PANEL_TEMP_SCALE));
      #if ENABLED(PIDTEMPBED)
        push_setting(PANEL_SET_PID_BED_KP, scaled(getBedPID_Kp(), PANEL_VAL_SCALE));
        push_setting(PANEL_SET_PID_BED_KI, scaled(getBedPID_Ki(), PANEL_VAL_SCALE));
        push_setting(PANEL_SET_PID_BED_KD, scaled(getBedPID_Kd(), PANEL_VAL_SCALE));
      #endif
    #endif
    #if HAS_BED_PROBE
      push_setting(PANEL_SET_PROBE_OFFSET_X, scaled(getProbeOffset_mm(X), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_PROBE_OFFSET_Y, scaled(getProbeOffset_mm(Y), PANEL_VAL_SCALE));
      push_setting(PANEL_SET_PROBE_OFFSET_Z, scaled(getProbeOffset_mm(Z), PANEL_VAL_SCALE));
    #endif
    // M204 S/R/T: gia toc khi in / khi retract / khi travel (planner.settings)
    push_setting(PANEL_SET_MAXACCEL_PRINT, scaled(planner.settings.acceleration, PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXACCEL_RETRACT, scaled(planner.settings.retract_acceleration, PANEL_VAL_SCALE));
    push_setting(PANEL_SET_MAXACCEL_TRAVEL, scaled(planner.settings.travel_acceleration, PANEL_VAL_SCALE));
    #if HAS_TRINAMIC_CONFIG
      // Dong dien RMS (mA) dang dat trong driver - M906. val_mA la cung gia tri menu TMC hien.
      #if AXIS_IS_TMC(X)
        push_setting(PANEL_SET_TMC_CURRENT_X, (int32_t)stepperX.val_mA);
      #endif
      #if AXIS_IS_TMC(Y)
        push_setting(PANEL_SET_TMC_CURRENT_Y, (int32_t)stepperY.val_mA);
      #endif
      #if AXIS_IS_TMC(Z)
        push_setting(PANEL_SET_TMC_CURRENT_Z, (int32_t)stepperZ.val_mA);
      #endif
      #if AXIS_IS_TMC(E0)
        push_setting(PANEL_SET_TMC_CURRENT_E, (int32_t)stepperE0.val_mA);
      #endif
    #endif
    #if HAS_FILAMENT_SENSOR
      push_setting(PANEL_SET_RUNOUT_EN, runout.enabled ? 1 : 0);
    #endif
  }

  // Gui DUNG 1 dong setting moi lan goi (xem tx_pump). Het danh sach thi tat co liet ke.
  static void settings_pump() {
    if (!settings_listing) return;
    settings_step = 0;
    settings_target = settings_sent;
    push_settings_body();
    if (settings_sent == settings_target) settings_listing = false;   // khong con dong nao
  }

  // ---- Luoi bed mesh: cung gui DAN tung hang (7 hang, moi hang 1 frame) ----
  // Truoc day 7 hang duoc day vao TX buffer trong 1 vong lap -> khi hang doi dang co frame khac
  // thi vai hang bi bo => panel ve thieu o luoi. Nay gui 1 hang moi lan onIdle.
  #if BOTH(HAS_LEVELING, HAS_MESH)
    bool    mesh_listing = false;
    uint8_t mesh_row = 0;

    static void mesh_pump() {
      if (!mesh_listing) return;
      bed_mesh_t &m = getMeshArray();                  // mang luoi song, doc tai thoi diem gui
      const uint8_t r = mesh_row++;
      tx_begin(PANEL_F_MESH);
      tx_u8(GRID_MAX_POINTS_Y);
      tx_u8(r);
      tx_u8(GRID_MAX_POINTS_X);
      for (uint8_t c = 0; c < GRID_MAX_POINTS_X; c++)
        tx_i16((int16_t)scaled(m[c][r], PANEL_VAL_SCALE));
      tx_send();
      if (mesh_row >= GRID_MAX_POINTS_Y) mesh_listing = false;
    }
  #endif

  void push_mesh() {                                 // (ten cu) BAT DAU gui lai toan bo luoi
    #if BOTH(HAS_LEVELING, HAS_MESH)
      mesh_row = 0;
      mesh_listing = true;
    #endif
  }

  void push_event(const panel_event_t ev, const uint16_t v1 = 0, const uint16_t v2 = 0, const char *text = nullptr) {
    tx_begin(PANEL_F_EVENT);
    tx_u8((uint8_t)ev);
    tx_u16(v1);
    tx_u16(v2);
    tx_str(text);
    tx_send();
  }

  void push_message(const panel_severity_t sev, const char *text) {
    tx_begin(PANEL_F_MESSAGE);
    tx_u8((uint8_t)sev);
    tx_str(text);
    tx_send();
  }

  void push_prompt(const panel_prompt_t type, const char *text) {
    tx_begin(PANEL_F_PROMPT);
    tx_u8(0);                       // prompt_id (v1: luon 0)
    tx_u8((uint8_t)type);
    tx_str(text);
    tx_send();
  }

  // -------------------------------------------------- danh sach file ----
  #if ENABLED(SDSUPPORT)
    FileList panel_files;

    // ---- Liet ke file: 1 frame/lan, KHONG burst ----
    // Moi file 1 frame (~60 byte voi PANEL_STR_MAX=48) + 1 frame ket thuc. Truoc day ca danh
    // sach duoc day trong 1 vong lap: TX buffer 64 byte (~2.6 ms moi ra het) khong theo kip nen
    // phan lon file bi bo va frame KET THUC (flags 0xFF) co the mat -> panel khong bao gio biet
    // danh sach da xong. Nay push_files() chi BAT DAU; files_pump() gui 1 file moi lan onIdle va
    // luon gui duoc frame ket thuc.
    bool     files_listing = false;
    uint16_t files_index = 0, files_total = 0;

    void push_files() {                               // (ten cu) BAT DAU liet ke thu muc hien tai
      panel_files.refresh();
      files_total = panel_files.count();
      files_index = 0;
      files_listing = true;                           // yeu cau moi khi dang liet ke -> bat dau lai
    }

    // Gui 1 frame file; het danh sach thi gui frame ket thuc. Goi tu tx_pump (onIdle).
    static void files_pump() {
      if (!files_listing) return;
      while (files_index < files_total) {
        const uint16_t i = files_index++;
        if (!panel_files.seek(i)) continue;           // file loi/thu muc vua doi -> bo qua
        tx_begin(PANEL_F_FILES);
        tx_u8(panel_files.isDir() ? 0x01 : 0x00);
        tx_i32(0);                                    // size: ExtUI khong cung cap -> 0
        tx_str(panel_files.longFilename());
        tx_send();
        return;
      }
      tx_begin(PANEL_F_FILES);                        // ket thuc danh sach (PHAI toi duoc panel)
      tx_u8(0xFF);
      tx_i32(0);
      tx_str("");
      tx_send();
      files_listing = false;
    }
  #endif

  // ------------------------------------------------- nap hang doi ----
  // Moi lan onIdle chi nap TOI DA 1 frame tu cac "nguon liet ke" (danh sach file / luoi mesh /
  // bang setting), va luon GIU TRONG 2 slot cho cac frame su kien / message / TRAM (di thang qua
  // tx_send). Nho vay du lieu hang loat khong bao gio chiem het hang doi lam mat su kien.
  static void tx_pump() {
    if (txq_count + 2 > PANEL_TXQ_SLOTS) return;      // giu 2 slot trong cho su kien/TRAM
    #if ENABLED(SDSUPPORT)
      if (files_listing) { files_pump(); return; }    // nguoi dung dang doi danh sach file
    #endif
    #if BOTH(HAS_LEVELING, HAS_MESH)
      if (mesh_listing) { mesh_pump(); return; }
    #endif
    settings_pump();
  }

  // ------------------------------------------------------- RX / lenh ----
  panel_rx_t rx;
  millis_t last_rx_ms = 0;
  bool     panel_alive = false;

  #if ENABLED(PANEL_SD_OVER_UART)
    // Nhan 1 manh cua sector (goi tu handle_frame khi dang cho du lieu)
    void sd_take_chunk() {
      if (rx.len < 7) return;
      const uint8_t st   = rx.payload[0];
      const uint8_t idx  = rx.payload[5];
      const uint8_t n    = rx.payload[6];
      if (st != PANEL_SD_ST_OK) {                    // ERR / NOCARD
        s_sd_err = st ? st : (uint8_t)PANEL_SD_ST_ERR;
        if (st == PANEL_SD_ST_NOCARD) s_sd_present = false;
        return;
      }
      if (!s_sd_dst || idx >= PANEL_SD_CHUNKS || n > PANEL_SD_CHUNK) return;
      if ((uint16_t)rx.len < (uint16_t)(7 + n)) return;   // frame thieu du lieu
      memcpy(s_sd_dst + (uint16_t)idx * PANEL_SD_CHUNK, &rx.payload[7], n);
      s_sd_have |= (uint8_t)(1u << idx);
      if (s_sd_have == (uint8_t)((1u << PANEL_SD_CHUNKS) - 1u)) { s_sd_done = true; s_sd_present = true; }
    }

    void sd_defer() {
      if (s_defer_n >= 2) return;                    // hang hoan day -> bo (cuc hiem)
      sd_defer_t &d = s_defer[s_defer_n++];
      uint8_t n = rx.len;
      if (n > (uint8_t)sizeof(d.payload)) n = (uint8_t)sizeof(d.payload);
      d.type = rx.type; d.len = n;
      memcpy(d.payload, rx.payload, n);
    }

    void sd_flush_deferred() {
      for (uint8_t i = 0; i < s_defer_n; i++) {
        rx.type = s_defer[i].type;
        rx.len  = s_defer[i].len;
        memcpy(rx.payload, s_defer[i].payload, s_defer[i].len);
        s_sd_busy = false;                           // de handle_frame xu ly that su
        handle_frame();
      }
      s_defer_n = 0;
    }
  #endif  // PANEL_SD_OVER_UART

  void do_request() {
    const uint8_t what = rx.len ? rx.payload[0] : (uint8_t)PANEL_REQ_FULL;
    switch (what) {
      case PANEL_REQ_FULL:
        push_hello(); push_status(); push_temps(); push_position(); push_fans(); push_settings();
        break;
      case PANEL_REQ_SETTINGS:
        push_settings();
        break;
      #if BOTH(HAS_LEVELING, HAS_MESH)
        case PANEL_REQ_MESH:
          push_mesh();
          break;
      #endif
      #if ENABLED(SDSUPPORT)
        case PANEL_REQ_FILES:
          push_files();
          break;
        case PANEL_REQ_FILES_UP: {
          panel_files.upDir();
          push_files();
          break;
        }
        case PANEL_REQ_FILES_DIR: {
          static char name[PANEL_STR_MAX + 1];
          const uint8_t n = (rx.len >= 2) ? MIN((uint8_t)(rx.len - 1), (uint8_t)PANEL_STR_MAX) : 0;
          for (uint8_t i = 0; i < n; i++) name[i] = (char)rx.payload[1 + i];
          name[n] = '\0';
          if (n) panel_files.changeDir(name);
          push_files();
          break;
        }
      #endif
      case PANEL_REQ_SAVE_SETTINGS:
        queue.enqueue_one_now("M500");
        push_message(PANEL_SEV_INFO, "Dang luu EEPROM (M500)");
        break;
      case PANEL_REQ_ABORT:
        queue.enqueue_one_now("M524");       // huy in tu SD
        queue.enqueue_one_now("M410");       // dung khan cap
        break;
      case PANEL_REQ_ENDSTOPS:
        push_endstops();
        break;
      #if ENABLED(STM32_DFU_REBOOT)
        case PANEL_REQ_DFU:
          // Nhay vao ROM DFU bootloader de nap lai firmware qua USB (khong can BOOT0+RESET).
          // Giong Marlin/src/lcd/menu/menu_advanced.cpp: CHI khi may dang ranh.
          if (printer_busy())
            push_message(PANEL_SEV_WARN, "May dang ban - khong vao DFU");
          else {
            thermalManager.disable_all_heaters();  // BAT BUOC: nhay kieu nay khong reset GPIO
            planner.finish_and_disable();          // dung chuyen dong + nha motor
            flashFirmware(0);                      // khong bao gio quay lai
          }
          break;
      #endif
      default: break;
    }
  }

  void handle_frame() {
    last_rx_ms = millis();
    panel_alive = true;
    #if ENABLED(PANEL_SD_OVER_UART)
      // Dang cho du lieu sector: tra loi SD xu ly NGAY, cac frame mang lenh thi HOAN lai.
      if (rx.type == PANEL_F_SDDATA) { sd_take_chunk(); return; }
      if (rx.type == PANEL_F_SDACK) {                 // tra loi lenh GHI
        if (rx.len >= 5) {
          s_wr_status = rx.payload[0];
          s_wr_sector = panel_u32(&rx.payload[1]);
        }
        return;
      }
      if (s_sd_busy && is_cmd_frame(rx.type)) { sd_defer(); return; }
    #endif
    switch ((panel_frame_t)rx.type) {
      case PANEL_F_PING: {
        tx_begin(PANEL_F_PONG);
        tx_u8(rx.len ? rx.payload[0] : 0);
        tx_send();
        break;
      }
      case PANEL_F_GCODE: {
        // 1 dong G-code -> hang doi cua Marlin (khong block vong lap UI)
        static char cmd[PANEL_MAX_PAYLOAD + 2];
        // Payload kieu "str" = u8 len + n byte  => PHAI BO byte len dau tien.
        // BUG CU (2026-10-13): lay ca byte len -> lenh thanh "\x03G28" (1 ky tu dieu khien o dau,
        // khong hien trong log) => parser KHONG thuc thi lenh. Day la ly do G28/G35/G1 tu panel
        // khong chay du frame van toi, trong khi lenh gui qua USB (khong co byte len) chay binh thuong.
        uint8_t n = 0;
        for (uint8_t i = 1; i < rx.len && n < PANEL_MAX_PAYLOAD; i++) {
          const char c = (char)rx.payload[i];
          cmd[n++] = (c == '\r' || c == '\n') ? ' ' : c;
        }
        cmd[n] = '\0';
        if (n) queue.enqueue_one_now(cmd);
        break;
      }
      case PANEL_F_REQUEST:
        do_request();
        break;
      case PANEL_F_ANSWER:
        if (rx.len >= 2 && rx.payload[1]) setUserConfirmed();
        break;
      #if ENABLED(ASSISTED_TRAMMING_WIZARD)
        case PANEL_F_TRAM_CMD:
          // 1 lenh wizard ('P','D','M','R','B','Q'). Lenh hop le tu dat co "gui lai ngay"
          // (tramming_report_force_take() o onIdle); 'Q' luon duoc xu ly ke ca khi busy.
          if (rx.len >= 1) tramming_report_cmd((char)rx.payload[0]);
          break;
      #endif
      default: break;                       // frame la khong ho tro -> bo qua
    }
  }

} // namespace

// ---------------------------------------------------------------------------
// Hook ExtUI
// ---------------------------------------------------------------------------
namespace ExtUI {

  void onStartup() {
    PANEL_SERIAL.begin(PANEL_BAUD);
    panel_rx_init(&rx);
    push_hello();
    push_event(PANEL_EV_BOOT);
  }

  void onIdle() {
    #if ENABLED(PANEL_SD_OVER_UART)
      // Cac frame mang lenh bi hoan lai trong luc cho sector SD -> xu ly tai day (dung ngu canh
      // vong lap chinh, khong phai trong disk_read).
      if (s_defer_n) sd_flush_deferred();
    #endif

    // Day hang doi TX ra UART TRUOC TIEN (khong cho doi - xem tx_drain): vua giai phong cho
    // trong hang doi, vua tra cac frame dang ton dong ra duong truyen.
    tx_drain();

    while (PANEL_SERIAL.available())
      if (panel_rx_byte(&rx, (uint8_t)PANEL_SERIAL.read())) handle_frame();

    const millis_t now = millis();
    if (panel_alive && ELAPSED(now, last_rx_ms + PANEL_TIMEOUT_MS)) panel_alive = false;

    static millis_t next_push_ms = 0, next_progress_ms = 0;
    if (!next_push_ms || ELAPSED(now, next_push_ms)) {
      next_push_ms = now + PANEL_PUSH_MS;
      push_status();
      push_temps();
      push_position();
      if (isPrinting()) push_fans();
    }
    if (isPrinting() && (!next_progress_ms || ELAPSED(now, next_progress_ms))) {
      next_progress_ms = now + PANEL_PROGRESS_MS;
      push_progress();
    }
    #if ENABLED(ASSISTED_TRAMMING_WIZARD)
      // TRAM: day 1 buoc may trang thai. ExtUI KHONG bien dich menu 12864 (menu_tramming.cpp)
      // nen day la nguon DUY NHAT tien buoc wizard - thieu dong nay thi wizard dung o TR_HOMING.
      // tick() cu cua panel_tram_link lam dung viec nay; ui_attached giu nguyen y nghia cu.
      if (tramming_run::active && !tramming_run::ui_attached) tramming_run::step();

      // Gui trang thai TRAM ~5Hz, va NGAY khi co lenh hop le tu panel ('Q' hoi trang thai).
      const bool tram_due = !next_tram_ms || ELAPSED(now, next_tram_ms);
      const bool tram_now = tramming_report_force_take();
      if (tram_due) next_tram_ms = now + TRAM_REPORT_PUSH_MS;
      if (tram_due || tram_now) push_tram();
    #endif
    if (mesh_dirty && !flag_leveling) {           // mesh vua doi xong -> day 1 lan
      mesh_dirty = false;
      push_mesh();
    }

    // Cuoi cung moi nap them 1 frame liet ke (setting/file/mesh) neu hang doi con cho: cac
    // frame su kien o tren luon duoc di truoc, con du lieu hang loat thi chay dan theo UART.
    tx_pump();
  }

  #if ENABLED(PANEL_SD_OVER_UART)
    // ------------------------------------------------------------------------
    // SD qua UART: Marlin hoi panel tung sector 512 B (4 manh 128 B).
    // Xem panel_serial.h + lcd_panel/README.md muc 12.
    // ------------------------------------------------------------------------
    bool panel_serial_sd_read(uint32_t sector, uint8_t *dst, uint32_t timeout_ms) {
      if (!dst) return false;

      // 1) Gui yeu cau (di THANG ra UART, khong qua hang doi -> khong bi tre sau cac frame PUSH)
      panel_tx_begin(&tx, PANEL_F_SDREQ);
      panel_tx_u8(&tx, (uint8_t)PANEL_SD_OP_READ);
      panel_tx_u32(&tx, sector);
      const uint8_t total = panel_tx_finish(&tx);

      s_sd_dst = dst; s_sd_have = 0; s_sd_err = 0; s_sd_done = false;
      const bool busy_prev = s_sd_busy;
      s_sd_busy = true;
      tx_write_now(tx.buf, total);

      // 2) Cho du 4 manh (bom RX + day TX + nuoi watchdog)
      const millis_t t0 = millis();
      while (!s_sd_done && !s_sd_err) {
        if (ELAPSED(millis(), t0 + timeout_ms)) break;
        while (PANEL_SERIAL.available())
          if (panel_rx_byte(&rx, (uint8_t)PANEL_SERIAL.read())) handle_frame();
        tx_drain();
        if (!s_sd_done && !s_sd_err) hal.watchdog_refresh();
      }

      s_sd_busy = busy_prev;
      s_sd_dst = nullptr;
      if (!s_sd_done || s_sd_err) {
        s_sd_present = (s_sd_err == PANEL_SD_ST_ERR);   // ERR = co the nhung loi; NOCARD = khong the
        return false;
      }
      return true;
    }

    // Ghi sector: GIAI DOAN 1 (P1) CHUA ho tro - Marlin chay voi SDCARD_READONLY (xem README muc 12).
    bool panel_serial_sd_write(uint32_t sector, const uint8_t *src, uint32_t timeout_ms) {
      if (!src) return false;
      // 1) Yeu cau GHI (5 byte, di thang ra UART)
      panel_tx_begin(&tx, PANEL_F_SDREQ);
      panel_tx_u8(&tx, (uint8_t)PANEL_SD_OP_WRITE);
      panel_tx_u32(&tx, sector);
      uint8_t total = panel_tx_finish(&tx);
      const bool busy_prev = s_sd_busy;
      s_sd_busy = true;
      s_wr_status = 0xFF;
      tx_write_now(tx.buf, total);

      // 2) Gui PANEL_SD_CHUNKS frame du lieu (moi frame 128 B)
      for (uint8_t idx = 0; idx < PANEL_SD_CHUNKS; idx++) {
        panel_tx_begin(&tx, PANEL_F_SDDATA);
        panel_tx_u8(&tx, (uint8_t)PANEL_SD_ST_OK);
        panel_tx_u32(&tx, sector);
        panel_tx_u8(&tx, idx);
        panel_tx_u8(&tx, PANEL_SD_CHUNK);
        panel_tx_raw(&tx, src + (uint16_t)idx * PANEL_SD_CHUNK, PANEL_SD_CHUNK);
        total = panel_tx_finish(&tx);
        tx_write_now(tx.buf, total);
      }

      // 3) Cho PANEL_F_SDACK
      const millis_t t0 = millis();
      while (s_wr_status == 0xFF) {
        if (ELAPSED(millis(), t0 + timeout_ms)) break;
        while (PANEL_SERIAL.available())
          if (panel_rx_byte(&rx, (uint8_t)PANEL_SERIAL.read())) handle_frame();
        tx_drain();
        if (s_wr_status == 0xFF) hal.watchdog_refresh();
      }
      s_sd_busy = busy_prev;
      const bool ok = (s_wr_status == PANEL_SD_ST_OK) && (s_wr_sector == sector);
      if (!ok) {
        SERIAL_ECHOLNPGM("PANEL SD: ghi sector ", sector, " THAT BAI (status=", s_wr_status, ")");
        if (s_wr_status == PANEL_SD_ST_NOCARD) s_sd_present = false;
      }
      return ok;
    }

    bool panel_serial_sd_init() {
      // Hoi panel bang 1 lan doc sector 0 (boot sector / BPB). Tra loi duoc = co the + UART song.
      static uint8_t tmp[PANEL_SD_SECTOR];
      const bool ok = panel_serial_sd_read(0, tmp, PANEL_SD_TIMEOUT_MS);
      s_sd_present = ok;
      return ok;
    }

    bool panel_serial_sd_present() { return s_sd_present; }
  #endif  // PANEL_SD_OVER_UART

  void onStatusChanged(const char * const msg) { push_message(PANEL_SEV_INFO, msg); }
  void onUserConfirmRequired(const char * const msg) {
    push_prompt(PANEL_PROMPT_USER_CONFIRM, msg);
  }

  void onPrinterKilled(FSTR_P const error, FSTR_P const component) {
    flag_killed = true;
    push_message(PANEL_SEV_FATAL, FTOP(error));
    push_event(PANEL_EV_PRINT_STOP);
    push_status();
    UNUSED(component);
  }

  void onMediaInserted() { push_event(PANEL_EV_MEDIA_INSERTED); push_status(); }
  void onMediaRemoved()  { push_event(PANEL_EV_MEDIA_REMOVED);  push_status(); }
  void onMediaError()    { push_event(PANEL_EV_MEDIA_ERROR);    push_message(PANEL_SEV_ERROR, "Loi the SD"); }

  void onPlayTone(const uint16_t frequency, const uint16_t duration) {
    push_event(PANEL_EV_TONE, frequency, duration);
  }

  void onPrintTimerStarted() { push_event(PANEL_EV_PRINT_START);  push_status(); }
  void onPrintTimerPaused()  { push_event(PANEL_EV_PRINT_PAUSE);  push_status(); }
  void onPrintTimerStopped() { push_event(PANEL_EV_PRINT_STOP);   push_status(); }
  void onPrintDone()         { push_event(PANEL_EV_PRINT_DONE);   push_status(); }

  void onFilamentRunout(const extruder_t extruder) {
    UNUSED(extruder);
    push_event(PANEL_EV_FILAMENT_RUNOUT);
    push_message(PANEL_SEV_ERROR, "Het filament");
    push_status();
  }

  void onHomingStart() { flag_homing = true;  push_event(PANEL_EV_HOMING_START); push_status(); }
  void onHomingDone()  { flag_homing = false; flag_homed = true; flag_steppers = true; push_event(PANEL_EV_HOMING_DONE); push_status(); }

  void onSteppersDisabled() { flag_steppers = false; flag_homed = false; push_event(PANEL_EV_STEPPERS_OFF); push_status(); }
  void onSteppersEnabled()  { flag_steppers = true;  push_event(PANEL_EV_STEPPERS_ON);  push_status(); }

  void onFactoryReset() { push_event(PANEL_EV_FACTORY_RESET); push_message(PANEL_SEV_WARN, "Da reset cai dat"); }

  void onStoreSettings(char *) { /* panel giu cai dat rieng cua no (flash cua panel) */ }
  void onLoadSettings(const char *) { }
  void onPostprocessSettings() { }
  void onSettingsStored(bool success) {
    push_event(PANEL_EV_SETTINGS_STORED);
    push_message(success ? PANEL_SEV_INFO : PANEL_SEV_ERROR, success ? "Da luu EEPROM" : "Luu EEPROM loi");
  }
  void onSettingsLoaded(bool success) {
    push_event(PANEL_EV_SETTINGS_LOADED);
    UNUSED(success);
    push_settings();
  }

  #if BOTH(HAS_LEVELING, HAS_MESH)
    void onLevelingStart() { flag_leveling = true;  push_event(PANEL_EV_LEVELING_START); push_status(); }
    void onLevelingDone() {
      flag_leveling = false;
      mesh_dirty = true;
      push_event(PANEL_EV_LEVELING_DONE);
      push_status();
    }
    void onMeshUpdate(const int8_t xpos, const int8_t ypos, const_float_t zval) {
      UNUSED(xpos); UNUSED(ypos); UNUSED(zval);
      mesh_dirty = true;                       // day 1 lan khi leveling xong, khong spam
    }
    void onMeshUpdate(const int8_t xpos, const int8_t ypos, const probe_state_t state) {
      UNUSED(xpos); UNUSED(ypos); UNUSED(state);
    }
  #endif

  #if ENABLED(POWER_LOSS_RECOVERY)
    void onPowerLossResume() { push_event(PANEL_EV_POWERLOSS_RESUME); push_message(PANEL_SEV_WARN, "Phuc hoi sau mat dien"); }
  #endif

  #if HAS_PID_HEATING
    void onPidTuning(const result_t rst) {
      push_event(PANEL_EV_PID_DONE, (uint16_t)rst);
      push_settings();
    }
  #endif

} // namespace ExtUI

#endif // PANEL_SERIAL_UI && EXTENSIBLE_UI
