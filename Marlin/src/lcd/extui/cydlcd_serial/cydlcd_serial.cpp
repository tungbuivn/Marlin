/**
 * cydlcd_serial.cpp -- ExtUI "cydlcd_serial": day du lieu xuong man hinh rieng qua UART.
 *
 *  - Marlin -> man hinh CYD: frame nhi phan (cydlcd_protocol.h), day dinh ky + ngay khi co su kien.
 *  - man hinh CYD -> Marlin: lenh G-code (chi de HANH DONG) + vai yeu cau (REQUEST) + PING.
 *
 * UART: CYDLCD_SERIAL = MSerial3 (USART3, PD8/PD9 tren MKS Monster8 V2). Marlin KHONG
 * dien giai duong nay nhu G-code (khong dung SERIAL_PORT_2) nen frame nhi phan di qua
 * nguyen ven; lenh G-code tu man hinh CYD duoc dua vao hang doi bang queue.enqueue_one_now().
 */
#include "../../../inc/MarlinConfigPre.h"

#if BOTH(CYDLCD_SERIAL_UI, EXTENSIBLE_UI)

#include "cydlcd_serial.h"
#include "../ui_api.h"
#include "../../../gcode/queue.h"
#include "../../../gcode/gcode.h"                       // GcodeSuite::M524() (huy in - xu ly ngay)
#if ENABLED(ASSISTED_TRAMMING_WIZARD)
  #include "../../../feature/tbt_report.h"   // dong "TBT ..." + 6 lenh (dung chung cydlcd_tbt_link)
  #include "../../../feature/tbt_run.h"      // may trang thai Bed Tramming Wizard
#endif
#if ENABLED(SDSUPPORT)
  #include "../../../sd/cardreader.h"
#endif
#include "../../../MarlinCore.h"                    // printer_busy() / wait_for_heatup
#include "../../../module/motion.h"                  // quickstop_stepper() (M410 xu ly ngay)
#include "../../../module/planner.h"                // planner.settings / finish_and_disable()
#include "../../../module/temperature.h"            // thermalManager.disable_all_heaters()
#if HAS_TRINAMIC_CONFIG
  #include "../../../module/stepper/indirection.h" // stepperX/Y/Z/E0.val_mA
#endif
#if HAS_FILAMENT_SENSOR
  #include "../../../feature/runout.h"             // runout.enabled
#endif
#if ENABLED(ADVANCED_PAUSE_FEATURE)
  #include "../../../feature/pause.h"              // fc_settings[] (M603 L/U)
#endif

using namespace ExtUI;

namespace {

  // ---------------------------------------------------------------- TX ----
  // Bo tao/giai frame nam trong cydlcd_protocol.h -> dung CHUNG voi firmware man hinh CYD
  // (va duoc kiem bang chuong trinh test chay tren may tinh: cydlcd/tools/).
  cydlcd_tx_t tx;
  uint16_t tx_sent = 0, tx_dropped = 0;

  FORCE_INLINE void tx_begin(const cydlcd_frame_t type) { cydlcd_tx_begin(&tx, (uint8_t)type); }
  FORCE_INLINE void tx_u8(const uint8_t v)   { cydlcd_tx_u8(&tx, v); }
  FORCE_INLINE void tx_u16(const uint16_t v) { cydlcd_tx_u16(&tx, v); }
  FORCE_INLINE void tx_i16(const int16_t v)  { cydlcd_tx_i16(&tx, v); }
  FORCE_INLINE void tx_u32(const uint32_t v) { cydlcd_tx_u32(&tx, v); }
  FORCE_INLINE void tx_i32(const int32_t v)  { cydlcd_tx_i32(&tx, v); }
  FORCE_INLINE void tx_str(const char *s, const uint8_t maxlen = CYDLCD_STR_MAX) { cydlcd_tx_str(&tx, s, maxlen); }

  // ---- Hang doi TX (paced TX queue) ----
  // VI SAO CAN: TX ring buffer cua core STM32 (HardwareSerial, MarlinSerial thua ke) chi co
  // SERIAL_TX_BUFFER_SIZE = 64 byte (framework-arduinoststm32/cores/arduino/HardwareSerial.h
  // dong 43-58; da nang len 256 bang build_flags trong ini/features.ini, key CYDLCD_SERIAL_UI)
  // trong khi UART chay 250000 baud = 40 us/byte -> 64 byte can ~2.6 ms moi ra het duong truyen.
  // Cac cho goi LIEN TIEP thi nhanh hon UART rat nhieu:
  //   - push_settings(): 33 frame x 12 byte (6 overhead + 2 id + 4 i32) = 396 byte (~16 ms);
  //   - push_files():    moi file 1 frame (~60 byte) + 1 frame ket thuc 0xFF;
  //   - push_mesh():     7 hang luoi, moi hang 1 frame.
  // Truoc day tx_send() ghi thang ra UART va BO frame ngay khi availableForWrite() < do dai
  // frame => phan lon 33 dong setting bi mat (man hinh CYD chi hien vai dong co gia tri) va co the mat
  // luon frame KET THUC danh sach file (man hinh CYD khong bao gio biet danh sach da xong).
  // Tu day: tx_send() chi XEP HANG (khong ghi ra UART), tx_drain() o dau onIdle() day dan ra
  // UART theo dung toc do that cua UART -> khong mat frame nao.
  uint8_t txq_buf[CYDLCD_TXQ_SLOTS][CYDLCD_MAX_FRAME];  // moi slot giu NGUYEN 1 frame
  uint8_t txq_len[CYDLCD_TXQ_SLOTS];                   // do dai that cua frame trong slot
  uint8_t txq_off[CYDLCD_TXQ_SLOTS];                   // so byte da ghi ra (>0 chi khi buffer < frame)
  uint8_t txq_head = 0, txq_tail = 0, txq_count = 0;

  void tx_send() {
    const uint8_t total = cydlcd_tx_finish(&tx);
    if (txq_count >= CYDLCD_TXQ_SLOTS) {
      // Hang doi day = UART that su khong theo kip, luc do moi bo frame.
      // (Loi cu: so availableForWrite() voi CA frame nen 25/33 dong setting bi bo ngay.)
      tx_dropped++;
      return;
    }
    for (uint8_t i = 0; i < total; i++) txq_buf[txq_tail][i] = tx.buf[i];
    txq_len[txq_tail] = total;
    txq_off[txq_tail] = 0;
    txq_tail = (uint8_t)((txq_tail + 1) % CYDLCD_TXQ_SLOTS);
    txq_count++;
  }

  // Day hang doi ra UART. TUYET DOI khong cho: HardwareSerial::write() cua core STM32 quay cho
  // (while) khi TX buffer day, nen chi ghi khi buffer con du cho. Khong while-cho, khong delay,
  // khong ghi blocking -> goi duoc tu onIdle() (main loop, KHONG phai trong ngat).
  static void tx_drain() {
    while (txq_count) {
      const uint8_t left = (uint8_t)(txq_len[txq_head] - txq_off[txq_head]);
      const int space = CYDLCD_SERIAL.availableForWrite();
      if (space <= 0) break;                                          // TX buffer day -> de lan sau
      if (space >= (int)left) {                                       // du cho CA frame
        CYDLCD_SERIAL.write(txq_buf[txq_head] + txq_off[txq_head], left);
        txq_off[txq_head] = 0;
        txq_head = (uint8_t)((txq_head + 1) % CYDLCD_TXQ_SLOTS);
        txq_count--;
        tx_sent++;
      }
      else {
        // Chi xay ra khi TX buffer nho hon 1 frame (khong xay ra voi SERIAL_TX_BUFFER_SIZE=256
        // >= CYDLCD_MAX_FRAME=198): ghi phan vua cho, phan con lai ghi o lan onIdle sau => khong
        // bao gio ket hang doi va khong bao gio block.
        CYDLCD_SERIAL.write(txq_buf[txq_head] + txq_off[txq_head], (size_t)space);
        txq_off[txq_head] = (uint8_t)(txq_off[txq_head] + (uint8_t)space);
      }
    }
  }

  // ------------------------------------------------- SD qua UART (proto 3) ----
  // Marlin KHONG tu doc the SPI nua (the nam o khe man hinh): moi sector 512 B duoc hoi man hinh CYD qua
  // chinh UART nay. Xem cydlcd_serial.h + cydlcd/README.md muc 12.
  #if ENABLED(CYDLCD_SD_OVER_UART)
    void handle_frame();               // dinh nghia o duoi file (can cho sd_flush_deferred)
    uint8_t *s_sd_dst = nullptr;       // noi ghep 4 manh 128 B vao (do cydlcd_serial_sd_read dat)
    uint8_t  s_sd_have = 0;            // bitmask: da nhan duoc manh nao
    uint8_t  s_sd_err  = 0;            // CYDLCD_SD_ST_* (khac OK = loi)
    bool     s_sd_done = false;        // du 4 manh
    bool     s_sd_busy = false;        // dang cho sector -> hoan cac frame mang LENH lai
    bool     s_sd_present = false;     // cydlcd bao co the (tu lan cydlcd_serial_sd_init)
    volatile uint8_t s_wr_status = 0xFF;   // ket qua lenh GHI (CYDLCD_F_SDACK): 0xFF = chua co
    uint16_t s_cmd_drop = 0;               // so lenh thuong bi BO vi hang doi day qua 2 s (chan doan)
    volatile uint32_t s_wr_sector = 0;

    // Ghi THANG ra UART, khong qua hang doi TX: chi dung cho yeu cau SD (5 byte). Co tran thoi gian
    // + nuoi watchdog nen khong bao gio treo vinh vien du UART bi ket.
    void tx_write_now(const uint8_t *b, const uint16_t n) {
      uint16_t sent = 0;
      const millis_t t0 = millis();
      while (sent < n) {
        const int space = CYDLCD_SERIAL.availableForWrite();
        if (space > 0) {
          uint16_t k = (uint16_t)(n - sent);
          if (k > (uint16_t)space) k = (uint16_t)space;
          CYDLCD_SERIAL.write(b + sent, k);
          sent = (uint16_t)(sent + k);
        }
        else if (ELAPSED(millis(), t0 + 1000)) break;
        else hal.watchdog_refresh();
      }
    }

    // Cac frame mang LENH tu man hinh CYD (G-code / REQUEST / TBT_CMD / ANSWER) KHONG duoc xu ly ngay trong
    // luc dang cho sector: chung co the enqueue G-code, chay M500 hay nhay DFU... -> HOAN lai, onIdle
    // xu ly sau. Payload cua chung rat ngan (<= 48 byte) nen chi ton vai chuc byte RAM.
    struct sd_defer_t { uint8_t type, len, payload[CYDLCD_STR_MAX + 8]; };
    sd_defer_t s_defer[2];
    uint8_t    s_defer_n = 0;

    FORCE_INLINE bool is_cmd_frame(const uint8_t t) {
      return t == CYDLCD_F_GCODE || t == CYDLCD_F_REQUEST || t == CYDLCD_F_ANSWER || t == CYDLCD_F_TBT_CMD;
    }
    // (sd_take_chunk / sd_defer / sd_flush_deferred nam SAU khai bao `rx` o duoi - chung dung rx.)
  #endif  // CYDLCD_SD_OVER_UART

  FORCE_INLINE int32_t scaled(const float v, const float s) {
    return (int32_t)(v * s + (v < 0 ? -0.5f : 0.5f));
  }

  // ------------------------------------------------------- trang thai ----
  bool flag_homing = false, flag_leveling = false, flag_killed = false, mesh_dirty = false;
  bool flag_homed = false, flag_steppers = false;

  uint8_t state_bits() {
    uint8_t f = 0;
    if (flag_homed)    f |= CYDLCD_SF_HOMED;
    if (flag_steppers) f |= CYDLCD_SF_STEPPERS;
    #if ENABLED(SDSUPPORT)
      if (isMediaInserted())     f |= CYDLCD_SF_MEDIA;
      if (isPrintingFromMedia()) f |= CYDLCD_SF_SD_PRINTING;
    #endif
    if (isPrintingPaused())    f |= CYDLCD_SF_PAUSED;
    if (flag_leveling)         f |= CYDLCD_SF_LEVELING;
    #if BOTH(HAS_LEVELING, HAS_MESH)
      if (getMeshValid()) f |= CYDLCD_SF_MESH_VALID;
    #endif
    #if HAS_FILAMENT_SENSOR
      if (getFilamentRunoutState()) f |= CYDLCD_SF_RUNOUT;
    #endif
    return f;
  }

  uint8_t state_idle() {
    if (flag_killed)            return CYDLCD_ST_KILLED;
    if (flag_homing)            return CYDLCD_ST_HOMING;
    if (flag_leveling)          return CYDLCD_ST_LEVELING;
    if (isPrintingPaused())     return CYDLCD_ST_PAUSED;
    if (isPrinting())           return CYDLCD_ST_PRINTING;
    TERN_(HAS_FILAMENT_SENSOR, if (getFilamentRunoutState()) return CYDLCD_ST_RUNOUT);
    return CYDLCD_ST_IDLE;
  }

  // ---------------------------------------------------------- frames ----
  uint8_t hello_flags() {
    uint8_t f = 0;
    #if ENABLED(SDSUPPORT)
      f |= CYDLCD_HAS_SD;
    #endif
    #if HAS_MESH
      f |= CYDLCD_HAS_MESH;
    #endif
    #if HAS_BED_PROBE
      f |= CYDLCD_HAS_PROBE;
    #endif
    #if HAS_FILAMENT_SENSOR
      f |= CYDLCD_HAS_FILAMENT;
    #endif
    #if HAS_HEATED_BED
      f |= CYDLCD_HAS_BED;
    #endif
    #if HAS_HEATED_CHAMBER
      f |= CYDLCD_HAS_CHAMBER;
    #endif
    #if ENABLED(POWER_LOSS_RECOVERY)
      f |= CYDLCD_HAS_POWERLOSS;
    #endif
    #if HAS_TRINAMIC_CONFIG
      f |= CYDLCD_HAS_TMC;
    #endif
    return f;
  }

  void push_hello() {
    tx_begin(CYDLCD_F_HELLO);
    tx_u16(CYDLCD_PROTO_VERSION);
    tx_u8(CYDLCD_PROTO_VERSION);
    tx_u8(hello_flags());
    #if HAS_HOTEND
      tx_u8(HOTENDS);
    #else
      tx_u8(0);
    #endif
    tx_u8(FAN_COUNT > 4 ? 4 : FAN_COUNT);
    // Ten hien tren man hinh CYD: uu tien CUSTOM_MACHINE_NAME (ten MAY, vd "TBT"), khong co thi lay ten firmware.
    #ifdef CUSTOM_MACHINE_NAME
      tx_str(CUSTOM_MACHINE_NAME);
    #else
      tx_str((const char*)getFirmwareName_str());
    #endif
    // Proto >= 2: them 1 str = SHORT_BUILD_VERSION (vd "2.1.2"). Panel doc kieu "neu con byte thi doc"
    // nen man hinh CYD ban cu (chi doc den het `name`) van chay binh thuong.
    tx_str(SHORT_BUILD_VERSION);
    tx_send();
  }

  void push_status() {
    tx_begin(CYDLCD_F_STATUS);
    tx_u8(state_idle());
    tx_u8(state_bits());
    tx_u8(getProgress_percent());
    // [2026-10-15] Byte thu 4: BED LEVELING/MESH dang BAT hay TAT - de man hinh hien "UBL ON/OFF".
    // VI SAO: man BED MESH co nut "UBL ON" (G29 A) / "UBL OFF" (G29 D) nhung khong hien trang thai nao ->
    // nguoi dung bam xong khong biet da an hay chua. `getLevelingActive()` chinh la planner.leveling_active
    // (Marlin/src/lcd/extui/ui_api.cpp:916) nen phan anh dung ca G29 A/D, M420 S1/S0 va G29 sau khi do mesh.
    #if HAS_LEVELING
      tx_u8(getLevelingActive() ? 1 : 0);
    #else
      tx_u8(0);
    #endif
    tx_send();
  }

  void push_temps() {
    tx_begin(CYDLCD_F_TEMPS);
    tx_u8(0);                                              // cho trong: so luong (dien sau)
    uint8_t n = 0;
    #if HOTENDS
      tx_u8((uint8_t)(CYDLCD_TEMP_E0 + getActiveTool()));
      tx_i16((int16_t)scaled(getActualTemp_celsius(getActiveTool()), CYDLCD_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(getActiveTool()), CYDLCD_TEMP_SCALE));
      n++;
    #endif
    #if HAS_HEATED_BED
      tx_u8(CYDLCD_TEMP_BED);
      tx_i16((int16_t)scaled(getActualTemp_celsius(BED), CYDLCD_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(BED), CYDLCD_TEMP_SCALE));
      n++;
    #endif
    #if HAS_HEATED_CHAMBER
      tx_u8(CYDLCD_TEMP_CHAMBER);
      tx_i16((int16_t)scaled(getActualTemp_celsius(CHAMBER), CYDLCD_TEMP_SCALE));
      tx_i16((int16_t)scaled(getTargetTemp_celsius(CHAMBER), CYDLCD_TEMP_SCALE));
      n++;
    #endif
    tx.buf[5] = n;                                         // dien so luong vao byte dau payload
    tx_send();
  }

  void push_position() {
    tx_begin(CYDLCD_F_POSITION);
    tx_i32(scaled(getAxisPosition_mm(X), CYDLCD_POS_SCALE));
    tx_i32(scaled(getAxisPosition_mm(Y), CYDLCD_POS_SCALE));
    tx_i32(scaled(getAxisPosition_mm(Z), CYDLCD_POS_SCALE));
    #if HAS_EXTRUDERS
      tx_i32(scaled(getAxisPosition_mm(getActiveTool()), CYDLCD_POS_SCALE));
    #else
      tx_i32(0);
    #endif
    tx_u16(0);                                             // feed mm/s*10: ExtUI khong co API -> 0
    tx_i16((int16_t)scaled(getFeedrate_percent(), CYDLCD_PCT_SCALE));
    #if HAS_EXTRUDERS
      tx_i16((int16_t)scaled((float)getFlow_percent(E0), CYDLCD_PCT_SCALE));
    #else
      tx_i16(0);
    #endif
    tx_send();
  }

  #if ENABLED(ASSISTED_TRAMMING_WIZARD)
    // ------------------------------------------------- Bed Tramming Wizard ----
    // Man TRAMMING cua man hinh CYD (thay cho che do CYDLCD_TBT_LINK cu): gui NGUYEN 1 dong ASCII
    // "TBT ...\n" (tbt_protocol.h) lam payload str cua CYDLCD_F_TBT, ~5Hz. Panel gui lenh
    // hanh dong bang CYDLCD_F_TBT_CMD ('P','D','M','R','B','Q').
    char tbt_line[TBT_REPORT_LINE_MAX];
    millis_t next_tbt_ms = 0;

    void push_tram() {
      const int n = tbt_report_line(tbt_line, (int)sizeof(tbt_line));
      if (n <= 0) return;
      tx_begin(CYDLCD_F_TBT);
      tx_str(tbt_line, TBT_REPORT_LINE_MAX);   // str: u8 len + n byte (co ca '\n' cuoi dong)
      // Dong TBT ~70-75 byte -> di qua CHINH hang doi TX (tx_send) chu khong con ghi blocking
      // nhu tx_send_wait() cu: hang doi giu nguyen ca frame, tx_drain() day ra theo toc do UART
      // (250000 baud = 40 us/byte) nen dong dai bao nhieu cung khong bi cat va khong chan vong lap.
      tx_send();
    }
  #endif

  void push_progress() {
    tx_begin(CYDLCD_F_PROGRESS);
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
    tx_begin(CYDLCD_F_FANS);
    const uint8_t n = FAN_COUNT > 4 ? 4 : FAN_COUNT;
    tx_u8(n);
    for (uint8_t i = 0; i < n; i++) {
      tx_u8(i);
      tx_i16((int16_t)scaled(getTargetFan_percent((fan_t)i), CYDLCD_PCT_SCALE));
    }
    tx_send();
  }

  // Trang thai endstop/probe kieu M119 (frame CYDLCD_F_ENDSTOPS).
  // Doc y nhu Marlin: module/endstops.cpp report_states() -- READ(PIN) != PIN_ENDSTOP_INVERTING.
  // LUU Y may nay: Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN (Configuration.h) => chan Z_MIN chinh la
  // dau probe, nen bao no voi id CYDLCD_ES_PROBE (0) va KHONG bao trung id CYDLCD_ES_Z_MIN (3).
  void push_endstops() {
    tx_begin(CYDLCD_F_ENDSTOPS);
    tx_u8(0);                                              // cho trong: so luong (dien sau)
    uint8_t n = 0;
    #if HAS_Z_MIN
      #if ENABLED(Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN)
        tx_u8(CYDLCD_ES_PROBE);
      #else
        tx_u8(CYDLCD_ES_Z_MIN);
      #endif
      tx_u8(READ(Z_MIN_PIN) != Z_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_X_MIN
      tx_u8(CYDLCD_ES_X_MIN);
      tx_u8(READ(X_MIN_PIN) != X_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_Y_MIN
      tx_u8(CYDLCD_ES_Y_MIN);
      tx_u8(READ(Y_MIN_PIN) != Y_MIN_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_X_MAX
      tx_u8(CYDLCD_ES_X_MAX);
      tx_u8(READ(X_MAX_PIN) != X_MAX_ENDSTOP_INVERTING ? 1 : 0);
      n++;
    #endif
    #if HAS_Y_MAX
      tx_u8(CYDLCD_ES_Y_MAX);
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

  void push_setting(const cydlcd_setting_t id, const int32_t v) {
    if (settings_step++ != settings_target) return;   // chua toi luot -> chi dem buoc, khong gui
    tx_begin(CYDLCD_F_SETTING);
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
    push_setting(CYDLCD_SET_Z_OFFSET, scaled(getZOffset_mm(), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_FEEDRATE_PCT, scaled(getFeedrate_percent(), CYDLCD_PCT_SCALE));
    #if HAS_EXTRUDERS
      push_setting(CYDLCD_SET_FLOW_PCT, scaled((float)getFlow_percent(E0), CYDLCD_PCT_SCALE));
      push_setting(CYDLCD_SET_STEPS_MM_E, scaled(getAxisSteps_per_mm(E0), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_MAXFEED_E, scaled(getAxisMaxFeedrate_mm_s(E0), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_MAXACCEL_E, scaled(getAxisMaxAcceleration_mm_s2(E0), CYDLCD_VAL_SCALE));
      #if ENABLED(CLASSIC_JERK)
        push_setting(CYDLCD_SET_JERK_E, scaled(getAxisMaxJerk_mm_s(E0), CYDLCD_VAL_SCALE));
      #endif
      #if ENABLED(LIN_ADVANCE)
        push_setting(CYDLCD_SET_LINEAR_ADV_E, scaled(getLinearAdvance_mm_mm_s(E0), CYDLCD_VAL_SCALE));
      #endif
      push_setting(CYDLCD_SET_TARGET_TEMP_E0, scaled(getTargetTemp_celsius(E0), CYDLCD_TEMP_SCALE));
      push_setting(CYDLCD_SET_FAN_PCT_E0, scaled(getTargetFan_percent(FAN0), CYDLCD_PCT_SCALE));
      #if ENABLED(PIDTEMP)
        push_setting(CYDLCD_SET_PID_E_KP, scaled(getPID_Kp(E0), CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_PID_E_KI, scaled(getPID_Ki(E0), CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_PID_E_KD, scaled(getPID_Kd(E0), CYDLCD_VAL_SCALE));
      #endif
    #endif
    push_setting(CYDLCD_SET_STEPS_MM_X, scaled(getAxisSteps_per_mm(X), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_STEPS_MM_Y, scaled(getAxisSteps_per_mm(Y), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_STEPS_MM_Z, scaled(getAxisSteps_per_mm(Z), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXFEED_X, scaled(getAxisMaxFeedrate_mm_s(X), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXFEED_Y, scaled(getAxisMaxFeedrate_mm_s(Y), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXFEED_Z, scaled(getAxisMaxFeedrate_mm_s(Z), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXACCEL_X, scaled(getAxisMaxAcceleration_mm_s2(X), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXACCEL_Y, scaled(getAxisMaxAcceleration_mm_s2(Y), CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXACCEL_Z, scaled(getAxisMaxAcceleration_mm_s2(Z), CYDLCD_VAL_SCALE));
    #if ENABLED(CLASSIC_JERK)
      push_setting(CYDLCD_SET_JERK_X, scaled(getAxisMaxJerk_mm_s(X), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_JERK_Y, scaled(getAxisMaxJerk_mm_s(Y), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_JERK_Z, scaled(getAxisMaxJerk_mm_s(Z), CYDLCD_VAL_SCALE));
    #elif HAS_JUNCTION_DEVIATION
      push_setting(CYDLCD_SET_JUNCTION_DEV, scaled(getJunctionDeviation_mm(), CYDLCD_VAL_SCALE));
    #endif
    #if HAS_HEATED_BED
      push_setting(CYDLCD_SET_TARGET_TEMP_BED, scaled(getTargetTemp_celsius(BED), CYDLCD_TEMP_SCALE));
      #if ENABLED(PIDTEMPBED)
        push_setting(CYDLCD_SET_PID_BED_KP, scaled(getBedPID_Kp(), CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_PID_BED_KI, scaled(getBedPID_Ki(), CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_PID_BED_KD, scaled(getBedPID_Kd(), CYDLCD_VAL_SCALE));
      #endif
    #endif
    #if HAS_BED_PROBE
      push_setting(CYDLCD_SET_PROBE_OFFSET_X, scaled(getProbeOffset_mm(X), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_PROBE_OFFSET_Y, scaled(getProbeOffset_mm(Y), CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_PROBE_OFFSET_Z, scaled(getProbeOffset_mm(Z), CYDLCD_VAL_SCALE));
    #endif
    // M204 S/R/T: gia toc khi in / khi retract / khi travel (planner.settings)
    push_setting(CYDLCD_SET_MAXACCEL_PRINT, scaled(planner.settings.acceleration, CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXACCEL_RETRACT, scaled(planner.settings.retract_acceleration, CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_MAXACCEL_TRAVEL, scaled(planner.settings.travel_acceleration, CYDLCD_VAL_SCALE));
    #if HAS_TRINAMIC_CONFIG
      // Dong dien RMS (mA) dang dat trong driver - M906. val_mA la cung gia tri menu TMC hien.
      #if AXIS_IS_TMC(X)
        push_setting(CYDLCD_SET_TMC_CURRENT_X, (int32_t)stepperX.val_mA);
      #endif
      #if AXIS_IS_TMC(Y)
        push_setting(CYDLCD_SET_TMC_CURRENT_Y, (int32_t)stepperY.val_mA);
      #endif
      #if AXIS_IS_TMC(Z)
        push_setting(CYDLCD_SET_TMC_CURRENT_Z, (int32_t)stepperZ.val_mA);
      #endif
      #if AXIS_IS_TMC(E0)
        push_setting(CYDLCD_SET_TMC_CURRENT_E, (int32_t)stepperE0.val_mA);
      #endif
    #endif
    #if HAS_FILAMENT_SENSOR
      push_setting(CYDLCD_SET_RUNOUT_EN, runout.enabled ? 1 : 0);
    #endif
    // Chieu cao FADE cua bed leveling (M420 Z<mm>) - de man SETTINGS co dong "Fade height" (2026-10-15:
    // nguoi dung bao "trong setting chua co fade height"). Chi co khi ENABLE_LEVELING_FADE_HEIGHT.
    #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
      push_setting(CYDLCD_SET_FADE_HEIGHT, scaled(planner.z_fade_height, CYDLCD_VAL_SCALE));
    #endif
    // 6 tham so MPC (M306) - man TUNING cua man hinh hien thi giong menu MPC cua Marlin 12864
    // (yeu cau nguoi dung 2026-10-15: "chua co MPC value trong setting 'Nhiet do'...").
    #if ENABLED(MPCTEMP)
      {
        const MPC_t &mpc = thermalManager.temp_hotend[0].constants;
        push_setting(CYDLCD_SET_MPC_P, scaled(mpc.heater_power, CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_MPC_C, scaled(mpc.block_heat_capacity, CYDLCD_VAL_SCALE));
        push_setting(CYDLCD_SET_MPC_R, scaled(mpc.sensor_responsiveness, 10000.0f));
        push_setting(CYDLCD_SET_MPC_A, scaled(mpc.ambient_xfer_coeff_fan0, 10000.0f));
        push_setting(CYDLCD_SET_MPC_F,
                     scaled(mpc.ambient_xfer_coeff_fan0 + mpc.fan255_adjustment, 10000.0f));
        push_setting(CYDLCD_SET_MPC_H, scaled(mpc.filament_heat_capacity_permm, 10000.0f));
      }
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      // M603 L/U: chieu dai NAP/RUT filament khi doi filament (M600) - de bang THONG SO co du nhu M503.
      push_setting(CYDLCD_SET_FC_LOAD,   scaled(fc_settings[0].load_length, CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_FC_UNLOAD, scaled(fc_settings[0].unload_length, CYDLCD_VAL_SCALE));
    #endif
    // M206 home offset X/Y. KHONG gui Z: Voron Tap dung probe Z offset (M851 Z), sua M206 Z se lam sai
    // do cao dau in (xem ghi chu o ui_page_settings.cpp: SET_ROWS khong co dong "Z offset").
    push_setting(CYDLCD_SET_HOME_OFFSET_X, scaled(home_offset.x, CYDLCD_VAL_SCALE));
    push_setting(CYDLCD_SET_HOME_OFFSET_Y, scaled(home_offset.y, CYDLCD_VAL_SCALE));
    #if DISABLED(NO_VOLUMETRICS)
      // M200 D (duong kinh filament) + M200 S (bu bang the tich BAT/TAT).
      // !!! PHAI gui CA HAI khi sua D: trong Marlin, `M200 D<v>` mot minh se TU BAT bu bang the tich
      // (M200-M205.cpp: "Dn = enable for compatibility") -> luong ep thay doi => hong ban in. Man hinh
      // doc trang thai S tu day roi gui kem (`M200 D<v> S<s>` - xem ui_page_settings.cpp).
      push_setting(CYDLCD_SET_FILAMENT_DIA, scaled(planner.filament_size[0], CYDLCD_VAL_SCALE));
      push_setting(CYDLCD_SET_FILAMENT_VOL, parser.volumetric_enabled ? 1 : 0);
    #endif
    // GHI CHU (2026-10-14): KHONG gui preheat presets tu day. Ban nay khong co menu MarlinUI 12864
    // nen `HAS_PREHEAT = 0` => `M145` khong duoc bien dich va bang preheat khong nam trong
    // `settings_t`. Bat lai thi `settings_t` doi kich thuoc -> version/CRC EEPROM doi -> Marlin nap
    // MAC DINH (mat M851/PID/M92/M201/M203/M205/M906 + MESH UBL). Vi vay 3 preset PLA/PETG/ABS do
    // CHINH MAN HINH CYD luu trong NVS (namespace "ph") va sua o trang SETTINGS - xem ui_lvgl.cpp.
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
  // thi vai hang bi bo => man hinh CYD ve thieu o luoi. Nay gui 1 hang moi lan onIdle.
  #if BOTH(HAS_LEVELING, HAS_MESH)
    bool    mesh_listing = false;
    uint8_t mesh_row = 0;

    static void mesh_pump() {
      if (!mesh_listing) return;
      bed_mesh_t &m = getMeshArray();                  // mang luoi song, doc tai thoi diem gui
      const uint8_t r = mesh_row++;
      tx_begin(CYDLCD_F_MESH);
      tx_u8(GRID_MAX_POINTS_Y);
      tx_u8(r);
      tx_u8(GRID_MAX_POINTS_X);
      for (uint8_t c = 0; c < GRID_MAX_POINTS_X; c++)
        tx_i16((int16_t)scaled(m[c][r], CYDLCD_VAL_SCALE));
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

  void push_event(const cydlcd_event_t ev, const uint16_t v1 = 0, const uint16_t v2 = 0, const char *text = nullptr) {
    tx_begin(CYDLCD_F_EVENT);
    tx_u8((uint8_t)ev);
    tx_u16(v1);
    tx_u16(v2);
    tx_str(text);
    tx_send();
  }

  void push_message(const cydlcd_severity_t sev, const char *text) {
    tx_begin(CYDLCD_F_MESSAGE);
    tx_u8((uint8_t)sev);
    tx_str(text);
    tx_send();
  }

  void push_prompt(const cydlcd_prompt_t type, const char *text) {
    tx_begin(CYDLCD_F_PROMPT);
    tx_u8(0);                       // prompt_id (v1: luon 0)
    tx_u8((uint8_t)type);
    tx_str(text);
    tx_send();
  }

  // -------------------------------------------------- danh sach file ----
  #if ENABLED(SDSUPPORT)
    FileList cydlcd_files;

    // ---- Liet ke file: 1 frame/lan, KHONG burst ----
    // Moi file 1 frame (~60 byte voi CYDLCD_STR_MAX=48) + 1 frame ket thuc. Truoc day ca danh
    // sach duoc day trong 1 vong lap: TX buffer 64 byte (~2.6 ms moi ra het) khong theo kip nen
    // phan lon file bi bo va frame KET THUC (flags 0xFF) co the mat -> man hinh CYD khong bao gio biet
    // danh sach da xong. Nay push_files() chi BAT DAU; files_pump() gui 1 file moi lan onIdle va
    // luon gui duoc frame ket thuc.
    bool     files_listing = false;
    uint16_t files_index = 0, files_total = 0;

    void push_files() {                               // (ten cu) BAT DAU liet ke thu muc hien tai
      cydlcd_files.refresh();
      files_total = cydlcd_files.count();
      files_index = 0;
      files_listing = true;                           // yeu cau moi khi dang liet ke -> bat dau lai
    }

    // Gui 1 frame file; het danh sach thi gui frame ket thuc. Goi tu tx_pump (onIdle).
    static void files_pump() {
      if (!files_listing) return;
      while (files_index < files_total) {
        const uint16_t i = files_index++;
        if (!cydlcd_files.seek(i)) continue;           // file loi/thu muc vua doi -> bo qua
        tx_begin(CYDLCD_F_FILES);
        tx_u8(cydlcd_files.isDir() ? 0x01 : 0x00);
        tx_i32(0);                                    // size: ExtUI khong cung cap -> 0
        // TEN FILE: uu tien ten DAI (LFN), nhung PHAI co duong lui ve ten 8.3 khi entry KHONG co
        // LFN - neu khong man hinh CYD se hien 1 dong TEN RONG (gap that tren the nay: mot thu muc
        // chi co ten 8.3 -> longFilename() tra "" => danh sach co 1 muc trong, trong nhu loi UI).
        {
          const char *nm = cydlcd_files.longFilename();
          if (!nm || !nm[0]) nm = cydlcd_files.shortFilename();
          tx_str(nm ? nm : "");
          // [proto >= 4] THEM TEN 8.3 ngay sau ten dai. VI SAO: SdFat chi giu LFN trong
          // LONG_FILENAME_LENGTH = 27 byte (SdFatConfig.h: FILENAME_LENGTH 13 * MAX_VFAT_ENTRIES 2 + 1)
          // nen ten > 26 ky tu bi CAT (mat ca phan mo rong) VA `M23 <ten da cat>` KHONG BAO GIO khop
          // duoc voi ten that tren the -> bam PRINT khong in duoc (gap that 2026-10-15 voi 2 file
          // "...bridge_3hole.gcode" (39 ky tu) va "...no-text.gcode" (36 ky tu)). Ten 8.3 (<= 12 ky tu)
          // thi LUON khop -> man hinh gui `M23 <sfn>`. Panel doc kieu "neu con byte thi doc" nen
          // Marlin ban cu (proto 3) van chay binh thuong.
          tx_str(cydlcd_files.shortFilename());
        }
        tx_send();
        return;
      }
      tx_begin(CYDLCD_F_FILES);                        // ket thuc danh sach (PHAI toi duoc cydlcd)
      tx_u8(0xFF);
      tx_i32(0);
      tx_str("");
      tx_str("");                                      // [proto >= 4] ten 8.3 (rong o frame ket thuc)
      tx_send();
      files_listing = false;
    }
  #endif

  // ------------------------------------------------- nap hang doi ----
  // Moi lan onIdle chi nap TOI DA 1 frame tu cac "nguon liet ke" (danh sach file / luoi mesh /
  // bang setting), va luon GIU TRONG 2 slot cho cac frame su kien / message / TRAM (di thang qua
  // tx_send). Nho vay du lieu hang loat khong bao gio chiem het hang doi lam mat su kien.
  static void tx_pump() {
    if (txq_count + 2 > CYDLCD_TXQ_SLOTS) return;      // giu 2 slot trong cho su kien/TRAM
    #if ENABLED(SDSUPPORT)
      if (files_listing) { files_pump(); return; }    // nguoi dung dang doi danh sach file
    #endif
    #if BOTH(HAS_LEVELING, HAS_MESH)
      if (mesh_listing) { mesh_pump(); return; }
    #endif
    settings_pump();
  }

  // ------------------------------------------------------- RX / lenh ----
  cydlcd_rx_t rx;
  millis_t last_rx_ms = 0;
  bool     cydlcd_alive = false;

  #if ENABLED(CYDLCD_SD_OVER_UART)
    // Nhan 1 manh cua sector (goi tu handle_frame khi dang cho du lieu)
    void sd_take_chunk() {
      if (rx.len < 7) return;
      const uint8_t st   = rx.payload[0];
      const uint8_t idx  = rx.payload[5];
      const uint8_t n    = rx.payload[6];
      if (st != CYDLCD_SD_ST_OK) {                    // ERR / NOCARD
        s_sd_err = st ? st : (uint8_t)CYDLCD_SD_ST_ERR;
        if (st == CYDLCD_SD_ST_NOCARD) s_sd_present = false;
        return;
      }
      if (!s_sd_dst || idx >= CYDLCD_SD_CHUNKS || n > CYDLCD_SD_CHUNK) return;
      if ((uint16_t)rx.len < (uint16_t)(7 + n)) return;   // frame thieu du lieu
      memcpy(s_sd_dst + (uint16_t)idx * CYDLCD_SD_CHUNK, &rx.payload[7], n);
      s_sd_have |= (uint8_t)(1u << idx);
      if (s_sd_have == (uint8_t)((1u << CYDLCD_SD_CHUNKS) - 1u)) { s_sd_done = true; s_sd_present = true; }
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
  #endif  // CYDLCD_SD_OVER_UART

  void do_request() {
    const uint8_t what = rx.len ? rx.payload[0] : (uint8_t)CYDLCD_REQ_FULL;
    switch (what) {
      case CYDLCD_REQ_FULL:
        push_hello(); push_status(); push_temps(); push_position(); push_fans(); push_settings();
        break;
      case CYDLCD_REQ_SETTINGS:
        push_settings();
        break;
      #if BOTH(HAS_LEVELING, HAS_MESH)
        case CYDLCD_REQ_MESH:
          push_mesh();
          break;
      #endif
      #if ENABLED(SDSUPPORT)
        case CYDLCD_REQ_FILES:
          push_files();
          break;
        case CYDLCD_REQ_FILES_UP: {
          cydlcd_files.upDir();
          push_files();
          break;
        }
        case CYDLCD_REQ_FILES_DIR: {
          static char name[CYDLCD_STR_MAX + 1];
          const uint8_t n = (rx.len >= 2) ? MIN((uint8_t)(rx.len - 1), (uint8_t)CYDLCD_STR_MAX) : 0;
          for (uint8_t i = 0; i < n; i++) name[i] = (char)rx.payload[1 + i];
          name[n] = '\0';
          if (n) cydlcd_files.changeDir(name);
          push_files();
          break;
        }
      #endif
      case CYDLCD_REQ_SAVE_SETTINGS:
        queue.enqueue_one_now("M500");
        push_message(CYDLCD_SEV_INFO, "Dang luu EEPROM (M500)");
        break;
      case CYDLCD_REQ_ABORT:
        queue.enqueue_one_now("M524");       // huy in tu SD
        queue.enqueue_one_now("M410");       // dung khan cap
        break;
      case CYDLCD_REQ_ENDSTOPS:
        push_endstops();
        break;
      #if ENABLED(STM32_DFU_REBOOT)
        case CYDLCD_REQ_DFU:
          // Nhay vao ROM DFU bootloader de nap lai firmware qua USB (khong can BOOT0+RESET).
          // Giong Marlin/src/lcd/menu/menu_advanced.cpp: CHI khi may dang ranh.
          if (printer_busy())
            push_message(CYDLCD_SEV_WARN, "May dang ban - khong vao DFU");
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
    cydlcd_alive = true;
    #if ENABLED(CYDLCD_SD_OVER_UART)
      // Dang cho du lieu sector: tra loi SD xu ly NGAY, cac frame mang lenh thi HOAN lai.
      if (rx.type == CYDLCD_F_SDDATA) { sd_take_chunk(); return; }
      if (rx.type == CYDLCD_F_SDACK) {                 // tra loi lenh GHI
        if (rx.len >= 5) {
          s_wr_status = rx.payload[0];
          s_wr_sector = cydlcd_u32(&rx.payload[1]);
        }
        return;
      }
      if (s_sd_busy && is_cmd_frame(rx.type)) { sd_defer(); return; }
    #endif
    switch ((cydlcd_frame_t)rx.type) {
      case CYDLCD_F_PING: {
        tx_begin(CYDLCD_F_PONG);
        tx_u8(rx.len ? rx.payload[0] : 0);
        tx_send();
        break;
      }
      case CYDLCD_F_GCODE: {
        // 1 dong G-code -> hang doi cua Marlin (khong block vong lap UI)
        static char cmd[CYDLCD_MAX_PAYLOAD + 2];
        // Payload kieu "str" = u8 len + n byte  => PHAI BO byte len dau tien.
        // BUG CU (2026-10-13): lay ca byte len -> lenh thanh "\x03G28" (1 ky tu dieu khien o dau,
        // khong hien trong log) => parser KHONG thuc thi lenh. Day la ly do G28/G35/G1 tu man hinh CYD
        // khong chay du frame van toi, trong khi lenh gui qua USB (khong co byte len) chay binh thuong.
        uint8_t n = 0;
        for (uint8_t i = 1; i < rx.len && n < CYDLCD_MAX_PAYLOAD; i++) {
          const char c = (char)rx.payload[i];
          cmd[n++] = (c == '\r' || c == '\n') ? ' ' : c;
        }
        cmd[n] = '\0';
        // ---------------------------------------------------------------------------
        // LENH TU MAN HINH: CRITICAL chay NGAY, thuong thi vao hang doi CO TRAN (2026-10-15).
        //
        // CHO TU CHOI THAT SU (khong phai dong "busy: processing" - dong do chi la thong bao dinh
        // ky cua GcodeSuite::host_keepalive(), gcode.cpp:1212):
        //     `queue.enqueue_one_now(cmd)` == `while (!enqueue_one(cmd)) idle();` (queue.cpp)
        //   -> khi hang doi DAY va KHONG BAO GIO trong (vd ban in dang cho `M190 S70` ma ban khong
        //      nong duoc vi PSU tat) thi Marlin BLOCK VINH VIEN ngay trong ham nhan frame:
        //      khong con gui frame trang thai (man hinh mat link, hien trang thai CU) va bam STOP
        //      cung khong huy duoc ban in. Gap that 2026-10-15, phai reset board.
        //
        // CACH XU LY (theo yeu cau: "lenh critical thi chay luon"):
        //   - CRITICAL (M108 thoat vong cho nong, M524 huy ban in) -> chay NGAY, khong qua hang doi;
        //   - lenh thuong -> doi cho hang doi nhung CO TRAN (2 s), het tran thi BO + dem lai
        //     (s_cmd_drop) + bao ra COM4, KHONG BAO GIO block vo han.
        // ---------------------------------------------------------------------------
        if (n) {
          // M108: thoat vong cho nong (M109/M190/M303) - giong GcodeSuite::M108()/e_parser.h:215.
          if (strcasecmp(cmd, "M108") == 0) {
            wait_for_heatup = false;
            TERN_(HAS_RESUME_CONTINUE, wait_for_user = false);
            SERIAL_ECHOLNPGM("PANEL: M108 (thoat vong cho nong) - xu ly NGAY");
          }
          // M524: huy ban in - cung hanh vi nhu GcodeSuite::M524() cua ban nay (gcode/sd/M524.cpp)
          // NHUNG lam NGAY tai day, khong doi `loop()`:
          //   1) `wait_for_heatup = false` -> thoat vong cho nong (neu khong thi `loop()` khong chay
          //      nen co abort khong bao gio duoc xu ly);
          //   2) co abort -> IS_SD_PRINTING() = false -> NGUNG FEED noi dung file (coi nhu doc xong);
          //   3) xoa hang doi -> cac lenh da nap tu file khong chay tiep;
          //   4) TAT GIA NHIET NGAY (2026-10-15, nguoi dung bao: "huy in xong van thay hotend dang
          //      nong"): `abortSDPrinting()` cua MarlinCore (MarlinCore.cpp:363) co
          //      `thermalManager.disable_all_heaters()` + `quickstop_stepper()` nhung CHI chay o vong
          //      `loop()` (MarlinCore.cpp:1680) -> bi TRE neu Marlin dang o trong 1 lenh DAI; trong luc
          //      tre do NHIET DO MUC TIEU (vd 250 C tu file) VAN CON -> hotend van nung.
          //      `disable_all_heaters()` KHONG chan (chi ghi lai target = 0) nen goi o day an toan.
          //
          // !! KHONG goi `quickstop_stepper()` o day !!
          //   Ham do goi `planner.synchronize()` = CHO den khi het chuyen dong dang chay -> trong ham
          //   nhan frame (chay trong `idle()`) se KHOA Marlin lai giua duong (khong doc UART, khong xu
          //   ly lenh) va co the bi goi LAP (nut STOP gui lai moi 400 ms) -> rui ro treo/restart Marlin.
          //   Viec dung chuyen dong de `abortSDPrinting()` lam o vong `loop()` (dung cho, ngay sau do
          //   vi co abort da duoc dat).
          else if (strcasecmp(cmd, "M524") == 0) {
            wait_for_heatup = false;
            TERN_(HAS_RESUME_CONTINUE, wait_for_user = false);
            card.flag.abort_sd_printing = true;
            queue.clear();
            IF_DISABLED(SD_ABORT_NO_COOLDOWN, thermalManager.disable_all_heaters());
            SERIAL_ECHOLNPGM("PANEL: M524 (huy ban in) - xu ly NGAY: ngung feed, xoa hang doi, TAT gia nhiet");
          }
          // M410: quickstop (dung moi chuyen dong dang xep) - cung la lenh "critical" cho an toan.
          else if (strcasecmp(cmd, "M410") == 0) {
            queue.clear();
            quickstop_stepper();
            SERIAL_ECHOLNPGM("PANEL: M410 (quickstop) - xu ly NGAY");
          }
          else {
            const millis_t t0 = millis();
            while (!queue.enqueue_one(cmd)) {
              if (ELAPSED(millis(), t0 + 2000)) {     // TRAN 2 s: khong bao gio block vo han
                s_cmd_drop++;
                SERIAL_ECHOLNPGM("PANEL: hang doi day qua 2s -> BO lenh [", cmd, "]");
                break;
              }
              idle();
            }
          }
        }
        break;
      }
      case CYDLCD_F_REQUEST:
        do_request();
        break;
      case CYDLCD_F_ANSWER:
        if (rx.len >= 2 && rx.payload[1]) setUserConfirmed();
        break;
      #if ENABLED(ASSISTED_TRAMMING_WIZARD)
        case CYDLCD_F_TBT_CMD:
          // 1 lenh wizard ('P','D','M','R','B','Q'). Lenh hop le tu dat co "gui lai ngay"
          // (tbt_report_force_take() o onIdle); 'Q' luon duoc xu ly ke ca khi busy.
          if (rx.len >= 1) tbt_report_cmd((char)rx.payload[0]);
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
    CYDLCD_SERIAL.begin(CYDLCD_BAUD);
    cydlcd_rx_init(&rx);
    push_hello();
    push_event(CYDLCD_EV_BOOT);
  }

  void onIdle() {
    #if ENABLED(CYDLCD_SD_OVER_UART)
      // Cac frame mang lenh bi hoan lai trong luc cho sector SD -> xu ly tai day (dung ngu canh
      // vong lap chinh, khong phai trong disk_read).
      if (s_defer_n) sd_flush_deferred();
    #endif

    // Day hang doi TX ra UART TRUOC TIEN (khong cho doi - xem tx_drain): vua giai phong cho
    // trong hang doi, vua tra cac frame dang ton dong ra duong truyen.
    tx_drain();

    while (CYDLCD_SERIAL.available())
      if (cydlcd_rx_byte(&rx, (uint8_t)CYDLCD_SERIAL.read())) handle_frame();

    const millis_t now = millis();
    if (cydlcd_alive && ELAPSED(now, last_rx_ms + CYDLCD_TIMEOUT_MS)) cydlcd_alive = false;

    static millis_t next_push_ms = 0, next_progress_ms = 0;
    if (!next_push_ms || ELAPSED(now, next_push_ms)) {
      next_push_ms = now + CYDLCD_PUSH_MS;
      push_status();
      push_temps();
      push_position();
      if (isPrinting()) push_fans();
    }
    if (isPrinting() && (!next_progress_ms || ELAPSED(now, next_progress_ms))) {
      next_progress_ms = now + CYDLCD_PROGRESS_MS;
      push_progress();
    }
    #if ENABLED(ASSISTED_TRAMMING_WIZARD)
      // TRAM: day 1 buoc may trang thai. ExtUI KHONG bien dich menu 12864 (menu_tramming.cpp)
      // nen day la nguon DUY NHAT tien buoc wizard - thieu dong nay thi wizard dung o TR_HOMING.
      // tick() cu cua cydlcd_tbt_link lam dung viec nay; ui_attached giu nguyen y nghia cu.
      if (tbt_run::active && !tbt_run::ui_attached) tbt_run::step();

      // Gui trang thai TRAM ~5Hz, va NGAY khi co lenh hop le tu man hinh CYD ('Q' hoi trang thai).
      const bool tbt_due = !next_tbt_ms || ELAPSED(now, next_tbt_ms);
      const bool tbt_now = tbt_report_force_take();
      if (tbt_due) next_tbt_ms = now + TBT_REPORT_PUSH_MS;
      if (tbt_due || tbt_now) push_tram();
    #endif
    if (mesh_dirty && !flag_leveling) {           // mesh vua doi xong -> day 1 lan
      mesh_dirty = false;
      push_mesh();
    }

    // Cuoi cung moi nap them 1 frame liet ke (setting/file/mesh) neu hang doi con cho: cac
    // frame su kien o tren luon duoc di truoc, con du lieu hang loat thi chay dan theo UART.
    tx_pump();
  }

  #if ENABLED(CYDLCD_SD_OVER_UART)
    // ------------------------------------------------------------------------
    // SD qua UART: Marlin hoi man hinh CYD tung sector 512 B (4 manh 128 B).
    // Xem cydlcd_serial.h + cydlcd/README.md muc 12.
    // ------------------------------------------------------------------------
    bool cydlcd_serial_sd_read(uint32_t sector, uint8_t *dst, uint32_t timeout_ms) {
      if (!dst) return false;

      // 1) Gui yeu cau (di THANG ra UART, khong qua hang doi -> khong bi tre sau cac frame PUSH)
      cydlcd_tx_begin(&tx, CYDLCD_F_SDREQ);
      cydlcd_tx_u8(&tx, (uint8_t)CYDLCD_SD_OP_READ);
      cydlcd_tx_u32(&tx, sector);
      const uint8_t total = cydlcd_tx_finish(&tx);

      s_sd_dst = dst; s_sd_have = 0; s_sd_err = 0; s_sd_done = false;
      const bool busy_prev = s_sd_busy;
      s_sd_busy = true;
      tx_write_now(tx.buf, total);

      // 2) Cho du 4 manh (bom RX + day TX + nuoi watchdog)
      const millis_t t0 = millis();
      while (!s_sd_done && !s_sd_err) {
        if (ELAPSED(millis(), t0 + timeout_ms)) break;
        while (CYDLCD_SERIAL.available())
          if (cydlcd_rx_byte(&rx, (uint8_t)CYDLCD_SERIAL.read())) handle_frame();
        tx_drain();
        if (!s_sd_done && !s_sd_err) hal.watchdog_refresh();
      }

      s_sd_busy = busy_prev;
      s_sd_dst = nullptr;
      if (!s_sd_done || s_sd_err) {
        // PHAN BIET 3 TRUONG HOP (sua 2026-10-15 tu cau hoi "trong luc upload ma Marlin gui M21/M22 thi co
        // bi block khong?"):
        //   NOCARD  = man hinh bao KHONG co the      -> mat the THAT SU (rut the) => s_sd_present = false
        //   ERR     = co the nhung loi doc/man hinh dang ban ghi the => the VAN CON => giu true
        //   TIMEOUT = khong co tra loi (man hinh bi `server.handleClient()` chan suot phien upload WiFi
        //             ~20-30 s) => KHONG duoc ket luan la mat the!
        // Truoc day dong nay dat `s_sd_present = (s_sd_err == CYDLCD_SD_ST_ERR)` nen moi lan timeout lai
        // bao "khong co the" -> Marlin tuong vua bi rut the trong luc upload.
        if (s_sd_err == CYDLCD_SD_ST_NOCARD) s_sd_present = false;
        else if (s_sd_err == CYDLCD_SD_ST_ERR) s_sd_present = true;
        return false;                                    // (timeout: giu nguyen trang thai cu)
      }
      return true;
    }

    // Ghi sector: GIAI DOAN 1 (P1) CHUA ho tro - Marlin chay voi SDCARD_READONLY (xem README muc 12).
    bool cydlcd_serial_sd_write(uint32_t sector, const uint8_t *src, uint32_t timeout_ms) {
      if (!src) return false;
      // 1) Yeu cau GHI (5 byte, di thang ra UART)
      cydlcd_tx_begin(&tx, CYDLCD_F_SDREQ);
      cydlcd_tx_u8(&tx, (uint8_t)CYDLCD_SD_OP_WRITE);
      cydlcd_tx_u32(&tx, sector);
      uint8_t total = cydlcd_tx_finish(&tx);
      const bool busy_prev = s_sd_busy;
      s_sd_busy = true;
      s_wr_status = 0xFF;
      tx_write_now(tx.buf, total);

      // 2) Gui CYDLCD_SD_CHUNKS frame du lieu (moi frame 128 B)
      for (uint8_t idx = 0; idx < CYDLCD_SD_CHUNKS; idx++) {
        cydlcd_tx_begin(&tx, CYDLCD_F_SDDATA);
        cydlcd_tx_u8(&tx, (uint8_t)CYDLCD_SD_ST_OK);
        cydlcd_tx_u32(&tx, sector);
        cydlcd_tx_u8(&tx, idx);
        cydlcd_tx_u8(&tx, CYDLCD_SD_CHUNK);
        cydlcd_tx_raw(&tx, src + (uint16_t)idx * CYDLCD_SD_CHUNK, CYDLCD_SD_CHUNK);
        total = cydlcd_tx_finish(&tx);
        tx_write_now(tx.buf, total);
      }

      // 3) Cho CYDLCD_F_SDACK
      const millis_t t0 = millis();
      while (s_wr_status == 0xFF) {
        if (ELAPSED(millis(), t0 + timeout_ms)) break;
        while (CYDLCD_SERIAL.available())
          if (cydlcd_rx_byte(&rx, (uint8_t)CYDLCD_SERIAL.read())) handle_frame();
        tx_drain();
        if (s_wr_status == 0xFF) hal.watchdog_refresh();
      }
      s_sd_busy = busy_prev;
      const bool ok = (s_wr_status == CYDLCD_SD_ST_OK) && (s_wr_sector == sector);
      if (!ok) {
        SERIAL_ECHOLNPGM("PANEL SD: ghi sector ", sector, " THAT BAI (status=", s_wr_status, ")");
        if (s_wr_status == CYDLCD_SD_ST_NOCARD) s_sd_present = false;
      }
      return ok;
    }

    bool cydlcd_serial_sd_init() {
      // Hoi man hinh CYD bang 1 lan doc sector 0 (boot sector / BPB). Tra loi duoc = co the + UART song.
      // LUU Y (2026-10-15): khi man hinh DANG ban ghi file upload thi lan doc nay se TIMEOUT (vong lap ben
      // do bi chan) -> KHONG duoc coi la "mat the" (xem cydlcd_serial_sd_read): chi dat true khi doc duoc,
      // con mat the that su thi man hinh da bao NOCARD ro rang.
      static uint8_t tmp[CYDLCD_SD_SECTOR];
      const bool ok = cydlcd_serial_sd_read(0, tmp, CYDLCD_SD_TIMEOUT_MS);
      if (ok) s_sd_present = true;
      return ok;
    }

    bool cydlcd_serial_sd_present() { return s_sd_present; }
  #endif  // CYDLCD_SD_OVER_UART

  void onStatusChanged(const char * const msg) { push_message(CYDLCD_SEV_INFO, msg); }
  void onUserConfirmRequired(const char * const msg) {
    push_prompt(CYDLCD_PROMPT_USER_CONFIRM, msg);
  }

  void onPrinterKilled(FSTR_P const error, FSTR_P const component) {
    flag_killed = true;
    push_message(CYDLCD_SEV_FATAL, FTOP(error));
    push_event(CYDLCD_EV_PRINT_STOP);
    push_status();
    UNUSED(component);
  }

  void onMediaInserted() { push_event(CYDLCD_EV_MEDIA_INSERTED); push_status(); }
  void onMediaRemoved()  { push_event(CYDLCD_EV_MEDIA_REMOVED);  push_status(); }
  void onMediaError()    { push_event(CYDLCD_EV_MEDIA_ERROR);    push_message(CYDLCD_SEV_ERROR, "Loi the SD"); }

  void onPlayTone(const uint16_t frequency, const uint16_t duration) {
    push_event(CYDLCD_EV_TONE, frequency, duration);
  }

  void onPrintTimerStarted() { push_event(CYDLCD_EV_PRINT_START);  push_status(); }
  void onPrintTimerPaused()  { push_event(CYDLCD_EV_PRINT_PAUSE);  push_status(); }
  void onPrintTimerStopped() { push_event(CYDLCD_EV_PRINT_STOP);   push_status(); }
  void onPrintDone()         { push_event(CYDLCD_EV_PRINT_DONE);   push_status(); }

  void onFilamentRunout(const extruder_t extruder) {
    UNUSED(extruder);
    push_event(CYDLCD_EV_FILAMENT_RUNOUT);
    push_message(CYDLCD_SEV_ERROR, "Het filament");
    push_status();
  }

  void onHomingStart() { flag_homing = true;  push_event(CYDLCD_EV_HOMING_START); push_status(); }
  void onHomingDone()  { flag_homing = false; flag_homed = true; flag_steppers = true; push_event(CYDLCD_EV_HOMING_DONE); push_status(); }

  void onSteppersDisabled() { flag_steppers = false; flag_homed = false; push_event(CYDLCD_EV_STEPPERS_OFF); push_status(); }
  void onSteppersEnabled()  { flag_steppers = true;  push_event(CYDLCD_EV_STEPPERS_ON);  push_status(); }

  void onFactoryReset() { push_event(CYDLCD_EV_FACTORY_RESET); push_message(CYDLCD_SEV_WARN, "Da reset cai dat"); }

  void onStoreSettings(char *) { /* cydlcd giu cai dat rieng cua no (flash cua cydlcd) */ }
  void onLoadSettings(const char *) { }
  void onPostprocessSettings() { }
  void onSettingsStored(bool success) {
    push_event(CYDLCD_EV_SETTINGS_STORED);
    push_message(success ? CYDLCD_SEV_INFO : CYDLCD_SEV_ERROR, success ? "Da luu EEPROM" : "Luu EEPROM loi");
  }
  void onSettingsLoaded(bool success) {
    push_event(CYDLCD_EV_SETTINGS_LOADED);
    UNUSED(success);
    push_settings();
  }

  #if BOTH(HAS_LEVELING, HAS_MESH)
    void onLevelingStart() { flag_leveling = true;  push_event(CYDLCD_EV_LEVELING_START); push_status(); }
    void onLevelingDone() {
      flag_leveling = false;
      mesh_dirty = true;
      push_event(CYDLCD_EV_LEVELING_DONE);
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
    void onPowerLossResume() { push_event(CYDLCD_EV_POWERLOSS_RESUME); push_message(CYDLCD_SEV_WARN, "Phuc hoi sau mat dien"); }
  #endif

  #if HAS_PID_HEATING
    void onPidTuning(const result_t rst) {
      push_event(CYDLCD_EV_PID_DONE, (uint16_t)rst);
      push_settings();
    }
  #endif

} // namespace ExtUI

#endif // CYDLCD_SERIAL_UI && EXTENSIBLE_UI
