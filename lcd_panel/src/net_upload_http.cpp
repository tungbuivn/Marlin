/**
 * net_upload_http.cpp -- Web server tren panel (ESP32 WebServer cua core) - xem net_upload_http.h.
 *
 * Trang HTML nam trong FLASH (PROGMEM) -> khong ton RAM (chi ~1,6 KB flash). Doc file trong 4 KB
 * mot lan khi day len ESP3D (net_esp3d.cpp) -> khong can buffer lon.
 */
#include "panel_config.h"

// Include header truoc `#if` de nhanh stub cung co linkage C (xem net_esp3d.cpp).
#include "net_upload_http.h"

#if PANEL_WIFI_UPLOAD

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>
#include <string.h>
#include <stdio.h>

#if PANEL_HTTP_USE_MDNS
  #include <ESPmDNS.h>
  #define HTTP_USE_MDNS 1
#else
  #define HTTP_USE_MDNS 0
#endif

#include "net_upload_http.h"
#include "net_esp3d.h"
#include "net_wifi.h"
#include "panel_link.h"

#if defined(PANEL_CYD_DEBUG)
  #define HLOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define HLOG(...)   do { } while (0)
#endif

namespace {

// ---- trang HTML (PROGMEM). Phai chua chuoi "TBT-PANEL" (client nhan dang du phong). ----
const char HTML_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="vi"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TBT-PANEL - gui file in</title>
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
<h1>TBT-PANEL</h1>
<div class="s">Gui G-code tu dien thoai/PC -> panel ghi vao the TF -> M22/M21 cho Marlin doc lai.</div>
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
  log('panel: '+j.state+' | nhan '+j.rx_bytes+'/'+j.total_bytes+' B | da day '+j.posted_bytes+
      ' B | '+j.file+'\nket qua: '+(j.result||'-')+' '+(j.message||''));
  if(j.state=='posting'||j.state=='receiving') setTimeout(poll,700);
});}
function send(){
  var f=document.getElementById('f').files[0]; if(!f){log('chua chon file');return;}
  var tok=document.getElementById('tok').value;
  var fd=new FormData(); fd.append('name',f.name); fd.append('file',f, f.name);
  var x=new XMLHttpRequest();
  x.open('POST','/upload?token='+encodeURIComponent(tok));
  x.setRequestHeader('X-Panel-Token',tok);
  x.upload.onprogress=function(e){document.getElementById('bi').style.width=
     (e.lengthComputable?Math.round(e.loaded*100/e.total):0)+'%';};
  x.onload=function(){log('tra loi: '+x.responseText); poll();};
  x.onerror=function(){log('loi ket noi toi panel');};
  x.send(fd); log('dang gui '+f.name+' ('+f.size+' B)...');
}
</script></body></html>)HTML";

WebServer server(PANEL_HTTP_PORT);
bool s_started = false;
bool s_push_pending = false;
char s_client[20] = "-";

// ---- trang thai cho /status ----
enum : uint8_t { HS_IDLE = 0, HS_RECV, HS_POST, HS_DONE, HS_ERR };
volatile uint8_t s_http_state = HS_IDLE;
uint32_t s_rx = 0;                 // byte da nhan tu client (nhanh)
uint32_t s_total = 0;              // tong byte theo Content-Length cua phan file (0 = khong biet)
char     s_file[40] = "";          // ten file dang nhan / vua nhan
char     s_msg[96] = "";           // thong bao cho /status (message)

const char *state_str(void) {
  switch (s_http_state) {
    case HS_RECV: return "receiving";
    case HS_POST: return "posting";
    case HS_DONE: return "done";
    case HS_ERR:  return "error";
    default:      return "idle";
  }
}

// Bo dau " o hai dau gia tri (core 2.0.17/ESP3D cat chuoi theo substring -> co the lan ca dau ")
void unquote(char *dst, const size_t cap, const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  size_t i = 0, o = 0;
  if (src[0] == '"') i = 1;
  while (src[i] != '\0' && src[i] != '"' && o + 1u < cap) dst[o++] = src[i++];
  dst[o] = '\0';
}

bool token_ok(void) {
  if (PANEL_HTTP_TOKEN[0] == '\0') return true;       // khong cau hinh token -> khong kiem
  const String q = server.arg("token");
  if (q.length() && q == PANEL_HTTP_TOKEN) return true;
  const String h = server.header("X-Panel-Token");
  if (h.length() && h == PANEL_HTTP_TOKEN) return true;
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
  const uint32_t posted = net_esp3d_last_posted();
  const char *res = net_esp3d_last_result();
  snprintf(js, sizeof(js),
           "{\"panel\":\"TBT\",\"device\":\"TBT-PANEL\",\"state\":\"%s\","
           "\"rx_bytes\":%lu,\"total_bytes\":%lu,\"posted_bytes\":%lu,"
           "\"file\":\"%s\",\"result\":\"%s\",\"message\":\"%s\","
           "\"sd\":%d,\"wifi\":%u,\"ip\":\"%s\",\"heap\":%lu}",
           state_str(), (unsigned long)s_rx, (unsigned long)s_total, (unsigned long)posted,
           s_file, res, s_msg, (int)net_esp3d_sd_ready(), (unsigned)net_wifi_state(),
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
      // handle_upload_done chi la lop chan thu 2). Query ?token= / header X-Panel-Token DA duoc
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
      if (!net_esp3d_temp_open(nm, s_total)) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "khong mo duoc file tam (the TF?)");
        HLOG("[HTTP] LOI: %s\n", s_msg);
        return;                                    // UPLOAD_FILE_WRITE se tra ve 0 -> core huy
      }
      break;
    }
    case UPLOAD_FILE_WRITE: {
      if (s_http_state != HS_RECV) return;
      const size_t w = net_esp3d_temp_write(up.buf, up.currentSize);
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
        net_esp3d_temp_close();
        snprintf(s_msg, sizeof(s_msg), "da nhan xong, cho ghi vao the SD");
        HLOG("[HTTP] nhan xong %lu B (totalSize=%lu) -> %s\n", (unsigned long)s_rx,
             (unsigned long)s_total, net_esp3d_temp_path());
      }
      break;
    }
    default:                                       // UPLOAD_FILE_ABORT
      net_esp3d_temp_abort();
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
    net_esp3d_temp_abort();
    return;
  }
#if !PANEL_HTTP_ALLOW_WHILE_PRINTING
  if (net_esp3d_printer_busy()) {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "Marlin dang in");
    HLOG("[HTTP] TU CHOI /upload tu %s: Marlin dang in\n", s_client);
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"Marlin dang in - khong nhan file\"}");
    send_json(409, js);
    net_esp3d_temp_abort();
    return;
  }
#endif
  if (s_http_state == HS_ERR) {
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"%s\"}", s_msg);
    send_json(500, js);
    net_esp3d_temp_abort();
    return;
  }
  if (s_rx == 0 || net_esp3d_temp_path()[0] == '\0') {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "khong nhan duoc du lieu");
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"khong nhan duoc du lieu\"}");
    send_json(400, js);
    net_esp3d_temp_abort();              // file tam 0 byte / con mo -> don ngay (khong de lai rac)
    return;
  }
  // Tra loi NGAY (khong de trinh duyet cho vai chuc giay): viec ghi vao the / day sang ESP3D
  // chay o task()
#if PANEL_SD_SHARED || PANEL_SD_UART
  snprintf(js, sizeof(js),
           "{\"status\":\"Ok\",\"file\":\"%s\",\"rx_bytes\":%lu,\"total_bytes\":%lu,"
           "\"message\":\"da nhan, panel dang ghi vao the SD\"}",
           s_file, (unsigned long)s_rx, (unsigned long)s_total);
  HLOG("[HTTP] da nhan du %lu B (%s) -> se ghi vao the SD\n", (unsigned long)s_rx, s_file);
#else
  snprintf(js, sizeof(js),
           "{\"status\":\"Ok\",\"file\":\"%s\",\"rx_bytes\":%lu,\"total_bytes\":%lu,"
           "\"message\":\"da nhan, panel dang day len ESP3D\"}",
           s_file, (unsigned long)s_rx, (unsigned long)s_total);
  HLOG("[HTTP] da nhan du %lu B (%s) -> se day len ESP3D\n", (unsigned long)s_rx, s_file);
#endif
  send_json(200, js);
  s_push_pending = true;
  s_http_state = HS_POST;
#if PANEL_SD_SHARED || PANEL_SD_UART
  snprintf(s_msg, sizeof(s_msg), "dang ghi vao the SD");
#else
  snprintf(s_msg, sizeof(s_msg), "dang day len ESP3D");
#endif
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
  net_esp3d_begin();                   // mount the TF + don file tam mo coi
  server.on("/", HTTP_GET, handle_root);
  server.on("/status", HTTP_GET, handle_status);
  server.on("/upload", HTTP_POST, handle_upload_done, handle_upload_stream);
  server.onNotFound(handle_not_found);
  server.begin();
  s_started = true;
  HLOG("[HTTP] web server bat o cong %u (token: %s, mDNS: %s, heap=%u B)\n",
       (unsigned)PANEL_HTTP_PORT, PANEL_HTTP_TOKEN[0] ? "co" : "KHONG",
       HTTP_USE_MDNS ? PANEL_HTTP_MDNS_NAME : "tat", (unsigned)ESP.getFreeHeap());
#if HTTP_USE_MDNS
  if (MDNS.begin(PANEL_HTTP_MDNS_NAME)) {
    MDNS.addService("http", "tcp", PANEL_HTTP_PORT);
    HLOG("[HTTP] mDNS: http://%s.local/ (hoac http://%s.local/?token=%s)\n",
         PANEL_HTTP_MDNS_NAME, PANEL_HTTP_MDNS_NAME, PANEL_HTTP_TOKEN);
  } else {
    HLOG("[HTTP] CANH BAO: MDNS.begin(\"%s\") that bai - dung IP\n", PANEL_HTTP_MDNS_NAME);
  }
#endif
}

void net_upload_http_task(void) {
  if (!s_started) return;
  net_wifi_task();                     // cap nhat trang thai + tu ket noi lai (khong chan)
  net_esp3d_task();                    // theo doi the TF nong
  server.handleClient();               // nhan request (khong chan lau)

  // Che do THE DUNG CHUNG (PANEL_SD_SHARED): khong can WiFi de "day file" - buoc ghi the chay
  // ngay khi file tam da nhan xong. Che do ESP3D cu: phai cho WiFi ket noi truoc khi POST.
  const bool can_push = (PANEL_SD_SHARED || PANEL_SD_UART) ? true : (net_wifi_state() == 2);
  if (s_push_pending && can_push) {
    s_push_pending = false;
    uint32_t posted = 0;
    char err[64] = "";
    const uint32_t got = net_esp3d_temp_bytes();
    const bool ok = net_esp3d_push(s_file, got, &posted, err, sizeof(err));
    if (ok) {
      s_http_state = HS_DONE;
#if PANEL_SD_SHARED || PANEL_SD_UART
      snprintf(s_msg, sizeof(s_msg), "da ghi vao the SD va gui M22/M21");
      HLOG("[HTTP] DONE: %lu B -> the SD (M22/M21 da gui)\n", (unsigned long)posted);
#else
      snprintf(s_msg, sizeof(s_msg), "da day xong len ESP3D va gui M22/M21");
      HLOG("[HTTP] DONE: %lu B -> ESP3D, da gui M22/M21\n", (unsigned long)posted);
#endif
    } else {
      s_http_state = HS_ERR;
      snprintf(s_msg, sizeof(s_msg), "%s", err[0] ? err : "ghi vao the SD that bai");
      HLOG("[HTTP] ERR: %s\n", s_msg);
    }
  } else if (s_push_pending) {
    // WiFi chua ket noi -> doi (khong chan)
    if (millis() % 4000u < 40u) HLOG("[HTTP] cho WiFi truoc khi day file len ESP3D...\n");
  }
}

void net_upload_http_log(void) {
  net_esp3d_log();
}

bool net_upload_http_active(void) { return s_started; }

// true = dang co phien nhan/ghi file (UI dung de chan cac lenh lam Marlin doc the SD)
bool net_upload_active(void) { return s_http_state == HS_RECV || s_http_state == HS_POST; }

// Xoa file ngay tren panel (che do the qua UART). Xem net_upload_http.h.
bool net_sd_delete_file(const char *name) { return net_esp3d_delete(name); }

#else   // ---- PANEL_WIFI_UPLOAD = 0: stub (moi env van link duoc) ----

void net_upload_http_begin(void) {}
void net_upload_http_task(void) {}
void net_upload_http_log(void) {}
bool net_upload_http_active(void) { return false; }
bool net_upload_active(void) { return false; }
bool net_sd_delete_file(const char *name) { (void)name; return false; }

#endif  // PANEL_WIFI_UPLOAD
