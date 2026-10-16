# Hướng dẫn màn hình CYD cho Marlin trên MKS Monster8 V2

Tài liệu này dành cho người mới, bám theo nhánh `mks-monster8-Voron-2.1.x` tại commit `e655ee2f`.

Tên project màn hình hiện nay là **`cydlcd/`**. Tên cũ `lcd_panel/`, các macro `PANEL_*` và các công cụ `panel_*` đã được đổi sang `cydlcd/`, `CYDLCD_*` và `cydlcd_*`.

Cấu hình đang dùng trong nhánh này là:

- màn **ESP32-2432S028R “CYD”** 2,8 inch, 320×240 ngang, cảm ứng XPT2046;
- giao diện **LVGL**;
- Marlin đẩy dữ liệu bằng giao thức **PUSH nhị phân phiên bản 4**;
- thẻ SD nằm ở **khe của màn hình CYD**; Marlin đọc và ghi sector qua UART;
- file G-code được tải lên bằng **web server WiFi trên chính màn hình CYD**;
- đường upload Bluetooth SPP và đường POST trực tiếp đến module ESP3D đã bị xóa.

> [!WARNING]
> Khi giao thức thay đổi, phải nạp lại **cả Marlin lẫn firmware `cydlcd`**. Hai bên khác phiên bản có thể vẫn sáng màn hình nhưng không đọc file, không in hoặc báo mất liên kết.

## 1. Hiểu kiến trúc trước khi đấu dây

Máy có hai bộ điều khiển độc lập:

1. **MKS Monster8 V2** chạy Marlin, điều khiển chuyển động, nhiệt, quạt, endstop và tiến trình in.
2. **ESP32 trên màn CYD** chạy firmware trong thư mục `cydlcd/`, tự vẽ giao diện, đọc cảm ứng, quản lý WiFi và truy cập thẻ SD gắn ở màn hình.

Hai board nói chuyện qua một UART hai chiều:

```text
Marlin / MKS Monster8 V2
        │
        │  PUSH protocol v4 + sector SD
        │  250000 baud, TX/RX/GND
        ▼
ESP32 / màn hình CYD
        │
        ├── LVGL + cảm ứng
        ├── khe thẻ SD
        └── WiFi + web upload
```

Marlin vẫn dùng toàn bộ lớp SD/FAT và các lệnh `M23`, `M24`, `M25`, `M28`, `M29`, `M30`, `M524`. Khác biệt là thao tác block 512 byte không đi tới SPI trên mainboard mà đi qua `DiskIODriver_CydlcdUART` đến khe SD của màn hình.

### Đa luồng trong firmware màn hình (từ 2026-10-15)

ESP32 có hai nhân. Firmware chia việc thành 4 task FreeRTOS để đường serial tới Marlin **không bao giờ bị chặn** — trước đây mọi thứ chạy trong một vòng lặp, một lần upload WiFi (20–30 giây) là serial chết hẳn:

| Task | Nhân | Ưu tiên | Việc duy nhất nó làm |
|---|---|---|---|
| `link` | 1 (một mình) | 20 | UART tới Marlin: nhận frame, gửi PONG/G-code, phục vụ sector SD cho Marlin |
| `ui` | 0 | 5 | **Nơi duy nhất** được gọi LVGL — vẽ màn hình 30 lần/giây, đọc cảm ứng |
| `net` | 0 | 4 | WiFi, web server (nhận file upload), thẻ TF |
| `log` | 0 | 2 | In log ra cổng USB (UART0) qua hàng đợi, không chặn ai |

Nhân 1 chỉ chạy mỗi task `link` nên dù UI đang vẽ hay WiFi đang bận, serial vẫn thông. Các task giao tiếp qua hàng đợi không chặn (queue): lệnh G-code, hành động UI, dòng log. Thẻ SD dùng chung qua một mutex — xem mục 8.

Giám sát treo: mỗi task tăng một bộ đếm heartbeat riêng. Task chẩn đoán 1 Hz kiểm tra — `link`/`ui` đứng quá 5 giây hoặc `net` đứng quá 30 giây (trừ lúc đang upload) thì board tự khởi động lại. Dòng log `[CYD] hb: link=… ui=… net=… log=…` cho biết nhịp của từng task.

### Ba kiểu giao tiếp có trong code

| Kiểu | Env màn hình | Dữ liệu | Dùng khi nào |
|---|---|---|---|
| G-code | `cydlcd_f411cc`, `cydlcd_f103c6` | Màn tự hỏi `M105`, `M114`, `M27` | Board STM32 ít RAM, UI đơn giản |
| PUSH | `cydlcd_f411cc_lvgl`, `cydlcd_f407vgz_lvgl`, `cydlcd_esp32` | Marlin đẩy frame đầy đủ | **Cấu hình đang dùng** |
| TRAM | env LVGL, Marlin bật `CYDLCD_TBT_LINK` | Chỉ trạng thái Tramming Wizard | Muốn giữ màn 12864 và chỉ thêm tramming |

PUSH và TRAM là hai chế độ loại trừ nhau. Tài liệu bên dưới tập trung vào **PUSH**, vì đây là đường duy nhất có đủ FILES, mesh, settings, prompt, WiFi và SD qua UART.

## 2. Phần cứng và nối dây

### 2.1 Cổng UART đang dùng

Cấu hình hiện tại trong `ini/features.ini` đặt:

```text
CYDLCD_SERIAL_PORT=1
USART1_RX = PA10
USART1_TX = PA9
baud = 250000
```

Đó là hai chân RX/TX trên header **MKS WiFi** của Monster8 V2. Không dùng `SERIAL_PORT_2` của Marlin, vì PA2/PA3 trên Monster8 đã có chức năng khác.

### 2.2 Nối MKS Monster8 V2 với CYD

| MKS Monster8 V2 | Màn hình CYD, connector P3 | Ý nghĩa |
|---|---|---|
| PA9 / USART1_TX | GPIO35 / RX | dữ liệu Marlin → màn hình |
| PA10 / USART1_RX | GPIO22 / TX | dữ liệu màn hình → Marlin |
| GND | GND | mass chung bắt buộc |

Quy tắc quan trọng:

- luôn nối **chéo TX → RX, RX ← TX**;
- luôn nối **GND chung**;
- cấp nguồn cho CYD qua USB hoặc nguồn 5 V phù hợp của board; không đưa 5 V vào chân 3,3 V;
- GPIO35 của ESP32 chỉ là input, nên được dùng đúng vai trò RX.

### 2.3 Chọn đúng connector trên CYD

CYD có nhiều đầu 4 chân trông gần giống nhau. Firmware hiện tại dùng `Serial2` trên **GPIO22/GPIO35**, vì vậy phải dùng **P3**:

| Connector | Chân liên quan | Dùng cho link Marlin? |
|---|---|---|
| P3 | GND, IO35, IO22, IO21 | Có; chỉ dùng GND, IO35, IO22 |
| CN1 | GND, IO22, IO27, 3.3V | Không theo cấu hình mặc định |
| P5/P1 cạnh USB-C | UART0 IO1/IO3 | Không |

IO21 thường là đèn nền TFT. Không nối nhầm dây UART vào IO21.

### 2.4 Test vòng lặp riêng màn hình

Khi nghi sai cổng hoặc hỏng UART:

1. tháo dây sang mainboard;
2. nối tạm IO22 với IO35 trên P3;
3. mở log USB của CYD ở 115200 baud;
4. nếu bộ đếm RX tăng, UART và đúng hai chân đang hoạt động.

`rx=0` và `err=0` thường là không có tín hiệu ở RX: sai connector, sai dây hoặc chưa chung GND. Sai baud thường làm bộ đếm lỗi tăng.

## 3. Chuẩn bị phần mềm và chọn đúng env

Cài:

- Git;
- Visual Studio Code và PlatformIO, hoặc PlatformIO Core;
- driver USB của CYD nếu máy chưa nhận cổng COM;
- công cụ nạp cho MKS Monster8 V2 theo cách đang dùng: USB DFU hoặc ST-Link.

Từ thư mục gốc repo, kiểm tra nhánh:

```powershell
git branch --show-current
git rev-parse HEAD
```

Kết quả mong đợi của bản tài liệu này:

```text
mks-monster8-Voron-2.1.x
9d6a107282a0127e834a5479a0c23dc08600cfaf
```

Marlin và `cydlcd` là **hai project PlatformIO riêng**:

- Marlin dùng các env trong repo chính;
- màn hình dùng `cydlcd/platformio.ini`;
- cờ build của project này không tự lọt sang project kia.

### Env màn hình

| Env | Board | UI / link |
|---|---|---|
| `cydlcd_esp32` | ESP32-2432S028R CYD | LVGL + PUSH, đang dùng |
| `cydlcd_f411cc_lvgl` | STM32F411CCU6 | LVGL + PUSH |
| `cydlcd_f407vgz_lvgl` | STM32F407VGT6 | LVGL + PUSH |
| `cydlcd_f411cc` | STM32F411CCU6 | UI nhẹ + G-code |
| `cydlcd_f103c6` | STM32F103C6T6 | UI nhẹ + G-code |

`default_envs` của project màn hình là `cydlcd_f411cc`, không phải CYD. Vì vậy khi build CYD phải luôn ghi rõ `-e cydlcd_esp32`.

## 4. Build và nạp firmware

### 4.1 Build và nạp màn hình CYD

Cắm USB vào CYD rồi chạy từ thư mục gốc repo:

```powershell
pio run -d cydlcd -e cydlcd_esp32
pio run -d cydlcd -e cydlcd_esp32 -t upload
```

Nếu PlatformIO chọn nhầm cổng, liệt kê thiết bị rồi chỉ rõ cổng:

```powershell
pio device list
pio run -d cydlcd -e cydlcd_esp32 -t upload --upload-port COM7
```

Sau khi nạp, mở log ở 115200 baud:

```powershell
pio device monitor -p COM7 -b 115200
```

Tên COM chỉ là ví dụ; dùng cổng thực tế trên máy.

### 4.2 Build và nạp Marlin

Cấu hình hiện tại đã bật:

```cpp
#define CYDLCD_SERIAL_UI
```

Và đã tắt màn 12864 cùng các menu MarlinUI xung đột. `ini/features.ini` tự thêm USART1 PA9/PA10 và buffer serial khi macro trên được bật.

Build Marlin:

```powershell
pio run -e mks_monster8
```

Nạp bằng quy trình hiện có của mainboard. Nếu dùng ROM DFU, nút **REBOOT DFU** trên trang CUSTOM gửi Marlin vào bootloader qua `M997`; sau đó dùng công cụ DFU trên PC.

> [!WARNING]
> Không thể nạp firmware Marlin từ thẻ SD đang nằm ở khe CYD. Bootloader STM32 không có driver SD-over-UART nên không nhìn thấy thẻ này. Hãy dùng USB DFU hoặc ST-Link.

### 4.3 Nếu muốn quay lại màn 12864

Script đổi chế độ nằm tại:

```powershell
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode status
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode off
pwsh cydlcd/tools/set_cydlcd_mode.ps1 -Mode on
```

- `on`: bật `CYDLCD_SERIAL_UI`, tắt màn 12864 và các mục xung đột;
- `off`: trả về cấu hình màn 12864;
- `status`: chỉ xem trạng thái.

Không bật đồng thời `CYDLCD_SERIAL_UI` và `CYDLCD_TBT_LINK`.

## 5. Kiểm tra lần đầu sau khi nạp

Làm theo đúng thứ tự để dễ khoanh vùng lỗi:

1. **Màn sáng và hiện UI.** Nếu trắng hoặc không có đèn nền, kiểm tra cấu hình TFT và chân backlight trước.
2. **Header báo LINK.** Nếu `NO LINK`, kiểm tra P3, dây chéo TX/RX, GND và baud 250000.
3. **Phiên bản protocol khớp.** Cả hai file `cydlcd_protocol.h` phải là phiên bản 4.
4. **Thẻ SD được nhận.** Cắm thẻ vào khe CYD, vào FILES và bấm REFRESH.
5. **FILES có danh sách.** Nếu trống, bấm `Init SD` hoặc REFRESH; không reset máy vội.
6. **Cảm ứng đúng vị trí.** Nếu lệch, hiệu chuẩn 4 góc.
7. **WiFi có IP.** Vào trang WiFi, chọn mạng 2,4 GHz và kết nối.
8. **Thử file nhỏ.** Upload, kiểm tra tên file, mở FILES, chọn rồi PRINT.
9. **Thử PAUSE, RESUME, STOP.** STOP có xác nhận và gửi đường lệnh ưu tiên để dừng chắc chắn.

Khi mọi bước trên đạt, mới thử file lớn hoặc chỉnh thông số máy.

## 6. Cách điều khiển chung

Màn hình ngang 320×240 có thanh tiêu đề, trạng thái link, biểu tượng WiFi/SD và nút BACK. Nội dung dài tự xuất hiện thanh PREV/NEXT ở đáy.

- chạm một nút để chạy;
- ô đang chọn có màu nhấn;
- BACK quay lại MENU; từ MENU, BACK về STATUS;
- biểu tượng WiFi mở trực tiếp trang WiFi;
- các thao tác phá huỷ như xóa file, reset EEPROM, quên WiFi hoặc vào DFU đều có xác nhận;
- thông báo trạng thái của web upload được lưu ở trang web `/status`, không bật hộp OK che màn hình LCD.

### Hiệu chuẩn cảm ứng

Có hai đường vào:

1. tại MENU, giữ **Calibrate touch** ít nhất 1,5 giây rồi nhả tay;
2. nếu cảm ứng lệch đến mức không bấm được menu, giữ tay trên màn lúc cấp nguồn để vào hiệu chuẩn.

Chạm đúng bốn điểm theo hướng dẫn. Kết quả lưu trong NVS nên không mất khi reset hoặc nạp lại firmware có cùng bảng phân vùng NVS.

## 7. Toàn bộ màn hình và chức năng

### 7.1 STATUS

STATUS mô phỏng bố cục TFT của Marlin:

- ba ô đầu in, bàn nhiệt và quạt;
- hàng tọa độ X/Y/Z với font lớn;
- feed rate `FR` và flow rate `FL`;
- icon thẻ SD, WiFi và SETTINGS;
- thời gian, tiến độ và dòng trạng thái cuối.

Chạm vào:

- ô đầu in hoặc bàn nhiệt → TEMPERATURE;
- X/Y/Z → JOG;
- FR/FL → FEED/FLOW;
- icon SD → FILES;
- icon WiFi → WiFi;
- SETTINGS → MENU.

Dải màu nhiệt độ dựa trên **nhiệt độ thực**, không dựa trên tỷ lệ so với target: lạnh xanh, ấm vàng, nóng chuyển sang đỏ. Icon heater đổi dạng đặc/viền theo trạng thái cấp nguồn.

### 7.2 MENU

MENU có 10 mục chính và một dòng hiệu chuẩn cảm ứng:

1. `Temperature`
2. `Motion`
3. `Feed/Flow rate`
4. `Print from Media`
5. `Bed mesh`
6. `Custom/Init SD`
7. `Configuration`
8. `Tramming wizard`
9. `Info`
10. `WiFi`
11. `Calibrate touch (giu 1.5s)` — chỉ hiện trên board có touch.

Chi tiết hiển thị hiện tại:

- dòng `Print from Media` (tên cũ `SD card`, đổi 2026-10-15 cho giống Marlin) **không còn đếm số file**;
- dòng `Bed mesh` cho biết kích thước lưới và `ON/OFF`;
- dòng `Configuration` mở cây menu con kiểu 12864 (xem 7.10), không còn là bảng phẳng;
- dòng `Motion` mở menu MOTION kiểu 12864 (Move Axis (JOG), Z Probe Wizard, Tramming, UBL, Level Bed,
  Deploy/Stow Z-Probe, M48, Endstops (M119), Disable Steppers);
- `Tramming wizard` hiển thị trạng thái phiên;
- `WiFi` hiển thị tắt, đang nối hoặc đã nối.

> **Mục `Tuning (MPC/PID)` đã bỏ (2026-10-15)**: tuning MPC/PID nay chỉ còn ở
> **Configuration → Advanced Settings → Temperature** (xem 7.11).

### 7.3 TEMPERATURE

Trang này chỉnh:

- target đầu in bằng các nút `-10`, `-1`, `+1`, `+10` → `M104`;
- target bàn nhiệt → `M140`;
- tốc độ quạt từ OFF đến MAX → `M106`;
- 6 nút vật liệu preheat xếp 2 hàng × 3 cột (chỉ hiện tên; bấm vào gửi nhiệt theo vật liệu đó);
- `COOLDOWN` → `M104 S0` và `M140 S0`.

Danh sách vật liệu preheat (tối đa 6) do người dùng tự thêm/bớt/đổi tên, mỗi vật liệu gồm tên (tối đa 8 ký tự) + nhiệt nozzle/bed/fan. Sửa trong `Configuration` → `Preheat`: chạm một vật liệu để mở hộp sửa 3-trong-1 (3 ô NOZ/BED/FAN trên cùng một hàng, chạm ô nào thì `-`/`+` chỉnh ô đó), có nút thêm vật liệu mới và xóa (không cho xóa hết). Dòng `Profile` cho biết vật liệu đang dùng. Toàn bộ lưu trong NVS của màn hình, tách khỏi EEPROM của Marlin.

### 7.4 MOTION (JOG)

Chọn trục X, Y hoặc Z rồi nhích:

- bước nhỏ: `0.01`, `0.05`, `0.1` mm;
- bước lớn: `1`, `10` mm;
- X/Y dùng tốc độ 3000 mm/phút;
- Z dùng 600 mm/phút.

Mỗi lần jog gửi `G91`, lệnh `G1`, rồi `G90`. Các nút khác:

- `HOME ALL (G28)`;
- `G28 X`, `G28 Y`, `G28 Z`;
- `M84` tắt stepper;
- `M851 Z` mở hộp sửa probe Z offset.

Chỉ jog khi vùng chuyển động an toàn. Với bước 10 mm, kiểm tra đầu in còn đủ khoảng trống.

### 7.5 FEED/FLOW RATE

- FEED dùng `M220`;
- FLOW dùng `M221`;
- mỗi giá trị có `-10`, `-1`, `+1`, `+10`;
- `RESET 100%` trả cả hai về 100%.

Trang tự xin lại settings định kỳ nên thay đổi từ console hoặc G-code cũng cập nhật lên màn hình.

### 7.6 SD CARD / FILES

Danh sách cố định **4 dòng mỗi trang**. PREV/NEXT lật trang; chạm một dòng chỉ chọn và tô sáng, chưa in ngay.

Thứ tự hiển thị:

1. ẩn các thư mục hệ thống được khai báo trong `CYDLCD_HIDE_DIRS`, hiện gồm `cydlcd_tmp` và `BTUP`;
2. thư mục đứng trước file;
3. trong mỗi nhóm, sắp xếp tên không phân biệt hoa/thường.

Các nút:

| Nút | Tác dụng |
|---|---|
| PRINT | chọn file G-code bằng `M23`, rồi chạy `M24` |
| DIR | vào thư mục đang chọn |
| UP | lên thư mục cha |
| REFRESH | `M22`, `M21`, rồi xin lại danh sách |
| PAUSE | `M25` |
| RESUME | `M24` |
| STOP | xác nhận rồi dừng bằng đường ưu tiên `M108` + `M524` |
| PREV/NEXT | lật từng trang 5 dòng |
| XOA | xóa file hoặc thư mục rỗng sau xác nhận |

Quy tắc an toàn:

- PRINT chỉ bật cho file G-code, không bật cho `.stl`, `.step`, `.txt` hoặc thư mục;
- khi đang in hoặc đang tạm dừng, PRINT và XOA bị khóa;
- trong lúc web đang nhận/ghi file, các thao tác cần truy cập SD bị chặn;
- thư mục chỉ xóa được khi rỗng;
- sau khi xóa hoặc upload, màn hình yêu cầu Marlin mount lại thẻ.

#### Vì sao phải dùng tên 8.3

Protocol v4 gửi cả tên dài để hiển thị và tên ngắn 8.3 để thao tác. Marlin/SdFat có thể cắt tên dài quá 26 ký tự khi gửi danh sách; nếu dùng chuỗi bị cắt cho `M23`, file thật không mở được. UI vì vậy:

- hiển thị tên dài cho dễ đọc;
- gửi PRINT, DIR và các thao tác nhạy tên bằng tên 8.3;
- nếu tên hiển thị bị cắt mất phần mở rộng, cố gắng ghép phần mở rộng từ tên 8.3.

Nếu vừa nạp một bên firmware mà bấm PRINT không chạy, kiểm tra đầu tiên là hai bên có cùng **proto 4** hay không.

### 7.7 BED MESH

Trang hiển thị lưới UBL toàn chiều rộng, số hàng đã nhận, min/max, `valid` và trạng thái `UBL:ON/OFF`.

Màu ô:

- cao hơn `+0.05 mm`: xanh;
- thấp hơn `-0.05 mm`: đỏ;
- nằm trong khoảng: màu nền thường.

Nút thao tác:

- `G29 P1`: đo mesh;
- `G29 P3T0`: điền các ô chưa có;
- `G29 P4RT`: fine tune;
- `UBL ON`: `G29 A`;
- `UBL OFF`: `G29 D`;
- `SAVE S0`: `G29 S0`.

Nếu mở trang mà chưa có lưới, màn hình tự gửi `CYDLCD_REQ_MESH` để xin lại, không cần reset.

#### Sửa trực tiếp từng ô

1. bấm `EDIT`; nút đổi màu và hiện bốn nút điều hướng;
2. dùng `<`, `^`, `v`, `>` để dịch vùng nhìn nếu lưới lớn;
3. chạm ô cần sửa;
4. chọn bước `0.01`, `0.05` hoặc `0.1` mm;
5. dùng `-`/`+`;
6. bấm `OK` để gửi `M421 I<cot> J<hang> Z<gia-tri>`; `HUY` để bỏ.

Giá trị bị giới hạn trong khoảng ±2 mm. Sau khi gửi, UI xin lại mesh để hiển thị giá trị thật từ Marlin.

### 7.8 CUSTOM G34/SD

Trang CUSTOM gom các chức năng không đủ chỗ ở MENU:

- `G34 Bed` → `G34 Q99`;
- `Init SD` → `M22` rồi `M21`;
- `ENDSTOP` → trang M119;
- `M600 B0` thay filament;
- `M412: BAT/TAT` bật hoặc tắt filament runout;
- `REBOOT DFU` → xác nhận hai bước rồi `M997`.

### 7.9 ENDSTOPS (M119)

Vào: **MENU → `Motion` → `Endstops (M119)`** — nằm **ngay sau `Stow Z-Probe (M402)`** (từ 2026-10-15; trước
đây là một dòng của MENU chính).

Trang hiển thị trạng thái các chân endstop/probe. Máy này Marlin chỉ báo **3 chân** —`PROBE`, `X min`, `Y min` —
nên trang chỉ hiện đúng 3 dòng; dòng nào bản build không có thì **tự ẩn** (trước đây hiện thêm `Z min`, `X max`,
`Y max` với giá trị `mo (open)` giả tạo).

- `mo (open)` = chưa chạm (chữ xanh); `CHAM (triggered)` = đang chạm (chữ đỏ).
- Giá trị **tự đọc lại 30 lần/giây**, **không còn nút `REFRESH`**.
- **Ra khỏi màn là ngừng đọc**: chỉ khi màn ENDSTOP đang mở panel mới xin dữ liệu M119; vào màn khác là dừng
  hẳn (không tốn đường truyền).
- Dòng `PROBE`: máy này dùng chung chân Z min cho đầu dò và bật `PROBE_ENABLE_DISABLE`, nên **khi CHƯA deploy
  đầu dò thì Marlin coi như `open`** (đúng như Marlin bỏ qua chân này trong lúc chưa bật đầu dò). Muốn thấy
  `CHAM` thì phải deploy đầu dò trước (`Motion → Deploy Z-Probe (M401)`, hoặc khi máy đang home trục Z).

### 7.10 CONFIGURATION (cây menu kiểu 12864)

Mục `Configuration` trong MENU mở một cây menu con giống màn 12864, thay cho bảng phẳng trước đây.

Thứ tự ở nhóm gốc `CAU HINH` (2026-10-15):

1. `Advanced Settings` — **dòng đầu tiên**; bên trong có `Max Speed`, `Acceleration`, `Jerk`,
   `Probe Offsets`, `Steps/mm`, `TMC Drivers`, `Temperature`, `Filament`, `Tune (runtime)`,
   `Home Offset X/Y`;
2. `Probe Z Offset` (`M851 Z`, mở hộp sửa);
3. `Fade Height` (`M420 Z`) — **ngay dưới `Probe Z Offset`**; đây là **chỗ duy nhất** có dòng này
   (đã bỏ khỏi `Advanced`, `Motion` và `UBL`);
4. `Controller Fan` (`M710` I/A/S/D);
5. `Preheat`: danh sách vật liệu (xem 7.3);
6. `Store Settings (M500)` · `Load Settings (M501)` · `Restore Defaults (M502)` (có xác nhận 2 bước);
7. `All settings (table)`: bảng phẳng cũ (trang THONG SO) với UP/DOWN, SAVE EEPROM, M502.

Nhóm `Advanced Settings → Temperature` là nơi **duy nhất** để tuning nhiệt: 6 tham số MPC (`M306`),
`MPC Autotune (M306 T)`, PID bàn (`M304`) + `Bed PID Autotune (M303)`, PID đầu in (`M301`) — xem 7.11.

Trong bảng `All settings`: hai cột `THONG SO` và `GIA TRI`; chạm một dòng để mở hộp sửa với bước, `-`, `+`, `OK`, `Hủy`. Dòng nào Marlin không gửi giá trị thì tự ẩn. Các thông số mới gồm Min Velocity/Min Travel Speed (`M205`), dòng TMC cho Z2/Z3 (`M906`), StealthChop 6 trục (`M569`), quạt điều khiển (`M710`).

- `SAVE EEPROM` yêu cầu Marlin chạy `M500`;
- `M502 RESET (ve mac dinh)` có xác nhận, chạy `M502` rồi lưu lại;
- không còn nút `M501 LOAD` vì nhãn “LOAD” dễ gây hiểu nhầm.

> [!WARNING]
> Thay đổi một giá trị thường chỉ đổi RAM của Marlin. Muốn giữ qua lần tắt máy, bấm **SAVE EEPROM** sau khi kiểm tra máy chạy đúng.

### 7.11 TUNING (MPC/PID)

⛔ **Trang “TUNING (MPC/PID)” đã bỏ (2026-10-15)** — tuning MPC/PID nay **chỉ còn một chỗ**:
**Configuration → Advanced Settings → Temperature**.

Vào: MENU → `Configuration` → dòng đầu `Advanced Settings` → `Temperature`. Trong nhóm đó có sáu giá trị MPC
thật từ Marlin (sửa được bằng `-` / `+` / `OK`):

- P: heater power;
- C: block heat capacity;
- R: sensor responsiveness;
- A: ambient transfer với fan 0;
- F: ambient transfer với fan tối đa;
- H: hệ số filament.

Các nút:

- `MPC Autotune (M306 T)`: autotune MPC đầu in;
- `Bed PID Autotune (M303)`: PID autotune bàn nhiệt ở 60 °C;
- `Bed PID-P/I/D`, `Hotend PID-P/I/D`: sửa tay thông số PID.

Muốn lưu kết quả: quay ra `Configuration` → `Store Settings (M500)`. Làm autotune khi máy an toàn, đầu in nguội
theo yêu cầu của Marlin và không có bản in đang chạy.

### 7.11b NGÔN NGỮ (đa ngôn ngữ)

Màn hình có **2 ngôn ngữ: Tiếng Việt và English** (mặc định là Tiếng Việt).

Đổi ngôn ngữ: **`Configuration` → `Advanced Settings` → `Language`** → bấm `Tiếng Việt` hoặc `English`.
Màn hình dựng lại ngay; lựa chọn được **lưu vào bộ nhớ trong** nên lần sau bật máy vẫn giữ nguyên.

- Hai dòng trong menu luôn ghi bằng **chính tên ngôn ngữ đó** (`Tiếng Việt`, `English`) — không dịch — để
  người đang ở ngôn ngữ nào cũng tìm được đường đổi lại.
- Đổi ngôn ngữ **không** ảnh hưởng máy in: không gửi lệnh nào sang Marlin, không đổi thông số.
- Vài chữ **cố ý không dịch** vì là ký hiệu/mã: `II`, `[]`, `ON`/`OFF`, `TBT`/`PUSH`, `NVS`, `MAX`/`MIN`, hay
  tên lệnh G-code (`M119`, `M500`…).
- Muốn kiểm tra từ PC: lệnh `UI LANG` (xem đang ở ngôn ngữ nào), `UI LANG 0|1` (đổi), `UI LANG?` (kiểm tra
  bảng chữ có thiếu dòng dịch nào không).

### 7.12 TRAMMING WIZARD

Trang hiển thị bốn góc theo thứ tự:

- BR: Back-Right;
- BL: Back-Left;
- FL: Front-Left;
- FR: Front-Right.

Dấu `*` đánh dấu góc đầu dò đang đứng. Marlin gửi độ lệch và hướng vặn vít; UI chỉ hiển thị và chuyển lệnh.

Nút:

- `HOME + PROBE 4 GOC`: khởi động lại wizard;
- `PROBE`: đo góc hiện tại;
- `MAX/MIN`: đổi cách chọn điểm tham chiếu;
- `BACK`: quay lại bước trước;
- `DONE`: kết thúc.

Chế độ MAX/MIN chỉ có hiệu lực trong phiên tramming hiện tại; lần chạy mới bắt đầu theo mặc định của firmware.

### 7.13 INFO

INFO hiển thị các dữ liệu chẩn đoán thật:

- phiên bản firmware màn hình và board;
- kiểu UI/font;
- trạng thái WiFi;
- protocol;
- phiên bản Marlin;
- link, byte RX;
- SD;
- thông tin hệ thống.

Dùng trang này để chụp lại trạng thái trước khi sửa dây hoặc nạp lại firmware.

### 7.14 WiFi

Trang WiFi hiển thị trạng thái, RSSI, SSID và IP. Các thao tác:

1. `QUET` tìm mạng; kết quả xếp theo RSSI, tối đa 8 mạng trên roller;
2. chọn mạng;
3. `MAT KHAU...` mở bàn phím trên env CYD; ký tự được che;
4. `KET NOI` kết nối và lưu cấu hình vào NVS;
5. `NGAT` ngắt nhưng giữ cấu hình;
6. `XOA` quên cấu hình sau xác nhận.

CYD ESP32 chỉ dùng WiFi 2,4 GHz. Không quét mạng khi Marlin đang in: firmware chủ động từ chối vì quét có thể làm radio đổi kênh và ảnh hưởng đường SD/web. Việc rời trang WiFi không làm mất kết nối hiện tại.

## 8. Thẻ SD qua UART

### 8.1 Cấu hình hiện tại

Trong `Marlin/Configuration_adv.h`:

```cpp
#define NO_SD_DETECT
#define CYDLCD_SD_OVER_UART
//#define CYDLCD_SD_READONLY
```

Ý nghĩa:

- Marlin không dựa vào công tắc detect trên mainboard;
- driver SD chuyển thao tác sector qua link PUSH;
- mặc định đang cho phép cả đọc và ghi;
- nếu chỉ muốn đọc, bỏ comment `CYDLCD_SD_READONLY` để Marlin thêm `SDCARD_READONLY`.

Protocol v4 dùng các frame `SDREQ`, `SDDATA`, `SDACK`. Một sector 512 byte được chia ra nhiều frame vì payload của một frame nhỏ hơn 512 byte.

Số đo trên máy thật của nhánh này:

- đọc sector qua UART khoảng **11–13 KB/s**;
- upload WiFi vào thẻ khoảng **150–210 KB/s**;
- vòng lặp chính của firmware khoảng **11 300 vòng/giây** (trước khi tối ưu đa luồng chỉ 0,5 vòng/giây vì bị chặn ở thẻ và WiFi).

Tốc độ SD qua UART thấp hơn SPI trực tiếp, nhưng vẫn đủ cho G-code thông thường. Nếu file có rất nhiều đoạn cực ngắn và planner bị đói dữ liệu, giảm độ chi tiết của G-code hoặc kiểm tra chất lượng link UART.

### 8.1a Thẻ dùng chung giữa các task — Marlin được ưu tiên

Thẻ SD ở khe màn hình vừa phục vụ Marlin (đọc sector khi in) vừa phục vụ màn hình (liệt kê file, web upload). Hai bên không được chạm thẻ cùng lúc nên có một mutex — mỗi lần chỉ một bên giữ thẻ.

Luật ưu tiên (theo yêu cầu người dùng):

- Khi Marlin đang dùng thẻ (có request sector trong 400 ms vừa qua), **màn hình tự nhường**: các thao tác thẻ của màn hình (liệt kê, xóa, web `/files`) bị hủy hoặc thử lại sau. Ngoại lệ duy nhất: đang có phiên upload thì upload chạy tiếp cho xong.
- Firmware phân biệt hai trạng thái trả về: `BUSY` (bận vì Marlin đang đọc/ghi → thử lại sau, **không phải lỗi**) và `ERR` (lỗi thật). Gặp `BUSY` thì đợi rồi thử lại, đừng coi như hỏng thẻ.
- Khi thẻ bị rút/mất, firmware thử mount lại với nhịp thưa dần: 2,5 → 5 → 10 → 20 → **30 giây** (thay vì thử dồn dập như trước). Cắm thẻ lại hoặc bấm REFRESH thì thử ngay lập tức.

### 8.2 Rút/cắm thẻ

Thẻ nằm ở màn hình nên CYD có thể phát hiện lại. Sau khi cắm lại:

1. chờ biểu tượng SD ổn định;
2. vào FILES;
3. bấm REFRESH hoặc CUSTOM → `Init SD`;
4. đợi danh sách mới.

Không rút thẻ khi đang in, đang upload hoặc đang xóa file.

### 8.3 Giới hạn bắt buộc nhớ

- mọi thao tác SD của Marlin phụ thuộc màn hình CYD còn sống và UART thông;
- reset/nạp lại màn hình trong lúc in có thể làm Marlin mất media;
- không thể dùng file `mks_monster8.bin` trong khe CYD để bootloader mainboard tự cập nhật;
- MUX SPI 74HC4053 chỉ còn là đường lui trong tài liệu kỹ thuật, **không phải cấu hình đang dùng**.

## 9. Upload G-code qua WiFi

Đường đang dùng:

```text
PC hoặc điện thoại
       │ WiFi / HTTP
       ▼
web server trên màn CYD
       │ ghi trực tiếp
       ▼
thẻ SD ở khe CYD
       │ M22 + M21, sau đó SD-over-UART
       ▼
Marlin
```

Đây là **đường upload duy nhất** trong code hiện tại. Bluetooth SPP và ESP3D không còn được build.

### 9.1 Dùng trình duyệt

1. kết nối CYD vào cùng mạng 2,4 GHz với PC/điện thoại;
2. mở `http://TBTCYDLCD.local/`;
3. nếu mDNS không phân giải, xem IP ở trang WiFi rồi mở địa chỉ IP đó;
4. nhập token đang cấu hình trong `CYDLCD_HTTP_TOKEN`;
5. chọn file và bấm gửi;
6. theo dõi tiến độ và thông báo cuối;
7. sau khi trạng thái hoàn tất, vào FILES và REFRESH nếu cần.

Trang web còn có danh sách file/thư mục ở gốc thẻ và nút xóa. Chỉ xóa được thư mục rỗng. Các request `/status` và `/files` chạy nối tiếp để không tạo hai luồng truy cập cùng lúc.

> [!WARNING]
> Token là thông tin xác thực. Không chụp hoặc gửi token cho người khác. Nếu đã lộ, thay `CYDLCD_HTTP_TOKEN`, build và nạp lại firmware màn hình.

### 9.2 Dùng script PowerShell

Công cụ tham chiếu:

```powershell
pwsh cydlcd/tools/cydlcd-upload.ps1 -File .\benchy.gcode
```

Ép hostname hoặc IP khi tự dò không được:

```powershell
pwsh cydlcd/tools/cydlcd-upload.ps1 -File .\benchy.gcode -Host <ip-hoac-hostname>
```

Đổi tên đích bằng `-Name`. Nếu token đã đổi, truyền giá trị bằng tham số `-Token` theo cách an toàn tại máy cục bộ, không ghi token vào tài liệu hoặc lịch sử chia sẻ.

Script tự dò thiết bị theo thứ tự: `-Host` (nếu có) → **địa chỉ MAC của màn hình** → mDNS → gateway → quét LAN → danh sách cố định. Từ 2026-10-15 script **dò theo MAC thay vì cache IP** (theo yêu cầu người dùng): IP do DHCP cấp có thể đổi, còn MAC WiFi thì cố định.

```powershell
# Đổi MAC nếu màn hình của bạn khác (mặc định 48-AF-F3-70-B1-F8)
pwsh cydlcd/tools/cydlcd-upload.ps1 -File .\benchy.gcode -Mac 48-AF-F3-70-B1-F8
```

Đo MAC thật của màn hình bằng: `arp -a | findstr <IP của màn hình>` (hoặc xem trên router). Tham số `-NoCache` vẫn giữ để tương thích nhưng không còn tác dụng.

Khi upload xong, màn hình tự gửi `M22` + `M21`; bình thường không cần `-InitSd`.

### 9.3 Quy tắc tên file

Biên web chỉ cho các ký tự trong whitelist mặc định: chữ cái, số, dấu chấm và gạch dưới. Không dùng khoảng trắng, dấu tiếng Việt hoặc ký tự lạ nếu không sửa `CYDLCD_NAME_ALLOW_EXTRA`.

Nên đặt tên ngắn, ví dụ:

```text
VORON_CUBE.GCO
PART_001.GCO
```

Tên ngắn giúp tránh khác biệt LFN/8.3 giữa trình duyệt, FAT và Marlin.

## 10. Gửi lệnh và đọc chẩn đoán từ PC

### 10.1 Console chuẩn

Dùng script ở gốc repo; script bật DTR và luôn gửi CRLF:

```powershell
# Qua USB của màn hình CYD
pwsh -File .\serial-console.ps1 -Port COM7 -Baud 115200

# Gửi thẳng vào Marlin qua USB
pwsh -File .\serial-console.ps1 -Port COM4 -Baud 250000
```

Đổi COM theo máy. Gõ `exit` để thoát. Các lệnh G-code nhập ở console CYD được chuyển tiếp qua đúng đường giống nút trên màn hình.

Lệnh thử an toàn:

```text
M115
M105
M114
M27
M119
```

Không thử lệnh chuyển động hoặc gia nhiệt nếu chưa đứng cạnh máy và chưa kiểm tra vùng an toàn.

### 10.2 Lệnh chẩn đoán riêng của CYD

| Lệnh | Mục đích |
|---|---|
| `UI ?` | liệt kê các trang |
| `UI <ten>` | mở một trang từ console |
| `UI BOX` | in cây widget, hộp thoại, trang và trạng thái liên quan |
| `UI RX` | thống kê frame theo loại, HELLO, PING/PONG và hai chiều link |
| `UI TEMPS` | so sánh nhiệt PUSH/TRAM và giá trị đang hiển thị |
| `UI GRAD` | in dải màu nhiệt mà không cần đốt nóng |
| `UI BUSY` | xem máy có đang in/tạm dừng và upload có bị chặn không |
| `UI SET` | in các setting Marlin đã gửi |
| `UI EDIT <id>` | mở hộp sửa setting theo ID |
| `UI MCELL <cot> <hang>` | mở hộp sửa một ô mesh |
| `UI TAP ...` | mô phỏng chọn file, PREV/NEXT, PRINT, STOP, XOA |
| `UI PERF` | đo hiệu năng: số vòng UI/giây, thời gian vẽ, pixel đã đẩy qua SPI, thời gian các chặng WiFi/thẻ/log |
| `UI CFG [nhom]` | in cây menu Configuration / mở một nhóm và liệt kê các dòng |
| `UI HIT <k>` | bấm nút thứ k (mô phỏng chạm) — dùng khi hộp sửa đang mở |
| `UI BACK` | nút BACK (đóng lớp phủ đang mở trước, rồi mới lùi nhóm) |
| `WIFI?` | trạng thái WiFi, IP, RSSI và heap |
| `PRE?` | in danh sách vật liệu preheat |

`UI RX` là lệnh quan trọng nhất khi màn vẫn cập nhật nhiệt nhưng bấm nút không chạy:

- STATUS/TEMPS tăng nhưng PONG không có → chiều Marlin → màn còn tốt, chiều màn → Marlin có vấn đề;
- không có HELLO → màn không biết khả năng Marlin, mesh/settings có thể trống;
- `err` tăng → nghi sai baud hoặc nhiễu dây;
- PONG bình thường dưới khoảng 1,5 giây → đường hai chiều đang khỏe;
- dòng `TX cho=/bo tong=/BO LENH (G-code)=` cho biết **phía màn hình có gửi được lệnh hay không**:
  `BO LENH (G-code) = 0` nghĩa là mọi lần bấm nút đều đã đẩy được frame lệnh ra UART (lỗi nằm ở phía sau:
  dây, RX của Marlin, hoặc hàng đợi lệnh của Marlin). Khác `0` nghĩa là có lần bấm **không gửi được** — xem
  `TX cho=` (hàng đợi TX đang nghẽn) và log `[UP] CANH BAO: TX kin qua …`.

Khi bấm nút mà máy không chạy, đối chiếu **3 bước** (không đoán):

| Bước | Xem ở đâu | Kết luận |
|---|---|---|
| 1 | `UI RX` (COM8) | `BO LENH (G-code)=0` ⇒ màn hình đã gửi lệnh |
| 2 | `PANEL DIAG` (COM4, 5 giây/lần): `LENH tu man hinh: nhan=` | Tăng khi bấm nút ⇒ Marlin **có** nhận lệnh. Không tăng (dù bước 1 = 0) ⇒ **RX của Marlin kẹt** (chờ watchdog tự khởi tạo lại UART sau 10 giây) |
| 3 | `PANEL DIAG`: `xep=` / `cho lai=` / `bo=` | `nhan=` tăng mà `xep=` không tăng ⇒ **hàng đợi lệnh của Marlin đầy** (đang in từ thẻ; `BUFSIZE = 4` lệnh). Lệnh được giữ lại và thử lại tới 20 giây; `bo=` tăng nghĩa là hàng đợi bị kẹt thật (ví dụ đang chờ nóng) |

Mỗi lệnh nhận được từ màn hình đều in một dòng `PANEL: lenh [G28]` ra COM4 — đây là **bằng chứng trực tiếp**
rằng frame đã tới Marlin và đã được giải mã đúng (không còn ký tự điều khiển lạ ở đầu lệnh).

Marlin có watchdog RX: quá 10 giây không nhận frame sẽ khởi tạo lại UART, **tối đa 1 lần mỗi 60 giây** (giới hạn này để tránh khởi tạo lại liên tục ngay sau khi nạp lại firmware màn hình). Màn hình cũng tiếp tục xin HELLO cho đến khi nhận được, thay vì chỉ thử vài lần rồi bỏ.

### 10.3 Log 1 Hz của CYD

Khi build có `CYDLCD_CYD_DEBUG=1`, mở monitor 115200 để xem:

```text
link PUSH=...
rx=...
err=...
heap=...
hb: link=... ui=... net=... log=...
```

- `link PUSH=1`: đã nhận frame PUSH hợp lệ;
- `rx` phải tăng đều khi Marlin chạy;
- `err` không nên tăng;
- `heap` phải ổn định qua thời gian;
- `hb:` là heartbeat của từng task (số lần mỗi task chạy trong 1 giây) — task nào đứng yên quá 5 giây (link/ui) hoặc 30 giây (net, trừ lúc upload) thì board tự khởi động lại.

Thời gian các chặng trong log đo bằng **micro giây** (µs), không phải mili giây như trước. Cờ debug đang phục vụ chẩn đoán. Khi máy đã ổn định, có thể tắt để giảm log và kích thước firmware.

## 11. Dừng in an toàn

STOP không chỉ gửi một `M524` thông thường. Firmware xử lý lệnh critical để vẫn dừng được khi Marlin đang chờ nhiệt hoặc hàng đợi đầy:

1. UI yêu cầu `M108` để thoát vòng chờ;
2. gửi `M524` để hủy bản in;
3. gửi lại theo chu kỳ 400 ms, tối đa khoảng 6 giây cho tới khi trạng thái in biến mất;
4. phía Marlin xử lý `M108`, `M524`, `M410` theo đường ưu tiên, không chờ hàng đợi G-code bình thường;
5. target heater được đưa về 0 ngay; quickstop chuyển động được thực hiện ở ngữ cảnh vòng lặp an toàn.

Nếu bản in đứng ở `M190` vì bàn không nóng được, bấm STOP. Không reset màn CYD trước, vì việc đó đồng thời làm mất đường SD của Marlin.

## 12. Xử lý sự cố theo triệu chứng

| Triệu chứng | Kiểm tra | Cách xử lý |
|---|---|---|
| Màn trắng, không đèn nền | env và TFT setup | build đúng `cydlcd_esp32`; kiểm tra `USER_SETUP_LOADED`, driver ST7789 và chân BL |
| Có UI nhưng `NO LINK` | P3, TX/RX, GND | nối chéo GPIO22/35 với PA10/PA9; test loopback |
| `LINK ERR` | `err` trong log | kiểm tra baud 250000, nhiễu, hai firmware cùng proto 4 |
| Nhiệt/tiến độ chạy nhưng nút không tác dụng | `UI RX` | xem PONG; reset/nạp lại Marlin nếu RX của mainboard không tự phục hồi |
| Mesh trắng | `UI RX`, HELLO, `CYDLCD_HAS_MESH` | chờ resync, mở lại BED MESH; UI sẽ tự xin mesh |
| FILES trống | icon SD, REFRESH, log sector | cắm lại thẻ, `M22`/`M21`, kiểm tra link hai chiều |
| Thấy file nhưng PRINT không chạy | proto và tên 8.3 | nạp cả hai board proto 4; chọn đúng file G-code |
| PRINT bị mờ | loại mục/trạng thái in | file không phải G-code, đang in/tạm dừng hoặc web đang upload |
| XOA bị mờ | trạng thái in | STOP bản in trước; chờ upload hoàn tất |
| Không xóa được thư mục | số mục con | chỉ xóa được thư mục rỗng |
| SD read error giữa bản in | `err`, PONG, quét WiFi | không quét mạng khi in; rút ngắn dây; kiểm tra buffer và nguồn |
| Upload bị từ chối | `UI BUSY`, trang `/status` | dừng bản in từ SD; chờ phiên upload/xóa trước kết thúc |
| Không mở được host mDNS | trang WiFi | dùng IP đang hiển thị; đảm bảo cùng LAN 2,4 GHz |
| Upload xong không thấy file | trạng thái done, REFRESH | đợi hoàn tất ghi/merge, rồi mount lại SD |
| Tên file sai hoặc thiếu đuôi | tên upload | dùng chữ/số/dấu chấm/gạch dưới và tên ngắn |
| Cảm ứng lệch | calibration | giữ dòng Calibrate touch 1,5 giây hoặc giữ tay khi boot |
| Hộp sửa không thấy | `UI EDIT`, `UI BOX` | xác nhận đang chạy firmware mới; hộp phải nằm trên page |
| Bấm ô mesh không mở sửa | EDIT, `UI MCELL` | bật EDIT; thử `UI MCELL`; kiểm tra touch tới table |
| Settings ít hơn dự kiến | `UI SET` | Marlin chỉ gửi mục tính năng đang có; số trên MENU là số đã nhận |
| Màu nhiệt sai | `UI TEMPS`, `UI GRAD` | kiểm tra đơn vị °C×100 và firmware màn mới |
| Màn CYD tự reboot | log reset/chặng/heap | kiểm tra nguồn, watchdog, heap và thao tác đang chặn vòng lặp |
| Marlin reboot khi mở cổng USB | DTR | không mở console Marlin giữa lúc in; mở DTR có thể reset board |

### Thẻ và định dạng

Dùng thẻ FAT16/FAT32 ổn định. Nếu thẻ chập chờn:

1. sao lưu file;
2. format FAT32;
3. chép một file G-code tên ngắn;
4. cắm vào CYD;
5. REFRESH và thử đọc trước khi upload file lớn.

### Dây và nguồn

Nếu lỗi frame chỉ xuất hiện khi heater/quạt hoạt động:

- đi dây UART xa dây heater và motor;
- dùng dây ngắn, xoắn TX/GND và RX/GND nếu có thể;
- bảo đảm mass chung chắc chắn;
- kiểm tra nguồn 5 V cấp CYD không sụt áp;
- không nối nhầm IO21 backlight.

## 13. Các chế độ thay thế

### 13.1 TRAM giữ màn 12864

Nếu chỉ cần trang Tramming Wizard trên CYD nhưng muốn giữ nguyên màn 12864, tắt PUSH rồi bật:

```cpp
#define CYDLCD_TBT_LINK
```

TRAM dùng link ASCII nhỏ trên USART1, không có FILES, mesh, SETTINGS, WiFi upload tích hợp hay SD-over-UART đầy đủ. Không bật cùng `CYDLCD_SERIAL_UI`.

### 13.2 UI G-code cho board STM32 nhỏ

Hai env `cydlcd_f411cc` và `cydlcd_f103c6` dùng driver ILI9341 nhẹ, hỏi Marlin bằng G-code. Chúng phù hợp khi tài nguyên hạn chế nhưng không có dữ liệu nặng như danh sách file, mesh, settings và prompt. Chân màn hình/encoder của biến thể STM32 nằm trong `cydlcd_config.h` và `platformio.ini`; chúng không phải sơ đồ của board CYD ESP32.

### 13.3 MUX SPI chỉ là đường lui

Code và tài liệu kỹ thuật vẫn giữ phương án 74HC4053 để chia sẻ thẻ bằng SPI, nhưng cấu hình hiện tại đã chuyển sang `CYDLCD_SD_OVER_UART`. Không lắp MUX nếu đang làm theo tài liệu này.

## 14. Bản đồ mã nguồn để tự sửa

### Phía màn hình

| File/thư mục | Vai trò |
|---|---|
| `cydlcd/platformio.ini` | env, board, thư viện, cờ build |
| `cydlcd/src/main.cpp` | setup/loop, console, watchdog |
| `cydlcd/src/app/cydlcd_config.h` | chân, buffer, WiFi, HTTP, SD |
| `cydlcd/src/app/cydlcd_protocol.h` | frame protocol v4 dùng chung |
| `cydlcd/src/app/cydlcd_link.cpp` | PUSH RX/TX, PING/PONG, HELLO |
| `cydlcd/src/app/cyd_sd.cpp` | thẻ ở khe CYD, sector, upload |
| `cydlcd/src/net/net_wifi.cpp` | quản lý WiFi và NVS |
| `cydlcd/src/net/net_upload_http.cpp` | web server, upload, status, files, delete |
| `cydlcd/src/ui/ui_lvgl.cpp` | khung UI, điều hướng, action chung |
| `cydlcd/src/ui_page_*.cpp` | từng trang giao diện |
| `cydlcd/src/ui/ui_preheat.cpp` | preset PLA/PETG/ABS trong NVS |
| `cydlcd/src/ui/port/ui_port_cyd_st7789.cpp` | TFT/touch của CYD |
| `cydlcd/tools/cydlcd-upload.ps1` | client upload Windows |

UI đã được tách trang; khi sửa một chức năng, mở đúng `ui_page_*.cpp` thay vì tìm mọi thứ trong một file lớn.

### Phía Marlin

| File | Vai trò |
|---|---|
| `Marlin/Configuration.h` | bật `CYDLCD_SERIAL_UI` hoặc `CYDLCD_TBT_LINK` |
| `Marlin/Configuration_adv.h` | `CYDLCD_SD_OVER_UART`, read-only, NO_SD_DETECT |
| `ini/features.ini` | chọn USART1 PA9/PA10 và buffer |
| `Marlin/src/lcd/extui/cydlcd_serial/` | ExtUI PUSH, settings, files, mesh, lệnh |
| `Marlin/src/sd/cydlcd_sd_uart.cpp` | driver block SD qua UART |
| `Marlin/src/feature/cydlcd_tbt_link.cpp` | link TRAM ASCII |
| `Marlin/src/feature/tbt_report.cpp` | trạng thái Tramming Wizard |

Hai bản `cydlcd_protocol.h` phải giữ cùng `CYDLCD_PROTO_VERSION`, mã frame, cấu trúc payload và ID setting. Phần chú thích có thể khác nhau.

### Bảng tên cũ và mới

| Tên cũ | Tên mới |
|---|---|
| `lcd_panel/` | `cydlcd/` |
| `panel_config.h` | `cydlcd_config.h` |
| `panel_protocol.h` | `cydlcd_protocol.h` |
| `panel_link.*` | `cydlcd_link.*` |
| `PANEL_SERIAL_UI` | `CYDLCD_SERIAL_UI` |
| `PANEL_TRAM_LINK` | `CYDLCD_TBT_LINK` |
| `PANEL_SD_OVER_UART` | `CYDLCD_SD_OVER_UART` |
| `set_panel_mode.ps1` | `set_cydlcd_mode.ps1` |
| `panel-upload.ps1` | `cydlcd-upload.ps1` |

Không dùng hướng dẫn cũ có `panel_*` để sửa code hiện tại.

## 15. Font và hiển thị

UI mặc định dùng font mono tự sinh 4 bpp có đủ dấu tiếng Việt, cộng font icon WiFi/SD riêng. Font tiêu đề giữ Montserrat. Công cụ sinh font là `cydlcd/tools/ttf2gfxfont.py`; các font từng sinh bằng `lv_font_conv` đã bị loại bỏ.

Nếu ép `UI_MONO_FONT=0`, LVGL chỉ dùng Montserrat 14 và một số chữ tiếng Việt có thể hiện ô vuông. Vì vậy bản dùng thực tế nên giữ chế độ font mặc định.

Màn ngang có vùng nội dung 206 px. Các trang dài dùng cuộn hoặc PREV/NEXT; FILES dùng phân trang cố định, không dùng cuộn vòng cho tên file.

## 16. Checklist trước một bản in thật

- [ ] CYD chạy firmware `cydlcd_esp32` mới.
- [ ] Marlin và màn hình cùng protocol 4.
- [ ] `LINK OK`, `rx` tăng, `err` không tăng.
- [ ] PONG trong `UI RX` còn mới.
- [ ] Thẻ SD nằm trong khe CYD và FILES đọc được.
- [ ] WiFi có IP nếu cần upload.
- [ ] File upload đã báo hoàn tất, không còn trạng thái receiving/writing.
- [ ] Tên file ngắn, đúng phần mở rộng G-code.
- [ ] UBL ON/OFF đúng nhu cầu và mesh hợp lệ.
- [ ] Target nhiệt, quạt, feed và flow đúng.
- [ ] Không còn hộp xác nhận hoặc phiên xóa/upload dở.
- [ ] STOP đã được thử với file nhỏ.

Nếu có lỗi, giữ nguyên trạng thái máy, chụp trang INFO và lấy `UI RX`, `UI BUSY`, `UI BOX` trước khi reset. Ba kết quả này thường đủ phân biệt lỗi UI, lỗi link, lỗi trạng thái Marlin và lỗi SD.
