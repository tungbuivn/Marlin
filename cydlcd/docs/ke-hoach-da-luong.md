# Phương án ĐA LUỒNG cho panel CYD (ESP32) — **ĐỂ XEM TRƯỚC, CHƯA THỰC HIỆN**

Mục tiêu người dùng (chốt lại): **chỉ cần việc phục vụ serial / tương tác với Marlin KHÔNG BỊ GIÁN ĐOẠN** —
các chỗ khác (nhịp UI, upload có chặn hay không, log, console…) thế nào cũng được.
Người dùng đã duyệt (2026-10-15) với 2 chỉnh sửa: **UI chạy 30 Hz** (cho mượt) và **KHÔNG cần làm phần "upload
trong lúc Marlin đọc sector"** (đọc/ghi xen kẽ) — vì 2 việc đó hiếm khi xảy ra cùng lúc và không cần cho lúc in.

Tài liệu này là **phương án** kèm số đo thật + ràng buộc đã kiểm tra trong code. Chưa sửa dòng code nào cho đa
luồng. Duyệt xong mới làm, theo từng bước nạp được và kiểm tra được (§5).

---

## 1. Hiện trạng: vì sao cần đa luồng

Toàn bộ panel hiện chạy trong **MỘT vòng lặp** `loop()` (Arduino `loopTask`, core 1). Mỗi vòng làm hết: log 1 Hz,
chuyển lệnh console, **UART Marlin (RX/TX + phục vụ sector SD cho Marlin)**, WiFi/WebServer (upload 20–30 s), thẻ TF
(mount 2 s), LVGL (vẽ + flush SPI tới 76 ms/khung), đọc console.

Đo được (README mục *"Vì sao màn hình chạy chậm"*):

| Việc trong vòng lặp | Chi phí | Hậu quả lên ĐƯỜNG SERIAL (điều duy nhất cần bảo vệ) |
|---|---|---|
| `server.handleClient()` đang upload | **20–30 s**/file 4,3 MB | serial **chết hẳn**: không PONG, `alive=0`, `refused rd/wr` tăng, frame REQUEST bị **bỏ** |
| `cyd_sd_task()` khi thẻ không mount | **~2 s**/lần thử | vòng lặp 0,5 Hz ⇒ Marlin chờ sector quá 1000 ms ⇒ "SD read error" |
| `lv_timer_handler()` (vẽ + flush) | tới **17–76 ms** | frame Marlin trễ; sector phải chờ hết khung vẽ |
| log 1 Hz trên UART0 | **15–20 ms/s** | chiếm UART, khựng nhịp đọc/đáp |
| `sd_mount()` / `clean_dir()` | 20 ms – 2 s | chặn luôn cả phục vụ sector |

⇒ Trong 1 luồng **không thể** bảo đảm "serial không bị gián đoạn": chỉ cần `handleClient()` vào phiên upload là chết.

---

## 2. Ràng buộc kỹ thuật (đã kiểm tra trong code)

1. **LVGL KHÔNG thread-safe** ⇒ **chỉ DUY NHẤT 1 task được gọi `lv_*`**. Hiện `cyd_sd.cpp: ov_set()` (dòng
   271/305/310/1280/1285/1328/1356) đang gọi LVGL **từ đường upload** ⇒ bắt buộc đổi thành "gửi thông điệp cho task UI".
2. **Hai bus SPI ĐỘC LẬP**: LCD ở **HSPI** (TFT_eSPI `USE_HSPI_PORT`, chân 13/12/14/15/2), thẻ SD ở **VSPI**
   (SCK 18 / MISO 19 / MOSI 23 / CS 5 @20 MHz — `cydlcd_config.h:453-457`) ⇒ vẽ và đọc thẻ **song song được thật**.
3. **Thẻ SD (FATFS + sector thô) KHÔNG re-entrant** ⇒ phải có mutex; các cờ `s_fatfs_stale`/`s_fatfs_written` phải
   được đọc/ghi trong vùng bảo vệ đó.
4. **Ngân sách của Marlin**: `CYDLCD_SD_TIMEOUT_MS = 1000 ms` cho 1 sector 512 B
   (`Marlin/src/lcd/extui/cydlcd_serial/cydlcd_serial.h:65`, bình thường ~25 ms). Panel trả lời 540 B ≈ 22 ms
   @250 kbaud ⇒ **dư rất nhiều**, miễn link task không bị giữ > ~300 ms.
5. **`Serial` (COM8) = UART0 @115200**: in 500 B mất ~43 ms ⇒ không task nào được in trực tiếp; phải qua task `log`.
6. **NVS / ghi flash**: mỗi lần commit NVS (lưu WiFi, lưu preset preheat) **tắt cache flash ⇒ CẢ HAI CORE đứng
   ~10–100 ms**. Vẫn trong ngân sách 1000 ms nhưng phải biết mà đo (xem §6).
7. Đã có: task chẩn đoán 1 Hz trên core 0 (stack 4096) + watchdog treo 15 s theo `s_stage` (sẽ đổi sang heartbeat).
8. Free heap ~**160 KB** ⇒ thừa cho ~28 KB stack của các task mới.

---

## 3. Phương án: tách task, và **core 1 chỉ dành cho serial**

### 3.1 Bảng task

| Task | Core | Prio | Stack | Sở hữu (chỉ task này chạm) | Được phép block ở đâu | TUYỆT ĐỐI KHÔNG |
|---|---|---|---|---|---|---|
| **`link`** | **1 (một mình)** | **20** | 4 KB | `CYDLCD_MARLIN_SERIAL` (RX parse, TX queue, PING/resync), **phục vụ sector SD thô cho Marlin**, byte-sniff TBT | chờ mutex thẻ **≤300 ms** (trần, hết thì trả `ERR`), chờ TX ring (có trần) | gọi `lv_*`, `Serial.print`, WiFi, FATFS |
| **`ui`** | 0 (PRO) | 5 | 8–12 KB | **TOÀN BỘ LVGL** + TFT_eSPI + cảm ứng + hiệu chuẩn + overlay upload | vẽ/flush, hiệu chuẩn cảm ứng | chạm UART/thẻ/WiFi (chỉ đọc snapshot) |
| **`net`** | 0 (PRO) | 4 | 8 KB | WiFi + WebServer (**upload 20–30 s**) + mount/dọn thẻ + FATFS cho web + mDNS | `SD.begin()` (2 s), upload | gọi `lv_*` (phải post thông điệp), chạm UART Marlin |
| **`log`** | 0 (PRO) | 2 | 3 KB | `Serial` (UART0) + parser console COM8 | ghi UART | thực thi hành động UI (chỉ post queue) |
| `diag` (đã có) | 0 | 1 | 4 KB | heartbeat từng task, watchdog, `esp_restart()` | `vTaskDelay` | — |

**Vì sao `link` ở core 1 MỘT MÌNH** (đây là thay đổi so với bản trước): mục tiêu duy nhất là serial không bị gián
đoạn, nên để **không có gì khác chạy trên core 1** ⇒ kể cả UI vẽ 76 ms hay WiFi bận, link **vẫn chạy** — không phụ
thuộc vào việc "code kia có nhường đúng không". `loopTask` của Arduino (mặc định core 1) sẽ được cho **kết thúc**
(`setup()` tạo task rồi `vTaskDelete(NULL)`), hoặc ghim sang core 0.
Đánh đổi: UI/net/log dùng chung core 0 với WiFi driver ⇒ UI vẫn chạy (driver WiFi phần lớn ở trạng thái chờ, và UI
ưu tiên cao hơn `net`), chỉ là **chậm hơn chút khi đang upload** — chấp nhận được vì "các chỗ khác thế nào cũng được".

### 3.2 Luật chia sẻ dữ liệu (IPC)

| Dữ liệu | Ai ghi | Ai đọc | Cách chia sẻ | Chống chặn |
|---|---|---|---|---|
| Trạng thái Marlin (`cydlcd`, `tbt`, `pos`, `temps`…) | `link` | `ui`, `net`, `log` | `link` **ghi thẳng** vào buffer (không khoá, không cấp phát); `ui` **pull 10 lần/s**: `memcpy` ra bản riêng rồi vẽ. **Seqlock** (đếm `seq`, lẻ = đang ghi): nếu `link` chen ngang giữa lúc copy thì `ui` copy lại | `link` **không chờ ai, không tốn thêm bước nào**; `ui` là bên retry (~10–20 µs, rất hiếm) |
| Lệnh gửi Marlin (G-code, REQUEST, PONG, TBT cmd) | `ui`, `log`, `net` | `link` | `xQueueSend(link_q, {loại, tham số}, 0)` | không chờ; 16 slot, đầy thì đếm + log |
| Hành động UI (đổi trang, overlay upload, thông báo) | `net`, `log`, `link` | `ui` | `xQueueSend(ui_q, {hành động, chuỗi}, 0)` | không chờ; `ui` thức ngay khi có phần tử |
| Thẻ SD (FATFS + sector thô) | `net`, `link` | — | **1 mutex `card_mtx`**, luật "1 thao tác/lần giữ" (đơn giản, **không cần thiết kế đọc/ghi xen kẽ** — người dùng chốt: upload và Marlin đọc sector hiếm khi cùng lúc, và `/upload` vốn đã **bị từ chối khi đang in**) | đây là **chỗ duy nhất** `link` có thể phải chờ ⇒ trần 300 ms << 1000 ms của Marlin.<br>Trường hợp CÓ thể trùng thật: `sd_alive()` 1 Hz / `clean_dir()` 30 phút (task `net`) chạy **trong lúc Marlin đang in từ thẻ** ⇒ mutex lo phần này | |
| Chuỗi log | mọi task | `log` | `xQueueSend(log_q, "≤64 B", 0)` (8 slot, đầy thì bỏ) | không bao giờ chặn |
| Tick LVGL | — | `ui` | `lv_tick_inc()` trong `ui`, mỗi lần thức | — |

Nguyên tắc vàng: **3 thứ không bao giờ qua ranh giới task** — con trỏ widget LVGL, đối tượng UART, thao tác thẻ chưa
lấy mutex.

### 3.3 Nhịp task `ui` = **30 lần/s** (theo chốt của người dùng: "để 30 Hz cho mượt")

| Tình huống | Nhịp |
|---|---|
| Bình thường | **33 ms (30 lần/s)** |
| Có thông điệp trong `ui_q` (overlay/thông báo/đổi trang) | thức **ngay** |
| Vừa chạm xuống | thức **ngay** — **ngắt `TOUCH_IRQ` (GPIO36)** → `xTaskNotifyGive` |

Ngủ bằng `ulTaskNotifyTake(pdTRUE, 33 ms)` (không spin). `LV_DISP_DEF_REFR_PERIOD` **giữ 30 ms** (khớp nhịp 33 ms),
`LV_INDEV_DEF_READ_PERIOD` **giữ 20 ms**. Vì đã có helper "chỉ ghi khi giá trị đổi" (commit `b50ac82318`), mỗi lần
thức mà không có gì đổi gần như **0 chi phí**. Ở 30 Hz, một khung vẽ toàn màn (~36 ms SPI) là ngoại lệ hiếm (chỉ khi
đổi trang), không phải nhịp thường.

### 3.4 Ba loại dữ liệu, ba luật (trả lời "vậy vẫn phải ghi buffer à?")

1. **Trả lời NGAY, không buffer, không qua UI** (nằm trọn trong `link`): sector SD (`SDREQ` → 4 frame `SDDATA`),
   `PING`→`PONG`, TX G-code/REQUEST, trả lời prompt ⇒ **đường sống-còn không đi qua buffer/UI**.
2. **Giá trị mới nhất (ghi đè)** — buffer chính là struct `cydlcd` hiện có (nhiệt độ, vị trí, feed/flow, trạng thái,
   tiến độ, TBT, mesh 7×7×i16, 58 dòng setting, ~30 dòng file). Ghi = vài phép gán/`memcpy` **vài µs**, không cấp
   phát, không queue dài; tần suất frame chỉ ~20–30 lần/giây ⇒ không phình RAM, không trễ dần.
3. **Sự kiện KHÔNG được mất → queue nhỏ, không gộp**: prompt Marlin (M0/M1/đổi nhựa), thông báo (`notify_seq`),
   hành động UI. Mỗi queue ≤8 phần tử.

"Ghi buffer" **là 1 lần xử lý**, nhưng ở task `link`, chỉ vài µs và **không chờ ai** — không phải render. Bỏ buffer thì
chỉ còn 2 cách tệ hơn: vẽ ngay theo từng frame (kiểu 1 luồng hiện tại) hoặc mất dữ liệu.

---

## 4. Việc phải sửa kèm (bắt buộc, nếu không sẽ hỏng)

1. **Overlay upload** (`cyd_sd.cpp: ov_set/ov_note/ov_label`): → `ui_post_overlay(state, info, ok)`; `net` gửi thông
   điệp, `ui` mới chạm widget.
2. **Hiệu chuẩn cảm ứng** (`ui_port_touch_calibrate()`): chạy trong `ui` (block 60 s cũng không sao) — hiện đang chặn
   cả vòng lặp ⇒ hiện tại hiệu chuẩn làm **serial chết**.
3. **Console COM8**: parser sang `log`; lệnh UI (`UI BOX`, `UI MESH`…) phải **post** cho `ui`, không `show_page()`
   từ task khác.
4. **Watchdog**: `s_stage`/`s_hung_s` → **heartbeat từng task** (`link` trễ >1 s ⇒ báo động; `ui` >5 s; `net` miễn trừ
   khi upload **có tiến triển** — giữ đúng luật hiện tại), vẫn `esp_restart()` khi treo thật.
5. **Không in/log trong `link`**: mọi `SDLOG/ELOG/WIFILOG` trong `cyd_sd.cpp`, `cydlcd_link.cpp`, `net_upload_http.cpp`
   → `log_post()`; `Serial` chỉ còn ở task `log`.
6. **Ghi NVS** (`preferences.put*`, lưu preset/WiFi): chỉ gọi từ `net`/`log` (không bao giờ từ `link`), và tránh lúc
   đang phục vụ sector — vì commit NVS tắt cache flash, **cả 2 core** đứng 10–100 ms.
7. **6 chỗ GHI `cydlcd` từ ngoài task `link`** (đã rà: `cyd_sd.cpp:407-408`, `ui_page_files.cpp:424-425` và
   `:543-544` — đều là `files_done = false; n_files = 0;` để xin lại danh sách file): phải đổi thành **lệnh gửi cho
   `link`** (`link_post_reset_files()`), vì từ B1 trở đi `link` là **chủ sở hữu duy nhất** của struct trạng thái.
   (Toàn bộ chỗ đọc thì không phải sửa: `cydlcd` vẫn là bản công khai cho người đọc — xem §3.2.)

---

## 5. Lộ trình 5 bước — ưu tiên đúng mục tiêu (link trước, rồi thứ hay chặn link)

| Bước | Việc | Cách kiểm tra (không cần đốt nóng/di chuyển) |
|---|---|---|
| **B0** | Dựng "ranh giới" (`state_read()` seqlock, `link_post_*()`, `ui_post_*()`, `card_lock/unlock()`, `log_post()`) — **vẫn 1 luồng** — **+ đồng hồ đo độ trễ của link** (`max gap` giữa 2 lần link chạy, số lần trễ > X ms) | `UI RX`, `UI PERF`, `UI BOX`, `M220 S42` → Marlin `FR:43%`; **ghi lại `max gap` làm mốc "trước khi tách"** |
| **B1** | Tách **`link` một mình trên core 1** (prio 20) + UART `onReceive` notify + toàn bộ log qua queue; `loopTask` kết thúc | ✅ **XONG 2026-10-15** — xem §5.1 (số đo ngay dưới) |
| **B2** | Tách **`net`** (core 0, prio 4) + `card_mtx` (1 thao tác/lần giữ) + đổi 6 chỗ ghi `cydlcd` ngoài `link` thành lệnh + overlay upload qua thông điệp | upload 4 MB ⇒ `PING` **không sót frame**, `max gap` vẫn nhỏ, UI vẫn vẽ, `UI RX` trả lời được; (không cần thử chồng với Marlin đọc sector — người dùng chốt là không cần) |
| **B3** | Tách **`ui`** (core 0, prio 5, 10 lần/s + ngắt touch) + bỏ LVGL khỏi `loop()` | `UI PERF` → ~10 lần thức/s; `UI STALL 30` (chỉ chặn task `net`) ⇒ PING/PONG vẫn đều; hiệu chuẩn cảm ứng không làm link chết |
| **B4** | Tách **`log`**/console + heartbeat từng task + xoá `s_stage`; đo lại, cập nhật README | bắn lệnh COM8 liên tục trong lúc upload + vẽ ⇒ `PING` vẫn đủ, `max gap` vẫn nhỏ |

Mỗi bước: nạp panel → chạy bộ kiểm tra ở cột phải → commit riêng (revert được). **Không cần đổi gì phía Marlin**
(`cydlcd_protocol.h` giữ nguyên).

### 5.1 B1 — KẾT QUẢ ĐO ĐƯỢC (2026-10-15, commit `2e60e5b774`)

| Phép thử | Kết quả |
|---|---|
| Độ trễ task `link` — **trước** (1 luồng) | `gap max 63 549 µs`, **1 lần > 50 ms** mỗi ~10 s (do log 700 B @115200 in ngay trong đường link) |
| Độ trễ task `link` — **sau** (rảnh) | `gap max 6 172 µs`, **0 lần > 50 ms** |
| Độ trễ task `link` — **trong lúc upload 4,3 MB qua WiFi** | `gap max 4 910 µs` (lần đầu sau boot 23 950 µs), **0 lần > 50 ms** |
| **Marlin nhận PING trong lúc upload** (COM4 `PANEL DIAG`) | `PING` 128 → 133 → 138 → 143 → 148 → 153 → 158 — **đúng +5 mỗi 5 s = 1/s, không sót frame nào**; `err=0`; `alive=1`; `hoan=8 bo=0` |
| Upload thật (2 lần, 4,31 MB và 4,31 MB) | `143 KB/s`, `state=done`, `written=4523980/4523824`; panel ghi thẻ rồi gửi `M22`/`M21` → Marlin `echo:SD card ok` |
| Marlin đọc lại thẻ sau upload (`M20`) | `V300_B~1.GCO 4523980` + `V300_B~2.GCO 4523824` (⇒ đường SD-qua-UART vẫn nguyên vẹn) |
| Lệnh từ panel → Marlin | `M220 S42` gửi từ panel ⇒ Marlin `FR:42%` (đã trả lại `M220 S100`) |
| Màn hình | `hello=0x9F`, `PONG cuoi 752 ms`, `mat PONG 0`, `SETTING=101`, `MESH=14 (7x7)`, `FILES=13` |
| UI | `ui_lvgl_task` tb 17 µs, vẫn 30 Hz; console COM8 **không trả lời trong lúc upload** (task `loop`/core 0 bận xử lý body — chấp nhận được, đúng ưu tiên "chỉ cần serial không bị gián đoạn") |

⚠️ **Việc còn lại của B2 phải làm NGAY vì B1 đã tạo ra truy cập song song lên thẻ**: task `link` đọc/ghi sector thô
trên **core 1** trong khi `cyd_sd_task()` (`sd_alive()` 1 Hz, `clean_dir()`) và đường upload (FATFS) chạy trên
**core 0** — **chưa có `card_mtx`**. Lần thử vừa rồi may mắn không đụng nhau (`bo=0`, không lỗi), nhưng khi **đang in**
thì Marlin đọc sector liên tục ⇒ phải thêm `card_mtx` trước khi in.

---

## 6. Tiêu chí nghiệm thu (một câu, đo được)

> Trong **60 giây** chạy đồng thời: **upload 4 MB qua WiFi** + **UI mở/đóng trang liên tục** + log bật + console gõ
> lệnh (và, nếu đang in, Marlin đọc sector thẻ liên tục) — thì:
> * phía **Marlin** (`PANEL DIAG`, COM4): `PING` nhận **đủ ~60 frame** (không sót), `err=0`, `alive=1`;
> * phía **panel** (`UI RX`): tuổi PONG **< 1 s** liên tục, `mat PONG = 0`;
> * **`max gap`** giữa 2 lần task `link` chạy **< 50 ms** (bình thường < 5 ms);
> * khi Marlin đọc sector: `refused rd/wr = 0`.
> (Không yêu cầu thử chồng "upload ghi thẻ" với "Marlin đọc sector" — người dùng chốt là không cần.)

Số **`max gap`** là thứ biến "không bị gián đoạn" thành con số — nó được thêm ngay ở **B0** để có mốc so sánh trước/sau.
(Dự phòng: mốc duy nhất có thể làm link trễ là **commit NVS** — cache flash tắt, cả 2 core đứng 10–100 ms — nên §4.6
cấm ghi NVS từ `link` và hạn chế lúc đang phục vụ sector.)

**Không giải quyết được** (ngoài phạm vi): băng thông SPI màn hình (~36 ms/toàn màn là giới hạn phần cứng — đã tối ưu
để gần như không phải vẽ lại) và **lỗi thẻ TF hiện tại** (`f_mount failed (3)` — phần cứng/thẻ).

---

## 7. Rủi ro & cách chặn

| Rủi ro | Cách chặn |
|---|---|
| Gọi `lv_*` từ task khác (hỏng LVGL) | 1 chủ sở hữu + rà soát mọi lời gọi `lv_` ngoài `ui` (đã liệt kê §4.1–4.3); tuỳ chọn assert khi `-DCYDLCD_CYD_DEBUG` |
| Hai task cùng chạm thẻ ⇒ hỏng FAT | `card_mtx` + "1 thao tác/lần giữ"; mount/dọn **chỉ khi không có phiên upload** (giữ yêu cầu cũ *"không bao giờ mount lại sdcard"*) |
| `link` chờ mutex thẻ quá lâu | `net` nhả sau **từng khối 16 KB** (1 lệnh `SD.write` ~10–30 ms); `link` trần 300 ms ⇒ xấu nhất vẫn << 1000 ms |
| Commit NVS làm đứng cả 2 core | Không ghi NVS từ `link`; gom ghi NVS vào lúc rảnh (§4.6) |
| Mất byte UART khi link bị trễ | ring buffer RX **1024 B** (đã có) ≈ **41 ms** đệm @250 kbaud + `onReceive` đánh thức ngay |
| Đảo ưu tiên / đói UI | `link` tự `vTaskDelay(1)` khi rảnh; UI prio 5 > net 4; đo bằng heartbeat + `UI PERF` |
| Hết RAM / tràn stack | Kiểm tra `xTaskCreatePinnedToCore` trả về; heap còn ~160 KB; đo `uxTaskGetStackHighWaterMark` ở `diag` |
| Hồi quy chức năng (upload/in/mesh) | Làm 5 bước, mỗi bước nạp + kiểm tra + commit riêng |

---

## 8. Phương án thay thế (nêu để so)

Giữ 1 luồng nhưng **làm phi tắc nghẽn** (`handleClient()` nhận từng mẩu body, `sd_mount()` theo máy trạng thái, log
không chặn…). Ưu: ít rủi ro đồng bộ. Nhược: **vẫn không bảo đảm** serial không bị gián đoạn (vẫn phải chia thời gian
cho vẽ/flush SPI) và code phức tạp hơn tách task. Tôi **không** đề xuất.

---

## 9. Cần anh quyết

1. **Duyệt lộ trình 5 bước** (B0 → B1 link → B2 net → B3 ui → B4 log), mỗi bước nạp + kiểm tra + commit riêng?
2. **Core 1 chỉ dành cho `link`** (UI/net/log sang core 0) — đồng ý? Đây là điểm khác bản trước và là cách bảo đảm
   chắc nhất cho yêu cầu "serial không bị gián đoạn"; đánh đổi là UI hơi chậm hơn khi đang upload.
3. **Giữ nguyên** 250 kbaud (Marlin) và 20 MHz (thẻ SD) trong đợt này?
