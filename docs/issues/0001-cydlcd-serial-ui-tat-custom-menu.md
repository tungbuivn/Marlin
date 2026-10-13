# Issue nháp: `CUSTOM_MENU_MAIN` bị tắt khi bật `CYDLCD_SERIAL_UI` (cần cơ chế để thêm lại)

> **Trạng thái:** NHÁP — chưa tạo trên GitHub (máy này không có `gh` CLI, không có `GITHUB_TOKEN`,
> và PowerShell không ra được `api.github.com`). Cách tạo: xem §4 cuối file.
> **Repo:** `tungbuivn/Marlin` · **nhánh liên quan:** `mks-monster8-Voron-2.1.x`

---

## Tiêu đề (dùng nguyên văn khi tạo issue)

```
CYDLCD_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến
```

## Nhãn gợi ý

`enhancement` · `config` · `man hinh CYD`

---

## 1. Chuyện gì đã xảy ra

Để dùng màn hình riêng (LVGL) qua ExtUI `cydlcd_serial`, Marlin phải chạy ở "chế độ man hinh CYD":
`pwsh cydlcd/tools/set_panel_mode.ps1 -Mode on` đổi **5 dòng** config (script nằm trong repo):

| Dòng | File:line | Chế độ màn 12864 (mặc định) | Chế độ man hinh CYD | Vì sao |
|---|---|---|---|---|
| `CYDLCD_SERIAL_UI` | `Marlin/Configuration.h:3176` | tắt | **bật** | ExtUI `cydlcd_serial` đẩy frame dữ liệu xuống màn hình |
| `MKS_MINI_12864_V3` | `Marlin/Configuration.h:2950` | bật | **tắt** | ExtUI chiếm `MarlinUI::init_lcd()/update()` → vừa có màn gắn trực tiếp là lỗi `multiple definition of MarlinUI::init_lcd()` |
| `NEOPIXEL_LED` | `Marlin/Configuration.h:3449` | bật | **tắt** | PD8 (đèn nền RGB của màn V3) **trùng USART3_TX** dùng cho man hinh CYD |
| `CUSTOM_MENU_MAIN` | `Marlin/Configuration_adv.h:3792` | bật | **tắt** | ExtUI không dùng hệ thống menu của MarlinUI (Marlin báo lỗi SanityCheck `CUSTOM_MENU_MAIN requires an LCD controller that implements the menu`) |
| `LED_CONTROL_MENU` | `Marlin/Configuration_adv.h:1508` | bật | **tắt** | cùng lý do (và menu đèn LED cần NeoPixel) |

⇒ Đây là **thoả hiệp tạm thời**, không phải thiết kế mong muốn: chế độ man hinh CYD hiện **không có**
menu tuỳ biến của Marlin.

## 2. Đã bù bằng gì (và còn thiếu gì)

* 2 mục menu tuỳ biến hiện có đã được đưa sang **màn CUSTOM của man hinh CYD** (`ui_lvgl.cpp`), đúng nội dung cũ:
  * `MAIN_MENU_ITEM_1` — `"G34 Bed"` → `G34 Q99` (`Configuration_adv.h:3799-3800`)
  * `MAIN_MENU_ITEM_2` — `"Init SD card"` → `M22` + `M21` (`Configuration_adv.h:3804-3805`)
* **Còn thiếu / sẽ thiếu về sau:**
  1. Thêm mục menu tuỳ biến mới hiện phải **sửa code man hinh CYD** (`ui_lvgl.cpp`) rồi build lại — không
     còn là "sửa 1 dòng config như `MAIN_MENU_ITEM_n`" nữa.
  2. `LED_CONTROL_MENU` + `LED_COLOR_PRESETS` (đổi màu/độ sáng đèn) biến mất hoàn toàn ở chế độ man hinh CYD.
  3. Các menu khác của MarlinUI (`CUSTOM_MENU_CONFIG`, `CUSTOM_USER_MENUS`…) sẽ vướng cùng lỗi nếu bật.

## 3. Việc cần làm để "thêm lại được" (đề xuất)

**Cách A (khuyến nghị) — Marlin đẩy danh sách menu tuỳ biến xuống man hinh CYD:**

1. Thêm frame mới vào `panel_protocol.h` (dùng chung 2 phía), ví dụ:
   `PANEL_F_MENU = 0x0E` — payload: `u8 so_muc`, mỗi mục `str nhan`, `str gcode` (gửi nhiều frame
   nếu dài; frame kết thúc `so_muc = 0xFF`).
2. Trong `Marlin/src/lcd/extui/cydlcd_serial/cydlcd_serial.cpp`: đọc danh sách từ config mới
   (ví dụ `PANEL_MENU_ITEM_1_DESC/GCODE` … hoặc tái dùng chính `MAIN_MENU_ITEM_n_*` khi
   `CUSTOM_MENU_MAIN` tắt — cần `#if ENABLED(CYDLCD_SERIAL_UI)` cho phép define mà không cần menu),
   gửi khi `HELLO`/`REQUEST FULL`.
3. Panel (`ui_lvgl.cpp` màn CUSTOM): vẽ các mục nhận được thành nút, bấm → `panel_link_send_gcode()`.
4. Lợi: thêm mục mới chỉ cần sửa `Configuration_adv.h` + nạp lại Marlin, **không** phải build lại man hinh CYD.

**Cách B — bật lại `LED_CONTROL_MENU`:** chỉ hợp lý nếu quay lại dùng NeoPixel; khi đó phải chọn
chân LED khác (không dùng PD8 vì trùng USART3_TX của man hinh CYD).

**Cách C — quay lại màn 12864:** `set_panel_mode.ps1 -Mode off` (mọi thứ trở lại như cũ).

**Tiêu chí nghiệm thu:** ở chế độ `CYDLCD_SERIAL_UI`, thêm 1 mục menu tuỳ biến trong
`Configuration_adv.h` → nạp Marlin → màn CUSTOM của man hinh CYD thấy mục đó và bấm chạy đúng G-code,
không cần sửa/build lại firmware man hinh CYD.

## 3b. Phương án "chạy song song 12864 + màn mới" — ĐÃ CÂN NHẮC, TẠM HOÃN

Hỏi ngày ghi chú: *"có dùng song song cả MKS12864 và màn hình mới để đối chiếu được không?"*
→ **Không làm** (chủ máy quyết định hủy phần chạy song song). Ghi lại đây để sau này khỏi mất công
tìm hiểu lại — nếu muốn làm thì đây là 4 ràng buộc **đã kiểm bằng build lỗi thật**:

1. Trùng symbol khi link khi bật cả `CYDLCD_SERIAL_UI` và màn gắn trực tiếp:
   `MarlinUI::init_lcd()` (`extui/ui_api.cpp:1188` vs `dogm/marlinui_DOGM.cpp:263`),
   `MarlinUI::update()` (`ui_api.cpp:1190` vs `lcd/marlinui.cpp:942`),
   `MarlinUI::kill_screen()` (`ui_api.cpp:1194` vs `marlinui.cpp:726`),
   `MarlinUI::pause_show_message()` (`marlinui.cpp:1936` vs `menu/menu_filament.cpp:339`).
   → Muốn song song phải: cho 3 hàm đầu gọi **cả** DOGM **và** `ExtUI::onStartup()/onIdle()`
   (thay vì `ui_api.cpp` định nghĩa đè), và để `pause_show_message` chỉ còn 1 định nghĩa.
2. **PD8 vừa là `NEOPIXEL_PIN`** (đèn nền RGB của Mini12864 V3) **vừa là USART3_TX** của man hinh CYD ⇒
   dùng song song thì phải chuyển man hinh CYD sang **USART1 = PA9/PA10 (header WiFi)** (đã kiểm: PA9 =
   `USART1_TX`, PA10 = `USART1_RX` trong variant, `WIFI_SERIAL` đang comment nên còn trống).
3. ExtUI không dùng hệ thống menu của MarlinUI ⇒ SanityCheck chặn combo (muốn song song phải nới
   điều kiện này; khi đó `CUSTOM_MENU_MAIN` **không** phải tắt — tiện đối chiếu).
4. Tốn thêm flash/RAM cho cả hai UI cùng lúc.

Cách thay thế đang dùng: **nạp qua lại** 2 firmware (`set_panel_mode.ps1 -Mode on|off` + `pio run -e
mks_monster8 -t upload`), hoặc tạm dùng man hinh CYD kiểu G-code cũ trên `SERIAL_PORT_2 1` (USART1) nếu chỉ
cần đối chiếu nhiệt độ / vị trí / tiến độ mà không muốn sửa Marlin.


**File sẽ phải sửa:** `Marlin/src/lcd/extui/cydlcd_serial/panel_protocol.h` (+ bản copy
`cydlcd/src/panel_protocol.h`), `Marlin/src/lcd/extui/cydlcd_serial/cydlcd_serial.cpp`,
`cydlcd/src/ui_lvgl.cpp`, `Marlin/Configuration_adv.h`, `cydlcd/tools/test_panel_link.cpp`
(thêm test cho frame MENU).

## 3c. HƯỚNG MỚI ĐÃ CHỌN (2026-10-13) — `PANEL_TRAM_LINK`: Tramming Wizard trên màn riêng **KHÔNG cần ExtUI**

**Vấn đề:** màn CYD cần hiển thị + điều khiển **Bed Tramming Wizard**. Cách cũ (bật `CYDLCD_SERIAL_UI`)
bắt buộc tắt `MKS_MINI_12864_V3` + `CUSTOM_MENU_MAIN` + `LED_CONTROL_MENU` (mục 1) ⇒ **mất toàn bộ
menu MarlinUI** đang dùng (Tramming Wizard, `Z ALIGN MANUAL`, custom menu…). Không chấp nhận được.

**Cách đã chọn:** thêm **một link serial RIÊNG, rất nhỏ** (`src/feature/panel_tram_link.cpp`) chỉ để
đổi trạng thái Tramming Wizard, **không dùng ExtUI**:

1. **Tách thuật toán khỏi menu:** toàn bộ máy trạng thái của wizard được chuyển từ
   `src/lcd/menu/menu_tramming.cpp` sang `src/feature/tramming_run.cpp/.h` (không phụ thuộc
   MarlinUI). `menu_tramming.cpp` trở thành lớp UI mỏng. **Hành vi không đổi** (mốc `Z-MIN`/`Z-MAX`
   chốt 1 lần, ngưỡng auto-DAT 0.01 mm, tự đi tiếp khi DAT, `DONE` = góc xong, `BACK` = thoát).
2. **Hai UI dùng CHUNG một trạng thái** ⇒ số trên màn 12864 và trên man hinh CYD luôn giống nhau; chỉ
   **một** nguồn được "tiến bước" (`tramming_run::step()`):
   * màn 12864 đang mở wizard ⇒ `menu_tramming.cpp` gọi `step()`;
   * không mở ⇒ `panel_tram_link::tick()` (trong `manage_inactivity()`) gọi `step()`.
3. **Giao thức ASCII** (`src/feature/tram_protocol.h`, bản y nguyên ở `cydlcd/src/tram_protocol.h`):
   * man hinh CYD → Marlin: `P` probe, `D` done, `M` đổi MAX/MIN, `R` home+probe lại, `B` thoát, `Q` hỏi;
   * Marlin → man hinh CYD: `TRAM <phase> <mode> <idx> <ref> <d0..d3> <A0..A3> <active> <all_dat> <hint>`,
     gửi khi trạng thái đổi + định kỳ ~5 Hz. Trường `hint` do **Marlin** tính (dùng đúng
     `tramming_turn_cw` + `TRAM_DONE_MM`) nên man hinh CYD không phải tự suy lại chiều vặn vít.
4. **Chân nối: USART1 = PA9 (TX) / PA10 (RX)** — đúng như đã ghi ở §3b điểm 2. **Không** dùng
   USART3/PD8 nữa, nên **không phải tắt `NEOPIXEL_LED`** và **không đụng chân reset của màn 12864**
   (`PD9 = EXP1_05 = LCD_RESET_PIN`). Header dùng: hàng chân **MKS WiFi** (`WIFI_SERIAL 1` đang
   comment trong `pins_MKS_MONSTER8_V2.h`).
   Build flag `-DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9` được khai trong
   `ini/features.ini` (key `PANEL_TRAM_LINK`) nên **chỉ** có tác dụng khi bật `#define PANEL_TRAM_LINK`.
5. **Mặc định TẮT** (`//#define PANEL_TRAM_LINK` trong `feature/panel_tram_link.h`). Chưa bật ⇒
   không biên dịch module, không chiếm UART, không tốn flash, `Configuration.h`/`Configuration_adv.h`
   giữ nguyên 100%.
6. **Cách bật:** thêm đúng 1 dòng `#define PANEL_TRAM_LINK` vào `Marlin/Configuration.h` rồi build
   `pio run -e mks_monster8`. Khi đó: màn 12864 **vẫn đủ menu như cũ**, màn CYD có thêm màn
   `TRAMMING` (vào từ MENU) với 4 số của 4 góc + dòng hướng dẫn + 5 nút HOME+PROBE (`R`, khởi động
   wizard) / PROBE / MAX→MIN / BACK / DONE.

**So với Cách A (§3):** Cách A vẫn hữu ích nếu sau này muốn **toàn bộ** UI man hinh CYD (file SD, bed mesh,
PID…) mà không cần màn 12864 — nhưng nó vẫn phải trả giá bằng việc tắt `MKS_MINI_12864_V3` và
`CUSTOM_MENU_MAIN`/`LED_CONTROL_MENU`. Hướng `PANEL_TRAM_LINK` giải quyết đúng nhu cầu trước mắt
(Tramming Wizard) mà **không mất gì** của bản build hiện tại.

**Đổi lại (đã biết, chấp nhận):** màn CYD ở chế độ này chỉ có màn `TRAMMING` là có dữ liệu; các màn
STATUS/TEMP/FILES… vẫn hiện nhưng báo `NO LINK` vì không có frame PUSH (muốn có thì phải bật
`CYDLCD_SERIAL_UI` — và khi đó `PANEL_TRAM_LINK` tự bị `#error` chặn vì 2 chế độ loại trừ nhau).

**File đã sửa/thêm (2026-10-13):** `Marlin/src/feature/tramming_run.{h,cpp}` (mới),
`Marlin/src/feature/panel_tram_link.{h,cpp}` (mới), `Marlin/src/feature/tram_protocol.h` (mới, copy
sang `cydlcd/src/tram_protocol.h`), `Marlin/src/lcd/menu/menu_tramming.cpp` (rút gọn thành UI mỏng),
`Marlin/src/MarlinCore.cpp` (gọi `panel_tram_link::tick()` trong `manage_inactivity()`),
`ini/features.ini`, `ini/stm32f4.ini` (ghi chú), `cydlcd/src/tram_link.{h,cpp}` (mới),
`cydlcd/src/panel_link.cpp` (chia sẻ UART), `cydlcd/src/ui_lvgl.cpp` (màn TRAMMING + 1 dòng MENU),
`cydlcd/tools/test_panel_link.cpp` (thêm ~25 test cho parse/format TRAM).

**Kiểm chứng đã chạy (2026-10-13, build trên máy này):**

| Build | Kết quả | Flash | RAM |
|---|---|---|---|
| `pio run -e mks_monster8` — `PANEL_TRAM_LINK` **tắt** (mặc định, 12864) | **SUCCESS** | 267.576 B (25,5 %) | 12.220 B (9,3 %) |
| `pio run -e mks_monster8` — `PANEL_TRAM_LINK` **bật** (thêm tạm 1 dòng vào `Configuration.h`) | **SUCCESS** | 270.592 B (25,8 %) | 12.828 B (9,8 %) |
| `pio run -d cydlcd -e cydlcd_esp32` | **SUCCESS** | 463.101 B (35,3 %) | 102.044 B (31,1 %) |
| `pio run -d cydlcd -e panel_f407vgz_lvgl` | **SUCCESS** | 158.756 B (15,1 %) | 81.328 B (13,9 %) |
| `pio run -d cydlcd -e cydlcd_f411cc_lvgl` | **SUCCESS** | xem `cydlcd/README.md` §1.1 | |

* Sau khi kiểm, `Marlin/Configuration.h`/`Configuration_adv.h` đã **trả về đúng bản gốc**:
  `git diff -- Marlin/Configuration.h Marlin/Configuration_adv.h` = **RỖNG** (đã xác nhận).
* `ini/features.ini` (key `PANEL_TRAM_LINK`) tự thêm
  `-DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9` **chỉ khi** define được bật.
* Host test: thêm **35 kiểm tra** cho TRAM vào `cydlcd/tools/test_panel_link.cpp`. **Máy này
  KHÔNG chạy được** `run_host_test.ps1` (không có `g++`/MinGW, không có `pwsh`) — đã kiểm bằng
  `arm-none-eabi-g++ -fsyntax-only -Wall -Wextra` → **0 lỗi, 0 cảnh báo**. Cần chạy thật bằng MinGW.
* **CHƯA kiểm chứng trên máy in**: chưa nạp firmware nào (theo yêu cầu). Cần nạp thử để xác nhận
  dây USART1 (PA9/PA10) + màn TRAMMING chạy thật.

## 4. Ghi chú vận hành (không quên)

* Trước khi bật `CYDLCD_SERIAL_UI`, luôn chạy `set_panel_mode.ps1 -Mode on` (script tự tắt 4 mục
  trên) — nếu bật tay mà quên, Marlin sẽ báo lỗi rõ ràng ở SanityCheck.
* Nhớ lại trạng thái cũ bất cứ lúc nào: `pwsh cydlcd/tools/set_panel_mode.ps1 -Mode status`.

---

## 5. Cách tạo issue này trên GitHub

**a) Bằng trình duyệt (nhanh nhất):**
1. Mở https://github.com/tungbuivn/Marlin/issues/new
2. Dán **Tiêu đề** ở §"Tiêu đề" phía trên.
3. Dán toàn bộ mục 1–4 vào phần nội dung, gắn nhãn như §"Nhãn gợi ý" rồi Create.

**b) Bằng `gh` CLI (nếu cài):**

```powershell
winget install --id GitHub.cli           # cai gh (1 lan)
gh auth login                            # dang nhap tai khoan tungbuivn
cd D:\Marlin
gh issue create `
  --repo tungbuivn/Marlin `
  --title "CYDLCD_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến" `
  --body-file docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md `
  --label enhancement,config,man hinh CYD
```

**c) Bằng REST API (nếu có token):**

```powershell
$env:GITHUB_TOKEN = "ghp_..."; # token co quyen repo
$body = @{ title = "CYDLCD_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến";
           body  = (Get-Content docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md -Raw) } | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/tungbuivn/Marlin/issues" `
  -Headers @{ Authorization = "Bearer $env:GITHUB_TOKEN"; Accept = "application/vnd.github+json" } `
  -ContentType "application/json" -Body $body
```

> Sau khi issue được tạo, xoá dòng "NHÁP — chưa tạo trên GitHub" ở đầu file này và thêm số issue
> vào `cydlcd/README.md` (§3) để lần sau tra cứu.
