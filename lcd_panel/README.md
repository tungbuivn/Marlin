# lcd_panel — firmware màn hình riêng cho Marlin (Voron 300 / MKS Monster8 V2)

Một MCU **riêng** lo toàn bộ việc vẽ màn hình; Marlin không phải render gì.
Có **2 kiểu giao tiếp**, chọn bằng env trong `lcd_panel/platformio.ini` (không phải sửa code):

| | **Kiểu G-code** | **Kiểu PUSH** (mới, dùng cho màn LVGL) |
|---|---|---|
| Env | `panel_f411cc`, `panel_f103c6` | `panel_f407vgz_lvgl`, `panel_f411cc_lvgl`, `panel_esp32_cyd` |
| Panel lấy dữ liệu | tự hỏi `M105` / `M114` / `M27` (`marlin_link.cpp`) | Marlin **chủ động đẩy frame nhị phân** xuống (`panel_link.cpp`) |
| Vẽ bằng | driver ILI9341 tự viết + font 8×8 (`ui.cpp`) | **LVGL 8.3** (`ui_lvgl.cpp`) |
| Marlin cần | chỉ bật 1 UART trống | bật `PANEL_SERIAL_UI` (ExtUI `panel_serial`) + tắt màn 12864 |
| Dữ liệu nặng (file SD, bed mesh, PID/steps/mm, prompt…) | **không có** | **có** (Marlin đẩy hết) |
| Flash/RAM | 20,7 KB / 1,9 KB (F411) · 21,1 KB / 1,9 KB (F103C6) | cần ≥ 256 KB flash / 128 KB RAM (LVGL + font) |

> **Vì sao có kiểu PUSH?** Panel ở MCU riêng *không* đọc được biến nội bộ của Marlin. Nếu chỉ
> hỏi bằng G-code thì rất nhiều "khối" không thể vẽ: danh sách file SD, lưới bed mesh, thông số
> PID / steps/mm / jerk, trạng thái probe/endstop, hộp thoại xác nhận, tên file đang in…
> (MKS TFT cũng làm kiểu G-code và phải thêm hàng loạt M-code riêng —
> tham khảo `makerbase-mks/MKS-TFT28-32-Firmware`.) Nên kiểu mới: Marlin **đẩy** dữ liệu xuống,
> panel chỉ gửi G-code cho **hành động** (G28 / M104 / M140 / M24 / M524…).

---

## 1. Chọn board / env

| Env | Board | Màn hình | Giao tiếp |
|---|---|---|---|
| `panel_f411cc` | STM32F411CCU6 ("Black Pill") | ILI9341 SPI 240×320 | G-code |
| `panel_f103c6` | STM32F103C6T6 ("Blue Pill") | ILI9341 SPI 240×320 | G-code |
| `panel_f411cc_lvgl` | STM32F411CCU6 | ILI9341 SPI 240×320 | PUSH + LVGL |
| `panel_f407vgz_lvgl` | STM32F407VGT6 (1 MB/128 KB) | ILI9341 SPI 240×320 | PUSH + LVGL |
| `panel_esp32_cyd` | **ESP32-2432S028R "CYD"** 2.8" | ILI9341 + XPT2046 (gắn sẵn trên board) | PUSH + LVGL |

* Board F407 khác (F407ZGT6 144 chân…): sửa `board =` trong env (`black_f407zg`,
  `vccgnd_f407zg_mini`, `genericSTM32F407IGT6`…) và giữ nguyên phần còn lại.
* Env LVGL kéo thư viện từ mạng lần đầu: `lvgl@^8.3.11` (+ `TFT_eSPI` cho CYD).

```powershell
# từ thư mục gốc repo
pio run -d lcd_panel -e panel_f411cc                     # build (G-code UI)
pio run -d lcd_panel -e panel_f407vgz_lvgl               # build (LVGL + PUSH)
pio run -d lcd_panel -e panel_esp32_cyd -t upload        # nạp qua USB (CYD)
pio run -d lcd_panel -e panel_f407vgz_lvgl -t upload     # nạp qua ST-Link
```

`lcd_panel` là **project PlatformIO riêng**: cờ/script/thư viện của Marlin không lọt vào và
ngược lại. Mỗi env chỉ lấy đúng file nó cần (`build_src_filter`: env G-code loại
`panel_link.cpp`/`ui_lvgl*.cpp`; env PUSH loại `marlin_link.cpp`/`ui.cpp`; riêng env CYD loại thêm
`tft_ili9341.cpp`/`encoder.cpp` vì đó là driver/chân của STM32).

### 1.1 Số liệu build thật (đã build kiểm trên máy này)

| Env | Flash | RAM |
|---|---|---|
| `panel_f411cc` (G-code UI) | 20.688 B / 262.144 (7,9 %) | 1.912 B / 131.072 (1,5 %) |
| `panel_f103c6` (G-code UI) | 21.096 B / 32.768 (**64,4 %**) | 1.880 B / 10.240 (18,4 %) |
| `panel_f411cc_lvgl` | 134.012 B / 262.144 (51,1 %) | 46.180 B / 131.072 (35,2 %) |
| `panel_f407vgz_lvgl` | 135.928 B / 1.048.576 (13,0 %) | 74.900 B / 131.072 (57,1 %) |
| `panel_esp32_cyd` | 441.561 B / 1.310.720 (33,7 %) | 95.284 B / 327.680 (29,1 %) |

> Nếu F411 thấy lỗi cấp phát LVGL (`lv_mem` hết) thì tăng `LV_MEM_SIZE` trong `src/lv_conf.h`
> từ 20 KB lên 28–32 KB (F411 còn 128 KB RAM, buffer vẽ 240×40×2 = 19,2 KB).

---

## 2. Nối dây

### 2.1 UART panel ↔ Marlin (MKS Monster8 V2)

Marlin đang dùng `SERIAL_PORT -1` (USB CDC) để nói với PC → còn UART trống. Dùng **USART3**
vì chân nằm ngay header EXP1 (header màn 12864 cũ):

| Monster8 EXP1 | Chân MCU | Nối tới panel |
|---|---|---|
| chân 5 | PD9 = USART3 **RX** | ← TX của panel |
| chân 6 | PD8 = USART3 **TX** | → RX của panel |
| chân 9 | GND | GND (bắt buộc nối chung) |
| chân 10 | 5V | 5V *nếu* board panel có regulator 5V→3V3 |

* Chiều chân lấy từ `buildroot/share/PlatformIO/variants/MARLIN_F4x7Vx/variant.h`:
  **PD8 = USART3_TX, PD9 = USART3_RX** ⇒ dây phải **chéo** (TX↔RX), **chung GND**.
* ⚠️ **USART2 (PA2/PA3) của Monster8 đã bị dùng** cho `HEATER_2` (PA3) và `FAN0` (PA2).
* ⚠️ **PD8 còn là `NEOPIXEL_PIN` = `EXP1_06_PIN`** (đèn nền RGB của màn 12864 V3, `NEOPIXEL_LED`
  đang bật trong `Configuration.h`) ⇒ phải tắt cả `MKS_MINI_12864_V3` **và** `NEOPIXEL_LED`
  (xem §4).
* **Panel CYD (ESP32)**: UART đến Marlin = `Serial2`, mặc định **GPIO22 = TX** (nối vào PD9),
  **GPIO35 = RX** (nối từ PD8). ESP32 là 3V3 nên **không** nối 5V từ EXP1 vào chân 3V3 của CYD;
  cấp nguồn cho CYD bằng USB hoặc 5V riêng. Đổi chân trong `src/panel_config.h`
  (`PANEL_UART_TX` / `PANEL_UART_RX`).

### 2.2 Màn hình (khi đi dây với board STM32)

| Chân panel | Chức năng | Ghi chú |
|---|---|---|
| PA5 / PA7 / PA6 | SPI1 SCK / MOSI / MISO | ILI9341 |
| PA4 / PB0 / PB1 | CS / DC(RS) / RST | |
| PB10 | Backlight (BL) | `TFT_PIN_BL -1` nếu BL nối thẳng 3V3 |
| **PA2 / PA3** | **USART2 TX / RX → mainboard** | đây là `Serial` trong `panel_config.h` |
| PB12 / PB13 / PB14 | Encoder A / B / SW (nút) | dùng `INPUT_PULLUP` (không cần cho CYD) |

Với **CYD**: màn + cảm ứng đã nối sẵn trong board, cấu hình nằm ở
`lcd_panel/tools/tft_setup/User_Setup.h` (ILI9341: 13/12/14/15/2/21; touch: 32/39/25/33/36).

---

## 3. Phía Marlin — bật chế độ panel (1 lệnh)

```powershell
pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode on     # bat panel:  PANEL_SERIAL_UI + tat man cu
pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode off    # tra ve man 12864 nhu cu
pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode status # xem dang bat gi
pio run -e mks_monster8 -t upload                    # nap Marlin
```

Script đổi **5 dòng** trong `Marlin/Configuration.h` + `Marlin/Configuration_adv.h`:

| Dòng | Chế độ panel | Vì sao |
|---|---|---|
| `PANEL_SERIAL_UI` | **bật** | ExtUI `panel_serial`: đẩy frame xuống panel |
| `MKS_MINI_12864_V3` | tắt | ExtUI chiếm `MarlinUI::init_lcd()/update()` → không thể vừa có màn gắn trực tiếp (lỗi `multiple definition of MarlinUI::init_lcd()`) |
| `NEOPIXEL_LED` | tắt | PD8 (đèn nền RGB của màn V3) trùng USART3_TX |
| `CUSTOM_MENU_MAIN` | tắt | ExtUI không dùng hệ thống menu của MarlinUI — menu tuỳ biến chuyển sang panel (màn **CUSTOM** có đúng 2 nút cũ: `G34 Q99`, `M22`+`M21`) |
| `LED_CONTROL_MENU` | tắt | menu đèn LED thuộc MarlinUI |

Cần thêm trong `ini/stm32f4.ini` (env `mks_monster8`) — **đã có sẵn** trong repo:

```ini
-DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8
```

> 📌 **`CUSTOM_MENU_MAIN` bị tắt ở chế độ panel** — đây là thoả hiệp tạm thời. 2 mục menu cũ đã
> chuyển sang màn **CUSTOM** của panel, nhưng muốn *thêm mục mới* thì hiện phải sửa code panel.
> Kế hoạch làm lại (Marlin đẩy danh sách menu xuống panel): **`docs/issues/0001-panel-serial-ui-tat-custom-menu.md`**.

Số liệu build thật (`pio run -e mks_monster8`):

| | Flash | RAM |
|---|---|---|
| Chế độ màn 12864 (mặc định) | 266.536 B (25,4 %) | 12.192 B (9,3 %) |
| Chế độ panel `PANEL_SERIAL_UI` | 196.444 B (18,7 %) | 11.672 B (8,9 %) |

---

## 4. Giao thức PUSH (frame nhị phân)

Định nghĩa duy nhất ở **`src/panel_protocol.h`** — file này **dùng chung y nguyên** với
`Marlin/src/lcd/extui/panel_serial/panel_protocol.h` (sửa một bên thì copy sang bên kia).

```
[0]=0xA5 [1]=0x5A [2]=type [3]=seq [4]=len [5..]=payload [5+len]=CRC8(poly 0x07)
```

Marlin → panel: `HELLO` `STATUS` `TEMPS` `POSITION` `PROGRESS` `MESSAGE` `FANS` `MESH` `FILES`
`EVENT` `SETTING` `PROMPT` `PONG`.
Panel → Marlin: `GCODE` (hành động), `REQUEST` (xin file/mesh/settings/FULL), `PING`, `ANSWER`.

Số nguyên little-endian; nhiệt độ = °C×100 (i16), vị trí = mm×1000 (i32), phần trăm = %×100.
Marlin đẩy `STATUS`+`TEMPS`+`POSITION` mỗi 250 ms, `PROGRESS` mỗi 1 s khi đang in, và các sự kiện
ngay khi xảy ra; panel gửi `PING` 1 s/lần để hai bên biết còn sống (mất liên kết sau 3 s).

### 4.1 Kiểm thử trên máy tính (không cần board)

```powershell
pwsh lcd_panel/tools/run_host_test.ps1
```

Chương trình `tools/test_panel_link.cpp` dùng **đúng** bộ tạo/giải frame trong
`src/panel_protocol.h` và nạp trực tiếp `src/panel_link.cpp` với một UART giả → kiểm luôn
đường nhận + phân tích frame. Hiện có **40 kiểm tra**: HELLO/STATUS/TEMPS/POSITION/PROGRESS/
FANS/SETTING/MESH/FILES/EVENT/PROMPT/ANSWER, PING + mất liên kết, và nhóm chịu lỗi (rác trước
frame, CRC sai, `len` quá lớn, tự đồng bộ lại sau lỗi). Lần chạy đầu đã bắt được **2 lỗi thật**
(lệch offset 2 byte của `feed/flow %`, và điều kiện gửi PING sai) — nay **40/40 pass**.

---

## 5. Điều khiển trên panel (UI LVGL)

| Thao tác | Việc |
|---|---|
| Xoay encoder / chạm | di chuyển focus trong màn hình |
| Bấm encoder / chạm nút | chọn |
| Bấm giữ (>0,7 s) | quay về màn STATUS |
| Nút trên màn | chuyển màn: STATUS · TEMP · JOG · FILES · SETTINGS · MESH · CUSTOM · INFO |

* **TEMP**: +/- nhiệt độ nozzle & bed → `M104 S…` / `M140 S…`; COOLDOWN → `M104 S0`+`M140 S0`.
* **JOG**: chọn trục, ±0.1/±1/±10 mm (`G91` → `G1 <trục><d> F600` → `G90`), HOME ALL → `G28`.
* **FILES**: danh sách từ SD (Marlin đẩy xuống), bấm file → `M23` + `M24`; PAUSE `M25`,
  RESUME `M24`, STOP `M524`; UP/REFRESH gửi `REQUEST`.
* **SETTINGS**: steps/mm · max feed · max accel · jerk (hoặc junction deviation) · PID · probe
  offset · z-offset · feed/flow %; SAVE → `M500`.
* **MESH**: lưới bed mesh (7×7 UBL) đọc từ Marlin, ô cao/thấp tô màu.
* **CUSTOM**: `G34 Bed` (`G34 Q99`) và `Init SD` (`M22`+`M21`) — đúng 2 mục menu cũ.
* **Prompt của Marlin** (hết filament, xác nhận…) hiện hộp thoại OK/CANCEL; OK gửi `ANSWER`
  → Marlin gọi `ExtUI::setUserConfirmed()`.

### 5.1 Encoder đọc bằng ngắt (`src/encoder.cpp`)

Panel **không polling** encoder: hai chân A/B ngắt đổi trạng thái, ISR chỉ đẩy mẫu 2 bit vào một
**ring cố định 32 phần tử** (không heap, không STL), rồi luồng chính giải mã bằng bảng chuyển tiếp:
chuyển tiếp **1 bit** = cạnh thật → ±1 xung (đủ `ENC_PULSES_PER_DETENT` = 4 xung = 1 nấc);
chuyển tiếp **2 bit** (dội phím / cạnh đến lúc ring đầy) → **không đếm**, chỉ đồng bộ lại;
quay quá nhanh trong một lần đọc → **kẹp ở `ENC_MAX_DETENTS`** (3).

Lý do: mỗi lần vẽ một khối lớn (ví dụ `fillScreen` 240×320 ≈ 150 KB qua SPI) mất hàng chục ms —
trong khoảng đó polling **không đọc được chân** nên chắc chắn mất nấc.

---

## 6. Xử lý sự cố

| Hiện tượng | Kiểm gì |
|---|---|
| Màn trắng / không có gì | dây SPI (SCK/MOSI/CS/DC/RST), chân BL, `TFT_ROTATION`; thử `TFT_SPI_SPEED` thấp hơn (12 MHz) nếu dây dài |
| Header hiện `NO LINK` | dây TX/RX có **chéo** không, GND chung chưa, baud 2 bên có trùng `250000`, Marlin đã bật `PANEL_SERIAL_UI` chưa (và đã tắt màn 12864 chưa), cờ `-DUSING_HW_SERIAL3 -DPIN_SERIAL3_*` có trong env chưa |
| Panel không nhận nhiệt độ/vị trí | Marlin phải build ở **chế độ panel** (`set_panel_mode.ps1 -Mode status` để xem) — nếu còn `MKS_MINI_12864_V3` thì Marlin sẽ báo lỗi SanityCheck |
| Encoder quay ngược | `ENC_REVERSE 0/1` trong `src/panel_config.h` |
| Nhiệt độ nhảy loạn | đường serial nhiễu: giảm baud 2 bên (`PANEL_MARLIN_BAUD` + `BAUDRATE_2`) hoặc dùng cáp ngắn/xoắn |
| ESP32 build lỗi `lv_conf.h` | phải có `src/lv_conf.h` và cờ `-DLV_CONF_INCLUDE_SIMPLE -Isrc` (đã có trong env). Nếu LVGL báo `Possible failure to include lv_conf.h` thì include path sai (đừng dùng `${PROJECT_DIR}` — nó bị expand sai trong project này) |
| CYD: chạm không ăn / lệch | TFT_eSPI giả định XPT2046 **chung bus SPI với LCD** (chỉ khác CS) nên `tft.getTouch()` **không** đọc được XPT2046 trên CYD; `ui_lvgl_port.cpp` đã bit-bang SPI mềm cho touch (T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36). Cần chỉnh `PANEL_CYD_TOUCH_SWAP_XY/INVERT_X/INVERT_Y/RAW_MIN/MAX` cho đúng board thật; muốn thử lại đường TFT_eSPI thì `#define PANEL_CYD_TOUCH_USE_TFTESPI` |
| CYD: màu bị đảo | bật `TFT_INVERSION_ON` / đổi `TFT_RGB_ORDER` trong `tools/tft_setup/User_Setup.h` |
| TFT_eSPI không thấy file setup | thêm `-DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h` vào `build_flags` (file đã có `#warning` nhắc) |
| Muốn xem panel nhận/gửi gì | bật `-DUSBD_USE_CDC -DHAL_PCD_MODULE_ENABLED` trong env F411 **và** `#define PANEL_DEBUG` trong `panel_config.h` → log ra SerialUSB |

---

## 7. Về font chữ (chỉ dùng cho kiểu G-code)

`src/font8x8.h` là **file sinh tự động** (Lucida Console 11px → ô 8×8) bằng
`tools/make_font.py` (cần Pillow). Muốn đổi font/cỡ:

```powershell
python lcd_panel/tools/make_font.py     # in ra bản ASCII-art để kiểm nét
```

Muốn chữ to hơn thì không cần sinh lại: driver đã hỗ trợ `print(..., size)` với `size = 2`.
Kiểu LVGL dùng font có sẵn của LVGL (`LV_FONT_MONTSERRAT_14`, bật trong `src/lv_conf.h`).
