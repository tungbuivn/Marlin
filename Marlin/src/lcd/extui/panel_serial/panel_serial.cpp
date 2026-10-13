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

  void tx_send() {
    const uint8_t total = panel_tx_finish(&tx);
    // Khong cho block ca vong lap: TX buffer day thi bo frame nay (panel nhan lai o frame sau).
    if (PANEL_SERIAL.availableForWrite() < (int)total) { tx_dropped++; return; }
    PANEL_SERIAL.write(tx.buf, total);
    tx_sent++;
  }

  // Nhu tx_send nhung KHONG BAO GIO bo frame: ghi tung byte de write() tu cho den khi TX buffer
  // con cho. Dung cho dong "TRAM ..." (~85..110 byte) vi TX ring buffer cua core STM32 chi co
  // SERIAL_TX_BUFFER_SIZE = 64 byte -> tx_send() se bo frame VINH VIEN (xem panel_tram_link.cpp,
  // loi da gap that 2026-10-13). Ca dong ~4 ms @250000 baud - qua ngan de anh huong vong lap.
  #if ENABLED(ASSISTED_TRAMMING_WIZARD)
    void tx_send_wait() {
      const uint8_t total = panel_tx_finish(&tx);
      for (uint8_t i = 0; i < total; i++) PANEL_SERIAL.write(tx.buf[i]);
      tx_sent++;
    }
  #endif

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
      tx_send_wait();                            // dong dai > TX buffer -> khong duoc bo frame
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

  void push_setting(const panel_setting_t id, const int32_t v) {
    tx_begin(PANEL_F_SETTING);
    tx_u16((uint16_t)id);
    tx_i32(v);
    tx_send();
  }

  void push_settings() {
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
  }

  void push_mesh() {
    #if BOTH(HAS_LEVELING, HAS_MESH)
      bed_mesh_t &m = getMeshArray();
      for (uint8_t r = 0; r < GRID_MAX_POINTS_Y; r++) {
        tx_begin(PANEL_F_MESH);
        tx_u8(GRID_MAX_POINTS_Y);
        tx_u8(r);
        tx_u8(GRID_MAX_POINTS_X);
        for (uint8_t c = 0; c < GRID_MAX_POINTS_X; c++)
          tx_i16((int16_t)scaled(m[c][r], PANEL_VAL_SCALE));
        tx_send();
      }
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

    void push_files() {
      panel_files.refresh();
      const uint16_t n = panel_files.count();
      for (uint16_t i = 0; i < n; i++) {
        if (!panel_files.seek(i)) continue;
        tx_begin(PANEL_F_FILES);
        tx_u8(panel_files.isDir() ? 0x01 : 0x00);
        tx_i32(0);                                    // size: ExtUI khong cung cap -> 0
        tx_str(panel_files.longFilename());
        tx_send();
      }
      tx_begin(PANEL_F_FILES);                        // ket thuc danh sach
      tx_u8(0xFF);
      tx_i32(0);
      tx_str("");
      tx_send();
    }
  #endif

  // ------------------------------------------------------- RX / lenh ----
  panel_rx_t rx;
  millis_t last_rx_ms = 0;
  bool     panel_alive = false;

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
      default: break;
    }
  }

  void handle_frame() {
    last_rx_ms = millis();
    panel_alive = true;
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
        uint8_t n = rx.len < PANEL_MAX_PAYLOAD ? rx.len : (uint8_t)PANEL_MAX_PAYLOAD;
        for (uint8_t i = 0; i < n; i++) {
          const char c = (char)rx.payload[i];
          cmd[i] = (c == '\r' || c == '\n') ? ' ' : c;
        }
        cmd[n] = '\n';
        cmd[n + 1] = '\0';
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
  }

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
