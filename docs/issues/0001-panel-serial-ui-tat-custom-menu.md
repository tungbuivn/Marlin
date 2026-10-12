# Issue nháp: `CUSTOM_MENU_MAIN` bị tắt khi bật `PANEL_SERIAL_UI` (cần cơ chế để thêm lại)

> **Trạng thái:** NHÁP — chưa tạo trên GitHub (máy này không có `gh` CLI, không có `GITHUB_TOKEN`,
> và PowerShell không ra được `api.github.com`). Cách tạo: xem §4 cuối file.
> **Repo:** `tungbuivn/Marlin` · **nhánh liên quan:** `mks-monster8-Voron-2.1.x`

---

## Tiêu đề (dùng nguyên văn khi tạo issue)

```
PANEL_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến
```

## Nhãn gợi ý

`enhancement` · `config` · `panel`

---

## 1. Chuyện gì đã xảy ra

Để dùng màn hình riêng (LVGL) qua ExtUI `panel_serial`, Marlin phải chạy ở "chế độ panel":
`pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode on` đổi **5 dòng** config (script nằm trong repo):

| Dòng | File:line | Chế độ màn 12864 (mặc định) | Chế độ panel | Vì sao |
|---|---|---|---|---|
| `PANEL_SERIAL_UI` | `Marlin/Configuration.h:3176` | tắt | **bật** | ExtUI `panel_serial` đẩy frame dữ liệu xuống màn hình |
| `MKS_MINI_12864_V3` | `Marlin/Configuration.h:2950` | bật | **tắt** | ExtUI chiếm `MarlinUI::init_lcd()/update()` → vừa có màn gắn trực tiếp là lỗi `multiple definition of MarlinUI::init_lcd()` |
| `NEOPIXEL_LED` | `Marlin/Configuration.h:3449` | bật | **tắt** | PD8 (đèn nền RGB của màn V3) **trùng USART3_TX** dùng cho panel |
| `CUSTOM_MENU_MAIN` | `Marlin/Configuration_adv.h:3792` | bật | **tắt** | ExtUI không dùng hệ thống menu của MarlinUI (Marlin báo lỗi SanityCheck `CUSTOM_MENU_MAIN requires an LCD controller that implements the menu`) |
| `LED_CONTROL_MENU` | `Marlin/Configuration_adv.h:1508` | bật | **tắt** | cùng lý do (và menu đèn LED cần NeoPixel) |

⇒ Đây là **thoả hiệp tạm thời**, không phải thiết kế mong muốn: chế độ panel hiện **không có**
menu tuỳ biến của Marlin.

## 2. Đã bù bằng gì (và còn thiếu gì)

* 2 mục menu tuỳ biến hiện có đã được đưa sang **màn CUSTOM của panel** (`ui_lvgl.cpp`), đúng nội dung cũ:
  * `MAIN_MENU_ITEM_1` — `"G34 Bed"` → `G34 Q99` (`Configuration_adv.h:3799-3800`)
  * `MAIN_MENU_ITEM_2` — `"Init SD card"` → `M22` + `M21` (`Configuration_adv.h:3804-3805`)
* **Còn thiếu / sẽ thiếu về sau:**
  1. Thêm mục menu tuỳ biến mới hiện phải **sửa code panel** (`ui_lvgl.cpp`) rồi build lại — không
     còn là "sửa 1 dòng config như `MAIN_MENU_ITEM_n`" nữa.
  2. `LED_CONTROL_MENU` + `LED_COLOR_PRESETS` (đổi màu/độ sáng đèn) biến mất hoàn toàn ở chế độ panel.
  3. Các menu khác của MarlinUI (`CUSTOM_MENU_CONFIG`, `CUSTOM_USER_MENUS`…) sẽ vướng cùng lỗi nếu bật.

## 3. Việc cần làm để "thêm lại được" (đề xuất)

**Cách A (khuyến nghị) — Marlin đẩy danh sách menu tuỳ biến xuống panel:**

1. Thêm frame mới vào `panel_protocol.h` (dùng chung 2 phía), ví dụ:
   `PANEL_F_MENU = 0x0E` — payload: `u8 so_muc`, mỗi mục `str nhan`, `str gcode` (gửi nhiều frame
   nếu dài; frame kết thúc `so_muc = 0xFF`).
2. Trong `Marlin/src/lcd/extui/panel_serial/panel_serial.cpp`: đọc danh sách từ config mới
   (ví dụ `PANEL_MENU_ITEM_1_DESC/GCODE` … hoặc tái dùng chính `MAIN_MENU_ITEM_n_*` khi
   `CUSTOM_MENU_MAIN` tắt — cần `#if ENABLED(PANEL_SERIAL_UI)` cho phép define mà không cần menu),
   gửi khi `HELLO`/`REQUEST FULL`.
3. Panel (`ui_lvgl.cpp` màn CUSTOM): vẽ các mục nhận được thành nút, bấm → `panel_link_send_gcode()`.
4. Lợi: thêm mục mới chỉ cần sửa `Configuration_adv.h` + nạp lại Marlin, **không** phải build lại panel.

**Cách B — bật lại `LED_CONTROL_MENU`:** chỉ hợp lý nếu quay lại dùng NeoPixel; khi đó phải chọn
chân LED khác (không dùng PD8 vì trùng USART3_TX của panel).

**Cách C — quay lại màn 12864:** `set_panel_mode.ps1 -Mode off` (mọi thứ trở lại như cũ).

**Tiêu chí nghiệm thu:** ở chế độ `PANEL_SERIAL_UI`, thêm 1 mục menu tuỳ biến trong
`Configuration_adv.h` → nạp Marlin → màn CUSTOM của panel thấy mục đó và bấm chạy đúng G-code,
không cần sửa/build lại firmware panel.

## 3b. Phương án "chạy song song 12864 + màn mới" — ĐÃ CÂN NHẮC, TẠM HOÃN

Hỏi ngày ghi chú: *"có dùng song song cả MKS12864 và màn hình mới để đối chiếu được không?"*
→ **Không làm** (chủ máy quyết định hủy phần chạy song song). Ghi lại đây để sau này khỏi mất công
tìm hiểu lại — nếu muốn làm thì đây là 4 ràng buộc **đã kiểm bằng build lỗi thật**:

1. Trùng symbol khi link khi bật cả `PANEL_SERIAL_UI` và màn gắn trực tiếp:
   `MarlinUI::init_lcd()` (`extui/ui_api.cpp:1188` vs `dogm/marlinui_DOGM.cpp:263`),
   `MarlinUI::update()` (`ui_api.cpp:1190` vs `lcd/marlinui.cpp:942`),
   `MarlinUI::kill_screen()` (`ui_api.cpp:1194` vs `marlinui.cpp:726`),
   `MarlinUI::pause_show_message()` (`marlinui.cpp:1936` vs `menu/menu_filament.cpp:339`).
   → Muốn song song phải: cho 3 hàm đầu gọi **cả** DOGM **và** `ExtUI::onStartup()/onIdle()`
   (thay vì `ui_api.cpp` định nghĩa đè), và để `pause_show_message` chỉ còn 1 định nghĩa.
2. **PD8 vừa là `NEOPIXEL_PIN`** (đèn nền RGB của Mini12864 V3) **vừa là USART3_TX** của panel ⇒
   dùng song song thì phải chuyển panel sang **USART1 = PA9/PA10 (header WiFi)** (đã kiểm: PA9 =
   `USART1_TX`, PA10 = `USART1_RX` trong variant, `WIFI_SERIAL` đang comment nên còn trống).
3. ExtUI không dùng hệ thống menu của MarlinUI ⇒ SanityCheck chặn combo (muốn song song phải nới
   điều kiện này; khi đó `CUSTOM_MENU_MAIN` **không** phải tắt — tiện đối chiếu).
4. Tốn thêm flash/RAM cho cả hai UI cùng lúc.

Cách thay thế đang dùng: **nạp qua lại** 2 firmware (`set_panel_mode.ps1 -Mode on|off` + `pio run -e
mks_monster8 -t upload`), hoặc tạm dùng panel kiểu G-code cũ trên `SERIAL_PORT_2 1` (USART1) nếu chỉ
cần đối chiếu nhiệt độ / vị trí / tiến độ mà không muốn sửa Marlin.


**File sẽ phải sửa:** `Marlin/src/lcd/extui/panel_serial/panel_protocol.h` (+ bản copy
`lcd_panel/src/panel_protocol.h`), `Marlin/src/lcd/extui/panel_serial/panel_serial.cpp`,
`lcd_panel/src/ui_lvgl.cpp`, `Marlin/Configuration_adv.h`, `lcd_panel/tools/test_panel_link.cpp`
(thêm test cho frame MENU).

## 4. Ghi chú vận hành (không quên)

* Trước khi bật `PANEL_SERIAL_UI`, luôn chạy `set_panel_mode.ps1 -Mode on` (script tự tắt 4 mục
  trên) — nếu bật tay mà quên, Marlin sẽ báo lỗi rõ ràng ở SanityCheck.
* Nhớ lại trạng thái cũ bất cứ lúc nào: `pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode status`.

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
  --title "PANEL_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến" `
  --body-file docs/issues/0001-panel-serial-ui-tat-custom-menu.md `
  --label enhancement,config,panel
```

**c) Bằng REST API (nếu có token):**

```powershell
$env:GITHUB_TOKEN = "ghp_..."; # token co quyen repo
$body = @{ title = "PANEL_SERIAL_UI: phải tắt CUSTOM_MENU_MAIN + LED_CONTROL_MENU — cần cách thêm lại menu tuỳ biến";
           body  = (Get-Content docs/issues/0001-panel-serial-ui-tat-custom-menu.md -Raw) } | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/tungbuivn/Marlin/issues" `
  -Headers @{ Authorization = "Bearer $env:GITHUB_TOKEN"; Accept = "application/vnd.github+json" } `
  -ContentType "application/json" -Body $body
```

> Sau khi issue được tạo, xoá dòng "NHÁP — chưa tạo trên GitHub" ở đầu file này và thêm số issue
> vào `lcd_panel/README.md` (§3) để lần sau tra cứu.
