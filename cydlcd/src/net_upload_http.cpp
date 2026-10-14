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
 html,body{height:100%}
 body{font-family:system-ui,Arial,sans-serif;margin:0;padding:16px;background:#12102a;color:#e6e1e5;
   box-sizing:border-box;min-height:100vh;display:flex;flex-direction:column}
 h1{font-size:20px;margin:0 0 4px;color:#00e5ff}
 .s{color:#9aa;font-size:13px;margin-bottom:14px}
 .card{background:#1e156e;border:1px solid #2a2080;border-radius:8px;padding:14px;margin-bottom:12px;
   display:flex;flex-direction:column;min-height:0}
 .card.up{flex:0 0 auto}
 .card.fl{flex:1 1 auto;min-height:180px}      /* the "File tren the SD": chiem HET phan con lai cua trang */
 input[type=file],input[type=text]{width:100%;box-sizing:border-box;margin:6px 0;padding:8px;
   background:#12102a;border:1px solid #2a2080;border-radius:6px;color:#e6e1e5}
 button{width:100%;padding:12px;font-size:16px;border:0;border-radius:6px;background:#00e5ff;
   color:#12102a;font-weight:700}
 button:disabled{opacity:.5}
 #bar{height:14px;background:#2a2080;border-radius:3px;overflow:hidden;margin-top:10px}
 #bar>i{display:block;height:100%;width:0;background:#00e5ff}
 pre{white-space:pre-wrap;font-size:12px;color:#9aa;margin:8px 0 0}
 .hd{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;flex:0 0 auto}
 button.mini{width:auto;padding:6px 10px;font-size:13px}
 /* Danh sach file: full size - chiem het chieu cao con lai cua the, chi cuon khi danh sach dai hon */
 .list{font-size:13px;flex:1 1 auto;min-height:0;overflow:auto}
 /* NUT XOA DAT TRUOC TEN FILE (yeu cau 2026-10-15: "nut xoa de truoc ten file cho khoi nham"): nut nam
    ben trai, ten file chiem phan con lai -> khong bi lan voi dong khac khi ten bi cat ngan. */
 .list .row{display:flex;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid #2a2080}
 .list .row span{flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
 button.del{flex:0 0 auto;width:auto;padding:6px 10px;font-size:13px;background:#ff5252;color:#fff}
</style></head><body>
<h1>TBT-CYDLCD</h1>
<div class="s">Gui G-code tu dien thoai/PC -> cydlcd ghi vao the TF -> M22/M21 cho Marlin doc lai.</div>
<div class="card up">
  <input type="text" id="tok" placeholder="token (vd TBT-3D-up)" value="%%TOKEN%%">
  <input type="file" id="f">
  <button id="go" onclick="send()">Gui file</button>
  <div id="bar"><i id="bi"></i></div>
  <pre id="log">san sang.</pre>
</div>
<div class="card fl">
  <div class="hd"><b>File tren the SD</b><button id="rf" class="mini" onclick="loadFiles()">Lam moi</button></div>
  <div id="flist" class="list">dang doc...</div>
</div>
<script>
var q=new URLSearchParams(location.search); if(q.get('token')) document.getElementById('tok').value=q.get('token');
function log(t){document.getElementById('log').textContent=t;}
function fmtB(n){ if(n>=1048576) return (n/1048576).toFixed(2)+' MB'; if(n>=1024) return Math.round(n/1024)+' KB'; return n+' B'; }
function poll(){fetch('/status').then(r=>r.json()).then(j=>{
  document.getElementById('bi').style.width=(j.total_bytes?Math.round(j.rx_bytes*100/j.total_bytes):0)+'%';
  log('cydlcd: '+j.state+' | nhan '+j.rx_bytes+'/'+j.total_bytes+' B | da day '+j.written_bytes+
      ' B | '+j.file+'\nket qua: '+(j.result||'-')+' '+(j.message||''));
  if(j.state=='writing'||j.state=='receiving') setTimeout(poll,700);
});}
function loadFiles(){
  var box=document.getElementById('flist'); box.textContent='dang doc...';
  fetch('/files?token='+encodeURIComponent(document.getElementById('tok').value))
   .then(r=>r.json()).then(function(j){
     box.textContent='';
     if(j.status!=='Ok'){ box.textContent='loi: '+(j.message||'?'); return; }
     if(!j.count){ box.textContent='(khong co file nao tren the / chua cam the)'; return; }
     j.files.forEach(function(f){
       var row=document.createElement('div'); row.className='row';
       var b=document.createElement('button'); b.className='del'; b.textContent='Xoa';
       b.title='Xoa '+(f.d?'thu muc ':'file ')+f.n;
       b.onclick=function(){ delFile(f.n, f.d, row, b); };
       var sp=document.createElement('span');
       sp.textContent=(f.d?'[TM] ':'')+f.n+(f.d?'':' - '+fmtB(f.s));
       row.appendChild(b); row.appendChild(sp);      // NUT XOA TRUOC, ten file sau
       box.appendChild(row);
     });
   }).catch(function(){ box.textContent='loi ket noi toi cydlcd'; });
}
function delFile(n,isdir,row,btn){
  // KHONG hoi xac nhan + KHONG goi lai /files sau khi xoa (yeu cau nguoi dung 2026-10-15):
  // xoa xong thi GO LUON dong do khoi danh sach dang hien (nhanh, khong nhay trang).
  // Xoa LOI thi giu nguyen dong + tra nut lai de bam lai (server da bao ly do o o log).
  if(btn) btn.disabled=true;
  var tok=document.getElementById('tok').value;
  fetch('/delete?token='+encodeURIComponent(tok)+'&name='+encodeURIComponent(n),{method:'POST'})
   .then(r=>r.json()).then(function(j){
     log('xoa '+(isdir?'thu muc ':'file ')+n+': '+(j.status==='Ok'?'XONG':'LOI')+' - '+(j.message||''));
     if(j.status==='Ok'){
       if(row && row.parentNode) row.parentNode.removeChild(row);
       var box=document.getElementById('flist');
       if(box && !box.querySelector('.row')) box.textContent='(khong con file nao tren the)';
     } else if(btn) btn.disabled=false;
   }).catch(function(){
     log('loi ket noi khi xoa '+n);
     if(btn) btn.disabled=false;
   });
}
function send(){
  var f=document.getElementById('f').files[0]; if(!f){log('chua chon file');return;}
  var tok=document.getElementById('tok').value;
  var fd=new FormData(); fd.append('name',f.name); fd.append('size',f.size); fd.append('file',f, f.name);
  var x=new XMLHttpRequest();
  x.open('POST','/upload?token='+encodeURIComponent(tok));
  x.setRequestHeader('X-CYDLCD-Token',tok);
  x.upload.onprogress=function(e){document.getElementById('bi').style.width=
     (e.lengthComputable?Math.round(e.loaded*100/e.total):0)+'%';};
  x.onload=function(){log('tra loi: '+x.responseText); poll(); loadFiles();};
  x.onerror=function(){log('loi ket noi toi cydlcd');};
  x.send(fd); log('dang gui '+f.name+' ('+f.size+' B)...');
}
loadFiles();
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
// Thoi diem lan cuoi phien upload co TIEN TRIEN (xem net_upload_active()/watchdog o main.cpp).
static volatile uint32_t s_last_activity_ms = 0;

// ---- CHAN DOAN: 1 request co MAY PHAN FILE? ----
// VI SAO (cau hoi nguoi dung 2026-10-15: "co phai do multiple part dan den open close file khac?"):
// WebServer cua ESP32 goi `UPLOAD_FILE_START` MOT LAN cho MOI PHAN FILE trong multipart. Neu client gui
// file thanh nhieu phan thi moi phan se vao `cyd_sd_temp_open()` -> (ban cu) XOA file tam roi mo lai =>
// MAT het du lieu da nhan. Dem lai de BIET CHAC thay vi doan: `parts`=so phan file cua request dang chay,
// `chunks`=so chunk WRITE. Ca 2 deu hien o /status va trong log.
static uint8_t  s_parts = 0;          // so phan file (UPLOAD_FILE_START) trong request dang chay
static uint32_t s_chunks = 0;         // so chunk (UPLOAD_FILE_WRITE) da nhan trong request dang chay
static bool     s_sess_busy = false;  // dang trong 1 request upload (tu START dau tien den handle_upload_done)
// TONG so byte file khach KHAI BAO (field text `size`, gui TRUOC phan file). 0 = khach khong khai bao.
// Dung de: (a) kiem tra cho trong the ngay tu dau, (b) chot "xong" khi tong byte ghi duoc == so nay.
static uint32_t s_declared = 0;

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
  // Chen TOKEN MAC DINH (CYDLCD_HTTP_TOKEN) vao o nhap token ngay khi tra trang - nguoi dung khong phai
  // go tay nua (yeu cau 2026-10-15: "chen luon gia tri token default vao input token"). Van doi duoc bang
  // `?token=...` (JS o cuoi trang se ghi de gia tri trong o nhap).
  // Trang nam trong FLASH (PROGMEM) nen phai COPY ra String (~4 KB heap, heap luc ranh ~135 KB) roi moi
  // thay the duoc placeholder.
  String page = FPSTR(HTML_PAGE);
  page.replace("%%TOKEN%%", CYDLCD_HTTP_TOKEN);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
  HLOG("[HTTP] GET / tu %s (%u B, token mac dinh: %s)\n",
       server.client().remoteIP().toString().c_str(), (unsigned)page.length(),
       CYDLCD_HTTP_TOKEN[0] ? "co" : "KHONG");
}

// ===========================================================================
// GET /status  (PHAI tra duoc ca khi chua co phien: state=idle)
// ===========================================================================
void handle_status(void) {
  char js[512];
  // `written_bytes`: dang NHAN thi lay so byte THUC da ghi xuong the cua phien dang chay; con lai lay ket
  // qua phien vua xong. Truoc day luon lay phien vua xong => trang web hien "da day 4523980 B" (so lieu
  // CU cua file truoc) trong khi file moi chi moi nhan duoc 1,7 MB - dung cai nguoi dung hoi 2026-10-15.
  const uint32_t written = (s_http_state == HS_RECV) ? cyd_sd_temp_bytes() : cyd_sd_last_written();
  const char *res = cyd_sd_last_result();
  snprintf(js, sizeof(js),
           "{\"cydlcd\":\"TBT\",\"device\":\"TBT-CYDLCD\",\"state\":\"%s\","
           "\"rx_bytes\":%lu,\"total_bytes\":%lu,\"written_bytes\":%lu,"
           "\"parts\":%u,\"parts_done\":%u,\"chunks\":%lu,\"declared\":%lu,"
           "\"file\":\"%s\",\"result\":\"%s\",\"message\":\"%s\","
           "\"sd\":%d,\"wifi\":%u,\"ip\":\"%s\",\"heap\":%lu}",
           state_str(), (unsigned long)s_rx, (unsigned long)s_total, (unsigned long)written,
           (unsigned)s_parts, (unsigned)cyd_sd_temp_parts_done(), (unsigned long)s_chunks,
           (unsigned long)s_declared,
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
      // Ranh gioi request: phan file DAU TIEN cua 1 request = phien moi (dem lai parts/chunks/tong).
      if (!s_sess_busy) {
        s_sess_busy = true;
        s_parts = 0;
        s_chunks = 0;
        s_total = 0;                            // CONG DON qua cac part -> phai reset o PHIEN MOI
        s_rx = 0;
        // So byte khach KHAI BAO (field text "size", gui truoc phan file - xem hop dong trong README).
        const String sz = server.arg("size");
        s_declared = sz.length() ? (uint32_t)sz.toInt() : 0;
      }
      s_parts++;
      // LUU Y (do that 2026-10-14): core ESP32 2.0.17 dat `totalSize = 0` o UPLOAD_FILE_START
      // (Parsing.cpp:428) va chi CONG DON dan trong luc ghi -> o day luon = 0. Kich thuoc THAT cua tung
      // part chi biet duoc o UPLOAD_FILE_END (Parsing.cpp:471) => cong don o nhanh END.
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
      HLOG("[HTTP] PHAN FILE #%u: \"%s\" tu %s (khach khai bao: %lu B)\n",
           (unsigned)s_parts, s_file, s_client, (unsigned long)s_declared);
      if (s_parts > 1) {
        // Nhieu phan trong CUNG 1 request: moi phan ghi vao 1 FILE PART rieng roi DONG NGAY; het request
        // man hinh GHEP lai (xem cyd_sd_finish_upload/merge_parts). Khong bao gio mo lai/xoa part da xong.
        HLOG("[HTTP] request nay co nhieu PHAN FILE - moi phan 1 file rieng, het request se GHEP lai\n");
      }
      s_http_state = HS_RECV;
      s_last_activity_ms = millis();
      snprintf(s_msg, sizeof(s_msg), "dang nhan file");
      if (!cyd_sd_temp_open(nm, s_parts == 1 ? s_declared : 0, s_parts)) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "khong mo duoc file tam (the TF?)");
        HLOG("[HTTP] LOI: %s\n", s_msg);
        return;                                    // UPLOAD_FILE_WRITE se tra ve 0 -> core huy
      }
      break;
    }
    case UPLOAD_FILE_WRITE: {
      if (s_http_state != HS_RECV) return;
      s_chunks++;
      s_last_activity_ms = millis();                 // con tien trien -> watchdog tiep tuc mien tru
      // `s_rx` = byte NHAN TU MANG (khong phu thuoc the ghi duoc bao nhieu) => /status phan anh dung
      // tien do that cua client, va cuoi phien SO SANH duoc voi so byte da ghi xuong the.
      s_rx += (uint32_t)up.currentSize;
      const size_t w = cyd_sd_temp_write(up.buf, up.currentSize);
      s_last_activity_ms = millis();                 // (cap nhat lai SAU khi ghi: ghi the co the lau)
      if (w != up.currentSize) {
        s_http_state = HS_ERR;
        // Ly do THAT lay tu lop the SD (cyd_sd_temp_err) - truoc day luon bao "ghi the TF loi" nen khong
        // phan biet duoc "file tam khong mo" (loi logic) voi "the ghi thieu" (loi the/FATFS).
        snprintf(s_msg, sizeof(s_msg), "%s%s", cyd_sd_temp_err()[0] ? cyd_sd_temp_err() : "ghi the TF loi",
                 " - thu gui lai");
        HLOG("[HTTP] LOI ghi the: %s (chunk %u B, nhan %lu B, ghi the %lu B, phan #%u)\n",
             s_msg, (unsigned)up.currentSize, (unsigned long)s_rx,
             (unsigned long)cyd_sd_temp_bytes(), (unsigned)s_parts);
      }
      break;
    }
    case UPLOAD_FILE_END: {
      // Kich thuoc THAT cua tung phan file chi co o day (totalSize = tong da cong don - Parsing.cpp:471)
      s_total += up.totalSize;              // CONG DON qua cac part = tong file
      if (s_http_state == HS_RECV) {
        // DONG PART NGAY khi het part (theo yeu cau nguoi dung: "moi lan nhan duoc part thi open va close
        // part do luon"): du lieu part nay nam chac tren the, va khong con file handle mo lau.
        const uint32_t tot = cyd_sd_temp_close();
        snprintf(s_msg, sizeof(s_msg), "da nhan xong, cho ghi vao the SD");
        HLOG("[HTTP] PHAN FILE #%u xong: %lu B (part), tong da ghi %lu B / %lu B khai bao\n",
             (unsigned)s_parts, (unsigned long)up.totalSize, (unsigned long)tot,
             (unsigned long)s_declared);
      }
      break;
    }
    default:                                       // UPLOAD_FILE_ABORT
      cyd_sd_temp_abort();
      s_http_state = HS_ERR;
      s_sess_busy = false;
      snprintf(s_msg, sizeof(s_msg), "client ngat giua luc gui");
      HLOG("[HTTP] %s (phan #%u, %u chunk, da nhan %lu B)\n",
           s_msg, (unsigned)s_parts, (unsigned)s_chunks, (unsigned long)s_rx);
      break;
  }
}

void handle_upload_done(void) {
  char js[256];
  // Het request (du thanh cong hay loi): mo phien ra de request sau lai la "phien moi".
  // Ghi chu chan doan: `parts`>1 nghia la client gui file thanh NHIEU PHAN (multipart nhieu phan file).
  if (!token_ok()) {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "sai ma token");
    HLOG("[HTTP] TU CHOI /upload tu %s: sai token (parts=%u)\n",
         server.client().remoteIP().toString().c_str(), (unsigned)s_parts);
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"sai ma token\"}");
    send_json(401, js);
    cyd_sd_temp_abort();
    s_sess_busy = false;
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
    s_sess_busy = false;
    return;
  }
#endif
  if (s_http_state == HS_ERR) {
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"%s\"}", s_msg);
    send_json(500, js);
    HLOG("[HTTP] HET REQUEST (LOI): parts=%u chunks=%u rx=%lu B, ly do: %s\n",
         (unsigned)s_parts, (unsigned)s_chunks, (unsigned long)s_rx, s_msg);
    cyd_sd_temp_abort();
    s_sess_busy = false;
    return;
  }
  if (s_rx == 0) {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "khong nhan duoc du lieu");
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"khong nhan duoc du lieu\"}");
    send_json(400, js);
    cyd_sd_temp_abort();              // khong co du lieu -> don moi file part/.tmp (khong de lai rac)
    s_sess_busy = false;
    return;
  }
  // CHOT "XONG" THEO SO BYTE (theo dung de xuat nguoi dung: "tao mot bien de cong don file size, neu bang
  // so luong user khai bao luc dau thi la done"): so byte da GHI XUONG THE phai bang so byte khach khai
  // bao (neu co khai bao) / bang so byte da nhan tu mang.
  const uint32_t got = cyd_sd_temp_bytes();
  const uint32_t expect = s_declared ? s_declared : s_rx;
  if (got != expect) {
    s_http_state = HS_ERR;
    snprintf(s_msg, sizeof(s_msg), "size khong khop (the co %lu B / can %lu B) - thu gui lai",
             (unsigned long)got, (unsigned long)expect);
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"%s\"}", s_msg);
    HLOG("[HTTP] ERR: %s (nhan %lu B tu mang, %u part) -> xoa het part, KHONG bao thanh cong\n",
         s_msg, (unsigned long)s_rx, (unsigned)cyd_sd_temp_parts_done());
    send_json(500, js);
    cyd_sd_temp_abort();
    s_sess_busy = false;
    return;
  }
  // Tra loi NGAY (khong de trinh duyet cho vai chuc giay): buoc doi ten/GHEP chay o task()
  snprintf(js, sizeof(js),
           "{\"status\":\"Ok\",\"file\":\"%s\",\"rx_bytes\":%lu,\"total_bytes\":%lu,\"parts\":%u,"
           "\"message\":\"da nhan, man hinh dang ghi vao the SD\"}",
           s_file, (unsigned long)s_rx, (unsigned long)s_total, (unsigned)s_parts);
  HLOG("[HTTP] HET REQUEST (OK): parts=%u chunks=%u da nhan %lu B, tren the %lu B (%s) -> "
       "doi ten/GHEP + ghi vao goc the\n",
       (unsigned)s_parts, (unsigned)s_chunks, (unsigned long)s_rx, (unsigned long)got, s_file);
  send_json(200, js);
  s_finish_pending = true;
  s_http_state = HS_WRITE;
  snprintf(s_msg, sizeof(s_msg), "dang ghi vao the SD");
  s_sess_busy = false;                 // request nay xong; request sau = phien moi
}

void handle_not_found(void) {
  char js[128];
  snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"khong co endpoint %s\"}",
           server.uri().c_str());
  send_json(404, js);
}

// ===========================================================================
// GET /files  -- danh sach file/thu muc o GOC the (cho phan "File tren the SD" cua trang web).
//   Tra: {"status":"Ok","sd":0|1,"count":N,"files":[{"n":"<ten>","s":<byte>,"d":0|1},...]}
//   - An cac muc he thong (cydlcd_tmp/BTUP) giong man FILES tren LCD.
//   - Doc bang FATFS CUA MAN HINH (the nam o khe man hinh) nen khong can Marlin.
// ===========================================================================
void handle_files(void) {
  if (!token_ok()) {
    HLOG("[HTTP] TU CHOI GET /files tu %s: sai token\n",
         server.client().remoteIP().toString().c_str());
    send_json(401, "{\"status\":\"Error\",\"message\":\"sai ma token\"}");
    return;
  }
  String js = "{\"status\":\"Ok\",\"sd\":";
  js += (cyd_sd_ready() ? '1' : '0');
  js += ",\"files\":[";
  char nm[64];
  uint8_t n = 0;
  for (uint8_t i = 0; i < 64; i++) {
    uint32_t sz = 0;
    bool isdir = false;
    if (!cyd_sd_root_entry(i, nm, sizeof(nm), &sz, &isdir)) break;
    if (n) js += ',';
    js += "{\"n\":\"";
    for (const char *p = nm; *p; p++) {          // escape JSON (" va \)
      if (*p == '"' || *p == '\\') js += '\\';
      js += *p;
    }
    js += "\",\"s\":";
    js += String((unsigned long)sz);
    js += ",\"d\":";
    js += (isdir ? '1' : '0');
    js += '}';
    n++;
  }
  js += "],\"count\":";
  js += String((unsigned)n);
  js += '}';
  HLOG("[HTTP] GET /files -> %u muc (sd=%d)\n", (unsigned)n, (int)cyd_sd_ready());
  send_json(200, js.c_str());
}

// ===========================================================================
// POST /delete?token=...&name=<ten>  -- xoa file (hoac THU MUC RONG) ngay tren trang web.
//   Chan: sai token (401), dang co phien upload (409), Marlin dang in tu the (409),
//         ten khong hop le / co duong dan (400), khong xoa duoc (500).
//   Xoa bang FATFS CUA MAN HINH (nhu nut XOA tren man FILES) roi BAO MARLIN MOUNT LAI (M22/M21) vi cache
//   cua Marlin dang giu trang thai CU (co the lam file da xoa "song lai" hoac hien lai trong danh sach).
// ===========================================================================
void handle_delete(void) {
  char js[200];
  if (!token_ok()) {
    HLOG("[HTTP] TU CHOI /delete tu %s: sai token\n",
         server.client().remoteIP().toString().c_str());
    send_json(401, "{\"status\":\"Error\",\"message\":\"sai ma token\"}");
    return;
  }
  if (net_upload_active()) {
    send_json(409, "{\"status\":\"Error\",\"message\":\"dang nhan file - xoa sau khi gui xong\"}");
    return;
  }
#if !CYDLCD_HTTP_ALLOW_WHILE_PRINTING
  if (cyd_sd_printer_busy()) {
    HLOG("[HTTP] TU CHOI /delete: Marlin dang in tu the SD (st=%u fl=0x%02X)\n",
         (unsigned)cydlcd.state, (unsigned)cydlcd.flags);
    send_json(409, "{\"status\":\"Error\",\"message\":\"Marlin dang IN TU THE SD - khong xoa file\"}");
    return;
  }
#endif
  char nm[64];
  unquote(nm, sizeof(nm), server.arg("name").c_str());
  // Chi cho xoa o GOC the: khong duong dan, khong "..", khong rong.
  if (!nm[0] || strchr(nm, '/') || strchr(nm, '\\') || strstr(nm, "..")) {
    send_json(400, "{\"status\":\"Error\",\"message\":\"ten file khong hop le\"}");
    return;
  }
  if (!cyd_sd_ready()) {
    send_json(503, "{\"status\":\"Error\",\"message\":\"chua co the TF\"}");
    return;
  }
  const int cnt = cyd_sd_dir_count(nm);      // >=0: la THU MUC (so muc con); -1: khong phai thu muc
  bool ok = false;
  if (cnt == 0) {
    bool not_empty = false;
    ok = cyd_sd_delete_dir(nm, &not_empty);
    if (ok) snprintf(js, sizeof(js), "{\"status\":\"Ok\",\"message\":\"da xoa thu muc rong %s\"}", nm);
    else    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"%s %s\"}",
                     not_empty ? "thu muc con muc con - khong xoa:" : "khong xoa duoc thu muc:", nm);
  } else if (cnt > 0) {
    snprintf(js, sizeof(js),
             "{\"status\":\"Error\",\"message\":\"thu muc con %d muc - chi xoa duoc thu muc RONG\"}", cnt);
  } else {
    ok = cyd_sd_delete(nm);
    snprintf(js, sizeof(js), ok ? "{\"status\":\"Ok\",\"message\":\"da xoa file %s\"}"
                                : "{\"status\":\"Error\",\"message\":\"khong xoa duoc %s (khong thay file?)\"}",
             nm);
  }
  HLOG("[HTTP] /delete \"%s\": %s\n", nm, ok ? "OK" : "LOI");
  send_json(ok ? 200 : 500, js);
  if (ok) cyd_sd_notify_marlin_remount();    // M22/M21 + xin lai danh sach (Marlin thay file da xoa)
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
  server.on("/files", HTTP_GET, handle_files);
  server.on("/delete", HTTP_POST, handle_delete);
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
    // Cac PART da duoc DONG ngay o UPLOAD_FILE_END; goi close o day la idempotent (giu cho an toan neu
    // con part nao chua dong) va tra ve TONG byte da ghi ca phien.
    const uint32_t got = cyd_sd_temp_close();
    // Chot lai lan cuoi: tong byte tren the phai bang so byte da nhan (xem handle_upload_done).
    const uint32_t expect = s_declared ? s_declared : s_rx;
    if (got != expect) {
      s_http_state = HS_ERR;
      snprintf(s_msg, sizeof(s_msg), "size khong khop khi chot (the co %lu B / can %lu B) - gui lai",
               (unsigned long)got, (unsigned long)expect);
      HLOG("[HTTP] ERR: %s -> xoa het part, KHONG doi ten\n", s_msg);
      cyd_sd_temp_abort();
      return;
    }
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

uint32_t net_upload_last_activity_ms(void) { return s_last_activity_ms; }

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
