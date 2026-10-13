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
| **PUSH** (`PANEL_SERIAL_UI`) | USART3 | PD9 (RX) / PD8 (TX) = EXP1 chân 5/6 | chân nằm ngay header màn 12864 cũ |
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
