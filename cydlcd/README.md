# cydlcd — firmware màn hình riêng cho Marlin (Voron 300 / MKS Monster8 V2)

Một MCU **riêng** lo toàn bộ việc vẽ màn hình; Marlin không phải render gì.
Có **3 kiểu giao tiếp**, chọn bằng env trong `cydlcd/platformio.ini` (không phải sửa code):

| | **Kiểu G-code** | **Kiểu PUSH** (mới, dùng cho màn LVGL) | **Kiểu TRAM** (mới — **không cần ExtUI**) |
|---|---|---|---|
| Env | `cydlcd_f411cc`, `cydlcd_f103c6` | `cydlcd_f407vgz_lvgl`, `cydlcd_f411cc_lvgl`, `cydlcd_esp32` | như kiểu PUSH |
| Màn hình CYD lấy dữ liệu | tự hỏi `M105` / `M114` / `M27` (`marlin_link.cpp`) | Marlin **chủ động đẩy frame nhị phân** xuống (`cydlcd_link.cpp`) | dòng ASCII `TRAM …` (`tbt_link.cpp`) — **chỉ** dữ liệu Tramming Wizard |
| Vẽ bằng | driver ILI9341 tự viết + font 8×8 (`ui.cpp`) | **LVGL 8.3** (`ui_lvgl.cpp`) — **màn NGANG 320×240**, bố cục + **icon lấy từ màn TFT của Marlin** (§5) |
| Marlin cần | chỉ bật 1 UART trống | bật `CYDLCD_SERIAL_UI` (ExtUI `cydlcd_serial`) + tắt màn 12864 | **chỉ thêm 1 dòng `#define CYDLCD_TBT_LINK`** — **KHÔNG** tắt gì của MarlinUI ⇒ **màn 12864 + menu MarlinUI vẫn nguyên vẹn** |
| Dữ liệu nặng (file SD, bed mesh, PID/steps/mm, prompt…) | **không có** | **có** (Marlin đẩy hết) | **không có** (chỉ Tramming Wizard — §5.6) |
| Flash/RAM | 20,7 KB / 1,9 KB (F411) · 21,1 KB / 1,9 KB (F103C6) | cần ≥ 256 KB flash / 128 KB RAM (LVGL + font + ~17 KB icon) | như kiểu PUSH (+0,3 KB khi thêm màn TRAMMING — xem §1.1) |

> 🗂️ **Đang đổi tên `lcd_panel/` → `cydlcd/`** (env `cydlcd_esp32`, file `cydlcd_*.h`, macro `CYDLCD_*`,
> biến toàn cục `cydlcd`…) và **đã XOÁ HẲN 2 đường upload cũ** (module WiFi rời + Bluetooth, 2026-10-14).
> Bảng đổi tên cũ → mới + lý do: **§9.23**; đường upload duy nhất còn lại: **§10**.

> **Vì sao có kiểu PUSH?** Màn hình CYD ở MCU riêng *không* đọc được biến nội bộ của Marlin. Nếu chỉ
> hỏi bằng G-code thì rất nhiều "khối" không thể vẽ: danh sách file SD, lưới bed mesh, thông số
> PID / steps/mm / jerk, trạng thái probe/endstop, hộp thoại xác nhận, tên file đang in…
> (MKS TFT cũng làm kiểu G-code và phải thêm hàng loạt M-code riêng —
> tham khảo `makerbase-mks/MKS-TFT28-32-Firmware`.) Nên kiểu mới: Marlin **đẩy** dữ liệu xuống,
> màn hình CYD chỉ gửi G-code cho **hành động** (G28 / M104 / M140 / M24 / M524…).

> **Vì sao có thêm kiểu TRAM (2026-10-13)?** Kiểu PUSH bắt buộc bật `CYDLCD_SERIAL_UI` (ExtUI), mà
> ExtUI **chiếm `MarlinUI::init_lcd()/update()`** nên phải **tắt màn 12864 + `CUSTOM_MENU_MAIN` +
> `LED_CONTROL_MENU`** ⇒ **mất toàn bộ menu MarlinUI** (Tramming Wizard, `Z ALIGN MANUAL`…).
> Kiểu TRAM chỉ thêm **một link ASCII rất nhỏ** (`Marlin/src/feature/cydlcd_tbt_link.cpp`) để màn
> riêng hiển thị + điều khiển **Bed Tramming Wizard**, **không dùng ExtUI** ⇒ chỉ cần thêm
> `#define CYDLCD_TBT_LINK` là xong, bản build 12864 không mất gì. Xem **§5.6** và
> `docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md` §3c.

---

## 1. Chọn board / env

| Env | Board | Màn hình | Giao tiếp |
|---|---|---|---|
| `cydlcd_f411cc` | STM32F411CCU6 ("Black Pill") | ILI9341 SPI 240×320 | G-code |
| `cydlcd_f103c6` | STM32F103C6T6 ("Blue Pill") | ILI9341 SPI 240×320 | G-code |
| `cydlcd_f411cc_lvgl` | STM32F411CCU6 | ILI9341 SPI 240×320 → **ngang 320×240** (`-DTFT_ROTATION=1`) | PUSH + LVGL |
| `cydlcd_f407vgz_lvgl` | STM32F407VGT6 (1 MB/128 KB) | ILI9341 SPI 240×320 → **ngang 320×240** (`-DTFT_ROTATION=1`) | PUSH + LVGL |
| `cydlcd_esp32` | **ESP32-2432S028R "CYD"** 2.8" | **ST7789** 240×320 (bản ILI9341 chỉ khác 1 dòng driver) + XPT2046 (gắn sẵn trên board) → **ngang 320×240** (`setRotation(1)`) | PUSH + LVGL |

* Board F407 khác (F407ZGT6 144 chân…): sửa `board =` trong env (`black_f407zg`,
  `vccgnd_f407zg_mini`, `genericSTM32F407IGT6`…) và giữ nguyên phần còn lại.
* Env LVGL kéo thư viện từ mạng lần đầu: `lvgl@^8.3.11` (+ `TFT_eSPI` cho CYD).
* **`TFT_ROTATION` (2026-10-13)**: `cydlcd_config.h` chỉ còn là **mặc định** (`TFT_ROTATION 0`, portrait
  cho UI G-code `ui.cpp`); 2 env LVGL truyền `-DTFT_ROTATION=1` để UI LVGL chạy **ngang 320×240**.
  Đổi chỗ này thì phải xem lại `UI_LCD_HOR_RES/VER_RES` trong `src/ui_lvgl.h`.
* **Env CYD — 2 cờ BẮT BUỘC** (đã có sẵn trong `platformio.ini`):
  `-DUSER_SETUP_LOADED=1 -include tools/tft_setup_cyd/User_Setup.h`. Thiếu 2 cờ này TFT_eSPI dùng
  `User_Setup.h` mặc định trong thư viện (ILI9341, **không có `TFT_BL`**) → màn trắng/không đèn nền.
  `ui_port_cyd_st7789.cpp` cũng có `#warning` nhắc nếu `CYDLCD_CYD_SETUP_LOADED` không được định nghĩa.

```powershell
# từ thư mục gốc repo
pio run -d cydlcd -e cydlcd_f411cc                     # build (G-code UI)
pio run -d cydlcd -e cydlcd_f407vgz_lvgl               # build (LVGL + PUSH)
pio run -d cydlcd -e cydlcd_esp32 -t upload        # nạp qua USB (CYD)
pio run -d cydlcd -e cydlcd_f407vgz_lvgl -t upload     # nạp qua ST-Link
```

`cydlcd` là **project PlatformIO riêng**: cờ/script/thư viện của Marlin không lọt vào và
ngược lại. Mỗi env chỉ lấy đúng file nó cần (`build_src_filter`: env G-code loại
`cydlcd_link.cpp`/`ui_lvgl*.cpp`; env PUSH loại `marlin_link.cpp`/`ui.cpp`; riêng env CYD loại thêm
`tft_ili9341.cpp`/`encoder.cpp` vì đó là driver/chân của STM32).

### 1.1 Số liệu build thật (đã build kiểm trên máy này)

| Env | Flash | RAM |
|---|---|---|
| `cydlcd_f411cc` (G-code UI) | 20.688 B / 262.144 (7,9 %) | 1.912 B / 131.072 (1,5 %) |
| `cydlcd_f103c6` (G-code UI) | 21.096 B / 32.768 (**64,4 %**) | 1.880 B / 10.240 (18,4 %) |
| `cydlcd_f411cc_lvgl` | 154.544 B / 262.144 (59,0 %) | 52.376 B / 131.072 (40,0 %) |
| `cydlcd_f407vgz_lvgl` | 156.136 B / 1.048.576 (14,9 %) | 81.096 B / 131.072 (61,9 %) |
| `cydlcd_esp32` | 460.005 B / 1.310.720 (35,1 %) | 101.812 B / 327.680 (31,1 %) |

> **2026-10-13** (commit `1edfd7aa0b` — UI kiểu Marlin + icon thật): số LVGL **đã đo lại** sau khi
> thêm bộ icon (`ui_lvgl_icons.h`, ~17 KB) và màn MENU/FEED-FLOW. Trước đó: F411 **134.012/46.180**,
> F407 **135.928/74.900**, CYD **441.561/95.284**. Con số CYD ở trên **đang bật `-DCYDLCD_CYD_DEBUG=1`**
> (cờ tạm để hiệu chuẩn touch) — tắt đi thì nhỏ hơn một chút.
> Hai env G-code (`cydlcd_f411cc`, `cydlcd_f103c6`) **không đổi** (chúng loại `ui_lvgl*.cpp` khỏi build).

> **2026-10-13** (màn **TRAMMING**, §5.6): đo lại sau khi thêm `tbt_link.cpp` + màn TRAMMING:
> `cydlcd_f411cc_lvgl` **157.152 B / 262.144 (59,9 %) · RAM 52.608 B / 131.072 (40,1 %)**;
> `cydlcd_f407vgz_lvgl` **158.756 B (15,1 %) / 81.328 B (62,0 %)**;
> `cydlcd_esp32` **463.101 B (35,3 %) / 102.044 B (31,1 %)** (vẫn bật `-DCYDLCD_CYD_DEBUG=1`).
> Màn TRAMMING thêm **~2,6 KB flash** (F407: 156.136 → 158.756) — chủ yếu là code LVGL cho 4 nút +
> 6 nhãn, phần link TRAM chỉ vài trăm byte.

> 🆕 **2026-10-14 — ĐO LẠI sau refactor (bỏ ESP3D + Bluetooth, đổi tên `panel` → `cydlcd`)**:
> `cydlcd_esp32` = **Flash 1.164.593 B / 3.145.728 (37,0 %)** · **RAM 94.324 B / 327.680 (28,8 %)**
> (bảng phía trên đo bằng bảng phân vùng **`default.csv`** cũ 1,25 MB; từ 2026-10-14 env CYD dùng
> **`huge_app.csv`** 3 MB — xem `platformio.ini` — nên **mẫu số khác**, đừng so trực tiếp).
> So với lần đo 463.101 B: flash **giảm ~2 KB** vì đã xoá hẳn 2 đường upload cũ, và RAM **giảm ~7 KB**
> (không còn đệm/Bluedroid). Vẫn bật `-DCYDLCD_CYD_DEBUG=1`. Env STM32 chưa đo lại sau refactor
> (env `cydlcd_f103c6` build được; 2 env `*_lvgl` cho STM32 **lâu nay chưa từng link được** —
> lỗi `_fstat`/`_isatty` của newlib, xem cạm bẫy 43 ở `README.md` gốc).
> Build này **vẫn giữ nguyên** `-DCYDLCD_CYD_DEBUG=1` như trước (không phải số của bản dùng thật).

> Nếu F411 thấy lỗi cấp phát LVGL (`lv_mem` hết) thì tăng `LV_MEM_SIZE` trong `src/lv_conf.h`
> từ 20 KB lên 28–32 KB (F411 còn 128 KB RAM; **buffer vẽ giờ là 320×40×2 = 25,6 KB** vì màn ngang).

> ⚠️ **Mọi số của env `cydlcd_esp32` ở trên đo TRƯỚC 2026-10-14** — tức là lúc firmware còn **cả WiFi lẫn
> Bluetooth Classic**. Sau khi xoá hẳn đường Bluetooth (§9.23) thì flash/RAM của env này **nhỏ hơn** (bớt
> cả stack Bluedroid); số mới **chưa đo lại** trong tài liệu này.

---

## 2. Nối dây

### 2.1 UART màn hình CYD ↔ Marlin (MKS Monster8 V2)

Marlin đang dùng `SERIAL_PORT -1` (USB CDC) để nói với PC → còn UART trống. **Chân nối phụ thuộc
kiểu giao tiếp** — hai kiểu dùng **hai UART khác nhau**:

| Kiểu | UART (Marlin) | Chân Monster8 | Vì sao |
|---|---|---|---|
| **PUSH** (`CYDLCD_SERIAL_UI`) | **USART1 (mặc định hiện nay)** hoặc USART3 | **USART1: PA10 (RX) / PA9 (TX) = header MKS WiFi** ← *đang dùng* · USART3: PD9 (RX) / PD8 (TX) = EXP1 chân 5/6 | Chọn bằng `-DCYDLCD_SERIAL_PORT=1` (USART1, header WiFi — dây đang đi sẵn) hoặc `=3` (USART3/EXP1, chỉ khi đã tháo màn 12864 vì EXP1_06 = `NEOPIXEL_PIN`). Cờ build nằm ngay trong `ini/features.ini` (key `CYDLCD_SERIAL_UI`). ⚠️ **Trước 2026-10-13 PUSH chỉ chạy USART3** ⇒ màn hình CYD báo `NO LINK` dù dây đúng ở header WiFi (đã sửa) |
| **TRAM** (`CYDLCD_TBT_LINK`) | **USART1** | **PA10 (RX) / PA9 (TX) = header MKS WiFi** | USART3 **hết chân TX rảnh** (PD8 = `NEOPIXEL_PIN`, PB10 = `HEATER_BED`, PC10 = `SD_SCK`); PA9/PA10 **hoàn toàn rảnh** (`//#define WIFI_SERIAL 1 // USART1` đang comment trong `pins_MKS_MONSTER8_V2.h`) |

**Kiểu TRAM (khuyến nghị cho Tramming Wizard) — KHÔNG đụng PD8/NeoPixel, KHÔNG đụng chân reset màn 12864:**

| Monster8 (header **MKS WiFi**) | Chân MCU | Nối tới màn hình CYD |
|---|---|---|
| chân RX | **PA10 = USART1_RX** | ← **TX của màn hình CYD** (CYD: GPIO22) |
| chân TX | **PA9 = USART1_TX** | → **RX của màn hình CYD** (CYD: GPIO35) |
| GND | GND | GND (**bắt buộc nối chung**) |

* Dây **chéo TX↔RX**; baud 250000 hai bên; màn hình CYD cấp nguồn riêng (USB/5V) — **không** lấy 5V từ
  header vào chân 3V3 của CYD.
* ⚠️ Header MKS WiFi còn **3 chân khác — KHÔNG dùng**: `IO0` = **PB14**, `IO1` = **PB15**
  (`WIFI_IO0_PIN`/`WIFI_IO1_PIN`, còn gọi là UDISK), và `RESET` = **PD14** (`WIFI_RESET_PIN`). Ba chân
  này **không phải UART** — PB14/PB15 là USB OTG_HS nên **không có AF USART**, và STM32F4 **không có
  software-serial tổng quát** trong Marlin ⇒ nối vào cũng không chạy. Chỉ lấy đúng **TX / RX / GND**.
* Build flag cho USART1 nằm trong **`ini/features.ini`** (key `CYDLCD_TBT_LINK`):
  `-DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9` ⇒ **chỉ** được thêm khi bật
  `#define CYDLCD_TBT_LINK`, nên bản build hiện tại không đổi gì.
* ⚠️ Nếu định bật **cả** `WIFI_SERIAL` (USART1) thì phải chọn cổng khác — `cydlcd_tbt_link.h` có
  `#error` chặn sẵn.

**Kiểu PUSH (`CYDLCD_SERIAL_UI`) — USART3, chỉ dùng khi chấp nhận tắt màn 12864:**

| Monster8 EXP1 | Chân MCU | Nối tới màn hình CYD |
|---|---|---|
| chân 5 | PD9 = USART3 **RX** | ← TX của màn hình CYD |
| chân 6 | PD8 = USART3 **TX** | → RX của màn hình CYD |
| chân 9 | GND | GND (bắt buộc nối chung) |
| chân 10 | 5V | 5V *nếu* board màn hình CYD có regulator 5V→3V3 |

* Chiều chân lấy từ `buildroot/share/PlatformIO/variants/MARLIN_F4x7Vx/variant.h`:
  **PD8 = USART3_TX, PD9 = USART3_RX** ⇒ dây phải **chéo** (TX↔RX), **chung GND**.
* ⚠️ **USART2 (PA2/PA3) của Monster8 đã bị dùng** cho `HEATER_2` (PA3) và `FAN0` (PA2).
* ⚠️ **PD8 còn là `NEOPIXEL_PIN` = `EXP1_06_PIN`** (đèn nền RGB của màn 12864 V3, `NEOPIXEL_LED`
  đang bật trong `Configuration.h`) ⇒ phải tắt cả `MKS_MINI_12864_V3` **và** `NEOPIXEL_LED`
  (xem §4).
* **Màn hình CYD (ESP32)**: UART đến Marlin = `Serial2`, mặc định **GPIO22 = TX**, **GPIO35 = RX**
  (chân khai trong `src/cydlcd_config.h`: `CYDLCD_UART_TX` / `CYDLCD_UART_RX`). Nối **chéo** với
  USART3 (PD9/PD8) ở kiểu PUSH, hoặc với **USART1 (PA10/PA9)** ở kiểu TRAM — xem bảng đầu §2.1.
  ESP32 là 3V3 nên **không** nối 5V từ EXP1 vào chân 3V3 của CYD; cấp nguồn cho CYD bằng USB hoặc
  5V riêng. `GPIO35` chỉ **đọc** được nên đúng cho chiều RX.
* ⚠️ **CYD có 3 connector 4 chân giống hệt nhau — cắm nhầm là im lặng hoàn toàn** (gặp thật
  2026-10-13: `linked=0 rx=0 err=0`, tưởng hỏng dây/baud, thực ra đang cắm vào cổng serial):

  | Connector | Chân (thứ tự trên board) | Dùng cho link? |
  |---|---|---|
  | **P3** | `GND` · **`IO35`** · **`IO22`** · `IO21` (**= TFT_BL, đèn nền — ĐỪNG nối**) | ✅ **đúng cổng** (có cả IO22 + IO35) |
  | **CN1** | `GND` · `IO22` · `IO27` · `3.3V` | ⚠️ chỉ có IO22, **không** có IO35 |
  | **P5** (một số bản in là **P1**, nằm cạnh USB-C) | `VIN` · `IO1/TX` · `IO3/RX` · `GND` | ❌ **KHÔNG** — đây là **UART0** nối với **CH340/USB**; firmware dùng `Serial2` nên **không bao giờ** thấy gì dù dây đúng chiều |

  Thứ tự chân và tên connector theo tài liệu cộng đồng CYD
  ([PINS.md](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md)):
  `IO35` (input-only) và `IO22` là 2 chân cạnh nhau trên **P3**, chân thứ 4 của P3 là **IO21 = đèn nền**
  (cắm vào đó thì màn **tối/nháy** — dấu hiệu nhận biết ngay). Muốn dùng **CN1** thì phải build lại với
  `-DCYDLCD_UART_RX=27` (firmware TRAM dùng `IO27` làm RX thay cho `IO35`).
  Cách tìm chân ①(GND) khi board không in nhãn: đo **thông mạch với vỏ USB-C**; hai chân liền kề là
  IO35 rồi IO22.
* **Test riêng phía màn hình CYD (không cần board in)**: cắm jumper **IO22 ↔ IO35** trên P3 rồi xem log USB
  (`monitor_speed 115200`): `cydlcd_link` tự nghe thấy chính nó ⇒ `rx` **tăng** = UART + 2 chân OK.
  `rx=0` **và** `err=0` = **không có tín hiệu** ở chân RX (sai cổng/chưa chung GND); nếu sai baud thì
  `err` sẽ tăng chứ không phải 0.

### 2.2 Màn hình (khi đi dây với board STM32)

| Chân màn hình CYD | Chức năng | Ghi chú |
|---|---|---|
| PA5 / PA7 / PA6 | SPI1 SCK / MOSI / MISO | ILI9341 |
| PA4 / PB0 / PB1 | CS / DC(RS) / RST | |
| PB10 | Backlight (BL) | `TFT_PIN_BL -1` nếu BL nối thẳng 3V3 |
| **PA2 / PA3** | **USART2 TX / RX → mainboard** | đây là `Serial` trong `cydlcd_config.h` |
| PB12 / PB13 / PB14 | Encoder A / B / SW (nút) | dùng `INPUT_PULLUP` (không cần cho CYD) |

Với **CYD**: màn + cảm ứng đã nối sẵn trong board, cấu hình nằm ở
`cydlcd/tools/tft_setup_cyd/User_Setup.h` — **bắt buộc** nạp file này bằng
`-DUSER_SETUP_LOADED=1 -include tools/tft_setup_cyd/User_Setup.h` (đã có trong env).

* **Driver (2026-10-13)**: board này dùng **ST7789** (`#define ST7789_DRIVER` + `TFT_INVERSION_ON`);
  nếu là bản ILI9341 thì đổi đúng 1 dòng thành `ILI9341_2_DRIVER` (và bỏ `TFT_INVERSION_ON` nếu
  trắng/đen bị ngược).
* **Chân**: LCD MOSI 13 / MISO 12 / SCLK 14 / CS 15 / DC 2 / RST −1; **đèn nền `TFT_BL` = GPIO21**
  (CYD/ST7789 bản phổ biến) **hoặc GPIO27** (nhiều bản ST7789 khác) — dò bằng `-DCYDLCD_CYD_DEBUG=1` + thử lần lượt.
  Touch: T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36 (có thể đổi bằng macro
  `CYDLCD_CYD_TOUCH_*` trong `cydlcd_config.h`).

---

## 3. Phía Marlin — 2 cách bật

### 3.1 Kiểu TRAM (khuyến nghị — giữ nguyên màn 12864 + menu MarlinUI)

```powershell
# 1) thêm ĐÚNG 1 dòng vào Marlin/Configuration.h (bất kỳ chỗ nào, ví dụ ngay dưới khối BAUDRATE):
#        #define CYDLCD_TBT_LINK
# 2) build + nạp Marlin như bình thường:
pio run -e mks_monster8 -t upload
```

* **Mặc định TẮT** (`//#define CYDLCD_TBT_LINK` trong `src/feature/cydlcd_tbt_link.h`). Chưa thêm
  dòng nào ⇒ **không** biên dịch module, **không** chiếm UART, **không** tốn flash, và
  `Configuration.h`/`Configuration_adv.h` **giữ nguyên 100%** so với bản 12864.
* Khi bật: `MKS_MINI_12864_V3` **vẫn bật**, `CUSTOM_MENU_MAIN` **vẫn bật**, `LED_CONTROL_MENU`
  **vẫn bật**, `NEOPIXEL_LED` **vẫn bật** (link dùng **USART1/PA9-PA10**, xem §2.1) ⇒ **không mất
  menu MarlinUI nào**.
* Build flag USART1 được thêm tự động từ `ini/features.ini` (key `CYDLCD_TBT_LINK`) — đúng như khi
  bạn bật định nghĩa đó, không cần sửa `platformio.ini`/`stm32f4.ini`.
* Số liệu build thật (`pio run -e mks_monster8`, đo 2026-10-13):

| | Flash | RAM |
|---|---|---|
| `CYDLCD_TBT_LINK` **tắt** (mặc định, 12864) | **267.576 B** (25,5 %) | **12.220 B** (9,3 %) |
| `CYDLCD_TBT_LINK` **bật** | **270.592 B** (25,8 %) | **12.828 B** (9,8 %) |

⇒ tính năng thêm **~3,0 KB flash / ~0,6 KB RAM** khi bật; tắt thì **không có gì thay đổi**.

### 3.2 Kiểu PUSH (`CYDLCD_SERIAL_UI`) — chỉ khi chấp nhận mất menu 12864

```powershell
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode on     # bat màn hình CYD:  CYDLCD_SERIAL_UI + tat man cu
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode off    # tra ve man 12864 nhu cu
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode status # xem dang bat gi
pio run -e mks_monster8 -t upload                    # nap Marlin
```

Script đổi **5 dòng** trong `Marlin/Configuration.h` + `Marlin/Configuration_adv.h`:

| Dòng | Chế độ màn hình CYD | Vì sao |
|---|---|---|
| `CYDLCD_SERIAL_UI` | **bật** | ExtUI `cydlcd_serial`: đẩy frame xuống màn hình CYD |
| `MKS_MINI_12864_V3` | tắt | ExtUI chiếm `MarlinUI::init_lcd()/update()` → không thể vừa có màn gắn trực tiếp (lỗi `multiple definition of MarlinUI::init_lcd()`) |
| `NEOPIXEL_LED` | tắt | PD8 (đèn nền RGB của màn V3) trùng USART3_TX |
| `CUSTOM_MENU_MAIN` | tắt | ExtUI không dùng hệ thống menu của MarlinUI — menu tuỳ biến chuyển sang màn hình CYD (màn **CUSTOM** có đúng 2 nút cũ: `G34 Q99`, `M22`+`M21`) |
| `LED_CONTROL_MENU` | tắt | menu đèn LED thuộc MarlinUI |

Cần thêm trong `ini/stm32f4.ini` (env `mks_monster8`) — **đã có sẵn** trong repo:

```ini
-DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8
```

> 📌 **`CUSTOM_MENU_MAIN` bị tắt ở chế độ màn hình CYD** — đây là thoả hiệp tạm thời. 2 mục menu cũ đã
> chuyển sang màn **CUSTOM** của màn hình CYD, nhưng muốn *thêm mục mới* thì hiện phải sửa code màn hình CYD.
> Kế hoạch làm lại (Marlin đẩy danh sách menu xuống màn hình CYD): **`docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md`**.
> 👉 **Từ 2026-10-13 không cần thoả hiệp này nữa nếu chỉ cần Tramming Wizard** — dùng **kiểu TRAM**
> (§3.1 + §5.6): màn 12864 + toàn bộ menu MarlinUI giữ nguyên.

Số liệu build thật của **kiểu PUSH** (`pio run -e mks_monster8`):

| | Flash | RAM |
|---|---|---|
| Chế độ màn 12864 (mặc định) | 266.536 B (25,4 %) | 12.192 B (9,3 %) |
| Chế độ màn hình CYD `CYDLCD_SERIAL_UI` | 196.444 B (18,7 %) | 11.672 B (8,9 %) |

---

## 4. Giao thức PUSH (frame nhị phân)

Định nghĩa duy nhất ở **`src/cydlcd_protocol.h`** — file này **dùng chung y nguyên** với
`Marlin/src/lcd/extui/cydlcd_serial/cydlcd_protocol.h` (sửa một bên thì copy sang bên kia).

```
[0]=0xA5 [1]=0x5A [2]=type [3]=seq [4]=len [5..]=payload [5+len]=CRC8(poly 0x07)
```

Marlin → màn hình CYD: `HELLO` `STATUS` `TEMPS` `POSITION` `PROGRESS` `MESSAGE` `FANS` `MESH` `FILES`
`EVENT` `SETTING` `PROMPT` `PONG`.
Màn hình CYD → Marlin: `GCODE` (hành động), `REQUEST` (xin file/mesh/settings/FULL), `PING`, `ANSWER`.

Số nguyên little-endian; nhiệt độ = °C×100 (i16), vị trí = mm×1000 (i32), phần trăm = %×100.
Marlin đẩy `STATUS`+`TEMPS`+`POSITION` mỗi 250 ms, `PROGRESS` mỗi 1 s khi đang in, và các sự kiện
ngay khi xảy ra; màn hình CYD gửi `PING` 1 s/lần để hai bên biết còn sống (mất liên kết sau 3 s).

### 4.1 Kiểm thử trên máy tính (không cần board)

```powershell
pwsh cydlcd/tools/run_host_test.ps1
```

Chương trình `tools/test_cydlcd_link.cpp` dùng **đúng** bộ tạo/giải frame trong
`src/cydlcd_protocol.h` và nạp trực tiếp `src/cydlcd_link.cpp` với một UART giả → kiểm luôn
đường nhận + phân tích frame. Hiện có **81 kiểm tra**: HELLO/STATUS/TEMPS/POSITION/PROGRESS/
FANS/SETTING/MESH/FILES/EVENT/PROMPT/ANSWER, PING + mất liên kết, nhóm chịu lỗi (rác trước
frame, CRC sai, `len` quá lớn, tự đồng bộ lại sau lỗi), TRAM, **8b** (hồi quy byte `len` cho
`G28`/`G28 X/Y/Z`) và **8c** (UART kín → frame lệnh bị bỏ **nhưng có đếm**, không im lặng).
Lần chạy đầu đã bắt được **2 lỗi thật** (lệch offset 2 byte của `feed/flow %`, và điều kiện gửi PING sai).

> ⚠️ **2026-10-16 — bộ test này đã CHẾT ÂM suốt từ 2026-10-15** (không build được: `ui_lvgl.h` kéo
> `cydlcd_config.h` vào bản host ⇒ `'Serial' was not declared`; `CYDLCD_HIDE_DIRS` nằm trong khối WiFi;
> và `run_host_test.ps1` để `$ErrorActionPreference='Stop'` quanh `g++` nên **mọi WARNING thành lỗi kết
> thúc**). Hậu quả: lỗi **vòng lặp vô tận** trong `send_frame_wait()` (xem cám bẫy 45 ở README gốc) lọt
> lưới. Nay đã sửa cả 3 chỗ và test chạy lại **81/81**. **Bài học: bộ test không chạy được = không có test.**

### 4.2 Giả lập **UI** trên PC (`cydlcd/sim`) — xem ảnh màn hình mà không cần board

```powershell
pwsh cydlcd/tools/run_sim.ps1                  # mở MỌI trang, chụp ảnh + tự kiểm tra hàng X/Y/Z
pwsh cydlcd/tools/run_sim.ps1 -Script cydlcd/sim/scenarios/test-xyz-dai.txt
pwsh cydlcd/tools/run_sim.ps1 -OldXyz -Script cydlcd/sim/scenarios/test-xyz-dai.txt   # PHẢI báo lỗi (xem dưới)
```

Ảnh `.bmp` ở `cydlcd/sim/out/`. Cách làm: build **đúng** các file `src/ui_*.cpp` của firmware (không vẽ
lại UI) rồi thay lớp phần cứng bằng stub — màn hình = framebuffer 320×240 trong RAM, cảm ứng = hàng đợi
"chạm" do kịch bản bơm vào, thẻ SD/WiFi/NVS = giả, đồng hồ = giả (chạy nhanh hơn thực tế). Chi tiết +
bảng "thật/giả" + ngôn ngữ kịch bản: **`cydlcd/sim/README.md`**.

**Kiểm tra tự động hàng X/Y/Z** (`checkxyz`): đọc chính framebuffer, tách nhóm cột "có mực" trong dải y của
hàng XYZ rồi đo khoảng trống giữa **nhãn trục** và **số** (phải ≥ 3 px). Đã chứng minh bẫy này **thật sự bắt
được lỗi** người dùng báo *"tại trục X = 285.98 thì số `2` đè vào nhãn `X`"*:

| Bố cục | `checkxyz` với `pos -1234.56 …` |
|---|---|
| **Cũ** (giá trị căn phải trong hộp bắt đầu NGAY tại nhãn — `MR_XYZ_VX = MR_XYZ_LX`) | `X: 1 nhóm [14..72] <-- nhan va so DINH LIEN (loi!)`, `Y: … loi!` ⇒ **4 lỗi**, exit code 4 |
| **Mới** (`MR_XYZ_VX` = nhãn + 14; hộp 66/66/68 px — xem §5.1) | `X: [14..24] [30..94] khe = 5 px OK` ⇒ **0 lỗi** |

> 📌 **Phát hiện kèm khi làm giả lập (2026-10-16): 2 env STM32 LVGL `cydlcd_f411cc_lvgl` /
> `cydlcd_f407vgz_lvgl` ĐANG KHÔNG BUILD ĐƯỢC** — và không liên quan gì tới giả lập:
> `cyd_sd.cpp:32` thiếu `CYDLCD_NAME_ALLOW_EXTRA`, `cyd_sd.cpp:1651` thiếu `ELOG`,
> `ui_boxes.cpp:288,296` thiếu `CYDLCD_MSG_BOX_ON_LCD`/`CYDLCD_MSG_TTL_MS`, `ui_lvgl.cpp:1445` dùng
> `ESP.getFreeHeap()`, `ui_overlay.cpp:166` dùng `lv_font_montserrat_20` (font này chỉ được bật trong
> `lv_conf_cydlcd.h` của board CYD), `ui_lang.cpp:29,33` thiếu `NULL`.
> **Đã sửa hết các lỗi thuộc file dùng chung** (macro chuyển ra ngoài khối WiFi, `ELOG` lên trước nhánh
> stub, `ESP.getFreeHeap()` có `#if`, font 20 có fallback 16 cho STM32, thêm `<stddef.h>`) — board CYD
> vẫn build **SUCCESS**. **Còn lại**: `src/main.cpp` của 2 env đó dùng thẳng API ESP32/FreeRTOS
> (`RTC_NOINIT_ATTR`, `vTaskDelay`, `xTaskCreatePinnedToCore`, `pdPASS`…) nên vẫn chưa build được —
> cần một `main.cpp` riêng cho STM32 LVGL (chưa làm; đây là việc riêng, không thuộc phần giả lập).


> **2026-10-13 — thêm nhóm test cho TRAM** (`src/tbt_link.cpp` + `src/tbt_protocol.h`): **+35 kiểm
> tra** (tổng **~75**): định dạng số `tbt_fmt_mm` (0 · 0,15 · số âm · làm tròn), **format → parse
> round-trip** đầy đủ trường, đọc được **dòng tối thiểu** (thiếu 3 trường mở rộng) và **từ chối
> dòng sai**, `tbt_cmd_parse_exact` (không nhận lệnh rác), bộ "sniff" dòng TRAM trên **cùng UART
> với frame PUSH** (byte `T`/`TRAM` trong payload **không** bị ăn mất), và việc **ngừng gửi PING**
> khi đã vào chế độ TRAM.
### Gửi lệnh G-code/M-code từ PC xuống Marlin (2026-10-13)

Hai cách, dùng khi muốn thử lệnh **y như bấm nút trên LCD**:

1. **`serial-console.ps1`** (gốc repo — công cụ khuyến nghị, luôn gửi **CRLF** và bật **DTR**):
   ```powershell
   pwsh -File D:\Marlin\serial-console.ps1                          # qua màn hình CYD (COM7 @115200)
   pwsh -File D:\Marlin\serial-console.ps1 -Port COM4 -Baud 250000  # thẳng vào Marlin (USB)
   ```
   Gõ `G28`, `G35`, `M220 S100`, `G91` → `G1 X10 F600` → `G90`, … ; gõ `exit` để thoát.
2. **Terminal ngoài**: PuTTY (COM7, 115200, 8N1, bật *Implicit CR in every LF*) hoặc PlatformIO
   `pio device monitor -p COM7 -b 115200`. ⚠️ Nếu terminal chỉ gửi `\r` thì các lệnh gõ liên tiếp
   sẽ **dồn thành 1 dòng** (xem README gốc, cạm bẫy 40) — firmware màn hình CYD nay đã chấp nhận **cả `\r`
   và `\n`**, nhưng vẫn nên dùng CRLF.

> ⚠️ **Máy này chưa chạy được `run_host_test.ps1`** (không có `g++`/MinGW trên PATH; script cần
> `pwsh`). Đã kiểm bằng `arm-none-eabi-g++ -fsyntax-only -Wall -Wextra` → **0 lỗi, 0 cảnh báo**;
> muốn chạy thật thì cần cài MinGW (`winget install BrechtSanders.WinLibs.POSIX.UCRT`) rồi chạy
> `pwsh cydlcd/tools/run_host_test.ps1`.

---

## 5. Điều khiển trên màn hình CYD (UI LVGL — **kiểu Marlin**)

> **2026-10-13** (commit `1edfd7aa0b`): UI LVGL làm lại **theo đúng màn TFT ILI9341 của Marlin**
> (`Marlin/src/lcd/tft/ui_320x240.cpp` + `tft_color.h`) và chạy **màn NGANG 320×240**:
> bỏ thanh 8 nút ở đáy, thêm **màn MENU dọc kiểu Marlin**, dùng **icon thật của Marlin**
> (bitmap GREYSCALE4 → LVGL, §5.2) và thêm **hiệu chuẩn cảm ứng 4 góc** cho board có touch (§5.5).

| Thao tác | Việc |
|---|---|
| Xoay encoder (hoặc chạm trên CYD) | di chuyển focus trong màn hình |
| Bấm encoder (hoặc chạm nút) | chọn |
| Nút **BACK** ở **góc phải dòng tiêu đề** | lùi 1 cấp: màn con → MENU → STATUS (màn STATUS — gốc — không có BACK) |

* **Luồng màn hình**: `STATUS` —(nút **SETTINGS** góc phải) → `MENU` —(chọn dòng) → màn con
  —(**BACK**)→ `MENU` —(**BACK**)→ `STATUS`. Trên STATUS các khối cũng là **nút**:
  đầu in/bàn in/quạt → TEMP · hàng XYZ → JOG · SD → FILES · FR/FL → FEED/FLOW · SETTINGS → MENU.
* **Mọi màn trừ STATUS** dùng chung **1 dòng tiêu đề** `[tên màn] …… [LINK / NO LINK] [BACK]`
  (MENU cũng vậy — không còn BACK riêng, không còn tiêu đề trùng).
* **Đã bỏ** thanh 8 nút ở đáy: BACK nằm ở dòng tiêu đề, và STATUS (màn gốc) dùng kín 320×240 như
  màn Marlin. (Dòng *"bấm giữ (>0,7 s) → quay về STATUS"* trong tài liệu bản cũ **không đúng với UI
  LVGL** — bản cũ cũng không có thao tác này; giữ ghi chú ở đây để khỏi đi tìm lại.)

### 5.1 Màn STATUS — bố cục y nguyên màn TFT của Marlin

Toạ độ lấy nguyên từ `Marlin/src/lcd/tft/ui_320x240.cpp` (cùng 320×240), màu theo `tft_color.h`
(nền `#1E156E`, COLD/FAN aqua, HOTEND scarlet, HEATED_BED dark-orange, AXIS_HOMED trắng,
AXIS_NOT_HOMED vàng, RATE_100 xanh lá, RATE_ALTERED vàng, PRINT_TIME aqua, PROGRESS_BAR xanh dương):

| Khối | Vị trí / nội dung |
|---|---|
| 3 ô nhiệt + quạt (64×100) | x = **21 / 127 / 234**: **đầu in (E0)** · **bàn in (BED)** · **quạt (FAN)** — target ở y=8, **icon 64×64** ở y=18 (quạt y=10), nhiệt độ hiện tại ở y=72; đổi màu + đổi icon theo nhiệt độ/tốc độ |
| Hàng **XYZ** | khung (4, 103, 312, 24); chưa home → hiện `?` màu vàng (`AXIS_NOT_HOMED`), khung đổi màu theo liên kết. **Nhãn trục** ở x = 10 / 127 / 219 (đúng Marlin), **vùng giá trị** ở 24..90 / 141..207 / 233..301 (rộng 66/66/68 px) — xem ghi chú bên dưới |
| **FR / FL** + **icon WiFi** | hàng giữa, **y 136..167**. `FR` (54, 136, 68×32) · **icon WiFi 30×30** (128, 139) + **số dBm `-43`** (161, 143) · `FL` (192, 136, 68×32) — icon 32×32 + `%` ở x=32. **Icon WiFi to ĐÚNG BẰNG icon FR/FL** (font icon bản to 30 px) và nằm **giữa 2 ô tốc độ in/flow**; bấm icon → mở trang WIFI. Màu theo trạng thái: **xanh** = đã kết nối, **đỏ** = đang kết nối (`--`), **xám** = WiFi tắt (chu rỗng) |
| **SD / SETTINGS** | 2 nút **icon 48×48** ở (0, 138) và (272, 138) — **đã thu nhỏ từ 64×64** của Marlin để nhường chỗ cho icon WiFi ở giữa (bitmap 48×48 **sinh sẵn** bằng `tools/make_lvgl_icons.py`, KHÔNG zoom lúc chạy — xem ghi chú bên dưới) |
| Thời gian in | (96, 176, 128, 20) — `đã in / còn lại` |
| Thanh tiến độ | (4, 198, 312, 9) + `%` ở (226, 176) |
| Dòng trạng thái | (0, 216, 320, 20) — `STATE  tên file` hoặc `NO LINK` |

> ⚠️ **Nhãn trục và số toạ độ KHÔNG được dùng chung một hộp (sửa 2026-10-16).** Người dùng báo:
> *"tại trục X = 285.98 thì số `2` đè vào nhãn `X`"*. Bố cục Marlin gốc (`ui_320x240.cpp:271-279`) vẽ nhãn
> ở x = 10/127/219 rồi **căn phải số vào mốc 68/185/301** — nhưng Marlin dùng font TFT 8×12 **hẹp**
> (~6 px/ký tự) nên 6 ký tự chỉ ~36 px. Panel dùng **Montserrat 16** (theo yêu cầu *"font X/Y/Z to hơn"*
> 2026-10-15) với bề rộng thật (đo từ `lv_font_montserrat_16.c`, `adv_w`/16): `'2'`=9,19 · `'8'`=10,31 ·
> `'.'`=3,62 · `'-'`=6,12 · `'X'`=10,75 px ⇒ `285.98` = **52,5 px** (dài nhất `-285.98` = 58,6 px) mà hộp
> chỉ 58 px **lại bắt đầu ngay tại nhãn** ⇒ số bắt đầu ở x = 68 − 52,5 = 15,5 ⇒ **đè lên `X`** (10..20,75).
> Cách sửa: giữ nguyên nhãn, **tách vùng giá trị ra sau nhãn** (`MR_XYZ_VX` = nhãn + 14) và nới rộng sang
> phải: **X 24..90 · Y 141..207 · Z 233..301** (66/66/68 px) — đủ cho cả `-285.98` và không chạm nhãn kế tiếp.
>
> ⚠️ **Vì sao 2 ô thẻ/SETTINGS phải thu nhỏ, và vì sao KHÔNG dùng `lv_img_set_zoom()`:** hàng giữa của
> Marlin là `[SD 64] [FR 80] [FL 80] [SET 64]` = **320 px kín**, khe giữa FR và FL chỉ **20 px** — không
> đủ cho icon 30 px + số dBm. Phải nhường: 2 ô thẻ 64→**48** và 2 khối FR/FL 80→**68** (giá trị `100%`
> = 4 ký tự × 8 px = 32 px vẫn vừa). Icon thẻ/SETTINGS của Marlin là **ALPHA_4BIT**, mà LVGL 8.4
> **chỉ transform (zoom/xoay) được TRUE_COLOR\*/RGB565A8** (`lv_draw_sw_transform.c` đưa ALPHA_4BIT vào
> `default:` ⇒ **không vẽ gì**) — nên nếu zoom thì 2 icon đó **biến mất**; cách đúng là **sinh sẵn bitmap
> 48×48** (`tools/make_lvgl_icons.py`, mục `SMALL`, lọc trung bình theo diện tích).

### 5.2 Icon thật của Marlin (`src/ui_lvgl_icons.h`)

Icon **không còn** là hình vẽ lại bằng LVGL: `cydlcd/src/ui_lvgl_icons.h` chứa **12 icon
64×64 / 48×48 / 32×32 chuyển nguyên từ bitmap GREYSCALE4 của Marlin** (`Marlin/src/lcd/tft/images/*.cpp`)
sang `LV_IMG_CF_ALPHA_4BIT`, rồi **đổi màu lúc chạy** bằng `lv_obj_set_style_img_recolor()` +
`recolor_opa = LV_OPA_COVER`.

| Nguồn trong Marlin | Icon xuất ra | Dùng ở |
|---|---|---|
| `hotend_64x64x4.cpp` | `ui_icon_hotend` | ô đầu in (STATUS) |
| `heated_bed_64x64x4.cpp` | `ui_icon_bed`, `ui_icon_bed_heated` | ô bàn in — tắt / đang nung (target > 0) |
| `fan_64x64x4.cpp`, `fan_slow_64x64x4.cpp`, `fan_fast_64x64x4.cpp` | `ui_icon_fan`, `ui_icon_fan_slow`, `ui_icon_fan_fast` | ô quạt — theo tốc độ (0 / <50 % / ≥50 %) |
| `sd_64x64x4.cpp` | `ui_icon_sd` (+ `ui_icon_sd_48`) | nút SD (STATUS dùng bản **48×48** — xem §5.1) |
| `settings_64x64x4.cpp` | `ui_icon_settings` (+ `ui_icon_settings_48`) | nút SETTINGS (STATUS dùng bản **48×48**) |
| `feedrate_32x32x4.cpp`, `flowrate_32x32x4.cpp` | `ui_icon_feedrate`, `ui_icon_flowrate` | FR/FL trên STATUS + hàng FEED/FLOW |

**Sinh lại icon** (chạy từ **thư mục gốc repo**):

```powershell
python cydlcd/tools/make_lvgl_icons.py
# -> Da ghi D:\Marlin\cydlcd\src\ui_lvgl_icons.h (12 icon, 19712 byte du lieu)
```

* Cần **Python 3** — script chỉ dùng `re` / `sys` / `pathlib`, **không cần Pillow** (khác
  `tools/make_font.py`), và cần **nguồn Marlin** có sẵn ở `Marlin/src/lcd/tft/images/`.
* Kết quả ~**19,3 KB** dữ liệu icon, nằm trong flash của màn hình CYD.
* **KHÔNG sửa tay** `ui_lvgl_icons.h` — chạy lại script là **ghi đè**. Muốn thêm/bớt/đổi icon thì
  sửa bảng `ICONS` (icon nguyên gốc) hoặc `SMALL` (bản thu nhỏ cho STATUS — khai báo thêm chiều rộng
  đích; hiện dùng cho `sd`/`settings` → 48×48) trong `tools/make_lvgl_icons.py` rồi chạy lại.
* Bản thu nhỏ dùng **lọc trung bình theo diện tích** (`downsample()`), KHÔNG phải nearest — để nét
  icon mảnh (khe thẻ, răng bánh răng) không bị răng cưa. **Không dùng `lv_img_set_zoom()`** được:
  LVGL 8.4 chỉ transform (zoom/xoay) được `TRUE_COLOR*`/`RGB565A8`, còn `ALPHA_4BIT` rơi vào
  `default:` của `lv_draw_sw_transform.c` ⇒ **không vẽ gì** (icon biến mất).
* Đầu file sinh ra có sẵn dòng cảnh báo `FILE NAY DUOC SINH TU DONG -- dung sua tay`.
* Bảng đổi mức xám: `level ≤ 8` (nền + lớp bóng của Marlin) → **trong suốt**; `9…15` → alpha
  `(level−8)×15/7`, nên icon hiện **giống hệt bản Marlin** khi `recolor` = màu chính và nền =
  `COLOR_BACKGROUND`.
* Cần **`#define LV_USE_IMG 1`** trong `src/lv_conf.h` (đã bật ở commit này; trước đây là `0`).

### 5.3 Các màn

| Màn | Nội dung / lệnh gửi |
|---|---|
| **STATUS** | bố cục y nguyên Marlin — §5.1 (màn gốc, không có BACK) |
| **MENU** | menu dọc kiểu Marlin: Temperature · **Motion** · Feed/Flow rate · **Print from Media** · Bed mesh · **Custom/Init SD** · **Configuration** · Info; cột phải mỗi dòng là **giá trị tóm tắt** (nhiệt E0, X/Y/Z đã home, feed/flow %, cỡ mesh, `n muc`, LINK…). Dòng **`Motion`** mở **menu MOTION** kiểu 12864 (Move Axis (JOG) · Z Probe Wizard · Tramming Wizard · Unified Bed Leveling · Level Bed · Deploy/Stow Z-Probe · Endstops (M119) · M48 Probe Test · Disable Steppers — trước 2026-10-15 dòng này tên `Motion (JOG)` và mở thẳng trang JOG; `Homing` ẩn 2026-10-15, `Auto Z-Align (G34)` ẩn vì G34 tắt hẳn, `Fade Height` gom về CONFIGURATION). Dòng **`Configuration`** mở **cây menu kiểu 12864** (xem mục *"Menu CONFIGURATION kiểu 12864"* ở cuối file) — trước 2026-10-15 dòng này tên `Settings` và mở thẳng bảng phẳng. Dòng **`Print from Media`** (2026-10-15: đổi tên từ `SD card` theo yêu cầu người dùng — *"thay text sd card ở menu thành 'Print from media' giống marlin"*; lấy đúng chuỗi của Marlin `MSG_MEDIA_MENU = "Print from " MEDIA_TYPE_EN` = **`Print from Media`**) **KHÔNG hiện số file** (yêu cầu người dùng 2026-10-15: *"bỏ phần đếm số lượng file ở menu file"* — trước là `12 file`, nay để trống như mục của Marlin: thẻ đọc được hay không đã có **icon thẻ** ở thanh trên, còn danh sách thì thấy ngay khi mở trang) |
| **FEED/FLOW** | chỉnh feed/flow bằng **`M220 S…` / `M221 S…`** (nút −10/−1/+1/+10, giới hạn 10…999 %, gửi ngay khi bấm); nút `RESET 100%` → `M220 S100` + `M221 S100`. **Tự xin lại trạng thái (2026-10-15):** FR/FL lấy từ gói `SETTING` mà Marlin **chỉ gửi khi được hỏi** (`CYDLCD_REQ_SETTINGS`) — trang hoi **ngay khi mở** và **định kỳ 2 s** trong lúc đang mở, nên giá trị đổi từ **chỗ khác** (console COM7, `M220/M221` trong G-code bản in…) cũng hiện đúng ngay (trước đây nhãn đứng yên ở giá trị cũ cho tới khi rời trang rồi vào lại) |
| **TEMP** | +/- 1 và 10 °C cho nozzle & bed → `M104 S…` / `M140 S…`; **hàng FAN** → `M106 S0…255`; `COOLDOWN` → `M104 S0` + `M140 S0` |
| **JOG** | chọn trục X/Y/Z, ±0.1/±1/±10 mm (`G91` → `G1 <trục><d> F600` → `G90`), `HOME ALL (G28)` |
| **FILES** | danh sách file từ SD (Marlin đẩy xuống): **chạm 1 dòng = CHỌN + tô sáng dòng đó — chỉ nút `PRINT` mới in** (trước đây chạm vào danh sách là in luôn: hợp với encoder nhưng trên màn cảm ứng thì rất dễ in nhầm); `PRINT` → `M23` + `M24`; `DIR` = vào thư mục; `UP`/`REFRESH` gửi `REQUEST`; PAUSE `M25`, RESUME `M24`, STOP `M524`. **Bố cục (2026-10-15, vòng 2):** danh sách là **4 dòng cố định (21 px/dòng — tăng từ 18 px khi font mono lên size 15, xem mục *Font chữ*; mỗi dòng 1 nút)** + **3 hàng nút được đẩy xuống SÁT ĐÁY** bằng `mk_spacer(grow)`; **KHÔNG cuộn** — lật trang bằng **`PREV`/`NEXT`** ở hàng nút cuối (kèm `T x/y` + **tên mục đang chọn** ở dòng info, nút nào hết trang thì **khoá thật** bằng `LV_STATE_DISABLED`: `lv_indev.c:412` chặn chạm, `lv_group.c:434` chặn encoder). **Vì sao bỏ `lv_roller` cũ:** (1) cuộn bằng ngón tay rất khó — phải kéo đúng cự ly mới đổi được dòng; (2) **lỗi nặng hơn:** chạm vào 1 dòng roller **không** làm `lv_roller_get_selected()` đổi theo dòng vừa chạm (roller chỉ đổi khi kéo rồi thắng lại) ⇒ `files_selected()` trả về dòng chọn **cũ** ⇒ bấm `XOA` trên 1 file lại đi xoá **thư mục** đang chọn trước đó (triệu chứng: *"chọn 1 file mà báo Không xoá thư mục"*). Nay mỗi dòng là 1 nút giữ **chỉ số tuyệt đối** `s_sel` + **tên** (`s_sel_name`, để giữ lựa chọn qua lần `REFRESH`/xoá xong vì thứ tự file do FAT quyết định). **Nút `XOA` xoá CẢ file LẪN thư mục** (2026-10-15, vòng 3), luôn hỏi xác nhận **2 bước** (xem §8b cho hộp thoại): **file** → `net_sd_delete_file()` (`SD.remove()` = FATFS `f_unlink`) hoặc lùi về `M30`; **thư mục** → `net_sd_delete_dir()` = **`SD.rmdir()`** và **chỉ khi thư mục RỖNG** (FATFS `rmdir` không xoá được thư mục còn mục con) — còn mục con thì chỉ hiện ghi chú `Thu muc con file - xoa file ben trong truoc`. ⚠️ **KHÔNG dùng `M30` cho thư mục**: `M30` → `CardReader::removeFile()` → `SdBaseFile::remove()` → `truncate(0)` không xoá được thư mục (chính Marlin ghi trong `SdBaseFile.cpp`), nên đường thư mục **chỉ chạy khi thẻ ở khe màn hình CYD** (§12). ⚠️ `SD.remove()` của ESP32 **từ chối thư mục** (`vfs_api.cpp`: *"does not exists or is directory"*) — đã gặp thật: xoá thư mục rỗng mà báo "Khong xoa duoc"; **phải** dùng `SD.rmdir()`. **Đã kiểm end-to-end (COM7):** chọn `cydlcd_tmp` → `XOA` → OK ×2 → log `[UP] da xoa thu muc RONG /cydlcd_tmp`, danh sách còn 6 mục, không reboot (`cydlcd_tmp` là thư mục tạm của màn hình, `cyd_sd.cpp` **tự tạo lại** khi cần upload). **KHOÁ NÚT THEO TRẠNG THÁI MÁY (2026-10-15, vòng 4):** `PRINT` và `XOA` bị **khoá thật** (`LV_STATE_DISABLED`) khi **đang in / tạm dừng** (`files_actions_mark()`); `PRINT` còn bị khoá khi mục đang chọn **không phải file G-code** (`.gcode/.gco/.gc/.g`) hoặc là **thư mục** — trước đây bấm PRINT không thấy gì và cũng không có phản hồi nào. Đã kiểm: đang in → `PRINT=khoa XOA=khoa | DANG IN`; sau khi huỷ → `PRINT=mo XOA=mo`. **GỬI LỆNH BẰNG TÊN 8.3 (proto 4, §12.4):** `M23`/`changeDir` dùng **tên 8.3** (`sfn`) chứ không dùng tên dài, vì SdFat của Marlin chỉ giữ LFN trong **27 byte** (tên > 26 ký tự bị cắt, mất cả phần mở rộng ⇒ `M23 <tên dài>` **không bao giờ mở được file** ⇒ bấm PRINT không in). **Đã kiểm:** `M23 V300_C~1.GCO` mở đúng `V300_case-esp-2432s208_no-text.gcode` (36 ký tự) và `M23 V300__~1.GC` mở `V300_[a]_xy_joint_cable_bridge_3hole.gc` (39 ký tự) → Marlin `st=1`, `fl=0x26/0xA7` (bit `SD_PRINTING`), `file="..."`. **THƯ MỤC HỆ THỐNG CHỈ BỊ ẨN KHI HIỂN THỊ (2026-10-15, vòng 5):** `cydlcd_tmp` (thư mục tạm của **chính màn hình** — `cyd_sd.cpp` tự tạo) và `BTUP` (thư mục cũ của đường upload **Bluetooth** đã xoá 2026-10-14) nằm trong `CYDLCD_HIDE_DIRS` (`cydlcd_config.h`). ⚠️ **Danh sách Marlin gửi xuống VẪN GIỮ NGUYÊN ĐỦ** (`cydlcd.files[]` + `cydlcd.n_files`, đúng chỉ số như Marlin — Marlin vẫn dùng danh sách đó để in/xoá, ẩn đi là hỏng): màn hình chỉ **ẩn lúc HIỂN THỊ** và **sắp xếp lại** bằng một **VIEW** riêng (`s_view[]`, `files_view_build()` trong `ui_page_files.cpp`; `s_sel` luôn là chỉ số THẬT). **Sắp xếp: THƯ MỤC trước, rồi theo TÊN** (không phân biệt hoa/thường) — nhờ vậy thứ tự **ỔN ĐỊNH**: thêm/bớt 1 file (upload/xoá xong) thì các dòng cũ **không nhảy chỗ**, người dùng không chạm nhầm (thứ tự FAT/Marlin thì mỗi lần đổi danh sách là xáo). Đã kiểm: `UI TAP` in `5/6 muc hien thi (Marlin gui 6)` + dòng `(an) muc #2 <<cydlcd_tmp>> ... van co trong danh sach Marlin`. **Tên file**: ưu tiên tên dài (LFN), **thiếu LFN thì lùi về tên 8.3** (`cydlcd_serial.cpp`); entry không có tên nào vẫn hiện `(khong ten)` chứ không để dòng trống; dòng được **căn trái** và **cắt bằng `...`** khi quá dài; **tên dài bị cắt quá 24 ký tự thì nối thêm phần mở rộng lấy từ tên 8.3** (vd `V300_case-esp-2432s208_no-.GCO`) để biết đó là file in được. Mục `.` và `..` của FAT **không** xuất hiện (`SdBaseFile::readDir()` đã bỏ qua) |
| **SETTINGS** (`THONG SO`) | bảng phẳng **76 dòng** (steps/mm · max feed · max accel · jerk · PID · probe offset · MPC · TMC · M710…); `SAVE EEPROM` → `CYDLCD_REQ_SAVE_SETTINGS`. Từ 2026-10-15 **không còn là màn chính của cấu hình**: vào bằng `Configuration` → `Advanced Settings` → `All settings (table)` (xem mục *"Menu CONFIGURATION kiểu 12864"*) |
| **MESH** | lưới bed mesh (7×7 UBL) đọc từ Marlin, ô cao/thấp tô màu; `REFRESH` → `CYDLCD_REQ_MESH` |
| **CUSTOM** | `Init SD` (`M22`+`M21`) · `ENDSTOP` (trang M119) · `M600 B0` (đổi filament) · `M412` (cảm biến hết filament ON/OFF) · `REBOOT DFU` (2 bước xác nhận). 🆕 Nút `G34 Bed` **đã bỏ** 2026-10-15 vì G34 bị tắt hẳn trên máy này — xem mục *"TẮT HẲN G34"* |
| **TRAMMING** | Bed Tramming Wizard của Marlin (kiểu TRAM, **không cần ExtUI**): 4 số của 4 góc, dấu `*` góc đang đứng, dòng hướng dẫn `CW/CCW…`/`DAT`, mode `MAX`/`MIN`, 5 nút **HOME+PROBE · PROBE · MAX/MIN · BACK · DONE** → xem **§5.6** |

> 🆕 **Cập nhật 2026-10-13 (UI LVGL sau khi chuyển sang PUSH):**
> * **TEMP** có **2 hàng nút PREHEAT** (tối đa **6 vật liệu**, mỗi nút gửi `M104 S<nozzle>` + `M140 S<bed>`,
>   thêm `M106 S<fan>` **chỉ khi** fan > 0) + **`COOLDOWN`** (`M104 S0` + `M140 S0`). 🆕 **Từ 2026-10-15 danh
>   sách vật liệu do người dùng tự thêm/bớt/sửa** (NVS `ph2`, xem **§9.22**): nút chỉ hiện **tên** vật liệu,
>   bấm xong dòng đầu hiện `Profile: <tên> <nozzle>/<bed>`; **hàng nút `TUNING (MPC/PID)` đã bỏ** để lấy chỗ
>   (tuning MPC/PID nay nằm ở **Configuration → Advanced Settings → Temperature** — mục `Tuning (MPC/PID)`
>   trong MENU và cả trang đó **đã bỏ hẳn** 2026-10-15).
> * **SETTINGS**: bảng **chỉ vẽ những dòng Marlin thực sự gửi giá trị** (bỏ `Junction dev` / `Linear adv` /
>   PID khi build không dùng) và **sửa được tại chỗ**: chạm một dòng → hiện lớp phủ có **4 nút bước
>   `0.01` · `0.1` · `1` · `10`** (nút đang chọn được tô nổi) + `-` / `+` (giữ nút = nhảy nhanh) + `OK` /
>   `HUY` → `OK` gửi M-code tương ứng (`M92` steps · `M203` max feed · `M201` max accel ·
>   `M205` jerk · `M851` probe offset · `M220/M221` feed/flow · `M304` PID bàn · `M104/M140/M106`
>   nhiệt & quạt) rồi tự xin lại thông số. Hàng nút ở đáy: **`UP` · `DOWN` · `SAVE EEPROM`** (SAVE = `M500`).
>   ⚠️ **KHÔNG còn dòng `Z offset mm`** (nó trùng `Probe Z mm` vì cùng là `probe.offset.z`, và từng gửi
>   nhầm `M206 Z` = home offset) — chỉ dùng **`Probe Z mm` → `M851 Z`**.
> * **Chữ tiếng Việt có dấu**: chỉ có ở **chế độ font 4** (`UI_MONO_FONT 4` — mặc định): font mono
>   **tự sinh** `gfx4_font_robotomono` (dải `0x1EA0-0x1EF9` + `Ă ă Đ đ Ơ ơ Ư ư`) qua driver
>   `lv_font_gfxfont` — xem `tools/ttf2gfxfont.py`. Các font sinh bằng `lv_font_conv` **đã xoá khỏi
>   repo**; ở chế độ 0 chỉ còn Montserrat của LVGL nên **không hiện được dấu**.
> * **FEED/FLOW**: sau mỗi lần `+/-` (gửi `M220`/`M221`) màn hình CYD **tự xin lại thông số** nên con số trên màn
>   cập nhật ngay (trước đây bị đứng ở 100%).
> * **JOG**: X/Y nhích ở **`F3000` = 50 mm/s** (bằng tốc độ home), Z giữ **`F600` = 10 mm/s** (trần `M203 Z10`).
| **INFO** | 11 dong thong tin that (FW/BOARD/UI/FONT/WIFI/PROTO/MARLIN/LINK/RX-ERR/SD+SYS); khong con nut REFRESH FULL |

* **Prompt của Marlin** (hết filament, xác nhận…) hiện hộp thoại OK/CANCEL; OK gửi `ANSWER`
  → Marlin gọi `ExtUI::setUserConfirmed()`. Thông báo (`cydlcd.message`) tự đóng sau 6 s.
* Chữ trên màn hình là **ASCII** (font `montserrat_14` không có dấu tiếng Việt) — đó là lý do
  nhãn màn hình viết không dấu (`NHIET DO`, `THONG SO`…).

### 5.4 Encoder đọc bằng ngắt (`src/encoder.cpp`)

Màn hình CYD **không polling** encoder: hai chân A/B ngắt đổi trạng thái, ISR chỉ đẩy mẫu 2 bit vào một
**ring cố định 32 phần tử** (không heap, không STL), rồi luồng chính giải mã bằng bảng chuyển tiếp:
chuyển tiếp **1 bit** = cạnh thật → ±1 xung (đủ `ENC_PULSES_PER_DETENT` = 4 xung = 1 nấc);
chuyển tiếp **2 bit** (dội phím / cạnh đến lúc ring đầy) → **không đếm**, chỉ đồng bộ lại;
quay quá nhanh trong một lần đọc → **kẹp ở `ENC_MAX_DETENTS`** (3).

Lý do: mỗi lần vẽ một khối lớn (ví dụ `fillScreen` 320×240 ≈ 150 KB qua SPI) mất hàng chục ms —
trong khoảng đó polling **không đọc được chân** nên chắc chắn mất nấc.

### 5.5 Hiệu chuẩn cảm ứng 4 góc (chỉ board có touch — CYD)

Cảm ứng XPT2046 trên CYD do màn hình CYD **tự bit-bang SPI mềm** (TFT_eSPI chỉ đọc được XPT2046 khi nó
chung bus SPI với LCD, mà CYD đi dây riêng), nên hiệu chuẩn nằm **trong firmware màn hình CYD**
(`ui_port_touch_calibrate()` — `src/ui_port_cyd_st7789.cpp`).

**Cách vào** (2 đường, không cần build lại):

1. Trên STATUS bấm **SETTINGS** → **MENU** → dòng **`Calibrate touch (giu 1.5s)`** — **PHẢI GIỮ TAY
   1,5 giây rồi NHẤC** mới vào; chạm ngắn (bấm nhầm) **không làm gì** (cột phải đổi `dang giu` →
   `NHAC TAY` để biết đã đủ lâu). Lúc rảnh cột phải ghi `NVS` nếu đã có hiệu chuẩn lưu trong flash,
   `mac dinh` nếu đang dùng giá trị trong `cydlcd_config.h`.
   > **Vì sao phải giữ:** trước đây chỉ cần 1 lần chạm là vào, mà ở **mỗi góc cũng chỉ cần 1 lần chạm**
   > là được nhận và **ghi ngay vào NVS** (`touch_cal_save()` trước khi hiện kết quả) ⇒ lỡ tay trong
   > MENU là mất hiệu chuẩn, phải giữ tay lúc cấp điện mới vào lại được.
2. **Giữ tay trên màn hình lúc cấp điện** — màn hình CYD phát hiện `T_IRQ` mức thấp trong
   `ui_port_input_init()` và vào thẳng chế độ hiệu chuẩn. Đây là **"thoát hiểm"** khi giá trị cũ
   quá tệ, không chạm trúng được dòng menu.

> Dòng `Calibrate touch` **chỉ hiện trên board có touch** (`ui_port_touch_cal_available()`);
> board STM32 (chỉ encoder) không có dòng này và hàm hiệu chuẩn là hàm rỗng.

**Cách làm** (làm lần lượt **TRÊN-TRÁI → TRÊN-PHẢI → DƯỚI-PHẢI → DƯỚI-TRÁI**):

1. Màn hình đen hiện **tên góc** cần hiệu chuẩn + `CHAM + KEO SAT GOC NAY / ROI NHAC TAY`.
2. Chạm vào màn hình → **CẢ MÀN HÌNH ĐỔI SANG XANH** = đã nhận (màn xanh cũng là dấu "đang lấy mẫu").
3. **Kéo ngón tay/bút sát vào đúng góc đó**, rồi **nhấc tay** — lúc **mất cảm ứng** chính là lúc
   chốt toạ độ: firmware lấy **cực trị (min/max)** ghi được trong cả quá trình kéo (bỏ qua mẫu
   nhảy > 500 khi nhấc tay). Cần ≥ 5 mẫu, không đủ thì màn đỏ `QUA NGAN / NHIEU` và làm lại (tối đa
   3 lần/góc, chờ tối đa 60 s mỗi lần chạm).
4. Từ **4 góc**, firmware **tự suy ra** `SWAP_XY`, `INVERT_X/Y` và `XMIN/XMAX`, `YMIN/YMAX` rồi hiện
   màn `DA LUU` (kèm 4 cặp raw của 4 góc + `cham de tiep tuc`).

**Lưu ở đâu**: **NVS (flash) của ESP32** qua `Preferences`, namespace **`cydtouch`**, khoá
`xmin/xmax/ymin/ymax/swap/invx/invy` (`touch_cal_save()`); lúc khởi động `touch_cal_load()` nạp lại
và in `[TOUCHCAL] nap tu NVS: …`. ⇒ **Reset/mất điện không mất hiệu chuẩn, không phải build lại**
(`erase_flash` mới xoá). Khi chưa có NVS thì dùng **giá trị mặc định trong `src/cydlcd_config.h`**:
`CYDLCD_CYD_TOUCH_SWAP_XY 0`, `INVERT_X/Y 0`, `X 184..3866`, `Y 185..3910` — đây là **kết quả
hiệu chuẩn thật** đã chạy tốt trên board (dòng MENU hiện `NVS`/`mac dinh` để biết đang dùng bộ nào).

**Khi nào cần làm lại**: đổi/thay tấm cảm ứng hoặc board, chạm lệch rõ ở **giữa** màn hình dù 4
góc đúng, hoặc sau khi `erase_flash`. Không cần làm lại sau mỗi lần nạp firmware thường.

**Đọc thêm khi hiệu chuẩn**: bật `-DCYDLCD_CYD_DEBUG=1` (env `cydlcd_esp32`, **đang bật tạm**) →
Serial 115200 in `[TOUCH] raw=… -> px=…` (mỗi 150 ms khi chạm), `[CAL] 1:raw=…, …` (5 cụm chạm gần
nhất, in 1 Hz — chạm 5 điểm theo thứ tự bất kỳ rồi đọc lại lúc nào cũng được),
`[TOUCHCAL]` khi lưu/nạp. Ngoài ra cờ này còn in reset reason, dung lượng heap, chân đèn nền và
heartbeat 1 s (xem `src/main.cpp`). **Nhớ bỏ cờ khi dùng thật.**

> ⚠️ **Sửa lỗi cũ**: bản trước ghi `raw_x` là trục DỌC (comment trong code bị ngược 2 kênh) nên
> chạm bị **leo trục**. Đúng ra **`raw_x` = trục NGANG** (trái 184 → phải 3866), **`raw_y` = trục
> DỌC** (trên 185 → dưới 3910), **cả hai đều TĂNG** theo hướng phải/xuống ⇒ không đảo trục.
> Cách "chấm 4 góc bằng mắt" trước đây chỉ ra `332..3645 / 453..3658` (hụt vào 150–250 đơn vị ở
> mỗi mép → lệch ở giữa màn hình); cách **kéo tới lúc mất cảm ứng** mới lấy đúng mép vật lý.

### 5.6 Màn TRAMMING — Bed Tramming Wizard (kiểu TRAM, **không cần ExtUI**)

> **2026-10-13**: màn mới cho màn hình riêng, dùng **chung thuật toán** với wizard trên màn 12864
> (`Marlin/src/feature/tbt_run.cpp`). Phía Marlin chỉ cần **1 dòng** `#define CYDLCD_TBT_LINK`
> — **KHÔNG** phải tắt `MKS_MINI_12864_V3` / `CUSTOM_MENU_MAIN` / `LED_CONTROL_MENU`.

**Vào màn:** STATUS → **SETTINGS** → MENU → dòng **`Tramming wizard`** (cột phải ghi `chua bat` /
`san sang` / `MAX PROBING` / `mat link`).

| Hàng | Nội dung |
|---|---|
| 1 | `MAX  IDLE  goc 2` (mode · phase · góc nozzle đang đứng) + `LINK OK` / `NO LINK` bên phải |
| 2–3 | **4 góc, 2 góc một dòng**: ` BR  -0.012   BL  +0.021` — `*` = góc đang đứng, mm so với **mốc**; `---` = góc chưa probe được |
| 4 | **hướng dẫn vặn vít** do **Marlin** gửi: `CW 0.150` / `CCW nhe` / `DAT` / `PROBE` / `ERR` |
| 5 | nút **`HOME + PROBE 4 GOC`** (rộng cả dòng) |
| 6 | **4 nút**: `PROBE` · `MAX/MIN` · `BACK` · `DONE` |
| 7 | chú thích `DONE = goc xong \| MAX/MIN = doi moc` |

**Nút `HOME + PROBE 4 GOC` gửi `R`** = home + probe lại **cả 4 góc** và **là nút khởi động**: nếu
wizard chưa chạy thì `R` gọi `tramming_run::begin()`, đang chạy thì `restart()`. Vì vậy màn
TRAMMING trên màn hình CYD **dùng được độc lập**, không cần mở wizard trên màn 12864 trước. Dòng 1 hiện
`READY (bam HOME + PROBE)` khi wizard chưa chạy.

> 🔀 **2026-10-13 — CHUYỂN SANG PUSH (ExtUI `CYDLCD_SERIAL_UI`)**: để màn hình CYD có **đủ dữ liệu** (nhiệt độ,
> X/Y/Z, tiến độ + tên file, quạt, lưới bed mesh, danh sách file SD, prompt) thay vì chỉ dòng `TRAM`,
> dự án đã bật PUSH và **tắt** `MKS_MINI_12864_V3` + `NEOPIXEL_LED` + `CUSTOM_MENU_MAIN` +
> `LED_CONTROL_MENU` (`cydlcd/tools/set_cydlcd_mode.ps1 -Mode on`), đồng thời **comment**
> `CYDLCD_TBT_LINK` (2 chế độ loại trừ nhau: cùng 1 UART).
> **Màn TRAMMING trên màn hình CYD vẫn chạy**: trạng thái wizard được Marlin gửi trong frame
> **`CYDLCD_F_TBT` (0x0E)** — payload là **1 dòng ASCII đúng format `tbt_protocol.h`** (tái dùng
> nguyên bộ giải `tbt_status_parse()`), và lệnh của màn hình CYD đi trong frame **`CYDLCD_F_TBT_CMD` (0x44)**
> (`u8` ký tự `P/D/M/R/B/Q`) → `tramming_report_cmd()` phía Marlin. Nhờ vậy **không mất** wizard khi
> đổi giao thức. Code chế độ TRAM cũ **vẫn nằm trong repo** (bật lại bằng `set_cydlcd_mode.ps1 -Mode off`
> + `#define CYDLCD_TBT_LINK`), màn hình CYD tự nhận cả 2 đường (`cydlcd.linked` → gửi frame; ngược lại → ASCII thô).
>
> 🐞 **LỖI ĐÃ SỬA 2026-10-15 — "nút preheat không cập nhật nhiệt độ bàn in cần đạt"**: 4 token nhiệt độ
> ở **cuối** dòng TBT (`... t_e0 t_bed t_e0_tgt t_bed_tgt\n`) — token **cuối cùng dính liền `\n`** — mà
> `tbt_next_tok()` chỉ coi `' '`/`'\t'`/`'\r'` là dấu phân cách ⇒ token cuối là `"6000\n"` ⇒
> `tbt_tok_uint()` gặp `\n` trả `-1` ⇒ `tbt_status_parse()` **`break`** ⇒ **`t_bed_tgt` LUÔN = 0**.
> Vì `temp_lookup()` (ui_widgets.cpp) **ưu tiên nguồn TRAM khi `tbt.ever`**, màn hình hiện
> `BED <nhiệt> / 0 C` dù Marlin **đã** đặt target 60 (frame PUSH báo `tgt=6000`).
> **Sửa 2 chỗ** (đo trên máy thật bằng `UI TEMPS`): (1) `tbt_next_tok()` thêm `'\n'` vào bộ dấu phân
> cách — **ở CẢ HAI bản** `cydlcd/src/tbt_protocol.h` + `Marlin/src/feature/tbt_protocol.h`;
> (2) `temp_lookup()` **ưu tiên frame PUSH** (`cydlcd.temps[]`) và chỉ lùi về dòng TBT khi không có
> (chế độ TRAM) ⇒ nguồn hiển thị luôn là nguồn đầy đủ/cập nhật nhất. Thêm test hồi quy
> (`cydlcd/tools/test_cydlcd_link.cpp`: `p.t_e0/t_bed/t_e0_tgt/t_bed_tgt` phải bằng đúng giá trị đã
> format — chạy khi có `g++`).
> Đo trước/sau trên máy thật: trước `PUSH id=0 tgt=0 | TRAM BED 5393/0`; sau khi bấm `PLA` →
> `PUSH ... id=0 tgt=6000 | TRAM BED 5322/6000 | HIỂN THỊ BED 5322/6000` (= **53.2 / 60 C**) ✓.

**Chỉ báo link — chính sách hiển thị (2026-10-13, theo yêu cầu: gộp về header):**
| Chỗ hiển thị | Nội dung | Ghi chú |
|---|---|---|
| **Thanh trên cùng** (mọi màn) | `LINK OK` / `LINK ERR` / `NO LINK` | **NƠI DUY NHẤT** hiện chỉ báo link — `link_state_now()`: **1** = đọc được dòng `TRAM` hợp lệ (`cydlcd.linked \|\| tram.linked`), **2** = **có byte tới nhưng không thành dòng hợp lệ** (`tram.rx_active`, nghi sai baud/khung/nhiễu), **0** = **không có byte nào** (đứt dây/sai cổng/board tắt). Trước đây chỉ xét `cydlcd.linked` nên ở chế độ TRAM **luôn** `NO LINK` dù link tốt |
| Trang **STATUS (home)** | dòng trạng thái + viền khung XYZ | vẫn báo mất link (viền đỏ + `NO LINK`) — theo yêu cầu, giữ riêng cho trang home |
| Màn **TRAMMING** | mode/phase/góc + dòng hướng dẫn `CW/CCW…`/`DAT` | **đã BỎ nhãn `LINK OK/NO LINK`** ở đây (trước đây hiện trùng với header) |
| MENU dòng `Tramming wizard` | `san sang` / `chua bat` / `mat link` / `<MODE> <phase>` | trạng thái **wizard**, không phải chỉ báo link (dùng `link_ok()` để quyết định `mat link`) |
| MENU dòng `Info` | `TRAM` / `PUSH` / `-` | giao thức đang dùng (trước đây ghi `LINK`/`NO LINK` → trùng) |
| Màn `INFO` | `LINK: OK (TRAM) state …` | trang chẩn đoán nên vẫn ghi rõ giao thức + trạng thái |

Mọi chỗ hiển thị link đều đi qua **một hàm duy nhất** `link_ok()` trong `ui_lvgl.cpp` — sửa logic link chỉ cần sửa hàm này.

**Tên máy in:** ở chế độ PUSH tên lấy từ gói `HELLO` của Marlin; ở chế độ **TRAM không có HELLO** nên màn INFO hiện `FW: <CYDLCD_MACHINE_NAME> (TRAM link)` — đổi tên bằng `CYDLCD_MACHINE_NAME` trong `src/cydlcd_config.h` hoặc `-DCYDLCD_MACHINE_NAME=\"...\"` trong `platformio.ini`.

**Chẩn đoán không cần thiết bị gì thêm:** log USB của CYD (115200) in **mỗi giây** một dòng:

```
[CYD] loop OK | t=8600 ms | link PUSH=0 TBT=1 rx=0 err=0 heap=140852 | TBT dong=67 loi=0 byte=4221 cach-day=97ms
```

`byte` = **tổng số byte** nhận trên chân RX (đếm cả rác), `dong` = số dòng `TBT` hợp lệ, `loi` = dòng sai khung, `cach-day` = ms từ dòng cuối. Link tốt ⇒ `dong` tăng ~**5/giây**, `loi=0`, `cach-day` < **2000 ms** (`TBT_LINK_TIMEOUT_MS`).
⚠️ Phần sau dấu `|` (`TBT dong/loi/byte/cach-day`) **chỉ in ở chế độ TBT**; khi chưa từng nhận dòng nào thì `cach-day` = `millis()` (số rất lớn = "chưa bao giờ"). 4 bộ đếm này lấy từ `tbt.lines/tbt.errors/tbt.raw_bytes/tbt.last_rx_ms`, in ở `main.cpp` (năm 2026-10-14 chúng từng bị bỏ khỏi dòng log nhưng **vẫn được đếm** — đã in lại).

**4 nút còn lại gửi đúng 1 ký tự xuống Marlin** (`tbt_protocol.h`): `P` = probe lại góc đang đứng ·
`D` = góc này xong (đi góc cần chỉnh kế tiếp; hết góc → home Z + probe lại 4 góc) · `M` = đổi mốc
`MAX`↔`MIN` · **`BACK`** gửi `B` (thoát wizard: stow probe + đánh dấu Z chưa home) rồi về MENU.
Ngoài ra màn hình CYD gửi `Q` khi mới vào màn. Marlin trả lời bằng **1 dòng ASCII mỗi 200 ms**:

```
TRAM <phase> <mode> <idx> <ref> <d0> <d1> <d2> <d3> <A0> <A1> <A2> <A3> <active> <all_dat> <hint>
```

* `phase` 0 IDLE / 1 HOMING / 2 PROBING / 3 GOTO / 4 POINT / 5 AUTO · `mode` 1 = MAX (mốc `Z-MIN`),
  0 = MIN (mốc `Z-MAX`) · `idx` = góc nozzle đang đứng · `ref` = **mốc** đang so (mm, 3 số lẻ) ·
  `d0..d3` = độ lệch từng góc (`---.---` = chưa đo) · `A0..A3` = đã bấm DONE ·
  `hint` = `DAT` / `CW:0.150` / `CCW:nhe` / `PROBE` / `ERR` / `-`.
* **`hint` do Marlin tính sẵn** (dùng đúng `tramming_turn_cw` + `TRAM_DONE_MM` như màn 12864) nên
  màn hình CYD **không** tự suy lại chiều vặn vít ⇒ số và chiều ở 2 màn luôn giống nhau.
* Định nghĩa **dùng chung y nguyên** ở 2 nơi: `Marlin/src/feature/tbt_protocol.h` và
  `cydlcd/src/tbt_protocol.h` (sửa một bên thì **copy sang bên kia**).
* Màn hình CYD đọc dòng `TRAM …` **ngay trong vòng đọc UART của `cydlcd_link.cpp`**; byte không thuộc dòng
  TRAM được trả lại **nguyên vẹn** cho bộ giải frame PUSH (không mất byte nào), và khi đã vào chế độ
  TRAM thì màn hình CYD **ngừng gửi PING** (tránh byte nhị phân lọt vào bộ giải lệnh ASCII của Marlin).
* Ở chế độ này các màn khác (STATUS/TEMP/FILES…) vẫn mở được nhưng hiện `NO LINK` — chúng cần frame
  PUSH của `CYDLCD_SERIAL_UI`, mà 2 chế độ **loại trừ nhau** (`cydlcd_tbt_link.h` có `#error` chặn).

**Nối dây:** USART1 = **PA9 (TX) / PA10 (RX)** — bảng đầu §2.1.

---

## 6. Xử lý sự cố

| Hiện tượng | Kiểm gì |
|---|---|
| Màn trắng / không có gì | dây SPI (SCK/MOSI/CS/DC/RST), chân BL, `TFT_ROTATION`; thử `TFT_SPI_SPEED` thấp hơn (12 MHz) nếu dây dài |
| Header hiện `NO LINK` | **kiểu PUSH**: dây TX/RX có **chéo** không, GND chung chưa, baud 2 bên có trùng `250000`, Marlin đã bật `CYDLCD_SERIAL_UI` chưa (và đã tắt màn 12864 chưa), cờ `-DUSING_HW_SERIAL3 -DPIN_SERIAL3_*` có trong env chưa |
| Màn **TRAMMING** hiện `NO LINK` | **kiểu TBT**: Marlin đã thêm `#define CYDLCD_TBT_LINK` chưa; dây nối **USART1 = PA9/PA10** (header MKS WiFi) đã **chéo TX↔RX** + **chung GND** chưa; baud 2 bên 250000; `git diff Marlin/Configuration.h` phải thấy đúng 1 dòng `CYDLCD_TBT_LINK`. Trong MENU, dòng `Tramming wizard` ghi `chua bat` = chưa nhận được dòng `TBT` nào. **Trước tiên hãy chắc chắn dây màn hình CYD đang ở `P3` (IO22+IO35) chứ KHÔNG phải `P5`/`P1` cạnh USB-C** — xem §2.1; và đọc log USB: `byte` đứng yên = không có tín hiệu (sai cổng/chưa chung GND), `loi>0` = sai baud |
| **Thanh trên cùng ghi `NO LINK` nhưng dòng MENU ghi `san sang`** | **Bình thường ở chế độ TBT** (trước 2026-10-13 là lỗi hiển thị, đã sửa): thanh trên từng chỉ xét `cydlcd.linked` (**giao thức PUSH**) nên ở chế độ TBT luôn `NO LINK` dù link TBT tốt. Nay thanh trên xét **cả hai** (`cydlcd.linked \|\| tbt.linked`) ⇒ hiện `LINK OK`. Nếu vẫn `NO LINK` thật thì đọc log USB: `TBT dong=` có tăng không, `cach-day` có < 2000 ms không |
| Cần biết **màn hình CYD có nhận byte nào không** (không cần thiết bị gì thêm) | Log USB của CYD (`115200`) in mỗi giây: `link PUSH=<0/1> TBT=<0/1> rx=<số frame> err=<số lỗi> heap=<B> \| TBT dong=<số dòng hợp lệ> loi=<dòng lỗi> byte=<TỔNG byte nhận> cach-day=<ms>`. Đọc: **cờ TBT=0** = link TBT chưa tính là "sống" · `byte` **đứng yên** = **không có tín hiệu** ở chân RX (sai dây/cổng/chưa chung GND/board tắt), còn `byte` **tăng** mà `dong=0` = **có byte nhưng sai khung/baud** (tương ứng `LINK ERR`) · `dong` tăng ~**5/giây** = link tốt · `cach-day` khi rảnh phải **< 2000 ms**, khi wizard **đang bận** cho phép tới **20 s** (`TBT_LINK_BUSY_TIMEOUT_MS`) |
| **`NO LINK` hiện đúng lúc máy đang probe / di chuyển giữa các góc** | **Đã sửa 2026-10-13** (link KHÔNG hề mất — bấm `PROBE` vẫn chạy): Marlin có những đoạn **block vài giây** mà **không gọi `idle()`** (probe/dò, `home Z`) nên các dòng `TBT` bị ngưng; màn hình CYD hết timeout 2 s là báo sai. Nay: (1) khi trạng thái cuối cho thấy wizard **đang bận** (`active` + `phase != IDLE`) thì dùng timeout dài **`TBT_LINK_BUSY_TIMEOUT_MS` 20 s**; (2) màn hình CYD **chủ động hỏi lại bằng `Q`** mỗi `TBT_LINK_QUERY_MS` (1 s) khi im lặng — Marlin đáp ngay ở tick kế tiếp (`TBT_CMD_QUERY` → `s_force`), nên trạng thái luôn tươi trong lúc máy làm việc |
| Màn **TRAMMING** hiện `PROBE`/`ERR` mãi | góc đó chưa probe được (đầu dò lỗi/ngoài tầm) → bấm `PROBE` lại; `ERR` = lần probe vừa rồi trả `NAN` |
| Màn hình CYD không nhận nhiệt độ/vị trí | Marlin phải build ở **chế độ màn hình CYD** (`set_cydlcd_mode.ps1 -Mode status` để xem) — nếu còn `MKS_MINI_12864_V3` thì Marlin sẽ báo lỗi SanityCheck |
| Encoder quay ngược | `ENC_REVERSE 0/1` trong `src/cydlcd_config.h` |
| Nhiệt độ nhảy loạn | đường serial nhiễu: giảm baud 2 bên (`CYDLCD_MARLIN_BAUD` + `BAUDRATE_2`) hoặc dùng cáp ngắn/xoắn |
| ESP32 build lỗi `lv_conf.h` | phải có `src/lv_conf.h` và cờ `-DLV_CONF_INCLUDE_SIMPLE -Isrc` (đã có trong env). Nếu LVGL báo `Possible failure to include lv_conf.h` thì include path sai (đừng dùng `${PROJECT_DIR}` — nó bị expand sai trong project này) |
| CYD: chạm không ăn / lệch | TFT_eSPI giả định XPT2046 **chung bus SPI với LCD** (chỉ khác CS) nên `tft.getTouch()` **không** đọc được XPT2046 trên CYD; `ui_port_cyd_st7789.cpp` đã bit-bang SPI mềm cho touch (T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36). **Đừng sửa macro bằng tay nữa — chạy hiệu chuẩn 4 góc** (menu `Calibrate touch`, hoặc giữ tay lúc cấp điện) rồi xem **§5.5**; kết quả lưu vào NVS nên không cần build lại. Muốn thử lại đường TFT_eSPI thì `#define CYDLCD_CYD_TOUCH_USE_TFTESPI` (khi đó **không** hiệu chuẩn được) |
| CYD: màu không giống bản giả lập / sai màu | **Chạy `UI COLORTEST`** (mục “Kiểm tra đường màu” ngay dưới) rồi đối chiếu: đỏ↔xanh dương ⇒ **sai thứ tự kênh** (`TFT_RGB_ORDER`); trắng↔đen ngược ⇒ **panel bị đảo màu** (inversion). ⚠️ `TFT_INVERSION_ON` trong `User_Setup.h` **không có tác dụng** với driver ST7789 của TFT_eSPI 2.5.43 (xem mục dưới) |
| CYD: không đèn nền / màn tối | `TFT_BL` mặc định **GPIO21**; nhiều bản ST7789 dùng **GPIO27** — bật `-DCYDLCD_CYD_DEBUG=1` + thử lần lượt, và thử `fillScreen(TFT_RED)` để tách lỗi đèn nền khỏi lỗi driver/SPI |
| TFT_eSPI không thấy file setup | **bắt buộc** có `-DUSER_SETUP_LOADED=1 -include tools/tft_setup_cyd/User_Setup.h` trong `build_flags` — env CYD **đã có sẵn**; thiếu thì TFT_eSPI dùng setup mặc định (ILI9341, không có `TFT_BL`) và `ui_port_cyd_st7789.cpp` in `#warning` |
| ESP32 build lỗi `PermissionError: [Errno 13] … _contrib\intelhex\__init__.py` | ACL của `%USERPROFILE%\.platformio` bị hỏng (esptool không chạy được) → chạy **`tools/fix_platformio_acl.ps1`** bằng PowerShell **Administrator** — xem §8 |
| Muốn xem màn hình CYD nhận/gửi gì | bật `-DUSBD_USE_CDC -DHAL_PCD_MODULE_ENABLED` trong env F411 **và** `#define CYDLCD_DEBUG` trong `cydlcd_config.h` → log ra SerialUSB |

---

## Kiểm tra đường màu (`UI COLORTEST`) — màu trên board ≠ màu bản giả lập (2026-10-16)

**Người dùng báo:** *"màu trên board thật không giống bản sim, có phải thứ tự RGB không đúng?"*.
Đã rà **cả 3 tầng** của đường màu — **không có lỗi ở code UI/LVGL**; khác biệt (nếu có) chỉ có thể đến từ
**cấu hình panel**, và có đúng 2 khả năng, mỗi cái là **1 dòng**:

| Tầng | Kiểm chứng |
|---|---|
| **1. Màu gốc** (`src/ui_colortest.h`, `ui_lvgl_int.h`) | Dùng `#RRGGBB` chuẩn, khớp `tft_color.h` của Marlin (vd `MR_BG = lv_color_hex(0x1E156E)` ⇔ Marlin `COLOR_BACKGROUND 0x20AC // #1E156E`) |
| **2. Đường flush** (`ui_port_cyd_st7789.cpp`) | `pushColors(px, n, swap=true)` — tham số `true` **chỉ đổi 2 byte** (endianness ST7789), **không đổi kênh màu**. `lv_conf.h`: `LV_COLOR_DEPTH 16` + `LV_COLOR_16_SWAP 0` ⇒ nhất quán (bật SWAP=1 mới sai hết). `TFT_RED 0xF800` / `TFT_BLUE 0x001F` của TFT_eSPI **cùng quy ước** RGB565 với LVGL ⇒ hai thư viện không lệch nhau |
| **3. Panel** (`tools/tft_setup_cyd/User_Setup.h`) | Thứ tự kênh do **`TFT_RGB_ORDER`** quyết định. Dòng đó **đang comment** ⇒ TFT_eSPI 2.5.43 lấy mặc định **`TFT_MAD_RGB`** (`TFT_Drivers/ST7789_Defines.h`). Nếu panel thật là **BGR** ⇒ **đỏ ↔ xanh dương đổi nhau** |

⚠️ **Phát hiện quan trọng**: `ST7789_Init.h:102` của TFT_eSPI 2.5.43 gửi **`ST7789_INVON` VÔ ĐIỀU KIỆN** —
nó **không đọc** `TFT_INVERSION_ON`/`TFT_INVERSION_OFF` (đã grep cả file: chỉ có 1 dòng `INVON`, không có
`#if` nào quanh nó). Nên:
* dòng `#define TFT_INVERSION_ON` đang có trong `User_Setup.h` **không có tác dụng gì** với ST7789;
* panel nào **không** cần đảo màu ⇒ cả màn hình bị **"âm bản"** (trắng↔đen ngược);
* cách tắt thật: gọi `invertDisplay(false)` **sau** init → nay đã có sẵn cờ
  **`-DCYDLCD_CYD_INVERT_OFF=1`** trong `build_flags` của env `cydlcd_esp32` (xem `ui_port_cyd_st7789.cpp`).

**Cách xác định trong 30 giây (không cần đoán):**

```text
1) Board  : cắm USB, monitor 115200, gõ `UI COLORTEST`   (hoặc gửi qua COM7)
2) PC     : pwsh cydlcd/tools/run_sim.ps1 -Script cydlcd/sim/scenarios/test-color.txt
            -> 8 ảnh `cydlcd/sim/out/colortest-<i>-<TEN>.bmp` = màu ĐÚNG (đã tự kiểm bằng pixel)
3) So 8 màu trên màn hình với 8 ảnh:
     đỏ hiện ra xanh dương  -> bỏ comment `#define TFT_RGB_ORDER TFT_BGR` trong User_Setup.h (hoặc ngược lại)
     trắng <-> đen ngược    -> thêm `-DCYDLCD_CYD_INVERT_OFF=1` vào build_flags env CYD
     giống hệt              -> đường màu OK, khác biệt nằm ở độ sáng/tương phản/đèn nền
```

Bản giả lập dùng **chính bảng màu đó** (`src/ui_colortest.h`) và có kiểm tra tự động `checkcolor`: nó **đọc
lại file .bmp** rồi đo màu giữa màn hình, so với `#RRGGBB` (sai số 8/kênh vì RGB565 chỉ 5-6 bit/kênh). Kết
quả chạy thật: `RED #FF0000 OK · GREEN #00FF00 OK · BLUE #0000FF OK · WHITE/ BLACK OK · BG #18146B (~#1E156E) OK
· CYAN #00E7FF (~#00E5FF) OK · HOTEND #FF8A84 (~#FF8A80) OK` ⇒ **tầng 1+2 đúng**, nên nếu board lệch thì
chắc chắn ở tầng 3.

### Tương phản chữ trên nền — bẫy "đổi nền mà quên đổi chữ" (2026-10-16)

UI có **4 màu nền SÁNG**: `COL_ACCENT` `#00E5FF`, `COL_OK` `#81C784`, `COL_WARN` `#FFD54F`,
`COL_ERR` `#FF8A80`. `mk_btn()` luôn đặt chữ là `COL_TXT` (sáng) **bất kể nền**, nên khi ai đó đổi
**nền** mà quên đổi **chữ** thì chữ gần như tan vào nền:

| Chữ `COL_TXT` `#E6E1E5` trên nền | Tương phản | |
|---|---|---|
| `COL_ACCENT` (cyan) | **1,19:1** | ❌ |
| `COL_OK` (xanh lá) | **1,58:1** | ❌ |
| `COL_WARN` (vàng) | **1,10:1** | ❌ |
| *(chuẩn WCAG / Material 3 cần)* | *≥ 4,5:1* | |

Đã từng có **6 chỗ ở 5 màn hình** mắc lỗi này: JOG (trục đang chọn) · BED MESH (nút ON **và** nút OFF) ·
NHIỆT ĐỘ (ô vật liệu + nút bước) · THÔNG SỐ (nút bước) · CUSTOM (nút RUNOUT). Hai chỗ khác (danh sách
FILE, roller WIFI) tự đặt màu tối `0x102030` nên **đúng** — tức tác giả đã biết cách làm, chỉ là không
có gì ép buộc.

Nay gom về **một hàm tự chọn màu chữ theo độ sáng của nền**, để không thể quên nữa:

```c
lv_color_t mk_color_on(lv_color_t bg);            // nền sáng -> chữ tối, nền tối -> chữ sáng
void mk_btn_set_bg(lv_obj_t *btn, lv_color_t bg); // ui_widgets.cpp: đổi NỀN + đặt CHỮ tương phản
void mk_btn_set_sel(lv_obj_t *btn, bool sel);     // = mk_btn_set_bg(sel ? COL_ACCENT : COL_BTN)
#define COL_ON_ACCENT lv_color_hex(0x102030)      // màu chữ tối dùng chung
```

**Quy tắc:** mọi chỗ đổi nền nút thì gọi `mk_btn_set_bg()`, KHÔNG tự gọi `lv_obj_set_style_bg_color()`.
Hàm tự bỏ qua khi màu không đổi (tránh nhấp nháy + tốn SPI — các màn cập nhật 150–400 ms một lần).
Đã kiểm công thức trên **cả 12 màu** của bảng màu: tương phản thấp nhất **5,09:1** ✓.

Kiểm chứng trên board thật (đọc pixel từ ảnh `/screen.bmp`): nút X trang JOG `10,90:1` · nút ON trang
BED MESH `8,24:1` · nút RUNOUT trang CUSTOM `8,24:1` (trước là `1,19` / `1,24` / `1,58`).

#### ⚠️ Bẫy khi tự tính độ sáng: `lv_color_hex()` KHÔNG trả về 8 bit

Với `LV_COLOR_DEPTH 16`, `lv_color_hex(0x81C784)` **nhét thẳng RGB565** vào `r.full`
(xem `lv_color_hex` trong `lvgl/src/misc/lv_color.h`), nên `bg.ch.red/green/blue` là giá trị
**5/6/5 bit** — `(1, 7, 4)` chứ KHÔNG phải `(129, 199, 132)`. Nhân trọng số 8 bit (`2126/7152/722`)
lên giá trị 5 bit làm **mọi màu đều ra "tối"** ⇒ luôn chọn chữ sáng ⇒ **dựng lại đúng lỗi cũ**.
`mk_color_on()` phải mở rộng về 8 bit **trước** khi tính (đã mắc lỗi này thật một lần).

Cũng **đừng dùng** `lv_color_brightness()` của LVGL: công thức là `(3R+G+B)>>3`, cho cyan `#00E5FF`
chỉ `60/255` (vì `R = 0`) nên xếp cyan vào nhóm "nền tối".

**Material 3 — bo góc:** `mk_btn()` đổi **3 → 8 px** (filled button của M3 dùng bo góc lớn/pill).
**`GAP` CỐ Ý giữ nguyên 2 px**: tăng lên 4 làm trang JOG vượt ngưỡng và **bật thanh trạng thái dưới**
(`bot_sync()` so `scroll_top + scroll_bottom > PAGE_PAD`), lúc đó vùng nội dung bị co còn 192 px.
Muốn tăng khoảng cách thì phải đo lại bằng bản giả lập trước.

### Nút hoà vào nền màn hình — viền `outlined` (2026-10-16)

**Người dùng báo:** *"bản thân các button có màu nền xanh nhưng như vậy lại lẫn vào màu nền của toàn màn
hình cũng màu xanh như vậy"*. Đúng — và đây là lỗi **toàn cục**, không riêng trang nào:

| Cặp khối | Tương phản | WCAG 1.4.11 (khối UI ≥ 3:1) |
|---|---|---|
| Nền nút `COL_BTN` `#2A2080` vs nền màn hình `MR_BG` `#1E156E` | **1,21:1** | ❌ |
| Dòng tiêu đề `COL_BAR` `#160C52` vs nền màn hình | **1,13:1** | ❌ |
| Nền progress bar `MR_PROGBG` vs nền nút | **1,00:1** | ❌ (giống hệt) |

`COL_BTN` và `MR_BG` **cùng tông xanh tím, chỉ chênh 1,81 lần độ chói** → viền nút gần như vô hình.

> ⚠️ **Đổi màu nền KHÔNG cứu được** — đã tính kỹ trước khi chọn cách sửa:
> - **Nền tối hơn:** dù để **đen tuyệt đối** thì nút/nền cũng chỉ đạt **1,63:1** (vì *cả hai* đều tối, công
>   thức `(L_nút+0,05)/(L_nền+0,05)` bị chặn trần). Không có màu nền tối nào sửa được.
> - **Nền sáng hơn:** được về mặt toán (`L_nền ≥ 0,195`, ví dụ `#B0B0B0` → 6,12:1) nhưng đó là **đổi sang
>   theme sáng** — toàn bộ chữ đang là màu sáng, `COL_TXT` trên nền trắng chỉ còn **1,27:1** → phải làm
>   lại **cả bảng màu**.
> - **Đổi cả nền lẫn nút:** có cặp hợp lệ nhưng **mâu thuẫn**: làm nút sáng lên để tách khỏi nền thì chữ
>   sáng trên nút lại tụt dưới 4,5:1.

**Đã chọn: thêm viền 1 px** — cách duy nhất không đụng tới độ sáng của bất cứ thứ gì:

```c
#define COL_BTN_OUTLINE lv_color_hex(0x8A7FD0)   // 4,45:1 với nền màn hình · 3,68:1 với nền nút
```

- Chỉ vẽ viền cho nút **nền TỐI**. Nút nền sáng (`COL_ACCENT`/`COL_OK`/`COL_WARN`/`COL_ERR`) đã nổi sẵn
  (≥ 7:1) nên **không** viền — `outline_apply()` tự quyết theo `bg_is_light()`.
- Viền vẽ **vào trong** nên **layout không đổi**: đo lại bằng giả lập, vẫn **22 nút**, chiều cao
  `[20, 26, 28, 30, 34]`, toạ độ y hệt trước.
- ⚠️ **`focus_mark()` phải sửa kèm**: nó vốn đặt `border_width = 0` khi focus rời đi → sẽ **xoá viền**.
  Nay nút được đánh dấu `LV_OBJ_FLAG_USER_2` để `focus_mark()` biết đường **trả lại** viền
  (`USER_1` = vật tự quản lý **nền**, `USER_2` = nút có **viền**; hai cờ độc lập nhau).
  Vật KHÔNG phải nút vẫn nhận `border_width = 0` như cũ.

---

## Chụp màn hình từ xa — `GET /screen.bmp` (2026-10-16)

**Vì sao cần:** màn hình CYD **không có framebuffer** (driver vẽ thẳng ra SPI) và 320×240×2 = 150 KB thì
không đủ RAM, nên trước đây **không có cách nào xem màn hình từ xa** — mỗi lần kiểm tra màu hay bố cục đều
phải nhờ người ngồi trước máy. Vụ lệch màu panel (mục *Kiểm tra đường màu* ngay trên) phải đối chiếu đi
đối chiếu lại nhiều lần chính vì lý do đó.

```bash
curl -o shot.bmp http://<ip-man-hinh>/screen.bmp      # BMP 24-bit 320×240, 230.454 byte, ~3 giây
```

**Cách làm:** ST7789 có lệnh đọc RAM (`RAMRD`, `0x2E`). Firmware đọc **từng dải 8 dòng** bằng
`readRectRGB()` rồi trả về BMP. Vì mọi truy cập TFT_eSPI phải nằm trong **một** task, task `net` không tự
đọc panel mà gọi `ui_shot_band()`: đặt yêu cầu + **đợi semaphore**, task `ui` thấy cờ thì chụp rồi trả
semaphore ⇒ task `ui` **vẫn chạy bình thường** trong lúc chờ, không bị chặn.

### 5 cái bẫy của đường đọc ngược (đã gặp thật — ghi lại để khỏi mất thời gian lần sau)

| # | Bẫy | Xử lý |
|---|---|---|
| 1 | `readRect()` đọc **3 byte/pixel** (18-bit) nhưng init đặt `COLMOD = 0x55` (16-bit) ⇒ lệch hoàn toàn (ghi `0xF800` đọc ra `0xE007`) | Tạm đổi COLMOD sang `0x66` trong lúc đọc rồi trả về `0x55`. RAM panel vốn 18-bit/pixel nên **ảnh đang hiện không đổi** |
| 2 | `readRect()` còn **ghép kênh sai** cho ST7789 | Dùng `readRectRGB()` lấy **byte thô** rồi tự giải mã |
| 3 | Byte thô: `raw[0]` là **1 byte thừa**, rồi mỗi pixel 3 byte R6,G6,B6 (giá trị 6-bit nằm ở 6 bit cao) | Bỏ byte đầu, mask `>> 2` |
| 4 | Dữ liệu đọc về **bị dịch phải 1 bit** | Bù `<< 1`. Chứng minh: `00011000 00010100 01101000` đọc thành `00001100 00001010 00110100`. Vì giá trị 6-bit nằm ở 6 bit cao (2 bit thấp luôn 0) nên dịch phải **không mất dữ liệu** |
| 5 | Hệ toạ độ là **landscape 320×240** (`setRotation(1)`), không phải 240×320 | Đọc đúng 320 cột. Đọc nhầm 240×320 sẽ chỉ lấy 240 cột đầu và tràn chiều cao |

⚠️ Bẫy #4 là cái khó nhất: `UI READTEST` với **màu thuần** (0 và max) **luôn đạt dù công thức sai** — phải
đo thêm **tổng trung gian** (`BG 0x18AD`) mới lộ ra.

### Kiểm chứng

`UI READTEST` (console COM7) tô 5 màu đã biết, chụp lại rồi so với giá trị panel **thực sự hiển thị**:

```
BLACK 0x0000 -> chup RGB(  0,  0,  0), panel hien thi (  0,  0,  0) | lech 0,0,0 -> OK
BG    0x18AD -> chup RGB( 24, 20,105), panel hien thi ( 24, 20,105) | lech 0,0,0 -> OK
WHITE 0xFFFF -> chup RGB(255,255,255), panel hien thi (255,255,255) | lech 0,0,0 -> OK
RED   0xF800 -> chup RGB(255,  0,  0), panel hien thi (255,  0,  0) | lech 0,0,0 -> OK
GREEN 0x07E0 -> chup RGB(  0,255,  0), panel hien thi (  0,255,  0) | lech 0,0,0 -> OK
KET LUAN: 5/5 -> CHUP MAN HINH DUNG MAU
```

Đối chiếu với bản giả lập trên PC (`cydlcd/sim`, cùng trang STATUS): **87,7 % pixel khớp**, lệch trung bình
35/765. Phần lệch còn lại là do **trạng thái màn hình khác nhau** (nhiệt độ, toạ độ, giờ) — không phải sai màu.

### Mang sang máy khác build — `tools/patch_tft_espi.py` (2026-10-16)

Phần giải mã ở trên **phụ thuộc vào hành vi cụ thể của TFT_eSPI 2.5.43** (số byte/pixel, thứ tự byte, byte
dummy). Trước đây `lib_deps` ghi `^2.5.43` — **dấu `^` cho phép lên 2.5.44, 2.6.x…**, nên máy khác build sẽ
kéo bản khác và **ảnh chụp sai màu âm thầm** (không compiler nào báo lỗi; chỉ nhìn ảnh mới biết).

Đã xử lý bằng **2 việc**:

**1. Ghim đúng phiên bản** trong `cydlcd/platformio.ini` (bỏ `^`):
```ini
lib_deps = lvgl/lvgl@8.4.0
           bodmer/TFT_eSPI@2.5.43
```
*(trước đây ghi `^8.3.11` nhưng thực tế đã kéo lên **LVGL 8.4.0** — nay ghi đúng số đang chạy)*

**Bản giả lập trên PC cũng phải cùng bản LVGL:** `tools/run_sim.sh` nay ghim `LVGL_VER=8.4.0` (trước đây
8.3.11). Hai bên khác bản thì ảnh giả lập có thể lệch bố cục/màu so với board mà rất khó lần ra — và đó
chính là loại lỗi mà công cụ giả lập sinh ra để phát hiện. Đã build lại và kiểm chứng với LVGL 8.4.0: mọi
kiểm tra tự động đạt (exit 0).

**2. `tools/patch_tft_espi.py`** chạy tự động trước mỗi lần build CYD:
```ini
extra_scripts = pre:tools/patch_tft_espi.py
```
Nó kiểm tra **đúng version** và **mã nguồn thư viện còn khớp giả định của phần giải mã** chưa (COLMOD
16-bit trong init; `readRectRGB` đọc 3 byte/pixel, đúng 1 byte dummy, R–G–B liền nhau). Lệch ⇒ **build dừng
ngay** kèm hướng dẫn cụ thể, không để lọt tới lúc nhìn ảnh mới phát hiện.

Dòng log khi đạt:
```
[patch_tft_espi] TFT_eSPI 2.5.43: da ghim version + kiem tra ma nguon khop gia dinh (duong doc nguoc panel cho /screen.bmp)
```

> **Vì sao KHÔNG sửa tay file trong `libdeps/`:** PlatformIO quản lý thư viện theo version + checksum
> (`integrity.dat`); sửa tay sẽ bị **ghi đè** ở lần `pio pkg install` / `pio run -t clean` sau, và máy khác
> thì không có bản sửa đó ⇒ dựng lại đúng lỗi cũ. "Patch" bền vững ở đây là **ghim version + cổng kiểm tra**,
> không phải chép đè mã nguồn thư viện.

**Khi buộc phải nâng TFT_eSPI:** sửa `platformio.ini` → build → nạp → chạy `UI READTEST` trên board.
**5/5 đạt** thì phần giải mã còn đúng; lệch thì phải chỉnh `ui_port_capture_band()` rồi đo lại.

### Hạn chế đã biết

- Chụp mất **~3 giây**, trong lúc đó web server **không phục vụ request khác** (task `net` bận stream).
  Dùng để kiểm tra thì tốt, **không dùng để quay video**.
- **Có thể rách ảnh nhẹ nếu màn hình đang động**: task `ui` vẫn vẽ giữa các dải, nên một progress bar đang
  chạy có thể lệch giữa dải trên và dải dưới. Màn hình tĩnh thì không sao.

---

## 7. Về font chữ (chỉ dùng cho kiểu G-code)

`src/font8x8.h` là **file sinh tự động** (Lucida Console 11px → ô 8×8) bằng
`tools/make_font.py` (cần Pillow). Muốn đổi font/cỡ:

```powershell
python cydlcd/tools/make_font.py     # in ra bản ASCII-art để kiểm nét
```

Muốn chữ to hơn thì không cần sinh lại: driver đã hỗ trợ `print(..., size)` với `size = 2`.
Kiểu LVGL dùng font có sẵn của LVGL (`LV_FONT_MONTSERRAT_14`, bật trong `src/lv_conf.h`).
Còn **icon** của kiểu LVGL là file sinh tự động khác — xem **§5.2**.

---

## 8. `tools/fix_platformio_acl.ps1` — sửa ACL hỏng của PlatformIO (Windows)

**Vấn đề nó giải quyết**: trên máy này một số thư mục trong
`%USERPROFILE%\.platformio\packages\tool-esptoolpy\_contrib\` (`intelhex`, `ecdsa`) bị **DENY ACE** →
**không đọc được file**, kể cả `Get-Acl`/`icacls` cũng bị từ chối. Hậu quả: `esptool.py` không chạy
được ⇒ **PlatformIO KHÔNG build/nạp được env ESP32 (`cydlcd_esp32`)** với lỗi:

```
PermissionError: [Errno 13] Permission denied: '...\_contrib\intelhex\__init__.py'
```

Env **STM32 không dùng esptool** nên **không bị ảnh hưởng**.

**Chạy khi nào**: chỉ khi build ESP32/CYD gặp đúng lỗi `PermissionError` ở trên.
**Không bắt buộc** cho việc build bình thường — máy sạch (ACL đúng) thì không cần chạy.

```powershell
# PowerShell với quyền Administrator (bắt buộc)
powershell -ExecutionPolicy Bypass -File D:\Marlin\cydlcd\tools\fix_platformio_acl.ps1
```

Script làm 4 việc, **an toàn và chạy lại được nhiều lần**:

1. quét `~/.platformio` (`packages`, `platforms`, file ở gốc) → liệt kê mục **không đọc được**;
2. với từng mục hỏng: `takeown` (không đệ quy) → `icacls /reset` → grant Full cho
   **Administrators** + user hiện tại, rồi làm lại **có đệ quy**;
3. quét lại cả cây (`takeown /R` + `icacls /reset /T` + grant) cho chắc;
4. kiểm tra lại các file quan trọng của esptool và in `esptool.py version`.

* **Không** grant cho `Everyone` — chỉ `BUILTIN\Administrators` (S-1-5-32-544) và tài khoản đang đăng nhập.
* Tham số: `-Root <thư mục>` (mặc định `%USERPROFILE%\.platformio`) và `-SkipFullTree`
  (bỏ bước 3, nhanh hơn — chỉ sửa các mục tìm thấy).
* Bước 3 có thể mất 10–60 giây; script thoát với mã lỗi = số mục còn hỏng (`0` = xong).
* Sau khi xong, build/nạp bình thường:
  `pio run -d D:\Marlin\cydlcd -e cydlcd_esp32 -t upload --upload-port COM3`.

---

## 8b. HỘP THOẠI (msgbox): `lv_event_get_target()` **SAI** — phải dùng `lv_event_get_current_target()` (đã sửa 2026-10-15)

> 🎨 **Kiểu nút trong hộp thoại (thống nhất 2026-10-15, yêu cầu người dùng: *"các nút bấm ở các dialog
> cân vào giữa dialog, có khoảng trắng giữa các nút, nút OK luôn màu xanh và hủy hoặc xóa luôn màu đỏ.
> Bàn phím touch chưa có màu nền nên không nhìn rõ chữ"*)** — mọi hộp thoại dùng chung 3 thứ trong
> `ui_widgets.cpp` + `ui_lvgl_int.h`:
>
> | Thứ | Việc |
> |---|---|
> | `mk_dlg_row()` | hàng nút **căn GIỮA** hộp (flex `ALIGN_CENTER` cả 2 chiều), không dàn hết bề rộng |
> | `mk_dlg_btn(txt, w, h, bg, …)` | nút **bề rộng cố định** + **màu nền chỉ định**; `DLG_BTN_GAP` = **8 px** khoảng trắng giữa các nút; chữ trắng tuyệt đối; cờ `USER_1` để khung focus chỉ vẽ viền (không ghi đè nền) |
> | `style_keyboard()` | `lv_keyboard`: **nền `COL_BAR` + viền `COL_ACCENT`** + phím nền `COL_BTN` chữ trắng (mặc định bàn phím trong suốt → chữ trắng trên nền sáng không đọc được); ô nhập cũng được đặt nền tối |
>
> Màu: `DLG_OK_BG` = **xanh lá đậm** `#1B5E20` (OK/YES/Đồng ý) · `DLG_CANCEL_BG` = **đỏ đậm** `#B71C1C`
> (CANCEL/NO/HỦY/XÓA) · `DLG_NEUTRAL_BG` = `COL_BTN` (nút `-`/`+`/mức bước). Nút của `lv_msgbox` nằm
> trong `lv_btnmatrix` và **bản LVGL này không có `LV_BTNMATRIX_CTRL_CUSTOM_1/2`** để style theo từng nút
> → `ui_boxes.cpp` dùng **`LV_EVENT_DRAW_PART_BEGIN`** (`dsc->id` = chỉ số nút) để đổi `rect_dsc`/
> `label_dsc` theo **chữ của nút**. Khi mở hộp, log COM8 in ra màu sẽ tô, ví dụ:
> `[UI] hop 'KIEM TRA': [OK=XANH] [CANCEL=DO]`. Đo lại bằng `UI BOX`: hộp 200x76 ở `x=60 y=82` (giữa
> 320x240), `btnmatrix` 132x26 ở `x=33` (giữa khung 198 px) = 2 nút 62 px + khe 8 px.
>
> ⌨️ **Bàn phím: kích thước + lề (sửa 2026-10-15, lần 2)** — người dùng báo *"bàn phím chưa fill hết margin
> trái phải, mà chữ cao quá nên hàng cuối bị tụt và bị che"*. Nguyên nhân: `lv_btnmatrix` chia **đều** chiều
> cao cho số hàng (`content_height / row_cnt`, `lv_btnmatrix.c:422`) nên đặt cao 194 px thì 4 hàng × ~46 px =
> phím bị **kéo cao**, hàng cuối sát mép dưới; còn bàn phím đặt ở `x=4` rộng `320-8` + padding 3 px nên chừa
> lề 2 bên. Nay: `KB_H` = **144 px** (mỗi hàng ~35 px), **rộng đúng 320** (`x=0`), **căn ĐÁY**
> (`LV_ALIGN_BOTTOM_MID`) nên hàng cuối không bao giờ bị cắt, padding 2 px + `pad_row`/`pad_column` 2 px để
> phím fill hết bề ngang, và `lv_obj_move_foreground()` lúc mở (trang bị `page_rebuild()` sau đó sẽ vẽ **đè
> lên** bàn phím vì LVGL vẽ con theo thứ tự tạo). Đo lại bằng `UI BOX` (mục *"lớp phủ trên MÀN"*, mới thêm):
> `obj x=0 y=96 320x144` + ô nhập `x=2 y=4 316x26` — đúng cho **cả** bàn phím WiFi và bàn phím đặt tên vật liệu.

**Triệu chứng**: bấm **OK/CANCEL** của **bất kỳ** hộp thoại nào (prompt của Marlin *"Click to Resume..."*,
hộp **XAC NHAN** của `XOA`/`REBOOT DFU`, hộp **THONG BAO**, hộp **PID**) → màn hình CYD **tự reboot**:

```
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
```
(rồi log khởi động lại: `[UP] thu mount the TF ...`). Đây **rất có thể** là một phần của các báo cáo
"huỷ là bị restart LCD" trước đây — không phải lúc nào cũng do hàng đợi Marlin đầy.

**Nguyên nhân** (đọc mã LVGL 8.4 trong `.pio/libdeps/*/lvgl/src`, không đoán):
* nút của msgbox là một `lv_btnmatrix` và nó được gắn `LV_OBJ_FLAG_EVENT_BUBBLE`
  (`extra/widgets/msgbox/lv_msgbox.c:135`) để sự kiện `VALUE_CHANGED` **nổi** lên hộp;
* khi một sự kiện **bubble**:
  * `lv_event_get_target()` = vật **GỐC** phát sự kiện = **btnmatrix** (không phải hộp!),
  * `lv_event_get_current_target()` = vật **đang nhận** sự kiện = **hộp** (nơi ta gắn callback)
  (`core/lv_event.c:105-113`);
* `cb_prompt_value()` / `cb_confirm_value()` cũ dùng `lv_event_get_target()` ⇒ `box` là **btnmatrix**;
  `lv_msgbox_get_active_btn_text(box)` gọi `lv_msgbox_get_btns()` = đọc `((lv_msgbox_t*)btnmatrix)->btns`
  → **con trỏ rác** → `LoadProhibited`.

**Cách sửa**: một helper duy nhất trong `ui_boxes.cpp` — mọi handler lấy hộp qua nó:

```cpp
static lv_obj_t *box_from_event(lv_event_t *e) { return lv_event_get_current_target(e); }
```

**Đã kiểm trên máy thật (COM7)**: gửi `M0` → `[UI] hop PROMPT cua Marlin: "MARLIN" | "Click to Resume..."`;
`UI TAP K` (bấm OK) → `[UI] tra loi PROMPT cua Marlin: OK (id=0)`, Marlin `st=1` → **`st=0`**, hộp biến mất,
**không reboot**. Hộp XAC NHAN của `XOA` cũng đi đúng đường này (xem §5.3 màn FILES).

> **Ghi chú về prompt**: Marlin chờ người dùng trong `wait_for_user_response()` (`while (wait_for_user) idle();`)
> — giống kiểu "block" của `M190`/`M524`, nhưng `idle()` **vẫn chạy** nên khung `PROMPT` từ Marlin vẫn tới màn hình
> và khung `ANSWER` của màn hình vẫn được Marlin xử lý NGAY (không qua hàng đợi). Muốn thoát từ console:
> gửi `M108` (đường "critical" của `cydlcd_serial.cpp` xử lý tức thì, xem §"DỪNG IN").

---

## 9. Màn hình CYD (ESP32): khởi động, thẻ TF, WiFi, nhận file qua web server

> Đây là **ghi chép kỹ thuật của firmware màn hình CYD** (những gì đã sửa + số đo thật), không phải
> hướng dẫn từng bước. Muốn upload file: **§10**; thẻ SD đang đi đường nào: **§12**.
>
> ⚠️ **2026-10-14 — đã XOÁ HẲN hai đường upload cũ**: (a) POST file lên **module WiFi rời** và
> (b) upload qua **Bluetooth SPP**. Lý do + bảng đổi tên: xem **§9.23**. Đường duy nhất còn lại là
> **web server WiFi trên màn hình CYD** (§9.21 + §10) ghi thẳng vào thẻ ở khe màn hình (§12).

### 9.17 HOÃN INIT RA SAU KHI MÀN HÌNH ĐÃ HIỆN (boot nhanh) — đã sửa

Vấn đề gốc: `setup()` làm hết ngay (**leo thang mount thẻ 20→10→4 MHz + khởi động module nhận file
~1–1,5 s**) ⇒ màn hình chưa hiện đã chờ 2–3 s.

**Cách làm:** phần nặng được đẩy ra khỏi `setup()`; `setup()` chỉ còn `cydlcd_link_begin()` +
`ui_lvgl_begin()` (vẽ frame đầu), rồi `net_wifi_begin()` (đọc NVS, **không chặn**) và
`net_upload_http_begin()` → `cyd_sd_begin()` (mount thẻ + dọn file tạm) + đăng ký route + `server.begin()`
+ mDNS. Nhờ vậy **loop đầu chạy ở ~0,7–0,9 s** thay vì ~2,5–3 s.

* **Không còn lớp phủ “DANG KHOI TAO…”**: phần init còn lại đủ nhẹ nên không cần khoá UI lúc boot (trước
  đây phải có vì còn cả bước khởi động module nhận file). Lớp phủ + `ui_lock()` nay **chỉ** dùng lúc
  **ghi file vào thẻ** — xem §9.18.
* Đo thời gian boot: đọc `t=<ms>` ở dòng `[CYD] loop OK | t=… ms | …` **đầu tiên** (log 1 Hz) ⇒ so được
  **trước/sau**. Lúc mount thẻ, `cyd_sd_begin()`/`clean_dir()` in `[UP] …` (tốc độ, cardType, dung lượng).
* Thẻ chưa cắm: init vẫn xong bình thường, `cyd_sd_task()` tự mount lại mỗi 2,5 s (§9.12).

### 9.18 BA GIAI ĐOẠN HIỂN THỊ — INIT (không khoá) / NHẬN FILE (không khoá) / GHI VÀO THẺ (khoá UI) — đã sửa

Người dùng chốt: **chỉ khoá UI khi thật cần**, và **không hiện tên file** ở bất kỳ đâu (tốn diện tích).

| Giai đoạn | Lớp phủ | Khoá UI | Hiển thị |
|---|---|---|---|
| **INIT** (`setup()`: thẻ TF + WiFi + web server) | **KHÔNG** | **KHÔNG** | màn STATUS vẽ bình thường; COM7 in `[UP] …` / `[HTTP] web server bat …` |
| **NHẬN FILE** (`POST /upload`, state `receiving`) | **KHÔNG có lớp phủ** | **KHÔNG** | tiến độ do **trình duyệt** vẽ (`xhr.upload.onprogress` + poll `/status`); COM7 in 1 Hz `[UP] … rx=… written=…` |
| **GHI VÀO THẺ** (state `writing`) | toàn màn hình, nền mờ | **CÓ** | `DANG GHI VAO THE SD...` + dòng `%  KB đã ghi/tổng KB  KB/s` + thanh tiến độ |
| Kết quả | vẫn là lớp phủ trên | không | `DA GHI VAO THE SD` hoặc lý do lỗi (`ov_set()`), rồi tự gỡ |

* Chỉ lúc **ghi vào thẻ** mới `ov_create_full()` + `ui_lock(true)` (trong `cyd_sd_finish_upload()`): để
  người dùng không bấm IN / mở trang FILES trong lúc màn hình đang ghi thẻ ⇒ **đó mới là lúc cần khoá UI**.
* **Hợp đồng API cho nhánh UI** — `src/net_upload_http.h`:

```c
// true = ĐANG có phiên nhận/ghi file (từ lúc bắt đầu nhận tới lúc ghi xong vào thẻ).
// Chỉ đọc biến trạng thái đã cache -> không chặn, không cấp phát.
// UI dùng cờ này (sd_host_busy() trong ui_lvgl.cpp) để CHẶN các lệnh làm Marlin đọc thẻ
// (M23/M24/M30, M21, mở trang FILES) trong lúc màn hình đang bận với thẻ.
bool net_upload_active(void);
```

* Nhánh UI **không** đọc số byte trực tiếp: tiến độ chi tiết nằm ở `/status` (`state`, `rx_bytes`,
  `total_bytes`, `written_bytes`) — xem **§9.21**; màn hình chỉ cần biết *có phiên đang chạy* hay không.
* Khi tắt web server (`CYDLCD_WIFI_UPLOAD 0`) hoặc board khác: **stub trong `net_upload_http.cpp`** luôn
  trả `false`; `ui_lvgl.cpp` khai báo `weak`/`extern "C"` nên mọi env vẫn build + link được.
* Lớp phủ lúc ghi thẻ dùng thêm font **`lv_font_montserrat_20`** cho dòng trạng thái (chữ to) — font được
  bật trong `src/lv_conf_cydlcd.h` (chỉ env CYD): tốn **flash** (~22 KB), **không tốn DRAM** vì font là `const`.
* Vòng đời lớp phủ: tạo ở `cyd_sd_finish_upload()`, gỡ ở cuối hàm; lúc **nhận file** không tạo object LVGL
  nào (không tốn LVGL heap).


### 9.20 WiFi là MODULE RIÊNG (`net_wifi.cpp`, cờ `CYDLCD_WIFI_MGMT`) — đã sửa

Trước đây (trước 2026-10-14) phần WiFi nằm **trong module upload qua Bluetooth**, nên khi tắt đường đó thì
`net_wifi_state()` biến mất ⇒ icon WiFi ở header thành **xám** và không bấm vào đâu được. Nay WiFi là module
**độc lập**: chỉ **một nơi duy nhất** (`net_wifi.cpp`) được gọi `WiFi.begin/mode/disconnect`.

**Cách tách cờ** (`cydlcd_config.h`):
* `CYDLCD_WIFI_MGMT` — mặc định **1 với board CYD**, 0 với board khác; `net_wifi.cpp` compile theo cờ này.
* `CYDLCD_WIFI_UPLOAD 1` (mặc định với CYD) ⇒ **ép** `CYDLCD_WIFI_MGMT 1` vì web server cần `net_wifi`.
* `CYDLCD_WIFI_MGMT 0` ⇒ `net_wifi.cpp` chỉ còn **stub** (mọi hàm trả 0/`""`/false) nên nhánh UI vẫn build.
* `CYDLCD_WIFI_AUTOSTART` — mặc định **1**: màn hình tự vào mạng nhà ngay lúc boot (không phải chờ một phiên
  upload nào), nên icon WiFi ở header **đúng ngay từ đầu**.

**API cho nhánh UI** (`net_wifi.h`, tất cả **`extern "C"`** ⇒ symbol **không mạng hoá**; đã kiểm bằng `nm`:
`T net_wifi_state`, `T net_wifi_scan`, … khớp khai báo `extern "C"` + weak trong `ui_lvgl.cpp`):

```c
uint8_t     net_wifi_state(void);          // 0 tat/loi, 1 dang ket noi, 2 da ket noi (cache, khong chan)
const char *net_wifi_ssid(void);           // SSID dang ket noi ("" neu chua bat)
const char *net_wifi_ip(void);             // "192.168.0.42" ("" neu chua ket noi)
int8_t      net_wifi_rssi(void);           // dBm, 0 = khong biet
int         net_wifi_scan(void);           // !!! CHAN 8-12 s (scanNetworks dong bo) - UI KHONG dung nua
const char *net_wifi_scan_ssid(int i);     // NULL neu i ngoai khoang
int8_t      net_wifi_scan_rssi(int i);     // 0 neu i ngoai khoang
bool        net_wifi_connect(const char *ssid, const char *pass, bool save);  // khong chan (save=true -> NVS)
void        net_wifi_disconnect(bool forget);                                 // khong chan
// QUET KHONG CHAN (ui_lvgl.cpp dung bo nay, lap vao nhip 400 ms):
int         net_wifi_scan_begin(void);     // 0 = da bat dau | -1 = dang quet roi
int         net_wifi_scan_poll(void);      // -1 dang quet | -2 loi/qua han | >=0 XONG (so mang)
void        net_wifi_scan_cancel(void);    // huy phien (goi khi roi trang WIFI)
bool        net_wifi_scanning(void);
// them (khong bat buoc): net_wifi_configured/saved_ssid/scan_max/scan_ms + net_wifi_join_temp/leave_temp
```

* **NVS**: `Preferences`, namespace `"wifi"`, khoá `ssid`/`pass`. Chưa có NVS ⇒ dùng giá trị **gợi ý mặc định**
  `CYDLCD_WIFI_SSID_DEF`/`CYDLCD_WIFI_PASS_DEF` (= `TP-Link_31A0` / `89278062`); đổi từ trang settings ⇒ ghi NVS.
* **Tự kết nối lại**: mất mạng thì thử lại mỗi `CYDLCD_WIFI_RETRY_MS` (8 s); cache cập nhật ở `net_wifi_task()`.
  **Chống dội (2026-10-14)**: chỉ ép `WiFi.begin()` khi đã mất kết nối **liên tục ≥ 3 s**
  (`CYDLCD_WIFI_DOWN_CONFIRM_MS`) và **KHÔNG bao giờ** ép trong lúc đang quét hoặc trong 20 s sau khi quét
  (`CYDLCD_WIFI_SCAN_GRACE_MS`) — để core tự auto-reconnect, tránh WiFi chập chờn (đã gặp: màn hình CYD tự reboot).
* **Console COM7** (test không cần UI): `WIFI?`, `WIFI SCAN` (quét **không chặn**, vẫn bơm link),
  `WIFI ASYNC 0|1|2` (chẩn đoán), `WIFI CONNECT <ssid> <pass> [S]` (S = lưu NVS), `WIFI OFF`, `WIFI FORGET`,
  và `PRE?` / `PRE <i 0..2> <nozzle> <bed> [fan]` (preset preheat, **§9.22**).
* **RAM tĩnh** thêm ~0,9 KB (SSID 33 + pass 65 + NVS + 16 mạng quét). Sau khi xoá Bluetooth (2026-10-14)
  **không còn phải cân RAM giữa hai stack radio** nữa ⇒ WiFi bật thường trực, không cần bật/tắt theo phiên.

#### ⚠️ Quét mạng: 3 cái bẫy đã gặp thật (2026-10-14)

1. **Quét ĐỒNG BỘ chặn vòng lặp 8-12 s** (đo thật: **11,6 s** với 39 mạng; `max_ms_per_chan 150` cho 8,2 s).
   Trong lúc đó màn hình CYD **không trả lời sector thẻ SD cho Marlin** (đang in mà quét = **hỏng bản in**) và
   không phục vụ web server; frame PUSH còn bị mất (`err=1`). ⇒ UI **đã chuyển sang quét KHÔNG CHẶN**
   (`net_wifi_scan_begin`/`poll`, kết quả lấy ở `page_wifi_update()`), và **từ chối quét khi Marlin đang in**.
2. **Bẫy của core Arduino** (`libraries/WiFi/src/WiFiScan.cpp:63,144`): timeout của quét **async** =
   `max_ms_per_chan × 20`. Với `150` ⇒ timeout **3 s** < thời gian quét thật ⇒ `scanComplete()` luôn trả
   `WIFI_SCAN_FAILED (-2)` dù quét vẫn chạy. ⇒ đặt `CYDLCD_WIFI_SCAN_MS_PER_CHAN 600` (timeout 12 s).
   Triệu chứng nếu quên: log `[WIFI] quet LOI` sau ~2 s, roller không có mạng nào.
3. **`net_wifi_poll()` gọi `WiFi.begin()` giữa lúc đang quét sẽ HUỶ phiên quét** (và làm WiFi chập chờn:
   mỗi lần `begin()` lại là một lần rớt/ nối, trong khi mDNS + web server đang mở). ⇒ `net_wifi_poll()`
   **return ngay khi `s_scanning`**, và sau khi quét còn 20 s "ân hạn" không ép `begin()`.

**Rời trang WIFI KHÔNG đụng tới WiFi**: `wifi_page_leave()` (gọi từ `show_page()` khi `p != PG_WIFI`) chỉ
đóng bàn phím mật khẩu, ẩn lớp phủ quét, **huỷ phiên quét đang chạy** và mở khoá input. Chỉ 4 nút
`QUET` / `KET NOI` / `NGAT` / `XOA` mới thay đổi WiFi — đúng như mong đợi "không sửa thông tin thì đừng
reset WiFi".

### 9.21 UPLOAD QUA WiFi: WEB SERVER TRÊN MÀN HÌNH CYD (`net_upload_http.cpp`, cờ `CYDLCD_WIFI_UPLOAD`)

Luồng: **PC/điện thoại → WiFi → màn hình CYD ghi `/cydlcd_tmp/<tên>.tmp` trên thẻ TF → đổi tên vào **GỐC**
thẻ → `M22`+`M21` cho Marlin mount lại + xin lại danh sách file → xoá file tạm.** Đây là **đường duy nhất**
còn lại: đường POST sang module WiFi rời và đường upload qua Bluetooth **đã xoá 2026-10-14** (xem **§9.23**).

> 🐞 **LỖI ĐÃ SỬA 2026-10-15 (vòng 2) — "upload bị chặn với lý do máy in đang in, tôi chỉ bấm G35"**:
> `printer_busy()` (`cyd_sd.cpp`) trước đây chặn upload khi `cydlcd.state` là **PRINTING / PAUSED /
> HOMING / LEVELING / RUNOUT / KILLED** — mà **G35 (Bed Tramming Wizard)** chỉ HOME + PROBE ⇒ Marlin báo
> `HOMING`/`LEVELING` ⇒ bị coi là "đang in" và từ chối gửi file ❌ (các trạng thái đó **không đụng tới
> thẻ SD** nên ghi file upload là an toàn).
> **Sửa**: `printer_busy()` **chỉ xét CỜ IN TỪ THẺ SD** — `CYDLCD_SF_SD_PRINTING` (Marlin đọc thẻ liên
> tục) hoặc `CYDLCD_SF_PAUSED` — tức chỉ chặn khi **thật sự đang in / tạm dừng bản in TỪ THẺ**. HOME/
> PROBE/G35 **không** còn bị coi là đang in.
> * Thông báo từ chối nay nói rõ: `Marlin dang IN TU THE SD - khong nhan file` (kèm `st=`/`fl=` trong log).
> * Nút `STOP` cũng dùng đúng phép kiểm tra đó (`stop_retry_task()`), nên bấm STOP rồi thoát cũng không
>   gửi `M108`/`M524` trong lúc máy đang G35.
> * **Cách biết máy có đang in hay không**: lệnh `UI BUSY` (hoặc `UI BOX`) in
>   `[UI] MAY: st=<mã> (<tên>) fl=0x.. file="<file đang in>" prog=<%> | IN TU THE SD=0/1 TAM DUNG=0/1`
>   rồi `[UI] upload: cho nhan file | BI CHAN (...)`. Trên màn hình: dòng trạng thái ở STATUS + **2 nút
>   `II`/`[]` trên thanh tiêu đề chỉ hiện khi đang in từ thẻ SD** + tên file/tiến độ; màn FILE SD thì
>   `PRINT`/`XOA` bị khoá trong lúc in.

> 🐞 **LỖI ĐÃ SỬA 2026-10-15 — "gửi file 4,3 MB bị đứt kết nối giữa chừng"**: file **2 MB** gửi được
> (~13 s) nhưng file **4,3 MB** luôn chết ở **~46%** (~1,9 MB). Log màn hình cho thấy nguyên nhân **không
> phải** mạng/thẻ:
> ```
> 5.0s [HTTP] nhan file "V300_bot_2.gcode" tu 192.168.0.174 ...
> 7.8s [CYD] CANH BAO: vong lap KHONG chay 2 s (dang o chang 3)     <- chang 3 = web server
> 15.8s ... 10 s
> 20.8s [CYD] TREO > 15 s o chang 3 -> TU KHOI DONG LAI             <- watchdog chan doan TU REBOOT panel
> ```
> `server.handleClient()` của WebServer (ESP32 core) xử lý **CẢ BODY** của POST trong **một** lần gọi
> (đồng bộ) ⇒ vòng lặp chính **đứng HỢP LỆ** suốt phiên upload, thời gian **tỉ lệ với kích thước file**
> (~13 s cho 2 MB, ~30 s cho 4,3 MB) ⇒ watchdog 15 s (`cyd_diag_task`) tưởng "treo" và **tự khởi động
> lại giữa phiên** ⇒ PC thấy `ket noi bi dut giua chung`, và `SD.write()` báo `ghi the TF loi` (do bị
> ngắt giữa đường).
> **Sửa 3 lớp**:
> 1. `main.cpp` (`cyd_diag_task`): **MIỄN TRỪ watchdog khi `net_upload_active()`** (đang nhận/ghi file) —
>    log `dang nhan file WiFi -> MIEN TRU watchdog`. Đây là mấu chốt.
> 2. `cyd_sd_temp_open()`: gửi **`M22`** ngay khi bắt đầu nhận file (Marlin **nhả thẻ**, thôi đọc sector
>    trong suốt phiên; cuối phiên `notify_media_changed()` gửi `M22`+`M21` để mount lại).
> 3. `cyd_sd_raw_read()`: **từ chối** đọc sector thô khi `s_file`/`s_busy` (phòng khi Marlin chưa kịp xử
>    lý `M22`) — đếm ở `refused=` trong dòng `[MRL]`; an toàn vì phiên upload đã bị chặn khi máy đang in.
> **Đo lại trên máy thật**: `V300_bot_2.gcode` **4 523 980 byte** gửi trong **33,1 s (133,6 KB/s)** →
> `{"state":"done","rx_bytes":4523980,"total_bytes":4523980,"written_bytes":4523980,
> "file":"V300_bot_2.gcode","result":"DONE 4523980"}`; `UI TAP L` xác nhận trên thẻ đúng **4 523 980 B**,
> và file hiện trong danh sách màn FILE SD.
> (Nhánh script `cydlcd/tools/cydlcd-upload.ps1` cũng đã sửa: thông báo lỗi "No route to host" không còn
> bị dịch nhầm thành "không phân giải được tên", thêm dòng trắng trước thông báo lỗi để không dính vào
> thanh tiến độ `KB/s`, và **thử lại 3 lần** với chờ lâu dần — commit `331c91a8e7`.)

> **VÒNG SỬA 2026-10-15 (sau khi người dùng hỏi "có phải do multiple part dẫn đến open/close file khác?").**
>
> **Trả lời bằng số đo, không đoán.** Đã thêm bộ đếm chẩn đoán rồi upload lại 1 lần file 4,3 MB:
> ```text
> [HTTP] PHAN FILE #1: "V300_bot_2.gcode" tu 192.168.0.174 (kich thuoc chua biet)
> [UP] MO file tam lan 1 (phien moi, lan mo thu 1) heap=126284 B heapmin=130840 B
> [UP] da ghi 1024 KB / 6347 ms (161 KB/s, heap=120304 B)
> [UP] da ghi 2048 KB / 13477 ms (151 KB/s, heap=121976 B)
> [UP] da ghi 3072 KB / 19833 ms (154 KB/s, heap=120304 B)
> [UP] da ghi 4096 KB / 26889 ms (152 KB/s, heap=121824 B)
> [HTTP] PHAN FILE #1 xong: 4523980 B (file con MO, cho het request)
> [HTTP] HET REQUEST (OK): parts=1 chunks=3151 da nhan 4523980 B -> dong file + ghi vao the SD
> [UP] DONG file tam ...: 4523980 B, 1 lan mo trong phien
> [UP] da doi ten ... -> /V300_bot_2.gcode (4523980 B) ; [UP] bao Marlin mount lai the (M22/M21)
> ```
> ⇒ **`parts=1`, mở file **1 lần**, đóng **1 lần** cho cả file** ⇒ **KHÔNG phải do nhiều phần**: script
> (`New-MultipartParts`) chỉ gửi **1 field text `name` + 1 phần file**, và `Parsing.cpp` gọi
> `UPLOAD_FILE_START` **một lần cho mỗi phần file** (core 2.0.17, dòng 423-433). Tốc độ **đều 151-161 KB/s**
> (trước đây 10-16 KB/s rồi đứt), heap thấp nhất **113 KB** — không hề cạn heap/không phân mảnh.
>
> **Nhưng câu hỏi đó chỉ ra 1 lỗi thật trong code cũ, đã sửa:** bản cũ **đóng file tạm ngay ở
> `UPLOAD_FILE_END`** — mà `_parseForm()` (Parsing.cpp:480-486) **quay lại đọc phần kế tiếp** nếu request
> có ≥2 phần file. Khi đó: phần #2 gọi `cyd_sd_temp_open()` → `SD.remove(s_tmp)` **XOÁ SẠCH dữ liệu đã
> nhận của phần #1**; còn nếu file đã bị đóng thì mọi chunk sau trả về 0 ⇒ báo `ghi the TF loi` (thông báo
> SAI bản chất). Sửa:
> 1. `net_upload_http.cpp`: **chỉ đóng file MỘT LẦN khi hết request** (`handle_upload_done`) — không đóng ở
>    `UPLOAD_FILE_END`; cờ `s_sess_busy` đánh dấu ranh giới request.
> 2. `cyd_sd_temp_open(name, size_hint, same_request)`: phần #≥2 **GHI TIẾP** vào file đang mở (cùng tên
>    đích) thay vì xoá/mở lại ⇒ có nhiều phần thì file vẫn liền mạch, không mất dữ liệu.
> 3. Thông báo lỗi **tách bạch 2 nguyên nhân**: `file tam khong mo (phien bi ngat?)` (lỗi logic) vs
>    `the ghi thieu (n/m B)` (lỗi thẻ/FATFS) — trước đây gộp cả thành `ghi the TF loi` nên không biết
>    đường nào hỏng. Lý do lấy từ `cyd_sd_temp_err()`.
>
> **Chẩn đoán mới** (để lần sau đọc log là biết ngay, không phải suy đoán):
> * log `[UP] MO file tam lan N (phien moi, lan mo thu M)` + `[UP] DONG file tam …: N B, M lan mo trong
>   phien` ⇒ **M ≥ 2 là dấu hiệu bị mở lại giữa phiên** (đúng thứ câu hỏi trên lo);
> * log tiến độ **mỗi 1 MB** kèm **KB/s + heap** ⇒ thấy ngay phiên chậm/đứt ở mốc nào;
> * log `[UP] PHAN tiep theo (lan mo thu M trong request) -> GHI TIEP …` khi client gửi nhiều phần;
> * `/status` thêm **`parts`** (số phần file của request) và **`chunks`** (số chunk WRITE) — nhìn là biết
>   client gửi 1 phần hay nhiều phần, và có phải "gửi lại" (mỗi lần script thử lại = **request mới** =
>   phiên mới ⇒ đó mới là trường hợp "mở/đóng file khác", hiện ra thành 2 dòng `[UP] MO file tam lan 1` và
>   `lan 2` trong log).
> * **Đo lần này**: 4 523 980 B trong **28,9 s (152,7 KB/s)**, `{"status":"Ok",…,"parts":1}`, `/status` →
>   `state=done written=4523980`; `UI TAP L` xác nhận trên thẻ `V300_bot_2.gcode 4523980 B` (gốc thẻ 8 mục,
>   `cydlcd_tmp` rỗng — không còn file `.tmp` rác). RSSI −43…−57, `heapmin=113076 B`.

> **VÒNG SỬA 2026-10-15 (b) — KIẾN TRÚC PART + size khai báo + SPI max + dọn rác 30 phút.**
> (Theo đúng đề xuất/yêu cầu của người dùng: *"mỗi lần nhận được part thì open và close part đó luôn, try
> catch để close file, đặt tên các part là xxxx_001, xxxx_002… rồi khi done thì merge lại… chỉ cần một biến
> cộng dồn file size, nếu bằng số lượng user khai báo lúc đầu thì là done"*, *"không bao giờ mount lại sdcard,
> spi clock để max đi"*, *"file rác do bị abort/lỗi thì dọn dẹp định kỳ 30 phút 1 lần, file tạm quá 30 phút
> thì xóa"*, *"thêm đoạn kiểm tra file size sau khi merge, nếu không đúng thì lỗi"*.)
>
> **Lỗi thật đã tái hiện được (không phải do nhiều part):** upload lại `V300_bot_2.gcode` 4,5 MB thì chết ở
> **3 809 280 B** = đúng **930 lần ghi 4096 B** rồi `File.write()` trả về **0** mãi. Log 1 Hz ngay sau đó:
> `free=0KB` (⇒ `f_getfree()` **lỗi**), `sd=0` (thẻ **mất kết nối**), rồi vài giây sau `sd=1 free=15 GB`
> trở lại ⇒ **thẻ/card "đứt" tạm thời rồi TỰ HỒI**, không phải do ai unmount giữa đường (trong lúc nhận file,
> vòng lặp chính bị `handleClient()` chặn hoàn toàn nên *không ai khác chạm được thẻ*). Lỗi nằm ở tầng driver
> thẻ (SPI/card), xác suất ~1/2 lần với file 4,5 MB.
>
> **Kiến trúc ghi mới (mỗi part = 1 file riêng, đóng ngay):**
> * `cyd_sd_temp_open(name, size_hint, part)` → mỗi phần file ghi vào `/cydlcd_tmp/<ten>_NNN.tmp`
>   (`_001`, `_002`, …). Part #1 = **phiên mới** (dọn part/`.tmp` cũ của cùng tên, kiểm tra chỗ trống bằng
>   **size khách khai báo**).
> * `UPLOAD_FILE_END` → `cyd_sd_temp_close()` **ĐÓNG NGAY** part đó (flush hết đệm rồi `close()`): dữ liệu
>   part đã xong **nằm chắc trên thẻ**, part sau lỗi/ngắt không làm mất part trước.
>   *ESP32 Arduino tắt exception (`-fno-exceptions`) nên "try/catch" ở đây = **mọi đường thoát đều gọi hàm
>   đóng***: hết part, hết request, lỗi ghi, client ngắt, và cả trường hợp mở part mới mà part cũ chưa đóng.
> * Hết request: **1 part ⇒ đổi tên thẳng** `_001.tmp → /<ten>` (**không copy**, không tốn thêm thời gian);
>   **≥2 part ⇒ MERGE** tuần tự `_001 + _002 + …` vào `<ten>.tmp` (đọc 16 KB/lần, xoá từng part sau khi
>   copy xong, dùng chính đường ghi có đệm + phục hồi), rồi mới đổi tên vào gốc thẻ.
> * **Biến cộng dồn + size khách khai báo**: client gửi thêm field text **`size=<số byte>`** (script và trang
>   web đều gửi; trang web: `fd.append('size', f.size)`). Màn hình chốt **DONE khi tổng byte ghi xuống thẻ ==
>   size khai báo** (không khai báo thì lấy số byte nhận từ mạng).
> * **3 lớp kiểm tra size** (yêu cầu *"thêm đoạn kiểm tra file size sau khi merge"*):
>   1. từng part: `đọc được == size FAT báo`;
>   2. sau merge: **mở lại file ghép đọc size thật trên thẻ** == size khai báo;
>   3. sau khi đổi tên: **mở lại `/<ten>` đọc size thật** == size khai báo; **sai ⇒ XOÁ file và báo lỗi**
>      (không bao giờ để file `.gcode` cut/sai nằm trong gốc thẻ — chọn in là hỏng bản in).
> * **KHÔNG BAO GIỜ MOUNT LẠI THẺ**: bỏ hẳn `SD.end()`/`SD.begin()` khỏi đường phục hồi. Khi thẻ ghi lỗi:
>   **chờ đợi kiên nhẫn** (backoff 20→2000 ms, tổng ~7,4 s) + **đóng/mở lại file ở `FILE_APPEND`** (chỉ đọc
>   lại dir entry của chính file đó, không đụng volume) rồi ghi tiếp; chỉ bỏ phiên khi thật sự hết cách.
>   (Đường mount lại duy nhất còn lại là khi **Marlin vừa ghi sector thô** — lúc đó cache FATFS của màn hình
>   *bắt buộc* phải đọc lại, nếu không sẽ ghi đè dữ liệu của Marlin.)
> * **SPI clock = MAX 20 MHz** (`CYDLCD_SD_HZ`, trước là 4 MHz) + **đệm ghi 16 KB** (32 sector/lần thay vì
>   4 KB) ⇒ giảm 4 lần số phép ghi (276 lần cho 4,5 MB thay vì 1104) và rút ngắn cửa sổ rủi ro.
>   `sd_mount()` vẫn tự hạ 20 → 10 → 4 MHz nếu thẻ không chịu được.
> * **Dọn file rác định kỳ 30 phút** (`CYDLCD_SD_CLEANUP_MS`, chạy trong `cyd_sd_task()` khi **đang rảnh**):
>   quét `/cydlcd_tmp`, xoá file `.tmp` **cũ hơn 30 phút** theo `File::getLastWrite()` (đồng hồ chưa được đặt
>   nên sau khi reboot `now < mtime` ⇒ coi là rác của lần chạy trước và xoá). Vẫn giữ dọn lúc khởi động và
>   dọn trước mỗi phiên mới; **mọi đường lỗi/abort đều xoá part + `.tmp` ngay**.
> * Lỗi ghi báo **tách bạch 2 nguyên nhân**: `file tam khong mo (phien bi ngat?)` (lỗi logic) vs
>   `the ghi thieu (0/16384 B)` (lỗi thẻ) — và `/status` không còn hiện số liệu **CŨ** của phiên trước
>   (`written_bytes` lấy của phiên đang chạy; đầu phiên đã xoá `s_result`/`s_written`).
>
> **Đo lại trên máy thật (sau khi sửa):**
> ```text
> # 1 part (4 523 980 B): MO PART #1 -> ..._001.tmp ; DONG PART #1: 4523980 B
> [HTTP] HET REQUEST (OK): parts=1 chunks=3151 da nhan 4523980 B, tren the 4523980 B -> doi ten/GHEP
> [UP] da doi ten ..._001.tmp -> /V300_bot_2.gcode (4523980 B, kiem tra lai tren the = 4523980 B)
> # 3 part (63 472 B, gui co chu dich de test merge):
> [UP] MO PART #1/#2/#3 -> ..._001/_002/_003.tmp ; DONG PART #1/#2/#3: 21158 / 21158 / 21156 B
> [UP] GHEP 3 part -> .../SPLIT3_TEST.gcode.tmp
> [UP] GHEP part #1: doc duoc 21158 B / size tren the 21158 B        (… #2, #3 tuong tu)
> [UP] GHEP: kiem tra size file ghep tren the = 63472 B (bo dem 63472 B / can 63472 B)
> [UP] GHEP XONG: 63472 B -> ... ; [UP] da doi ten ... -> /SPLIT3_TEST.gcode (63472 B, kiem tra lai = 63472 B)
> ```
> `/status`: `state=done parts=3 parts_done=3 declared=63472 written_bytes=63472 result=DONE 63472`;
> `UI TAP L`: gốc thẻ có **`SPLIT3_TEST.gcode 63472 B`** + `V300_bot_2.gcode 4523980 B`, `/cydlcd_tmp` rỗng.
> Tốc độ upload 4,5 MB: **21,7 s (203,9 KB/s)** ở 20 MHz (trước 28,9 s ở 4 MHz); heap thấp nhất ~95 KB.
> *Ghi chú test*: bản test đầu tiên của script chia part bị **ghi header 2 lần** ⇒ panel nhận dư **320 B
> (= 2 × 160 B header)** và **từ chối công bố** (`size khong khop (the co 1184637 B / can 1184317 B)`) —
> tức lớp chốt size đã hoạt động đúng trước cả khi tìm ra lỗi ở script test.

> **VÒNG SỬA 2026-10-15 (c) — ĐO ĐỘ ỔN ĐỊNH: 4/4 LẦN THÀNH CÔNG, PHỤC HỒI KHÔNG MOUNT LẠI THẺ.**
> Chạy **4 lần liên tiếp** upload `V300_bot.gcode` **4 563 245 B** (20 MHz) — **4/4 thành công**:
> ```text
> lan 1: sach
> lan 2: [UP] the ghi thieu (0/16384 B, da ghi 704512 B) - cho 20 ms roi THU LAI lan 1/9
>        [UP] PHUC HOI (KHONG mount lai the): dong + mo lai file part o che do GHI TIEP (can 704512 B)
>        [UP] PHUC HOI OK: mo lai duoc ..._001.tmp (704512 B), ghi tiep
>        [UP] the ghi thieu (0/16384 B, da ghi 704512 B) - cho 50 ms roi THU LAI lan 2/9
>        -> DONG PART #1: 4563245 B ; da doi ten ... -> /V300_bot.gcode (kiem tra lai = 4563245 B)
> lan 3: sach
> lan 4: dut o 999424 B -> PHUC HOI OK -> xong (4563245 B)
> ```
> ⇒ Thẻ **vẫn "đứt" ~50% số lần** như trước (đúng như log `free=0KB`/`sd=0` đã chỉ ra — vấn đề phần cứng
> thẻ/khe thẻ), nhưng **panel tự phục hồi và upload vẫn hoàn tất**: không mount lại thẻ, không mất dữ liệu,
> không cần người dùng gửi lại. Tốc độ 4 lần: 21,9 / 20,9 / 21,5 / 24,3 s (183–213 KB/s).
> * Thêm **thử lại bước ĐỔI TÊN** (backoff 100→2000 ms, ~8,4 s): lần chạy trước đó 1/3 lần hỏng **ngay ở
>   `SD.rename()`** (log ngay sau: `free=0KB`, `sd=0`) — nay đổi tên cũng kiên nhẫn như bước ghi.
> * Kiểm tra size sau merge/đổi tên phân biệt **"không mở lại được"** (thẻ đang lỗi) với **"sai size"** —
>   tránh xoá nhầm một file vừa ghi đúng chỉ vì `SD.open()` thất bại tạm thời.
> * Bộ **dọn rác định kỳ đã chạy thật** (test với chu kỳ 40 s): `don /cydlcd_tmp (dinh ky moi 0 phut): xoa 0
>   file .tmp (0 KB), giu lai 0 file con moi`; dọn lúc khởi động cũng đã xoá 1 file `.tmp` 4 417 KB còn lại
>   của phiên bị lỗi trước đó. Chu kỳ thật đã trả về **30 phút** (`CYDLCD_SD_CLEANUP_MS`).
> * `UI TAP L` sau 4 lần: gốc thẻ có `V300_bot.gcode 4563245 B` (đúng size), `/cydlcd_tmp` rỗng.

> **VÒNG SỬA 2026-10-15 (d) — TRANG WEB CÓ DANH SÁCH FILE + NÚT XOÁ NGAY TRÊN WEB.**
> (Yêu cầu người dùng: *“thêm phần danh sách file ở giao diện web, cho phép người dùng xóa file ngay tại đó”*.)
>
> * Trang `GET /` thêm thẻ **“File trên thẻ SD”**: liệt kê tên + kích thước (B/KB/MB, thư mục ghi `[TM]`),
>   nút **Làm mới**, mỗi dòng một nút **Xoá** — **nút Xoá đặt TRƯỚC tên file** (tránh nhầm dòng khi tên bị
>   cắt ngắn) và **xoá ngay khi bấm, KHÔNG hỏi xác nhận** (yêu cầu người dùng). Thẻ này **giãn full size**:
>   `body` là flex cột cao 100%, thẻ gửi file `flex:0 0 auto`, thẻ danh sách `flex:1 1 auto; min-height:180px`
>   và `.list` tự cuộn khi danh sách dài hơn màn hình. Danh sách tự nạp khi mở trang và **tự nạp lại sau khi
>   upload xong**; riêng khi **xoá** thì **KHÔNG gọi lại `/files`** — JS **gỡ luôn dòng vừa xoá** khỏi danh
>   sách đang hiện (nhanh, không nháy trang); nếu xoá LỖI thì giữ nguyên dòng và trả nút lại để bấm lại.
> * **Ô token được điền sẵn `CYDLCD_HTTP_TOKEN`** (mặc định `TBT-3D-up`) khi trả trang: `handle_root()` copy
>   trang từ PROGMEM ra `String` rồi thay placeholder `%%TOKEN%%` — không phải gõ tay nữa. Vẫn đổi được bằng
>   `?token=...` (JS ghi đè giá trị trong ô nhập nếu URL có token).
> * **Nút “Gửi file” chỉ bật khi đã chọn file** (`<button … disabled>` + `input[type=file] onchange="updGo()"`)
>   và **bị khoá trong suốt lúc đang gửi** (`upBusy`, mở lại ở `x.onload`/`x.onerror`) ⇒ không bấm được khi
>   chưa có file và không gửi trùng 2 lần.
> * **KHÔNG gọi request song song** (lỗi gặp thật: `/status` và `/files` chạy cùng lúc thì WebServer ESP32
>   — vốn chỉ phục vụ **1 request một lần** — làm request sau treo/timeout): mọi request đều đi qua
>   `apiFetch()` nối vào **một chuỗi Promise** (`apiQ`) nên request sau chỉ bắt đầu khi request trước **xong**.
>   Thêm nữa, `/files` **chỉ được gọi SAU khi `/status` kết thúc** (`poll(true)` → khi state `done`/`error`
>   mới `loadFiles()`), và **chỉ có 1 luồng `/status`** chạy tại một thời điểm (cờ `pollRun`).
> * 🔒 **BẢO MẬT TÊN FILE** (yêu cầu người dùng: *“tên file trên server cần fix bảo mật, chỉ cho phép
>   a-z0-9._”*): tên đến từ client (field `name` của multipart **hoặc** query `name=…` của `/delete`) là dữ
>   liệu **không đáng tin** ⇒ đi qua 2 lớp trong `cyd_sd.cpp`:
>   1. `cyd_sd_name_clean()` — bỏ hết phần đường dẫn (lấy sau dấu `/` cuối), đổi **mọi ký tự ngoài whitelist
>      thành `_`**, giới hạn độ dài, thêm `.gco` nếu chưa có đuôi;
>   2. `cyd_sd_name_check()` — **từ chối** nếu tên không rỗng/≤ 64 ký tự/không bắt đầu bằng `.`/không chứa
>      `..`/và mọi ký tự thuộc whitelist.
>   Whitelist = `a-z A-Z 0-9 . _` + `CYDLCD_NAME_ALLOW_EXTRA` (**mặc định `-`** vì thẻ đang có
>   `V300_case-esp-2432s208_no-text.gcode`; **muốn chặt đúng “chỉ a-z0-9._” thì đổi macro này thành `""`** —
>   một dòng ở `cydlcd_config.h`). Tập ký tự cho phép nay chỉ có **một định nghĩa** (`name_char_ok()`) dùng
>   chung cho cả lớp làm sạch và lớp kiểm tra.
>   *Lưu ý*: đây là lớp chặn ở **biên giới web**; nút XOA trên màn FILES (tên lấy từ danh sách của Marlin)
>   **không** bị giới hạn này, nếu không sẽ không xoá được các tên lạ như `V300_[a]_xy_…gc`.
>   **Đo trên máy thật**: `POST /delete` với `../cydlcd_tmp/x.tmp`, `a/b.gcode`, `evil;rm -rf.gcode`,
>   `.hidden`, `co dau cach.gcode`, `V300_[a]_xy.gc` ⇒ **HTTP 400** (kèm lý do); tên hợp lệ ⇒ qua được tới
>   bước “không thấy file” (500). Upload **thô qua socket** (bỏ qua script) với `name="../evil2.gcode"` +
>   `filename="../../hack.gcode"` ⇒ panel ghi ra **`/evil2.gcode`** (mất hết đường dẫn, không thoát khỏi gốc
>   thẻ) và log `CANH BAO bao mat: ten khach gui "…" -> lam sach thanh "…"` (log thật cũng bắt được lượt của
>   người dùng: `V300_[a]_xy_joint_cable_bridge_3hole.gc` → `V300__a__xy_joint_cable_bridge_3hole.gc`).
> * 📢 **THÔNG BÁO STATUS: KHÔNG hiện hộp thoại trên LCD nữa, chỉ hiện ở trang `/status`** (yêu cầu người
>   dùng: *“các hộp thoại thông báo status mà có nút ok và tự ẩn không hiển thị trên màn hình, chỉ hiển thị ở
>   trang status; ở trang status luôn lưu lại message lần cuối, sau thời gian timeout thì tự biến mất nếu có
>   time, không có time thì cứ để lại”*):
>   * Hộp **THÔNG BÁO** của Marlin (`cydlcd.message`, đúng loại “1 nút OK + tự ẩn sau 6 s”) nay **không tạo
>     msgbox** nữa — `ui_boxes.cpp` gọi `net_notify_set(msg, sev, CYDLCD_MSG_TTL_MS)` và log
>     `[UI] thong bao [INFO] -> TRANG /status (khong hien hop tren LCD)`. Muốn quay lại hành vi cũ: đặt
>     `CYDLCD_MSG_BOX_ON_LCD 1` (cydlcd_config.h).
>   * Kho thông báo (`net_upload_http.cpp`): **luôn giữ message LẦN CUỐI** (`notify_seq` tăng mỗi lần có cái
>     mới), `ttl_ms = 0` ⇒ **giữ mãi**; có `ttl_ms` ⇒ hết hạn thì **tự xoá** (log
>     `[HTTP] thong bao het thoi gian (6000 ms) -> xoa: "…"`).
>   * `/status` thêm `notify`, `notify_sev`, `notify_ttl_ms`, `notify_left_ms` (đếm ngược), `notify_seq`,
>     `msg_on_lcd`. Trang web hiện thông báo trong **khu riêng** (viền màu theo mức độ, kèm `[Ns]` đếm ngược),
>     tự ẩn khi hết; khi rảnh trang vẫn hỏi `/status` mỗi **5 s** để thông báo mới của Marlin hiện ra kịp thời.
>   * Chẩn đoán: `UI NOTIFY [ms] <text>` (COM8) — không tham số = in đang lưu; chỉ có số (`UI NOTIFY 0`) =
>     xoá; có `ms` = tự ẩn sau `ms`; `ms=0` = giữ mãi.
>   * **Đo trên máy thật**: `M500` (Marlin gửi 2 thông báo liên tiếp) ⇒ `/status` `notify='Da luu EEPROM'
>     left=2907 ttl=6000` → sau 7 s **rỗng** (tự hết hạn); đặt `ttl=30000` ⇒ `left` giảm 16896 → 12737;
>     `ttl=0` ⇒ vẫn còn sau nhiều lần đọc; `UI NOTIFY 0` ⇒ xoá ngay. **Trong lúc thông báo tới, `UI BOX` báo
>     `lop tren (hop thoai): 0 vat`** ⇒ màn hình LCD thật sự **không còn hộp thông báo nào**.
> * Hai endpoint mới (đăng ký ở `net_upload_http_begin()`): `GET /files` và `POST /delete` — xem bảng hợp
>   đồng ở §9.21. Cả hai đều cần token như `/upload`.
> * Duyệt thẻ: `cyd_sd_root_entry(idx, …)` (mở lại thư mục gốc mỗi lần gọi, **ẩn mục hệ thống** theo
>   `CYDLCD_HIDE_DIRS` — cùng quy tắc với màn FILES trên LCD), tối đa 64 mục.
> * Xoá: dùng đúng đường của nút XOA trên LCD — `cyd_sd_dir_count()` để biết là thư mục hay file →
>   `cyd_sd_delete()` (file) / `cyd_sd_delete_dir()` (thư mục **rỗng**, `SD.rmdir`). Sau khi xoá gọi
>   `cyd_sd_notify_marlin_remount()` = `M22`+`M21` + xin lại danh sách file, vì **cache FAT của Marlin đang
>   giữ trạng thái cũ** (không mount lại thì file đã xoá có thể “sống lại” khi Marlin ghi cache cũ lên thẻ).
> * **Chặn an toàn**: sai token (`401`) · đang có phiên upload (`409`) · Marlin đang in từ thẻ (`409`) ·
>   tên có `/`, `\`, `..` hoặc rỗng (`400`, chống xoá ra ngoài gốc thẻ) · thư mục còn mục con (`500`, không
>   xoá dữ liệu) · không thấy file (`500`).
> * **Đo trên máy thật**:
> ```text
> GET  /files?token=…            -> {"status":"Ok","sd":1,"count":10,"files":[{"n":"Body10.gcode","s":370404,"d":0}, …]}
> GET  /files (không token)      -> HTTP 401
> POST /delete?name=SPLIT3_TEST.gcode -> {"status":"Ok","message":"da xoa file SPLIT3_TEST.gcode"}
>      [UP] da xoa file /SPLIT3_TEST.gcode (cydlcd tu xoa - che do the qua UART)
>      [UP] bao Marlin mount lai the (M22/M21) + xin lai danh sach file
>      [MRL] … | SD r=14/0 …  ->  r=20/0  ->  r=32/0     (Marlin MOUNT LAI va doc sector that)
>      refused rd/wr=0/0                                  (không lần nào bị từ chối)
> POST /delete?name=../cydlcd_tmp/x.tmp -> HTTP 400     (chặn đường dẫn)
> POST /delete?name=KHONGCO.gcode       -> HTTP 500
> GET  /files?token=…            -> count 9  (SPLIT3_TEST.gcode đã biến mất)
> ```
> Sau đó (yêu cầu “chèn luôn giá trị token default vào input token”): `GET /` trả
> `<input type="text" id="tok" … value="TBT-3D-up">` — kiểm tra bằng cách tải trang và tìm `%%TOKEN%%`
> (không còn placeholder sót lại).
> Build panel: RAM 120 844 B, Flash **1 204 013 B** (sau khi chèn token: **1 203 853 B**).

* **Cờ**: `CYDLCD_WIFI_UPLOAD` (**mặc định 1 với CYD**), `CYDLCD_HTTP_PORT` 80, `CYDLCD_HTTP_TOKEN`
  (`TBT-3D-up`), `CYDLCD_HTTP_MDNS_NAME` (`TBTCYDLCD` ⇒ `http://TBTCYDLCD.local/`), `CYDLCD_HTTP_USE_MDNS` 1,
  `CYDLCD_HTTP_MAX_BYTES` 256 MB, `CYDLCD_HTTP_ALLOW_WHILE_PRINTING` 0,
  `CYDLCD_UPLOAD_TMP_DIR` = `/cydlcd_tmp` (Marlin chỉ liệt kê **gốc** thẻ nên không thấy thư mục này).
* **Hợp đồng HTTP** (đã chốt với nhánh script):

  | Endpoint | Trả về |
  |---|---|
  | `GET /` | HTML (chứa chuỗi `TBT-CYDLCD`): chọn file + nút Gửi + thanh tiến độ + poll `/status` |
  | `GET /status` | JSON: `"cydlcd":"TBT"`, `"device":"TBT-CYDLCD"`, `state` (`idle\|receiving\|writing\|done\|error`), `rx_bytes` (byte nhận từ mạng), `total_bytes` (tổng byte các part), `written_bytes` (byte ĐÃ GHI xuống thẻ của phiên đang chạy / phiên vừa xong), `parts` (số PHẦN FILE của request), `parts_done` (số part đã đóng), `chunks`, `declared` (size khách khai báo), `file`, `result`, `message`, **`notify`/`notify_sev`/`notify_ttl_ms`/`notify_left_ms`/`notify_seq`/`msg_on_lcd`** (thông báo status — xem §9.21 vòng sửa (e)), `sd`, `wifi`, `ip`, `heap` — **trả được cả khi chưa có phiên** (`state=idle`) |
  | `GET /files?token=…` | JSON danh sách **gốc thẻ** cho phần “File trên thẻ SD” của trang web: `{"status":"Ok","sd":0\|1,"count":N,"files":[{"n":"<tên>","s":<byte>,"d":0\|1},…]}` (`d=1` = thư mục). **Ẩn mục hệ thống** (`cydlcd_tmp`, `BTUP`) giống màn FILES trên LCD; đọc bằng FATFS **của màn hình** nên không cần Marlin. Sai token ⇒ `401` |
  | `POST /delete?token=…&name=<tên>` | Xoá **file** ở gốc thẻ, hoặc **thư mục RỒNG** (dùng `SD.rmdir`, không xoá được thư mục còn mục con). Xong thì panel **gửi `M22`+`M21`** để Marlin mount lại (FAT của Marlin đang cache trạng thái cũ — không làm bước này thì file đã xoá có thể “sống lại”/hiện lại trong danh sách). Mã lỗi: `401` sai token · `409` đang có phiên upload **hoặc** Marlin đang in từ thẻ · `400` tên không hợp lệ (có `/`, `\`, `..`, rỗng) · `500` không xoá được (không thấy file, thư mục còn mục con) |
  | `POST /upload?token=…` | `multipart/form-data`: các field text `name="name"` (tên đích) + `name="size"` (**tổng số byte**, đặt TRƯỚC phần file) rồi phần file `name="file"` `filename="<tên>"` → `200 {"status":"Ok",…,"parts":N}` / `{"status":"Error","message":"…"}` (`401` sai token, `409` Marlin đang in, `400/500` lỗi thẻ TF / size không khớp). **Nhiều phần file trong cùng request được chấp nhận**: mỗi phần là một file riêng `_001/_002/…` đóng ngay, hết request thì **đổi tên thẳng** (1 phần) hoặc **GHEP lại** (≥2 phần) rồi kiểm tra size thật trên thẻ — xem vòng sửa 2026-10-15 (b) ở §9.21 |

* **Token**: query `?token=` **hoặc** header `X-CYDLCD-Token` (`CYDLCD_HTTP_TOKEN ""` = tắt kiểm, không khuyến nghị).
* **Trả lời ngay** sau khi nhận đủ file; bước **ghi vào thẻ** (`cyd_sd_finish_upload()`) chạy ở nhịp sau của
  `net_upload_http_task()` (có lớp phủ + khoá UI — §9.18) ⇒ trình duyệt không bị treo; client theo dõi qua
  `/status` (`receiving` → `writing` → `done`/`error`).
* **Không cần WiFi để “chốt” phiên**: thẻ nằm ở khe màn hình nên ghi thẻ chạy ngay khi file tạm nhận xong —
  WiFi chỉ là đường **nhận file từ trình duyệt**. Màn hình chỉ cần ở **cùng mạng nhà** (STA) với PC/điện thoại.
* ⚠️ **`Content-Disposition`**: phía màn hình CYD parse tên file bằng `unquote()` — nhận **có/không có** dấu
  `"` bao quanh, KHÔNG dùng cách cắt chuỗi `substring(idx+2, len-1)` (bài học đầy đủ ở **§10.3**).
* **Test nhanh**: xem COM7 (`[UP] wifi=2 ssid="…" ip=192.168.x.y …`) hoặc gõ `WIFI?` / `WIFI SCAN` → trên PC mở
  `http://TBTCYDLCD.local/?token=TBT-3D-up` (hoặc `http://<ip>/`) → chọn file `.gco` → Gửi → theo dõi
  `state`/`written_bytes`; kiểm Marlin thấy file mới (M22/M21 đã gửi) và `/cydlcd_tmp` sạch file tạm.
### 9.22 PRESET PREHEAT: DANH SÁCH VẬT LIỆU do người dùng THÊM/BỚT (NVS `ph2`) — 2026-10-15

**Yêu cầu người dùng 2026-10-15:** *"trong phần setting có menu preheat lặp lại rất nhiều lần, gom vào 1
menu là preheat, trong preheat cho phép user thêm bớt các loại vật liệu. Max là 6 vì trang temperature kín
rồi, trong trang temperature bỏ nút turning để có chỗ render preheat… nếu có thể gộp 3 thuộc tính của
preheat trên 1 dòng để điều chỉnh thì không cần vào menu con để sửa nữa"*.

| | Trước 2026-10-15 | Nay |
|---|---|---|
| Cấu trúc | **3 preset cố định** PLA/PETG/ABS, hiện **3 nhóm** `Preheat … Settings` trong menu CONFIGURATION **và 9 dòng** ở bảng THONG SO (lặp lại 2 nơi) | **1 nhóm `Preheat`** trong CONFIGURATION: mỗi vật liệu **1 dòng** hiện `TÊN  nozzle/bed/fan`; bảng THONG SO **bỏ hẳn 9 dòng** preheat |
| Thêm/bớt | không được | dòng `+ Thêm vật liệu` (bàn phím LVGL đặt tên, tối đa 8 ký tự) + nút `Xóa` trong hộp sửa (xác nhận **2 bước**); **tối đa `CYDLCD_PRE_MAX` = 6** (trang NHIỆT ĐỘ chỉ đủ 6 nút); **không cho xóa vật liệu cuối cùng** |
| Sửa | vào menu con rồi sửa từng dòng (nozzle / bed / fan riêng) | **hộp 3-trong-1** (`ui_page_preheat.cpp`): 3 ô `NOZ`/`BED`/`FAN` **cùng 1 hàng** — chạm ô nào thì `-`/`+` chỉnh ô đó; 3 mức bước (nhiệt độ `1/5/10`, quạt `1/5/25`); `OK` · `Hủy` · `Xóa` |
| Trang NHIỆT ĐỘ | 1 hàng 3 nút `PLA/PETG/ABS (kèm số)` + 1 hàng `TUNING (MPC/PID)` | **2 hàng × 3 nút** = tối đa 6 vật liệu (nút chỉ hiện **TÊN**, số hiện ở dòng `Profile:` sau khi bấm); **hàng `TUNING` đã bỏ**, và **trang TUNING cũng bỏ hẳn** (2026-10-15) — tuning MPC/PID chỉ còn ở Configuration → Advanced Settings → Temperature |

| | |
|---|---|
| Kho giá trị | NVS `Preferences`, namespace **`"ph2"`**: `n` (số vật liệu), `t<i>` (tên), `h<i>` / `b<i>` / `f<i>` (nozzle/bed/fan) |
| **Di trú** | lần chạy đầu sau khi nâng cấp: nếu `"ph2"` chưa có → đọc **namespace cũ `"ph"`** (`h0..h2`/`b0..b2`/`f0..f2` = 3 preset PLA/PETG/ABS) rồi ghi sang `"ph2"` ⇒ **không mất giá trị đã đặt trước đó** |
| Giới hạn | nozzle 0..300 °C · bed 0..120 °C · fan 0..255 (0 = **không** đụng tới quạt khi preheat) |
| API | `ui_preheat_count/name/get/set/add/remove/rename/rev` (`ui_lvgl.h`) — mọi trang đọc/ghi qua đây, chỉ `ui_preheat.cpp` giữ mảng `s_pre[]` |
| Vẽ lại | `ui_preheat_rev()` tăng mỗi lần đổi ⇒ menu CONFIGURATION và trang NHIỆT ĐỘ tự `page_rebuild()` khi số vật liệu đổi (thêm/bớt) |
| Nút BACK | đóng **lớp phủ trước** theo thứ tự: bàn phím đặt tên → hộp sửa 3-trong-1 → lớp phủ sửa số (THONG SO), rồi mới lên nhóm cha (nếu không, BACK vừa đổi nhóm vừa để hộp mở trên màn mới — lỗi đã gặp 2026-10-15) |
| Lệnh console | COM8: `UI PRE` (in danh sách) · `UI PRE ADD <tên>` · `UI PRE DEL <i>` · `UI PRE SET <i> <h> <b> <f>` · `UI PRE NAME <i> <tên>` · `UI PRE OPEN <i>` (mở hộp 3-trong-1) · `UI PRE KB` (mở bàn phím) · `UI PRE <k>` (bấm nút preheat thứ k ở trang NHIỆT ĐỘ); COM7: `PRE?` · `PRE <i> <h> <b> [fan]` · `PRE ADD/DEL/NAME` |


**Bối cảnh (bản cũ 2026-10-14, đã thay bằng bảng ở trên):** 3 nút `PLA`/`PETG`/`ABS` ở trang TEMP trước
đây **cứng trong code** (`210/60`, `240/80`, `250/100`); sau đó thành **3 preset sửa được** lưu NVS namespace
`"ph"` và hiện ở **9 dòng của bảng SETTINGS** (id cục bộ `0x4001..0x4009`, màn hình tự bơm vào
`cydlcd.settings[]` bằng `preheat_inject()`, `se_send_mcode()` thấy id đó thì **không gửi M-code** mà chỉ kẹp
trần → lưu NVS). Từ **2026-10-15** thiết kế đó bị thay bằng **danh sách vật liệu động** ở trên:
`preheat_inject()` + 9 dòng SETTINGS + nhánh `se_send_mcode()` cho `0x40xx` **đã xoá**, dữ liệu cũ được
**di trú tự động** sang `"ph2"`.

* **Vì sao KHÔNG đẩy sang Marlin/EEPROM**: build này **`HAS_PREHEAT = 0`** (không có `HAS_MARLINUI_MENU`) ⇒
  `M145` không được biên dịch và preset không nằm trong `settings_t`; bật lên sẽ **đổi kích thước `settings_t`
  ⇒ đổi version/CRC EEPROM ⇒ Marlin nạp lại mặc định**, mất `M851`/PID/`M92`/`M201`/`M203`/`M205`/`M906` + lưới UBL.
  Giữ preset ở màn hình CYD là **rẻ nhất và không rủi ro**.
* **Nút TEMP gửi gì**: `M104 S<nozzle>` + `M140 S<bed>`, và `M106 S<fan>` **chỉ khi** fan preset > 0
  (fan = 0 nghĩa là "không đụng tới quạt", tránh `M106 S0` tắt quạt ngoài ý muốn).
* **Đã kiểm trên máy thật (COM7, 2026-10-14)**:
  ```
  >>> PRE?
  [PRE] 0) PLA  nozzle=200 C  bed=60 C  fan=0        <- mặc định khi NVS trống
  >>> PRE 0 215 65 30
  [PRE] da luu 3 preset preheat vao NVS (ph)
  [PRE] dat preset 0 (PLA): nozzle=215 bed=65 fan=30 -> OK
  >>> PRE 1 999 0 0
  [PRE] dat PETG: nozzle=300 bed=0 fan=0              <- 999 bi kep ve tran 300
  ```
  Nạp lại firmware (KHÔNG xoá NVS) → log khởi động:
  `[PRE] preset: PLA 215/65/30 | PETG 300/0/0 | ABS 250/100/40` ⇒ **giá trị sống qua reset**;
  `[CYD] reset reason = 1 (poweron)`, `link PUSH=1 TBT=1 rx=1 err=0` ⇒ khởi động sạch.
  ⚠️ `PRE <i>` chỉ **đổi preset**, KHÔNG gia nhiệt, KHÔNG chuyển động (an toàn để test khi máy nguội).
* 🐞 **Bẫy đã gặp**: lệnh `PRE` lúc đầu đặt **lồng bên trong nhánh `WIFI`** của console nên `PRE?` không khớp,
  bị **chuyển tiếp sang Marlin** (im lặng, coi như G-code lạ). Đã tách thành nhánh **ngang cấp** với
  `WIFI`/`PRE` (`main.cpp`) — bài học: nhánh con phải so `s_fwd` chứ không phải `arg` đã trừ tiền tố.


### 9.10 🔴 PANIC THẬT 2026-10-14 (`StoreProhibited EXCVADDR=0x00000000`) — nguyên nhân & cách chặn

> **Ghi chú lịch sử (đọc kèm §9.23)** — panic này xảy ra lúc màn hình còn **hai stack radio**
> (WiFi + Bluetooth Classic/Bluedroid). Đường Bluetooth **đã xoá 2026-10-14** nên **nguyên nhân trực tiếp
> không còn tồn tại**, nhưng **thứ tự khởi động** rút ra từ đây thì vẫn đang áp dụng.

**Hiện tượng (log COM7 máy thật):** sau khi WiFi bắt đầu kết nối + mount thẻ thất bại, màn hình CYD
**reboot loop**:

```
Guru Meditation Error: Core 0 panic'ed (StoreProhibited). EXCVADDR: 0x00000000
Backtrace: 0x40091e09(memset) <- bta_sys_init (bta_sys_main.c:171)
           <- btu_task_start_up (btu_task.c:276) <- osi_thread_run (osi/thread.c:165)
```
Backtrace trỏ vào **Bluedroid** ⇒ chết ngay trong lúc stack Bluetooth khởi động, không phải lỗi UI.

**Nguyên nhân (chứng minh bằng mã + disassembly thư viện đã cài):**

| Bằng chứng | Ý nghĩa |
|---|---|
| `bta_sys_int.h`: `extern tBTA_SYS_CB *bta_sys_cb_ptr;` + `#define bta_sys_cb (*bta_sys_cb_ptr)` | `bta_sys_cb` là **con trỏ cấp phát động** (khi `BTA_DYNAMIC_MEMORY != FALSE`) |
| `nm libbt.a` → `bta_sys_main.c.obj: 00000004 C bta_sys_cb_ptr` | Là **COMMON/BSS** ⇒ bằng **0** lúc boot, phải được gán bằng `osi_malloc` |
| Disassembly `bta_sys_init` (libbt.a): `movi a12,0x130` (size), `movi a11,0` (giá trị), `l32i a10,a2,0` (**a10 = *bta_sys_cb_ptr**), `callx8 memset` | `memset(*bta_sys_cb_ptr, 0, 0x130)`: con trỏ còn **NULL** ⇒ **ghi vào địa chỉ 0** = đúng `StoreProhibited EXCVADDR=0x00000000` |
| Điểm cấp phát: `bte_init.c.obj` (`BTE_InitStack`) có literal trỏ tới `bta_sys_cb_ptr` cùng loạt `bta_dm_cb_ptr`, `avdt_cb_ptr`… | Bluedroid cấp phát cả loạt CB động **không kiểm tra NULL** ⇒ thiếu RAM/phân mảnh là **PANIC**, không phải lỗi trả về |
| `firmware.elf` trong `.pio/build` có `.flash.text` kết thúc `0x401178A3`, **không có** symbol `bta_sys_init`, còn backtrace trỏ `0x4015b630` | **ELF đó KHÔNG khớp firmware đã nạp** ⇒ muốn tra backtrace phải build sạch và giữ đúng ELF |

**Thủ phạm thực tế (thứ tự khởi tạo):** code cũ gọi `WiFi.mode()+WiFi.begin()` ở **boot**, sau đó
`SD.begin()` thất bại, rồi mới bật stack Bluetooth ⇒ **hai stack radio khởi tạo cùng lúc**
(log `E (3457) wifi:Set status to INIT` ngay trước panic) và WiFi đã ăn ~40-50 KB heap **trước khi**
Bluedroid cấp phát CB của nó.

**Quy tắc rút ra — vẫn đang áp dụng trong `net_wifi_begin()` / `net_upload_http_begin()` / `cyd_sd_begin()`:**

1. **Khởi động tuần tự, không chờ mạng**: `net_wifi_begin()` chỉ đọc NVS + gọi `WiFi.begin()` (**không chặn**),
   rồi `net_upload_http_begin()` mới mount thẻ (`cyd_sd_begin()`) + mở WebServer + mDNS. Không bước nào chờ
   mạng xong ⇒ boot **không phụ thuộc router**.
2. **Thẻ SD trước mọi việc cần thẻ**: `cyd_sd_begin()` mount thẻ; không mount được thì **không chặn boot** —
   chỉ log `[UP] CANH BAO: chua mount duoc the TF (cam the FAT32 - se tu thu lai moi 2,5 s)` và để
   `cyd_sd_task()` thử lại (§9.12).
3. **`sd_mount()` leo thang tốc độ 20 → 10 → 4 MHz**, log từng bước + `cardType()/totalBytes()` khi thành
   công; thất bại thì in gợi ý: cắm thẻ / **thẻ phải FAT32** / khe TF-bus VSPI 18/19/23/5.
4. **Không bao giờ để hai stack radio khởi tạo cùng lúc** — đây chính là điều đã dẫn tới việc **xoá hẳn
   đường Bluetooth** (2026-10-14, §9.23). Nay WiFi là stack radio **duy nhất**, do một mình `net_wifi.cpp`
   quản lý (§9.20).
5. Mọi `File` (`SD.open`/`openNextFile`) đều được kiểm tra trước khi dùng; `clean_dir()`/`cyd_sd_temp_open()`
   thoát ngay nếu `!s_sd_ready`.

**Đánh đổi (nay đã hết phần lớn):** trước đây icon WiFi **xám (0)** gần như suốt thời gian và mỗi phiên phải
chờ WiFi nối lại ~2-4 s. Nay WiFi bật từ boot nên icon ở header phản ánh **đúng** trạng thái ngay từ đầu —
chấp nhận đánh đổi này để màn hình CYD **không thể chết vì khởi tạo radio**.

**Bus SPI của thẻ không xung đột với LCD (đã kiểm lại):** `User_Setup.h:57` `#define USE_HSPI_PORT`
⇒ TFT_eSPI chạy **HSPI**; nhánh CYD của `ui_port_cyd_st7789.cpp` chỉ dùng `s_lcd` (TFT_eSPI) + cảm ứng
bit-bang 25/32/39/33/36, còn `SPI.beginTransaction` nằm trong nhánh **STM32**
(`ui_port_stm32_ili9341.cpp`, `#endif // CYDLCD_USE_LVGL && !CYDLCD_BOARD_CYD`) ⇒ thẻ TF trên
VSPI 18/19/23/5 là bus riêng.

### 9.12 THE TF NÓNG: rút ra / cắm vào không cần reset

| Việc | Chi tiết |
|---|---|
| Kiểm thẻ còn đó | mỗi **1 s** (`CYDLCD_SD_CHECK_MS`), **chỉ khi rảnh** — **không** kiểm giữa lúc đang nhận/đang ghi thẻ để không tranh SPI với chính phiên upload |
| Phép kiểm | `SD.exists("/")` = **1 lần `f_stat` thư mục gốc** (~0,3–1 ms SPI, ~0,1 % CPU ở nhịp 1 s). **Không** dùng `SD.cardType()` vì nó trả giá trị cache |
| Rút thẻ | `[UP] MAT THE TF: <lý do>` + `SD.end()` (ép unmount — nếu không, `SD.begin()` lần sau trả `true` ngay mà không mount lại) + nếu đang có phiên thì `cyd_sd_temp_write()` fail ⇒ phiên bị huỷ + xoá file tạm |
| Cắm lại | tự thử mount lại mỗi **2,5 s** (`CYDLCD_SD_RETRY_MS`), log khi thành công; thất bại thì **im lặng** (chỉ log lần đầu + mỗi lần đổi trạng thái, không spam) |
| Khi bắt đầu phiên upload | `cyd_sd_temp_open()` mount **ngay** + đòi `cydlcd.linked`; không được ⇒ lý do nằm ở `s_err` (`chua lien lac duoc Marlin` / `the TF cydlcd loi` / `Marlin dang in - khong dung the` …) và `/status` trả trong `message` |
| Cho UI | `cyd_sd_ready()` (cache, không SPI) và `cyd_sd_retry()` (chỉ bật cờ, mount thật chạy ở vòng lặp sau — không chặn UI) |
| Log 1 Hz | có thêm cờ `sd=1/0` |

Các phiên upload vẫn luôn kiểm: Marlin **không đang in**, còn **liên lạc** được với Marlin (`cydlcd.linked`),
mount được thẻ, và còn chỗ (**chừa 256 KB** so với kích thước khai trong `Content-Length`).

> 🚀 **Cách upload duy nhất: xem §10** — PC/điện thoại gửi file cho **web server của màn hình CYD**
> (`cydlcd-upload.ps1` / hàm `in3d`) trong mạng nhà; màn hình ghi vào thẻ ở khe màn hình rồi **tự gửi
> `M22`/`M21`** cho Marlin. Tự dò thiết bị, theo dõi tiến độ qua `GET /status`.

> 🆕 **Dò thiết bị theo **MAC** (2026-10-15) — ĐÃ BỎ CACHE IP** (yêu cầu người dùng): IP của màn hình do DHCP cấp
> nên **có thể đổi**, còn **MAC WiFi thì cố định** ⇒ mỗi lần upload chỉ cần tìm IP nào đang có đúng MAC đó.
> MAC hiện tại:**`48:AF:F3:70:B1:F8`** (đổi được bằng `-Mac`, đo lại bằng `arp -a | findstr <IP>`).
> Thứ tự dò: `-Host` → **MAC** (đọc bảng ARP; chưa có thì **ping quét song song cả /24 ~1,5 s** rồi đọc lại) →
> mDNS → gateway → quét LAN → danh sách cố định. **Không còn file `last-target.txt`**, không bao giờ dùng lại IP cũ.
> Đo thực tế: `[2/6] tim IP theo MAC 48AFF370B1F8` → `MAC = IP 192.168.0.146` trong **0,2 s**.


### 9.23 BẢNG ĐỔI TÊN `panel_*` → `cydlcd_*` + ĐÃ XOÁ 2 đường upload cũ (2026-10-14)

**Ngày 2026-10-14 đã XOÁ HẲN hai đường upload** — code bị xoá, không còn nhánh `#if`/`#else` nào bật lại được:

| Đã xoá | Gồm những gì |
|---|---|
| **Upload qua module WiFi ESP3D** | `net_esp3d.{cpp,h}` (phần POST multipart `/files` + probe `[ESP800]`), `tools/esp3d-upload.ps1`, khối macro `PANEL_ESP3D_*`, target `-Target esp3d`; AP `Voron2308` / IP `192.168.0.1` không còn dùng |
| **Upload qua Bluetooth SPP** | `panel_bt_upload.{cpp,h}`, `tools/bt-upload.ps1`, `tools/bt-upload-socket.ps1`, khối macro `PANEL_BT_*`, tên thiết bị / PIN / token của đường Bluetooth |

**Lý do**: (1) WiFi nhanh hơn **hàng trăm lần** (BT bị trần ~6 KB/s vì queue 512 B của Bluedroid);
(2) tiết kiệm **~100 KB heap** (Bluedroid Classic) + **~600 KB flash**; (3) bớt **2 đường bảo trì**
(mỗi đường có giao thức, script và cách chẩn đoán riêng) — và chính việc chạy hai stack radio cùng lúc là
**nguyên nhân panic** ở §9.10.

⚠️ **Tool CŨ không còn tồn tại**: `panel-upload.ps1`, `bt-upload.ps1`, `bt-upload-socket.ps1`,
`esp3d-upload.ps1` — dùng **`cydlcd/tools/cydlcd-upload.ps1`** (chỉ còn `-Target cydlcd`).

**Bảng đổi tên (cũ → mới)** — dùng khi đọc log / tài liệu / ghi chú cũ:

| Cũ | Mới |
|---|---|
| thư mục `lcd_panel/` | `cydlcd/` |
| env `panel_esp32_cyd` · `panel_f411cc` · `panel_f103c6` · `panel_f411cc_lvgl` · `panel_f407vgz_lvgl` | `cydlcd_esp32` · `cydlcd_f411cc` · `cydlcd_f103c6` · `cydlcd_f411cc_lvgl` · `cydlcd_f407vgz_lvgl` |
| `panel_config.h` · `panel_link.{cpp,h}` · `panel_protocol.h` · `panel_port.h` · `lv_conf_panel.h` | `cydlcd_config.h` · `cydlcd_link.{cpp,h}` · `cydlcd_protocol.h` · `cydlcd_port.h` · `lv_conf_cydlcd.h` |
| macro `PANEL_*` · hàm/biến `panel_*` · biến toàn cục `panel` | `CYDLCD_*` · `cydlcd_*` · `cydlcd` |
| `net_esp3d.{cpp,h}` = tầng thẻ SD (mount, sector thô, file tạm, bộ đếm + log 1 Hz) | **`cyd_sd.{cpp,h}`** |
| Đổi tên hàm/biến (trong 2 file trên): `begin/task/log` · `ready/mount_now/retry` · `temp_open/write/close/abort/path/bytes` · `push` · `raw_read/raw_write/delete` · `net_printer_busy` | `cyd_sd_begin/task/log` · `cyd_sd_ready/mount_now/retry` · `cyd_sd_temp_open/write/close/abort/path/bytes` · **`cyd_sd_finish_upload`** · `cyd_sd_raw_read/raw_write/delete` · `cyd_sd_printer_busy` (thêm `cyd_sd_last_result/last_written`) |
| `ui_lvgl_port.cpp` (1 file cho mọi board) | **tách 2 file**: `ui_port_cyd_st7789.cpp` (CYD: TFT_eSPI + **ST7789** + cảm ứng XPT2046 bit-bang + hiệu chuẩn) và `ui_port_stm32_ili9341.cpp` (STM32 F411/F407: driver tự viết + encoder) |
| `tft_ili9341.{cpp,h}` | **giữ nguyên tên** — nó ĐÚNG là ILI9341 và **chỉ** dùng cho 4 env STM32; CYD dùng **ST7789** |
| `tools/panel-upload.ps1` · `panel-log.ps1` · `set_panel_mode.ps1` · `test_panel_link.cpp` | `tools/cydlcd-upload.ps1` · `cydlcd-log.ps1` · `set_cydlcd_mode.ps1` · `test_cydlcd_link.cpp` |
| `tools/tft_setup/` (tên file `User_Setup.h` giữ nguyên — TFT_eSPI include đúng tên đó) | `tools/tft_setup_cyd/` |
| mDNS `TBTPANEL` (`http://TBTPANEL.local/`) · chuỗi nhận dạng trong trang HTML `TBT-PANEL` | **`TBTCYDLCD`** (`http://TBTCYDLCD.local/`) · **`TBT-CYDLCD`** |
| `/status` JSON `{"panel":"TBT",…}` · trường `posted_bytes` · giá trị `state` `posting` | `{"cydlcd":"TBT","device":"TBT-CYDLCD",…}` · `written_bytes` · `writing` (bộ giá trị: `idle\|receiving\|writing\|done\|error`) |
| token HTTP `TBT-3D-up` | **giữ nguyên** `TBT-3D-up` (client gửi qua `?token=` hoặc header `X-CYDLCD-Token`) |
| cache IP của tool: `%LOCALAPPDATA%\esp3d-upload` | `%LOCALAPPDATA%\cydlcd-upload\last-target.txt` (khoá `cydlcd=<ip>`; tool tự di trú cache cũ) |

**Đổi tên tiếp 2026-10-14 — phần link TRAMMING của dự án → `TBT` (tên máy in)**:

| Cũ | Mới |
|---|---|
| `tram_link.{cpp,h}` · `tram_protocol.h` | **`tbt_link.{cpp,h}`** · **`tbt_protocol.h`** |
| Marlin: `cydlcd_tram_link.{cpp,h}` · `tramming_report.{cpp,h}` · `tramming_run.{cpp,h}` | **`cydlcd_tbt_link.{cpp,h}`** · **`tbt_report.{cpp,h}`** · **`tbt_run.{cpp,h}`** |
| macro/hàm `TRAM_*` · `tram_*` · biến toàn cục `tram` · trang `PG_TRAM` | `TBT_*` · `tbt_*` · `tbt` · `PG_TBT` |
| cờ `CYDLCD_TRAM_LINK` · `CYDLCD_TRAM_BAUD` | **`CYDLCD_TBT_LINK`** · `CYDLCD_TBT_BAUD` |
| khung `CYDLCD_F_TRAM` · `CYDLCD_F_TRAM_CMD` (giá trị enum **không đổi**) | `CYDLCD_F_TBT` · `CYDLCD_F_TBT_CMD` |
| dòng ASCII trên dây `TRAM <phase> …` | **`TBT <phase> …`** (đổi **cả 2 đầu**; bộ dò token cũng thành `T`,`B`,`T`) |
| nhãn log `[CYD] loop OK … TRAM=1 …` | `… TBT=1 …` |

> ⚠️ **Vì sao `TBT`, không phải `TRAM`?** `TRAM` = **tramming** — tên **tính năng** của Marlin
> (`ASSISTED_TRAMMING` + `ASSISTED_TRAMMING_WIZARD`, lệnh `G35`, menu *Tramming Wizard*), **không phải**
> tên máy in. Phần **do dự án này viết** (link ASCII + state machine + khung PUSH) đã đổi sang **`TBT`**
> = tên máy in (`CUSTOM_MACHINE_NAME "TBT"`), còn **file gốc Marlin** (`feature/tramming.*`,
> `menu_tramming.cpp`, `TRAMMING_POINT_XY`, nhãn *TRAMMING* trên UI) **giữ nguyên** để không lệch upstream.
> Đổi token trên dây (`TRAM`→`TBT`) ⇒ **phải nạp cả 2 board** khi dùng chế độ `CYDLCD_TBT_LINK`
> (chế độ đang dùng là PUSH nên **khung PUSH không đổi byte**).
| file tạm trên thẻ `/btup/<ten>.tmp` | `/cydlcd_tmp/<ten>.tmp` (`CYDLCD_UPLOAD_TMP_DIR`) |

> Phía **Marlin** (không đổi trong lần này): ExtUI `cydlcd_serial` (`Marlin/src/lcd/extui/cydlcd_serial/`),
> `Marlin/src/sd/cydlcd_sd_uart.{cpp,h}`, `Marlin/src/feature/cydlcd_tbt_link.{cpp,h}`; cờ
> `CYDLCD_SERIAL_UI`, `CYDLCD_TBT_LINK`, `CYDLCD_SD_OVER_UART`.

---

## 10. Upload G-code qua **WiFi** — PC → **web server của màn hình CYD** → **thẻ SD**

> **Chỉ còn MỘT đường**: PC/điện thoại → WiFi → web server trên màn hình CYD → màn hình ghi vào thẻ ở khe
> màn hình → màn hình gửi `M22`+`M21` để Marlin mount lại. Hai đường cũ (module WiFi rời và Bluetooth)
> **đã xoá hẳn 2026-10-14** — xem **§9.23**. Đường này nhanh hơn hàng trăm lần và **tự tìm máy in**.

Một script duy nhất:

| Script | Việc |
|---|---|
| `cydlcd/tools/cydlcd-upload.ps1` | Gửi file lên **web server của màn hình CYD**. `-Target` chỉ nhận `cydlcd` (mặc định); mọi target khác **báo lỗi rõ ràng** ngay khi bind tham số (đường cũ đã xoá 2026-10-14 — §9.23) |

### 10.1 Chọn đường nào

Chỉ còn **một** đường: `cydlcd-upload.ps1 -File <file>` (mặc định `-Target cydlcd`).

| | **PC → web server màn hình CYD → thẻ SD** |
|---|---|
| Đường đi | PC → **HTTP server của màn hình CYD** (`POST /upload`) → màn hình ghi `/cydlcd_tmp/<tên>.tmp` → đổi tên vào **gốc thẻ** → **Marlin đọc thẻ qua UART** (§12) |
| PC phải ở mạng nào | **Cùng mạng WiFi với màn hình CYD** (mạng nhà `TP-Link_31A0`) — **không phải đổi mạng** |
| `M22` + `M21` | **Màn hình tự gửi** sau khi ghi xong ⇒ **không cần `-InitSd`** |
| Theo dõi tiến độ | **Có**: `GET /status` (`receiving` → `writing` → `done`/`error`, `rx_bytes`/`written_bytes`) |
| Nhận dạng thiết bị | `GET /status` ⇒ JSON `{"cydlcd":"TBT"}` / `{"device":"TBT-CYDLCD"}` (hoặc `GET /` chứa chuỗi `TBT-CYDLCD`) |

### 10.2 Hợp đồng HTTP của MÀN HÌNH CYD (client phải khớp firmware)

| Endpoint | Nội dung |
|---|---|
| `GET /status` | `200 application/json`. **Phải trả JSON cả khi rảnh** (đang init / không có phiên ⇒ `state:"idle"`), vì client dùng chính nó để **nhận dạng màn hình CYD**. Trường nhận dạng: **`"cydlcd":"TBT"`** và **`"device":"TBT-CYDLCD"`** (hoặc chuỗi `TBT-CYDLCD` trong body). Trường tiến độ: `state` (`idle\|receiving\|writing\|done\|error`), `rx_bytes`, `total_bytes`, `written_bytes`, `file`, `result`/`message` — kèm `sd`, `wifi`, `ip`, `heap` |
| `GET /` | Trang HTML cho điện thoại/PC gửi file (không cần app), **có chứa chuỗi `TBT-CYDLCD`** (dự phòng nhận dạng) |
| `POST /upload?token=<token>` | `multipart/form-data`; field text **`name="name"`** = tên file đích (**đứng TRƯỚC**), phần file **`name="file"` + `filename="<tên>"`** ⇒ `200 {"status":"Ok"}` hoặc `{"status":"Error","message":"..."}` (mã lỗi: `401` sai token · `409` Marlin đang in · `400/500` lỗi thẻ/ghi) |
| Header | Client gửi kèm **`X-CYDLCD-Token: <token>`** (firmware nhận **cả** `?token=` lẫn header) |
| Token | `TBT-3D-up` (= `CYDLCD_HTTP_TOKEN` trong `cydlcd_config.h`); đổi được lúc chạy: `-Token <khác>` |

### 10.3 ⚠️⚠️ BÀI HỌC QUAN TRỌNG: mọi giá trị trong `Content-Disposition` **PHẢI có dấu `"`**

`Parsing.cpp` của **ESP32 core 2.0.17** (thư viện mà màn hình CYD dùng) cắt chuỗi theo dấu `=` **ĐẦU TIÊN**,
**luôn bỏ 1 ký tự ngay sau `=`**, rồi bỏ **ký tự cuối**:

```
argName     = line.substring(line.indexOf('=') + 2);            // bỏ luôn 1 ký tự sau '='
argFilename = argName.substring(idx + 2, argName.length() - 1); // bỏ luôn ký tự cuối
```

| `Content-Disposition` gửi đi | `upload.filename` nhận được | Kết quả |
|---|---|---|
| `name="file"; filename="benchy.gcode"` | `benchy.gcode` | ✅ đúng |
| `name=file; filename=benchy.gcode` | `enchy.gcod` | ❌ **mất ký tự đầu + cuối** (file lên thẻ SD sai tên) |

⇒ **Windows PowerShell 5.1 / .NET Framework sinh `Content-Disposition` KHÔNG có dấu `"`** cho các
giá trị dạng token (`name`, `file`, `benchy.gcode`) ⇒ **KHÔNG dùng `Invoke-WebRequest -Form` /
`MultipartFormDataContent`** (kể cả `-Form` cũng chỉ có từ PowerShell 6.1). `cydlcd-upload.ps1` vì
vậy **tự dựng body multipart bằng byte** (ASCII) và gửi bằng `TcpClient` thô: `Content-Length` tự
tính, **không** chunked, **không** `Expect: 100-continue`, và in được tiến độ %/KB/s.

Kiểm chứng (đã chạy, xem §10.7): một parser chép đúng logic trên chạy trong Node cho ra
`{"argName":"file","argFilename":"enchy.gcod"}` với body **thiếu dấu `"`**.

### 10.4 Cách dò thiết bị (`cydlcd-upload.ps1`)

Thứ tự (bỏ qua khi có `-NoDiscover`), mỗi ứng viên timeout **1–1,5 s**, bước quét có trần
`-ScanBudgetSec` (mặc định 25 s). Nguồn tìm được in ra ở dòng `[cydlcd] thay <IP> (nguon: ...)`:

1. `-Host` (nếu có) — **vẫn xác nhận** chữ ký;
2. **CACHE** — IP của lần tìm **thành công gần nhất** (thêm 2026-10-14): bình thường chỉ mất
   **~0,2–0,5 s** là xong, **không** phải mDNS / quét LAN (17–27 s). IP cũ hoặc máy đổi IP thì tự
   rơi xuống các bước dưới rồi **ghi đè cache**. Tắt bằng `-NoCache`;
3. **mDNS**: `TBTCYDLCD.local`, `tbtcydlcd.local`; thêm **tên CŨ** `TBTPANEL.local` / `tbtpanel.local`
   (firmware trước 2026-10-14) để tương thích ngược;
4. **gateway** của card mạng đang dùng (một số mạng đặt màn hình làm gateway nội bộ);
5. **quét LAN** `/24` của card đang hoạt động, **song song** `-ScanThreads` luồng (mặc định 40):
   lọc “port 80 mở” trước, rồi mới **xác nhận chữ ký** ⇒ nguồn `quet`;
6. **danh sách cố định** (giữ từ thời module WiFi rời, chỉ còn là phương án cuối):
   `192.168.0.1, 192.168.4.1, 192.168.1.1, 192.168.1.50, 192.168.0.50`.

**Cache IP** ở `%LOCALAPPDATA%\cydlcd-upload\last-target.txt`, **mỗi đích 1 dòng**:

```
cydlcd=192.168.0.146
```

* Khoá `cydlcd=` được đọc cho `-Target cydlcd` (hiện là target duy nhất). Khoá **cũ** `panel=` — và cả
  thư mục cache cũ (tên trước 2026-10-14, xem bảng **§9.23**) — **vẫn đọc được** ⇒ tool **tự di trú** sang
  thư mục mới; các khoá khác của đường đã xoá bị bỏ qua.
* Định dạng **cũ** (file chỉ có 1 dòng IP trần) vẫn đọc được (coi như của `cydlcd`).
* Đo thật (2026-10-14): cache đúng → dò **0,2 s** (tổng cả phiên gửi 63 KB: **6,5 s**);
  cache sai IP (`192.168.0.99`) → mất 1,5 s thử rồi tự dò lại **18,4 s** và ghi đè lại IP đúng;
  `-NoCache` → vào thẳng bước mDNS như trước.
Tối ưu: nếu `/status` **thất bại ngay ở tầng TCP** thì **không** thử tiếp `GET /` cho IP đó
(ứng viên “chết” chỉ tốn **1** lần timeout).

### 10.5 Lệnh mẫu

```powershell
cd D:\Marlin
# 1) MẶC ĐỊNH (khuyến nghị): tự tìm màn hình CYD trong mạng nhà rồi gửi
.\cydlcd\tools\cydlcd-upload.ps1 -File .\benchy.gcode
.\cydlcd\tools\cydlcd-upload.ps1 -File d:\0in\V300_Part3_proj.gcode -Name PART3.GCO

# 2) Ép IP màn hình CYD / đổi token / gửi thêm M22/M21 cho chắc (màn hình vốn đã tự gửi)
.\cydlcd\tools\cydlcd-upload.ps1 -File big.gcode -Host 192.168.0.174 -Token TBT-3D-up
.\cydlcd\tools\cydlcd-upload.ps1 -File part.gcode -InitSd -MarlinPort COM4

# 3) Hàm in3d (đã cập nhật trong cả 2 profile PowerShell)
in3d                                   # liệt kê .gcode trong D:\0in (hành vi cũ giữ nguyên)
in3d V300_Part3_proj.gcode             # -> web server của màn hình CYD qua WiFi
```

Chuột phải `.gcode`/`.bin` (**GuiLenIn3D**) và Send-to (`cydlcd/tools/in3d-sendto.cmd`) nay đều
gọi `cydlcd-upload.ps1 -File "%1" -Token "TBT-3D-up"` (đường duy nhất: web server của màn hình CYD).

Tham số: `-File` (**bắt buộc**), `-Target cydlcd` (**target duy nhất** — target cũ đã xoá 2026-10-14 sẽ
**báo lỗi ngay** khi bind tham số, xem §9.23), `-Host`, `-Port` (80), `-Name`, `-Token`
(`TBT-3D-up`), `-CydlcdMarker`, `-InitSd`, `-MarlinPort` (COM4 @250000, `DtrEnable` bắt buộc),
`-TimeoutSec` (300), `-NoDiscover`, `-NoCache`, `-ScanThreads` (40), `-ScanTimeoutMs` (1000),
`-ScanBudgetSec` (25), `-ProbeTimeoutMs` (1500), `-StatusIntervalMs` (1000), `-StatusTimeoutSec`
(900), `-NoStatus`.
**Mã thoát**: `0` gửi OK · `2` lỗi phía server · `3` không tìm thấy thiết bị · `4` lỗi file/tham số.

### 10.6 Mạng + `M22`/`M21`

* **Màn hình CYD** vào WiFi **STA** của mạng nhà (`TP-Link_31A0`, 2.4 GHz) ⇒ PC đang ở mạng nhà là gửi
  được ngay. Nếu màn hình CYD chưa nối được WiFi thì nó **chưa có IP** ⇒ không dò ra (xem trang STATUS /
  `cydlcd-log.ps1`).
* **Không cần đổi mạng, không cần AP riêng**: đường duy nhất đi qua mạng nhà. (Trước 2026-10-14, đường
  gửi thẳng lên module WiFi rời bắt PC nối AP `Voron2308` — đã xoá cùng module, xem §9.23.)
* **Vì sao cần `M22`+`M21`**: Marlin **không tự biết** thẻ vừa có file mới (nó còn giữ trạng thái FAT cũ) ⇒
  màn hình gửi `M22` (nhả media) + `M21` (mount lại) + xin lại danh sách file **sau khi ghi xong** — việc
  này nằm trong `cyd_sd_finish_upload()`, client **không phải** gửi. Vì sao Marlin đọc thẻ qua UART: **§12**.
* ⚠️ Mở cổng COM4 (USB CDC STM32) làm **board reset** ~2,5 s rồi mới gửi `M22`/`M21` (giống
  `send-gcode.ps1`) — đừng chạy `-InitSd` khi đang in.

### 10.7 Đã thử thật những gì (2026-10, máy này)

| Phép thử | Kết quả |
|---|---|
| Dò đầy đủ (cache → mDNS → gateway → quét 253 IP → danh sách cố định), không có màn hình CYD | **23 s** rồi **thoát mã 3**; riêng **quét 253 IP = 7,2 s** (40 luồng) — chỉ 1 IP mở port 80 (router) |
| **Gửi lên mock HTTP server** (Node, chép đúng parser ESP32 core 2.0.17) | **exit 0**; mock xác nhận: `Content-Length` khớp, có `name="name"` **trước** phần file, `argFilename="benchy.gcode"`, `X-CYDLCD-Token` có mặt, body kết thúc `--boundary--`, **0 vấn đề**; theo dõi `/status`: `receiving` → `writing` → `done` |
| Toàn vẹn dữ liệu (file 1.348.648 B = 1,29 MB) | **SHA256 trùng khớp** (`received/cydlcd-benchy.gcode`) |
| Parser với body **thiếu dấu `"`** | trả về `enchy.gcod` ⇒ chứng minh §10.3 |
| `-InitSd -MarlinPort COM99` (không có cổng) | cảnh báo rõ, **không** làm hỏng kết quả upload (exit 0) |
| `in3d` trong cả 2 profile | parse OK; `in3d` (không tham số) **vẫn liệt kê** `D:\0in`; `in3d <file>` → web server của màn hình CYD |
| File lớn 9.860.011 B (9,4 MB) | **exit 0**, SHA256 khớp (đo qua loopback nên tốc độ không có ý nghĩa; điểm cần kiểm là **không mất byte** ở cỡ 10 MB) |
| `-Target` khác `cydlcd` | **báo lỗi rõ ràng ngay** khi bind tham số (`ValidateSet('cydlcd')`) — target cũ đã xoá 2026-10-14, **không gửi gì cả** |
| `/status` báo `state=error` | thoát mã **2** (không báo thành công giả) |
| `/status` giữ **kết quả phiên trước** (done + tên file khác) | **bỏ qua** trạng thái cũ, chờ phiên của mình; quá 30 s vẫn mơ hồ ⇒ báo “chưa xác nhận được”, thoát mã **0** (file đã lên màn hình CYD) |
| ✅ **Đã nạp lên màn hình CYD thật** (2026-10-14) | upload qua WiFi 63.472 B → `/status` = `done` → **Marlin thấy file mới qua UART** (xem §12.2). Các phép thử còn lại ở trên chạy trên **mock cục bộ** (`%TEMP%\…\mock-target.js`, chép đúng parser của ESP32) |

Chạy thật: `cydlcd-upload.ps1 -File <file>` (hoặc `in3d <file>`) → in ra
`[cydlcd] thay <IP> (nguon: cache|mdns|gateway|quet|danh sach)` rồi tiến độ `receiving → writing → done`
và `=> File DA DUOC GUI THANH CONG.`

### 10.8 Đối chiếu với firmware màn hình CYD **đã có** trong repo (`src/net_upload_http.cpp`)

Đã đọc `src/net_upload_http.cpp` + `cydlcd_config.h` và **khớp hợp đồng §10.2** (xem thêm §9.21):

| Mục | Firmware | Client |
|---|---|---|
| `GET /status` | `{"cydlcd":"TBT","device":"TBT-CYDLCD","state":"idle\|receiving\|writing\|done\|error",…}` — **trả cả khi rảnh** (`handle_status`) | nhận dạng bằng `cydlcd`/`device`; đọc `state/rx_bytes/total_bytes/written_bytes/file/result` ✅ |
| `GET /` | HTML PROGMEM **có chuỗi `TBT-CYDLCD`** (`handle_root`) | có, dùng làm nhận dạng dự phòng ✅ |
| `POST /upload` | `server.on("/upload", HTTP_POST, handle_upload_done, handle_upload_stream)` | `POST /upload?token=...` + header `X-CYDLCD-Token` ✅ |
| Token | `server.arg("token")` **hoặc** `server.header("X-CYDLCD-Token")` (`token_ok()`); `CYDLCD_HTTP_TOKEN "TBT-3D-up"` (`cydlcd_config.h`) | gửi **cả hai**, cùng giá trị ✅ |
| Cổng / mDNS | `CYDLCD_HTTP_PORT 80`; `CYDLCD_HTTP_MDNS_NAME "TBTCYDLCD"` ⇒ `TBTCYDLCD.local` (`cydlcd_config.h`) | `-Port 80`; mDNS thử `TBTCYDLCD.local` **đầu tiên** ✅ |
| Tên file đích | ưu tiên field text `server.arg("name")`, không có thì `up.filename`; `unquote()` bỏ dấu `"` | gửi `name="name"` **trước** + `filename="<tên>"` ✅ |
| Mã lỗi | `401` sai token · `409` Marlin đang in (`CYDLCD_HTTP_ALLOW_WHILE_PRINTING 0`) · `500` lỗi thẻ/ghi · `400` không có dữ liệu | in gợi ý riêng cho 401/409/403, thoát mã **2** ✅ |
| Bật/tắt | `CYDLCD_WIFI_UPLOAD 1` với CYD (`cydlcd_config.h`) | không thấy màn hình CYD thì kiểm cờ này có đang = 0 không |

⚠️ **Ba việc phía firmware đã sửa** (2026-10-14) — giữ lại đây làm hồ sơ:

1. **`total_bytes` luôn = 0.** `net_upload_http.cpp` đọc `s_total = up.totalSize` ở `UPLOAD_FILE_START`,
   nhưng **ESP32 core 2.0.17 đặt `totalSize = 0`** ở trạng thái đó (`Parsing.cpp`:
   `_currentUpload->totalSize = 0;` — nó chỉ **cộng dồn dần trong lúc ghi**) ⇒ `/status` luôn trả
   `total_bytes: 0` (trang HTML của màn hình cũng vì thế mà thanh tiến độ chỉ chạy nhờ
   `xhr.upload.onprogress`). **Đã sửa**: lấy tổng thật ở nhánh `UPLOAD_FILE_END` / từ `Content-Length`.
   *Client tự chịu được*: khi `total_bytes <= 0` thì lấy **kích thước file của chính nó** làm mốc và in
   `rx=…/<size>*` (dấu `*` = số ước lượng từ phía client).
2. **Chú thích lệch code**: `cydlcd_config.h` ghi token nhận qua header `X-Token`, nhưng code kiểm
   `X-CYDLCD-Token`. Client gửi theo **code** (`X-CYDLCD-Token`); chú thích đã sửa cho khỏi nhầm.
3. **Tên file bị thêm đuôi sai** (chỉ đọc log máy thật mới thấy): `V300_Part3_proj.gcode` →
   `...gcode.gco` — hàm `sanitize_name()` chỉ so 4 ký tự cuối với `.gco`; nay **chỉ** thêm `.gco` khi tên
   **chưa có đuôi nào**.

> Việc thứ 4 của bản cũ (đổi AP sang module WiFi rời để POST, join thất bại thì mất WiFi vĩnh viễn)
> **không còn nghĩa**: cả macro lẫn đường đổi AP đã bị xoá 2026-10-14 (xem §9.23).


## 11. [ĐƯỜNG LUI] THẺ SD DÙNG CHUNG với Marlin qua MUX SPI (74HC4053) — mặc định TẮT (2026-10-14)

> ⚠️ Mục này **không còn dùng** (đang dùng **mục 12**: thẻ ở khe màn hình, Marlin lấy sector qua UART).
> Giữ lại làm **đường lui duy nhất còn code** + tài liệu đấu dây nếu sau này quay lại.

Quyết định của người dùng: *"dùng 4053 chia SPI để màn hình CYD ghi thẳng vào thẻ SD dùng chung với
Marlin"*. Cờ `CYDLCD_SD_SHARED` **mặc định TẮT (`0`)**; bật bằng `-DCYDLCD_SD_SHARED=1` thì `CYDLCD_SD_UART`
**tự tắt** (2 chế độ loại trừ nhau). Ở chế độ MUX, Marlin **vẫn tự đọc thẻ bằng SPI**; màn hình chỉ giành
bus lúc nhận/ghi file rồi trả lại kèm `M22`/`M21`.

### 11.1 Luồng mới (đã chạy thật)

```
PC/điện thoại ──WiFi──▶ màn hình CYD (WebServer, /cydlcd_tmp/<tên>.tmp) ──đổi tên──▶ /<tên>  (CÙNG thẻ SD)
                                                    │
                                                    └─ SD.end() + MUX ▶ Marlin + M22/M21 + xin lại danh sách file
```

Nhật ký thật (COM7, file 63.472 B):

```
[HTTP] nhan file "V300_Part3_proj.gcode" tu 192.168.0.174 (kich thuoc chua biet)
[UP] the TF OK @ 20000000 Hz: cardType=3, tong 15257584 KB, da dung 40 KB
[UP] don /cydlcd_tmp (truoc phien moi): xoa 0 file .tmp (0 KB)
[UP] ghi vao /cydlcd_tmp/V300_Part3_proj.gcode.tmp (ten dich: V300_Part3_proj.gcode)
[HTTP] nhan xong 63472 B (totalSize=63472) -> /cydlcd_tmp/V300_Part3_proj.gcode.tmp
[UP] UI KHOA
[UP] da doi ten /cydlcd_tmp/V300_Part3_proj.gcode.tmp -> /V300_Part3_proj.gcode (63472 B)
[UP] UI MO KHOA
[UP] tra bus SPI the cho MARLIN + M22/M21
[HTTP] DONE: 63472 B -> the SD (M22/M21 da gui)
```

`GET /status` ⇒ `state=done`, `rx_bytes=63472`, `written_bytes=63472`, `result=DONE 63472` (2 s).
Heap khi rảnh **194 KB** (trước đây 166 KB vì không còn giữ mount + poll thẻ).

### 11.2 Vì sao KHÔNG dùng 74HC4052 (2×4:1) mà dùng 74HC4053 (3×2:1)

| | 74HC4052 | 74HC4053 |
|---|---|---|
| Số cực | **2** (X, Y) — mỗi cực 4 kênh | **3** (X, Y, Z) — mỗi cực 2 kênh |
| Đủ cho SPI 2 master? | ❌ chỉ chuyển được **2** trong 3 tín hiệu | ✅ chuyển **SCK + MOSI + CS** |
| Dùng cho việc này | phải dùng 2 con (thừa 1 nửa) hoặc chấp nhận rủi ro | đúng 1 con |

- **MISO không cần chuyển**: chỉ có **một** driver MISO (chính thẻ SD), cả hai master đều là input →
  nối thẳng vào cả hai.
- **CS phải chuyển**: nếu để chung, chân CS của master không được chọn (ví dụ Marlin lúc boot) có thể
  kéo thấp → thẻ bị chọn trong khi master kia đang clock ⇒ tranh chấp.
- **Điện áp**: 4053 chạy 3,3 V (`VCC=3V3`, `VEE=GND`, `VSS=GND`), tín hiệu 3,3 V ⇒ mức logic hợp lệ.
- **Tần số**: giữ SPI ≤ 10 MHz cho đường dây dài (màn hình CYD đang để 20 MHz cho đường NGẮN trong board —
  sau khi đi dây ra ngoài nên hạ `CYDLCD_SD_HZ` xuống 10 MHz nếu thấy lỗi mount).
- **Trạng thái mặc định phải về MARLIN**: chân chọn (A/B/C nối chung) kéo xuống GND bằng **điện trở
  10 kΩ** ⇒ khi màn hình CYD reset / mất điện / chân GPIO còn treo thì thẻ vẫn thuộc Marlin.

### 11.3 Sơ đồ chân (2 cách — chọn 1)

**Cách A (khuyến nghị): thẻ vẫn nằm ở khe của Marlin (MKS Monster8)**

| Tín hiệu | Phía MARLIN (MKS Monster8, SD onboard = SPI3) | Phía màn hình CYD (board CYD) | Qua 4053 |
|---|---|---|---|
| SCK  | `PC10` (chân CLK của khe TF) | `GPIO18` | cực X |
| MOSI | `PC12` (CMD) | `GPIO23` | cực Y |
| CS   | `PC9` (SDSS) | `GPIO5` | cực Z |
| MISO | `PC11` (DAT0) | `GPIO19` | **nối thẳng cả hai** |
| Chọn | — | `GPIO27` → A/B/C (nối chung) + 10 kΩ xuống GND | INH → GND |
| GND/3V3 | GND chung | 3V3 cấp cho 4053 | — |

Ưu điểm: **Marlin không phải sửa gì** — công tắc nhận thẻ (`SD_DETECT_PIN PC4`) vẫn hoạt động, cờ
`CYDLCD_SF_MEDIA` trong frame PUSH vẫn đúng, `isMediaInserted()` vẫn thật.

#### ⚠️ Chân chọn MUX và chân ĐÈN NỀN (`IO21`) — đọc trước khi hàn

Trên CYD, **`IO21` (chân thứ 4 của `P3`) = đèn nền (`TFT_BL`)**; `IO27` (chân 3 của `CN1`) **trống**.
Hai chân này **loại trừ nhau**:

* Mức chọn MUX phải là **LOW khi màn hình CYD nhường bus cho Marlin** (mặc định / 10 kΩ kéo xuống GND).
  Nếu chân đó vẫn nối vào đèn nền thì **mỗi lần trả bus, màn hình tắt/nháy**, và lúc rảnh thì tối luôn.
* Vì vậy: **hoặc** dùng `IO27` cho MUX (giữ nguyên đèn nền — khuyến nghị), **hoặc** dùng `IO21` cho
  MUX thì **bắt buộc đấu đèn nền thẳng vào 3V3** (màn luôn sáng hết cỡ, không tắt được nữa).
* Firmware đã **chặn lúc biên dịch**: nếu `CYDLCD_SD_MUX_PIN == CYDLCD_BL_PIN` thì build **báo lỗi**
  kèm hướng dẫn. Muốn dùng `IO21` cho MUX:
  `-DCYDLCD_SD_MUX_PIN=21 -DCYDLCD_BL_PIN=-1` (thêm vào `build_flags` của env `cydlcd_esp32`)
  **và** đấu đèn nền vào 3V3.
* Từ 2026-10-14 đèn nền do **firmware** điều khiển (`CYDLCD_BL_PIN`, mặc định 21): `User_Setup.h` đã
  đặt `TFT_BL -1` để TFT_eSPI không giữ chân đó ở mức HIGH vĩnh viễn. Board ST7789 nào dùng `IO27`
  làm đèn nền thì build với `-DCYDLCD_BL_PIN=27`.

**Cách B (theo đúng câu chữ của người dùng): thẻ nằm ở khe của CYD**

Giống bảng trên nhưng đảo đầu dây: SCK/MOSI/CS của Marlin (`PC10/PC12/PC9`) đi vào các cực, SCK/MOSI/CS
của CYD đi vào cực còn lại, và chân thẻ (khe CYD) là đầu chung. **Bắt buộc sửa Marlin**:
`SD_DETECT_PIN` phải thành `-1` (khe Marlin không còn thẻ ⇒ công tắc nhận thẻ báo "không có thẻ" ⇒
Marlin không mount), tức thêm vào `Configuration_adv.h`:

```cpp
#define NO_SD_DETECT        // hoặc #define SD_DETECT_PIN -1  (khe Marlin khong dung nua)
```

### 11.4 Firmware màn hình CYD đã làm gì (cờ `CYDLCD_SD_SHARED`, **mặc định TẮT** với CYD)

| Việc | Ở đâu |
|---|---|
| Chân chọn MUX (`CYDLCD_SD_MUX_PIN 27`, mức `HIGH`=màn hình CYD, `LOW`=Marlin, `CYDLCD_SD_MUX_SETTLE_MS 3`) | `cydlcd_config.h` (khối `CYDLCD_SD_SHARED`) |
| Chặn lúc biên dịch nếu chân chọn MUX trùng chân đèn nền (`CYDLCD_BL_PIN`) | `cydlcd_config.h` (`#error` ngay dưới khối trên) |
| Đèn nền do firmware điều khiển (`CYDLCD_BL_PIN`, `CYDLCD_BL_ON_LEVEL`, `-1` = đấu thẳng 3V3); `TFT_BL -1` | `cydlcd_config.h`, `tools/tft_setup_cyd/User_Setup.h`, `ui_port_cyd_st7789.cpp` (`ui_port_display_init`) |
| Giành bus **chỉ khi** mount/ghi thẻ; trả bus + `M22`+`M21` + xin lại danh sách file sau khi ghi xong | `cyd_sd.{h,cpp}` (`sd_mux_cydlcd`, `sd_release_to_marlin`) |
| KHÔNG mount thẻ lúc boot, KHÔNG kiểm tra thẻ 1 Hz khi rảnh (bus thuộc Marlin) | `cyd_sd_begin()`, `cyd_sd_task()` |
| Đổi tên `/cydlcd_tmp/<tên>.tmp` → `/<tên>` (ghi đè: xoá bản cũ trước vì FATFS rename không ghi đè) | `cyd_sd_finish_upload()` |
| Trạng thái thẻ lấy từ **Marlin** (`CYDLCD_SF_MEDIA`) → icon STATUS + `/status` | `cyd_sd_ready()`, `sd_state_now()` trong `ui_lvgl.cpp` |
| **Chặn** các lệnh làm Marlin đọc thẻ khi màn hình CYD đang giữ bus: `M23/M24` (in), `M30` (xoá), `M22/M21`/REFRESH, mở trang FILES | `ui_lvgl.cpp` (`sd_host_busy()` + `net_upload_active()`) |
| Từ chối nhận file nếu **chưa liên lạc được Marlin** (`!cydlcd.linked`) hoặc Marlin đang in/tạm dừng | `cyd_sd_temp_open()`, `cyd_sd_printer_busy()` |
| Đợi Marlin liệt kê xong danh sách file (≤400 ms) trước khi giành bus | `cyd_sd_temp_open()` |

Đường **MUX** nằm sau `#if CYDLCD_SD_SHARED`; mặc định (`0`) firmware chạy chế độ **thẻ ở khe màn hình +
Marlin hỏi sector qua UART** (`CYDLCD_SD_UART 1` — **§12**). Muốn quay lại MUX: bật `-DCYDLCD_SD_SHARED=1`
ở env `cydlcd_esp32` **và** bỏ `CYDLCD_SD_OVER_UART` ở Marlin.

### 11.5 Việc phải làm khi lắp phần cứng

1. **Rút/ngắt module WiFi rời cũ** (nếu còn gắn) khỏi bus SPI thẻ SD — module đó ghi thẳng vào thẻ qua
   SPI dùng chung, còn nối thì nó tranh bus với cả Marlin và màn hình CYD.
2. Hàn 4053 + **điện trở 10 kΩ kéo chân chọn xuống GND** (mặc định = Marlin).
3. **Cách B (đã chọn — thẻ ở khe CYD): `NO_SD_DETECT` ĐÃ BẬT** trong
   `Marlin/Configuration_adv.h` (mục "Enable if SD detect is rendered useless") và **đã nạp Marlin
   ngày 2026-10-14**. Kiểm chứng trên máy: `M20` liệt kê được file trên thẻ
   (`V300_B~1.GCO 1693754`, `V300_P~1.GCO 63472`) ⇒ mount thẻ vẫn tốt dù công tắc nhận thẻ bị bỏ.
   *Lưu ý:* bật `NO_SD_DETECT` khi thẻ **vẫn còn ở khe Marlin** thì vô hại (Marlin mount thẳng).
4. Cân nhắc hạ tốc độ SPI thẻ của Marlin cho đường dây dài: `Configuration_adv.h` →
   `#define SD_SPI_SPEED SPI_HALF_SPEED` (dòng ~1568). Màn hình CYD: hạ `CYDLCD_SD_HZ` (hiện 20 MHz) xuống
   10 MHz nếu thấy lỗi mount sau khi đi dây.
5. Nạp lại màn hình CYD, mở `http://TBTCYDLCD.local/` gửi 1 file → `/status` phải `done`, rồi mở trang FILES
   trên màn hình CYD (hoặc bấm REFRESH) để thấy file mới.

### 11.6 Nạp Marlin từ máy này (đường đi chắc chắn) — 2026-10-14

```powershell
cd D:\Marlin
pio run -e mks_monster8                                   # ~30 s
# 1) vao DFU: mo COM4 (USB CDC cua STM32), DTR = TRUE, gui "M997", cho ~2 s
$p = New-Object System.IO.Ports.SerialPort COM4,250000,None,8,one
$p.DtrEnable = $true; $p.RtsEnable = $false; $p.Open(); $p.WriteLine("M997"); $p.Close()
Start-Sleep 3
# 2) kiem tra phai thay [0483:df11]
& "$env:USERPROFILE\.platformio\packages\tool-dfuutil\bin\dfu-util.exe" --list
# 3) nap (~35 s cho 197 KB)
& "$env:USERPROFILE\.platformio\packages\tool-dfuutil\bin\dfu-util.exe" -a 0 -s "0x0800C000:leave" `
    -t 2048 -D ".pio\build\mks_monster8\mks_monster8.bin"
```

⚠️ **Đừng đọc log màn hình CYD bằng COM7 mà không tắt RTS**: chân DTR/RTS của CH340 nối vào EN/IO0 của
ESP32, để mặc định có thể **giữ màn hình CYD ở trạng thái RESET** (im lặng hoàn toàn, không WiFi) — trông y
như màn hình CYD chết. Luôn đặt `$p.DtrEnable=$false; $p.RtsEnable=$false` ngay sau `Open()` (xem README
gốc, "Cẩm bẫy 47"). Muốn reset màn hình CYD: kéo `RtsEnable` true→false một nhịp.


Da nap va xac nhan tren may that (`link PUSH=1 TBT=1 err=0`).
Protocol: `CYDLCD_PROTO_VERSION 2` - **phai nap ca Marlin va màn hình CYD**.
- `CYDLCD_F_ENDSTOPS (0x0F)` + `CYDLCD_REQ_ENDSTOPS`: trang ENDSTOP (xanh=mo do=CHAM).
  2026-10-15: **bỏ nút REFRESH** — trang tự gửi `CYDLCD_REQ_ENDSTOPS` mỗi **30 ms** (⇒ 30 lần/giây, chỉ khi
  màn ENDSTOP đang mở), và **chỉ hiện chân nào bản build THẬT SỰ có** (thêm `cydlcd.es_present` = bitmask id
  trong frame, xem *"ENDSTOP: đọc 30 lần/giây"* bên dưới).
- `CYDLCD_REQ_DFU`: nut `REBOOT DFU` trong CUSTOM, xac nhan 2 buoc; Marlin tu choi
  neu dang ban (`printer_busy()`) va tra WARN.
- Setting id 34..41: `M204 P/R/T` (accel print/retract/travel),
  `M906 X/Y/Z/E` (mA - gui RAW, KHONG nhan he so `cydlcd_val()`), `M412` (runout).
- HELLO mang them `SHORT_BUILD_VERSION` -> `cydlcd.marlin_ver`.
- Trang TEMP: dong dau `Profile: PLA/PETG/ABS` hoac `Profile: ---` (khong con dialog).
- Trang MESH: `G29 P1 | G29 P3T0 | G29 P4RT` + `G29 A | G29 D | G29 S0`.
- **(2026-10-15) Trang BED MESH có CHẾ ĐỘ SỬA (EDIT)** — yêu cầu người dùng: *“thêm nút edit vào bed mesh,
  đây là nút toggle, khi click thì fill màu, click lần nữa về ban đầu; ở chế độ edit người dùng có thể [chạm]
  1 ô để thay đổi giá trị giống trang setting; page navigation nằm ở 4 hướng trái, trên, phải, dưới để dịch
  chuyển vùng view”*:
  * Nút **EDIT** (góc phải-trên của lưới): **toggle** — bật thì tô nền `COL_ACCENT` (fill), tắt thì về màu nút
    thường; log `[UI] MESH EDIT: BAT/TAT`.
  * Bật EDIT: **chạm 1 ô** → mở **hộp sửa** giống lớp phủ của trang THÔNG SO: chọn bước **0.01 / 0.05 / 0.1 mm**
    (mục đang chọn tô nền), nút **−/+** đổi giá trị (±2 mm), **OK** gửi
    `M421 I<cột> J<hàng> Z<mm>` (UBL: `Marlin/src/gcode/bedlevel/ubl/M421.cpp`, có `ExtUI::onMeshUpdate()`
    nên **Marlin tự đẩy lại lưới**), **HUY** thì thôi. Có lớp mờ (mask) chặn chạm khi hộp đang mở.
  * **4 nút điều hướng `< ^ v >`** (chỉ hiện khi bật EDIT, đặt ở 4 phía của lưới) để **dịch vùng nhìn**
    (`lv_obj_scroll_by` 40 px ngang / 20 px dọc mỗi lần bấm).
  * **Đo trên máy thật**: `UI BED MESH` + `UI BOX` xác nhận nút EDIT 56x20 ở (254,2) và 4 nút điều hướng
    `[AN]` (ẩn) ở 4 phía; gửi `M421 I5 J0 Z0.500` ⇒ dòng thông tin của panel **đổi ngay** `0.01..0.50`
    (chứng minh vòng Marlin→panel chạy), trả lại `Z0.420` ⇒ `M420 V` về đúng `+0.42` như ban đầu.
  * *Lưu ý*: với lưới **7×7 hiện tại** thì toàn bộ lưới đã vừa khung (308×119 trong bảng 312×124) nên **4 nút
    điều hướng chưa có gì để dịch** — chúng dùng khi lưới lớn hơn (10×10) hoặc khi ô được phóng to; muốn
    phóng to ô (`MESH_CELL_W`) để phải cuộn thì chỉ cần đổi 1 dòng.
- **(2026-10-15) Trang TUNING (MPC/PID)** (`ui_page_mpc.cpp`, `PG_MPC`) — 📜 *lịch sử, trang này đã bị xoá
  cuối ngày 2026-10-15, xem gạch đầu dòng ⛔ ngay dưới*: mở từ nút **“TUNING (MPC/PID)”**
  ở man NHIỆT ĐỘ hoặc mục **“Tuning (MPC/PID)”** trong MENU. Hiển thị **6 tham số MPC (M306)** đúng như menu
  MPC của Marlin 12864: `P heater W`, `C block J/K`, `R sensor`, `A amb fan0 W/K`, `F amb fanN W/K`,
  `H filament` (4 số lẻ cho R/A/F/H), kèm dòng nhắc `nozzle <n> C` (vàng + “CHO NGUOI truoc khi tune!” khi
  nozzle > 40 °C). Nút: **`M306 T (tune)`**, **`M303 E-1 60`**, **`SAVE EEPROM`**, **`THONG SO`** (nhảy sang
  bảng SETTINGS để sửa tay 6 dòng `MPC …`, đã thêm vào `SET_ROWS` → gửi `M306 P/C/R/A/F/H<giá trị>`).
  ⇒ 3 nút tuning cũ ở man NHIỆT ĐỘ đã **MOVE** sang trang này (man NHIỆT ĐỘ còn 1 nút mở trang).
  * Giao thức: `CYDLCD_SET_MPC_P/C/R/A/F/H` (id 43..48); Marlin đẩy trong `push_settings_body()` dưới
    `#if ENABLED(MPCTEMP)` (P/C scale 1000; R/A/F/H scale 10000).
  * `fmt_scaled()` nay cho tới **4 số lẻ** (trước chặn ở 3 → R hiện `0.128` thay vì `0.1284`).
  * **Đo trên máy thật**: `id 43 MPC P = 40.00 · id 44 C = 7.13 · id 45 R = 0.1284 · id 46 A = 0.0680 ·
    id 47 F = 0.0970 · id 48 H = 0.0056` (khớp `M306` của Marlin) và `UI BOX` xác nhận trang TUNING có đủ
    6 dòng + 4 nút, **không phải cuộn** (`thanh day an`).
  * ⛔ **ĐÃ BỎ HẲN (2026-10-15, cuối ngày)** — yêu cầu người dùng: *“‘turning MPC’ đã có ở trong phần
    Configuration → advance setting → temperature rồi, bỏ những chỗ khác đi”*. Trang `PG_MPC` +
    mục `Tuning (MPC/PID)` ở MENU + `A_GOTO_MPC` + `ui_page_mpc.cpp` **đã xoá**; MENU còn **11 dòng**
    (`MENU_ROWS 12 → 11`). Nay **CHỈ CÒN MỘT CHỖ** để tune MPC/PID: **Configuration → Advanced Settings →
    Temperature** = 6 dòng `SM_V` M306 (sửa được) + `MPC Autotune (M306 T)` + `Bed PID Autotune (M303)` +
    `Hotend PID` (xem mục *“Menu CONFIGURATION kiểu 12864”*). Bảng phẳng `All settings (table)` vẫn có 6
    dòng `MPC …` (là bảng đầy đủ, không phải chỗ tune).
- **Icon đầu in / bàn in trên man STATUS (2026-10-15)** — theo yêu cầu: *“nếu Marlin cấp nguồn cho hot end thì
  icon fill đặc, màu thay đổi dần từ xanh đến đỏ; ngừng cấp nguồn thì chỉ vẽ border; bàn in chỉ hiện biểu
  tượng khói khi cấp nguồn”*:
  * “Cấp nguồn” = **có target** (`tgt > 0`) — đúng như Marlin TFT (`targetTemperature > 0 ? imgBedHeated :
    imgBed`) và đúng cả khi PID đang giữ nhiệt.
  * **Hotend**: có target → icon **ĐẶC** (`ui_icon_hotend`); không target → chỉ **VIỀN**
    (`ui_icon_hotend_line`). Bản viền được **SINH THÊM** trong `tools/make_lvgl_icons.py` (hàm `outline()`:
    giữ pixel có alpha>0 mà có láng giềng alpha=0) vì bitmap gốc của Marlin là hình đặc — LVGL ALPHA_4BIT
    không có khái niệm nét/viền.
  * **Bàn in**: có target → `ui_icon_bed_heated` (đĩa + **khói**), không → `ui_icon_bed` (chỉ đĩa).
  * **Màu**: `temp_grad()` nội suy `lv_color_mix(màu nóng, MR_COLD, cur/tgt)` → xanh khi nguội → đỏ khi tới
    target (bản cũ chỉ đổi màu khi vượt 50 °C nên không thấy “dần dần”). Áp cho cả icon và 2 dòng số.
  **(2026-10-15) Hiện TRẠNG THÁI UBL ON/OFF** — yêu cầu người dùng: *“phần setting bed mesh chưa hiển thị
  được tình trạng mesh on hay off, bấm xong mà không biết đã được hay chưa”*. Cách làm:
  * `CYDLCD_F_STATUS` thêm **byte thứ 4 `mesh_on`** = `planner.leveling_active` của Marlin (ExtUI
    `getLevelingActive()`; đúng cho `G29 A`/`G29 D`, `M420 S1/S0` và sau khi đo mesh). Marlin cũ chỉ gửi
    3 byte nên panel cũ/mới đều tương thích (panel cũ bỏ qua byte thừa, panel mới chỉ cập nhật khi `n >= 4`).
  * Dòng thông tin trên trang MESH thành `7x7 rows 7 -0.12..0.15 valid  UBL:ON` và **đổi màu**: xanh =
    đang bật (có mesh), **đỏ = đang bật nhưng CHƯA có mesh**, vàng = đang tắt. Hai nút `UBL ON`/`UBL OFF`
    cũng **tô màu theo trạng thái** (nút đang có tác dụng = xanh/vàng, nút kia = màu nút thường).
  * Dòng **MENU** (mục “Bed mesh”) cũng thêm trạng thái: `7x7 ON` / `7x7 OFF`.
  * Trạng thái cập nhật ~1 s (Marlin đẩy STATUS định kỳ) nên **bấm xong là thấy ngay**.
  * Chẩn đoán: `UI BUSY` in thêm `[UI] mesh: UBL ON/OFF | luoi 7x7 rows 7 | da co mesh | co MESH=co`.
  * Sửa kèm: `mesh_rows` **cộng dồn** mỗi lần Marlin đẩy lại cả lưới (thấy “7x7 rows 14”) — nay hàng 0 =
    bắt đầu lưới mới nên đếm lại từ đầu.
  * **Đo trên máy thật**: sau khi nạp Marlin mới → `UBL ON (dang bu) | luoi 7x7 rows 7`; gửi `G29 D` →
    `UBL OFF`; gửi `G29 A` → `UBL ON (dang bu)` (đã trả về đúng trạng thái ban đầu).
- JOG: 5 muc buoc 0.01/0.05/0.1/1/10 mm, X/Y F3000 (50 mm/s), Z F600.
- SETTINGS: hang 2 `M501 LOAD | M502 RESET` (co xac nhan), bang 124 px.
  **(2026-10-15) Sửa lỗi nút bước `0.01` vô hiệu** (yêu cầu: *“nút chọn 0.01 trong setting chưa có hiệu lực,
  khi tăng giảm vẫn là 0.1”*): `ov_step()` trước đây chặn `≥ ov_quantum()` (chữ số thập phân cuối của dòng)
  nên dòng hiện 1 số lẻ (vd “Fade height 10.0”) bị nâng bước 0.01 → 0.1. Nay trả về **đúng bước đã chọn**,
  và khi đang sửa dòng nào thì dòng đó **hiện thêm số thập phân theo bước** (`OV_STEP_DEC`, `se_fmt_id()`)
  để thấy giá trị thật đổi (10.00 → 10.01) — cả khi gửi G-code cũng dùng độ chính xác đó.
- CUSTOM: `M600 B0`, toggle `M412`, `REBOOT DFU`.
- Thanh tieu de: 2 nut nho `II` (pause) / `[]` (stop) chi hien khi
  `cydlcd.flags & CYDLCD_SF_SD_PRINTING`; nut STOP luon hoi xac nhan truoc `M524`.
- Font: chi 3 nhan co dau (`Gia tri`, `Buoc`, `Huy`) duoc gan font mono GFX (chi co o che do 4);
  KHONG gan font do cho ca cay (xem cam bay 43 o README goc). O che do 0, 3 nhan nay ra o vuong.

## 12. **ĐANG DÙNG** — THẺ SD Ở KHE MÀN HÌNH, MARLIN LẤY SECTOR QUA UART (2026-10-14)

Quyết định của người dùng: *"hoặc có thể thêm lệnh khi marlin đọc từng line sdcard thì gọi sang lcd
để lấy data thì sao? … hay implement kiểu sdcard mới mà lấy data usart"* → **P1 đã xong và chạy thật**.

**Ý tưởng**: không MUX, không chia bus, không ai tranh ai. **Màn hình CYD là người DUY NHẤT giữ thẻ**; Marlin
không tự đọc SPI nữa mà **hỏi màn hình CYD từng sector 512 B** qua chính UART của kênh PUSH. Toàn bộ ngăn xếp
SD của Marlin (SdVolume / SdBaseFile / FAT16-32 / liệt kê thư mục / `M23`-`M24` / tiến độ / `M32`) vẫn
chạy nguyên vì tất cả chỉ đi qua **2 hàm** của driver: `readBlock()` / `writeBlock()`.

```
   Màn hình CYD (ESP32) ── WiFi ── PC/điện thoại          Marlin (STM32)
   ┌──────────────┐                                ┌───────────────────────────────┐
   │ giữ thẻ TF   │  ◀── CYDLCD_F_SDREQ{sector} ───  │ DiskIODriver_CydlcdUART        │
   │ SD.readRAW() │  ──▶ CYDLCD_F_SDDATA ×4 (128 B)  │  (SdVolume gọi readBlock)     │
   └──────────────┘                                └───────────────────────────────┘
```

### 12.1 Vì sao đi hướng này (và vì sao KHÔNG hook "từng line")

| | Hook từng dòng G-code | **Sector qua UART (đang dùng)** | MUX 4053 (mục 11) |
|---|---|---|---|
| Phải viết lại gì ở Marlin | `M20/M23/M24/M25/M524/M27/M30/M32/M98`, seek, PLR… (**rất nhiều**) | **1 class driver ~150 dòng** (`src/sd/cydlcd_sd_uart.*`) | không |
| Phần cứng | không | **không** | 4053 + 3 dây + 10 kΩ |
| Tranh chấp bus | không | **không** | không (phần cứng cách ly) |
| Màn hình CYD reset/nạp firmware | — | Marlin mất thẻ **tạm thời**, tự có lại khi màn hình CYD lên | Marlin không hề hấn |
| Băng thông | thấp nhất | ~21–25 ms/sector ⇒ trần ~20 KB/s | — |

### 12.2 Số liệu (đo trên máy thật)

| Đại lượng | Giá trị |
|---|---|
| Link | 250 000 baud = 25 000 B/s; 1 sector = 4 frame × 134 B ≈ 22 ms ⇒ trần **~23 KB/s** |
| In thực tế cần | 1–4 KB/s G-code ⇒ dùng **~5–20 %** băng thông |
| Đệm chịu trễ của Marlin | queue + planner ⇒ chịu được màn hình CYD trả lời trễ vài chục ms |
| Đã kiểm | `M20` liệt kê đúng thẻ **của màn hình CYD**; upload WiFi 63.472 B → `/status` done → **Marlin thấy file mới qua UART**; màn hình CYD **nạp lại firmware xong Marlin tự đọc lại được** |
| RAM | Marlin 13,5 % (17 720 B, +~3 KB), màn hình CYD 28,6 % |

### 12.3 Cấu hình (đã bật sẵn)

| Nơi | Nội dung |
|---|---|
| `Marlin/Configuration_adv.h` | `#define CYDLCD_SD_OVER_UART` + `#define SDCARD_READONLY` (P1 chỉ đọc) + `NO_SD_DETECT` |
| `Marlin/src/inc/Conditionals_post.h` | `NEED_SD2CARD_CYDLCD_UART` (thay cho `NEED_SD2CARD_SPI`) |
| `Marlin/src/sd/cardreader.h`, `SdVolume.h` | chọn `DiskIODriver_CydlcdUART` |
| `Marlin/ini/features.ini` | env `CYDLCD_SD_OVER_UART` (thêm file driver) + `SERIAL_RX_BUFFER_SIZE=1024` (frame 141 B) |
| `cydlcd/src/cydlcd_config.h` | `CYDLCD_SD_UART 1` (CYD) — tự tắt `CYDLCD_SD_SHARED` |
| `cydlcd/src/cydlcd_link.cpp` | trả lời `CYDLCD_F_SDREQ` → `cyd_sd_raw_read()` → 4 frame `CYDLCD_F_SDDATA` |
| `cydlcd/src/cyd_sd.cpp` | `cyd_sd_raw_read/raw_write/delete`; mount thẻ lúc boot và **giữ luôn** |

### 12.4 Giao thức (proto **4** — PHẢI nạp cả 2 board)

| Frame | Chiều | Payload |
|---|---|---|
| `CYDLCD_F_SDREQ` (0x10) | Marlin → màn hình CYD | `u8 op` (0=đọc, 1=ghi), `u32 sector` |
| `CYDLCD_F_SDDATA` (0x45) | **2 chiều** | `u8 status`, `u32 sector`, `u8 idx` (0..3), `u8 n`, `n` byte — màn hình CYD→Marlin khi ĐỌC, Marlin→màn hình CYD khi GHI |
| `CYDLCD_F_SDACK` (0x46) | màn hình CYD → Marlin | `u8 status`, `u32 sector` (trả lời lệnh GHI sau khi đã ghi xuống thẻ) |
| `CYDLCD_F_FILES` (0x09) | Marlin → màn hình CYD | **[proto 4]** `u8 flags`, `u32 size`, `str name` (tên dài), `str sfn` (**tên 8.3**) |

* 1 sector 512 B = **4 frame × 128 B** vì trường `len` của khung chỉ có **1 byte** (tối đa 255 B) —
  nhờ vậy **không phải đổi định dạng khung**, `CYDLCD_MAX_PAYLOAD` vẫn 192, không tăng RAM.
* `status != OK` ⇒ màn hình CYD chỉ gửi **1 frame** (`n = 0`) để Marlin báo lỗi ngay.
* Marlin chờ tối đa `CYDLCD_SD_TIMEOUT_MS` (1 s) + **nuôi watchdog** trong lúc chờ; hết hạn ⇒
  `readBlock()` trả `false` ⇒ Marlin báo "SD read error" và **dừng sạch**, không treo máy.
* Frame mang **lệnh** từ màn hình CYD (G-code/REQUEST/ANSWER/TBT_CMD) rơi vào lúc đang chờ sector sẽ được
  **hoãn lại** rồi xử lý ở `onIdle()` (không enqueue G-code trong lúc `disk_read`).

#### 12.4.1 Vì sao phải có **TÊN 8.3** (proto 4) — lỗi "bấm PRINT không in" (2026-10-15)

**Triệu chứng người dùng báo**: *"nút print không thấy in"*.

**Nguyên nhân (đo trên máy thật, không đoán)**: SdFat của Marlin chỉ giữ tên dài trong **27 byte** —
`SdFatConfig.h:112`: `LONG_FILENAME_LENGTH = FILENAME_LENGTH(13) * LONG_FILENAME_CHARSIZE(1) *
MAX_VFAT_ENTRIES(2) + 1 = 27`. Tên dài hơn **26 ký tự bị CẮT**, mất luôn phần mở rộng:

| Tên THẬT trên thẻ | Dài | Màn hình CYD nhận được | Hệ quả |
|---|---|---|---|
| `V300_case-esp-2432s208_no-text.gcode` | 36 | `V300_case-esp-2432s208_no-` | `M23` không khớp tên trên thẻ ⇒ **không mở được file** |
| `V300_[a]_xy_joint_cable_bridge_3hole.gc` | 39 | `V300_[a]_xy_joint_cable_br` | như trên |
| `Body10.gcode` | 12 | `Body10.gcode` | in bình thường |

Tên trong 1 frame `CYDLCD_F_FILES` do đó **không đủ** để ra lệnh (cả `M23` lẫn `changeDir` đều so tên với
tên thật trên thẻ). **Cách sửa**: Marlin gửi kèm **tên 8.3** (`cydlcd_files.shortFilename()`), màn hình
**hiện tên dài** nhưng **gửi lệnh bằng tên 8.3** (≤ 12 ký tự, luôn khớp). Đọc theo kiểu *"nếu còn byte thì
đọc"* nên **panel/Marlin bản cũ vẫn chạy bình thường** (chỉ là không in được file tên dài).

**Đã kiểm end-to-end (COM7)**: `UI TAP 0` + `UI TAP I` →
`FILES: IN <<V300_case-esp-2432s208_no->> -> gui "M23 V300_C~1.GCO" + "M24"` → Marlin `st=1`,
`fl=0x26` (bit `SD_PRINTING`), `file="V300_case-esp-2432s208_no-"` ⇒ **đã mở được file và bắt đầu in**;
sau đó `M524` + `M108` → `st=1 → 3 → 0`, `fl` về `0xA3`, nút về `PRINT=mo XOA=mo`.

**Lệnh chẩn đoán đi kèm** (`UI TAP` trên COM7): `I` = bấm PRINT, `C` = đếm mục con của thư mục đang chọn
(không xoá gì), `K` = bấm OK hộp thoại (2 lần = chạy lệnh thật), `L [đường dẫn]` = liệt kê thư mục bằng
FATFS của **chính màn hình** (tên đầy đủ, không bị cắt 27 ký tự) để đối chiếu với tên Marlin gửi.

### 12.5 Đã làm tới đâu (P1 + P2 + P3 — **đủ dùng**)

| Giai đoạn | Nội dung | Trạng thái |
|---|---|---|
| **P1** | Đọc: `readBlock()` → `CYDLCD_F_SDREQ{READ}` → 4 frame `CYDLCD_F_SDDATA` | ✅ đã nạp, chạy thật |
| **P2** | Đo băng thông khi đọc liên tục | ✅ đo được **~11–13 KB/s** (xem 12.5b) |
| **P3** | Ghi: `writeBlock()` → `SDREQ{WRITE}` + 4 frame dữ liệu → màn hình CYD `SD.writeRAW()` → `CYDLCD_F_SDACK` | ✅ đã nạp, chạy thật |

**P3 cho phép gì**: `M28`/`M29` (ghi log ra thẻ), `M30` (xoá file), và `POWER_LOSS_RECOVERY` nếu bạn bật
lại trong `Configuration_adv.h`. `SDCARD_READONLY` **đã bỏ**; nếu chỉ muốn đọc cho an toàn thì bật
`#define CYDLCD_SD_READONLY` trong `Configuration_adv.h` (nó tự thêm `SDCARD_READONLY`).

**Đã kiểm trên máy thật (P3)**:
`M115` → `Cap:SD_WRITE:1`; `M28 TESTLOG.GCO` → “Now fresh file / Writing to file”; ghi 1 dòng log →
`M20` thấy `TESTLOG.GCO 57`; `M23 TESTLOG.GCO` → “File opened: Size: 57” (đọc lại đúng dữ liệu vừa ghi);
`M30 TESTLOG.GCO` → “File deleted” và `M20` không còn file ⇒ **ghi/đọc/xoá đều đi qua màn hình CYD**.

**Đồng bộ FATFS ↔ ghi thô** (2 chiều, đã cài trong `cyd_sd.cpp`):
* `cyd_sd_raw_write()`: nếu màn hình CYD đang mở file upload → **TỪ CHỐI** (`s_file`); nếu màn hình CYD vừa
  ghi qua FATFS (`s_fatfs_written`) → `fatfs_sync()` (unmount + mount lại) để đẩy hết cache xuống thẻ trước
  khi Marlin ghi đè. Sau khi Marlin ghi thô → đặt `s_fatfs_stale`.
* Trước mọi thao tác FATFS của màn hình CYD (`temp_open`, `delete`) mà `s_fatfs_stale` → `fatfs_sync()` để đọc
  lại FAT/dir mới. Nếu không làm bước này, cache cũ của FATFS sẽ ghi đè lên thay đổi của Marlin.
* **Đừng vừa upload WiFi vừa để Marlin ghi thẻ** (2 người ghi 1 FAT) — màn hình CYD có flush nhưng vẫn nên tránh;
  upload đã tự bị chặn khi Marlin đang in.

#### 12.5.1 “Trong lúc upload mà Marlin gửi M21/M22 thì có bị block không?” (2026-10-15)

**Không bị block (không treo/deadlock) — nhưng có 2 chiều khác nhau, và bản cũ có 1 lỗi thật:**

**Chiều A — Marlin → màn hình (Marlin tự mount/nhả thẻ, người dùng bấm trên LCD/console):**
* Marlin **không có driver thẻ riêng**: `M21`/`M22` của Marlin chỉ đổi thành các yêu cầu **sector thô** gửi
  qua UART xuống màn hình (`cydclcd_serial_sd_read/write`).
* Trong lúc màn hình đang có phiên ghi thẻ (`s_file`/`s_busy`), `cyd_sd_raw_read()`/`cyd_sd_raw_write()`
  **từ chối** và trả lời `ERR` → mount của Marlin **thất bại** (có chủ ý: đang giữ FAT thì không cho ai khác
  đọc/ghi để không phá cache FAT và không đè lên file đang ghi).
* Marlin chờ tối đa `CYDLCD_SD_TIMEOUT_MS` (1 s) mỗi sector và **nuôi watchdog trong lúc chờ**
  (`hal.watchdog_refresh()`) ⇒ **không reset, không treo**; Marlin chỉ coi thao tác đó lỗi và thử lại sau.
* Trong ~20–30 s **nhận** dữ liệu WiFi, vòng lặp chính của màn hình còn bị `server.handleClient()` chặn nên
  màn hình **không trả lời được frame nào** ⇒ Marlin chỉ thấy *timeout* (1 s/sector). Vì vậy (sửa 2026-10-15):
  `cydlcd_serial_sd_read()` **không còn coi timeout là “mất thẻ”** — trước đây `s_sd_present = (err == ERR)`
  nên mỗi lần timeout lại báo “không có thẻ”. Nay: chỉ `NOCARD` (màn hình báo rõ) mới xoá cờ có-thẻ, `ERR`
  giữ nguyên “có thẻ nhưng lỗi”, timeout **giữ nguyên trạng thái cũ**; `cydlcd_serial_sd_init()` cũng chỉ đặt
  `true` khi đọc được.
* Trong lúc **chốt phiên** (merge/đổi tên) màn hình *có* nuôi UART ⇒ Marlin nhận được câu trả lời `ERR` rõ
  ràng (không phải timeout).

**Chiều B — màn hình → Marlin (cuối mỗi phiên upload, `notify_media_changed()` gửi `M22`+`M21`):**
* 🔴 **LỖI THẬT đã sửa (2026-10-15)**: `cyd_sd_finish_upload()` gọi `notify_media_changed()` (bảo Marlin
  “mount lại đi”, rồi nuôi UART ~800 ms để Marlin mount + xin lại danh sách file) **trong khi `s_busy` vẫn
  còn `true`** ⇒ màn hình **vừa bảo mount lại vừa từ chối mọi sector** Marlin đọc để mount ⇒ Marlin mount
  lỗi / danh sách file rỗng (log cũ có đúng dòng `refused rd/wr=1/0` ngay sau `DONE`).
  Nay xoá `s_busy` (và `s_part`) **TRƯỚC** khi gọi `notify_media_changed()` — lúc đó file đã đóng + đổi tên
  xong nên không còn gì phải bảo vệ. **Đo lại: upload xong không còn lần `refused` nào** (`refused rd/wr=0/0`)
  và Marlin mount lại + lấy danh sách file bình thường.
* Đây là khác biệt bản chất với chiều A: `M22`/`M21` do **màn hình** gửi là *lệnh* cho Marlin, còn `M21`/`M22`
  do **Marlin** gửi chỉ là *yêu cầu sector* — cùng tên lệnh nhưng đi ngược chiều nhau, không đụng nhau.

**P2 — số đo thật**: file 1.172.890 B toàn dòng chú thích (Marlin đọc hết tốc lực, không chạy lệnh nào)
được đọc xong trong **~90–105 s ⇒ ~11–13 KB/s** (trần lý thuyết của link 250 000 baud là ~23 KB/s; phần
hao là độ trễ vòng lặp của màn hình CYD + ghi UART blocking). In thật chỉ cần **1–4 KB/s** ⇒ dư **3–12 lần**.
Ghi thì chậm hơn (mỗi sector = 5 lượt đi–về ≈ 30–40 ms) nhưng chỉ dùng cho log/xoá nên không quan trọng.
*Lưu ý khi đo*: file toàn chú thích làm hàng đợi G-code không bao giờ đầy ⇒ Marlin ở trong vòng đọc thẻ
liên tục nên **không trả lời lệnh qua USB** cho tới khi đọc xong (đó là hành vi của Marlin, không phải lỗi
của đường SD). File có lệnh thật (`G4`…) thì `M27` trả lời bình thường.

**Còn lại / tuỳ chọn**:
* Bật `POWER_LOSS_RECOVERY` nếu muốn Marlin tự ghi file phục hồi khi mất điện (đường ghi đã sẵn sàng).
* Muốn nhanh hơn: nâng baud 2 đầu (`CYDLCD_BAUD` + `CYDLCD_MARLIN_BAUD`) lên 460800/921600.
* ⚠️ **Mọi thao tác thẻ của Marlin giờ phụ thuộc UART + màn hình CYD còn sống.** Màn hình CYD chết/bận ⇒ Marlin
  không đọc được file ⇒ **bản in dừng**. Đây là đánh đổi lớn nhất so với MUX 4053 (mục 11 vẫn giữ
  nguyên code để quay lại: chỉ cần bỏ `CYDLCD_SD_OVER_UART` ở Marlin và bật `CYDLCD_SD_SHARED` ở màn hình CYD).

### 12.6 Bài test xác nhận driver SD qua UART — **chỉ lệnh get-info, KHÔNG gia nhiệt, KHÔNG chuyển động**

Mục đích: khẳng định `DiskIODriver_CydlcdUART` (Marlin) + card server (màn hình CYD) chạy chắc, **không cần
bật hotend/bàn, không di chuyển trục nào**. Dùng được cho cả khi máy đang nguội.

**Bằng chứng định lượng**: màn hình CYD in bộ đếm sector ở dòng log 1 Hz (COM7):

```
[MRL] st=0 fl=0x22 prog=0% file="" | SD r=274/0 w=5/0
                                       ^^^^^^^  ^^^^^^^
                                       đọc: 274 lần, 0 lỗi | ghi: 5 lần, 0 lỗi
```

**Trình tự test** (COM4 = Marlin; `cydlcd-upload.ps1` để đưa file lên thẻ):

| Bước | Lệnh | Kỳ vọng |
|---|---|---|
| 0 | `M105` | nhiệt độ hiện tại + **target `0.00`** (mốc "không gia nhiệt") |
| 1 | `M115` | `Cap:SDCARD:1`, `Cap:SD_WRITE:1` |
| 2 | `M22` / `M21` ×4 | `echo:SD card released` rồi `echo:SD card ok` — mỗi lần mount = đọc sector 0 + FAT qua UART |
| 3 | `M20` ×6 | **6/6** lần thấy file (liệt kê = đọc FAT + sector thư mục) |
| 4 | `M20 L` | liệt kê **tên dài** (đọc entry DIR) |
| 5 | `M33 INFO.GCO` | trả `/INFO.GCO` (tra tên dài) — ⚠️ Marlin này **không có `M36`** (báo "Unknown command") |
| 6 | `M23 <file>` | `File opened: <tên> Size: <n>` rồi `File selected`; **phải `M21` trước nếu vừa `M22`** |
| 7 | tạo `INFO.GCO` = 1.200 dòng `M105`/`M114`/`M27` (+ padding `;`) ≈ 93 KB, upload qua WiFi | `/status` → `done` |
| 8 | `M23 INFO.GCO` + `M24` | đọc **toàn bộ** file; `M27` báo `SD printing byte 93200/93200` rồi `Not SD printing` |
| 9 | `M105` lại | target vẫn `0.00` ⇒ **không hề gia nhiệt** |
| 10 | `M30 INFO.GCO` | `File deleted:...` (đường **GHI** chạy) + `M20` không còn file ⇒ dọn thẻ luôn |

**Kết quả đo thật (2026-10-14, máy này)**:

* Bộ đếm màn hình CYD: **`SD r=274/0 w=5/0`** — 274 sector đọc + 5 ghi, **không lỗi nào**; không có dòng
  `doc RAW sector ... THAT BAI` trong log.
* File get-info 93.200 B: `M27` báo **100 % ở t = 13,8 s** (≥ **6,8 KB/s**, chậm hơn trần vì mỗi dòng
  `M105/M114/M27` còn in ra USB), `Not SD printing` ở t = 18,4 s.
* Nhiệt độ trước/sau: `T:30.76 /0.00 B:28.76 /0.00` → `T:30.71 /0.00 B:28.83 /0.00` ⇒ **target không đổi**.
* `M22`/`M21` ×4 đều báo OK; `M20` **6/6**; `M20 L` ra `INFO.GCO 93200 INFO.GCO`; `M33` ra `/INFO.GCO`.
* `M23` lặp lại **ngay sau `M22`** không in gì (thẻ đã bị nhả) — nhớ `M21` trước.

### 12.7 ⛔ KHÔNG thể nạp firmware Marlin **từ thẻ SD** trong kiến trúc này (2026-10-14)

> **Trạng thái: CHƯA TEST — nhưng theo thiết kế là FAIL. Đừng trông cậy vào đường này.**
> Ghi ra đây để lần sau không mất thời gian thử: cả 3 đường “nạp từ thẻ” đều không đi được.

| Đường nạp từ thẻ | Vì sao fail |
|---|---|
| **Marlin `SD_FIRMWARE_UPDATE`** (đọc file trên thẻ rồi tự ghi flash) | Tính năng này **chỉ dành cho ATmega2560**: `SanityCheck.h:3929` báo `#error "SD_FIRMWARE_UPDATE requires an ATmega2560-based (Arduino Mega) board."` ⇒ với STM32F407 **bật lên là không build được**, không phải “chạy sai”. Bản này để comment (`Configuration_adv.h:1749`) |
| **Bootloader MKS nạp từ thẻ** (đặt `mks_monster8.bin`/`.cur` ở gốc thẻ rồi reset) | Bootloader phải **tự đọc thẻ SPI trên mainboard**, nhưng thẻ nay nằm ở **khe của màn hình CYD** (mainboard đã `NO_SD_DETECT`, không còn dùng SPI thẻ — khe thẻ trên board **để trống**; xem §12.3). Đường duy nhất tới thẻ đang dùng là **UART + giao thức của màn hình** (`cyd_usart` / SD-over-UART, §12.4) mà **bootloader KHÔNG có driver/giao thức đó** ⇒ nó “không thấy thẻ” ⇒ **không có file để nạp**. *(Ngoại lệ duy nhất, **CHƯA TEST**: **cắm thêm một thẻ vào khe trên mainboard** thì bootloader có thể đọc được — vì đó là SPI thẻ thật của board; nhưng Marlin ở cấu hình này **không dùng** khe đó, nên thẻ ấy chỉ dùng được cho việc nạp firmware, không dùng để in.)* |
| **Marlin nạp hộ** (`M997` + file trên thẻ) | `M997` ở bản này (`STM32_DFU_REBOOT`) chỉ **nhảy vào ROM DFU qua USB** — không đọc/copy từ thẻ (`HAL.cpp:172-224`, `Marlin/src/gcode/control/M997.cpp`) |

**Vì sao chắc chắn vướng (lý do kiến trúc)**: đọc thẻ trong cấu hình này cần **Marlin đang chạy**
(để gửi `CYDLCD_F_SDREQ` xin từng sector) **và màn hình CYD đang chạy** (để trả sector). Lúc cần
nạp firmware thì chính Marlin là thứ đang bị thay ⇒ **vòng lặp con gà–quả trứng**, bootloader không
thể tham gia vào giao thức đó.

**✅ Cách nạp đúng** (đã chạy thật):
* **Marlin**: USB DFU — `M997` (hoặc nút **REBOOT DFU** trong trang CUSTOM / menu Advanced) rồi
  `dfu-util -a 0 -s 0x0800C000:leave -D .pio\build\mks_monster8\mks_monster8.bin`
  (chi tiết: **`README.md` gốc §11.6**), hoặc **ST-Link**.
* **Màn hình CYD** (firmware riêng, không liên quan Marlin): nạp qua **COM7** —
  `pio run -d cydlcd -e cydlcd_esp32 -t upload --upload-port COM7`.
* ⚠️ Hai firmware **nạp riêng**; đổi giao thức (proto) thì phải nạp **cả 2** (§12.4).





## Thanh day (status bar) + phan trang noi dung
Them thanh co dinh o DAY man hinh (cao 22 px, `BOT_Y = 240 - 22 = 218`), sticky giong header:
`[PREV]  TRANG x/y  [NEXT]`.
- `bot_sync(true)` chay o CUOI `show_page()`: do noi dung THUC TE cua man vua mo.
  - Noi dung gon trong 206 px => **khong hien** thanh day, bo cuc giu nguyen nhu cu
    (TEMP/MESH/JOG/RATE/FILES/CUSTOM/ENDSTOP/TRAM/INFO khong doi gi).
  - Noi dung tran => thu trang con `CONT_H - BOT_H = 192` (vung nhin 184 px) + hien thanh day.
- Moi man con gio la flex COLUMN **cuon doc duoc** (an thanh cuon) => keo bang ngon tay cung cuon duoc.
- `page_scroll(±1)` = `lv_obj_scroll_by(±(height - 2*PAGE_PAD), LV_ANIM_ON)`, dung chung cho MOI man,
  khong rieng MENU. Nut nao het duong cuon thi bi lam mo (`bg_opa 40`).
- `bot_sync(false)` chay o nhip cham 400 ms: chi cap nhat nhan + do mo nut, KHONG do lai chieu cao
  (xem cam bay 44 o README goc).
- MENU: `MENU_ROWS` 9 -> 10 (them `Endstops (M119)` -> `PG_ENDSTOP`); **2026-10-15 (cuoi ngay) 12 -> 11**
  (bo muc `Tuning (MPC/PID)` - trung voi Configuration > Advanced Settings > Temperature); **11 -> 10**
  (chuyen `Endstops (M119)` vao menu MOTION, ngay sau `Stow Z-Probe`). MENU co 10 muc +
  dong "Calibrate touch" = 11 hang x 23 px = 253 px > 206 => thanh day luon xuat hien o MENU va phan
  trang duoc. Day la duong de them muc menu moi ma khong phai nhoi vao `PG_CUSTOM` nua.
- STATUS khong co thanh day (giong header).

## Chan doan link 2 chieu UART panel ↔ Marlin (2026-10-15, vòng 2)

**Triệu chứng người dùng:** *"bấm nút trên màn hình mà máy không chạy"*; trước đó là *"bed mesh mất hết dữ liệu,
vào thấy trắng"*. Trên màn hình **không có gì bất thường** (nhiệt độ, tiến độ, tên file vẫn chạy).

**Đo được (không đoán):**

| Đo bằng | Kết quả |
|---|---|
| `M220 S42` gửi **từ màn hình** rồi hỏi Marlin qua COM4 (`M220`) | Marlin vẫn `FR:100%` ⇒ lệnh **không tới** Marlin |
| `M20` trên COM4 (Marlin hỏi thẻ qua UART) | `V300_P~1.GCO 64439` rồi `PANEL SD: doc sector 3010 THAT BAI` |
| `UI BOX` trang BED MESH | `khong ho tro (CYDLCD_HAS_MESH=0)`, `PROTO: 0/4`, `MARLIN: 0x0000` ⇒ **chưa từng nhận HELLO** |
| `UI RX` (mới) | `STATUS=67 POS=67 …` nhưng `PING` (bên Marlin) `= 0`, `PONG CHUA BAO GIO` |
| `PANEL DIAG` (mới, in ở COM4 mỗi 5 s) | `frames=44 err=0 GCODE=1 REQ=13 PING=0 … hoan=6 bo=4` |

⇒ **Hai chiều của UART hỏng ĐỘC LẬP nhau**: chiều Marlin → màn hình tốt (STATUS/TEMPS/HELLO đều tới khi Marlin
mới khởi động), còn chiều **màn hình → Marlin bị KẾT** (mọi G-code/REQUEST/PING đều mất). Nạp lại Marlin (DFU)
thì hết ngay ⇒ **không phải dây lỏng** mà là RX của Marlin bị treo (STM32: cờ OverRun/ORE không được xoá thì
`RXNE` không bao giờ bật lại). Hệ quả kép: (a) bấm nút không chạy; (b) màn hình reset xong **không bao giờ** nhận
được HELLO ⇒ mất mesh/tham số ⇒ đúng triệu chứng *"bed mesh trắng"*.

**5 lỗi đã sửa:**

1. **Màn hình KHÔNG BAO GIỜ gửi PING** (`cydlcd_link.cpp`): điều kiện cũ `if (!tbt_link_active())` — mà ở chế độ
   PUSH, dòng TBT đi trong **frame nhị phân** `CYDLCD_F_TBT` nên `tbt.ever = true` **ngay frame đầu** ⇒ PING bị tắt
   vĩnh viễn ⇒ Marlin không bao giờ biết màn hình còn sống (`cydlcd_alive` luôn false) và màn hình không có cách
   nào biết Marlin có nhận được lệnh hay không. Nay chỉ bỏ PING khi **đúng chế độ TRAM ASCII thô** (`tbt.ever &&
   !cydlcd.rx_frames`); chế độ PUSH: PING 1 s/lần.
2. **PONG + cảnh báo** (`cydlcd_link.cpp`): đếm tuổi PONG; quá **5 s không có PONG** trong khi vẫn nhận frame
   trạng thái ⇒ in `[UP] CANH BAO: khong nhan duoc PONG tu Marlin > 5 s … LENH GUI TU MAN HINH CO THE KHONG CHAY`
   (10 s/lần, kèm số lần) — chính lỗi này trước đây **im lặng hoàn toàn**.
3. **HELLO resync nay XIN MÃI** (`cydlcd_link.cpp`): bản trước chỉ xin lại **5 lần** rồi thôi ⇒ chỉ cần 5 lần đó
   trượt (Marlin đang bận đọc thẻ ⇒ frame REQUEST bị **hoãn rồi bỏ**) là màn hình **mất mesh vĩnh viễn**. Nay
   2 s/lần trong 10 s đầu, sau đó 10 s/lần cho tới khi nhận được HELLO (mất link thì đếm lại từ đầu).
4. **Hàng hoãn frame lệnh: 2 → 8 slot** (`cydlcd_serial.cpp`, phía Marlin): màn hình gửi **3 REQUEST liền nhau**
   (`REQ_FULL` + `REQ_SETTINGS` + `REQ_MESH` — lệnh `UI HELLO` hoặc lúc resync) mà Marlin đang đọc thẻ thì cái thứ
   3 bị **bỏ** (`bo=4` đo được) ⇒ mất HELLO/mesh. Nay 8 slot + đếm `hoan/bo`.
5. **Watchdog RX phía Marlin** (`cydlcd_serial.cpp`): **10 s không nhận được frame nào** ⇒ tự
   `CYDLCD_SERIAL.begin()` lại + xoá nửa frame đang dở + log `PANEL: khong nhan duoc frame nao tu man hinh > 10 s
   -> khoi tao lai UART (lan N)` ⇒ tự phục hồi **không cần reset máy**.

**Đo lại sau khi sửa** (đã nạp cả 2 board): `UI RX` → `hello=0x9F`, `PONG cuoi 561 ms`, `HELLO=1 STATUS=66 POS=66
MESH=7 FILES=3 SETTING=52`; `PANEL DIAG` → `PING=47 … alive=1`; `M220 S42` từ màn hình ⇒ Marlin `FR:43%` và màn
hình hiện **43%** (đã trả lại `M220 S100`).

**Sửa kèm (dòng MENU):** mục `Print from Media` (tên cũ `SD card`, đổi 2026-10-15) **không còn đếm số file**
(yêu cầu người dùng: *"bỏ phần đếm số lượng file ở
menu file"*) và mục `Settings` không còn ghi `58/33` (tổng 33 là con số cũ; Marlin gửi 58 dòng) mà ghi **`58 muc`**
— màn hình không biết tổng số thật vì Marlin không gửi frame "hết danh sách" cho SETTING.

## "Bấm nút trên màn hình mà máy không chạy" — kiểm tra lại toàn bộ đường LỆNH (2026-10-16)

**Câu hỏi:** *"khi bấm vào menu của cydlcd thì có gửi lệnh đến Marlin không? các lệnh `G28`, `G28 X/Y/Z` đều
không chạy"*. Trả lời **có** — code màn hình gửi đúng; nhưng đường đi có **3 chỗ có thể bỏ lệnh trong im lặng**,
nay đã bịt và **có bằng chứng ở cả 2 đầu dây**. Đường đi đầy đủ (kiểm bằng đọc code + bộ test host):

| Bước | Nơi | Kiểm chứng |
|---|---|---|
| 1. Bấm nút `HOME ALL (G28)` / `G28 X` / `G28 Y` / `G28 Z` ở trang JOG | `ui_page_jog.cpp:107,112-114` → `cb_action` → `do_action(A_HOME/A_HOME_X/Y/Z)` | `ui_lvgl.cpp:326-329` gọi `cydlcd_link_send_gcode("G28" / "G28 X" / …)` |
| 2. Đóng gói frame `CYDLCD_F_GCODE` payload = `u8 len` + chuỗi | `cydlcd_link.cpp: send_gcode_locked()` → `cydlcd_tx_str()` (giao thức dùng chung 2 phía) | Test host `test_cydlcd_link.cpp` mục **8** + **8b**: payload `[4]G28` … đọc lại ra đúng `G28`/`G28 X`/`G28 Y`/`G28 Z` |
| 3. Ra UART (USART3 PD8/PD9 ↔ GPIO22/GPIO35) | `send_frame_user()` → `send_frame_wait()` | Mục **8c**: UART kín ⇒ frame bị bỏ **nhưng CÓ ĐẾM** (`cydlcd_link_tx_dropped_user`) |
| 4. Marlin nhận frame (CRC8) → bỏ byte `len` → `queue.enqueue_one()` | `Marlin/src/lcd/extui/cydlcd_serial/cydlcd_serial.cpp` (`case CYDLCD_F_GCODE`) | In `PANEL: lenh [G28]` ra COM4 cho **mọi** lệnh nhận được |
| 5. Hàng đợi → `parser.parse()` → `G28()` chạy | `Marlin/src/gcode/queue.cpp` (`BUFSIZE = 4` lệnh!) | `PANEL DIAG: LENH tu man hinh: nhan=/xep=/cho lai=/bo=` (1 dòng/5 s) |

**3 chỗ từng bỏ lệnh trong im lặng (đã sửa):**

1. **Panel — hàng đợi TX đầy:** `send_frame()` (frame trạng thái) bỏ frame và chỉ tăng `s_tx_drop`. Lệnh người
   dùng nay đi đường riêng `send_frame_user()`: **chờ tối đa 120 ms** cho TX rồi mới bỏ, có **đếm riêng**
   (`UI RX: … BO LENH (G-code)=n`) + log. `n > 0` ⇒ lỗi ở phía màn hình/hàng đợi, không phải ở Marlin.
2. **Marlin — hàng đợi lệnh đầy:** `BUFSIZE = 4` nên khi đang in từ thẻ (hoặc đang trong một vòng chờ nóng
   dài) hàng đợi gần như luôn kín. Bản cũ **bỏ lệnh sau 2 s**; nay **giữ lại 1 lệnh và thử lại mỗi vòng
   `onIdle()` tới 20 s** (`CYDLCD_CMD_PENDING_MS`) rồi mới bỏ — vẫn **không bao giờ block** vòng lặp chính.
3. **Tranh chấp 2 core trên bộ tạo frame:** task `ui` (core 0 — bấm nút) và task `link` (core 1 — PING/trả lời)
   cùng dùng biến `s_tx` + hàng đợi TX. Không khoá thì core 1 có thể **ghi đè frame `G28` đang dựng dở** ⇒ frame
   gửi đi hỏng CRC ⇒ Marlin bỏ **im lặng**. Nay mọi đường gửi đi qua `tx_lock()` (mutex FreeRTOS, có priority
   inheritance nên không đảo ngược ưu tiên; board 1 luồng = khoá rỗng).

**Cách chẩn đoán 3 bước (không cần đoán) khi tái phát:**

| Bước | Lệnh | Kết luận |
|---|---|---|
| 1 | `UI RX` (COM8) | Xem **`BO LENH (G-code)`**: `= 0` ⇒ màn hình ĐÃ gửi (lỗi ở phía sau). `> 0` ⇒ màn hình không gửi được ⇒ xem `TX cho=` (hàng đợi nghẽn) / dây TX |
| 2 | `PANEL DIAG` (COM4, 5 s/lần) | Xem **`LENH tu man hinh: nhan=`**: tăng khi bấm nút = Marlin **có** nhận. `nhan=` không tăng dù `UI RX` báo PONG đều ⇒ **RX của Marlin kẹt** (đã có watchdog tự `begin()` lại sau 10 s) |
| 3 | `nhan=` tăng mà `xep=` không tăng / `cho lai=` tăng | Lệnh tới Marlin nhưng **hàng đợi đầy** (đang in từ thẻ, `BUFSIZE=4`). Nay lệnh được giữ 20 s; nếu vẫn `bo=` tăng thì hàng đợi bị kẹt thật (vd đang chờ nóng mà PSU tắt) |

**Đo được sau khi sửa:** bộ test host **81 kiểm tra / 0 lỗi** (`pwsh cydlcd/tools/run_host_test.ps1`) — trong đó
mục 8b là hồi quy cho cám bẫy #39 (byte `len` ở đầu payload) và mục 8c là hồi quy cho việc bỏ lệnh im lặng.
⚠️ **Bộ test này đã KHÔNG BUILD ĐƯỢC từ 2026-10-15** (xem cám bẫy 46 ở README gốc) — nó chỉ được sửa trong
cùng đợt này, nên trước đó mọi lỗi dưới đây đều **không có lưới an toàn**:

* **`send_frame_wait()` vòng lặp vô tận** khi TX ring rỗng (ESP32 trả `availableForWrite() = 256`, bị cắt bằng
  `(uint8_t)` thành **0**) ⇒ task `link`/`ui` treo ⇒ watchdog **tự restart màn hình**, Marlin báo
  `PANEL SD: doc sector ... THAT BAI`. Đây là lỗi nặng nhất tìm được trong đợt này (cám bẫy 45 ở README gốc).
* **`tbt` không được publish** ở nhánh không-CYD (`state_publish()` chỉ copy `cydlcd`) ⇒ board F411/F407 LVGL:
  trang **TRAMMING đứng dữ liệu cũ (0)** dù link vẫn chạy.
* `run_host_test.ps1` để `$ErrorActionPreference='Stop'` quanh lệnh `g++` ⇒ **mọi WARNING của g++ bị PowerShell
  5.1 biến thành lỗi kết thúc** ⇒ script báo lỗi và **không chạy test** dù biên dịch xong.

**Chưa kiểm được (không có board trong tay):** hành vi trên dây thật. Mọi kết luận ở trên là từ **đọc code +
test host**; khi có board, chạy đúng 3 bước ở bảng chẩn đoán trên để xác nhận `nhan=` tăng khi bấm `G28`.


## Menu MOTION kiểu 12864 (2026-10-15) — nhóm `SMG_MOTION` trong `src/ui_page_submenu.cpp`

**Yêu cầu người dùng:** *"sửa nốt menu 'motion' giống sơ đồ menu của 12864"*. Trước đây dòng `Motion (JOG)`
trong MENU mở **thẳng trang JOG**; nay nó mở **menu MOTION** đúng thứ tự của menu `Motion` trong 12864
(`docs/12864-menu-map.md`, nút m20). Đây là **nhóm ngang cấp CONFIGURATION** (`parent = SM_PARENT_NONE`)
nên BACK từ MOTION về thẳng MENU — giống 12864 (Motion nằm trong Main menu).

```
MOTION
├─ Move Axis (JOG)          mở trang JOG (chọn trục + bước 0.1/1/10 mm như cũ)
├─ Z Probe Wizard           mở lớp phủ sửa M851 Z (như 12864)
├─ Tramming Wizard          mở trang TRAMMING (Bed Tramming Wizard)
├─ Unified Bed Leveling >   Activate (G29 A) · Deactivate (G29 D) · Bed Mesh (trang lưới) ·
│                           Manually Build Mesh (G29 I999 + G29 P2BT0) · Fill-in (G29 P3) ·
│                           Fine Tune All (G29 P4RT) · Save Bed Mesh (G29 S0) · Output UBL Info (G29 W) ·
│                           Output for Host (G29 T0) · Output for CSV (G29 T1) · Off Printer Backup (G29 S-1)
├─ Level Bed (G29 N)
├─ Deploy Z-Probe (M401) · Stow Z-Probe (M402)
├─ Endstops (M119)          ★ NGAY SAU `Stow Z-Probe` (2026-10-15: **chuyển từ MENU chính vào đây**)
├─ M48 Probe Test           gửi `G28 O` rồi `M48 P10` (đúng như 12864: `G28O\nM48 P10`)
└─ Disable Steppers (M84)
```

* **`Endstops (M119)` nằm ở ĐÂY, ngay sau `Stow Z-Probe (M402)`** (yêu cầu người dùng 2026-10-15: *"chỗ hiển
  thị endstop (m119) thì hãy cho vào motion"* rồi *"di chuyển menu 'endstop' xuống ngay sau 'stow probe'"*):
  dòng này đã **bỏ khỏi MENU chính** (`MENU_ROWS 11 → 10`), MENU giờ còn 10 mục + `Calibrate touch`.
  Trang ENDSTOP cũng đổi cách đọc: **bỏ nút `REFRESH`**, tự đọc lại **30 lần/giây**, và **ra khỏi màn là
  ngừng monitor** — xem mục *"ENDSTOP: đọc 30 lần/giây"* bên dưới.

* **KHÔNG có dòng `Fade Height`** (12864 có, ở cả `Motion` và `UBL`): từ 2026-10-15 nó được **gom về MỘT chỗ**
  trong CONFIGURATION, ngay dưới `Probe Z Offset` — xem mục *"★ Một chỗ DUY NHẤT cho `Fade Height`"*.
* **KHÔNG có mục `Homing >`** như 12864 (yêu cầu người dùng 2026-10-15: *"ẩn menu Homing vì đã có hết
  trong JOG rồi"*): trang JOG đã có `HOME ALL (G28)`, home từng trục X/Y/Z và `M84` — thêm menu con nữa là thừa.
* **KHÔNG có `Auto Z-Align (G34)`** — G34 **bị tắt hẳn** trên máy này (trục Z cứng), xem mục
  *"TẮT HẲN G34"* bên dưới.

## ENDSTOP: đọc 30 lần/giây, bỏ nút REFRESH, chỉ hiện chân có thật (2026-10-15)

**Yêu cầu người dùng:** *"chỗ hiển thị endstop (m115) thì hãy cho vào motion, đồng thời cập nhật value 10 lần
1 giây đi, không cần nút refresh nữa, giá trị hiển thị của z-probe đang bị ngược, bởi vì chưa gọi deploy probe
z nên value chưa là open"* → sau đó: *"di chuyển menu 'endstop' xuống ngay sau 'stow probe', đọc tín hiệu M115
30 lần 1 giây, khi ra khỏi menu thì bỏ việc monitor tín hiệu của màn hình 'endstop' đi"*.

| Việc | Ở đâu |
|---|---|
| Vào từ **Motion**, **ngay sau `Stow Z-Probe (M402)`** | `SM_I_MOTION` (`ui_page_submenu.cpp`): `Move Axis · Z Probe Wizard · Tramming · UBL · Level Bed · Deploy Z-Probe · Stow Z-Probe · **Endstops (M119)** · M48 Probe Test · Disable Steppers`; `MENU_ITEMS` bỏ dòng `Endstops (M119)` ⇒ **`MENU_ROWS 12 → 11 → 10`** (chỉ số cột giá trị ở MENU: Tramming 7 · link 8 · WiFi 9) |
| Bỏ nút `REFRESH` | `ui_page_endstop.cpp`: `mk_btn("REFRESH (M119)", …)` **đã xoá**; vẫn còn `A_REQ_ENDSTOPS` trong enum/`do_action` (không còn nút nào dùng) |
| **30 lần/giây** | `ES_REQ_MS 30` + hàm mới **`page_endstop_poll()`** (xin `CYDLCD_REQ_ENDSTOPS` + vẽ lại nhãn). Nó được gọi từ `ui_lvgl_task_body()` (task `ui` **30 Hz = 33 ms**) **TRƯỚC cổng `UI_REFRESH_MS`** và **trong khoá trạng thái** (`cydlcd_link_state_lock()`), **chỉ khi `s_page_act == PG_ENDSTOP`** |
| **Ra khỏi menu = NGỪNG monitor** | `page_endstop_poll()` chỉ được gọi khi trang ENDSTOP đang mở; đổi trang là không còn ai gọi ⇒ **không xin frame M119 nữa**. `page_endstop_destroy()` còn xoá `es_next_ms = 0` để lần sau vào lại xin frame đầu tiên ngay |
| `UI_REFRESH_MS` trả về **150** | Đã thử hạ 150 → 90 để lấy 10 Hz cho *mọi* trang; khi trang ENDSTOP có đường riêng thì **trả lại 150** cho các trang khác như cũ (đo lại: man ENDSTOP vẫn 30 lần/giây) |
| Chỉ hiện chân có thật | Thêm `uint8_t es_present` (**bitmask id**) vào struct `cydlcd` (`cydlcd_link.h`), phía Marlin điền trong `CYDLCD_F_ENDSTOPS`. Trang ENDSTOP **ẩn** dòng nào không có bit ⇒ máy này chỉ còn **PROBE · X min · Y min** (trước đây `Z min / X max / Y max` hiện `mo (open)` **giả tạo**) |
| ⚠️ Bẫy: ẩn/hiện phải SO SÁNH cờ trước | `lv_obj_add_flag/clear_flag(HIDDEN)` **luôn** gọi `lv_obj_invalidate()` + đánh dấu layout lại ⇒ gọi vô điều kiện 6 dòng × mỗi lượt làm **cả trang vẽ lại qua SPI**: đo được lượt cập nhật từ **0,1 ms nhảy lên 27 ms**, `ui cham >=20ms` = **11 lần/giây** (màn hình giật). Sửa: chỉ gọi khi `lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) != !present` ⇒ về lại **0,36 ms, 0 lần/giây** |
| **PROBE = `open` khi chưa deploy** | Marlin-side `push_endstops()` (`cydlcd_serial.cpp`): máy này `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN` + **`PROBE_ENABLE_DISABLE`** ⇒ chân Z_MIN **chính là đầu dò**, mà đầu dò chỉ trả tín hiệu khi **đã được bật** (`M401` / `G28 Z` → `probe.deploy()` → `endstops.enable_z_probe(true)`, trên MKS Monster8 `PROBE_ENABLE_PIN = PA8`). Lúc chưa deploy, PA8 = LOW nên đầu đọc lên mức "TRIGGERED" ⇒ panel báo `CHAM` **sai**. Nay gửi `0` (open) khi `!endstops.z_probe_enabled` — đúng như Marlin bỏ qua chân này trong `endstops.cpp update()` (`if (TERN1(Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN, z_probe_enabled)) PROCESS_ENDSTOP_Z(MIN);`). Thêm `#include "../../../module/endstops.h"` (dưới `#if HAS_BED_PROBE`) |

**Vì sao vẫn phải nạp LẠI CẢ MARLIN** (không chỉ màn hình): phần "PROBE = open khi chưa deploy" nằm ở phía
Marlin. Đã nạp `mks_monster8.bin` qua `M997` + `dfu-util` (bản sao ở `D:\0in\mks_monster8.bin`).

**Kiểm trên máy thật:**

| Lệnh / phép đo | Kết quả |
|---|---|
| `UI CFG 15` | `[15] MOTION dong=10/10`; **dòng 7 = `Endstops (M119) ACT 40`** (ngay sau `Stow Z-Probe (M402)`) |
| `UI HIT 7` (trong MOTION) | `trang 11 'ENDSTOP'`, BACK về đúng `MOTION` (nhớ dòng vừa bấm) |
| `UI MENU` + `UI BOX` | **10 mục** + `Calibrate touch`; cột giá trị đúng dòng (`Info = TBT`, `WiFi = DA NOI`) |
| `UI BOX` trang ENDSTOP | 3 dòng hiện (PROBE/X min/Y min), 3 dòng `[AN]` (ẩn, `y=0` — flex bỏ qua); nội dung 95 px ⇒ `thanh day an` |
| `M119` trên COM4 | `z_min: TRIGGERED` — **Marlin vẫn báo vậy** (đó là mức điện của chân); panel mới là chỗ gửi `open` khi chưa deploy |
| `M401` (Deploy Z-Probe) rồi xem lại trang ENDSTOP | ✅ **người dùng đã thử trên máy: đúng** — chưa deploy thì `PROBE = mo (open)`, sau `M401` (đầu dò đã bật) thì `PROBE = CHAM (triggered)`. `M402` (Stow) đưa về `open` như cũ |
| Đo nhịp trên COM4 (`PANEL DIAG … REQ=`) khi **đang mở** trang ENDSTOP | **300 frame / 10 s = 30,0 lần/giây** |
| Đo nhịp sau khi **`UI BACK` ra khỏi** trang ENDSTOP | `REQ` đứng yên: **0 frame / 5 s** ⇒ đã ngừng monitor |
| `[CYD] ui cham >=20ms` khi mở trang ENDSTOP | **0 lần/giây**, lượt cập nhật **≈0,36 ms** |
| `UI HELLO` (hàng đợi TX của màn hình) | `tx cho=0 bo=0` ⇒ 30 frame/s **không** làm rớt frame nào |
| `PANEL DIAG` phía Marlin | `err=0`, `hoan=18 bo=2` (tích luỹ từ lúc boot), `sd_busy=0 defer=0` |

## Đa ngôn ngữ (i18n) kiểu Marlin (2026-10-15) — `src/ui_lang.h`, `src/ui_lang.cpp`

**Yêu cầu người dùng:** *"các text trong project chưa sử dụng kiểu language cho multiple language giống
marlin, cách làm như thế nào thì ổn nhất?"* → đã chọn **B1 + B2 + B3, Việt + English**.

### Vì sao chọn "enum + bảng `const`" (chứ không `lv_i18n`/gettext/JSON trên thẻ)

| Ràng buộc thật của panel | Hệ quả |
|---|---|
| **DRAM tĩnh (`dram0_0_seg`) đã ĐẦY** (mấy lần trước thêm vài chục byte là linker báo overflow) | Bảng chuỗi **buộc phải `const`** → nằm ở **FLASH**. RAM chỉ tốn **1 byte** (`s_lang`) + 1 con trỏ nút BACK. KHÔNG copy chuỗi vào RAM, KHÔNG `String`/`std::map` |
| Flash còn ~1,8 MB | Thêm 1 bảng (~50–600 chuỗi × ~30 B) là không đáng kể. Đo sau B1: **+2,4 KB flash, +8 B RAM** |
| `gfx4_font_text` = Segoe UI 16 **đã có ASCII + tiếng Việt** (`vn1..vn4`) | **VI ↔ EN miễn phí font**. Ngôn ngữ có chữ mới (Thái/CJK…) phải sinh lại font ⇒ font phình rất nhanh ⇒ **không làm** |
| `lv_i18n`/gettext/JSON cần **RAM để giữ bảng đã parse** + filesystem | Với DRAM đã đầy là tự sát; thêm nữa chuỗi mất tính `const` ⇒ không kiểm tra được lúc biên dịch |

### Cách làm (đúng "linh hồn" `multi_language.h` của Marlin)

```c
// ui_lang.h
typedef enum { UI_T_NONE = 0, UI_T_CHROME_BACK, /*...*/ UI_T_COUNT } ui_text_t;
typedef struct { uint16_t id; const char *s; } ui_lang_ovr_t;   // 1 dong cua bang GHI DE
const char *ui_t(ui_text_t id);          // chuoi theo ngon ngu dang chon, KHONG BAO GIO NULL
#define T(id) ui_t(id)

// ui_lang_vi.h  = BANG GOC, day du, LIEN MACH theo enum (const -> FLASH, khoi tao co ten)
const char *const UI_STR_VI[UI_T_COUNT] = { [UI_T_NONE] = nullptr, [UI_T_CHROME_BACK] = "BACK", /*...*/ };

// ui_lang_en.h  = BANG GHI DE: chi nhung chuoi KHAC tieng Viet (VD 24 dong / 175 ID)
const ui_lang_ovr_t UI_OVR_EN[] = { { UI_T_SUB_THEM_VAT_LIEU, "+ Add material" }, /*...*/ };
```

**Chính sách 2 bảng (đổi ở batch 2):** bảng **VI là bảng gốc** (phải đầy đủ — cũng là bản dự phòng); bảng
**EN chỉ ghi đè** — ID nào không có trong EN thì `ui_t()` tự lấy chuỗi tiếng Việt, đúng nghĩa *"chuỗi này
giống hệt ở 2 ngôn ngữ"*. Lý do: ~150/175 chuỗi của panel là **thuật ngữ tiếng Anh** dùng chung
(`Max X Speed`, `Activate UBL (G29 A)`, `Bed PID-P`…), chép lại y nguyên vào bảng EN chỉ tạo ra ~150 dòng
trùng lặp vô nghĩa. **Đổi lại (đã biết trước):** checker *không* tự phát hiện được "quên dịch" — nên có
`python tools/check_lang.py --fallback` in ra danh sách ID đang dùng chung bản VI, **mỗi batch phải đọc
danh sách đó một lần** để chắc không lọt chữ tiếng Việt vào bản tiếng Anh.

* Marlin dùng **struct 1 ngôn ngữ** + `GET_TEXT(MSG)` (tam phân runtime) vì số chuỗi ít; panel có ~600
  chuỗi nên dùng **ID + mảng** — thêm ngôn ngữ = thêm 1 file, và **kiểm tra được** bằng script.
* Đổi ngôn ngữ: `ui_lang_set()` ghi NVS (`Preferences` namespace **`"ui"`**, khoá **`"lang"`**) rồi
  `ui_lang_apply_ui()` (**ui_lvgl.cpp**) đặt lại chữ chrome + **thu hồi/tạo lại trang đang mở**
  (`page_rebuild()` — với menu con thì GIỮ NGUYÊN nhóm đang mở) và đóng lớp phủ đang mở. Chuỗi đã được
  LVGL **copy vào từng nhãn lúc tạo** nên bắt buộc phải dựng lại.
* Chỉ số ngôn ngữ lưu NVS ⇒ **nhớ qua lần tắt máy** (đã đo: `UI LANG 1` rồi **reset panel**, sau khi boot
  `UI LANG` → `dang dung 1 = 'English'`).
* Tên trang cho **console** tách khỏi tên trang **hiển thị**: `PG_KEY[]` (KHÔNG dịch, `UI MENU`/`UI ENDSTOP`/
  `UI ?` giữ nguyên ở mọi ngôn ngữ) vs `PG_TITLE_ID[]` (dịch được). Tên tiếng Anh của trang chọn **NGẮN**
  (`TEMP`/`MEDIA`/`MESH`/`SETTINGS`/`CONFIG`) vì ô tiêu đề rộng **cố định 84 px** (`TITLE_W`).
* Menu Configuration (`ui_page_submenu.cpp`) cũng đã chuyển sang ID: `sm_item_t.name_id` và
  `sm_group_t.title_id` là `ui_text_t` (**2 B**) thay cho con trỏ chuỗi (**4 B**) — vừa dịch được, vừa
  **tiết kiệm flash**.
* Chuỗi **không** đưa vào bảng: G-code, khoá NVS, log chẩn đoán `[UI] …`, HTML/HTTP, tên frame protocol,
  ký hiệu (`II`/`[]`/`ON`/`OFF`/`TBT`/`PUSH`/`NVS`/`MAX`/`MIN`) và **endonym** (`Tiếng Việt`/`English`).

### ⚠️ 4 cái bẫy đã gặp thật

1. **GCC (xtensa, `-std=gnu++11`) chỉ nhận khởi tạo có tên khi các chỉ số TĂNG DẦN và LIỀN MẠCH**:
   * thiếu chỉ số **đầu tiên** → `sorry, unimplemented: non-trivial designated initializers not supported`;
   * **có khoảng trống** (bỏ qua vài chỉ số) → cũng lỗi y vậy;
   * chỉ số **đi ngược** → cũng lỗi.
   Đã thử trực tiếp trên compiler thật để chốt: `{ [A0]=0,[A1]="a",[A3]="c" }` ❌ ·
   `{ [A0]=0,[A1]="a",[A2]="b" }` ✅. ⇒ Bảng gốc (VI) **đầy đủ + liền mạch** nên dùng `[ID] = "..."` được;
   bảng **ghi đè (EN)** có khoảng trống nên **phải** dùng **danh sách cặp `{id, chuoi}`** (không có khởi
   tạo có tên ⇒ không bị ràng buộc thứ tự). `check_lang.py` kiểm luôn thứ tự/liền mạch của bảng VI để báo
   trước khi build.
2. **Đừng tin log COM8 khi kiểm chữ có dấu**: `System.IO.Ports.SerialPort.ReadExisting()` giải mã
   **ASCII** nên mọi byte ≥ 0x80 thành `?` — log hiện `Ti???ng Vi???t` **không** có nghĩa là màn hình sai.
   Cách kiểm đúng: đọc **byte thô** rồi in hex. Đã đo được đúng chuỗi UTF-8 trên dây:
   `54 69 E1 BA BF 6E 67 20 56 69 E1 BB 87 74` = `T i ế n g   V i ệ t` ✓.
3. `lv_obj_add_flag/clear_flag(HIDDEN)` **luôn** invalidate + đánh dấu layout lại — nếu gọi vô điều kiện
   mỗi lượt vẽ thì **cả trang vẽ lại qua SPI** (xem cảnh báo ở mục ENDSTOP: lượt cập nhật từ 0,1 ms nhảy
   lên 27 ms). Nguyên tắc chung: **luôn so sánh trạng thái trước khi ghi** style/cờ.
4. Thêm ID mới vào **giữa** enum mà quên dòng trong bảng VI ⇒ mọi chuỗi phía sau **lệch một bậc** (im
   lặng, không lỗi build). Vì vậy `check_lang.py` so **đúng vị trí** từng dòng với enum (không chỉ đếm).

### Kiểm tra tự động — `tools/check_lang.py`

```bash
cd cydlcd
python tools/check_lang.py              # bang GOC phai day du + dung vi tri; bang ghi de phai hop le
python tools/check_lang.py --audit      # 🚨 BẰNG CHỨNG B2: còn chuỗi HIỂN THỊ nào bằng tiếng Việt chưa bọc T()?
python tools/check_lang.py --fallback   # in ra ID đang DÙNG CHUNG bản VI (soi "quên dịch" mỗi batch)
python tools/check_lang.py --hardcoded  # + sổ tiến độ chuyển đổi (đếm thô, nhiều dương tính giả)
```
Script đọc `ui_lang.h` (enum) rồi kiểm: bảng VI **thiếu dòng / thừa / sai vị trí / chuỗi rỗng / khai 2 lần**
và bảng EN **ID lạ / trùng / rỗng** → **exit code 1**. `--hardcoded` in bảng xếp hạng theo file (theo dõi B2).

**`--audit` là bằng chứng hoàn thành B2** (không phải đếm thô): nó bỏ comment, tách từng **câu lệnh** (đến `;`),
bỏ qua câu lệnh **log** (`Serial.`/`WIFILOG`/`HLOG`), bỏ dòng `#warning`/`#error`, rồi soi các literal còn lại:
literal nào "trông như tiếng Việt" (bộ từ khoá âm tiết tiếng Việt) mà **không** nằm trong `T(...)` và dòng đó
không có marker `// i18n-ok` → báo `[CAN DICH]`. Kết quả hiện tại:

```
-- AUDIT: chuoi TIENG VIET con hard-code trong duong HIEN THI --
   [OK] khong con chuoi hien thi nao bang tieng Viet ngoai log/console/G-code
```
(Dòng nào **cố tình** không dịch — khoá console `PG_KEY[]`, log chẩn đoán `UI BOX`, `#error` lúc biên dịch —
được đánh dấu `// i18n-ok (khong phai chu hien thi)` ngay trong mã nguồn.)

### Kiểm "chữ tiếng Anh có bị tràn khỏi nút không"

Nút/nhãn có **bề rộng cố định** (flex chia đều), nên bản EN dài hơn có thể tràn. Cách kiểm: cho màn hình sang EN
rồi `UI BOX` — LVGL đặt nhãn canh giữa trong nút, nên **nhãn rộng hơn nút sẽ có `x` ÂM**. Quét cả bộ trang:

```powershell
# sau khi UI LANG 1, mo tung trang va UI BOX -> tim dong co x am
Select-String -Path <log> -Pattern 'label  x=-\d'
```
Đã quét TEMP · SETTINGS · INFO · FEED/FLOW · MENU · JOG · FILES · MESH · TRAMMING · WIFI · CONFIGURATION: **không
còn nhãn nào `x` âm**. Một trường hợp đã gặp và sửa: nút `DISCONNECT` (10 ký tự ≈ 85 px) **không vừa** nút 76 px
trong hàng 4 nút của trang WIFI (đo được `label x=-10`) → đổi bản EN thành **`LEAVE`** (VI vẫn `NGAT`).

### ⚠️ Bài học thêm: enum vượt 255 thì ID không còn nhét vừa `uint8_t`

Khi số ID vượt 255, các mảng `static const uint8_t WST[3] = { UI_T_… }` (trang WIFI/INFO) báo
`narrowing conversion … to 'uint8_t'` lúc biên dịch → đổi sang **`uint16_t`**. `ui_text_t` là enum (int) nên
bản thân ID luôn đủ chỗ; chỉ các mảng/biến phụ mới phải để ý.

### Trạng thái (cập nhật mỗi batch)

| Batch | Phạm vi | Trạng thái |
|---|---|---|
| **B1** | Khung `ui_lang.*` + `UI LANG` + `check_lang.py` + mục **Language** trong menu + NVS | ✅ **xong, đã kiểm trên máy** (45/45 chuỗi) |
| **B2+batch1** | `ui_chrome.cpp` (BACK/PREV/NEXT/`TRANG %d/%d`) · `PG_TITLE` · `ui_page_menu.cpp` (10 dòng + cột giá trị + `Calibrate touch`) | ✅ xong, đã kiểm |
| **B2+batch2** | **`ui_page_submenu.cpp`** — TOÀN BỘ cây Configuration: 111 tên dòng + 18 tiêu đề nhóm, `sm_item_t.name_id`/`sm_group_t.title_id` = ID 2 B | ✅ **xong, đã kiểm trên máy** (VI y nguyên như trước; EN: `CONFIGURATION`, `TEMP`, `+ Add material`…) |
| **B2+batch3** | `ui_page_jog.cpp` (`TRUC`/`DI CHUYEN (mm)`/hint G91) · `ui_page_custom.cpp` (`M412: BAT/TAT`, 3 dòng ghi chú) · `ui_page_rate.cpp` (dòng gợi ý) · `ui_page_endstop.cpp` (`CHAM`/`mo (open)` + dòng `M119: %u chan…`) · `ui_page_status.cpp` (`LINK ERR (byte khong hop le)`) · `ui_boxes.cpp` (`Chac chan? Bam OK lan nua`, `TUNE XONG`) | ✅ **xong, đã kiểm trên máy** |
| **B2+batch4** | `ui_page_files.cpp` (20 thông báo + 3 hộp xoá file/thư mục) · `ui_page_wifi.cpp` (4 nút + 4 nhãn mật khẩu + 3 trạng thái + lớp phủ "đang quét") · `ui_page_settings.cpp` (2 tiêu đề cột + dòng gợi ý + `(khong co)`) · `ui_page_info.cpp` (`FONT:` + trạng thái WiFi + `co`/`khong`) · `ui_page_mesh.cpp`, `ui_page_tbt.cpp` · `ui_page_preheat.cpp` (`XOA VAT LIEU` + hỏi xoá) · `ui_page_menu.cpp` (`4 goc`) · `ui_boxes.cpp` (**tiêu đề mức độ** `CANH BAO`/`LOI`/`LOI NANG`/`THONG BAO`) · `ui_lvgl.cpp` (hộp xác nhận `DUNG IN`, `M502`, **`REBOOT DFU`**) · `ui_port_cyd_st7789.cpp` (**màn HIỆU CHUẨN cảm ứng**: 12 câu, vẽ bằng TFT_eSPI) | ✅ **xong, đã kiểm trên máy** |
| **Quy tắc chọn có ID hay không** | Chỉ cấp ID cho chuỗi **có tiếng Việt** hoặc **câu/đoạn** mà bản EN sẽ khác. Chuỗi **một từ kỹ thuật / ký hiệu giống hệt ở 2 ngôn ngữ** thì để nguyên trong code (`FAN`, `OFF`, `MAX`, `COOLDOWN`, `NOZZLE`, `BED`, `PROBE`, `X min`, `INIT SD`, `M84`, `G28 X`, `FEED`, `FLOW`, `IDLE`/`PRINTING`…, `%-số`, `mm`, `%`) — thêm ID chỉ tạo dòng trùng lặp vô nghĩa | 📌 ghi tại đây; dòng nào cố tình KHÔNG dịch thì có marker `// i18n-ok` |
| **B3** | Bảng EN: **121 dòng ghi đè** (151 ID còn lại dùng chung bản VI - thuật ngữ tiếng Anh) | ✅ **xong** |
| Còn lại | — | ✅ **B2/B3 hoàn tất** |

**Đã kiểm trên máy thật (COM8):**

| Lệnh | Kết quả |
|---|---|
| `UI CFG 0` (VI) | `[0] CAU HINH dong=9/9` — **y hệt trước khi chuyển i18n** (không hồi quy) |
| `UI CFG 15` (VI) | `[15] MOTION dong=10/10`, đủ 10 dòng đúng thứ tự (Endstops sau Stow Z-Probe) |
| `UI LANG 1` + `UI CFG 0` | `[0] CONFIGURATION`, tiêu đề trang `trang 13 'CONFIGURATION'` |
| `UI LANG 1` + `UI CFG 14` | nhóm Preheat: dòng cuối là **`+ Add material`** (VI: `+ Them vat lieu`) |
| `UI LANG?` | `vi: day du (0/175)` · `en: chuoi rieng 24/175, dung chung ban VI 151 (hop le)` |
| `UI LANG` / `UI LANG 0\|1` / reset | giữ nguyên các kết quả của batch 1 (NVS nhớ đúng) |

## TẮT HẲN G34 (Auto Z-Align) — `CYDLCD_NO_G34` (2026-10-15)

**Yêu cầu người dùng:** *"đối với máy của tôi, hãy tạm ẩn và cấm chức năng G34 do thiết kế trục Z cứng,
kể cả GCODE cũng tắt, g34 không làm gì cả"*.

| Việc | Ở đâu |
|---|---|
| Công tắc | `#define CYDLCD_NO_G34` trong **`Marlin/Configuration.h`** (ngay sau `CYDLCD_SERIAL_UI`). Muốn bật lại G34: comment dòng đó rồi nạp lại Marlin |
| Chặn G-code | `Marlin/src/gcode/calibrate/G34_M422.cpp` → `GcodeSuite::G34()` **thoát ngay** khi bật công tắc: **không home, không kéo probe, không đổi trạng thái leveling**, chỉ trả lời `echo:G34 da TAT tren may nay (thiet ke truc Z cung) - khong lam gi` |
| Ẩn trên panel | bỏ dòng `Auto Z-Align (G34)` trong menu MOTION, bỏ nút `G34 Bed` ở trang CUSTOM (còn `Init SD` + `ENDSTOP`), đổi tên dòng MENU `Custom G34/SD` → **`Custom/Init SD`** (cột giá trị `G34/M22` → `M22`), và `A_G34` thành no-op (chỉ ghi log, KHÔNG gửi G-code) |

⚠️ **VÌ SAO KHÔNG comment `Z_STEPPER_AUTO_ALIGN` trong `Configuration_adv.h`** (cách "sạch" theo kiểu Marlin):
làm vậy sẽ bỏ `z_stepper_align` khỏi `settings_t` ⇒ **đổi kích thước/CRC EEPROM** ⇒ Marlin **nạp lại mặc định**,
mất `M851` (probe Z offset), PID, `M92/M201/M203/M205/M906` và **lưới UBL** đang lưu. Cách ở trên giữ nguyên
config nên **không mất gì**.

⚠️ **Sửa đúng file**: G34 của `Z_STEPPER_AUTO_ALIGN` nằm ở **`G34_M422.cpp`**; `G34.cpp` là bản
`MECHANICAL_GANTRY_CALIBRATION` (máy này không bật, file đó **không được biên dịch** — sửa vào đó thì
`grep` không thấy chuỗi trong `firmware.elf`, đã gặp đúng lỗi này).

**Đã kiểm trên máy thật (nạp lại CẢ Marlin + panel):**
* COM4 gửi `G34` → `echo:G34 da TAT tren may nay (thiet ke truc Z cung) - khong lam gi` + `ok`, **không chuyển động**;
* **giá trị đã hiệu chuẩn còn nguyên** (đây là điểm quan trọng nhất của cách làm này): `M851 X0.00 Y0.00 Z0.75`
  và `M906` X/Y/Z/Z2/Z3/E = 600 mA — EEPROM KHÔNG bị nạp lại mặc định;
* panel: `UI CFG 15` → MOTION **10/10 dòng** (không còn Auto Z-Align), trang CUSTOM chỉ còn `Init SD` +
  `ENDSTOP` + `M600` + `M412` + `REBOOT DFU`, dòng MENU hiện `Custom/Init SD` … `M22`, và **không còn chuỗi
  `G34` nào** trong bố cục mọi trang (`UI BOX`).

*(Chưa đụng tới `M422` — nó chỉ đặt vị trí align cho G34; G34 đã chết nên M422 vô hại. Muốn chặn luôn thì nói.)*

* **Hành động mới** (thêm vào enum `A_*` trong `ui_lvgl_int.h`, xử lý ở `do_action()`): `A_GOTO_MOTION`
  (mở nhóm MOTION), `A_GOTO_MESH` / `A_GOTO_TBT` (mở trang BED MESH / TRAMMING từ trong menu — MENU mở
  bằng chỉ số trang, menu con thì phải qua hành động), `A_PROBE_DEPLOY` (M401), `A_PROBE_STOW` (M402),
  `A_G29_LEVEL` (G29 N), `A_G29_INFO` (G29 W), `A_G29_UBL_MANUAL` (G29 I999 + G29 P2BT0),
  `A_UBL_T0` / `A_UBL_T1` / `A_UBL_S1`, `A_M48` (gửi 2 lệnh bằng `cydclcd_link_send_gcode2()`).
* **Dòng MENU có thể chạy HÀNH ĐỘNG** chứ không chỉ mở trang: `menu_item_t` thêm trường `act`, và
  `page == MENU_ACT_PAGE (0xFF)` ⇒ `cb_menu_row()` gọi `do_action(act)` (trước đây `page` là chỉ số trang).
* **3 mục của 12864 KHÔNG làm** (và vì sao): **`Z Align Manual`** (màn riêng của 12864),
  **`Worst Point = MAX/MIN`** (đã có trong trang TRAMMING) và **`Bed Leveling`** (G29 P1/P3/P4 — đã có
  nguyên trang BED MESH với các nút UBL, thêm nữa là trùng).
* ⚠️ **`Deploy/Stow Z-Probe` giữ nguyên dù máy dùng probe CỐ ĐỊNH** (Voron Tap): `PROBE_DEPLOY_STOW_MENU`
  đang bật nên 12864 cũng hiện 2 mục này; Marlin sẽ trả lỗi khi bấm — không ảnh hưởng gì.
* ⚠️ DRAM: thêm nhóm + bảng mới (const → FLASH) và mảng nhớ vị trí cuộn làm `dram0_0_seg` **vượt 8 byte**
  → đã hạ `CYDLCD_SETTINGS_MAX` 80 → **72** (menu PREHEAT không còn bơm 9 giá trị vào bảng thông số nên
  chỉ dùng 63/72) ⇒ tiết kiệm 128 B (2 bản `s_src` + `cydlcd`), build sạch (RAM 38.0%).

## Menu CONFIGURATION kiểu 12864 (2026-10-15) — `src/ui_page_submenu.cpp`

**Yêu cầu người dùng:** *"phần configuration tôi thấy làm giống 12864 tiện hơn"*. Trước đây dòng `Settings`
trong MENU mở **một bảng phẳng 76 dòng** phải cuộn; nay nó mở **cây menu nhiều cấp** giống menu
`Configuration` của Marlin 12864 (`docs/12864-menu-map.md`). Bảng phẳng **vẫn còn**: mục
`Advanced Settings` → `All settings (table)` (trang `THONG SO` cũ, giữ nguyên `UP`/`DOWN`/`SAVE EEPROM`/`M502`).

```
CAU HINH
├─ Advanced Settings >                ★ LÀ DÒNG ĐẦU TIÊN (yêu cầu người dùng 2026-10-15)
│   ├─ Max Speed >        Max X/Y/Z/E Speed · Min Velocity (M205 S) · Min Travel Speed (M205 T)
│   ├─ Acceleration >     Accel print/retract/travel (M204 P/R/T) · Max X/Y/Z/E Accel (M201)
│   ├─ Jerk >             Max X/Y/Z/E Jerk (M205) · Junction Dev · Linear Adv (M900 K)
│   ├─ Probe Offsets >    Probe X/Y/Z (M851) + dòng "Z Probe Wizard"
│   ├─ Steps/mm >         X/Y/Z/E (M92)
│   ├─ TMC Drivers >      Driver Current > X/Y/Z/Z2/Z3/E0 (M906) · Stepping Mode > StealthChop 6 trục (M569)
│   ├─ Temperature >      MPC P/C/R/A/F/H (M306) + MPC Autotune · Bed PID (M304) + Bed PID Autotune · Hotend PID (M301)
│   ├─ Filament >         E in mm3 + Filament Diameter (M200) · Load/Unload Length (M603) · Runout Sensor (M412)
│   ├─ Tune (runtime) >   Feed % (M220) · Flow % (M221) · Fan % (M106) · Target E0 (M104) · Target bed (M140)
│   ├─ Language >         Tiếng Việt · English  (đổi ngôn ngữ MÀN HÌNH, lưu NVS "ui"/"lang")
│   └─ Home Offset X/Y    (M206)
├─ Probe Z Offset                     M851 Z (mở lớp phủ sửa)
├─ Fade Height                        M420 Z (0.94) — ★ NGAY DƯỚI "Probe Z Offset", xem ghi chú dưới
├─ Controller Fan >                  Idle Speed · Auto Mode · Active Speed · Idle Period      M710 I/A/S/D
├─ Preheat >                         Danh sách vật liệu: mỗi dòng `TÊN  nozzle/bed/fan` (chạm = hộp
│                                    sửa 3-trong-1) + `+ Thêm vật liệu` (bàn phím đặt tên). TỐI ĐA 6
├─ Store Settings (M500) · Load Settings (M501) · Restore Defaults (M502 — xác nhận 2 bước)
└─ All settings (table)              mở lại BẢNG PHẲNG cũ (trang `THONG SO`)
```

### ★ Một chỗ DUY NHẤT cho `Fade Height` (2026-10-15)

**Yêu cầu người dùng:** *"tôi thấy có vài chỗ có config 'fade height' gỡ bỏ và cho vào configuration, ngay
bên dưới z probe offset, ngoài ra, advance setting trong configuration là item đầu tiên"*.

Trước đó dòng này **lặp 3 lần** trong cây (đúng như 12864 — 12864 để nó ở cả `Motion`, `UBL` và
`Advanced`): `SM_I_ADV`, `SM_I_MOTION`, `SM_I_UBL` đều có `SM_V("Fade Height", CYDLCD_SET_FADE_HEIGHT)`.
Nay **gỡ cả 3**, chỉ còn **1 dòng ở `SM_I_ROOT`** nằm ngay sau `Probe Z Offset`; `Advanced Settings` được
đưa lên **đầu bảng** `SM_I_ROOT`. Bảng phẳng `All settings (table)` **vẫn có** dòng `Fade height mm` (nó là
bảng đầy đủ 63 thông số Marlin gửi — không phải chỗ "cấu hình" nên giữ nguyên).

Kiểm trên máy thật (COM8, `UI CFG`): `[0] CAU HINH dong=9/9` → `Advanced Settings`, `Probe Z Offset = 0.750`,
`Fade Height = 0.94`, `Controller Fan`, `Preheat`, `Store`, `Load`, `Restore`, `All settings`;
`[1] ADVANCED dong=11/11` (kết thúc bằng `Home Offset X/Y` — **không còn** Fade Height);
`[15] MOTION dong=9/9` và `[16] UBL dong=11/11` — **cũng không còn** Fade Height.
`UI BOX` khi mở nhóm gốc: 9 nút cao 23 px (0…207), thanh đáy tự ẩn vì nội dung gọn.

**Cách làm (vì sao KHÔNG phải 17 trang):**

* **MỘT trang `PG_SUBMENU` cho cả cây**: `SM_GROUPS`/`SM_ITEMS` trong `ui_page_submenu.cpp` là `const` →
  nằm ở **FLASH**; nhóm đang mở giữ ở `s_sm_g`, đổi nhóm thì `page_rebuild()` dựng lại nội dung trang
  (KHÔNG tạo trang mới). Mỗi trang LVGL tốn ~5–10 KB pool nên 17 trang là hết pool 48 KB.
* Dòng `SM_VAL` **không tự format/gửi giá trị**: nó trỏ vào `SET_ROWS[]` của `ui_page_settings.cpp` **theo
  ID thông số** ⇒ **dùng lại nguyên lớp phủ sửa** (`-`/`+`/4 mức bước/`OK`) và `se_send_mcode()`.
* **Dòng nào không có dữ liệu thì TỰ ẨN** (`settings_has_value`): Marlin chỉ gửi thông số mà bản build có
  (MPC khi `MPCTEMP`, Z2/Z3 khi có driver, StealthChop khi driver hỗ trợ, M710 khi `USE_CONTROLLER_FAN`) ⇒
  cây giống 12864 nhưng không bao giờ hiện dòng rỗng. Máy này: `Jerk 4/6` (ẩn `Junction Dev`/`Linear Adv` vì
  build dùng `CLASSIC_JERK`), `TEMP (ADV) 11/14` (ẩn `Hotend PID` vì hotend chạy `MPCTEMP`).
* **BACK đi theo nhóm CHA** (`submenu_parent()`); đang mở lớp phủ sửa thì BACK **chỉ đóng lớp phủ**; và
  **NGĂN XẾP BACK** (2026-10-15, `s_back[]` trong `ui_lvgl.cpp`) ghi "nơi vừa rời" mỗi lần **đổi trang**:
  `{trang, NHÓM menu con, DÒNG vừa bấm}` (sâu 6, đầy thì bỏ mục cũ nhất). BACK lấy ra và trả về **đúng nơi
  vừa rời** + **tô sáng dòng vừa bấm** — nhờ vậy `MOTION > Move Axis (JOG)` → BACK về **MOTION** (không
  phải MENU) và focus đúng dòng `Move Axis (JOG)`; trang bảng phẳng mở từ trong cây cũng vậy (trước đây
  chỉ có 1 trường hợp đặc biệt cho trang THONG SO). Yêu cầu người dùng: *"chọn Motion, Move Axis (JOG),
  bấm Back lẽ ra phải highlight 'Move Axis (JOG)'"*.
* 🔙 **BACK nhớ CHỖ ĐANG XEM + tô sáng dòng vừa dùng** (2026-10-15 — yêu cầu người dùng: *"đối với menu có
  phân trang, khi bấm back chưa quay về trang trước đó, và cũng chưa highlight item đã trigger open
  sub-menu đó"*): vị trí cuộn lưu **theo từng trang** (`s_scroll_y[]`, `ui_lvgl.cpp`) và **theo từng NHÓM**
  với menu CONFIGURATION (`s_sm_scroll[]` — cả cây dùng chung 1 trang nên không thể lưu theo trang); dòng
  vừa mở được **focus lại** (`page_menu_focus_sel()` cho MENU, `submenu_focus_parent()` cho menu con) nên
  có viền sáng đúng dòng đó. Hai điểm dễ sai: (1) trả lại vị trí cuộn phải làm **SAU `bot_sync(true)`** vì
  `bot_sync(true)` đưa trang về đầu để đo chiều cao; (2) KHÔNG gọi `lv_obj_scroll_to_view()` theo dòng đã
  nhớ (`lv_group_focus_obj()` không tự cuộn) — cuộn theo dòng sẽ **ghi đè** vị trí cuộn vừa trả lại (đã gặp:
  nhóm ADVANCED ở scroll 92 px, quay lui bị kéo về 0). Chẩn đoán: `UI BOX` in thêm `scroll=<px>` +
  `focus=<chỉ số dòng> '<chữ>'`, và `UI NEXT` / `UI PREV` bấm nút phân trang từ console để kiểm tra.

**14 thông số mới** (phải nạp **CẢ HAI** firmware: enum `cydlcd_setting_t` trong 2 bản `cydlcd_protocol.h`
phải khớp y nguyên thứ tự — proto vẫn là 4): `MIN_FEEDRATE` (M205 S) · `MIN_TRAVEL_FEEDRATE` (M205 T) ·
`TMC_CURRENT_Z2/Z3` · `STEALTH_X/Y/Z/Z2/Z3/E` (M569 S0/S1) · `CTRLFAN_IDLE/AUTO/ACTIVE/DURATION` (M710 I/A/S/D).

⚠️ **Z2/Z3 KHÔNG có chữ cái riêng**: Marlin dùng tham số **`I<chỉ số phụ>`** (`M906.cpp`: *"I[index] … 1 for
X2/Y2/Z2; 2 for Z3"*) nên phải gửi **`M906 Z600 I1`** (Z2) / **`I2`** (Z3) và **`M569 S1 Z I1`** (Z2) /
**`I2`** (Z3). Viết `M906 Z2 600` sẽ bị parser đọc thành **`Z` = 2 mA** mà **KHÔNG báo lỗi**.

**DRAM (panel đã full — `dram0_0_seg`):** thêm 14 thông số ⇒ nâng `CYDLCD_SETTINGS_MAX` 72 → **80**
(+8 mục × 8 B × **2 bản** `s_src` + `cydlcd` = +128 B) nên linker báo *overflow 160 byte*; đã bù bằng cách
giảm `CYDLCD_MESH_MAX` 16 → **12** (`mesh[16][16]` = 512 B → `[12][12]` = 288 B, tiết kiệm 448 B; mesh máy này
7×7, vẫn đủ cho 10×10 sau này). `SET_ROWS`/`SM_GROUPS`/`SM_ITEMS` là `const` → **flash**, không tốn DRAM.
(Hàng đợi log `logq` nằm trên **heap** — `xQueueCreate` — nên giảm số dòng ở đó KHÔNG cứu được DRAM tĩnh.)

**Lệnh chẩn đoán mới (COM8):**

| Lệnh | Việc |
|---|---|
| `UI CFG` | in **cả cây**: mỗi nhóm (thụt lề theo nhóm cha) + `dong=hiện/tổng` + nhóm nào RỖNG (dòng trỏ tới nó sẽ bị ẩn) |
| `UI CFG <nhóm>` | **MỞ** nhóm đó (đi đúng `submenu_open()` như bấm tay) rồi in từng dòng + giá trị |
| `UI HIT <k>` | bắn `LV_EVENT_CLICKED` vào **nút thứ k** của trang đang mở → đi đúng đường sự kiện thật; khi lớp phủ sửa đang mở thì tìm trên `lv_layer_top()`: `0..3` = nút bước, `4` = `-`, `5` = `+`, `6` = `OK`, `7` = `Hủy` |
| `UI BACK` | bấm nút BACK ở dòng tiêu đề (đi đúng `do_action(A_BACK)`) |
| `UI LANG` / `UI LANG <0\|1>` / `UI LANG?` | 🆕 **đa ngôn ngữ** (2026-10-15): in ngôn ngữ đang dùng + danh sách · **đổi** ngôn ngữ (ghi NVS luôn, dựng lại màn hình) · in các ID còn **thiếu bản dịch**. Xem mục *"Đa ngôn ngữ (i18n)"* |

⚠️ `UI HIT` chỉ đếm **nút** (`lv_btn`) — đếm mọi vật có cờ `CLICK_FOCUSABLE` thì **nhãn chữ cũng bị đếm** và
chỉ số lệch hết so với dòng đang thấy trên LCD (đã gặp thật).

**Đã kiểm trên máy thật (2026-10-15):** `UI CFG 0` → **9/9 dòng** (2026-10-15: `Advanced Settings` lên đầu,
`Probe Z Offset = 0.750` rồi `Fade Height = 0.94` ngay dưới, `Advanced` còn 11 dòng);
`UI HIT 0` ở gốc (`Advanced Settings`, **trước 2026-10-15 là dòng 5**) → `ADVANCED`, `UI HIT 0` (`Max Speed`)
→ `MAX SPEED`; `UI HIT 3` ở gốc
(`Controller Fan`, trước là dòng 1) → `CTRL FAN` (idle 100 · auto 1 · active 255 · kỳ nghỉ 60 s); `UI CFG 8` → X/Y/Z/**Z2/Z3**/E0
= 600 mA; `UI CFG 9` → 6 × StealthChop = 1; sửa `Min Travel Speed` bằng `+`/`OK` **từ console** ⇒ Marlin nhận
`M205 T0.10` và màn hình đọc lại **0.10** (đã trả về `M205 T0`); `UI HIT 8` (`All settings`, trước là dòng 9) → trang `THONG SO`
(71/76 dòng có giá trị, Marlin gửi 72 thông số) rồi BACK về đúng cây.

## Vì sao màn hình chạy chậm (2026-10-15) — đo bằng `UI PERF`

**Triệu chứng người dùng:** *"LCD chạy cực chậm"*. Cách đo: `UI PERF` (COM8) + dòng log 1 Hz
`[CYD] chang: con=… link=… http=… ui=… us` (đã đổi sang **µs**) + `[CYD] ui cham >=20ms: N lan/s`.

| Đo được (trước khi sửa) | Nghĩa |
|---|---|
| `http=2088 ms` mỗi vòng lặp ⇒ **vòng lặp 0.5 Hz** | giao diện chỉ được cập nhật **2 lần/giây** |
| `http: sd_the 10 425 880 us` trong ~12 s, `sd_alive 0 lan` | **`cyd_sd_task()`**: thẻ TF **không mount được** nên cứ 2,5 s lại `sd_mount()`; mỗi lần thử **3 mức tốc độ** (20/10/4 MHz) ≈ **2 s** ⇒ gần như cả vòng lặp bị chặn |
| log đầy `sdcard_mount(): f_mount failed: (3)` | thẻ không phản hồi (rút ra / tiếp xúc kém) |
| `ui cham >=20ms: 7 lan/s | lan cuoi 68 000 us` | mỗi 150 ms (`UI_REFRESH_MS`) LVGL lại vẽ lại ~nửa màn hình: **~3,3 màn hình/giây đẩy ra SPI** |
| `handleClient 4,5 s / 9 s` | `WebServer::handleClient()` của core ESP32 gọi **`delay(1)`** mỗi lần khi chưa có client (`WebServer.cpp:275-283`) |
| `net_wifi_task 4,5 s / 9 s` | `WiFi.RSSI()` hỏi driver **mỗi vòng lặp** (~300 µs) dù chỉ hiển thị 400 ms/lần |
| `log 1 Hz: 21 ms/lần` | `SD.totalBytes()/usedBytes()` = FATFS `f_getfree` **quét bảng FAT** (4 lần gọi trong 1 biểu thức!) mỗi giây |

**6 chỗ đã sửa:**

1. **Thẻ không mount → thử lại THƯA DẦN** (`cyd_sd.cpp`): 2,5 → 5 → 10 → 20 → **30 s** (trước: mỗi vòng lặp). Thẻ
   mất hẳn chỉ còn tốn ~2 s mỗi 30 s (≈6 %) thay vì 80 % thời gian. Cắm thẻ / bấm `REFRESH` ⇒ thử ngay.
2. **Không ghi màu/ảnh khi giá trị KHÔNG ĐỔI** (`ui_widgets.cpp` + các trang): LVGL **không** tự bỏ qua —
   `lv_obj_set_style_*()` và `lv_img_set_src()` đều gọi `lv_obj_invalidate()` **vô điều kiện** (`lv_img.c:74`).
   Thêm 4 helper `set_text_color()` / `set_border_color()` / `set_bg_color()` / `set_img_src()` (và `icon_color()`
   nay tự so màu) — dùng thay cho lời gọi trực tiếp ở các trang cập nhật nhanh (STATUS/MENU/FEED-FLOW/ENDSTOP).
   ⇒ SPI từ **3 852 %/12 s** xuống **136 %/12 s** (29×), khung vẽ chậm từ **7 lần/s** xuống **0**.
3. **`server.enableDelay(false)`** (`net_upload_http.cpp`): bỏ `delay(1)` mỗi lần `handleClient()` khi rảnh.
   Không ảnh hưởng phiên upload (WebServer vẫn xử lý cả body trong 1 lần gọi).
4. **`net_wifi_task()` đọc trạng thái WiFi mỗi `CYDLCD_WIFI_REFRESH_MS` (500 ms)** (`net_wifi.cpp`): `RSSI()` chỉ
   để vẽ icon + chữ dBm (400 ms/lần) nên không cần đọc mỗi vòng lặp ⇒ 4,5 s → **0,2 s**.
5. **Đọc dung lượng thẻ mỗi `CYDLCD_SD_FREE_MS` (10 s)** thay vì mỗi giây (`cyd_sd.cpp`) — chỉ để in log.
6. **Gọi `server.handleClient()` mỗi `CYDLCD_HTTP_POLL_MS` (10 ms)** (`net_upload_http.cpp`): ~50 µs/lần gọi ×
   5 000 vòng/s là 27 % CPU vô ích.

**Đo lại sau khi sửa** (thẻ TF vẫn chưa mount được — lỗi phần cứng/thẻ, xem dưới):

```
[PERF] ui_lvgl_task: 112 998 vong | tb 7 us, max 17 086 us
[PERF] SPI: 21 lan flush, 65 366 px (85% cua 1 man) | 1 560 us/lan
[PERF] http: wifi 211 029 us | sd_the 2 303 422 us | handleClient 60 963 us (bo qua 112 272 lan)
[CYD] chang: con=0 link=0 http=1 ui=5 us      <- truoc: http=2088 ms, ui=68 000 us
```

⇒ vòng lặp từ **0,5 lần/giây** lên **~11 300 lần/giây** (~0,09 ms/vòng); phần VẼ gần như không còn (LVGL chỉ vẽ khi
có gì đó **thực sự đổi**). Chi phí còn lại: `sd_mount()` thử lại (theo backoff) và log chẩn đoán 1 Hz trên UART.

> ⚠️ **Thẻ TF của màn hình hiện KHÔNG mount được** (`sd=0`, log `sdcard_mount(): f_mount failed: (3) The physical
> drive cannot work`) — cắm lại thẻ hoặc thay thẻ khác rồi bấm `REFRESH`/khởi động lại. Trong lúc chưa có thẻ,
> Marlin cũng không đọc/ghi được thẻ SD (thẻ nằm ở khe màn hình, xem §12) nên `M20`/in từ thẻ sẽ báo lỗi.

## Dai mau nhiet do cua man STATUS (2026-10-15)

Tra loi cau hoi *"khi nhiệt độ tăng dần thì màu của icon hotend và bed có chuyển dần sang đỏ không?"* — **có**,
và từ bản này màu bám **NHIỆT ĐỘ THỰC** chứ không còn chia theo **target**.

* **Trước đây** (`temp_grad()`): màu = `cur / tgt`. Hệ quả: **cùng 50 °C** mà target 250 thì **XANH**, target 60
  thì **ĐỎ** ⇒ màu nói *"đã đạt bao nhiêu % của target"*, không nói *"đang nóng bao nhiêu"*; và target nhỏ
  (bàn in 60) thì đỏ ngay ở 60 °C.
* **Bây giờ** (`temp_ramp()`, `src/ui_page_status.cpp`): 3 mốc **XANH (`MR_COLD`) → VÀNG (`MR_WARM`) → màu NÓNG**,
  chạy theo **nhiệt độ đo được**. Trộn thẳng xanh→đỏ sẽ đi qua màu **xám hồng** (trông như bị *nhạt màu* chứ không
  như *nóng lên*) nên chèn mốc vàng ở giữa cho ra cảm giác nóng dần rõ ràng.
* **Hai đầu dải** (ý người dùng: *"min max hot-end là 10-250; bed 0-60"*): **mốc TRÊN lấy đúng ý** (250 °C đầu in,
  60 °C bàn in). **Mốc DƯỚI kéo lên 25 °C** (`MR_TEMP_AMB`) — **vì sao**: 10 và 0 đều **thấp hơn nhiệt độ phòng**
  (~29–31 °C) nên không bao giờ đạt được, và nặng hơn là bàn in **nguội 28,8 °C** với dải `0-60` đã ở **48 %** ⇒
  màu **VÀNG** dù heater **TẮT**, nhìn như đang nóng. Lấy 25 °C làm mốc đầu thì bàn in nguội ≈ 11 % ⇒ **vẫn XANH**.
* **Màu KHÔNG còn phụ thuộc "có target hay không"**: icon ĐẶC/VIỀN đã nói *có đang cấp nguồn hay không*, nên màu
  để nói *đang nóng bao nhiêu* ⇒ lúc heater tắt mà còn 180 °C thì icon là **VIỀN nhưng màu NÓNG** (đúng thực tế).
* Màu này dùng cho **cả icon lẫn 2 con số** trong ô. Đầu nóng: `MR_HOTEND #FF8A80` (đỏ nhạt, tương phản 6,7:1 —
  Marlin TFT dùng `COLOR_SCARLET #FF2020` nhưng trên nền xanh đậm chỉ ~4,4:1, chữ nhỏ khó đọc); bàn in:
  `MR_BEDHOT #FFB74D` (cam).
* ⚠️ **Đơn vị**: `temp_lookup()` trả về **°C×100** (vd `3102` = 31,02 °C) còn `temp_ramp()` tính bằng **°C** ⇒
  **phải chia 100** trước khi gọi. Bản đầu quên chia nên `3102 > 250` ⇒ đầu in ở nhiệt độ phòng hiện **ĐỎ ngay**
  (đã sửa; `UI GRAD` in ra màu *đang hiển thị* nên bắt được lỗi này).
* **Kiểm tra không cần đốt nóng**: `UI GRAD` in bảng màu của cả dải + màu **đang hiển thị** thật (xem bảng lệnh
  console). Bảng in ra dùng **đúng hàm và hằng số** của màn STATUS nên luôn khớp với cái đang vẽ.

## Font chu (cap nhat: chi con font TU SINH)
UI co **2 che do** font, chon bang `UI_MONO_FONT` trong `src/ui_lvgl.cpp` (co the ep tu ngoai:
`-DUI_MONO_FONT=0` de thu che do 0 ma khong phai sua file):
- **`UI_MONO_FONT 4` (MAC DINH)**: font **CHU TU SINH** `gfx4_font_text` (4 bpp) qua driver
  `lv_font_gfxfont`; gan TUNG nhan/nut (`mono_obj`), ten man (`s_title`) giu Montserrat.
  **CO DU DAU tieng Viet** (U+1EA0..U+1EF9 + Ă ă Đ đ Ơ ơ Ư ư). Sinh lai bang `tools/ttf2gfxfont.py`.
  * 🆕 **2026-10-15 — ĐỔI FONT 3 LẦN (theo yêu cầu người dùng)**: lần 1 thử **BlinkMacSystemFont size 17**
    (*"thử thay font này xem có đẹp hơn không?"*) → *"font này xấu quá"*; lần 2 thử **Segoe UI size 17**
    → *"chữ hơi to quá, chỉnh cùng cỡ với Roboto Mono 15 được không?"*; lần 3 (ĐANG DÙNG):
    **Segoe UI Regular size 16** (`C:/Windows/Fonts/segoeui.ttf`), font **TỈ LỆ**, **yAdvance 20**,
    base_line 4, ascender 16, box_h max 18, 301 glyph có nét, 14 746 byte bitmap
    (file `src/gfx4_font_text.h` + `.c`; tên cũ `gfx4_font_robotomono` — Roboto Mono 15 mono — **đã xoá**).
    **Nét chữ cao đúng 11 px = BẰNG Roboto Mono 15 bản đã chốt trước đó** (`'A' 10x11`, `'X' 9x11`,
    `'0' 8x11`) và **dòng 20 px** — nên bố cục quay về đúng các con số cũ (xem mục chiều cao bên dưới).
    Vì sao đổi được sang font tỉ lệ: driver đặt `dsc->adv_w = g->xAdvance` cho **từng glyph**, nên chỉ cần
    tool sinh đúng xAdvance từng glyph ⇒ đã thêm tuỳ chọn **`--proportional`** cho `tools/ttf2gfxfont.py`
    (trước đây tool ép 1 bước chung ⇒ chỉ dùng được cho font mono). **Font ICON vẫn phải mono** (`--advance`).
    Kiểm chứng trên máy (log khởi động): `line_height=20 base_line=4 (ascender thuc=16)`,
    `'A' box=10x11 adv_w=10`, `'0' 8x11 adv_w=9`, `'a' 7x8 adv_w=8`, `U+1EC7 (ệ) 8x15`
    ⇒ **adv_w khác nhau = tỉ lệ đúng**, dấu tiếng Việt vẫn có nét. 33 codepoint "thiếu" của Segoe UI đều là
    ký tự điều khiển U+007F..U+009F (không bao giờ dùng).
  * 🆕 **Icon tách khỏi chữ**: dòng menu có menu con **KHÔNG còn dùng ký tự `>`** (yêu cầu người dùng:
    *"việc hiển thị menu có submenu với '>' quá xấu, hãy vẽ icon font đẹp chút"* + *"đối với các icon
    không phải chữ thì tách ra chỗ khác"*) — nay dùng **font ICON riêng** `gfx4_font_icons` với glyph
    Material Icons: `U+E008` chevron_right (dòng menu con), `U+E009/E00A/E00B` = trái/lên/xuống (4 nút
    điều hướng màn BED MESH). Đã mở rộng font icon bằng `--first 0xE000 --last 0xE00B` + 4 `--map`
    (`0xE008=0xE5CC`, `0xE009=0xE5CB`, `0xE00A=0xE5CE`, `0xE00B=0xE5CF`) và **chứng minh 8 glyph cũ
    không đổi một byte** bằng `tools/cmp_font_headers.py` (KẾT LUẬN: 8 GLYPH CU KHONG DOI).
    `UI BOX` in ký tự vùng PUA thành `<icon E008>` cho dễ đọc.
- **`UI_MONO_FONT 0`**: chi Montserrat 14 mac dinh cua LVGL (du phong). **KHONG HIEN DUOC DAU tieng
  Viet** -> `vn_font()`/`vn_font_btn()` thanh no-op, 3 nhan co dau o lop phu SUA ("Gia tri" / "Buoc" /
  "Huy") se ra **O VUONG**. Muon co dau thi PHAI dung che do 4.
- Font ICON (WiFi / the nho / **chevron menu**) dung rieng `gfx4_font_icons` (ban nho 14 px — icon WiFi o
  header + 4 chevron) va `gfx4_font_icons_big` (ban to 30 px — icon WiFi o man STATUS, to bang icon FR/FL
  32x32), cung sinh bang `tools/ttf2gfxfont.py` tu **Material Icons**.
- Cac font sinh bang `lv_font_conv` **DA XOA khoi repo** (che do 1 va 2 gan o cap man hinh / tung
  vat deu da thu tren may that va **MAT HET CHU**; che do 3 chua lam) — dung lai la vo ich.
- ⚠️ **Chiều cao dòng của font chữ: 20 px** (Segoe UI 16). Lịch sử: 17 → 20 (Roboto Mono 15, đã chốt)
  → 24 (thử Blink 17) → 22 (thử Segoe 17) → **20 (Segoe UI 16, đang dùng)**. Vì đã nới cho 24 px khi thử
  nên 20 px dư nhiều — nhưng **đã trả lại đúng các con số đã chốt** cho gọn: `MENU_ROW_H` **25** (nội dung
  23), `FILES_ROW_H` **21** + `FILES_ROWS` **4** (FILE SD 196 px), `ES_ROW_H` **23** (ENDSTOP 198 px, KHÔNG
  phân trang nữa), `SET_TABLE_H` **122** (THONG SO 206 px), `OV_LINE_H` **22** + `OV_H` **118**,
  `TP_ROW_H` **30** + COOLDOWN **26** (NHIỆT ĐỘ 204 px). (Màn TUNING MPC đã bỏ 2026-10-15 nên không còn
  phải canh `pad_row` cho nó.)
  Đo trên máy: STATUS · CAU HINH · NHIỆT ĐỘ · FILE SD · THONG SO · ENDSTOP = **không tràn**;
  MENU = tràn (có thanh đáy PREV/NEXT) ✓ đúng như thiết kế.
- Bang (`THONG SO`, luoi BED MESH) va `roller` man WIFI dung **font MAC DINH cua LVGL (montserrat_14,
  line_height 16)** chu KHONG phai font chu (`mono_obj` chi duoc goi trong `mk_label`/`mk_btn`) → khi doi
  font chu thi cac bang nay KHONG doi, nen khong bi tran cot.
- ✅ **Đã kiểm không chồng lấn với font tỉ lệ**: quét `UI BOX` trên 9 màn (STATUS · MENU · CAU HINH ·
  MOTION · NHIET DO · FILE SD · THONG SO · INFO · CMD?) rồi so `x+w` của nhãn tên với `x` của nhãn giá trị
  trong cùng một dòng — **không dòng nào chồng lấn** (chỉ còn 2 cảnh báo GIẢ trên STATUS: các nhãn nhiệt
  độ nằm cùng ô 64 px nhưng KHÁC `y` = target/current xếp trên dưới).

## EEPROM tren trang SETTINGS (doi 2026-10-13)
- **Da BO nut `M501 LOAD`** theo yeu cau: M501 chi DOC LAI toan bo thong so tu chip EEPROM I2C
  vao RAM (tuc la HOAN TAC cac thay doi chua luu ke tu lan SAVE gan nhat) - no khong "load" tu
  file nao ca, nen nhan "LOAD" gay hieu nham. Neu can lai chuc nang hoan tac thi them 1 nut voi
  nhan ro nghia, vi du `HOAN TAC (M501)`.
- `M502 RESET (ve mac dinh)` chiem ca hang, **XAC NHAN 2 BUOC** (giong REBOOT DFU): bam OK lan 1
  doi text thanh "Chac chan? Bam OK lan nua", bam OK lan 2 moi gui `M502` + xin lai bang thong so.
  Ly do phai 2 buoc: M502 tra thong so TRONG RAM ve mac dinh cua firmware; neu sau do bam
  `SAVE EEPROM` (M500) thi mac dinh bi ghi VINH VIEN len chip EEPROM.
- Nho thu tu dung: `M502` (mac dinh) -> chinh lai -> `SAVE EEPROM` (M500).

---

## Cai dat / nap / kiem tra nhanh

### Build + nap

```powershell
# tu thu muc goc repo
pio run -d cydlcd -e cydlcd_esp32                               # build
pio run -d cydlcd -e cydlcd_esp32 -t upload --upload-port COM7   # nap

# hoac dung trong cydlcd:
cd cydlcd
pio run -e cydlcd_esp32 -t upload --upload-port COM7
```

* **PHAI chi ro `-e cydlcd_esp32`**: `default_envs = cydlcd_f411cc` (`platformio.ini:32`) nen
  `pio run -d cydlcd` **khong co `-e`** se build cho **F411 (STM32)**, khong phai CYD.
* Env CYD **khong khai `upload_port`** (`platformio.ini:123-149`) ⇒ phai truyen **`--upload-port COM7`**
  tren dong lenh; `monitor_speed = 115200` (`platformio.ini:148`).
* Env CYD **can mang lan dau** (keo `espressif32`, `lvgl`, `TFT_eSPI` — `platformio.ini:121, 144-145`).
* `CYDLCD_PROTO_VERSION` hien la **2** (`src/cydlcd_protocol.h:42`). Doi version thi **phai nap CA HAI
  board** (Marlin + màn hình CYD), khong thi màn hình CYD bao `LINK ERR` du byte van chay (cam bay 41 o README goc).

### Khi `-t upload` khong vao duoc bootloader → nap bang esptool truc tiep

CYD dung USB-UART **CH340** (noi vao UART0 cua ESP32) — cung chinh la **COM7** — nen co the go thang
bang esptool de kiem tra / nap:

```powershell
python "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
       --port COM7 --before default_reset --after hard_reset chip_id
```

* **Repo KHONG co thu muc `tool-esptoolpy`** — `esptool.py` nam trong **goi PlatformIO**, va duong dan
  tren dung la duong dan ma `tools/fix_platformio_acl.ps1:152` kiem tra
  (`%USERPROFILE%\.platformio\packages\tool-esptoolpy\esptool.py`). **Da kiem tren may nay: file ton tai.**
* `chip_id` chi de **kiem tra noi duoc voi chip** (in ra loai chip + MAC — dung de biet **COM7 co
  thuc su noi vao ESP32** khong). Muon nap tay thi thay bang `write_flash`; **file bin nam o
  `cydlcd\.pio\build\cydlcd_esp32\firmware.bin`**
  (da kiem: co that trong build dir; `bootloader.bin` + `partitions.bin` nam cung thu muc).
* Neu esptool bao `PermissionError: [Errno 13] ... _contrib\intelhex\__init__.py` thi do **ACL hong**
  trong `~/.platformio`, **khong phai** loi firmware ⇒ chay `tools/fix_platformio_acl.ps1` bang
  PowerShell **Administrator** — xem **§8**.

### Doc log chan doan 1 Hz tren COM7

Log nam tren **Serial cua ESP32 (USB-UART = COM7 @ 115200)** va **chi in khi env co
`-DCYDLCD_CYD_DEBUG=1`** — hien dang **BAT** (`platformio.ini:133`; **nho bo co khi dung that**):

```
[CYD] loop OK | t=<ms> | link PUSH=<0/1> TBT=<0/1> rx=<so frame> err=<so loi> heap=<byte>
             | TBT dong=<so dong hop le> loi=<so dong loi> byte=<TONG byte nhan> cach-day=<ms>
```

(`src/main.cpp`, in **1 lan/giay**. Luc boot con in **reset reason** + free heap + **vong lap truoc
dung o chang nao / bao lau**; moi giay con in them dong `[CYD] chang: ...` (thoi gian tung chang cua
vong lap truoc) va `[CYD] lvmem: used/tong B` (pool LVGL) — xem muc chan doan treo.)

Doc bang:

```powershell
pio device monitor -p COM7 -b 115200
```

| Truong | Lay tu | Doc the nao |
|---|---|---|
| `link PUSH=` | `cydlcd.linked` | **1** = da nhan duoc frame PUSH hop le (che do `CYDLCD_SERIAL_UI`) |
| `link TRAM=` | `tram.linked` | **1** = dang nhan duoc dong `TBT` (che do `CYDLCD_TBT_LINK`) |
| `rx=` | `cydlcd.rx_frames` | so **frame PUSH hop le** da nhan — phai **tang deu** khi Marlin dang chay |
| `err=` | `cydlcd.rx_errors` | so frame **loi** — **khong duoc tang** (tang = sai baud / nhieu day) |
| `heap=` | `ESP.getFreeHeap()` | RAM con lai — phai **on dinh**, khong tut dan qua thoi gian |

> ⚠️ O **che do TRAM** thi `rx`/`err` (dem frame PUSH) **co the dung o 0** vi Marlin chi gui dong
> ASCII `TBT ...` chu khong gui frame PUSH. Luc do xem `link TBT=1` va doc **man INFO** tren màn hình CYD:
> `LINK: OK (TRAM) ...` + `RX: <n> frame / <n> loi` (`src/ui_lvgl.cpp:2013-2018`).

### Console gui lenh qua COM7 (giong het bam nut tren màn hình CYD)

```powershell
pwsh -File D:\Marlin\serial-console.ps1 -Port COM7 -Baud 115200    # qua màn hình CYD (mac dinh)
pwsh -File D:\Marlin\serial-console.ps1 -Port COM4 -Baud 250000    # gui thang vao Marlin qua USB
```

`serial-console.ps1` luon gui **CRLF** va bat **DTR** (`serial-console.ps1:23, 42-43`); moi dong go
vao duoc màn hình CYD **chuyen tiep sang Marlin** bang dung duong ma cac nut tren UI dung
(`src/main.cpp:62-68, 73-88`). Go `exit` + Enter de thoat.

**Vai lenh RIENG cua màn hình CYD** (KHONG gui sang Marlin — màn hình CYD tu xu ly va tra loi ngay tren COM7):

| Lenh | Y nghia |
|---|---|
| `WIFI?` | in trang thai WiFi + IP + RSSI + ssid da luu + heap |
| `WIFI SCAN` / `WIFI ASYNC 0\|1\|2` | quet mang (khong chan, van bom link) / che do chan doan |
| `WIFI CONNECT <ssid> <pass> [S]` · `WIFI OFF` · `WIFI FORGET` | ket noi (S = luu NVS) · tat · xoa cau hinh |
| 🆕 `PRE?` | in **danh sách vật liệu preheat** (tên + nozzle/bed/fan) — xem **§9.22** |
| 🆕 `PRE ADD <tên>` / `PRE DEL <i>` / `PRE NAME <i> <tên>` | thêm / xóa / đổi tên vật liệu preheat (tối đa 6) — **§9.22** |
| 🆕 `PRE <i 0..2> <nozzle> <bed> [fan]` | dat + luu NVS preset (`0=PLA`, `1=PETG`, `2=ABS`); **khong gia nhiet** |
| 🆕 `UI ?` (hoac `UI`) | in danh sach 13 man + so thu tu (khong mo man nao) |
| 🆕 `UI <ten man>` | **mo** 1 man theo ten, vd `UI FILE SD`, `UI TRAMMING` — de tai hien loi UI tu PC |
| 🆕 `UI BOX` | in **bo cuc** man dang mo + **lop tren (hop thoai)**: toa do/kich thuoc tung vat **theo cay** (de quy 2 muc, thay ca nhan nam trong khung/nut), noi dung `lv_roller` (danh sach file), trang thai thanh day. Man hinh CYD **khong co framebuffer doc nguoc** (driver ve thang ra SPI) nen day la cach **duy nhat** kiem tra bo cuc tu PC — vd kiem tra nut co nam sat day khong, roller cao bao nhieu px, danh sach file co muc la khong, **hop thoai co o giua man hinh khong**. 🆕 Khi dang o man **FILE SD** in them **dong dang chon** (chi so + ten + dong nao dang to nen), va in them **hop nao dang mo** (PROMPT cua Marlin / XAC NHAN / THONG BAO / PID + chu cua no) |
| 🆕 `UI TESTBOX` | mo 1 **hop thoai XAC NHAN rong** (tu dong dong sau 6 s, bam OK/CANCEL khong lam gi) de kiem tra bo cuc hop thoai: doc `UI BOX` phan "lop tren" — hop PHAI co tam = tam man hinh (vd `x=60 y=82 200x76` ⇒ tam 160,120 = giua 320×240) |
| 🆕 `UI BUSY` | in **máy đang làm gì** + **có cho nhận file upload không**: `[UI] MAY: st=<mã> (<tên>) fl=0x.. file="<file đang in>" prog=<%> | IN TU THE SD=0/1 TAM DUNG=0/1` rồi `[UI] upload: cho nhan file \| BI CHAN (...)`. Trả lời *"làm sao biết máy đang in?"* — upload **chỉ** bị chặn khi đang in / tạm dừng bản in **từ thẻ SD** (HOME/PROBE/G35 không tính). Tự in kèm trong `UI BOX` |
| 🆕 `UI TEMPS` | in **cả 2 nguồn nhiệt độ** mà màn hình đang có để đối chiếu: **PUSH** = frame `CYDLCD_F_TEMPS` (`id=1` đầu in, `id=0` bàn in; `cur/tgt` đơn vị °C×100) và **TRAM** = 4 token nhiệt độ cuối dòng `TBT ...`, kèm dòng "đang HIỂN THỊ" = đúng giá trị `temp_lookup()` dùng để vẽ. Dùng khi *"nhiệt hiển thị không đúng"* (đã dùng để tìm ra lỗi target bàn in luôn 0). Tự in kèm trong `UI BOX` |
| 🆕 `UI GRAD` | in **bảng màu của dải nhiệt độ** mà màn STATUS dùng cho **cả icon lẫn 2 con số**: dòng đầu in 2 đầu dải (`hotend 25..250`, `bed 25..60`, mốc giữa `MR_WARM`), dòng 2 in **màu ĐANG HIỂN THỊ** thật (kèm °C của cả đầu in và bàn in), rồi 2 dòng quét `25C(0%)=#00E6FF … 250C(100%)=#FF8983` / `30C(14%)=#41DEC5 … 60C(100%)=#FFB64A` (kèm **% vị trí trong dải**). Trả lời *"nhiệt độ tăng dần thì màu có chuyển dần sang đỏ không?"* **mà KHÔNG phải đốt nóng máy**, và bắt được lỗi **sai đơn vị** (°C so với °C×100) — xem mục **"Dai mau nhiet do cua man STATUS"** |
| 🆕 `UI PERF` | **đo hiệu năng màn hình**, gọi 2 lần cách nhau N giây (lần 1 xoá bộ đếm, lần 2 đọc số của khoảng đó): `ui_lvgl_task` tb/max µs, `lv_timer_handler` tb/max, **SPI** (số lần flush + TỔNG SỐ PIXEL đã đẩy, % của 1 màn = 76 800 px), **cảm ứng** (µs/lần), `http` (wifi / thẻ TF / `handleClient` + số lần bỏ qua), `sd_task` (`sd_alive` / `clean_dir`), log 1 Hz, `lvmem` + heap. Dùng để biết **màn hình chậm là do VẼ, do SPI, do cảm ứng hay do vòng lặp bị chặn ở chỗ khác** — xem mục **"Vì sao màn hình chạy chậm"** |
| 🆕 `UI RX` | in **2 CHIỀU của UART** để biết bên nào hỏng: `PONG co/CHUA BAO GIO` + **tuổi PONG** (bình thường < 1500 ms) + **số lần mất PONG**, rồi bảng **số frame đã nhận theo LOẠI** (`HELLO/STATUS/TEMPS/POS/MESH/FILES/SETTING/PONG…`), và vị trí + FR/FL + `n_settings`/cỡ lưới. Dùng khi *"bấm nút trên màn hình mà máy không chạy"*: **PONG = 0 / tuổi lớn ⇒ chiều MÀN HÌNH → MARLIN hỏng** (trong khi `STATUS/TEMPS` vẫn tăng = chiều xuống vẫn tốt) — đối chiếu với `PANEL DIAG` in ở COM4. Xem mục **"Chan doan link 2 chieu"** |
| 🆕 `UI PRE` | in **danh sách vật liệu preheat** (số thứ tự + tên + nozzle/bed/fan) + trạng thái 2 lớp phủ preheat |
| 🆕 `UI PRE ADD <tên>` · `DEL <i>` · `SET <i> <h> <b> <f>` · `NAME <i> <tên>` | **thêm / xóa / sửa / đổi tên** vật liệu preheat (NVS `ph2`, tối đa 6) — đường **duy nhất** để làm việc này từ PC, vì hộp sửa + bàn phím đặt tên nằm trên LCD |
| 🆕 `UI PRE OPEN <i>` · `UI PRE KB` | **mở hộp sửa 3-trong-1** của vật liệu thứ i / **mở bàn phím** đặt tên (đi đúng callback thật) — phối với `UI HIT` để bấm `-`/`+`/`OK`/`Hủy`/`Xóa` (chỉ số khi lớp phủ này mở: `0..2` = ô NOZ/BED/FAN, `3..5` = mức bước, `6` = `-`, `7` = `+`, `8` = `OK`, `9` = `Hủy`, `10` = `Xóa`) |
| 🆕 `UI PRE <k>` | **bấm nút preheat thứ k** của màn NHIỆT ĐỘ (đi đúng đường sự kiện thật) — log ra `[PRE] bam <tên>: gui "M104 S<n>" + "M140 S<b>" (hàng đợi TX chờ=…, đã bỏ=…)` để đối chiếu màn hình gửi gì với Marlin nhận gì. Phải mở `UI NHIET DO` trước |
| 🆕 `UI TAP <0..4>` · `P\|N` · `I` · `D` · `K` · `C` · `L [đường dẫn]` | **ban su kien CHAM that** de kiem tra UI tu PC (màn hình CYD không có framebuffer đọc ngược): `<0..4>` = dòng thứ mấy **của TRANG đang xem** ở màn FILE SD (mở `UI FILE SD` trước), `P`/`N` = **PREV/NEXT**, `I` = **PRINT** (kiểm tra đường in: có gửi `M23`+`M24` không), `D` = **XOA** (hộp xác nhận nếu mở thì tự đóng sau 8 s), `C` = **đếm mục con** của thư mục đang chọn (**không xoá gì**), `K` = **bấm OK** hộp thoại đang mở (**mọi màn**; hộp 2 bước phải `K` **2 lần** mới chạy lệnh thật), `L` = **liệt kê thư mục bằng FATFS của chính màn hình** (tên ĐẦY ĐỦ, không bị SdFat cắt 27 ký tự; `UI TAP L /BTUP` để xem 1 thư mục). Đều đi **đúng đường sự kiện thật** (`lv_event_send` → `cb_files_row`/`cb_files_print`/`cb_prompt_value`/…) nên kiểm được: chạm dòng nào thì **dòng đó** được chọn + tô sáng, `XOA`/`PRINT` đúng dòng, prompt của Marlin được trả lời (`M0` → `K` → `st=1`→`st=0`), xoá file/thư mục rỗng có chạy không. `UI TAP` (không tham số) = in trạng thái danh sách (kèm `PRINT=`/`XOA=` khoá hay mở) |

⚠️ Do **thu tu so khop tien to** trong `main.cpp`, mot lenh la (vd `PRINT`) se bi coi la G-code va gui sang
Marlin; cac lenh màn hình CYD o bang tren deu duoc so **truoc** nhanh chuyen tiep nen khong bi lan.

### Hieu chinh cam ung (chi board co touch — CYD)

* **Vao:** STATUS → **SETTINGS** → **MENU** → dong **`Calibrate touch (giu 1.5s)`** — **PHAI GIU
  TAY 1,5 giay roi NHAC** (`CAL_TOUCH_HOLD_MS 1500`, `src/ui_lvgl.cpp:2176`; trong luc giu cot phai
  doi `dang giu` → `NHAC TAY` — `:2288-2289`).
* **Duong thoat hiem:** **giu tay tren man hinh luc cap dien** — màn hình CYD thay `T_IRQ` muc thap trong
  `ui_port_input_init()` va vao thang che do hieu chuan (dung khi gia tri cu qua te, khong cham trung
  duoc dong menu).
* **Cach lam:** lan luot **TREN-TRAI → TREN-PHAI → DUOI-PHAI → DUOI-TRAI**; moi goc: cham → **ca man
  hinh doi sang XANH** → **keo ngon tay sat goc roi NHAC TAY** (luc **mat cam ung** moi chot toa do,
  lay cuc tri min/max). Chi tiet + cho luu (NVS namespace `cydtouch`) o **§5.5**.

### DỪNG IN (nút STOP) và vì sao lệnh "critical" phải đi đường riêng (2026-10-15)

**Hiện tượng đã gặp thật:** đang in mà bấm `STOP` thì **không dừng được**, bấm vài lần thì **màn hình CYD tự
khởi động lại**, cuối cùng phải reset board Marlin.

**Chuỗi nhân quả (đo được từ log COM7/COM4):**
1. File in có `M190 S70` (chờ bàn nóng) trong start G-code. Nếu bàn **không nóng được** (PSU tắt, lỗi nhiệt),
   `thermalManager.wait_for_bed()` chờ **mãi mãi** — và trong lúc đó `loop()` của Marlin **không hề chạy**.
2. Marlin vẫn đọc tiếp file và **nhồi đầy hàng đợi lệnh**; mọi lệnh gửi tới bị Marlin trả `busy: processing`
   (`GcodeSuite::host_keepalive()`, `gcode.cpp:1212`).
3. Chỗ **từ chối thật sự** là `queue.enqueue_one_now(cmd)` = `while (!enqueue_one(cmd)) idle();`
   (`gcode/queue.cpp`): hàng đợi không bao giờ trống ⇒ Marlin **block vĩnh viễn ngay trong hàm nhận frame** ⇒
   **ngừng gửi frame trạng thái** (panel mất link, hiện trạng thái CŨ) và **STOP không tới được**.
4. Panel gửi `M524` khi Marlin không đọc UART ⇒ hàm ghi của panel **chờ mãi** ⇒ vòng lặp panel đứng ⇒
   **watchdog chẩn đoán restart panel** — đúng cái người dùng thấy là "hủy là LCD restart".

**Cách sửa (2 phía):**
* **Marlin** (`lcd/extui/cydlcd_serial/cydlcd_serial.cpp`, nhánh `CYDLCD_F_GCODE`): **lệnh critical chạy NGAY,
  không qua hàng đợi** — `M108` (thoát vòng chờ nóng: `wait_for_heatup = false`), `M524` (hủy in: đặt cờ abort
  + `queue.clear()`), `M410` (quickstop). Lệnh **thường** thì chờ hàng đợi nhưng **CÓ TRẦN 2 s** rồi bỏ + đếm
  lại (`s_cmd_drop`) — **không bao giờ block vô hạn**.
* **`gcode/sd/M524.cpp`** (bản sửa theo yêu cầu): M524 nay **ngừng feed nội dung file** (`IS_SD_PRINTING()` =
  false) và **xoá hàng đợi** ⇒ coi như file đã đọc xong, các lệnh đã nạp từ file không chạy tiếp; đồng thời
  `wait_for_heatup = false` để thoát vòng chờ nóng.
* 🆕 **TẮT GIA NHIỆT NGAY, nhưng KHÔNG được `quickstop_stepper()` trong hàm nhận frame** (2026-10-15:
  người dùng báo *"hủy in xong vẫn thấy hotend đang nóng"* rồi *"ngừng in v300-case-esp thì Marlin bị
  restart"*):
  * `thermalManager.disable_all_heaters()` **được** gọi ngay trong nhánh critical của `cydlcd_serial.cpp`
    (chỉ ghi lại target = 0, **không chặn**) ⇒ nhiệt độ mục tiêu về 0 ngay, không đợi `loop()`
    (trước đây `abortSDPrinting()` — nơi có tắt gia nhiệt — chỉ chạy ở vòng `loop()` nên bị TRỄ).
  * `quickstop_stepper()` **BỊ BỎ KHỎI** nhánh đó: hàm này gọi `planner.synchronize()` = **CHỜ** hết
    chuyển động đang chạy ⇒ trong hàm nhận frame (chạy trong `idle()`) sẽ **khoá Marlin giữa đường**
    (không đọc UART, không xử lý lệnh) và bị gọi **LẶP lại** vì nút STOP gửi `M108`+`M524` mỗi 400 ms
    ⇒ treo/`watchdog` reset = **"Marlin bị restart"**. Việc dừng chuyển động vẫn xảy ra, nhưng ở **đúng
    chỗ**: `abortSDPrinting()` (`MarlinCore.cpp:363-372`) gọi `quickstop_stepper()` trong vòng `loop()`
    ngay sau đó (vì cờ abort đã được đặt).
  * `gcode/sd/M524.cpp` **vẫn** có `quickstop_stepper()` — đường đó chạy **từ hàng đợi** (host trên
    COM4 gửi `M524`), tức là trong ngữ cảnh `loop()` bình thường, không phải trong hàm nhận frame.
  * **Đã kiểm trên máy thật (COM7, ĐÚNG thao tác người dùng)**: `UI FILE SD` → chọn
    `V300_case-esp-2432s208_no-` (36 ký tự) → `PRINT` → `st=1`, `fl=0xA7` (bit `SD_PRINTING`) →
    **STOP + OK** → `st=1 → 3 → 0`, `fl=0xA7 → 0xA3`, `file=""`, trang NHIỆT ĐỘ hiện
    `NOZZLE 33.7 / 0 C`, `BED 31.9 / 0 C` (target về 0), **KHÔNG có dòng `MARLIN VUA KHOI DONG LAI`**
    ⇒ Marlin không restart.
* 🆕 **Phát hiện Marlin restart từ phía màn hình**: mỗi khung `CYDLCD_F_HELLO` được đếm; từ lần thứ 2
  trở đi màn hình ghi `*** MARLIN VUA KHOI DONG LAI (HELLO lan N) - truoc do link=..., ly do reset
  khong ro ***` (`cydlcd_link.cpp`). Đây là **dấu hiệu duy nhất** phía màn hình để biết Marlin vừa
  reset (Marlin không gửi lý do reset qua kênh này) — dùng để chẩn đoán các lỗi kiểu "bấm STOP thì
  Marlin restart". ⚠️ **Mở COM4 bằng `marlin-cmd.ps1` (DTR=true) sẽ RESET board** — chỉ mở khi máy đang
  rảnh, đừng mở trong lúc đang in (làm mất trạng thái bản in).
* **Panel** (`cydlcd_link.cpp`): ghi ra Marlin **không bao giờ chặn cứng** — frame vào **hàng đợi TX 4 khe**
  và trải dần (`cydlcd_link_tx_task()`), chỉ **frame dữ liệu thẻ SD / SDACK** mới chờ (có trần 250 ms) vì mất 1
  frame là hỏng cả sector. `ui_lvgl.cpp`: nút STOP gửi `M108` + `M524` và **gửi lại mỗi 400 ms tối đa 6 s**
  cho tới khi Marlin thôi trạng thái in.

**Đã kiểm trên máy thật (qua console COM7 = đúng đường của nút bấm):**
`M21` → mount OK; `M23 BODY10~1.GCO` + `M24` → `st=1` (đang in, `SD r=15`); `M108` + `M524` → **`st=1` →
`st=0 IDLE` trong ~4 s, `SD r` ĐỨNG YÊN (đã ngừng feed file), nhiệt độ target về 0, Marlin trả lời `M27`
"Not SD printing" / `M114` bình thường, màn hình KHÔNG restart, link `PUSH=1` vẫn sống.**

**Và đã kiểm ĐÚNG trường hợp "hàng đợi Marlin ĐẦY, không nhận thêm data" (2026-10-15):**
dùng `cydlcd/tools/test_stop_full_queue.ps1` — bằng chứng đo được, hàng đợi đầy vì bản in đứng ở `M190`
(PSU tắt) nên **`SD r` ĐỨNG YÊN giữa file 370 KB** (Marlin hết chỗ nên ngừng nạp lệnh từ file):

| Trong lúc hàng đợi ĐẦY | Kết quả đo |
|---|---|
| Lệnh **thường** từ panel (`M220 S42`) | **BỊ BỎ** sau trần 2 s → mở trang FEED/FLOW thấy nhãn vẫn `100%` ✓ |
| Lệnh **critical** (`M108` + `M524`, đúng đường nút STOP) | **CHẠY NGAY**: `st=1` → `st=3` → **`st=0`** ✓ |
| Sau khi hủy, lệnh thường lại | **chạy bình thường**: nhãn FR trên STATUS = **`42%`** ✓ (hàng đợi đã trống) |
| Màn hình / link | **không restart**, `link PUSH=1` suốt quá trình ✓ |

⚠️ Lưu ý: nếu bàn **không nóng được** (PSU tắt/lỗi) thì bản in sẽ đứng ở `M190` — đây là hành vi đúng của
Marlin (chờ đủ nhiệt mới in); muốn thoát thì bấm `STOP` (đã sửa) hoặc gửi `M108`.

### Bảng "kiem tra nhanh sau khi nap"

| # | Kiem tra | Cach xem | Dat |
|---|---|---|---|
| 1 | Link PUSH | log 1 Hz tren COM7 | `link PUSH=1` |
| 2 | Link TRAM (neu dung kieu TRAM) | log 1 Hz + man INFO | `link TBT=1` / `LINK: OK (TRAM)` |
| 3 | Nhan frame | `rx=` trong log | **tang deu**, khong dung yen |
| 4 | Loi frame | `err=` trong log | **khong tang** (giu nguyen / 0) |
| 5 | RAM | `heap=` trong log | **on dinh** qua nhieu phut, **khong tut dan** |
| 6 | Chi bao tren man | thanh tieu de moi man (`link_state_now()`) | `LINK OK` (khong phai `LINK ERR` / `NO LINK`) |
| 7 | Cam ung | cham thu vao 1 nut | dung vi tri, khong leo truc |
| 8 | Lenh xuong Marlin | console COM7 → go `M115` | Marlin tra ve `Marlin 2.1.2 ...` |
| 9 | Phien ban 2 phia | man INFO (`proto` / `MARLIN fw`) | `CYDLCD_PROTO_VERSION` khop (`2`) — lech la phai nap lai **ca hai** board |

> ℹ️ Neu `LINK ERR` thay vi `NO LINK`: **co byte toi nhung khong thanh frame hop le** ⇒ nghi **sai
> baud / sai khung / nhieu day**, khong phai dut day. `NO LINK` + `rx=0` moi la **khong co tin hieu**
> (sai cong / chua chung GND / board tat) — xem bang xu ly su co o **§6** va bang chan doan o **§2.1**.

## Buffer serial (so lieu that, do 2026-10-13)
Baud **250000 8N1 = 25 000 B/s, 40 us/byte**. Frame PUSH toi da 198 B (`CYDLCD_MAX_PAYLOAD 192` + 6).
- **Marlin** (core STM32duino): mac dinh chi `SERIAL_RX_BUFFER_SIZE 64` / `SERIAL_TX_BUFFER_SIZE 64`
  (core chua `HardwareSerial.h:43-48`, tran an toan 256). Nay da ep len **256/256** bang build flag
  trong `ini/features.ini` (key `CYDLCD_SERIAL_UI`). 64 B chi du 2,56 ms dem -> khong du cho burst.
- **Màn hình CYD ESP32**: core mac dinh RX 256 B (du ~10 ms). Nay dat **1024 B** bang `setRxBufferSize()`
  TRUOC `begin()` (`src/cydlcd_config.h`, `CYDLCD_RX_BUFFER`) -> du ~41 ms, an toan khi LVGL giu vong lap
  vai chuc ms de ve man.
- **Burst thuc te** (mot lan goi, khong nhip): `REQ_SETTINGS` = 33 frame x 12 B = 396 B;
  `REQ_FULL` ~ 491 B; danh sach file 30 file ~ 1,9 kB; `REQ_MESH` 7 x 23 = 161 B.
- **Vi sao phai co hang doi TX phia Marlin**: `tx_send()` cu BO frame khi `availableForWrite() < total`
  (buffer 64 B, kha dung that 63 B) => 33 dong setting chi ~5 dong lot, frame ket thuc danh sach file
  co the mat (màn hình CYD hien "(dang nhan)" mai). Nay `cydlcd_serial.cpp` co hang doi 8 slot + `tx_drain()`
  ghi ra UART khi du cho (khong cho, khong delay) va cac nguon hang loat (SETTINGS/FILES/MESH) duoc
  bom 1 frame moi lan `onIdle()` => khong mat frame nao, do tre ~20-25 ms cho 33 dong.
- Tran RX thi **im lang** ca hai phia (khong co co tran o lop Arduino); phia Marlin `MarlinSerial.cpp`
  bo byte khi ring day, phia ESP32 driver bo byte khi het buffer => bieu hien la thieu du lieu chu
  khong bao loi. Vi vay moi nguong dem phai rong rai.

## Font 1-bit tu sinh (Adafruit GFXfont) - thay cho lv_font_conv
Vi sao: font sinh bang `lv_font_conv` (Consolas/Roboto Mono) **lam mat het chu** tren màn hình CYD that
(xem cam bay 45 o README goc: dinh dang bitmap + don vi `adv_w`). Nay dung font 1-bit tu sinh:
- `tools/ttf2gfxfont.py` - TTF -> header C dung dinh dang Adafruit GFXfont, nhieu dai lien tuc
  (`--font --size --extra-ranges --name -o`, co `--yadvance`/`--threshold`/`--dump`).
- `src/gfx_font_robotomono.h/.c` - Roboto Mono **size 13 px**: `xAdvance 8`, `yAdvance 17`, `base_line 3`,
  5 dai (0x20-0xFF, 0x0102-0x0103, 0x0110-0x0111, 0x01A0-0x01B1, 0x1EA0-0x1EF9), 287 glyph co net,
  **2926 byte bitmap**, tong 5714 byte flash (nam o DROM). Du 100% dau tieng Viet.
- `src/lv_font_gfxfont.c/.h` - driver `lv_font_t` doc GFXfont (repack sang dang lien tuc cua LVGL) +
  `lv_font_gfxfont_selftest()` in ra COM7: box/adv/ofs/checksum cua 'A' '0' 'a' U+1EC7 U+0110.
- Cong tac `UI_MONO_FONT` trong `ui_lvgl.cpp`: **0 = chi Montserrat (khong dau tieng Viet, du phong)**,
  **4 = mono GFX gan tung nhan (MAC DINH, dang dung, co du dau)**. Che do 1/2/3 (font sinh bang
  `lv_font_conv`) da **XOA** khoi repo vi da thu tren may that deu mat chu.
- Doi font mono: sua DUY NHAT `UI_MONO_FACE` trong `ui_lvgl.cpp`.
- Kiem tra tren may: reset board roi doc COM7 -> khoi `[GFXFONT]` phai khop gia tri trong
  `tools/test_gfxfont_layout.py` (tool nay con so TUNG PIXEL voi FreeType: 0 lech cho ca tieng Viet).
