/**
 * net_upload_http.cpp -- Web server tren man hinh CYD (ESP32 WebServer cua core) - xem net_upload_http.h.
 *
 * Trang HTML nam trong FLASH (PROGMEM) -> khong ton RAM (chi ~1,6 KB flash). Doc file trong 4 KB
 * mot lan khi ghi vao the SD (cyd_sd.cpp) -> khong can buffer lon.
 */
#include "cydlcd_config.h"

// Include header truoc `#if` de nhanh stub cung co linkage C (xem cyd_sd.cpp).
#include "net_upload_http.h"

#if CYDLCD_WIFI_UPLOAD

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>
#include <string.h>
#include <stdio.h>

#if CYDLCD_HTTP_USE_MDNS
  #include <ESPmDNS.h>
  #define HTTP_USE_MDNS 1
#else
  #define HTTP_USE_MDNS 0
#endif

#include "net_upload_http.h"
#include "cyd_sd.h"
#include "net_wifi.h"
#include "cydlcd_link.h"

#if defined(CYDLCD_CYD_DEBUG)
  #define HLOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define HLOG(...)   do { } while (0)
#endif

namespace {

// ---- trang HTML (PROGMEM). Phai chua chuoi "TBT-CYDLCD" (client nhan dang du phong). ----
const char HTML_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="vi"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TBT-CYDLCD - gui file in</title>
<style>
 body{font-family:system-ui,Arial,sans-serif;margin:0;padding:16px;background:#12102a;color:#e6e1e5}
 h1{font-size:20px;margin:0 0 4px;color:#00e5ff}
 .s{color:#9aa;font-size:13px;margin-bottom:14px}
 .card{background:#1e156e;border:1px solid #2a2080;border-radius:8px;padding:14px;margin-bottom:12px}
 input[type=file],input[type=text]{width:100%;box-sizing:border-box;margin:6px 0;padding:8px;
   background:#12102a;border:1px solid #2a2080;border-radius:6px;color:#e6e1e5}
 button{width:100%;padding:12px;font-size:16px;border:0;border-radius:6px;background:#00e5ff;
   color:#12102a;font-weight:700}
 button:disabled{opacity:.5}
 #bar{height:14px;background:#2a2080;border-radius:3px;overflow:hidden;margin-top:10px}
 #bar>i{display:block;height:100%;width:0;background:#00e5ff}
 pre{white-space:pre-wrap;font-size:12px;color:#9aa;margin:8px 0 0}
</style></head><body>
<h1>TBT-CYDLCD</h1>
<div class="s">Gui G-code tu dien thoai/PC -> cydlcd ghi vao the TF -> M22/M21 cho Marlin doc lai.</div>
<div class="card">
  <input type="text" id="tok" placeholder="token (vd TBT-3D-up)">
  <input type="file" id="f">
  <button id="go" onclick="send()">Gui file</button>
  <div id="bar"><i id="bi"></i></div>
  <pre id="log">san sang.</pre>
</div>
<script>
var q=new URLSearchParams(location.search); if(q.get('token')) document.getElementById('tok').value=q.get('token');
function log(t){document.getElementById('log').textContent=t;}
function poll(){fetch('/status').then(r=>r.json()).then(j=>{
  document.getElementById('bi').style.width=(j.total_bytes?Math.round(j.rx_bytes*100/j.total_bytes):0)+'%';
  log('cydlcd: '+j.state+' | nhan '+j.rx_bytes+'/'+j.total_bytes+' B | da day '+j.written_bytes+
      ' B | '+j.file+'\nket qua: '+(j.result||'-')+' '+(j.message||''));
  if(j.state=='writing'||j.state=='receiving') setTimeout(poll,700);
});}
function send(){
  var f=document.getElementById('f').files[0]; if(!f){log('chua chon file');return;}
  var tok=document.getElementById('tok').value;
  var fd=new FormData(); fd.append('name',f.name); fd.append('file',f, f.name);
  var x=new XMLHttpRequest();
  x.open('POST','/upload?token='+encodeURIComponent(tok));
  x.setRequestHeader('X-CYDLCD-Token',tok);
  x.upload.onprogress=function(e){document.getElementById('bi').style.width=
     (e.lengthComputable?Math.round(e.loaded*100/e.total):0)+'%';};
  x.onload=function(){log('tra loi: '+x.responseText); poll();};
  x.onerror=function(){log('loi ket noi toi cydlcd');};
  x.send(fd); log('dang gui '+f.name+' ('+f.size+' B)...');
}
</script></body></html>)HTML";

WebServer server(CYDLCD_HTTP_PORT);
bool s_started = false;
bool s_finish_pending = false;
char s_client[20] = "-";

// ---- trang thai cho /status ----
enum : uint8_t { HS_IDLE = 0, HS_RECV, HS_WRITE, HS_DONE, HS_ERR };
volatile uint8_t s_http_state = HS_IDLE;
uint32_t s_rx = 0;                 // byte da nhan tu client (nhanh)
uint32_t s_total = 0;              // tong byte theo Content-Length cua phan file (0 = khong biet)
char     s_file[40] = "";          // ten file dang nhan / vua nhan
char     s_msg[96] = "";           // thong bao cho /status (message)

const char *state_str(void) {
  switch (s_http_state) {
    case HS_RECV: return "receiving";
    case HS_WRITE: return "writing";
    case HS_DONE: return "done";
    case HS_ERR:  return "error";
    default:      return "idle";
  }
}

// Bo dau " o hai dau gia tri (mot so client cat chuoi theo substring -> co the lan ca dau ")
void unquote(char *dst, const size_t cap, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  size_t i = 0, o = 0;
  if (src[0] == '"') i = 1;
  while (src[i] != '\0' && src[i] != '"' && o + 1u < cap) dst[o++] = src[i++];
  dst[o] = '\0';
}

bool token_ok(void) {
  if (CYDLCD_HTTP_TOKEN[0] == '\0') return true;       // khong cau hinh token -> khong kiem
  const String q = server.arg("token");
  if (q.length() && q == CYDLCD_HTTP_TOKEN) return true;
  // Header token: ten MOI X-CYDLCD-Token; van nhan ten CU X-Panel-Token cho client cu
  // (HTTP header khong phan biet hoa/thuong nen X-cydlcd-Token cung khop).
  String h = server.header("X-CYDLCD-Token");
  if (!h.length()) h = server.header("X-Panel-Token");
  if (h.length() && h == CYDLCD_HTTP_TOKEN) return true;
  return false;
}

void send_json(const int code, const char *body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

// ===========================================================================
// GET /
// ===========================================================================
void handle_root(void) {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", HTML_PAGE);
  HLOG("[HTTP] GET / tu %s\n", server.client().remoteIP().toString().c_str());
}

// ===========================================================================
// GET /status  (PHAI tra duoc ca khi chua co phien: state=idle)
// ===========================================================================
void handle_status(void) {
  char js[512];
  const uint32_t written = cyd_sd_last_written();
  const char *res = cyd_sd_last_result();
  snprintf(js, sizeof(js),
           "{\"cydlcd\":\"TBT\",\"device\":\"TBT-CYDLCD\",\"state\":\"%s\","
           "\"rx_bytes\":%lu,\"total_bytes\":%lu,\"written_bytes\":%lu,"
           "\"file\":\"%s\",\"result\":\"%s\",\"message\":\"%s\","
           "\"sd\":%d,\"wifi\":%u,\"ip\":\"%s\",\"heap\":%lu}",
           state_str(), (unsigned long)s_rx, (unsigned long)s_total, (unsigned long)written,
           s_file, res, s_msg, (int)cyd_sd_ready(), (unsigned)net_wifi_state(),
           net_wifi_ip(), (unsigned long)ESP.getFreeHeap());
  send_json(200, js);
}

// ===========================================================================
// POST /upload  (multipart/form-data)
//   field text  name="name"  = ten file dich (dat TRUOC phan file - theo hop dong)
//   phan file   name="file" filename="<ten>"
// ===========================================================================
void handle_upload_stream(void) {
  HTTPUpload &up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START: {
      s_client[0] = '\0';
      snprintf(s_client, sizeof(s_client), "%s", server.client().remoteIP().toString().c_str());
      // LUU Y (do that 2026-10-14): core ESP32 2.0.17 dat `totalSize = 0` o UPLOAD_FILE_START
      // (Parsing.cpp:428) va chi CONG DON dan trong luc ghi -> o day luon = 0. Kich thuoc THAT chi
      // biet duoc o UPLOAD_FILE_END (Parsing.cpp:471) => s_total duoc gan o nhanh END.
      s_total = 0;
      s_rx = 0;
      // Kiem token NGAY tu dau: khong de client la ghi ca file vao the TF roi moi tu choi (401 o
      // handle_upload_done chi la lop chan thu 2). Query ?token= / header X-CYDLCD-Token DA duoc
      // core phan tich xong truoc khi goi handler nay.
      if (!token_ok()) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "sai ma token");
        s_file[0] = '\0';
        HLOG("[HTTP] TU CHOI /upload tu %s: sai token (khong ghi the TF)\n", s_client);
        return;                                    // moi goi WRITE sau do bi bo qua
      }
      // Ten dich: uu tien field text "name" (gui TRUOC phan file), khong co thi lay ten file
      char nm[40];
      unquote(nm, sizeof(nm), server.arg("name").c_str());
      if (nm[0] == '\0') unquote(nm, sizeof(nm), up.filename.c_str());
      snprintf(s_file, sizeof(s_file), "%s", nm);
      HLOG("[HTTP] nhan file \"%s\" tu %s (kich thuoc chua biet)\n", s_file, s_client);
      s_http_state = HS_RECV;
      snprintf(s_msg, sizeof(s_msg), "dang nhan file");
      if (!cyd_sd_temp_open(nm, s_total)) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "khong mo duoc file tam (the TF?)");
        HLOG("[HTTP] LOI: %s\n", s_msg);
        return;                                    // UPLOAD_FILE_WRITE se tra ve 0 -> core huy
      }
      break;
    }
    case UPLOAD_FILE_WRITE: {
      if (s_http_state != HS_RECV) return;
      const size_t w = cyd_sd_temp_write(up.buf, up.currentSize);
      s_rx += (uint32_t)w;
      if (w != up.currentSize) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "ghi the TF loi (the day / rut ra?)");
        HLOG("[HTTP] LOI: %s\n", s_msg);
      }
      break;
    }
    case UPLOAD_FILE_END: {
      // Kich thuoc THAT cua phan file chi co o day (totalSize = tong da cong don - Parsing.cpp:471)
      s_total = up.totalSize;
      if (s_http_state == HS_RECV) {
        cyd_sd_temp_close();
        snprintf(s_msg, sizeof(s_msg), "da nhan xong, cho ghi vao the SD");
        HLOG("[HTTP] nhan xong %lu B (totalSize=%lu) -> %s\n", (unsigned long)s_rx,
             (unsigned long)s_total, cyd_sd_temp_path());
      }
      break;
    }
    default:                                       // UPLOAD_FILE_ABORT
      cyd_sd_temp_abort();
      s_http_state = HS_ERR;
      snprintf(s_msg, sizeof(s_msg), "client ngat giua luc gui");
      HLOG("[HTTP] %s\n", s_msg);
      break;
  }
}

void handle_upload_done(void) {
  char js[256];
  if (!token_ok()) {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "sai ma token");
    HLOG("[HTTP] TU CHOI /upload tu %s: sai token\n",
         server.client().remoteIP().toString().c_str());
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"sai ma token\"}");
    send_json(401, js);
    cyd_sd_temp_abort();
    return;
  }
#if !CYDLCD_HTTP_ALLOW_WHILE_PRINTING
  if (cyd_sd_printer_busy()) {
    s_http_state = HS_ERR;
    // Thong bao PHAI noi ro LY DO (va chi dung khi THAT SU in tu the SD): nguoi dung tung hieu nham
    // "Marlin dang in" chi vi may dang HOME/PROBE (G35) - xem giai thich o printer_busy() (cyd_sd.cpp).
    // In kem trang thai de doc log biet vi sao bi tu choi.
    snprintf(s_msg, sizeof(s_msg), "Marlin dang IN TU THE SD - khong nhan file");
    HLOG("[HTTP] TU CHOI /upload tu %s: dang in tu the SD (st=%u fl=0x%02X) - xem printer_busy()\n",
         s_client, (unsigned)cydlcd.state, (unsigned)cydlcd.flags);
    snprintf(js, sizeof(js),
             "{\"status\":\"Error\",\"message\":\"Marlin dang IN TU THE SD (dang in thi khong nhan file) - dung in roi gui lai\"}");
    send_json(409, js);
    cyd_sd_temp_abort();
    return;
  }
#endif
  if (s_http_state == HS_ERR) {
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"%s\"}", s_msg);
    send_json(500, js);
    cyd_sd_temp_abort();
    return;
  }
  if (s_rx == 0 || cyd_sd_temp_path()[0] == '\0') {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "khong nhan duoc du lieu");
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"khong nhan duoc du lieu\"}");
    send_json(400, js);
    cyd_sd_temp_abort();              // file tam 0 byte / con mo -> don ngay (khong de lai rac)
    return;
  }
  // Tra loi NGAY (khong de trinh duyet cho vai chuc giay): viec ghi vao the SD chay o task()
  snprintf(js, sizeof(js),
           "{\"status\":\"Ok\",\"file\":\"%s\",\"rx_bytes\":%lu,\"total_bytes\":%lu,"
           "\"message\":\"da nhan, man hinh dang ghi vao the SD\"}",
           s_file, (unsigned long)s_rx, (unsigned long)s_total);
  HLOG("[HTTP] da nhan du %lu B (%s) -> se ghi vao the SD\n", (unsigned long)s_rx, s_file);
  send_json(200, js);
  s_finish_pending = true;
  s_http_state = HS_WRITE;
  snprintf(s_msg, sizeof(s_msg), "dang ghi vao the SD");
}

void handle_not_found(void) {
  char js[128];
  snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"khong co endpoint %s\"}",
           server.uri().c_str());
  send_json(404, js);
}

}  // namespace

// ===========================================================================
// API
// ===========================================================================
void net_upload_http_begin(void) {
  // LUU Y: net_wifi_begin() do main.cpp goi (can chay ca khi khong bat web server, vi duong BT
  // cung dung net_wifi). O day chi lo the TF + WebServer + mDNS.
  cyd_sd_begin();                   // mount the TF + don file tam mo coi
  server.on("/", HTTP_GET, handle_root);
  server.on("/status", HTTP_GET, handle_status);
  server.on("/upload", HTTP_POST, handle_upload_done, handle_upload_stream);
  server.onNotFound(handle_not_found);
  server.begin();
  s_started = true;
  HLOG("[HTTP] web server bat o cong %u (token: %s, mDNS: %s, heap=%u B)\n",
       (unsigned)CYDLCD_HTTP_PORT, CYDLCD_HTTP_TOKEN[0] ? "co" : "KHONG",
       HTTP_USE_MDNS ? CYDLCD_HTTP_MDNS_NAME : "tat", (unsigned)ESP.getFreeHeap());
#if HTTP_USE_MDNS
  if (MDNS.begin(CYDLCD_HTTP_MDNS_NAME)) {
    MDNS.addService("http", "tcp", CYDLCD_HTTP_PORT);
    HLOG("[HTTP] mDNS: http://%s.local/ (hoac http://%s.local/?token=%s)\n",
         CYDLCD_HTTP_MDNS_NAME, CYDLCD_HTTP_MDNS_NAME, CYDLCD_HTTP_TOKEN);
  } else {
    HLOG("[HTTP] CANH BAO: MDNS.begin(\"%s\") that bai - dung IP\n", CYDLCD_HTTP_MDNS_NAME);
  }
#endif
}

void net_upload_http_task(void) {
  if (!s_started) return;
  net_wifi_task();                     // cap nhat trang thai + tu ket noi lai (khong chan)
  cyd_sd_task();                    // theo doi the TF nong
  server.handleClient();               // nhan request (khong chan lau)

  // The SD nam o khe MAN HINH: khong can WiFi de "chot" phien upload - buoc ghi the chay ngay khi
  // file tam da nhan xong (WiFi chi la duong nhan file tu trinh duyet).
  if (s_finish_pending) {
    s_finish_pending = false;
    uint32_t written = 0;
    char err[64] = "";
    const uint32_t got = cyd_sd_temp_bytes();
    const bool ok = cyd_sd_finish_upload(s_file, got, &written, err, sizeof(err));
    if (ok) {
      s_http_state = HS_DONE;
      snprintf(s_msg, sizeof(s_msg), "da ghi vao the SD va bao Marlin mount lai");
      HLOG("[HTTP] DONE: %lu B -> the SD (M22/M21 da gui)\n", (unsigned long)written);
    } else {
      s_http_state = HS_ERR;
      snprintf(s_msg, sizeof(s_msg), "%s", err[0] ? err : "ghi vao the SD that bai");
      HLOG("[HTTP] ERR: %s\n", s_msg);
    }
  }
}

void net_upload_http_log(void) {
  cyd_sd_log();
}

bool net_upload_http_active(void) { return s_started; }

// true = dang co phien nhan/ghi file (UI dung de chan cac lenh lam Marlin doc the SD)
bool net_upload_active(void) { return s_http_state == HS_RECV || s_http_state == HS_WRITE; }

// Xoa file ngay tren man hinh CYD (che do the qua UART). Xem net_upload_http.h.
bool net_sd_delete_file(const char *name) { return cyd_sd_delete(name); }

// Dem muc con / xoa THU MUC RONG ngay tren man hinh CYD. Xem net_upload_http.h.
int  net_sd_dir_count(const char *name) { return cyd_sd_dir_count(name); }
bool net_sd_delete_dir(const char *name, bool *not_empty) { return cyd_sd_delete_dir(name, not_empty); }

// Tam dung / bat lai mDNS (dung khi QUET MANG - xem net_upload_http.h).
// LY DO: khi quet, STA roi kenh => interface "down"; responder mDNS rat de assert/panic trong tinh
// huong do (la mot trong cac nghi van gay man hinh CYD tu reboot). Tat han mDNS trong luc quet la an toan
// nhat: luc do mang cung khong dung duoc, xong quet thi bat lai.
bool net_upload_mdns_pause(const bool pause) {
#if CYDLCD_HTTP_USE_MDNS
  static bool s_mdns_on = false;
  if (pause) {
    if (s_mdns_on) {
      MDNS.end();
      s_mdns_on = false;
      HLOG("[HTTP] tam dung mDNS truoc khi quet mang\n");
    }
    return false;
  }
  if (!s_mdns_on) {
    if (MDNS.begin(CYDLCD_HTTP_MDNS_NAME)) {
      MDNS.addService("http", "tcp", CYDLCD_HTTP_PORT);
      s_mdns_on = true;
      HLOG("[HTTP] bat lai mDNS: http://%s.local/\n", CYDLCD_HTTP_MDNS_NAME);
    } else {
      HLOG("[HTTP] CANH BAO: khong bat lai duoc mDNS (dung IP)\n");
    }
  }
  return s_mdns_on;
#else
  // Khong bat mDNS (CYDLCD_HTTP_USE_MDNS 0) -> khong co gi phai tam dung
  (void)pause;
  return false;
#endif
}

#else   // ---- CYDLCD_WIFI_UPLOAD = 0: stub (moi env van link duoc) ----

void net_upload_http_begin(void) {}
void net_upload_http_task(void) {}
void net_upload_http_log(void) {}
bool net_upload_http_active(void) { return false; }
bool net_upload_active(void) { return false; }
bool net_sd_delete_file(const char *name) { (void)name; return false; }
int  net_sd_dir_count(const char *name) { (void)name; return -1; }
bool net_sd_delete_dir(const char *name, bool *not_empty) { (void)name; if (not_empty) *not_empty = false; return false; }
bool net_upload_mdns_pause(const bool pause) { (void)pause; return false; }

#endif  // CYDLCD_WIFI_UPLOAD
