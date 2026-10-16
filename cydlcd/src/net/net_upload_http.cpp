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
#include "ui_lvgl.h"          // ui_shot_band(): chup man hinh (ui_lvgl.h tu guard bang CYDLCD_USE_LVGL)

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
 /* Hang tieu de: <h1> ben trai, nut "Chup man hinh" ben phai */
 .hdhdr{display:flex;justify-content:space-between;align-items:center;gap:10px}
 /* Lop phu XEM ANH CHUP: phu kin man hinh, anh vua khung, duoi la 2 nut Luu / Dong */
 #snapov{display:none;position:fixed;inset:0;background:rgba(0,0,0,.88);z-index:9;
   flex-direction:column;align-items:center;justify-content:center;padding:16px;gap:12px}
 #snapov img{width:100%;max-width:640px;background:#000;border:1px solid #2a2080;border-radius:6px;
   image-rendering:pixelated}       /* pixelated: anh 320x240 phong to nhin ro tung diem, khong bi nhoe */
 .snaprow{display:flex;gap:10px;width:100%;max-width:640px}
 .snaprow>*{flex:1 1 0}
 a.dl{display:block;text-align:center;text-decoration:none;padding:10px;border-radius:6px;
   background:#00e5ff;color:#12102a;font-weight:700;font-size:13px;box-sizing:border-box}
 #snapinfo{font-size:12px;color:#9aa;text-align:center}
 /* Danh sach file: full size - chiem het chieu cao con lai cua the, chi cuon khi danh sach dai hon */
 .list{font-size:13px;flex:1 1 auto;min-height:0;overflow:auto}
 /* NUT XOA DAT TRUOC TEN FILE (yeu cau 2026-10-15: "nut xoa de truoc ten file cho khoi nham"): nut nam
    ben trai, ten file chiem phan con lai -> khong bi lan voi dong khac khi ten bi cat ngan. */
 .list .row{display:flex;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid #2a2080}
 .list .row span{flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
 button.del{flex:0 0 auto;width:auto;padding:6px 10px;font-size:13px;background:#ff5252;color:#fff}
 /* Khu THONG BAO (thay cho hop thoai tren LCD): luon hien message LAN CUOI; het thoi gian tu an thi an di */
 .ntf{margin-top:10px;padding:8px 10px;border-radius:6px;font-size:13px;background:#2a2080;
   border-left:4px solid #00e5ff;white-space:pre-wrap}
 .ntf.warn{border-left-color:#ffb300}
 .ntf.err{border-left-color:#ff5252}
</style></head><body>
<div class="hdhdr">
  <h1>TBT-CYDLCD</h1>
  <!-- NUT CHUP MAN HINH (yeu cau nguoi dung 2026-10-16). Goi GET /screen.bmp roi hien anh o lop phu
       ngay trong trang (khong mo tab moi) + cho LUU ve may voi ten co NGAY GIO. -->
  <button id="snapb" class="mini" onclick="snap()">Chup man hinh</button>
</div>
<div class="s">Gui G-code tu dien thoai/PC -> cydlcd ghi vao the TF -> M22/M21 cho Marlin doc lai.</div>
<div class="card up">
  <input type="text" id="tok" placeholder="token (vd TBT-3D-up)" value="%%TOKEN%%">
  <!-- `multiple`: chon NHIEU file cung luc (yeu cau nguoi dung 2026-10-16). Chung se duoc gui LAN LUOT
       vi man hinh CYD chi nhan 1 phien upload tai mot thoi diem (xem sendNext() trong JS). -->
  <input type="file" id="f" onchange="updGo()" multiple>
  <button id="go" onclick="send()" disabled>Gui file</button>
  <!-- DONG RIENG cho tien do gui NHIEU file. KHONG dung o #log vi `poll()` ghi de o do moi 700 ms
       trong luc upload -> tien do se bi xoa ngay. -->
  <div id="sel" class="s" style="margin:6px 0 0"></div>
  <div id="bar"><i id="bi"></i></div>
  <!-- THONG BAO status: hop thong bao tren LCD (co nut OK + tu an) KHONG hien tren man hinh nua ma hien o
       day; giu lai message LAN CUOI, het thoi gian tu an (neu co) thi tu mat, khong co thoi gian thi de lai -->
  <div id="ntf" class="ntf" style="display:none"></div>
  <pre id="log">san sang.</pre>
</div>
<div class="card fl">
  <div class="hd"><b>File tren the SD</b><button id="rf" class="mini" onclick="loadFiles()">Lam moi</button></div>
  <div id="flist" class="list">dang doc...</div>
</div>
<!-- LOP PHU XEM ANH CHUP (an mac dinh). Nut "Chup man hinh" o tieu de bat no len; Esc hoac "Dong" de tat.
     PHAI nam NGOAI khoi lenh JavaScript ben duoi - de lot vao trong do la trang web vo. -->
<div id="snapov">
  <img id="snapimg" alt="anh chup man hinh cydlcd">
  <div class="snaprow">
    <a id="snapdl" class="dl" download>Luu .bmp</a>
    <button class="mini" onclick="snapClose()">Dong</button>
  </div>
  <div id="snapinfo"></div>
</div>
<script>
var q=new URLSearchParams(location.search); if(q.get('token')) document.getElementById('tok').value=q.get('token');
var upBusy=false;                      // dang gui file -> khoa nut "Gui file"
function log(t){document.getElementById('log').textContent=t;}
// ---------------------------------------------------------------------------
// HANG DOI REQUEST (yeu cau nguoi dung 2026-10-15: "request den status va files dang chay song song nen se
// loi, cho status xong thi moi goi api files").
// VI SAO: WebServer cua ESP32 chi phuc vu MOT request mot lan; trinh duyet mo 2 ket noi song song thi
// request sau bi treo/timeout (nhat la trong luc dang upload - vong lap bi chan 20-30 s).
// Cach lam: moi request noi vao mot chuoi Promise -> cai sau chi BAT DAU khi cai truoc XONG.
// ---------------------------------------------------------------------------
var apiQ=Promise.resolve();
function apiFetch(url,opt){
  var r=apiQ.catch(function(){}).then(function(){ return fetch(url,opt); })
                            .then(function(res){ return res.json(); });
  apiQ=r.catch(function(){});          // 1 request loi khong lam dut chuoi
  return r;
}
// Nut "Gui file": CHI bat khi DA CHON FILE va KHONG dang gui.
// CSS da co `button:disabled{opacity:.5}` va trinh duyet chan click khi nut bi disabled.
function updGo(){
  var n=document.getElementById('f').files.length;
  document.getElementById('go').disabled=(n<1 || upBusy);
  // Dong tien do: chi cap nhat khi DANG RANH (luc dang gui thi sendNext() viet vao do).
  if(!upBusy) document.getElementById('sel').textContent =
    (n ? (n+' file da chon - se gui LAN LUOT (gui xong tung file mot)') : '');
}

// ---------------------------------------------------------------------------
//  GUI NHIEU FILE (yeu cau nguoi dung 2026-10-16: "sua upload cho phep chon multiple file tren trinh duyet")
//
//  VI SAO GUI LAN LUOT chu khong song song: WebServer cua ESP32 phuc vu 1 request mot lan, va
//  net_upload_http TU CHOI phien thu 2 bang 409 "dang nhan file". Ngoai ra moi file gui xong deu lam
//  Marlin mount lai the (M22/M21) -> gui chong se lam hong ca hai.
//
//  TIEN DO dat o #sel (khong phai #log): trong luc upload, `poll()` ghi de #log moi 700 ms.
// ---------------------------------------------------------------------------
var upQueue=[], upTotal=0, upDone=0, upFail=0;

function send(){
  var fs=document.getElementById('f').files;
  if(!fs.length){log('chua chon file');updGo();return;}
  if(upBusy){log('dang gui file khac - doi xong da');updGo();return;}
  // Chup lai danh sach ngay (nguoi dung co the doi lua chon giua chung -> khong anh huong lan gui nay).
  upQueue=Array.prototype.slice.call(fs);
  upTotal=upQueue.length; upDone=0; upFail=0;
  upBusy=true; updGo();
  sendNext();
}

function sendNext(){
  if(!upQueue.length){                       // het hang doi -> tong ket
    // KHONG goi updGo() o day: no ghi de #sel bang dong "N file da chon" -> mat thong bao XONG.
    // Cap nhat nut bang tay roi moi viet tong ket.
    upBusy=false;
    var n=document.getElementById('f').files.length;
    document.getElementById('go').disabled=(n<1);
    document.getElementById('sel').textContent =
      'XONG: '+upDone+'/'+upTotal+' file'+(upFail?(' - '+upFail+' file LOI (xem o log)'):'');
    log('gui xong '+upDone+'/'+upTotal+' file'+(upFail?(' - '+upFail+' LOI'):''));
    pollErr=0; poll(true);
    return;
  }
  var f=upQueue.shift();
  var tok=document.getElementById('tok').value;
  var fd=new FormData(); fd.append('name',f.name); fd.append('size',f.size); fd.append('file',f, f.name);
  var x=new XMLHttpRequest();
  x.open('POST','/upload?token='+encodeURIComponent(tok));
  x.setRequestHeader('X-CYDLCD-Token',tok);
  var idx=upDone+upFail+1;
  document.getElementById('bi').style.width='0%';
  document.getElementById('sel').textContent='dang gui '+idx+'/'+upTotal+': '+f.name;
  x.upload.onprogress=function(e){
    var p=e.lengthComputable?Math.round(e.loaded*100/e.total):0;
    document.getElementById('bi').style.width=p+'%';
    document.getElementById('sel').textContent='dang gui '+idx+'/'+upTotal+': '+f.name+' ('+p+'%)';
  };
  // Xong 1 file -> goi sendNext() cho file ke tiep (KHONG doi poll xong).
  x.onload=function(){
    var ok=(x.status===200 && x.responseText.indexOf('"Ok"')>=0);
    if(ok) upDone++; else { upFail++; log('LOI '+idx+'/'+upTotal+' '+f.name+': '+x.responseText); }
    document.getElementById('bi').style.width='0%';
    sendNext();
  };
  x.onerror=function(){
    upFail++; log('LOI ket noi khi gui '+idx+'/'+upTotal+' '+f.name);
    document.getElementById('bi').style.width='0%';
    sendNext();
  };
  x.send(fd);
}
function fmtB(n){ if(n>=1048576) return (n/1048576).toFixed(2)+' MB'; if(n>=1024) return Math.round(n/1024)+' KB'; return n+' B'; }
// `reloadAtEnd`: sau khi phien upload KET THUC (done/error) thi moi nap lai danh sach file -> /files luon
// chay SAU /status (khong con 2 request song song).
// `pollRun`: CHI cho phep 1 luong /status chay tai mot thoi diem (neu dang chay thi bo qua loi goi moi) -
// tranh 2 luong poll cung luc lam /files bi chen ngang.
var pollErr=0, pollRun=false;
function poll(reloadAtEnd){
  if(pollRun) return;
  pollRun=true; pollErr=0;
  var step=function(){
    apiFetch('/status').then(function(j){
      pollErr=0;
      document.getElementById('bi').style.width=(j.total_bytes?Math.round(j.rx_bytes*100/j.total_bytes):0)+'%';
      log('cydlcd: '+j.state+' | nhan '+j.rx_bytes+'/'+j.total_bytes+' B | da day '+j.written_bytes+
          ' B | '+j.file+'\nket qua: '+(j.result||'-')+' '+(j.message||''));
      // THONG BAO: /status luu message LAN CUOI; co thoi gian thi dem nguoc roi tu mat, khong co thi de lai.
      var nd=document.getElementById('ntf'), n=j.notify||'';
      if(n){
        nd.className='ntf'+(j.notify_sev==2?' err':(j.notify_sev==1?' warn':''));
        nd.textContent=n+(j.notify_left_ms?('   ['+Math.ceil(j.notify_left_ms/1000)+'s]'):'');
        nd.style.display='block';
      } else { nd.style.display='none'; nd.textContent=''; }
      if(j.state=='writing'||j.state=='receiving') setTimeout(step,700);
      else { pollRun=false; if(reloadAtEnd) loadFiles();
             // Khi RANH: van hoi lai cham (5 s) de thong bao moi cua Marlin hien ra kip thoi
             setTimeout(function(){ poll(false); },5000); }
    }).catch(function(){
      if(++pollErr<=5) setTimeout(step,1500);               // thu lai vai lan roi thoi
      else { pollRun=false; log('khong doc duoc /status (man hinh dang ban?) - bam Lam moi sau'); }
    });
  };
  step();
}
function loadFiles(){
  var box=document.getElementById('flist'); box.textContent='dang doc...';
  apiFetch('/files?token='+encodeURIComponent(document.getElementById('tok').value))
   .then(function(j){
     box.textContent='';
     if(j.status!=='Ok'){ box.textContent='loi: '+(j.message||'?'); return; }
     if(!j.count){ box.textContent='(khong co file nao tren the / chua cam the)'; return; }
     j.files.forEach(function(f){
       var row=document.createElement('div'); row.className='row';
       var b=document.createElement('button'); b.className='del'; b.textContent='Xoa';
       b.title='Xoa '+(f.d?'thu muc ':'file ')+f.n;
       b.onclick=function(){ delFile(f.n, f.d, row, b); };
       var sp=document.createElement('span');
       // KHONG hien size file nua (2026-10-16): trang web chi can TEN, va bo size la de man hinh
       // dung getNextFileName() = 1 readdir khong mo file/khong stat -> liet ke nhanh hon nhieu.
       sp.textContent=(f.d?'[TM] ':'')+f.n;
       row.appendChild(b); row.appendChild(sp);      // NUT XOA TRUOC, ten file sau
       box.appendChild(row);
     });
   }).catch(function(){ box.textContent='loi ket noi toi cydlcd'; });
}
function delFile(n,isdir,row,btn){
  // KHONG hoi xac nhan + KHONG goi lai /files sau khi xoa: xoa xong thi GO LUON dong do khoi danh sach.
  // Xoa LOI thi giu nguyen dong + tra nut lai de bam lai (server da bao ly do o o log).
  if(btn) btn.disabled=true;
  var tok=document.getElementById('tok').value;
  apiFetch('/delete?token='+encodeURIComponent(tok)+'&name='+encodeURIComponent(n),{method:'POST'})
   .then(function(j){
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
// ---- CHUP MAN HINH: nut "Chup man hinh" o tieu de goi ham nay ----
// Lay GET /screen.bmp (man hinh doc nguoc panel) -> hien trong lop phu #snapov + cho LUU ve may.
// PHAI them ?t=<ms>: khong co thi trinh duyet dung lai anh CU trong cache, bam lan sau van ra anh cu.
var snapUrl=null;
function snapName(){
  var d=new Date(), p=function(n){return String(n).padStart(2,'0');};
  return 'cyd-'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'-'+
         p(d.getHours())+p(d.getMinutes())+p(d.getSeconds())+'.bmp';
}
function snapClose(){document.getElementById('snapov').style.display='none';}
function snap(){
  var b=document.getElementById('snapb');
  if(b.disabled) return;                       // dang chup -> bo qua (tranh bam 2 lan)
  b.disabled=true; b.textContent='Dang chup...';
  var t0=Date.now();
  fetch('/screen.bmp?t='+t0,{cache:'no-store'})
   .then(function(r){ if(!r.ok) throw new Error('HTTP '+r.status); return r.blob(); })
   .then(function(bl){
     if(snapUrl) URL.revokeObjectURL(snapUrl);  // giai phong anh cu (bam nhieu lan se ro ri bo nho)
     snapUrl=URL.createObjectURL(bl);
     var name=snapName();
     document.getElementById('snapimg').src=snapUrl;
     var a=document.getElementById('snapdl'); a.href=snapUrl; a.download=name;
     document.getElementById('snapinfo').textContent=name+'  |  '+fmtB(bl.size)+'  |  '+(Date.now()-t0)+' ms';
     document.getElementById('snapov').style.display='flex';
     log('da chup man hinh: '+name+' ('+fmtB(bl.size)+')');
   })
   .catch(function(e){ log('chup man hinh LOI: '+e.message); })
   .finally(function(){ b.disabled=false; b.textContent='Chup man hinh'; });
}
// Esc de dong lop phu (tien khi xem tren may tinh)
document.addEventListener('keydown',function(e){ if(e.key==='Escape') snapClose(); });

// `?snap=1` -> TU CHUP ngay khi mo trang. Tien cho nguoi dung (luu 1 link, bam la co anh ngay) va
// cung la cach kiem tra tu dong bang trinh duyet headless.
if(q.get('snap')) snap();

updGo();
poll(true);                            // doc trang thai truoc, xong moi nap danh sach file
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

// ---- THONG BAO (status message): giu message LAN CUOI de tra o /status, co the tu an theo thoi gian ----
// Xem net_upload_http.h. `s_notify_ttl_ms` = 0 -> giu mai (khong co thoi gian).
static char     s_notify[CYDLCD_NOTIFY_LEN] = "";
static uint8_t  s_notify_sev = 0;
static uint32_t s_notify_set_ms = 0;
static uint32_t s_notify_ttl_ms = 0;
static uint32_t s_notify_seq = 0;      // tang moi lan co thong bao moi (client biet la thong bao MOI)

// Don thong bao neu da qua thoi gian tu an. Goi o MOI lan doc /status (va khi doc chan doan).
static void notify_expire(void) {
  if (!s_notify[0] || !s_notify_ttl_ms) return;
  if ((uint32_t)(millis() - s_notify_set_ms) < s_notify_ttl_ms) return;
  HLOG("[HTTP] thong bao het thoi gian (%lu ms) -> xoa: \"%s\"\n",
       (unsigned long)s_notify_ttl_ms, s_notify);
  s_notify[0] = '\0';
  s_notify_ttl_ms = 0;
}

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
  char js[900];
  notify_expire();                       // don thong bao da qua thoi gian tu an (neu co)
  // `written_bytes`: dang NHAN thi lay so byte THUC da ghi xuong the cua phien dang chay; con lai lay ket
  // qua phien vua xong. Truoc day luon lay phien vua xong => trang web hien "da day 4523980 B" (so lieu
  // CU cua file truoc) trong khi file moi chi moi nhan duoc 1,7 MB - dung cai nguoi dung hoi 2026-10-15.
  const uint32_t written = (s_http_state == HS_RECV) ? cyd_sd_temp_bytes() : cyd_sd_last_written();
  const char *res = cyd_sd_last_result();
  // `notify_*`: THONG BAO luu lai cho trang status (hop thong bao co nut OK + tu an KHONG hien tren LCD nua).
  // `notify_left_ms` = thoi gian con lai truoc khi tu mat (0 = khong co thoi gian -> giu mai).
  const uint32_t n_age = s_notify[0] ? (uint32_t)(millis() - s_notify_set_ms) : 0;
  const uint32_t n_left = (s_notify[0] && s_notify_ttl_ms && s_notify_ttl_ms > n_age)
                          ? (s_notify_ttl_ms - n_age) : 0;
  snprintf(js, sizeof(js),
           "{\"cydlcd\":\"TBT\",\"device\":\"TBT-CYDLCD\",\"state\":\"%s\","
           "\"rx_bytes\":%lu,\"total_bytes\":%lu,\"written_bytes\":%lu,"
           "\"parts\":%u,\"parts_done\":%u,\"chunks\":%lu,\"declared\":%lu,"
           "\"file\":\"%s\",\"result\":\"%s\",\"message\":\"%s\","
           "\"notify\":\"%s\",\"notify_sev\":%u,\"notify_ttl_ms\":%lu,\"notify_left_ms\":%lu,"
           "\"notify_seq\":%lu,\"msg_on_lcd\":%d,"
           "\"sd\":%d,\"wifi\":%u,\"ip\":\"%s\",\"heap\":%lu}",
           state_str(), (unsigned long)s_rx, (unsigned long)s_total, (unsigned long)written,
           (unsigned)s_parts, (unsigned)cyd_sd_temp_parts_done(), (unsigned long)s_chunks,
           (unsigned long)s_declared,
           s_file, res, s_msg,
           s_notify, (unsigned)s_notify_sev, (unsigned long)s_notify_ttl_ms, (unsigned long)n_left,
           (unsigned long)s_notify_seq, (int)CYDLCD_MSG_BOX_ON_LCD,
           (int)cyd_sd_ready(), (unsigned)net_wifi_state(),
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
      // `new_sess` = PHAN DAU cua mot REQUEST MOI. LAY NGAY O DAY, TRUOC khi dat `s_sess_busy = true`
      // (dat roi thi khong con phan biet duoc nua). Day la tin hieu GOC de cyd_sd_temp_open() biet
      // phai MO DANG GHI (truncate) hay GHI TIEP (append) - xem ghi chu o cyd_sd_temp_open().
      const bool new_sess = !s_sess_busy;
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
      // ---------------------------------------------------------------------------
      // BAO MAT: LAM SACH ten khach gui len (bo phan duong dan "../", doi ky tu la thanh '_', them ".gco"
      // neu chua co duoi) ROI moi KIEM TRA LAI bang whitelist `cyd_sd_name_check()`. Ten sau khi lam sach
      // luon hop le (do cung mot dinh nghia ky tu), nhung van kiem tra de chan truong hop bat ngo.
      // ---------------------------------------------------------------------------
      char nm_ok[40];
      cyd_sd_name_clean(nm, nm_ok, sizeof(nm_ok));
      const char *bad = cyd_sd_name_check(nm_ok);
      if (bad) {
        s_http_state = HS_ERR;
        snprintf(s_msg, sizeof(s_msg), "ten file khong hop le: %s", bad);
        HLOG("[HTTP] TU CHOI /upload: ten \"%s\" -> \"%s\": %s\n", nm, nm_ok, bad);
        s_file[0] = '\0';
        return;                                    // moi goi WRITE sau do bi bo qua
      }
      if (strcmp(nm, nm_ok) != 0)
        HLOG("[HTTP] CANH BAO bao mat: ten khach gui \"%s\" -> lam sach thanh \"%s\"\n", nm, nm_ok);
      snprintf(s_file, sizeof(s_file), "%s", nm_ok);
      HLOG("[HTTP] PHAN FILE #%u: \"%s\" tu %s (khach khai bao: %lu B)\n",
           (unsigned)s_parts, s_file, s_client, (unsigned long)s_declared);
      if (s_parts == 1) {
        HLOG("[HTTP] PHAN FILE DAU TIEN cua request nay = PHIEN MOI (mo file tam dang GHI, khong append)\n");
      } else {
        HLOG("[HTTP] PHAN FILE #%u cua CUNG request -> ghi TIEP vao chinh file tam do (FILE_APPEND)\n",
             (unsigned)s_parts);
      }
      s_http_state = HS_RECV;
      s_last_activity_ms = millis();
      snprintf(s_msg, sizeof(s_msg), "dang nhan file");
      // `new_sess`: day co phai PHAN DAU cua mot REQUEST MOI khong. Truyen THANG xuong cyd_sd_temp_open()
      // thay vi de no tu doan theo `part` (xem ghi chu o cyd_sd_temp_open - append nham lam HONG file
      // trung ten).
      if (!cyd_sd_temp_open(nm_ok, new_sess ? s_declared : 0, s_parts, new_sess)) {
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
    default:                                       // UPLOAD_FILE_ABORTED (core: Parsing.cpp:549) + khac
      // 2026-10-16: nhanh nay DA co tu truoc va no chinh la thu don sach khi client ngat giua duong
      // (dong tab / WiFi rot). Thieu no thi `s_sess_busy` ket o true -> request sau bi coi la "part 2,3"
      // -> cyd_sd_temp_open() se GHI TIEP vao file tam cu => file trung ten bi append, HONG FILE.
      cyd_sd_temp_abort();
      s_http_state = HS_ERR;
      s_sess_busy = false;
      s_parts = 0;                                 // don luon bo dem part (phong khi request sau khong reset)
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
  // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): Marlin dang doc/ghi the thi MAN HINH KHONG quet the ->
  // tra ve trang thai BUSY RO RANG (khac voi loi that) de trang web bao "thu lai sau" chu KHONG hien
  // danh sach rong (de bi hieu la the het file).
  if (cyd_sd_yield_to_marlin()) {
    HLOG("[HTTP] GET /files -> BAN: Marlin dang doc/ghi the SD (tra loi 'thu lai sau')\n");
    send_json(200, "{\"status\":\"Busy\",\"sd\":1,\"files\":[],\"count\":0,"
                   "\"message\":\"Marlin dang doc/ghi the SD - thu lai sau\"}");
    return;
  }
  js += ",\"files\":[";
  char nm[64];
  uint8_t n = 0;
  // DUYET MOT LUOT (2026-10-16). Truoc day vong lap goi cyd_sd_root_entry(i, ...) cho tung i, ma ham
  // do MO LAI thu muc roi dem tu dau toi i -> O(N^2). Do tren board that: 7 file 0,18 s | 22 file
  // 1,89 s | 27 file 3,11 s. Nay 1 lan duyet -> O(N).
  // KHONG con truong "s" (size file): trang web khong can, va phia Marlin cung da gui size = 0
  // (`tx_i32(0); // ExtUI khong cung cap`). Bo size cung la de dung getNextFileName() = 1 readdir,
  // khong mo file/khong stat tung muc.
  if (cyd_sd_list_begin()) {
    bool isdir = false;
    while (n < 64 && cyd_sd_list_next(nm, sizeof(nm), &isdir)) {
      if (n) js += ',';
      js += "{\"n\":\"";
      for (const char *p = nm; *p; p++) {          // escape JSON (" va \)
        if (*p == '"' || *p == '\\') js += '\\';
        js += *p;
      }
      js += "\",\"d\":";
      js += (isdir ? '1' : '0');
      js += '}';
      n++;
    }
    cyd_sd_list_end();
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
  // ---------------------------------------------------------------------------
  // BAO MAT TEN FILE (yeu cau nguoi dung 2026-10-15): ten file den tu client la du lieu KHONG dang tin.
  // Whitelist o `cyd_sd_name_check()`: chi a-z A-Z 0-9 '.' '_' (+ CYDLCD_NAME_ALLOW_EXTRA, mac dinh '-'),
  // khong rong, khong > 64 ky tu, khong bat dau bang '.', khong chua "..". Xem cyd_sd.cpp/cydlcd_config.h.
  // ---------------------------------------------------------------------------
  const char *bad = cyd_sd_name_check(nm);
  if (bad) {
    snprintf(js, sizeof(js), "{\"status\":\"Error\",\"message\":\"ten file khong hop le: %s\"}", bad);
    HLOG("[HTTP] TU CHOI /delete \"%s\": %s\n", nm, bad);
    send_json(400, js);
    return;
  }
  if (!cyd_sd_ready()) {
    send_json(503, "{\"status\":\"Error\",\"message\":\"chua co the TF\"}");
    return;
  }
  // UU TIEN MARLIN: Marlin dang doc/ghi the -> TRA LOI RO "BAN" (khac loi that) de trang web bao
  // "thu lai sau" (thao tac that bai thi cu thu lai, khong phai loi - y kien nguoi dung 2026-10-15).
  if (cyd_sd_yield_to_marlin()) {
    HLOG("[HTTP] HOAN /delete \"%s\": Marlin dang doc/ghi the SD -> thu lai sau\n", nm);
    send_json(409, "{\"status\":\"Busy\",\"message\":\"Marlin dang doc/ghi the SD - thu lai sau\"}");
    return;
  }
  const int cnt = cyd_sd_dir_count(nm);      // >=0: THU MUC (so muc con); -1: khong phai thu muc; -2: BAN
  if (cnt == -2) {                           // BAN vi Marlin dang doc/ghi the (khac loi that) -> thu lai sau
    HLOG("[HTTP] HOAN /delete \"%s\": Marlin dang doc/ghi the SD -> thu lai sau\n", nm);
    send_json(409, "{\"status\":\"Busy\",\"message\":\"Marlin dang doc/ghi the SD - thu lai sau\"}");
    return;
  }
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
//  /screen.bmp — CHUP MAN HINH (tra ve anh BMP 24-bit 240x320)
//
//  VI SAO CO: man hinh CYD khong co framebuffer (driver ve thang ra SPI) nen KHONG chup duoc tu PC,
//  va 240x320x2 = 150 KB thi khong du RAM giu mot ban sao. Truoc day moi lan kiem tra mau/bo cuc deu
//  phai nho nguoi ngoi nhin man hinh — trong khi vu loi mau panel (2026-10-16) phai doi chieu nhieu
//  lan. API nay doc NGUOC RAM cua panel qua ST7789_RAMRD theo tung dai 16 dong roi tra ve 1 file .bmp:
//      curl -o shot.bmp http://<ip>/screen.bmp
//  la xem duoc, ke ca khi ngoi tu xa.
//
//  Anh = 54 + 320*240*3 = 230.454 byte. HE TOA DO LA LANDSCAPE 320x240 (ui_port_cyd_st7789.cpp goi
//  setRotation(1)) — doc nham 240x320 se chi lay 240 cot dau va tran chieu cao. BMP ghi DAY-LEN
//  (height duong) nen phai doc dai TU DAY man hinh tro len, trong moi dai ghi dong tu duoi len.
//  320*3 = 960 chia het cho 4 -> khong can pad.
//
//  Viec doc panel phai do task `ui` lam (moi truy cap TFT_eSPI phai nam trong MOT task); o day chi
//  goi ui_shot_band() roi DOI SEMAPHORE, nen task `ui` van chay binh thuong trong luc cho.
// ===========================================================================
#define SHOT_W 320
#define SHOT_H 240
#define SHOT_BAND 8

void handle_screen(void) {
  // 1 lan cap phat: [dai RGB888][1 dong BGR] — tranh ca .bss (DRAM ESP32 da sat) lan stack cua
  // task `net` (6 KB).
  uint8_t *band = (uint8_t *)malloc((size_t)SHOT_W * SHOT_BAND * 3 + SHOT_W * 3);
  if (!band) { server.send(500, "text/plain", "het RAM"); return; }
  uint8_t *row = band + (size_t)SHOT_W * SHOT_BAND * 3;

  const uint32_t rowbytes = SHOT_W * 3;
  const uint32_t imgsize  = rowbytes * SHOT_H;
  const uint32_t filesize = 54 + imgsize;

  uint8_t hdr[54];
  memset(hdr, 0, sizeof(hdr));
  hdr[0] = 'B'; hdr[1] = 'M';
  hdr[2]  = (uint8_t)(filesize);        hdr[3]  = (uint8_t)(filesize >> 8);
  hdr[4]  = (uint8_t)(filesize >> 16);  hdr[5]  = (uint8_t)(filesize >> 24);
  hdr[10] = 54;                                    // offset to pixel data
  hdr[14] = 40;                                    // DIB header size
  hdr[18] = (uint8_t)(SHOT_W);          hdr[19] = (uint8_t)(SHOT_W >> 8);
  hdr[22] = (uint8_t)(SHOT_H);          hdr[23] = (uint8_t)(SHOT_H >> 8);   // duong = bottom-up
  hdr[26] = 1;                                     // planes
  hdr[28] = 24;                                    // bits/pixel
  hdr[34] = (uint8_t)(imgsize);         hdr[35] = (uint8_t)(imgsize >> 8);
  hdr[36] = (uint8_t)(imgsize >> 16);   hdr[37] = (uint8_t)(imgsize >> 24);

  server.setContentLength(filesize);
  server.send(200, "image/bmp", "");
  server.sendContent((const char *)hdr, sizeof(hdr));

  bool ok = true;
  for (int y = SHOT_H - SHOT_BAND; y >= 0 && ok; y -= SHOT_BAND) {
    ok = ui_shot_band(y, SHOT_BAND, SHOT_W, band, 3000);
    if (!ok) { Serial.printf("[SHOT] chup dai y=%d that bai\n", y); break; }
    for (int r = SHOT_BAND - 1; r >= 0; r--) {     // BMP bottom-up: ghi dong duoi truoc
      const uint8_t *src = band + (size_t)r * SHOT_W * 3;
      for (int x = 0; x < SHOT_W; x++) {
        row[3 * x + 0] = src[3 * x + 2];           // B
        row[3 * x + 1] = src[3 * x + 1];           // G
        row[3 * x + 2] = src[3 * x + 0];           // R
      }
      server.sendContent((const char *)row, rowbytes);
    }
  }
  free(band);
  Serial.printf("[SHOT] %s (240x320)\n", ok ? "da tra anh" : "tra anh THAT BAI");
}

// ===========================================================================
// API
// ===========================================================================
void net_upload_http_begin(void) {
  // LUU Y: net_wifi_begin() do main.cpp goi (can chay ca khi khong bat web server, vi duong BT
  // cung dung net_wifi). O day chi lo the TF + WebServer + mDNS.
  cyd_sd_begin();                   // mount the TF + don file tam mo coi
  server.on("/", HTTP_GET, handle_root);
  server.on("/status", HTTP_GET, handle_status);
  server.on("/screen.bmp", HTTP_GET, handle_screen);   // chup man hinh (doc nguoc tu panel)
  server.on("/upload", HTTP_POST, handle_upload_done, handle_upload_stream);
  server.on("/files", HTTP_GET, handle_files);
  server.on("/delete", HTTP_POST, handle_delete);
  server.onNotFound(handle_not_found);
  server.begin();
  // TAT "delay khi khong co request" cua WebServer ESP32 (mac dinh BAT): `WebServer::handleClient()`
  // (libraries/WebServer/src/WebServer.cpp:275-283) khi chua co client thi goi `delay(1)` ROI MOI tra ve
  // => vong lap man hinh bi an 1 ms MOI lan goi (do 2026-10-15 bang `UI PERF`: `handleClient` 1,5 ms/lan,
  // ~4,5 s trong 9 s = ~50% CPU). Voi vong lap chay ~300 lan/s thi day la 1/3 thoi gian VO ICH, va no lam
  // man hinh + link Marlin phan hoi cham. Tat di KHONG anh huong phien upload (WebServer van xu ly CA body
  // trong 1 lan goi nhu cu), chi bo khoang nghi 1 ms khi KHONG co ai ket noi.
  server.enableDelay(false);
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

// ---- DO HIEU NANG (`UI PERF`): tach thoi gian tung phan cua chang nay ----
// VI SAO CAN: chang `http` do duoc 2087 ms/vong lap (dong `[CYD] chang: ... http=2087 ms`) => vong lap
// chay 0.5 Hz => MAN HINH CHAM KHONG PHAI DO VE (ui chi 3-5 ms) MA DO CHO NAY. Phai biet cho nao:
//   net_wifi_task() (khong chan), cyd_sd_task() (the TF), hay server.handleClient() (WebServer cua ESP32
//   co the CHO du lieu toi HTTP_MAX_DATA_WAIT khi co client mo ket noi nua voi).
static uint32_t s_pf_wifi_us = 0, s_pf_sd_us = 0, s_pf_srv_us = 0, s_pf_srv_max_us = 0, s_pf_srv_skip = 0;

static inline uint32_t pf_t0(void) { return micros(); }
static inline void pf_add(uint32_t *dst, const uint32_t t0) { *dst += micros() - t0; }

void net_upload_http_task(void) {
  if (!s_started) return;
  uint32_t t = pf_t0();
  net_wifi_task();                     // cap nhat trang thai + tu ket noi lai (khong chan)
  pf_add(&s_pf_wifi_us, t);
  t = pf_t0();
  cyd_sd_task();                    // theo doi the TF nong
  pf_add(&s_pf_sd_us, t);
  t = pf_t0();
  // `server.handleClient()` KHONG con ton 1 ms (da `enableDelay(false)`) nhung van ton ~50 us MOI LAN GOI
  // (do 2026-10-15: 2,7 s trong 10 s = 27% CPU khi vong lap chay ~5000 lan/s) vi no phai hoi lwIP xem co
  // ket noi moi. Trang web cua man hinh chi duoc bam/poll vai lan moi giay nen goi moi CYDLCD_HTTP_POLL_MS
  // la du; phien upload KHONG bi anh huong vi WebServer xu ly CA body trong 1 lan goi (xem ghi chu o tren).
  {
    static uint32_t s_poll_ms = 0;
    const uint32_t nms = millis();
    if (!s_poll_ms || (uint32_t)(nms - s_poll_ms) >= CYDLCD_HTTP_POLL_MS) {
      s_poll_ms = nms;
      t = pf_t0();
      server.handleClient();               // nhan request
      const uint32_t dsrv = micros() - t;
      s_pf_srv_us += dsrv;
      if (dsrv > s_pf_srv_max_us) s_pf_srv_max_us = dsrv;
    }
    else s_pf_srv_skip++;
  }

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

// In + XOA bo dem thoi gian tung phan cua chang `http` (lenh `UI PERF`, xem net_upload_http.h).
void net_upload_http_perf_report(void) {
  Serial.printf("[PERF] http: wifi %lu us | sd_the %lu us | handleClient %lu us (max %lu us, bo qua %lu lan)\n",
                (unsigned long)s_pf_wifi_us, (unsigned long)s_pf_sd_us,
                (unsigned long)s_pf_srv_us, (unsigned long)s_pf_srv_max_us, (unsigned long)s_pf_srv_skip);
  s_pf_wifi_us = s_pf_sd_us = s_pf_srv_us = s_pf_srv_max_us = s_pf_srv_skip = 0;
}

bool net_upload_http_active(void) { return s_started; }

// true = dang co phien nhan/ghi file (UI dung de chan cac lenh lam Marlin doc the SD)
bool net_upload_active(void) { return s_http_state == HS_RECV || s_http_state == HS_WRITE; }

uint32_t net_upload_last_activity_ms(void) { return s_last_activity_ms; }

// ===========================================================================
// THONG BAO cho trang /status (xem net_upload_http.h)
// ===========================================================================
void net_notify_set(const char *msg, const uint8_t sev, const uint32_t ttl_ms) {
  if (!msg || !msg[0]) { net_notify_clear(); return; }
  // Cat bot neu qua dai + doi ky tu xuong dong thanh ' ' (de JSON/log gon, khong pha 1 dong log).
  size_t o = 0;
  for (size_t i = 0; msg[i] && o + 1u < sizeof(s_notify); i++) {
    char c = msg[i];
    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    if (c == '"' || c == '\\') c = '\'';            // an toan JSON (khong phai escape)
    s_notify[o++] = c;
  }
  s_notify[o] = '\0';
  s_notify_sev = sev;
  s_notify_set_ms = millis();
  s_notify_ttl_ms = ttl_ms;                          // 0 = giu mai (khong co thoi gian)
  s_notify_seq++;
  HLOG("[HTTP] thong bao (chi o /status, khong hien LCD): sev=%u ttl=%lu ms \"%s\"\n",
       (unsigned)sev, (unsigned long)ttl_ms, s_notify);
}

void net_notify_clear(void) {
  if (!s_notify[0]) return;
  HLOG("[HTTP] xoa thong bao: \"%s\"\n", s_notify);
  s_notify[0] = '\0';
  s_notify_ttl_ms = 0;
}

void net_notify_dump(void) {
  notify_expire();
  if (!s_notify[0]) { Serial.println("[HTTP] khong co thong bao nao dang luu"); return; }
  const uint32_t left = s_notify_ttl_ms
      ? (s_notify_ttl_ms > (millis() - s_notify_set_ms) ? s_notify_ttl_ms - (millis() - s_notify_set_ms) : 0)
      : 0;
  Serial.printf("[HTTP] thong bao: sev=%u \"%s\" (ttl=%lu ms, con %lu ms, lan %lu)\n",
                (unsigned)s_notify_sev, s_notify, (unsigned long)s_notify_ttl_ms,
                (unsigned long)left, (unsigned long)s_notify_seq);
}

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
void net_notify_set(const char *msg, uint8_t sev, uint32_t ttl_ms) {
  (void)msg; (void)sev; (void)ttl_ms;
}
void net_notify_clear(void) {}
void net_notify_dump(void) {}

#endif  // CYDLCD_WIFI_UPLOAD
