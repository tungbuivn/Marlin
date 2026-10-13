# lcd_panel — firmware màn hình riêng cho Marlin (Voron 300 / MKS Monster8 V2)

Một MCU **riêng** lo toàn bộ việc vẽ màn hình; Marlin không phải render gì.
Có **3 kiểu giao tiếp**, chọn bằng env trong `lcd_panel/platformio.ini` (không phải sửa code):

| | **Kiểu G-code** | **Kiểu PUSH** (mới, dùng cho màn LVGL) | **Kiểu TRAM** (mới — **không cần ExtUI**) |
|---|---|---|---|
| Env | `panel_f411cc`, `panel_f103c6` | `panel_f407vgz_lvgl`, `panel_f411cc_lvgl`, `panel_esp32_cyd` | như kiểu PUSH |
| Panel lấy dữ liệu | tự hỏi `M105` / `M114` / `M27` (`marlin_link.cpp`) | Marlin **chủ động đẩy frame nhị phân** xuống (`panel_link.cpp`) | dòng ASCII `TRAM …` (`tram_link.cpp`) — **chỉ** dữ liệu Tramming Wizard |
| Vẽ bằng | driver ILI9341 tự viết + font 8×8 (`ui.cpp`) | **LVGL 8.3** (`ui_lvgl.cpp`) — **màn NGANG 320×240**, bố cục + **icon lấy từ màn TFT của Marlin** (§5) |
| Marlin cần | chỉ bật 1 UART trống | bật `PANEL_SERIAL_UI` (ExtUI `panel_serial`) + tắt màn 12864 | **chỉ thêm 1 dòng `#define PANEL_TRAM_LINK`** — **KHÔNG** tắt gì của MarlinUI ⇒ **màn 12864 + menu MarlinUI vẫn nguyên vẹn** |
| Dữ liệu nặng (file SD, bed mesh, PID/steps/mm, prompt…) | **không có** | **có** (Marlin đẩy hết) | **không có** (chỉ Tramming Wizard — §5.6) |
| Flash/RAM | 20,7 KB / 1,9 KB (F411) · 21,1 KB / 1,9 KB (F103C6) | cần ≥ 256 KB flash / 128 KB RAM (LVGL + font + ~17 KB icon) | như kiểu PUSH (+0,3 KB khi thêm màn TRAMMING — xem §1.1) |

> **Vì sao có kiểu PUSH?** Panel ở MCU riêng *không* đọc được biến nội bộ của Marlin. Nếu chỉ
> hỏi bằng G-code thì rất nhiều "khối" không thể vẽ: danh sách file SD, lưới bed mesh, thông số
> PID / steps/mm / jerk, trạng thái probe/endstop, hộp thoại xác nhận, tên file đang in…
> (MKS TFT cũng làm kiểu G-code và phải thêm hàng loạt M-code riêng —
> tham khảo `makerbase-mks/MKS-TFT28-32-Firmware`.) Nên kiểu mới: Marlin **đẩy** dữ liệu xuống,
> panel chỉ gửi G-code cho **hành động** (G28 / M104 / M140 / M24 / M524…).

> **Vì sao có thêm kiểu TRAM (2026-10-13)?** Kiểu PUSH bắt buộc bật `PANEL_SERIAL_UI` (ExtUI), mà
> ExtUI **chiếm `MarlinUI::init_lcd()/update()`** nên phải **tắt màn 12864 + `CUSTOM_MENU_MAIN` +
> `LED_CONTROL_MENU`** ⇒ **mất toàn bộ menu MarlinUI** (Tramming Wizard, `Z ALIGN MANUAL`…).
> Kiểu TRAM chỉ thêm **một link ASCII rất nhỏ** (`Marlin/src/feature/panel_tram_link.cpp`) để màn
> riêng hiển thị + điều khiển **Bed Tramming Wizard**, **không dùng ExtUI** ⇒ chỉ cần thêm
> `#define PANEL_TRAM_LINK` là xong, bản build 12864 không mất gì. Xem **§5.6** và
> `docs/issues/0001-panel-serial-ui-tat-custom-menu.md` §3c.

---

## 1. Chọn board / env

| Env | Board | Màn hình | Giao tiếp |
|---|---|---|---|
| `panel_f411cc` | STM32F411CCU6 ("Black Pill") | ILI9341 SPI 240×320 | G-code |
| `panel_f103c6` | STM32F103C6T6 ("Blue Pill") | ILI9341 SPI 240×320 | G-code |
| `panel_f411cc_lvgl` | STM32F411CCU6 | ILI9341 SPI 240×320 → **ngang 320×240** (`-DTFT_ROTATION=1`) | PUSH + LVGL |
| `panel_f407vgz_lvgl` | STM32F407VGT6 (1 MB/128 KB) | ILI9341 SPI 240×320 → **ngang 320×240** (`-DTFT_ROTATION=1`) | PUSH + LVGL |
| `panel_esp32_cyd` | **ESP32-2432S028R "CYD"** 2.8" | **ST7789** 240×320 (bản ILI9341 chỉ khác 1 dòng driver) + XPT2046 (gắn sẵn trên board) → **ngang 320×240** (`setRotation(1)`) | PUSH + LVGL |

* Board F407 khác (F407ZGT6 144 chân…): sửa `board =` trong env (`black_f407zg`,
  `vccgnd_f407zg_mini`, `genericSTM32F407IGT6`…) và giữ nguyên phần còn lại.
* Env LVGL kéo thư viện từ mạng lần đầu: `lvgl@^8.3.11` (+ `TFT_eSPI` cho CYD).
* **`TFT_ROTATION` (2026-10-13)**: `panel_config.h` chỉ còn là **mặc định** (`TFT_ROTATION 0`, portrait
  cho UI G-code `ui.cpp`); 2 env LVGL truyền `-DTFT_ROTATION=1` để UI LVGL chạy **ngang 320×240**.
  Đổi chỗ này thì phải xem lại `UI_LCD_HOR_RES/VER_RES` trong `src/ui_lvgl.h`.
* **Env CYD — 2 cờ BẮT BUỘC** (đã có sẵn trong `platformio.ini`):
  `-DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h`. Thiếu 2 cờ này TFT_eSPI dùng
  `User_Setup.h` mặc định trong thư viện (ILI9341, **không có `TFT_BL`**) → màn trắng/không đèn nền.
  `ui_lvgl_port.cpp` cũng có `#warning` nhắc nếu `PANEL_CYD_SETUP_LOADED` không được định nghĩa.

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
| `panel_f411cc_lvgl` | 154.544 B / 262.144 (59,0 %) | 52.376 B / 131.072 (40,0 %) |
| `panel_f407vgz_lvgl` | 156.136 B / 1.048.576 (14,9 %) | 81.096 B / 131.072 (61,9 %) |
| `panel_esp32_cyd` | 460.005 B / 1.310.720 (35,1 %) | 101.812 B / 327.680 (31,1 %) |

> **2026-10-13** (commit `1edfd7aa0b` — UI kiểu Marlin + icon thật): số LVGL **đã đo lại** sau khi
> thêm bộ icon (`ui_lvgl_icons.h`, ~17 KB) và màn MENU/FEED-FLOW. Trước đó: F411 **134.012/46.180**,
> F407 **135.928/74.900**, CYD **441.561/95.284**. Con số CYD ở trên **đang bật `-DPANEL_CYD_DEBUG=1`**
> (cờ tạm để hiệu chuẩn touch) — tắt đi thì nhỏ hơn một chút.
> Hai env G-code (`panel_f411cc`, `panel_f103c6`) **không đổi** (chúng loại `ui_lvgl*.cpp` khỏi build).

> **2026-10-13** (màn **TRAMMING**, §5.6): đo lại sau khi thêm `tram_link.cpp` + màn TRAMMING:
> `panel_f411cc_lvgl` **157.152 B / 262.144 (59,9 %) · RAM 52.608 B / 131.072 (40,1 %)**;
> `panel_f407vgz_lvgl` **158.756 B (15,1 %) / 81.328 B (62,0 %)**;
> `panel_esp32_cyd` **463.101 B (35,3 %) / 102.044 B (31,1 %)** (vẫn bật `-DPANEL_CYD_DEBUG=1`).
> Màn TRAMMING thêm **~2,6 KB flash** (F407: 156.136 → 158.756) — chủ yếu là code LVGL cho 4 nút +
> 6 nhãn, phần link TRAM chỉ vài trăm byte.
> Build này **vẫn giữ nguyên** `-DPANEL_CYD_DEBUG=1` như trước (không phải số của bản dùng thật).

> Nếu F411 thấy lỗi cấp phát LVGL (`lv_mem` hết) thì tăng `LV_MEM_SIZE` trong `src/lv_conf.h`
> từ 20 KB lên 28–32 KB (F411 còn 128 KB RAM; **buffer vẽ giờ là 320×40×2 = 25,6 KB** vì màn ngang).

---

## 2. Nối dây

### 2.1 UART panel ↔ Marlin (MKS Monster8 V2)

Marlin đang dùng `SERIAL_PORT -1` (USB CDC) để nói với PC → còn UART trống. **Chân nối phụ thuộc
kiểu giao tiếp** — hai kiểu dùng **hai UART khác nhau**:

| Kiểu | UART (Marlin) | Chân Monster8 | Vì sao |
|---|---|---|---|
| **PUSH** (`PANEL_SERIAL_UI`) | **USART1 (mặc định hiện nay)** hoặc USART3 | **USART1: PA10 (RX) / PA9 (TX) = header MKS WiFi** ← *đang dùng* · USART3: PD9 (RX) / PD8 (TX) = EXP1 chân 5/6 | Chọn bằng `-DPANEL_SERIAL_PORT=1` (USART1, header WiFi — dây đang đi sẵn) hoặc `=3` (USART3/EXP1, chỉ khi đã tháo màn 12864 vì EXP1_06 = `NEOPIXEL_PIN`). Cờ build nằm ngay trong `ini/features.ini` (key `PANEL_SERIAL_UI`). ⚠️ **Trước 2026-10-13 PUSH chỉ chạy USART3** ⇒ panel báo `NO LINK` dù dây đúng ở header WiFi (đã sửa) |
| **TRAM** (`PANEL_TRAM_LINK`) | **USART1** | **PA10 (RX) / PA9 (TX) = header MKS WiFi** | USART3 **hết chân TX rảnh** (PD8 = `NEOPIXEL_PIN`, PB10 = `HEATER_BED`, PC10 = `SD_SCK`); PA9/PA10 **hoàn toàn rảnh** (`//#define WIFI_SERIAL 1 // USART1` đang comment trong `pins_MKS_MONSTER8_V2.h`) |

**Kiểu TRAM (khuyến nghị cho Tramming Wizard) — KHÔNG đụng PD8/NeoPixel, KHÔNG đụng chân reset màn 12864:**

| Monster8 (header **MKS WiFi**) | Chân MCU | Nối tới panel |
|---|---|---|
| chân RX | **PA10 = USART1_RX** | ← **TX của panel** (CYD: GPIO22) |
| chân TX | **PA9 = USART1_TX** | → **RX của panel** (CYD: GPIO35) |
| GND | GND | GND (**bắt buộc nối chung**) |

* Dây **chéo TX↔RX**; baud 250000 hai bên; panel cấp nguồn riêng (USB/5V) — **không** lấy 5V từ
  header vào chân 3V3 của CYD.
* ⚠️ Header MKS WiFi còn **3 chân khác — KHÔNG dùng**: `IO0` = **PB14**, `IO1` = **PB15**
  (`WIFI_IO0_PIN`/`WIFI_IO1_PIN`, còn gọi là UDISK), và `RESET` = **PD14** (`WIFI_RESET_PIN`). Ba chân
  này **không phải UART** — PB14/PB15 là USB OTG_HS nên **không có AF USART**, và STM32F4 **không có
  software-serial tổng quát** trong Marlin ⇒ nối vào cũng không chạy. Chỉ lấy đúng **TX / RX / GND**.
* Build flag cho USART1 nằm trong **`ini/features.ini`** (key `PANEL_TRAM_LINK`):
  `-DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9` ⇒ **chỉ** được thêm khi bật
  `#define PANEL_TRAM_LINK`, nên bản build hiện tại không đổi gì.
* ⚠️ Nếu định bật **cả** `WIFI_SERIAL` (USART1) thì phải chọn cổng khác — `panel_tram_link.h` có
  `#error` chặn sẵn.

**Kiểu PUSH (`PANEL_SERIAL_UI`) — USART3, chỉ dùng khi chấp nhận tắt màn 12864:**

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
* **Panel CYD (ESP32)**: UART đến Marlin = `Serial2`, mặc định **GPIO22 = TX**, **GPIO35 = RX**
  (chân khai trong `src/panel_config.h`: `PANEL_UART_TX` / `PANEL_UART_RX`). Nối **chéo** với
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
  `-DPANEL_UART_RX=27` (firmware TRAM dùng `IO27` làm RX thay cho `IO35`).
  Cách tìm chân ①(GND) khi board không in nhãn: đo **thông mạch với vỏ USB-C**; hai chân liền kề là
  IO35 rồi IO22.
* **Test riêng phía panel (không cần board in)**: cắm jumper **IO22 ↔ IO35** trên P3 rồi xem log USB
  (`monitor_speed 115200`): `panel_link` tự nghe thấy chính nó ⇒ `rx` **tăng** = UART + 2 chân OK.
  `rx=0` **và** `err=0` = **không có tín hiệu** ở chân RX (sai cổng/chưa chung GND); nếu sai baud thì
  `err` sẽ tăng chứ không phải 0.

### 2.2 Màn hình (khi đi dây với board STM32)

| Chân panel | Chức năng | Ghi chú |
|---|---|---|
| PA5 / PA7 / PA6 | SPI1 SCK / MOSI / MISO | ILI9341 |
| PA4 / PB0 / PB1 | CS / DC(RS) / RST | |
| PB10 | Backlight (BL) | `TFT_PIN_BL -1` nếu BL nối thẳng 3V3 |
| **PA2 / PA3** | **USART2 TX / RX → mainboard** | đây là `Serial` trong `panel_config.h` |
| PB12 / PB13 / PB14 | Encoder A / B / SW (nút) | dùng `INPUT_PULLUP` (không cần cho CYD) |

Với **CYD**: màn + cảm ứng đã nối sẵn trong board, cấu hình nằm ở
`lcd_panel/tools/tft_setup/User_Setup.h` — **bắt buộc** nạp file này bằng
`-DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h` (đã có trong env).

* **Driver (2026-10-13)**: board này dùng **ST7789** (`#define ST7789_DRIVER` + `TFT_INVERSION_ON`);
  nếu là bản ILI9341 thì đổi đúng 1 dòng thành `ILI9341_2_DRIVER` (và bỏ `TFT_INVERSION_ON` nếu
  trắng/đen bị ngược).
* **Chân**: LCD MOSI 13 / MISO 12 / SCLK 14 / CS 15 / DC 2 / RST −1; **đèn nền `TFT_BL` = GPIO21**
  (CYD/ILI9341) **hoặc GPIO27** (nhiều bản ST7789) — dò bằng `-DPANEL_CYD_DEBUG=1` + thử lần lượt.
  Touch: T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36 (có thể đổi bằng macro
  `PANEL_CYD_TOUCH_*` trong `panel_config.h`).

---

## 3. Phía Marlin — 2 cách bật

### 3.1 Kiểu TRAM (khuyến nghị — giữ nguyên màn 12864 + menu MarlinUI)

```powershell
# 1) thêm ĐÚNG 1 dòng vào Marlin/Configuration.h (bất kỳ chỗ nào, ví dụ ngay dưới khối BAUDRATE):
#        #define PANEL_TRAM_LINK
# 2) build + nạp Marlin như bình thường:
pio run -e mks_monster8 -t upload
```

* **Mặc định TẮT** (`//#define PANEL_TRAM_LINK` trong `src/feature/panel_tram_link.h`). Chưa thêm
  dòng nào ⇒ **không** biên dịch module, **không** chiếm UART, **không** tốn flash, và
  `Configuration.h`/`Configuration_adv.h` **giữ nguyên 100%** so với bản 12864.
* Khi bật: `MKS_MINI_12864_V3` **vẫn bật**, `CUSTOM_MENU_MAIN` **vẫn bật**, `LED_CONTROL_MENU`
  **vẫn bật**, `NEOPIXEL_LED` **vẫn bật** (link dùng **USART1/PA9-PA10**, xem §2.1) ⇒ **không mất
  menu MarlinUI nào**.
* Build flag USART1 được thêm tự động từ `ini/features.ini` (key `PANEL_TRAM_LINK`) — đúng như khi
  bạn bật định nghĩa đó, không cần sửa `platformio.ini`/`stm32f4.ini`.
* Số liệu build thật (`pio run -e mks_monster8`, đo 2026-10-13):

| | Flash | RAM |
|---|---|---|
| `PANEL_TRAM_LINK` **tắt** (mặc định, 12864) | **267.576 B** (25,5 %) | **12.220 B** (9,3 %) |
| `PANEL_TRAM_LINK` **bật** | **270.592 B** (25,8 %) | **12.828 B** (9,8 %) |

⇒ tính năng thêm **~3,0 KB flash / ~0,6 KB RAM** khi bật; tắt thì **không có gì thay đổi**.

### 3.2 Kiểu PUSH (`PANEL_SERIAL_UI`) — chỉ khi chấp nhận mất menu 12864

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
> 👉 **Từ 2026-10-13 không cần thoả hiệp này nữa nếu chỉ cần Tramming Wizard** — dùng **kiểu TRAM**
> (§3.1 + §5.6): màn 12864 + toàn bộ menu MarlinUI giữ nguyên.

Số liệu build thật của **kiểu PUSH** (`pio run -e mks_monster8`):

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

> **2026-10-13 — thêm nhóm test cho TRAM** (`src/tram_link.cpp` + `src/tram_protocol.h`): **+35 kiểm
> tra** (tổng **~75**): định dạng số `tram_fmt_mm` (0 · 0,15 · số âm · làm tròn), **format → parse
> round-trip** đầy đủ trường, đọc được **dòng tối thiểu** (thiếu 3 trường mở rộng) và **từ chối
> dòng sai**, `tram_cmd_parse_exact` (không nhận lệnh rác), bộ "sniff" dòng TRAM trên **cùng UART
> với frame PUSH** (byte `T`/`TRAM` trong payload **không** bị ăn mất), và việc **ngừng gửi PING**
> khi đã vào chế độ TRAM.
### Gửi lệnh G-code/M-code từ PC xuống Marlin (2026-10-13)

Hai cách, dùng khi muốn thử lệnh **y như bấm nút trên LCD**:

1. **`serial-console.ps1`** (gốc repo — công cụ khuyến nghị, luôn gửi **CRLF** và bật **DTR**):
   ```powershell
   pwsh -File D:\Marlin\serial-console.ps1                          # qua PANEL (COM7 @115200)
   pwsh -File D:\Marlin\serial-console.ps1 -Port COM4 -Baud 250000  # thẳng vào Marlin (USB)
   ```
   Gõ `G28`, `G35`, `M220 S100`, `G91` → `G1 X10 F600` → `G90`, … ; gõ `exit` để thoát.
2. **Terminal ngoài**: PuTTY (COM7, 115200, 8N1, bật *Implicit CR in every LF*) hoặc PlatformIO
   `pio device monitor -p COM7 -b 115200`. ⚠️ Nếu terminal chỉ gửi `\r` thì các lệnh gõ liên tiếp
   sẽ **dồn thành 1 dòng** (xem README gốc, cạm bẫy 40) — firmware panel nay đã chấp nhận **cả `\r`
   và `\n`**, nhưng vẫn nên dùng CRLF.

> ⚠️ **Máy này chưa chạy được `run_host_test.ps1`** (không có `g++`/MinGW trên PATH; script cần
> `pwsh`). Đã kiểm bằng `arm-none-eabi-g++ -fsyntax-only -Wall -Wextra` → **0 lỗi, 0 cảnh báo**;
> muốn chạy thật thì cần cài MinGW (`winget install BrechtSanders.WinLibs.POSIX.UCRT`) rồi chạy
> `pwsh lcd_panel/tools/run_host_test.ps1`.

---

## 5. Điều khiển trên panel (UI LVGL — **kiểu Marlin**)

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
| Hàng **XYZ** | khung (4, 103, 312, 24); chưa home → hiện `?` màu vàng (`AXIS_NOT_HOMED`), khung đổi màu theo liên kết |
| **FR / FL** | (70, 136) và (170, 136), **icon 32×32** + `%`; xanh lá khi đúng 100 %, vàng khi khác |
| **SD / SETTINGS** | 2 nút **icon 64×64** ở (0, 130) và (256, 130) |
| Thời gian in | (96, 176, 128, 20) — `đã in / còn lại` |
| Thanh tiến độ | (4, 198, 312, 9) + `%` ở (226, 176) |
| Dòng trạng thái | (0, 216, 320, 20) — `STATE  tên file` hoặc `NO LINK` |

### 5.2 Icon thật của Marlin (`src/ui_lvgl_icons.h`)

Icon **không còn** là hình vẽ lại bằng LVGL: `lcd_panel/src/ui_lvgl_icons.h` chứa **10 icon
64×64 / 32×32 chuyển nguyên từ bitmap GREYSCALE4 của Marlin** (`Marlin/src/lcd/tft/images/*.cpp`)
sang `LV_IMG_CF_ALPHA_4BIT`, rồi **đổi màu lúc chạy** bằng `lv_obj_set_style_img_recolor()` +
`recolor_opa = LV_OPA_COVER`.

| Nguồn trong Marlin | Icon xuất ra | Dùng ở |
|---|---|---|
| `hotend_64x64x4.cpp` | `ui_icon_hotend` | ô đầu in (STATUS) |
| `heated_bed_64x64x4.cpp` | `ui_icon_bed`, `ui_icon_bed_heated` | ô bàn in — tắt / đang nung (target > 0) |
| `fan_64x64x4.cpp`, `fan_slow_64x64x4.cpp`, `fan_fast_64x64x4.cpp` | `ui_icon_fan`, `ui_icon_fan_slow`, `ui_icon_fan_fast` | ô quạt — theo tốc độ (0 / <50 % / ≥50 %) |
| `sd_64x64x4.cpp` | `ui_icon_sd` | nút SD |
| `settings_64x64x4.cpp` | `ui_icon_settings` | nút SETTINGS |
| `feedrate_32x32x4.cpp`, `flowrate_32x32x4.cpp` | `ui_icon_feedrate`, `ui_icon_flowrate` | FR/FL trên STATUS + hàng FEED/FLOW |

**Sinh lại icon** (chạy từ **thư mục gốc repo**):

```powershell
python lcd_panel/tools/make_lvgl_icons.py
# -> Da ghi D:\Marlin\lcd_panel\src\ui_lvgl_icons.h (10 icon, 17408 byte du lieu)
```

* Cần **Python 3** — script chỉ dùng `re` / `sys` / `pathlib`, **không cần Pillow** (khác
  `tools/make_font.py`), và cần **nguồn Marlin** có sẵn ở `Marlin/src/lcd/tft/images/`.
* Kết quả ~**17,0 KB** dữ liệu icon, nằm trong flash của panel.
* **KHÔNG sửa tay** `ui_lvgl_icons.h` — chạy lại script là **ghi đè**. Muốn thêm/bớt/đổi icon thì
  sửa bảng `ICONS` trong `tools/make_lvgl_icons.py` (tên file `.cpp`, tên biến mảng, tên symbol
  xuất ra, rộng, cao) rồi chạy lại.
* Đầu file sinh ra có sẵn dòng cảnh báo `FILE NAY DUOC SINH TU DONG -- dung sua tay`.
* Bảng đổi mức xám: `level ≤ 8` (nền + lớp bóng của Marlin) → **trong suốt**; `9…15` → alpha
  `(level−8)×15/7`, nên icon hiện **giống hệt bản Marlin** khi `recolor` = màu chính và nền =
  `COLOR_BACKGROUND`.
* Cần **`#define LV_USE_IMG 1`** trong `src/lv_conf.h` (đã bật ở commit này; trước đây là `0`).

### 5.3 Các màn

| Màn | Nội dung / lệnh gửi |
|---|---|
| **STATUS** | bố cục y nguyên Marlin — §5.1 (màn gốc, không có BACK) |
| **MENU** | menu dọc kiểu Marlin: Temperature · Motion (JOG) · Feed/Flow rate · SD card · Bed mesh · Custom G34/SD · Settings · Info; cột phải mỗi dòng là **giá trị tóm tắt** (nhiệt E0, X/Y/Z đã home, feed/flow %, số file, cỡ mesh, `n/33`, LINK…) |
| **FEED/FLOW** | chỉnh feed/flow bằng **`M220 S…` / `M221 S…`** (nút −10/−1/+1/+10, giới hạn 10…999 %, gửi ngay khi bấm); nút `RESET 100%` → `M220 S100` + `M221 S100` |
| **TEMP** | +/- 1 và 10 °C cho nozzle & bed → `M104 S…` / `M140 S…`; **hàng FAN** → `M106 S0…255`; `COOLDOWN` → `M104 S0` + `M140 S0` |
| **JOG** | chọn trục X/Y/Z, ±0.1/±1/±10 mm (`G91` → `G1 <trục><d> F600` → `G90`), `HOME ALL (G28)` |
| **FILES** | `lv_roller` danh sách từ SD (Marlin đẩy xuống): `PRINT`/bấm file → `M23` + `M24`; `DIR` = vào thư mục; `UP`/`REFRESH` gửi `REQUEST`; PAUSE `M25`, RESUME `M24`, STOP `M524` |
| **SETTINGS** | bảng thông số (steps/mm · max feed · max accel · jerk · PID · probe offset · z-offset · feed/flow %); `SAVE EEPROM` → `PANEL_REQ_SAVE_SETTINGS` |
| **MESH** | lưới bed mesh (7×7 UBL) đọc từ Marlin, ô cao/thấp tô màu; `REFRESH` → `PANEL_REQ_MESH` |
| **CUSTOM** | `G34 Bed` (`G34 Q99`) và `Init SD` (`M22`+`M21`) — đúng 2 mục menu cũ |
| **TRAMMING** | Bed Tramming Wizard của Marlin (kiểu TRAM, **không cần ExtUI**): 4 số của 4 góc, dấu `*` góc đang đứng, dòng hướng dẫn `CW/CCW…`/`DAT`, mode `MAX`/`MIN`, 5 nút **HOME+PROBE · PROBE · MAX/MIN · BACK · DONE** → xem **§5.6** |

> 🆕 **Cập nhật 2026-10-13 (UI LVGL sau khi chuyển sang PUSH):**
> * **TEMP** có thêm **1 hàng preset PREHEAT** đúng như bản UI cũ (`ui.cpp`/`main.cpp`): **`PLA 210/60`** ·
>   **`PETG 240/80`** · **`ABS 250/100`** (mỗi nút gửi `M104 S<nozzle>` + `M140 S<bed>`) và **`COOLDOWN`**
>   (`M104 S0` + `M140 S0`).
> * **SETTINGS**: bảng **chỉ vẽ những dòng Marlin thực sự gửi giá trị** (bỏ `Junction dev` / `Linear adv` /
>   PID khi build không dùng) và **sửa được tại chỗ**: chạm một dòng → hiện lớp phủ có **4 nút bước
>   `0.01` · `0.1` · `1` · `10`** (nút đang chọn được tô nổi) + `-` / `+` (giữ nút = nhảy nhanh) + `OK` /
>   `HUY` → `OK` gửi M-code tương ứng (`M92` steps · `M203` max feed · `M201` max accel ·
>   `M205` jerk · `M851` probe offset · `M220/M221` feed/flow · `M304` PID bàn · `M104/M140/M106`
>   nhiệt & quạt) rồi tự xin lại thông số. Hàng nút ở đáy: **`UP` · `DOWN` · `SAVE EEPROM`** (SAVE = `M500`).
>   ⚠️ **KHÔNG còn dòng `Z offset mm`** (nó trùng `Probe Z mm` vì cùng là `probe.offset.z`, và từng gửi
>   nhầm `M206 Z` = home offset) — chỉ dùng **`Probe Z mm` → `M851 Z`**.
> * **Chữ tiếng Việt có dấu**: UI dùng font **Segoe UI 14** sinh riêng bằng `lv_font_conv`
>   (`lcd_panel/src/font_segoe_vn_14.c`, dải `0x1EA0-0x1EF9` + `Ă ă Đ đ Ơ ơ Ư ư`) — font Montserrat mặc
>   định của LVGL **không có dấu** nên nếu sinh lại font thì phải giữ đủ dải này.
> * **FEED/FLOW**: sau mỗi lần `+/-` (gửi `M220`/`M221`) panel **tự xin lại thông số** nên con số trên màn
>   cập nhật ngay (trước đây bị đứng ở 100%).
> * **JOG**: X/Y nhích ở **`F3000` = 50 mm/s** (bằng tốc độ home), Z giữ **`F600` = 10 mm/s** (trần `M203 Z10`).
| **INFO** | tên firmware, proto, frame/lỗi, `UI: LVGL 8.3 + encoder/touch`; `REFRESH FULL` → `PANEL_REQ_FULL` |

* **Prompt của Marlin** (hết filament, xác nhận…) hiện hộp thoại OK/CANCEL; OK gửi `ANSWER`
  → Marlin gọi `ExtUI::setUserConfirmed()`. Thông báo (`panel.message`) tự đóng sau 6 s.
* Chữ trên màn hình là **ASCII** (font `montserrat_14` không có dấu tiếng Việt) — đó là lý do
  nhãn màn hình viết không dấu (`NHIET DO`, `THONG SO`…).

### 5.4 Encoder đọc bằng ngắt (`src/encoder.cpp`)

Panel **không polling** encoder: hai chân A/B ngắt đổi trạng thái, ISR chỉ đẩy mẫu 2 bit vào một
**ring cố định 32 phần tử** (không heap, không STL), rồi luồng chính giải mã bằng bảng chuyển tiếp:
chuyển tiếp **1 bit** = cạnh thật → ±1 xung (đủ `ENC_PULSES_PER_DETENT` = 4 xung = 1 nấc);
chuyển tiếp **2 bit** (dội phím / cạnh đến lúc ring đầy) → **không đếm**, chỉ đồng bộ lại;
quay quá nhanh trong một lần đọc → **kẹp ở `ENC_MAX_DETENTS`** (3).

Lý do: mỗi lần vẽ một khối lớn (ví dụ `fillScreen` 320×240 ≈ 150 KB qua SPI) mất hàng chục ms —
trong khoảng đó polling **không đọc được chân** nên chắc chắn mất nấc.

### 5.5 Hiệu chuẩn cảm ứng 4 góc (chỉ board có touch — CYD)

Cảm ứng XPT2046 trên CYD do panel **tự bit-bang SPI mềm** (TFT_eSPI chỉ đọc được XPT2046 khi nó
chung bus SPI với LCD, mà CYD đi dây riêng), nên hiệu chuẩn nằm **trong firmware panel**
(`ui_port_touch_calibrate()` — `src/ui_lvgl_port.cpp`).

**Cách vào** (2 đường, không cần build lại):

1. Trên STATUS bấm **SETTINGS** → **MENU** → dòng **`Calibrate touch (giu 1.5s)`** — **PHẢI GIỮ TAY
   1,5 giây rồi NHẤC** mới vào; chạm ngắn (bấm nhầm) **không làm gì** (cột phải đổi `dang giu` →
   `NHAC TAY` để biết đã đủ lâu). Lúc rảnh cột phải ghi `NVS` nếu đã có hiệu chuẩn lưu trong flash,
   `mac dinh` nếu đang dùng giá trị trong `panel_config.h`.
   > **Vì sao phải giữ:** trước đây chỉ cần 1 lần chạm là vào, mà ở **mỗi góc cũng chỉ cần 1 lần chạm**
   > là được nhận và **ghi ngay vào NVS** (`touch_cal_save()` trước khi hiện kết quả) ⇒ lỡ tay trong
   > MENU là mất hiệu chuẩn, phải giữ tay lúc cấp điện mới vào lại được.
2. **Giữ tay trên màn hình lúc cấp điện** — panel phát hiện `T_IRQ` mức thấp trong
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
(`erase_flash` mới xoá). Khi chưa có NVS thì dùng **giá trị mặc định trong `src/panel_config.h`**:
`PANEL_CYD_TOUCH_SWAP_XY 0`, `INVERT_X/Y 0`, `X 184..3866`, `Y 185..3910` — đây là **kết quả
hiệu chuẩn thật** đã chạy tốt trên board (dòng MENU hiện `NVS`/`mac dinh` để biết đang dùng bộ nào).

**Khi nào cần làm lại**: đổi/thay tấm cảm ứng hoặc board, chạm lệch rõ ở **giữa** màn hình dù 4
góc đúng, hoặc sau khi `erase_flash`. Không cần làm lại sau mỗi lần nạp firmware thường.

**Đọc thêm khi hiệu chuẩn**: bật `-DPANEL_CYD_DEBUG=1` (env `panel_esp32_cyd`, **đang bật tạm**) →
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
> (`Marlin/src/feature/tramming_run.cpp`). Phía Marlin chỉ cần **1 dòng** `#define PANEL_TRAM_LINK`
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
TRAMMING trên panel **dùng được độc lập**, không cần mở wizard trên màn 12864 trước. Dòng 1 hiện
`READY (bam HOME + PROBE)` khi wizard chưa chạy.

> 🔀 **2026-10-13 — CHUYỂN SANG PUSH (ExtUI `PANEL_SERIAL_UI`)**: để panel có **đủ dữ liệu** (nhiệt độ,
> X/Y/Z, tiến độ + tên file, quạt, lưới bed mesh, danh sách file SD, prompt) thay vì chỉ dòng `TRAM`,
> dự án đã bật PUSH và **tắt** `MKS_MINI_12864_V3` + `NEOPIXEL_LED` + `CUSTOM_MENU_MAIN` +
> `LED_CONTROL_MENU` (`lcd_panel/tools/set_panel_mode.ps1 -Mode on`), đồng thời **comment**
> `PANEL_TRAM_LINK` (2 chế độ loại trừ nhau: cùng 1 UART).
> **Màn TRAMMING trên panel vẫn chạy**: trạng thái wizard được Marlin gửi trong frame
> **`PANEL_F_TRAM` (0x0E)** — payload là **1 dòng ASCII đúng format `tram_protocol.h`** (tái dùng
> nguyên bộ giải `tram_status_parse()`), và lệnh của panel đi trong frame **`PANEL_F_TRAM_CMD` (0x44)**
> (`u8` ký tự `P/D/M/R/B/Q`) → `tramming_report_cmd()` phía Marlin. Nhờ vậy **không mất** wizard khi
> đổi giao thức. Code chế độ TRAM cũ **vẫn nằm trong repo** (bật lại bằng `set_panel_mode.ps1 -Mode off`
> + `#define PANEL_TRAM_LINK`), panel tự nhận cả 2 đường (`panel.linked` → gửi frame; ngược lại → ASCII thô).

**Chỉ báo link — chính sách hiển thị (2026-10-13, theo yêu cầu: gộp về header):**
| Chỗ hiển thị | Nội dung | Ghi chú |
|---|---|---|
| **Thanh trên cùng** (mọi màn) | `LINK OK` / `LINK ERR` / `NO LINK` | **NƠI DUY NHẤT** hiện chỉ báo link — `link_state_now()`: **1** = đọc được dòng `TRAM` hợp lệ (`panel.linked \|\| tram.linked`), **2** = **có byte tới nhưng không thành dòng hợp lệ** (`tram.rx_active`, nghi sai baud/khung/nhiễu), **0** = **không có byte nào** (đứt dây/sai cổng/board tắt). Trước đây chỉ xét `panel.linked` nên ở chế độ TRAM **luôn** `NO LINK` dù link tốt |
| Trang **STATUS (home)** | dòng trạng thái + viền khung XYZ | vẫn báo mất link (viền đỏ + `NO LINK`) — theo yêu cầu, giữ riêng cho trang home |
| Màn **TRAMMING** | mode/phase/góc + dòng hướng dẫn `CW/CCW…`/`DAT` | **đã BỎ nhãn `LINK OK/NO LINK`** ở đây (trước đây hiện trùng với header) |
| MENU dòng `Tramming wizard` | `san sang` / `chua bat` / `mat link` / `<MODE> <phase>` | trạng thái **wizard**, không phải chỉ báo link (dùng `link_ok()` để quyết định `mat link`) |
| MENU dòng `Info` | `TRAM` / `PUSH` / `-` | giao thức đang dùng (trước đây ghi `LINK`/`NO LINK` → trùng) |
| Màn `INFO` | `LINK: OK (TRAM) state …` | trang chẩn đoán nên vẫn ghi rõ giao thức + trạng thái |

Mọi chỗ hiển thị link đều đi qua **một hàm duy nhất** `link_ok()` trong `ui_lvgl.cpp` — sửa logic link chỉ cần sửa hàm này.

**Tên máy in:** ở chế độ PUSH tên lấy từ gói `HELLO` của Marlin; ở chế độ **TRAM không có HELLO** nên màn INFO hiện `FW: <PANEL_MACHINE_NAME> (TRAM link)` — đổi tên bằng `PANEL_MACHINE_NAME` trong `src/panel_config.h` hoặc `-DPANEL_MACHINE_NAME=\"...\"` trong `platformio.ini`.

**Chẩn đoán không cần thiết bị gì thêm:** log USB của CYD (115200) in **mỗi giây** một dòng:

```
[CYD] loop OK | link PUSH=0 TRAM=1 state=0 rx=0 err=0 heap=... | TRAM dong=67 loi=0 byte=4221 cach-day=97ms
```

`byte` = **tổng số byte** nhận trên chân RX (đếm cả rác), `dong` = số dòng `TRAM` hợp lệ, `loi` = dòng sai khung, `cach-day` = ms từ dòng cuối. Link tốt ⇒ `dong` tăng ~**5/giây**, `loi=0`, `cach-day` < **2000 ms** (`TRAM_LINK_TIMEOUT_MS`).

**4 nút còn lại gửi đúng 1 ký tự xuống Marlin** (`tram_protocol.h`): `P` = probe lại góc đang đứng ·
`D` = góc này xong (đi góc cần chỉnh kế tiếp; hết góc → home Z + probe lại 4 góc) · `M` = đổi mốc
`MAX`↔`MIN` · **`BACK`** gửi `B` (thoát wizard: stow probe + đánh dấu Z chưa home) rồi về MENU.
Ngoài ra panel gửi `Q` khi mới vào màn. Marlin trả lời bằng **1 dòng ASCII mỗi 200 ms**:

```
TRAM <phase> <mode> <idx> <ref> <d0> <d1> <d2> <d3> <A0> <A1> <A2> <A3> <active> <all_dat> <hint>
```

* `phase` 0 IDLE / 1 HOMING / 2 PROBING / 3 GOTO / 4 POINT / 5 AUTO · `mode` 1 = MAX (mốc `Z-MIN`),
  0 = MIN (mốc `Z-MAX`) · `idx` = góc nozzle đang đứng · `ref` = **mốc** đang so (mm, 3 số lẻ) ·
  `d0..d3` = độ lệch từng góc (`---.---` = chưa đo) · `A0..A3` = đã bấm DONE ·
  `hint` = `DAT` / `CW:0.150` / `CCW:nhe` / `PROBE` / `ERR` / `-`.
* **`hint` do Marlin tính sẵn** (dùng đúng `tramming_turn_cw` + `TRAM_DONE_MM` như màn 12864) nên
  panel **không** tự suy lại chiều vặn vít ⇒ số và chiều ở 2 màn luôn giống nhau.
* Định nghĩa **dùng chung y nguyên** ở 2 nơi: `Marlin/src/feature/tram_protocol.h` và
  `lcd_panel/src/tram_protocol.h` (sửa một bên thì **copy sang bên kia**).
* Panel đọc dòng `TRAM …` **ngay trong vòng đọc UART của `panel_link.cpp`**; byte không thuộc dòng
  TRAM được trả lại **nguyên vẹn** cho bộ giải frame PUSH (không mất byte nào), và khi đã vào chế độ
  TRAM thì panel **ngừng gửi PING** (tránh byte nhị phân lọt vào bộ giải lệnh ASCII của Marlin).
* Ở chế độ này các màn khác (STATUS/TEMP/FILES…) vẫn mở được nhưng hiện `NO LINK` — chúng cần frame
  PUSH của `PANEL_SERIAL_UI`, mà 2 chế độ **loại trừ nhau** (`panel_tram_link.h` có `#error` chặn).

**Nối dây:** USART1 = **PA9 (TX) / PA10 (RX)** — bảng đầu §2.1.

---

## 6. Xử lý sự cố

| Hiện tượng | Kiểm gì |
|---|---|
| Màn trắng / không có gì | dây SPI (SCK/MOSI/CS/DC/RST), chân BL, `TFT_ROTATION`; thử `TFT_SPI_SPEED` thấp hơn (12 MHz) nếu dây dài |
| Header hiện `NO LINK` | **kiểu PUSH**: dây TX/RX có **chéo** không, GND chung chưa, baud 2 bên có trùng `250000`, Marlin đã bật `PANEL_SERIAL_UI` chưa (và đã tắt màn 12864 chưa), cờ `-DUSING_HW_SERIAL3 -DPIN_SERIAL3_*` có trong env chưa |
| Màn **TRAMMING** hiện `NO LINK` | **kiểu TRAM**: Marlin đã thêm `#define PANEL_TRAM_LINK` chưa; dây nối **USART1 = PA9/PA10** (header MKS WiFi) đã **chéo TX↔RX** + **chung GND** chưa; baud 2 bên 250000; `git diff Marlin/Configuration.h` phải thấy đúng 1 dòng `PANEL_TRAM_LINK`. Trong MENU, dòng `Tramming wizard` ghi `chua bat` = chưa nhận được dòng `TRAM` nào. **Trước tiên hãy chắc chắn dây panel đang ở `P3` (IO22+IO35) chứ KHÔNG phải `P5`/`P1` cạnh USB-C** — xem §2.1; và đọc log USB: `rx=0 err=0` = không có tín hiệu (sai cổng/chưa chung GND), `err>0` = sai baud |
| **Thanh trên cùng ghi `NO LINK` nhưng dòng MENU ghi `san sang`** | **Bình thường ở chế độ TRAM** (trước 2026-10-13 là lỗi hiển thị, đã sửa): thanh trên từng chỉ xét `panel.linked` (**giao thức PUSH**) nên ở chế độ TRAM luôn `NO LINK` dù link TRAM tốt. Nay thanh trên xét **cả hai** (`panel.linked \|\| tram.linked`) ⇒ hiện `LINK OK`. Nếu vẫn `NO LINK` thật thì đọc log USB: `TRAM dong=` có tăng không, `cach-day` có < 2000 ms không |
| Cần biết **panel có nhận byte nào không** (không cần thiết bị gì thêm) | Log USB của CYD (`115200`) in mỗi giây: `link PUSH=<0/1> TRAM=<0/1> rx-active=<0/1> … \| TRAM dong=<số dòng hợp lệ> loi=<dòng lỗi> byte=<TỔNG byte nhận> cach-day=<ms>`. Đọc: `byte=0` + `rx-active=0` = **không có tín hiệu** ở chân RX (sai dây/cổng/chưa chung GND/board tắt) · `rx-active=1` mà `dong=0` = **có byte nhưng sai khung/baud** (tương ứng `LINK ERR`) · `dong` tăng ~**5/giây** = link tốt · `cach-day` khi rảnh phải **< 2000 ms**, khi wizard **đang bận** cho phép tới **20 s** (`TRAM_LINK_BUSY_TIMEOUT_MS`) |
| **`NO LINK` hiện đúng lúc máy đang probe / di chuyển giữa các góc** | **Đã sửa 2026-10-13** (link KHÔNG hề mất — bấm `PROBE` vẫn chạy): Marlin có những đoạn **block vài giây** mà **không gọi `idle()`** (probe/dò, `home Z`) nên các dòng `TRAM` bị ngưng; panel hết timeout 2 s là báo sai. Nay: (1) khi trạng thái cuối cho thấy wizard **đang bận** (`active` + `phase != IDLE`) thì dùng timeout dài **`TRAM_LINK_BUSY_TIMEOUT_MS` 20 s**; (2) panel **chủ động hỏi lại bằng `Q`** mỗi `TRAM_LINK_QUERY_MS` (1 s) khi im lặng — Marlin đáp ngay ở tick kế tiếp (`TRAM_CMD_QUERY` → `s_force`), nên trạng thái luôn tươi trong lúc máy làm việc |
| Màn **TRAMMING** hiện `PROBE`/`ERR` mãi | góc đó chưa probe được (đầu dò lỗi/ngoài tầm) → bấm `PROBE` lại; `ERR` = lần probe vừa rồi trả `NAN` |
| Panel không nhận nhiệt độ/vị trí | Marlin phải build ở **chế độ panel** (`set_panel_mode.ps1 -Mode status` để xem) — nếu còn `MKS_MINI_12864_V3` thì Marlin sẽ báo lỗi SanityCheck |
| Encoder quay ngược | `ENC_REVERSE 0/1` trong `src/panel_config.h` |
| Nhiệt độ nhảy loạn | đường serial nhiễu: giảm baud 2 bên (`PANEL_MARLIN_BAUD` + `BAUDRATE_2`) hoặc dùng cáp ngắn/xoắn |
| ESP32 build lỗi `lv_conf.h` | phải có `src/lv_conf.h` và cờ `-DLV_CONF_INCLUDE_SIMPLE -Isrc` (đã có trong env). Nếu LVGL báo `Possible failure to include lv_conf.h` thì include path sai (đừng dùng `${PROJECT_DIR}` — nó bị expand sai trong project này) |
| CYD: chạm không ăn / lệch | TFT_eSPI giả định XPT2046 **chung bus SPI với LCD** (chỉ khác CS) nên `tft.getTouch()` **không** đọc được XPT2046 trên CYD; `ui_lvgl_port.cpp` đã bit-bang SPI mềm cho touch (T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36). **Đừng sửa macro bằng tay nữa — chạy hiệu chuẩn 4 góc** (menu `Calibrate touch`, hoặc giữ tay lúc cấp điện) rồi xem **§5.5**; kết quả lưu vào NVS nên không cần build lại. Muốn thử lại đường TFT_eSPI thì `#define PANEL_CYD_TOUCH_USE_TFTESPI` (khi đó **không** hiệu chuẩn được) |
| CYD: màu bị đảo | `tools/tft_setup/User_Setup.h` **đang** bật `TFT_INVERSION_ON` (ST7789 thường cần): trắng↔đen ngược thì **comment dòng đó**; xanh↔đỏ thì `TFT_RGB_ORDER TFT_BGR` |
| CYD: không đèn nền / màn tối | `TFT_BL` mặc định **GPIO21**; nhiều bản ST7789 dùng **GPIO27** — bật `-DPANEL_CYD_DEBUG=1` + thử lần lượt, và thử `fillScreen(TFT_RED)` để tách lỗi đèn nền khỏi lỗi driver/SPI |
| TFT_eSPI không thấy file setup | **bắt buộc** có `-DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h` trong `build_flags` — env CYD **đã có sẵn**; thiếu thì TFT_eSPI dùng setup mặc định (ILI9341, không có `TFT_BL`) và `ui_lvgl_port.cpp` in `#warning` |
| ESP32 build lỗi `PermissionError: [Errno 13] … _contrib\intelhex\__init__.py` | ACL của `%USERPROFILE%\.platformio` bị hỏng (esptool không chạy được) → chạy **`tools/fix_platformio_acl.ps1`** bằng PowerShell **Administrator** — xem §8 |
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
Còn **icon** của kiểu LVGL là file sinh tự động khác — xem **§5.2**.

---

## 8. `tools/fix_platformio_acl.ps1` — sửa ACL hỏng của PlatformIO (Windows)

**Vấn đề nó giải quyết**: trên máy này một số thư mục trong
`%USERPROFILE%\.platformio\packages\tool-esptoolpy\_contrib\` (`intelhex`, `ecdsa`) bị **DENY ACE** →
**không đọc được file**, kể cả `Get-Acl`/`icacls` cũng bị từ chối. Hậu quả: `esptool.py` không chạy
được ⇒ **PlatformIO KHÔNG build/nạp được env ESP32 (`panel_esp32_cyd`)** với lỗi:

```
PermissionError: [Errno 13] Permission denied: '...\_contrib\intelhex\__init__.py'
```

Env **STM32 không dùng esptool** nên **không bị ảnh hưởng**.

**Chạy khi nào**: chỉ khi build ESP32/CYD gặp đúng lỗi `PermissionError` ở trên.
**Không bắt buộc** cho việc build bình thường — máy sạch (ACL đúng) thì không cần chạy.

```powershell
# PowerShell với quyền Administrator (bắt buộc)
powershell -ExecutionPolicy Bypass -File D:\Marlin\lcd_panel\tools\fix_platformio_acl.ps1
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
  `pio run -d D:\Marlin\lcd_panel -e panel_esp32_cyd -t upload --upload-port COM3`.

---

## 9. Nhận file G-code qua Bluetooth (BT SPP) → thẻ TF của panel → ESP3D → thẻ SD của Marlin

> Chỉ có ở **env `panel_esp32_cyd`** (board CYD = ESP32). Tắt/mở bằng `PANEL_BT_UPLOAD` trong
> `src/panel_config.h` (hoặc `-DPANEL_BT_UPLOAD=0` trong `platformio.ini`). Code nằm ở
> `src/panel_bt_upload.cpp` + `src/panel_bt_upload.h`; **không** đụng `ui_lvgl.cpp` /
> `panel_protocol.h` / `panel_link.cpp`.

### 9.1 Luồng (2 pha + 1 bước cuối)

```
Điện thoại --BT SPP--> CYD --(ghi)--> /btup/<ten>.tmp trên thẻ TF của CYD
                                        |
                                        +--(pha 2, KHÓA UI)--> HTTP POST multipart --> ESP3D
                                                                  (module WiFi, ghi thẳng
                                                                   thẻ SD của Monster8)
                                        |
                                        +--> M22 + M21 (init lại thẻ cho Marlin) --> chờ 800 ms
                                             --> PANEL_REQ_FILES --> xóa file tạm --> DONE (BT)
```

* **Pha 1** (nhận qua BT) chạy trong `loop()` → UI vẫn dùng được bình thường (chỉ hiện 1 khung nhỏ).
* **Pha 2** (đẩy lên ESP3D) **blocking + khóa UI**: lớp phủ toàn màn hình trên `lv_layer_top()`
  và tắt mọi input device (`lv_indev_get_next()` + `lv_indev_enable(i,false)`), nên **không cần
  sửa `ui_lvgl.cpp`**. Mọi đường thoát (thành công/lỗi/timeout) đều mở khóa lại (`ui_lock(false)`)
  và xóa lớp phủ.
* Vì file đã nằm trên thẻ nên **biết trước `Content-Length`** → dùng `Content-Length` +
  `multipart/form-data`, **không cần chunked**.

### 9.2 Bật và tên thiết bị BT

| Việc | Ở đâu |
|---|---|
| Bật/tắt tính năng | `PANEL_BT_UPLOAD` — mặc định **1** với board CYD (`panel_config.h`) |
| Tên thiết bị khi quét BT | `PANEL_BT_NAME` = **`"TBT-PANEL"`** (`panel_config.h`) |
| WiFi + IP module ESP3D | `PANEL_ESP3D_SSID` / `PANEL_ESP3D_PASS` / `PANEL_ESP3D_HOST` / `PANEL_ESP3D_PORT` |

```c
// panel_config.h -- giá trị ĐANG DÙNG (khớp D:\arduino\ESP3D-direct-sd\esp3d\config.h)
#define PANEL_ESP3D_SSID "Voron2308"        // AP của module ESP3D   (config.h:370 DEFAULT_AP_SSID)
#define PANEL_ESP3D_PASS "v0r0n24s3cur3"    //                       (config.h:371 DEFAULT_AP_PASSWORD)
#define PANEL_ESP3D_HOST "192.168.0.1"      // AP IP STATIC của module (config.h:376 DEFAULT_IP_VALUE
                                            //  {192,168,0,1} + DEFAULT_AP_IP_MODE = STATIC_IP_MODE);
                                            //  "" = tự dò IP từ đầu (xem 9.2.1)
#define PANEL_ESP3D_PORT 80
```

WiFi bật **ngay từ lúc khởi động** (không phải chỉ trong lúc đẩy file) để icon trạng thái ở header
đúng ngay; `WiFi.setAutoReconnect(true)` + thử lại mỗi 8 s nếu mất kết nối. Lúc **nhận qua BT
(pha 1)** WiFi được để ở chế độ tiết kiệm (`WiFi.setSleep(true)`) cho BT có trọn radio; lúc **đẩy
file** thì tắt tiết kiệm để đẩy nhanh hơn.

#### 9.2.1 Tự dò IP module ESP3D (chỉ chạy khi bắt đầu pha 2, kết quả được cache)

Không phụ thuộc việc người dùng nhớ đúng IP. Thứ tự thử (`esp3d_ensure()` trong `panel_bt_upload.cpp`):

1. **`PANEL_ESP3D_HOST`** (đang là `192.168.0.1`) — nếu trả lời thì **dùng luôn, không dò tiếp**;
2. **mDNS** — chỉ khi build có `-DPANEL_ESP3D_USE_MDNS=1` (**mặc định TẮT**, xem lý do bên dưới):
   thử lần lượt các tên trong `PANEL_ESP3D_MDNS_NAMES` (`"MYESP,esp3d"`);
3. **gateway của mạng đang nối** (`WiFi.gatewayIP()`) — ở chế độ AP thì **chính là module**;
4. **danh sách ứng viên** `PANEL_ESP3D_IP_LIST` = `192.168.4.1, 192.168.0.1, 192.168.1.1,
   192.168.1.50, 192.168.0.50`.

Mỗi ứng viên tối đa **1,5 s**, tổng tối đa **15 s**; chỉ chạy **một lần** cho tới khi tìm được (cache
`IPAddress`), và tự xoá cache nếu lần POST sau không kết nối được. Log COM7:

```
[BTUP] dang do IP ESP3D...
[BTUP] ESP3D thay o 192.168.0.1 (nguon: cau hinh)      <- hoac: mdns / gateway / danh sach
[BTUP] KHONG tim thay ESP3D (da thu: 192.168.0.1,192.168.4.1,...)
```

**Cách nhận dạng “đúng là ESP3D”, không chỉ “port 80 mở”** (bắt buộc, vì `192.168.0.0/24` **trùng
dải IP mạng nhà** — router TP-Link cũng thường là `192.168.0.1`): panel gửi
`GET /command?plain=[ESP800]` và chỉ chấp nhận khi phản hồi có chuỗi **`FW version:`**
(`esp3d/command.cpp:1916+`). Lệnh này là lệnh duy nhất không cần đăng nhập và **không đụng vào thẻ
SD của Marlin** (khác với `GET /files`, thứ sẽ giành bus SPI của thẻ).

**Vì sao mDNS mặc định TẮT:** tên mDNS của ESP3D **không cố định** — `esp3d/config.h:34,37` đặt
`ESP_DEFAULT_NAME "MYESP"` và `ESP_HOST_NAME = ESP_DEFAULT_NAME` ⇒ mặc định là **`MYESP.local`**
(không phải `esp3d.local`), và tên thật còn sửa được trong EEPROM (`EP_HOSTNAME`, qua web UI).
Thêm nữa `ESPmDNS` kéo thêm DRAM biến tĩnh (xem 9.8: trần DRAM của CYD chỉ còn ~2,5 KB). Khi cần,
bật bằng `-DPANEL_ESP3D_USE_MDNS=1` — nhưng **phải build kiểm lại** vì có thể tràn DRAM.

### 9.3 Gửi file từ điện thoại — 2 kiểu

| Kiểu | Định dạng | Kết thúc phiên |
|---|---|---|
| **Có header** (ưu tiên) | dòng `PUT <ten_file> <so_byte>` + `\n`, rồi đúng `<so_byte>` byte nhị phân | đủ số byte (thiếu quá 8 s → `ERR`) |
| **Gửi thô** (app kiểu "Serial Bluetooth Terminal") | dữ liệu ngay từ byte đầu | không còn byte nào trong **3 giây** |

* Kiểu thô lấy tên mặc định `PANEL_BT_DEF_NAME` = `bt_upload.gco`; tên không có phần mở rộng thì
  tự thêm `.gco`; mọi ký tự lạ/đường dẫn bị loại.
* Tên file (kiểu header) tối đa 31 ký tự, chỉ `[A-Za-z0-9._-]`.
* **Chỉ nhận khi Marlin không đang in** (`panel.flags & PANEL_SF_SD_PRINTING` hoặc state
  PRINTING/PAUSED/HOMING/LEVELING/RUNOUT/KILLED) — vì lúc đẩy file ESP3D sẽ giành bus SPI của thẻ
  SD (xem 9.6), đang in mà giành bus là hỏng bản in. Đang in → `ERR dang in`.
* Không cho 2 phiên chồng nhau: trong lúc nhận/đẩy, byte mới tới chỉ bị bỏ và trả `ERR`.
* Heap < `PANEL_BT_HEAP_MIN` (20 KB) → `ERR heap thap`.

**Trả lời qua BT** (app đọc được, mỗi dòng kết thúc `\n`):

```
OK <ten> <so_byte>     phiên kiểu header được nhận
OK <ten> RAW           phiên kiểu thô được nhận
PROG <so_byte_da_nhan> mỗi ~64 KB
POST <so_byte>         đã nhận xong, bắt đầu đẩy lên ESP3D (bắt đầu KHÓA UI)
DONE <so_byte>         POST thành công, đã gửi M22/M21 + xin lại danh sách file
ERR <lý do>            lỗi (không gửi M22/M21)
```

### 9.4 Endpoint upload thật của ESP3D (đọc từ `D:\arduino\ESP3D-direct-sd\`)

| Mục | Giá trị | Nguồn |
|---|---|---|
| URL | `http://<PANEL_ESP3D_HOST>:<PANEL_ESP3D_PORT>/files` | `esp3d/webinterface.cpp:386` `web_server.on("/files", HTTP_ANY, handleSDFileList, SPIShareSdCardUpload);` |
| Method | `POST` (handler đăng ký `HTTP_ANY`) | như trên |
| Kiểu body | **bắt buộc `multipart/form-data`** (có `boundary`) mới gọi được upload handler | `Parsing-impl.h` (`_parseRequest`: chỉ khi `Content-Type` bắt đầu bằng `multipart/` mới đặt `isForm=true` → `_parseForm`) |
| Tên field | **không quan trọng** — ESP8266WebServer nhận ra phần file qua `filename="..."` | `Parsing-impl.h::_parseForm` (`argIsFile` khi có `filename=`) |
| Thứ tự trong `Content-Disposition` | **`name="..."` phải đứng TRƯỚC `filename="..."`** (code cắt chuỗi theo dấu `=` đầu tiên) | `Parsing-impl.h::_parseForm` |
| `Content-Length` | **BẮT BUỘC gửi** trong bản này (vì multipart + để hợp với mọi bản ESP3D). Lưu ý: `_parseForm` của ESP8266WebServer **bỏ qua** tham số `len` (`(void) len;`) và kết thúc theo `\r\n--boundary`, nên về lý thuyết không cần — nhưng panel đã có file trên thẻ nên cứ gửi cho chắc | `Parsing-impl.h::_parseForm` |
| `Transfer-Encoding: chunked` | **KHÔNG dùng** (server đồng bộ của ESP8266 không giải mã chunked; byte khung chunk sẽ bị ghi vào file) | `Parsing-impl.h::_parseRequest` (không hề đọc header này) |
| Đăng nhập | **không cần** — `AUTHENTICATION_FEATURE` đang tắt nên `is_authenticated()` trả `LEVEL_ADMIN` | `esp3d/config.h:89` + `esp3d/webinterface.cpp:437-439` |
| Ghi vào đâu | `/` + `filename` (gốc thẻ SD) khi là admin | `esp3d/syncwebserver.cpp:1013-1042` (`SPIShareSdCardUpload`), `PANEL_ESP3D_DIR` chỉ để ghi tài liệu |
| File trùng tên | bị **xóa rồi ghi lại** | `esp3d/syncwebserver.cpp:1031-1034` |
| Trả lời | `200 application/json`, JSON có `"status":"Ok"` khi xong (ngược lại `"Upload failed"`/`"Initialize sd failure"`); lỗi ghi file thì ESP3D trả sớm `500` | `esp3d/syncwebserver.cpp:443-696` (`handleSDFileList`), `:78-102` (`pushError`) |

Panel **coi là thành công** khi: dòng trạng thái HTTP đầu tiên là `200` **và** (nếu tìm thấy)
`"status":"Ok"`. Nếu card nhiều file đến mức phần JSON vượt `PANEL_BT_MAX_REPLY_BYTES` (24 KB) mà
chưa thấy `"status"` thì vẫn tính là xong và ghi cảnh báo vào log (xem 9.9).

### 9.5 Thẻ TF của CYD — chân, định dạng, file tạm

| Mục | Giá trị | Nguồn |
|---|---|---|
| Bus | **VSPI — bus RIÊNG với LCD** (LCD ở HSPI) | `tools/tft_setup/User_Setup.h:56-57`: “CYD: LCD o HSPI (VSPI de danh cho khe the SD: CS 5 / MOSI 23 / MISO 19 / SCLK 18)” + `#define USE_HSPI_PORT` |
| Chân | **SCK 18 / MISO 19 / MOSI 23 / CS 5** (`PANEL_SD_*` trong `panel_config.h`) | như trên; đây cũng là chân VSPI mặc định của core (`variants/esp32/pins_arduino.h`) |
| Tốc độ | `PANEL_SD_HZ` = 20 MHz | `panel_config.h` |
| Không đụng gì | cảm ứng XPT2046 là chân riêng 25/32/39/33/36 (bit-bang trong `ui_lvgl_port.cpp`), LCD 12/13/14/15 + DC 2 + BL 21 | `User_Setup.h:47-71` |
| Định dạng thẻ | **FAT32** (thư viện `SD` của core 2.0.17 không mount exFAT) | — |
| File tạm | `/btup/<ten>.tmp` (thư mục riêng, đuôi `.tmp`; Marlin chỉ liệt kê **gốc** thẻ nên không thấy) | `PANEL_BTUP_DIR`, `panel_bt_upload.cpp::open_temp()` |
| Kích thước tối đa | = chỗ trống còn lại của thẻ TF (kiểm tra trước khi ghi khi biết trước kích thước; chừa 256 KB) và ≤ 4 GB/FAT32 | `open_temp()` |
| Log mỗi phiên | `[BTUP] the TF: con X KB / tong Y KB (kieu header\|kieu RAW)` — không để thẻ đầy âm thầm | `open_temp()` |

#### 9.5.1 Dọn file tạm — chống “file mồ côi” khi panel bị reset

Nếu panel bị **reset / mất điện / watchdog** giữa lúc đang nhận hoặc đang POST thì `/btup/<ten>.tmp`
sẽ **nằm lại vĩnh viễn** (đầy thẻ dần mà không ai biết). Vì vậy:

* **Lúc khởi động**: `panel_bt_upload_begin()` mount thẻ TF rồi gọi `clean_dir("luc khoi dong", true)`
  — quét `/btup`, xóa **mọi** file `.tmp`, log số file + tổng byte thu hồi + dung lượng trống còn lại:
  `[BTUP] don /btup (luc khoi dong): xoa N file .tmp (M KB); the TF con X/Y KB`.
* **Đầu mỗi phiên**: `open_temp()` gọi `clean_dir("dau phien", false)` (chỉ log khi thật sự có file bị xóa).
* Chưa có thư mục thì **tạo**; mọi trường hợp lỗi (không tạo được, không mở được thư mục, không xóa
  được 1 file) đều được **log rõ ràng** — không im lặng.

**Danh sách đường thoát có xóa file tạm** (hàm chính: `del_temp()`):

| # | Đường thoát | Chỗ trong code |
|---|---|---|
| a | POST thành công | `finish_recv()` → `st_sdwait()` → `del_temp("xong")` |
| b | POST lỗi (ESP3D trả 500, `status != Ok`, timeout, TCP lỗi) | `finish_recv()` nhánh `if (!ok)` → `del_temp("post loi")` |
| c | **BT mất kết nối giữa pha 1** | `st_recv()` kiểm `bt_alive()` (`hasClient()`) → `fail_session()` → `del_temp("phien loi")` |
| d | Hết dữ liệu 3 s nhưng **thiếu byte** (kiểu header) | `st_recv()` (quá `PANEL_BT_STALL_MS`) hoặc `finish_recv()` (`got != s_declared`) → `fail_session()` |
| e | Lỗi ghi thẻ (hết chỗ / `write` fail / không mở được file tạm) | `open_temp()` (hết chỗ), `stage_flush()` (write fail) → `fail_session()` |
| f | **WiFi rớt giữa lúc POST** | `http_post()` (kiểm `WiFi.status()` mỗi khối + `write_all` fail) → `finish_recv()` → `del_temp("post loi")` |
| g | Người dùng ngắt phiên / app disconnect | như (c) — `bt_alive()` sai ⇒ `ERR BT mat ket noi giua pha 1` |
| h | Header sai định dạng / quá thời gian chờ header / đệm BT tràn / file rỗng | `st_sniff()`, `st_recv()` (`s_drop`), `finish_recv()` → `fail_session()` |
| i | Panel bị reset/mất điện giữa phiên | không kịp xóa ⇒ **dọn ở lần khởi động sau** (mục 9.5.1) |

**Đổi hành vi cần biết (kiểu RAW):** vì gửi thô không biết trước số byte, nên nếu **SPP bị ngắt
giữa lúc đang nhận** thì panel **coi là lỗi** (`ERR BT mat ket noi giua pha 1`) và **xóa file tạm**,
thay vì chấp nhận phần đã nhận. Lý do: thà bắt gửi lại còn hơn để **file G-code bị cắt** lọt lên thẻ
SD của Marlin rồi in ra hỏng. ⇒ App phải **giữ kết nối tới khi phiên xong** (muốn chắc chắn thì dùng
kiểu có header `PUT <ten> <so_byte>`). Muốn nới lỏng: bỏ 2 dòng `if (!bt_alive())` trong `st_recv()`.

### 9.6 Vì sao sau khi upload **phải** gửi `M22` + `M21`

`ESP3D-direct-sd` ghi trực tiếp lên thẻ SD của MKS Monster8 qua bus SPI **dùng chung**: trước khi
ghi nó đóng relay để tách Marlin khỏi thẻ (`esp3d/direct_sd.cpp:47-53` `switchRelay(LOW)` trong
`beginSDOperation()`, `:84-103`), ghi xong thì trả lại (`endSDOperation()` → `switchRelay(HIGH)`,
`:55-82`). Trong lúc đó Marlin **không hề biết** thẻ vừa có file mới (nó vẫn giữ trạng thái
`card`/FAT cũ), nên phải:

1. `panel_link_send_gcode2("M22", "M21")` — `M22` = nhả media, `M21` = init media (Marlin mount lại
   thẻ và đọc lại thư mục);
2. chờ **~800 ms** (`PANEL_BT_SDWAIT_MS`) — lệnh G-code đi qua **hàng đợi** của Marlin
   (`panel_serial.cpp: queue.enqueue_one_now()`), còn frame `REQUEST` được xử lý **ngay**; xin sớm
   quá sẽ nhận danh sách **cũ** (đúng như `files_job_task()` trong `ui_lvgl.cpp` đang làm);
3. `panel_link_request(PANEL_REQ_FILES)` (xóa `panel.files_done`/`n_files` trước, để thư mục rỗng
   vẫn được vẽ lại);
4. **chỉ khi POST thành công** mới làm 1–3; POST lỗi thì **không** gửi `M22/M21` mà báo `ERR`.

### 9.7 Icon WiFi ở header — hợp đồng với nhánh UI (`src/net_upload.h`)

```c
#include "net_upload.h"
uint8_t st = net_wifi_state();   // 0 = WiFi tắt/chưa cấu hình/không có module
                                 // 1 = ĐANG kết nối (đang thử, kể cả thử lại) -> icon ĐỎ
                                 // 2 = ĐÃ kết nối                              -> icon XANH
```

* Hàm **chỉ đọc biến trạng thái đã cache** (cập nhật trong `panel_bt_upload_task()`), không chặn,
  không cấp phát, không gọi `WiFi.*` → gọi được từ `ui_lvgl_task()` mỗi vòng.
* Chỉ trả `2` khi `WiFi.status() == WL_CONNECTED` (không phải chỉ vì đã gọi `WiFi.begin()`).
* `net_upload.h` chỉ include `<stdint.h>` (không kéo `WiFi.h`/`BluetoothSerial.h`/`SD.h` vào build
  của UI). Mọi env đều link được: khi tính năng tắt (STM32, hoặc `-DPANEL_BT_UPLOAD=0`) hàm trả `0`.

### 9.8 DRAM: vì sao phải đổi `LV_MEM_SIZE` và bảng phân vùng

ESP32 (không PSRAM) chỉ có **124.580 byte DRAM cho biến tĩnh** (vùng `dram0_0_seg`:
`tools/sdk/esp32/ld/memory.ld:66-67` = `0x3FFB0000 + 0xDB5C`, dài `0x2C200 - 0xDB5C`). Bật WiFi +
Bluetooth Classic kéo theo **~38 KB `.bss`/`.data` tĩnh** của thư viện SDK (libbt 13,0 KB;
libfreertos 5,0; liblwip 3,4; libmesh 2,9; libpp 2,9; libespcoredump 2,2; libnet80211 1,4;
libcoexist 1,2; …). Vì vậy đã phải:

**Số build thật (kiểm bằng build trong thư mục riêng, 0 warning):** `panel_esp32_cyd` →
**RAM 122.040 B / 327.680 (37,2 %) · Flash 1.770.457 B / 3.145.728 (56,3 %)** ⇒ **trần DRAM biến tĩnh
chỉ còn dư ~2,5 KB**. Riêng khi bật `-DPANEL_ESP3D_USE_MDNS=1` (thêm `ESPmDNS`): RAM **123.960 B
(+1.920 B)** · Flash **1.797.105 B (+26,6 KB)** — vẫn link được nhưng chỉ còn **~0,6 KB** DRAM dự
phòng, nên mặc định **để TẮT**.

| Thay đổi | File | Ghi chú |
|---|---|---|
| 2 buffer làm việc (vòng đệm BT **4 KB** + khối ghi thẻ **2 KB**) **cấp phát từ heap** thay vì mảng tĩnh | `src/panel_bt_upload.cpp` | cấp 1 lần trong `panel_bt_upload_begin()` (task BT cần vòng đệm cả khi rảnh). Trước đó để mảng tĩnh 8 KB + 4 KB thì linker báo `dram0_0_seg overflowed by 28144 bytes` |
| `LV_MEM_SIZE` 48 KB → **30 KB** bằng file cấu hình RIÊNG | **mới** `src/lv_conf_panel.h` + `-DLV_CONF_PATH=lv_conf_panel.h` (chỉ env CYD) | `lv_conf_panel.h` **khác `lv_conf.h` đúng 1 dòng cấu hình**: `#undef LV_MEM_SIZE` + `#define LV_MEM_SIZE (30U * 1024U)` (phần còn lại chỉ là chú thích). **KHÔNG sửa trực tiếp `lv_conf.h`** vì: (1) file đó là cấu hình CHUNG cho cả 3 env LVGL (F411/F407/CYD) — hạ pool ở đó sẽ hạ luôn cả 2 board STM32 nơi không có vấn đề DRAM; (2) `lv_conf_panel.h` include `lv_conf.h` rồi chỉ ghi đè 1 macro ⇒ vẫn **một nguồn cấu hình duy nhất**, không copy file |
| Bảng phân vùng `default.csv` (app 1.25 MB) → **`huge_app.csv`** (app 3 MB) | `platformio.ini` (env `panel_esp32_cyd`) | WiFi + BT Classic + LVGL + TFT_eSPI = **1,77 MB flash** > 1,25 MB |

**Vì sao đổi bảng phân vùng mà hiệu chuẩn cảm ứng KHÔNG mất:** cả hai file đều khai báo phân vùng
`nvs` ở **cùng offset `0x9000`, size `0x5000`** (20 KB), chỉ khác phần `app`/`spiffs`:

| File (trong package PlatformIO) | Dòng | Khai báo |
|---|---|---|
| `…\.platformio\packages\framework-arduinoespressif32\tools\partitions\default.csv` | **:2** | `nvs, data, nvs, 0x9000, 0x5000,` |
| `…\tools\partitions\huge_app.csv` | **:2** | `nvs, data, nvs, 0x9000, 0x5000,` |

⇒ NVS (nơi `ui_lvgl_port.cpp` lưu kết quả hiệu chuẩn cảm ứng) nằm nguyên chỗ. **Lưu ý:** bảng phân
vùng đổi thì lần nạp đầu phải nạp cả `firmware.bin` + `partitions.bin` (PlatformIO `-t upload` làm
sẵn), và **tuyệt đối không chạy `-t erase`** nếu muốn giữ hiệu chuẩn. File csv này nằm trong package
của PlatformIO (không phải trong repo) — package cập nhật thì phải xem lại 2 dòng trên.

Muốn trả `LV_MEM_SIZE` về 48 KB thì phải trả lại DRAM ở chỗ khác trước (nếu không linker FAIL):
cách sạch nhất là cho `ui_lvgl.cpp` cấp buffer vẽ 25,6 KB từ heap (hoặc giảm còn `320×20`) — file đó
thuộc nhánh UI khác.

**Buffer/heap mà module này dùng** (để cân đối với nhánh UI):

| Khoản | Byte | Ở đâu |
|---|---|---|
| `.bss`/`.data` (biến tĩnh) | **0** | tất cả đã chuyển sang heap/lớp phủ LVGL |
| Vòng đệm BT (`PANEL_BT_RING_BYTES`) | 4.096 | heap, cấp 1 lần lúc boot |
| Khối ghi thẻ (`PANEL_BT_SD_CHUNK`) | 2.048 | heap, cấp 1 lần lúc boot |
| Stack task hút BT (`TASK_STACK`) | 2.048 | heap (FreeRTOS cấp) |
| Lớp phủ LVGL (~7 object: 1 khung + 5 nhãn + 1 thanh) | ~0,4 KB | từ pool LVGL 30 KB (không phải heap) |
| **Tổng module lấy của heap** | **8.192** | in ra ở log `[BTUP] WiFi + BT Classic DA SAN SANG: … buffer cua module da lay …` |

**Cắt tiếp nếu heap quá thấp** (sửa 2 số trong `panel_config.h`, không cần đổi code):
`PANEL_BT_RING_BYTES` 4096 → 2048 và `PANEL_BT_SD_CHUNK` 2048 → 1024 (tiết kiệm thêm 3 KB; pha 1 vẫn
ghi thẳng xuống thẻ nên không cần buffer lớn) — đổi lại vòng đệm chỉ chịu được ~34 ms thẻ ghi chậm
thay vì ~68 ms, nên theo dõi `drop=` trong log; nếu `drop=` > 0 thì tăng lại.

**Heap lúc chạy:** tổng DRAM dùng được = `0x40000000 − 0x3FFBDB5C` = 271.524 B; trừ **122.040 B** biến
tĩnh (số build thật) → **~146 KB heap lúc boot** (trước khi FreeRTOS/WiFi/BT cấp phát). WiFi STA +
Bluetooth Classic (Bluedroid) lấy của heap **~80–130 KB** tuỳ lúc ⇒ **heap tự do sau khi cả hai đã
kết nối có thể chỉ còn ~10–60 KB**, thấp hơn ngưỡng `PANEL_BT_HEAP_WARN` = 60 KB. Log 1 Hz in
`heap=` (hiện tại) và `heapmin=` (thấp nhất kể từ lúc bắt đầu phiên gần nhất — có tính cả lúc đang
đẩy file), cộng dòng `!! CANH BAO HEAP THAP` khi dưới 60 KB. **Đây là số ước tính từ kích thước
biến tĩnh + tài liệu Bluedroid, KHÔNG đo được trên máy thật từ đây** — phải đọc `heap=`/`heapmin=`
trên COM7 sau khi nạp. Nếu heap tự do < 25 KB: cắt buffer như bảng trên, và/hoặc trả DRAM bằng cách
cho `ui_lvgl.cpp` cấp buffer vẽ từ heap.

### 9.9 Kiểm trên máy thật

1. `pio run -d D:\Marlin\lcd_panel -e panel_esp32_cyd -t upload --upload-port COMn` (đổi bảng phân
   vùng → nên `-t erase` một lần nếu mạch lạ, **sẽ mất hiệu chuẩn cảm ứng trong NVS**).
2. Mở COM7 115200, xem log 1 Hz: có dòng `[BTUP] bt=1 wifi=… st=idle … heap=…` và
   `[BTUP] BT SPP "TBT-PANEL" san sang (task=0)`.
3. Điện thoại: bật BT → thấy **TBT-PANEL** → ghép đôi. Icon WiFi ở header: **đỏ** khi đang kết nối
   WiFi, **xanh** khi đã vào mạng; rút AP đi thì icon về đỏ rồi tự xanh lại (≤ ~10 s).
4. Cắm thẻ TF (FAT32) vào khe của CYD; log phải in `[BTUP] the TF panel OK: …`.
5. Gửi file: app hỗ trợ dòng lệnh gửi `PUT benchy.gco 12345\r\n` + dữ liệu, hoặc app kiểu
   Serial Bluetooth Terminal gửi thẳng file (kết thúc bằng 3 s im lặng).
   - Khung overlay hiện tên file + % + KB/s; app nhận `OK`/`PROG`/`POST`/`DONE`.
   - Lúc `POST`, màn hình bị lớp phủ che và **cảm ứng/encoder không tác dụng** cho tới khi xong.
6. Sau `DONE`: mở màn **FILES** của panel → phải thấy file mới (nếu không, bấm **REFRESH** — nút này
   gửi lại M22/M21), và `ls` trên màn hình Marlin cũng phải thấy (M20 qua host).
7. Kiểm tra file tạm đã bị xóa: tháo thẻ TF của CYD ra PC → thư mục `btup` phải rỗng.
8. Kiểm **dọn file mồ côi lúc khởi động**: cắm thẻ CYD vào PC, tạo tay `btup\gia-lap.tmp` (vài KB),
   cắm lại thẻ vào CYD rồi **reset panel** → log phải có
   `[BTUP] don /btup (luc khoi dong): xoa 1 file .tmp (M KB); the TF con X/Y KB`, và tháo thẻ ra PC thì
   `btup` rỗng.
9. Kiểm **tự dò IP ESP3D**: xoá/đổi `PANEL_ESP3D_HOST` thành `""`, nạp lại, gửi 1 file → log phải có
   `[BTUP] dang do IP ESP3D...` rồi `[BTUP] ESP3D thay o <IP> (nguon: gateway|danh sach)`.
   Nếu thấy `KHONG tim thay ESP3D (da thu: ...)` thì kiểm lại AP của module và dải IP (mục 9.2.1).
8. Đo thông lượng thật: đọc `[BTUP] … KB/s` trong log (hoặc dòng `PROG` trên app). **Lưu ý:** BT
   Classic và WiFi **dùng chung radio** (chia thời gian) nên khi WiFi đang bật, BT chậm đi rõ rệt;
   kỳ vọng thực tế chỉ **vài chục KB/s** ⇒ file 10 MB mất **vài phút**. Nếu quá chậm, thử tắt
   `PANEL_BT_UPLOAD` để so sánh (hoặc để panel gần router).

**Hạn chế đã biết:** (a) BT và WiFi chia sẻ radio → thông lượng mỗi bên giảm; (b) file tạm bị xóa ở
**mọi** đường thoát (kể cả POST lỗi) nên phải gửi lại từ đầu nếu lỗi — đổi ở `finish_recv()` nếu muốn
giữ để đẩy lại (mặc định xóa để tránh đầy thẻ); (c) phiên kiểu header chấp nhận `PUT …` nên file
G-code bắt đầu bằng đúng chữ `PUT ` sẽ bị hiểu là header (rất khó xảy ra); (d) nếu phần JSON trả lời
của ESP3D vượt 24 KB mà chưa thấy `"status"` thì panel vẫn tính là thành công (chỉ log cảnh báo);
(e) **kiểu RAW: ngắt BT giữa phiên bị coi là lỗi** (xem 9.5.1) — app phải giữ kết nối tới khi xong;
(f) thẻ TF **mounted suốt** từ lúc boot (để dọn rác + nhận file ngay) nên nếu rút thẻ ra cắm lại thì
phải khởi động lại panel (hoặc để phiên sau tự thử mount lại — chỉ mount lại khi lần trước THẤT BẠI);
(g) **WiFi chỉ bật khi đẩy file** (xem 9.10) ⇒ icon WiFi ở header **xám (0) phần lớn thời gian**, chỉ
đỏ/xanh trong ~1 phút quanh mỗi lần upload.

### 9.10 🔴 PANIC THẬT 2026-10-14 (`StoreProhibited EXCVADDR=0x00000000`) — nguyên nhân & cách chặn

**Hiện tượng (log COM7 máy thật):** sau `[BTUP] WiFi bat dau ket noi "Voron2308" ...` + SD mount thất
bại, panel **reboot loop**:

```
Guru Meditation Error: Core 0 panic'ed (StoreProhibited). EXCVADDR: 0x00000000
Backtrace: 0x40091e09(memset) <- bta_sys_init (bta_sys_main.c:171)
           <- btu_task_start_up (btu_task.c:276) <- osi_thread_run (osi/thread.c:165)
```
Không hề có dòng `[BTUP] BT SPP "TBT-PANEL" san sang` ⇒ chết **trong lúc Bluedroid khởi động**, ngay
sau `SerialBT.begin()`.

**Nguyên nhân (chứng minh bằng mã + disassembly thư viện đã cài):**

| Bằng chứng | Ý nghĩa |
|---|---|
| `bta_sys_int.h`: `extern tBTA_SYS_CB *bta_sys_cb_ptr;` + `#define bta_sys_cb (*bta_sys_cb_ptr)` | `bta_sys_cb` là **con trỏ cấp phát động** (khi `BTA_DYNAMIC_MEMORY != FALSE`) |
| `nm libbt.a` → `bta_sys_main.c.obj: 00000004 C bta_sys_cb_ptr` | Là **COMMON/BSS** ⇒ bằng **0** lúc boot, phải được gán bằng `osi_malloc` |
| Disassembly `bta_sys_init` (libbt.a): `movi a12,0x130` (size), `movi a11,0` (giá trị), `l32i a10,a2,0` (**a10 = *bta_sys_cb_ptr**), `callx8 memset` | `memset(*bta_sys_cb_ptr, 0, 0x130)`: con trỏ còn **NULL** ⇒ **ghi vào địa chỉ 0** = đúng `StoreProhibited EXCVADDR=0x00000000` |
| Điểm cấp phát: `bte_init.c.obj` (`BTE_InitStack`) có literal trỏ tới `bta_sys_cb_ptr` cùng loạt `bta_dm_cb_ptr`, `avdt_cb_ptr`… | Bluedroid cấp phát cả loạt CB động **không kiểm tra NULL** ⇒ thiếu RAM/phân mảnh là **PANIC**, không phải lỗi trả về |
| `tools/sdk/esp32/sdkconfig`: `CONFIG_BT_CLASSIC_ENABLED=y`, `CONFIG_BT_SPP_ENABLED=y`, **`CONFIG_SW_COEXIST_ENABLE=y` (:2090)** | **Coexistence ĐANG BẬT** ⇒ loại giả thuyết “coex tắt” |
| `esp32-hal-bt.c:29` `#define BT_MODE ESP_BT_MODE_BTDM` | Controller chạy **BTDM** (Classic + BLE) ⇒ **KHÔNG** dùng `esp_bt_controller_mem_release(ESP_BT_MODE_BLE)` để tiết kiệm RAM (sẽ hỏng init) |
| `SerialBT.begin()` chỉ gọi **1 lần duy nhất**, không có `end()` | Loại giả thuyết “begin 2 lần / gọi sau end” |
| `firmware.elf` trong `.pio/build` có `.flash.text` kết thúc `0x401178A3`, **không có** symbol `bta_sys_init`, còn backtrace trỏ `0x4015b630` | **ELF đó KHÔNG khớp firmware đã nạp** ⇒ muốn tra backtrace phải build sạch và giữ đúng ELF |

**Thủ phạm thực tế (thứ tự khởi tạo):** code cũ gọi `WiFi.mode()+WiFi.begin()` ở **boot**, sau đó
`SD.begin()` thất bại, rồi mới `SerialBT.begin()` ⇒ **hai stack radio khởi tạo cùng lúc**
(log `E (3457) wifi:Set status to INIT` ngay trước panic) và WiFi đã ăn ~40-50 KB heap **trước khi**
Bluedroid cấp phát CB của nó.

**Cách chặn (thứ tự khởi động mới trong `panel_bt_upload_begin()`):**

1. **Mount thẻ TF trước**; không mount được ⇒ **TẮT tính năng**, log rõ, **không bật BT**.
   `sd_mount()` nay thử lần lượt **20 → 10 → 4 MHz**, log từng bước, log `cardType()/totalBytes()`
   khi thành công, và khi thất bại in gợi ý: cắm thẻ / **thẻ phải FAT32** / khe TF-bus VSPI 18/19/23/5.
2. Cấp buffer làm việc (heap 4 KB + 2 KB) rồi **cửa an toàn RAM**: in `heap=` và
   `khoi lien tuc lon nhat=` (`heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`), chỉ bật BT khi
   đạt `PANEL_BT_MIN_HEAP` (110 KB) **và** `PANEL_BT_MIN_BLOCK` (48 KB); không đạt ⇒ tắt tính năng.
3. **Sau đó** mới `SerialBT.begin()` + tạo task hút BT, rồi log lại heap/khoi liên tục sau khi BT lên.
4. **KHÔNG bật WiFi lúc boot**: WiFi chỉ `WiFi.begin()` khi vào **pha 2** (`wifi_up()`), tự
   `WiFi.disconnect(true) + WiFi.mode(WIFI_OFF)` sau `PANEL_ESP3D_KEEP_MS` (60 s) rảnh
   (`wifi_stop_idle()` trong `st_idle()`) ⇒ pha 1 chạy **một mình**, WiFi không bao giờ khởi tạo song
   song với BT.
5. Mọi `File` (`SD.open`/`openNextFile`) đều được kiểm tra trước khi dùng; `clean_dir()/open_temp()`
   thoát ngay nếu `!s_sd_ready`.

**Đánh đổi:** icon WiFi **xám (0)** phần lớn thời gian (chỉ đỏ/xanh ~1 phút quanh mỗi lần upload) và
mỗi phiên phải chờ WiFi kết nối lại (~2-4 s) — chấp nhận để panel **không thể chết vì BT/WiFi**. Nếu
sau này muốn WiFi thường trực: bật lại trong `panel_bt_upload_begin()` **và phải đo lại**
`heap=`/`khoi lien tuc` trước `SerialBT.begin()`.

**Bus SPI của thẻ không xung đột với LCD (đã kiểm lại):** `User_Setup.h:57` `#define USE_HSPI_PORT`
⇒ TFT_eSPI chạy **HSPI**; nhánh CYD của `ui_lvgl_port.cpp` chỉ dùng `s_lcd` (TFT_eSPI) + cảm ứng
bit-bang 25/32/39/33/36, còn `SPI.beginTransaction` ở `ui_lvgl_port.cpp:117` nằm trong nhánh **STM32**
(`#endif // !PANEL_BOARD_CYD` ở `:121`) ⇒ thẻ TF trên VSPI 18/19/23/5 là bus riêng.

## Cap nhat 2026-10-13 (protocol v2 + UI mo rong)
Da nap va xac nhan tren may that (`link PUSH=1 TRAM=1 err=0`).
Protocol: `PANEL_PROTO_VERSION 2` - **phai nap ca Marlin va panel**.
- `PANEL_F_ENDSTOPS (0x0F)` + `PANEL_REQ_ENDSTOPS`: trang ENDSTOP moi (6 o
  PROBE/X min/Y min/Z min/X max/Y max, xanh=mo do=CHAM, nut REFRESH gui M119).
- `PANEL_REQ_DFU`: nut `REBOOT DFU` trong CUSTOM, xac nhan 2 buoc; Marlin tu choi
  neu dang ban (`printer_busy()`) va tra WARN.
- Setting id 34..41: `M204 P/R/T` (accel print/retract/travel),
  `M906 X/Y/Z/E` (mA - gui RAW, KHONG nhan he so `panel_val()`), `M412` (runout).
- HELLO mang them `SHORT_BUILD_VERSION` -> `panel.marlin_ver`.
- Trang TEMP: dong dau `Profile: PLA/PETG/ABS` hoac `Profile: ---` (khong con dialog).
- Trang MESH: `G29 P1 | G29 P3T0 | G29 P4RT` + `G29 A | G29 D | G29 S0`.
- JOG: 5 muc buoc 0.01/0.05/0.1/1/10 mm, X/Y F3000 (50 mm/s), Z F600.
- SETTINGS: hang 2 `M501 LOAD | M502 RESET` (co xac nhan), bang 124 px.
- CUSTOM: `M600 B0`, toggle `M412`, `REBOOT DFU`.
- Thanh tieu de: 2 nut nho `II` (pause) / `[]` (stop) chi hien khi
  `panel.flags & PANEL_SF_SD_PRINTING`; nut STOP luon hoi xac nhan truoc `M524`.
- Font: chi 3 nhan co dau (`Gia tri`, `Buoc`, `Huy`) dung `font_segoe_vn_14`;
  KHONG dung font nay cho ca cay (xem cam bay 43 o README goc).

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
- MENU: `MENU_ROWS` 9 -> 10 (them `Endstops (M119)` -> `PG_ENDSTOP`). MENU co 10 muc + dong
  "Calibrate touch" = 11 hang x 20 px = 220 px > 206 => thanh day luon xuat hien o MENU va phan
  trang duoc. Day la duong de them muc menu moi ma khong phai nhoi vao `PG_CUSTOM` nua.
- STATUS khong co thanh day (giong header).

## Font chu (doi 2026-10-13 theo yeu cau)
Toan bo UI gio dung font **MONO** `font_consola_vn_14` (Consolas 14, bpp 4, sinh bang lv_font_conv):
moi ky tu rong 7,7 px nen so dang chay (nhiet do, XYZ, %, gia tri setting) **khong nhay ngang**
khi do dai gia tri doi; so thang cot dep hon.
- Cong tac: `#define UI_MONO_FONT 1` trong `src/ui_lvgl.cpp` (0 = quay lai Montserrat 14 cu).
- Dat font o cap MAN HINH trong `build_chrome()` (`text_font` la thuoc tinh THUA KE nen moi vat
  tao sau deu dung mono) + dat rieng cho `lv_layer_top()` vi `lv_msgbox_create(NULL, ...)` nam o
  layer_top, KHONG thua ke tu man hinh.
- Khi `UI_MONO_FONT = 1` thi `vn_font()` khong lam gi (ca man da la font co dau). Khi = 0 thi
  `vn_font()`/`vn_font_btn()` moi gan `font_segoe_vn_14` cho dung 3 nhan co dau.
- `line_height` 17 (Montserrat 14 la 16) nen hang cao 16 px co the bi cat 1 px: hay kiem tra trang
  INFO va danh sach FILES; neu thay cut thi hoac doi sang sinh font `--size 13`, hoac chi gan mono
  cho rieng cac o GIA TRI.
- Theme mac dinh cua LVGL chi dat font cuc bo cho `cb_marker_checked` (khong dung) => roller/table
  deu thua ke font cua man hinh, khong bi lan font.

Lenh sinh lai font (chay trong `lcd_panel`, giu NGUYEN `--size 14 --bpp 4` va dai ky tu):
```
npx --yes lv_font_conv --font C:\Windows\Fonts\consola.ttf --range 0x20-0x7E,0xA0-0xFF,0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 --size 14 --bpp 4 --format lvgl -o src/font_consola_vn_14.c --force-fast-kern-format
npx --yes lv_font_conv --font C:\Windows\Fonts\segoeui.ttf --range 0x20-0x7E,0xA0-0xFF,0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 --size 14 --bpp 4 --format lvgl -o src/font_segoe_vn_14.c --force-fast-kern-format
```
Sau khi sinh **phai sua tay khoi include** o dau file thanh `#include "lvgl.h"` (lv_font_conv sinh ra
`#ifdef LV_LVGL_H_INCLUDE_SIMPLE ... #else #include "lvgl/lvgl.h"`, ma PlatformIO chi them
`<lvgl-src>` vao duong include -> loi bien dich).
Nho: linker `--gc-sections` TU BO du lieu glyph cua font khong duoc tham chieu, nen giu ca 2 file
font trong `src/` khong ton them Flash (do lai: 484 061 B = 36.9%).

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
pio run -d lcd_panel -e panel_esp32_cyd                               # build
pio run -d lcd_panel -e panel_esp32_cyd -t upload --upload-port COM7   # nap

# hoac dung trong lcd_panel:
cd lcd_panel
pio run -e panel_esp32_cyd -t upload --upload-port COM7
```

* **PHAI chi ro `-e panel_esp32_cyd`**: `default_envs = panel_f411cc` (`platformio.ini:32`) nen
  `pio run -d lcd_panel` **khong co `-e`** se build cho **F411 (STM32)**, khong phai CYD.
* Env CYD **khong khai `upload_port`** (`platformio.ini:123-149`) ⇒ phai truyen **`--upload-port COM7`**
  tren dong lenh; `monitor_speed = 115200` (`platformio.ini:148`).
* Env CYD **can mang lan dau** (keo `espressif32`, `lvgl`, `TFT_eSPI` — `platformio.ini:121, 144-145`).
* `PANEL_PROTO_VERSION` hien la **2** (`src/panel_protocol.h:42`). Doi version thi **phai nap CA HAI
  board** (Marlin + panel), khong thi panel bao `LINK ERR` du byte van chay (cam bay 41 o README goc).

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
  `lcd_panel\.pio\build\panel_esp32_cyd\firmware.bin`**
  (da kiem: co that trong build dir; `bootloader.bin` + `partitions.bin` nam cung thu muc).
* Neu esptool bao `PermissionError: [Errno 13] ... _contrib\intelhex\__init__.py` thi do **ACL hong**
  trong `~/.platformio`, **khong phai** loi firmware ⇒ chay `tools/fix_platformio_acl.ps1` bang
  PowerShell **Administrator** — xem **§8**.

### Doc log chan doan 1 Hz tren COM7

Log nam tren **Serial cua ESP32 (USB-UART = COM7 @ 115200)** va **chi in khi env co
`-DPANEL_CYD_DEBUG=1`** — hien dang **BAT** (`platformio.ini:133`; **nho bo co khi dung that**):

```
[CYD] loop OK | link PUSH=<0/1> TRAM=<0/1> rx=<so frame> err=<so loi> heap=<byte>
```

(`src/main.cpp:56-58`, in **1 lan/giay**. Luc boot con in **reset reason** + free heap —
`src/main.cpp:35-36, 41, 45`.)

Doc bang:

```powershell
pio device monitor -p COM7 -b 115200
```

| Truong | Lay tu | Doc the nao |
|---|---|---|
| `link PUSH=` | `panel.linked` | **1** = da nhan duoc frame PUSH hop le (che do `PANEL_SERIAL_UI`) |
| `link TRAM=` | `tram.linked` | **1** = dang nhan duoc dong `TRAM` (che do `PANEL_TRAM_LINK`) |
| `rx=` | `panel.rx_frames` | so **frame PUSH hop le** da nhan — phai **tang deu** khi Marlin dang chay |
| `err=` | `panel.rx_errors` | so frame **loi** — **khong duoc tang** (tang = sai baud / nhieu day) |
| `heap=` | `ESP.getFreeHeap()` | RAM con lai — phai **on dinh**, khong tut dan qua thoi gian |

> ⚠️ O **che do TRAM** thi `rx`/`err` (dem frame PUSH) **co the dung o 0** vi Marlin chi gui dong
> ASCII `TRAM ...` chu khong gui frame PUSH. Luc do xem `link TRAM=1` va doc **man INFO** tren panel:
> `LINK: OK (TRAM) ...` + `RX: <n> frame / <n> loi` (`src/ui_lvgl.cpp:2013-2018`).

### Console gui lenh qua COM7 (giong het bam nut tren panel)

```powershell
pwsh -File D:\Marlin\serial-console.ps1 -Port COM7 -Baud 115200    # qua panel (mac dinh)
pwsh -File D:\Marlin\serial-console.ps1 -Port COM4 -Baud 250000    # gui thang vao Marlin qua USB
```

`serial-console.ps1` luon gui **CRLF** va bat **DTR** (`serial-console.ps1:23, 42-43`); moi dong go
vao duoc panel **chuyen tiep sang Marlin** bang dung duong ma cac nut tren UI dung
(`src/main.cpp:62-68, 73-88`). Go `exit` + Enter de thoat.

### Hieu chinh cam ung (chi board co touch — CYD)

* **Vao:** STATUS → **SETTINGS** → **MENU** → dong **`Calibrate touch (giu 1.5s)`** — **PHAI GIU
  TAY 1,5 giay roi NHAC** (`CAL_TOUCH_HOLD_MS 1500`, `src/ui_lvgl.cpp:2176`; trong luc giu cot phai
  doi `dang giu` → `NHAC TAY` — `:2288-2289`).
* **Duong thoat hiem:** **giu tay tren man hinh luc cap dien** — panel thay `T_IRQ` muc thap trong
  `ui_port_input_init()` va vao thang che do hieu chuan (dung khi gia tri cu qua te, khong cham trung
  duoc dong menu).
* **Cach lam:** lan luot **TREN-TRAI → TREN-PHAI → DUOI-PHAI → DUOI-TRAI**; moi goc: cham → **ca man
  hinh doi sang XANH** → **keo ngon tay sat goc roi NHAC TAY** (luc **mat cam ung** moi chot toa do,
  lay cuc tri min/max). Chi tiet + cho luu (NVS namespace `cydtouch`) o **§5.5**.

### Bang "kiem tra nhanh sau khi nap"

| # | Kiem tra | Cach xem | Dat |
|---|---|---|---|
| 1 | Link PUSH | log 1 Hz tren COM7 | `link PUSH=1` |
| 2 | Link TRAM (neu dung kieu TRAM) | log 1 Hz + man INFO | `link TRAM=1` / `LINK: OK (TRAM)` |
| 3 | Nhan frame | `rx=` trong log | **tang deu**, khong dung yen |
| 4 | Loi frame | `err=` trong log | **khong tang** (giu nguyen / 0) |
| 5 | RAM | `heap=` trong log | **on dinh** qua nhieu phut, **khong tut dan** |
| 6 | Chi bao tren man | thanh tieu de moi man (`link_state_now()`) | `LINK OK` (khong phai `LINK ERR` / `NO LINK`) |
| 7 | Cam ung | cham thu vao 1 nut | dung vi tri, khong leo truc |
| 8 | Lenh xuong Marlin | console COM7 → go `M115` | Marlin tra ve `Marlin 2.1.2 ...` |
| 9 | Phien ban 2 phia | man INFO (`proto` / `MARLIN fw`) | `PANEL_PROTO_VERSION` khop (`2`) — lech la phai nap lai **ca hai** board |

> ℹ️ Neu `LINK ERR` thay vi `NO LINK`: **co byte toi nhung khong thanh frame hop le** ⇒ nghi **sai
> baud / sai khung / nhieu day**, khong phai dut day. `NO LINK` + `rx=0` moi la **khong co tin hieu**
> (sai cong / chua chung GND / board tat) — xem bang xu ly su co o **§6** va bang chan doan o **§2.1**.

## Buffer serial (so lieu that, do 2026-10-13)
Baud **250000 8N1 = 25 000 B/s, 40 us/byte**. Frame PUSH toi da 198 B (`PANEL_MAX_PAYLOAD 192` + 6).
- **Marlin** (core STM32duino): mac dinh chi `SERIAL_RX_BUFFER_SIZE 64` / `SERIAL_TX_BUFFER_SIZE 64`
  (core chua `HardwareSerial.h:43-48`, tran an toan 256). Nay da ep len **256/256** bang build flag
  trong `ini/features.ini` (key `PANEL_SERIAL_UI`). 64 B chi du 2,56 ms dem -> khong du cho burst.
- **Panel ESP32**: core mac dinh RX 256 B (du ~10 ms). Nay dat **1024 B** bang `setRxBufferSize()`
  TRUOC `begin()` (`src/panel_config.h`, `PANEL_RX_BUFFER`) -> du ~41 ms, an toan khi LVGL giu vong lap
  vai chuc ms de ve man.
- **Burst thuc te** (mot lan goi, khong nhip): `REQ_SETTINGS` = 33 frame x 12 B = 396 B;
  `REQ_FULL` ~ 491 B; danh sach file 30 file ~ 1,9 kB; `REQ_MESH` 7 x 23 = 161 B.
- **Vi sao phai co hang doi TX phia Marlin**: `tx_send()` cu BO frame khi `availableForWrite() < total`
  (buffer 64 B, kha dung that 63 B) => 33 dong setting chi ~5 dong lot, frame ket thuc danh sach file
  co the mat (panel hien "(dang nhan)" mai). Nay `panel_serial.cpp` co hang doi 8 slot + `tx_drain()`
  ghi ra UART khi du cho (khong cho, khong delay) va cac nguon hang loat (SETTINGS/FILES/MESH) duoc
  bom 1 frame moi lan `onIdle()` => khong mat frame nao, do tre ~20-25 ms cho 33 dong.
- Tran RX thi **im lang** ca hai phia (khong co co tran o lop Arduino); phia Marlin `MarlinSerial.cpp`
  bo byte khi ring day, phia ESP32 driver bo byte khi het buffer => bieu hien la thieu du lieu chu
  khong bao loi. Vi vay moi nguong dem phai rong rai.

## Font 1-bit tu sinh (Adafruit GFXfont) - thay cho lv_font_conv
Vi sao: font sinh bang `lv_font_conv` (Consolas/Roboto Mono) **lam mat het chu** tren panel that
(xem cam bay 45 o README goc: dinh dang bitmap + don vi `adv_w`). Nay dung font 1-bit tu sinh:
- `tools/ttf2gfxfont.py` - TTF -> header C dung dinh dang Adafruit GFXfont, nhieu dai lien tuc
  (`--font --size --extra-ranges --name -o`, co `--yadvance`/`--threshold`/`--dump`).
- `src/gfx_font_robotomono.h/.c` - Roboto Mono **size 13 px**: `xAdvance 8`, `yAdvance 17`, `base_line 3`,
  5 dai (0x20-0xFF, 0x0102-0x0103, 0x0110-0x0111, 0x01A0-0x01B1, 0x1EA0-0x1EF9), 287 glyph co net,
  **2926 byte bitmap**, tong 5714 byte flash (nam o DROM). Du 100% dau tieng Viet.
- `src/lv_font_gfxfont.c/.h` - driver `lv_font_t` doc GFXfont (repack sang dang lien tuc cua LVGL) +
  `lv_font_gfxfont_selftest()` in ra COM7: box/adv/ofs/checksum cua 'A' '0' 'a' U+1EC7 U+0110.
- Cong tac `UI_MONO_FONT` trong `ui_lvgl.cpp`: 0 = Montserrat + Segoe (mac dinh an toan), 4 = mono GFX
  gan tung nhan (**dang dung**), 1/2 = mono kieu cu (**da biet lam mat chu**, giu de tham chieu).
- Doi font mono: sua DUY NHAT `UI_MONO_FACE` trong `ui_lvgl.cpp`.
- Kiem tra tren may: reset board roi doc COM7 -> khoi `[GFXFONT]` phai khop gia tri trong
  `tools/test_gfxfont_layout.py` (tool nay con so TUNG PIXEL voi FreeType: 0 lech cho ca tieng Viet).
