/**
 * net_wifi.cpp -- QUAN LY WiFi DOC LAP voi Bluetooth (xem net_wifi.h + README 9.20).
 *
 * Tach ra tu panel_bt_upload.cpp vi khi `PANEL_BT_UPLOAD 0` (nguoi dung tat BT de upload qua
 * WiFi/ESP3D) thi phan WiFi truoc day bien mat theo => icon WiFi o header thanh XAM.
 *
 * DAC DIEM:
 *   - Chi MOT noi duy nhat dieu khien WiFi (WiFi.begin/mode/disconnect). Module BT upload
 *     (panel_bt_upload.cpp) goi vao day qua net_wifi_up()/net_wifi_down()/net_wifi_poll().
 *   - Trang thai tra ve la bien DA CACHE (net_wifi_state) -> vong lap UI doc khong chan.
 *   - Cau hinh luu trong NVS (Preferences, namespace PANEL_WIFI_NVS_NS = "wifi"): uu tien NVS,
 *     neu chua co thi dung gia tri MAC DINH trong panel_config.h (PANEL_WIFI_SSID_DEF/PASS_DEF)
 *     -> nguoi dung doi duoc tu trang WiFi settings (net_wifi_connect(..., save=true)).
 *   - Tu ket noi lai khi mat (moi PANEL_WIFI_RETRY_MS) va tu ket noi luc boot neu
 *     PANEL_WIFI_AUTOSTART=1 (mac dinh: 1 khi BT TAT, 0 khi bat BT upload - xem panel_config.h).
 *   - Ho tro mang TAM (net_wifi_join_temp): doi sang AP cua ESP3D de POST roi quay lai mang chinh
 *     (che do PANEL_ESP3D_VIA_AP=1 - xem README 9.21).
 */
#include "panel_config.h"

#if PANEL_WIFI_MGMT

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "net_wifi.h"

#if defined(PANEL_CYD_DEBUG)
  #define WLOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define WLOG(...)   do { } while (0)
#endif

namespace {

enum : uint8_t { WS_OFF = 0, WS_CONNECTING = 1, WS_UP = 2 };

volatile uint8_t s_state = WS_OFF;      // 0 tat / 1 dang ket noi / 2 da ket noi
bool     s_started = false;             // da bat STA (WiFi.mode(WIFI_STA) + begin)
bool     s_want    = false;             // dang MUON ket noi -> bat auto-reconnect

// Cau hinh DANG DUNG (NVS neu co, khong thi gia tri mac dinh trong panel_config.h)
char     s_ssid[PANEL_WIFI_SSID_MAX + 1] = "";
char     s_pass[PANEL_WIFI_PASS_MAX + 1] = "";

// Cau hinh da LUU trong NVS (de UI hien "dang luu: ..." va biet co xoa duoc khong)
char     s_saved_ssid[PANEL_WIFI_SSID_MAX + 1] = "";
char     s_saved_pass[PANEL_WIFI_PASS_MAX + 1] = "";
bool     s_saved_have = false;

// Mang TAM (che do doi mang sang AP ESP3D): luu lai cau hinh chinh de quay ve
char     s_home_ssid[PANEL_WIFI_SSID_MAX + 1] = "";
char     s_home_pass[PANEL_WIFI_PASS_MAX + 1] = "";
bool     s_temp = false;

char     s_ip[16] = "";                 // IP dang co ("" = chua ket noi)
int8_t   s_rssi = 0;                    // dBm (0 = khong biet)
uint32_t s_retry_ms = 0;                // lan thu ket noi lai gan nhat
bool     s_up_logged = false;           // da log "DA ket noi" cho phien ket noi nay
uint32_t s_down_since = 0;              // millis luc BAT DAU thay mat ket noi (0 = dang ket noi)

// Sau khi QUET MANG, WiFi hay bao "mat ket noi" trong vai giay (radio phai roi kenh de quet) roi tu
// ket noi lai. Neu ta "chot ha" bang WiFi.begin() ngay luc do thi:
//   - huy qua trinh auto-reconnect cua core -> WiFi chap chon lien tuc;
//   - goi WiFi.begin() lap lai khi dang co mDNS + web server => de panic (da gap: panel tu reboot).
// Vi vay: (1) khong ep begin trong PANEL_WIFI_SCAN_GRACE_MS sau khi quet;
//         (2) chi ep begin khi da mat ket noi LIEN TUC >= PANEL_WIFI_DOWN_CONFIRM_MS.
#ifndef PANEL_WIFI_SCAN_GRACE_MS
  #define PANEL_WIFI_SCAN_GRACE_MS      20000u
#endif
#ifndef PANEL_WIFI_DOWN_CONFIRM_MS
  #define PANEL_WIFI_DOWN_CONFIRM_MS     3000u
#endif

// Thoi gian dwell toi da moi kenh khi QUET MANG.
//
// ⚠️⚠️ BAY CUA CORE (doc WiFiScan.cpp:63,144): timeout cua quet ASYNC = `max_ms_per_chan * 20`.
//    Voi 150 ms -> timeout chi 3 s, trong khi DO THAT tren may nay (khu vuc 30 mang) mot lan quet mat
//    **8,2 giay** => scanComplete() luon tra WIFI_SCAN_FAILED (-2) du quet van chay.
//    Chon 600 ms => timeout 12 s (du cho ~8 s) va van la "tran" dwell moi kenh (min = 100 ms).
#ifndef PANEL_WIFI_SCAN_MS_PER_CHAN
  #define PANEL_WIFI_SCAN_MS_PER_CHAN 600
#endif

// ---- ket qua quet mang (RSSI manh nhat truoc) ----
struct ScanEnt { char ssid[PANEL_WIFI_SSID_MAX + 1]; int8_t rssi; };
ScanEnt  s_scan[PANEL_WIFI_SCAN_MAX];
int      s_scan_n  = 0;
uint32_t s_scan_ms = 0;

// ---- NVS (Preferences) ----
bool nvs_load(void) {
  Preferences p;
  if (!p.begin(PANEL_WIFI_NVS_NS, true)) return false;     // read-only
  const String ss = p.getString("ssid", "");
  const String pw = p.getString("pass", "");
  p.end();
  if (ss.length() == 0 || ss.length() > PANEL_WIFI_SSID_MAX) return false;
  snprintf(s_saved_ssid, sizeof(s_saved_ssid), "%s", ss.c_str());
  snprintf(s_saved_pass, sizeof(s_saved_pass), "%s",
           (pw.length() <= PANEL_WIFI_PASS_MAX) ? pw.c_str() : "");
  s_saved_have = true;
  return true;
}

void nvs_save(const char *ssid, const char *pass) {
  Preferences p;
  if (!p.begin(PANEL_WIFI_NVS_NS, false)) {
    WLOG("[WIFI] KHONG mo duoc NVS namespace \"%s\" -> khong luu duoc\n", PANEL_WIFI_NVS_NS);
    return;
  }
  p.putString("ssid", ssid);
  p.putString("pass", pass ? pass : "");
  p.end();
  snprintf(s_saved_ssid, sizeof(s_saved_ssid), "%s", ssid);
  snprintf(s_saved_pass, sizeof(s_saved_pass), "%s", pass ? pass : "");
  s_saved_have = true;
  WLOG("[WIFI] da luu vao NVS: ssid=\"%s\" (mat khau %u ky tu)\n", ssid,
       (unsigned)strlen(pass ? pass : ""));
}

void nvs_clear(void) {
  Preferences p;
  if (p.begin(PANEL_WIFI_NVS_NS, false)) {
    p.clear();
    p.end();
  }
  s_saved_ssid[0] = '\0';
  s_saved_pass[0] = '\0';
  s_saved_have = false;
  WLOG("[WIFI] da XOA cau hinh trong NVS\n");
}

void ip_str(char *dst, const size_t n, const IPAddress &ip) {
  snprintf(dst, n, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// Cap nhat 3 bien cache (state/ip/rssi) tu WiFi.status() - KHONG chan.
void refresh(void) {
  if (!s_started) {
    s_state = WS_OFF;
    s_ip[0] = '\0';
    s_rssi  = 0;
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    s_state = WS_UP;
    ip_str(s_ip, sizeof(s_ip), WiFi.localIP());
    s_rssi = (int8_t)WiFi.RSSI();
    if (!s_up_logged) {
      s_up_logged = true;
      WLOG("[WIFI] DA ket noi \"%s\": IP %s, RSSI %d dBm, heap=%u B\n", s_ssid, s_ip,
           (int)s_rssi, (unsigned)ESP.getFreeHeap());
    }
  } else {
    s_state = WS_CONNECTING;                 // dang thu / dang thu lai -> icon DO
    s_ip[0] = '\0';
    s_rssi  = 0;
  }
}

// Bat STA va begin() voi cau hinh dang dung. KHONG chan. false = thieu RAM / chua co SSID.
bool start_sta(const bool want) {
  if (s_ssid[0] == '\0') return false;
  if (!s_started) {
    if (!net_wifi_ram_ok()) return false;
    WiFi.persistent(false);            // KHONG de core ghi credential xuong NVS cua no
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);       // driver tu ket noi lai khi mat (chot ha o net_wifi_poll)
    WiFi.setSleep(true);               // tiet kiem dien; net_wifi_ps(false) khi day file
    s_started = true;
  }
  WiFi.begin(s_ssid, s_pass);
  s_want = want;
  s_retry_ms = millis();
  s_up_logged = false;
  s_state = WS_CONNECTING;
  WLOG("[WIFI] bat dau ket noi \"%s\" (luu trong NVS: %s, %u ky tu mat khau), heap=%u B\n",
       s_ssid, s_saved_have ? "co" : "khong", (unsigned)strlen(s_pass),
       (unsigned)ESP.getFreeHeap());
  return true;
}

}  // namespace

// ===========================================================================
// Trang thai (cache)
// ===========================================================================
uint8_t net_wifi_state(void) { return s_state; }

const char *net_wifi_ssid(void) { return s_started ? s_ssid : ""; }

const char *net_wifi_ip(void) { return s_ip; }

int8_t net_wifi_rssi(void) { return s_rssi; }

bool net_wifi_configured(void) { return s_ssid[0] != '\0'; }

const char *net_wifi_saved_ssid(void) { return s_saved_ssid; }

// ===========================================================================
// Quet mang  (CHAN ~2-4 s - xem net_wifi.h)
// ===========================================================================
// Luu ket qua quet (da co san trong WiFi.SSID(i)/RSSI(i)) vao bang sap xep theo RSSI giam dan.
// Dung CHUNG cho ca duong dong bo (console) va duong async (UI).
static void scan_store(const int n) {
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      const String ss = WiFi.SSID(i);
      if (ss.length() == 0) continue;
      // Chen vao danh sach da sap xep theo RSSI GIAM dan (sap xep chen - danh sach rat ngan).
      if (s_scan_n < PANEL_WIFI_SCAN_MAX) {
        int pos = s_scan_n;
        while (pos > 0 && s_scan[pos - 1].rssi < (int8_t)WiFi.RSSI(i)) {
          s_scan[pos] = s_scan[pos - 1];
          pos--;
        }
        snprintf(s_scan[pos].ssid, sizeof(s_scan[pos].ssid), "%s", ss.c_str());
        s_scan[pos].rssi = (int8_t)WiFi.RSSI(i);
        s_scan_n++;
      } else if ((int8_t)WiFi.RSSI(i) > s_scan[PANEL_WIFI_SCAN_MAX - 1].rssi) {
        int pos = PANEL_WIFI_SCAN_MAX - 1;
        while (pos > 0 && s_scan[pos - 1].rssi < (int8_t)WiFi.RSSI(i)) {
          s_scan[pos] = s_scan[pos - 1];
          pos--;
        }
        snprintf(s_scan[pos].ssid, sizeof(s_scan[pos].ssid), "%s", ss.c_str());
        s_scan[pos].rssi = (int8_t)WiFi.RSSI(i);
      }
    }
  }
}

int net_wifi_scan(void) {
  s_scan_n = 0;
  // scanNetworks(false=chan cho xong, false=khong liet ke mang an, false=active, max_ms_per_chan):
  // Arduino core ngu toi khi driver bao xong. PHai goi tu task UI/main loop, KHONG duoc tu ISR.
  //
  // ⚠️ max_ms_per_chan: mac dinh cua core la 300 ms/kenh => 13 kenh = ~4 s (co the HON 5 s khi
  // dong kenh) - trong suot thoi gian do panel KHONG chay duoc gi khac: khong tra loi sector the SD
  // cho Marlin (=> dang in ma quet = hong ban in) va khong phuc vu web server. Dat 150 ms/kenh de
  // rut ngan cua so chan lai. Xem canh bao trong ui_lvgl.cpp: cb_wifi_scan() chan quet khi
  // Marlin dang in.
  const uint32_t t0 = millis();
  WiFi.scanDelete();
  const int n = WiFi.scanNetworks(false, false, false, PANEL_WIFI_SCAN_MS_PER_CHAN);
  const uint32_t dt = millis() - t0;
  scan_store(n);
  WiFi.scanDelete();
  s_scan_ms = millis();
  WLOG("[WIFI] quet xong: thay %d mang, luu %d (toi da %d) trong %lu ms (tran %u ms/kenh)\n",
       n, s_scan_n, PANEL_WIFI_SCAN_MAX, (unsigned long)dt, (unsigned)PANEL_WIFI_SCAN_MS_PER_CHAN);
  return s_scan_n;
}

// ===========================================================================
// QUET KHONG CHAN (UI): bat dau -> nhip 400 ms goi poll() -> xong thi doc ket qua.
// Vong lap chinh VAN CHAY trong luc quet => van tra loi Marlin (sector the SD) + web server.
// ===========================================================================
bool s_scanning = false;
uint32_t s_scan_start_ms = 0;

int net_wifi_scan_begin(void) {
  if (s_scanning) return -1;
  s_scan_n = 0;
  WiFi.scanDelete();
  const int16_t r = WiFi.scanNetworks(true /*async*/, false, false, PANEL_WIFI_SCAN_MS_PER_CHAN);
  if (r == WIFI_SCAN_FAILED) { WLOG("[WIFI] khong bat dau duoc quet (scanNetworks tra FAILED)\n"); return -1; }
  s_scanning = true;
  s_scan_start_ms = millis();
  s_scan_ms = s_scan_start_ms;      // danh dau NGAY tu luc bat dau: khoa cua an toan + grace period
  WLOG("[WIFI] bat dau quet KHONG CHAN (tran %u ms/kenh)\n", (unsigned)PANEL_WIFI_SCAN_MS_PER_CHAN);
  return 0;
}

bool net_wifi_scanning(void) { return s_scanning; }

int net_wifi_scan_poll(void) {
  if (!s_scanning) return -1;
  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    return (millis() - s_scan_start_ms > 30000u) ? -2 : -1;   // cua an toan 30 s
  }
  if (n == WIFI_SCAN_FAILED) { s_scanning = false; WiFi.scanDelete(); WLOG("[WIFI] quet LOI\n"); return -2; }
  scan_store((int)n);
  WiFi.scanDelete();
  s_scanning = false;
  s_scan_ms = millis();
  WLOG("[WIFI] quet KHONG CHAN xong sau %lu ms: thay %d mang, luu %d\n",
       (unsigned long)(millis() - s_scan_start_ms), (int)n, s_scan_n);
  return s_scan_n;
}

void net_wifi_scan_cancel(void) {
  if (!s_scanning) return;
  WiFi.scanDelete();
  s_scanning = false;
  WLOG("[WIFI] HUY phien quet (roi trang WIFI)\n");
}

int net_wifi_scan_max(void) { return PANEL_WIFI_SCAN_MAX; }

const char *net_wifi_scan_ssid(int i) {
  if (i < 0 || i >= s_scan_n) return nullptr;
  return s_scan[i].ssid;
}

int8_t net_wifi_scan_rssi(int i) {
  if (i < 0 || i >= s_scan_n) return 0;
  return s_scan[i].rssi;
}

uint32_t net_wifi_scan_ms(void) { return s_scan_ms; }

// ===========================================================================
// Ket noi / ngat  (khong chan)
// ===========================================================================
bool net_wifi_connect(const char *ssid, const char *pass, bool save) {
  if (!ssid || ssid[0] == '\0') { WLOG("[WIFI] connect: SSID rong -> tu choi\n"); return false; }
  if (strlen(ssid) > PANEL_WIFI_SSID_MAX) {
    WLOG("[WIFI] connect: SSID dai qua %u ky tu -> tu choi\n", (unsigned)PANEL_WIFI_SSID_MAX);
    return false;
  }
  if (pass && strlen(pass) > PANEL_WIFI_PASS_MAX) {
    WLOG("[WIFI] connect: mat khau dai qua %u ky tu -> tu choi\n", (unsigned)PANEL_WIFI_PASS_MAX);
    return false;
  }
  snprintf(s_ssid, sizeof(s_ssid), "%s", ssid);
  snprintf(s_pass, sizeof(s_pass), "%s", pass ? pass : "");
  s_temp = false;
  if (save) nvs_save(s_ssid, s_pass);
  // Doi mang: tat STA cu truoc (mode(WIFI_OFF) tra RAM) roi bat lai voi cau hinh moi.
  if (s_started) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    s_started = false;
  }
  const bool ok = start_sta(true);
  refresh();
  return ok;
}

void net_wifi_disconnect(bool forget) {
  if (forget) {
    nvs_clear();
    s_saved_have = false;
  }
  net_wifi_down();
  s_temp = false;
  WLOG("[WIFI] da ngat ket noi%s\n", forget ? " + XOA cau hinh da luu" : "");
}

// ===========================================================================
// Vong doi (main.cpp)
// ===========================================================================
void net_wifi_begin(void) {
  const bool have_nvs = nvs_load();
  if (have_nvs) {
    snprintf(s_ssid, sizeof(s_ssid), "%s", s_saved_ssid);
    snprintf(s_pass, sizeof(s_pass), "%s", s_saved_pass);
  } else {
    // Chua co gi trong NVS -> dung gia tri MAC DINH trong panel_config.h (chi de goi y;
    // nguoi dung doi duoc tu trang WiFi settings, luc do se ghi vao NVS).
    snprintf(s_ssid, sizeof(s_ssid), "%s", PANEL_WIFI_SSID_DEF);
    snprintf(s_pass, sizeof(s_pass), "%s", PANEL_WIFI_PASS_DEF);
  }
  WLOG("[WIFI] begin: ssid=\"%s\" (nguon: %s), AUTOSTART=%d, heap=%u B\n", s_ssid,
       have_nvs ? "NVS" : "mac dinh trong panel_config.h", (int)PANEL_WIFI_AUTOSTART,
       (unsigned)ESP.getFreeHeap());
#if PANEL_WIFI_AUTOSTART
  if (s_ssid[0]) start_sta(true);
#else
  WLOG("[WIFI] PANEL_WIFI_AUTOSTART=0 -> chua bat WiFi (module BT upload se bat o PHA 2)\n");
#endif
}

void net_wifi_task(void) { net_wifi_poll(); }

// ===========================================================================
// API noi bo cho module BT upload (PHA 2)
// ===========================================================================
bool net_wifi_up(void) {
  if (s_ssid[0] == '\0') return false;
  if (s_started) {
    if (WiFi.status() != WL_CONNECTED) {
      s_want = true;
      WiFi.begin(s_ssid, s_pass);
      s_retry_ms = millis();
    }
    refresh();
    return true;
  }
  const bool ok = start_sta(true);
  refresh();
  return ok;
}

void net_wifi_down(void) {
  s_want = false;
  if (s_started) {
    const uint32_t before = ESP.getFreeHeap();
    WiFi.disconnect(true);          // ngat + tat STA
    WiFi.mode(WIFI_OFF);            // -> espWiFiStop(): esp_wifi_stop + esp_wifi_deinit
    s_started = false;
    s_up_logged = false;
    WLOG("[WIFI] TAT WiFi: heap %u -> %u B (khoi lien tuc=%u B)\n", (unsigned)before,
         (unsigned)ESP.getFreeHeap(),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }
  s_state = WS_OFF;
  s_ip[0] = '\0';
  s_rssi  = 0;
}

void net_wifi_poll(void) {
  if (!s_started) { s_state = WS_OFF; return; }
  refresh();
  const uint32_t now = millis();
  if (s_state == WS_UP) { s_down_since = 0; return; }     // dang ket noi -> khong dung tới gi
  if (!s_want) return;

  // ⚠️ DANG QUET MANG (async): TUYET DOI khong goi WiFi.begin()/mode() luc nay - no se HUY phien
  // quet (do that: scanComplete() tra WIFI_SCAN_FAILED sau ~2 s) va lam WiFi chap chon.
  if (s_scanning) return;

  // Moi mat ket noi -> bat dau dem; chi ep ket noi lai khi da mat LIEN TUC du lau (tranh dut
  // auto-reconnect cua core chi vi 1 nhip doc trang thai bi sai).
  if (!s_down_since) s_down_since = now;
  if ((now - s_down_since) < PANEL_WIFI_DOWN_CONFIRM_MS) return;

  // Vua QUET MANG xong -> de core tu ket noi lai, KHONG ep begin (xem ghi chu o khai bao bien).
  if (s_scan_ms && (now - s_scan_ms) < PANEL_WIFI_SCAN_GRACE_MS) {
    static uint32_t s_last_grace_log = 0;
    if (now - s_last_grace_log > 5000u) {
      s_last_grace_log = now;
      WLOG("[WIFI] mat ket noi ngay sau khi quet -> cho auto-reconnect (con %lu ms), khong ep begin\n",
           (unsigned long)(PANEL_WIFI_SCAN_GRACE_MS - (now - s_scan_ms)));
    }
    return;
  }

  if (now - s_retry_ms < PANEL_WIFI_RETRY_MS) return;
  s_retry_ms = now;
  if (WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);       // autoReconnect lo phan lon; day la chot ha
  WLOG("[WIFI] mat ket noi > %lu ms -> thu lai \"%s\"\n",
       (unsigned long)PANEL_WIFI_DOWN_CONFIRM_MS, s_ssid);
}

bool net_wifi_connected(void) { return s_started && (WiFi.status() == WL_CONNECTED); }

bool net_wifi_ram_ok(void) {
  const uint32_t freeb = ESP.getFreeHeap();
  const size_t   big   = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const bool ok = (freeb >= PANEL_WIFI_MIN_HEAP) && (big >= PANEL_WIFI_MIN_BLOCK);
  WLOG("[WIFI] cua an toan RAM: heap=%u B, khoi lien tuc=%u B (can >= %u / %u) -> %s\n",
       (unsigned)freeb, (unsigned)big, (unsigned)PANEL_WIFI_MIN_HEAP,
       (unsigned)PANEL_WIFI_MIN_BLOCK, ok ? "OK" : "KHONG DU");
  return ok;
}

void net_wifi_ps(bool power_save) {
  if (!s_started) return;
  WiFi.setSleep(power_save);        // day file: tat modem sleep; xong bat lai cho tiet kiem
}

// ===========================================================================
// Mang TAM (che do doi mang sang AP cua ESP3D - PANEL_ESP3D_VIA_AP=1, README 9.21)
// ===========================================================================
bool net_wifi_join_temp(const char *ssid, const char *pass) {
  if (!ssid || ssid[0] == '\0') return false;
  if (!s_temp) {                    // lan dau: luu cau hinh chinh de quay ve
    snprintf(s_home_ssid, sizeof(s_home_ssid), "%s", s_ssid);
    snprintf(s_home_pass, sizeof(s_home_pass), "%s", s_pass);
  }
  const bool ok = net_wifi_connect(ssid, pass, false);   // save=false: KHONG ghi NVS
  s_temp = ok;
  if (ok) WLOG("[WIFI] da chuyen sang mang TAM \"%s\" (se quay ve \"%s\")\n", ssid, s_home_ssid);
  return ok;
}

void net_wifi_leave_temp(void) {
  if (!s_temp) return;
  s_temp = false;
  if (s_home_ssid[0]) {
    WLOG("[WIFI] quay ve mang chinh \"%s\"\n", s_home_ssid);
    net_wifi_connect(s_home_ssid, s_home_pass, false);
  }
}

#else   // ------------------- PANEL_WIFI_MGMT = 0: stub rong (moi env van link duoc) ----------

uint8_t     net_wifi_state(void) { return 0; }
const char *net_wifi_ssid(void) { return ""; }
const char *net_wifi_ip(void) { return ""; }
int8_t      net_wifi_rssi(void) { return 0; }
bool        net_wifi_configured(void) { return false; }
const char *net_wifi_saved_ssid(void) { return ""; }
int         net_wifi_scan(void) { return 0; }
int         net_wifi_scan_max(void) { return 0; }
const char *net_wifi_scan_ssid(int i) { (void)i; return nullptr; }
int8_t      net_wifi_scan_rssi(int i) { (void)i; return 0; }
uint32_t    net_wifi_scan_ms(void) { return 0; }
bool        net_wifi_connect(const char *ssid, const char *pass, bool save) {
  (void)ssid; (void)pass; (void)save; return false;
}
void        net_wifi_disconnect(bool forget) { (void)forget; }
int  net_wifi_scan_begin(void) { return -1; }
int  net_wifi_scan_poll(void) { return -2; }
void net_wifi_scan_cancel(void) {}
bool net_wifi_scanning(void) { return false; }
void        net_wifi_begin(void) {}
void        net_wifi_task(void) {}
bool        net_wifi_up(void) { return false; }
void        net_wifi_down(void) {}
void        net_wifi_poll(void) {}
bool        net_wifi_connected(void) { return false; }
bool        net_wifi_ram_ok(void) { return false; }
void        net_wifi_ps(bool power_save) { (void)power_save; }
bool        net_wifi_join_temp(const char *ssid, const char *pass) {
  (void)ssid; (void)pass; return false;
}
void        net_wifi_leave_temp(void) {}

#endif  // PANEL_WIFI_MGMT
