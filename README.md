# Marlin 2.1.2 — MKS Monster8 V2 / Voron 300

Firmware Marlin cho máy in **Voron 300 (bàn 310×310, vùng in 305×305)** dùng board
**MKS Monster8 V2**, **3 motor trục Z** với vít me SFU1204, **Voron Tap** (probe trùng
vị trí nozzle) và **bàn 4 lò xo chỉnh vít**.

| | |
|---|---|
| Nhánh | `mks-monster8-Voron-2.1.x` |
| PlatformIO env | `mks_monster8` |
| Phiên bản Marlin | **2.1.2** (cố ý giữ nguyên — không lên 2.1.2.8) |
| Firmware đang chạy | `M115` → `Marlin 2.1.2` |

---

## 1. Phần cứng

| Thành phần | Chi tiết |
|---|---|
| Board | MKS Monster8 **V2**, STM32F407VGT6 @168MHz, 128KB RAM + 64KB CCM, 1MB flash |
| Màn hình | MKS Mini 12864 V3 (LCD đồ hoạ DOGM/U8glib) + rotary encoder + khe SD |
| Driver | **TMC2209 ×6 — chế độ UART** (X, Y, Z, Z2, Z3, E) |
| Dòng driver | **400 mA RMS** mỗi driver |
| Microstep | **16** cho cả 6 driver, có nội suy (interpolation) + stealthChop |
| Nguồn | PSU máy in nuôi motor/logic, USB chỉ để giao tiếp |

Chân driver theo socket:

| Socket | Driver0 | Driver1 | Driver2 | Driver3 | Driver4 | Driver5 | Driver6 |
|---|---|---|---|---|---|---|---|
| Trục | X | Y | **Z** | E0 | **Z2** | **Z3** | *(trống)* |

> ⚠️ **Luôn cấp nguồn PSU khi kiểm tra endstop.** Chạy chỉ bằng USB (không có VM) làm
> TMC2209 ở trạng thái undervoltage/chưa cấu hình → **kéo cứng các đường endstop lên HIGH**.
> Triệu chứng: mọi endstop báo `TRIGGERED` và bấm switch không thay đổi gì, dù firmware đúng.
> `M122` khi đó sẽ cho `msteps 256`, `off time 0`, `stealthChop false` thay vì `16 / 3 / true`.

---

## 2. Bàn in, cơ khí & hệ toạ độ

| | |
|---|---|
| Tấm bàn | **310 × 310 mm**, **4 lò xo + vít chỉnh M4** (bước ren 0.7 mm) |
| Trục Z | **3 motor** (Z / Z2 / Z3), vít me **SFU1204** — bước ren **4 mm/vòng** |

### 2.1 Gốc toạ độ & vùng in được (đo thực tế)

**Gốc `(0, 0)` nằm đúng góc trước-trái của bàn in. Vùng in được là hình vuông `305 × 305`.**

| Trục | `MIN_POS` | `MAX_POS` | Hành trình | Cách xác định |
|---|---|---|---|---|
| X | **−6** | **305** | 311 | Jog nozzle tới mép trái bàn → toạ độ đọc `0` với `X_MIN_POS −6` → gốc X đúng |
| Y | **−22** | **305** | 327 | Jog tới mép trước bàn → toạ độ đọc **`−13`** (không phải 0) → gốc lệch 13mm → `−35 + 13 = −22`. `MAX_POS 305` cho vùng in bằng đúng trục X; khung chặn ở **313** nên còn dư 8mm |
| Z | 0 | 310 | 310 | |

Kiểm chứng bằng `G1` vượt biên (soft endstop `M211 S1` đang bật):

```
G1 X900   →  M114: X:305.00
G1 Y900   →  M114: Y:305.00
G1 Y-900  →  M114: Y:-22.00
```

> **`MIN_POS` là hằng số hiệu chuẩn, không phải nút chỉnh kích thước.** Nó nói mép trước-trái
> của bàn nằm ở đâu so với vị trí home. Trừ `MIN_POS` đi 1 để thu nhỏ vùng in sẽ **dịch luôn
> gốc toạ độ** → mọi bản in lệch đi 1mm. Muốn cắt bớt vùng in thì cắt ở `MAX_POS`.
> Đổi `MAX_POS` không dịch hệ toạ độ: điểm `Y=100` vẫn nằm đúng chỗ cũ, chỉ có đầu cuối hành
> trình ngắn lại (màn hình hiện `305` thay vì `306`).

> Tấm bàn là **310×310** nhưng chỉ khai báo **305×305**: dải `X 305→310` và `Y 305→310`
> nằm ngoài tầm với thật của đầu in.

### 2.2 Vì sao `BED_SIZE` = 305 mà không phải 310

Ba ràng buộc của Marlin — vi phạm là **build fail**, không phải lỗi im lặng:

| Ràng buộc | Nguồn | Hệ quả |
|---|---|---|
| `MAX_POS − MIN_POS ≥ BED_SIZE` | `SanityCheck.h:839` | X: `305 − (−6) = 311 ≥ 305` ✓ · Y: `305 − (−22) = 327 ≥ 305` ✓ |
| Probe bị chặn bởi `BED_SIZE − PROBING_MARGIN` | `probe.h:220-236` | probe max = `min(305 − 15, MAX_POS)` = `min(290, 305)` = **290** → đây chính là lý do `MESH_INSET` phải bằng 15 |
| Mọi điểm đo phải tới được | `tramming.h:36`, `z_stepper_align.cpp:57` (`static_assert`) | **Lưới an toàn**: đổi giới hạn trục mà quên dời điểm đo → build báo lỗi ngay |

---

## 3. Thông số chuyển động

| Thông số | Giá trị | Nguồn |
|---|---|---|
| Steps/mm | `M92 X80 Y80 Z800 E415` | 200 bước/vòng × 16 microstep ÷ 4 mm |
| Max feedrate (mm/s) | `M203 X500 Y500 Z10 E25` | |
| Accel (mm/s²) | `M201 X500 Y500 Z100 E1000` | |
| Accel print/retract/travel | `M204 P500 R500 T1000` | |
| Jerk | `M205 X10 Y10 Z0.40 E5` | |
| Homing feedrate | X/Y 3000, Z **480** mm/min | Z = 8 mm/s < max 10 mm/s |
| Soft endstop | `M211 S1` | |

---

## 4. Đầu dò Z

Cảm biến **Voron Tap** — probe **chính là nozzle**, kèm mạch enable qua chân PA8.

| | |
|---|---|
| Tín hiệu probe | **PB13** (header `Z-`), `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN` + `USE_PROBE_FOR_Z_HOMING` |
| Enable probe | **PA8** (`PROBE_ENABLE_PIN`, header servo) — `1` = bật mạch cảm biến, `0` = tắt |
| Điều khiển | **`M401`** = deploy (PA8 HIGH) · **`M402`** = stow (PA8 LOW) |
| Offset nozzle→probe | `M851 X0.00 Y0.00 Z0.00` — **XY = 0 vì probe là chính nozzle (Voron Tap)** |
| Cân Z offset | **`Motion` → `Probe Offset Wizard`** (hoặc `Advanced Settings` → `Z Probe Offsets` → `Probe Offset Wizard`) |
| `PROBING_MARGIN` | 15 mm |
| Logic | `Z_MIN_ENDSTOP_INVERTING false` |

> ⚠️ **Offset XY phải là 0.** Đây từng là `Y−25` và đó là gốc của một loạt sai số: mọi điểm
> mesh / G34 / G35 đều bị dịch 25mm theo Y, tầm probe bị tính hụt còn `MAX_POS − 25`, và
> `MESH_INSET` bị đẩy lên 25 để bù. Với Voron Tap thì `probe = nozzle`, nên XY = 0.
> Giá trị này **nằm trong EEPROM** — sửa `Configuration.h` thôi không đủ, phải `M851 X0 Y0` + `M500`.

> 🔴 **Offset Z sai có thể đâm nozzle vào bàn.** `motion.cpp:2349` chạy
> `current_position.z -= probe.offset.z` sau khi home. Với `Z-3.35` (số cũ của cảm biến trước),
> lúc probe trigger — tức nozzle **đang chạm bàn** — firmware lại tưởng Z = **+3.35**, nên
> `G1 Z0.2` sẽ đẩy nozzle **3.15 mm xuyên xuống bàn**. Đã đặt lại `M851 Z0` + `M500`;
> **chạy `Probe Offset Wizard` để lấy số chính xác trước khi in.**

> Probe chỉ có tín hiệu khi **đã deploy** (`M401`). Khi stow thì `z_min` không phản ánh bàn.
> `G35` và các wizard tự deploy/stow, không cần `M401` tay.

---

## 5. Cân bàn & căn gantry

| Tính năng | Cấu hình |
|---|---|
| Leveling | **UBL (Unified Bed Leveling)** |
| Mesh | **7 × 7 = 49 điểm**, `MESH_INSET 15` → phủ **`(15,15)` … `(290,290)`** |
| Fade height | 10 mm |
| Trạng thái | `M420 S0` — **leveling đang TẮT** cho tới khi tạo mesh |
| Căn gantry | `Z_STEPPER_AUTO_ALIGN` (**G34**) — 3 điểm `{280,285} {25,285} {25,25}` |
| Tram bàn | `ASSISTED_TRAMMING` (**G35**) + Tramming Wizard — 4 điểm góc |

**Vì sao `MESH_INSET` = 15:** `MESH_INSET` phải nằm trong tầm probe. Probe bị chặn bởi
`BED_SIZE − PROBING_MARGIN = 305 − 15 = 290` (và trần trục là 305, cao hơn), nên probe
tới được **`X 15…290`** và **`Y 15…290`**. `15` khớp đúng giới hạn đó → lưới 7×7 phủ trọn
`(15,15)`–`(290,290)`, **mọi điểm đều đo thật**, không phải nội suy `G29 P3`.

Kiểm chứng: `M420 V` in ra đúng góc `( 15, 15)` và `(290,290)`.

### G35 / Tramming Wizard đo bằng gì?

**Bằng chính probe đó — mỗi lần đo tại mỗi góc là một lần probe thật.** Không phải đo bằng
mắt hay bằng thước.

| | |
|---|---|
| Đo tại đâu | 4 điểm `TRAMMING_POINT_XY` = `{280,285} {25,285} {25,25} {280,25}` — phải **nằm trên 4 vít** |
| Đo bằng gì | `probe.probe_at_point(tramming_points[i], …)` — `G35.cpp:112`, `menu_tramming.cpp:58` |
| Tính gì | `diff = z_điểm_0 − z_điểm_i`, rồi `số_vòng = diff ÷ bước_ren` |
| Bước ren | `TRAMMING_SCREW_THREAD 40` → M4 × **0.7 mm/vòng** (`threads_factor = {0.5 M3, 0.7 M4, 0.8 M5}`) |
| Kết quả | `Turn <góc> CW/CCW by N turns and M minutes (x.xx mm)` |

**Cảm biến được bật/tắt tự động** — không cần `M401` trước:
`probe_at_point()` → `deploy()` → `Probe::set_deployed()` → `Endstops::enable_z_probe(true)`
→ `WRITE(PROBE_ENABLE_PIN, on)` (`endstops.cpp:466`), tức chân **PA8 lên HIGH**. G35 kết thúc
bằng `probe.stow()` (`G35.cpp:167`) → PA8 về LOW.

Khác nhau giữa hai cách:

**Menu trên máy: `Motion` → `Tramming Wizard`.** (`LCD_LANGUAGE en` nên nhãn là tiếng Anh —
`MSG_TRAMMING_WIZARD` = *"Tramming Wizard"*, khai báo ở `menu_motion.cpp:382`.)

| | `G35` | Tramming Wizard (menu) |
|---|---|---|
| Cách chạy | Đo **cả 4 điểm một lượt** rồi in ra bảng số vòng | Đo **từng góc một**, chọn 1 góc làm gốc so sánh |
| Sau khi vặn ốc | Phải chạy `G35` lại từ đầu | Bấm đo lại đúng góc đó, lặp tới khi ≈ 0 |
| Phù hợp | Vít đã gần đúng, muốn một bảng số đầy đủ | Vặn từng góc, cần phản hồi ngay |

> **`G35` không có mục nào trong menu LCD** — nó chỉ là lệnh G-code, phải gửi qua console/host.
> Trong `Motion` chỉ có **Tramming Wizard** là bản đồ hoạ của cùng thuật toán đó.
> `Motion` cũng có **Auto Z Align** (= `G34`) và **Deploy/Stow Z-Probe** (= `M401`/`M402`).
>
> Trên máy này **không có** mục *"Bed Tramming"* (`_lcd_level_bed_corners`) vì `LCD_BED_TRAMMING`
> không được bật — bản đó là tram **bằng tay**, không dùng probe, phải tự quay encoder để hạ
> nozzle. Cũng chưa bật `PROBE_OFFSET_WIZARD` (đang comment ở `Configuration_adv.h:1381`).

> ⚠️ **Với Voron Tap, nozzle chính là đầu đo.** Nên: lau sạch nozzle trước khi tram (một cục
> nhựa dính ở đầu nozzle làm sai chiều cao trigger), và tram **khi bàn đã đủ nhiệt in** —
> bàn nóng giãn nở, tram lúc nguội rồi in nóng là lệch lại.
>
> `G35` **không** làm bàn phẳng — nó chỉ đưa 4 góc về **cùng một mặt phẳng**. Độ cong ở giữa
> bàn là việc của mesh (`G29`).
>
> Cuối `G35` firmware gọi `set_axis_never_homed(Z_AXIS)` → **Z bị coi là chưa home**, phải
> `G28 Z` trước khi in.

### Thứ tự vận hành

```
1. Siết đều 4 vít lò xo (~50% hành trình)     ; bàn không xê dịch khi in
2. G28                                        ; home
3. G35  (hoặc Motion > Tramming Wizard)       ; vặn vít theo số vòng Marlin báo
4. G34                                        ; căn gantry theo bàn vừa tram
5. G29 P1 -> G29 P3 -> G29 S0                 ; đo + lưu mesh vào slot 0
6. G29 A -> M500                              ; bật leveling + lưu -> tự bật mỗi lần khởi động
```

---

## 6. Nhiệt độ

| | |
|---|---|
| Hotend | 1 extruder, `TEMP_SENSOR_0 = 1` (100k EPCOS), max **275°C** |
| Bàn | `TEMP_SENSOR_BED = 1`, max **100°C** |
| Bed PID | `M304 P145.62 I23.67 D597.15` |
| Hotend | **Model Predictive Control** (`M306 E0 P40.00 C7.13 R0.1284 A0.0680 F0.0970 H0.0056`) |
| Preset vật liệu | `M145 S0 H200 B60` · `S1 H230 B60` · `S2 H240 B60` |
| Filament runout | có, mặc định **TẮT** (`M412 S0`), chân PA13 |

---

## 7. Chân kết nối (đã kiểm chứng bằng `M43`)

| Chức năng | Chân | Header | Ghi chú |
|---|---|---|---|
| `X_MIN_PIN` | PA14 | `X-` | **≡ `X_DIAG_PIN`** (dùng chung net) |
| `Y_MIN_PIN` | PA15 | `Y-` | **≡ `Y_DIAG_PIN`** |
| `Z_MIN_PIN` / probe | PB13 | `Z-` | **≡ `Z_DIAG_PIN`** |
| `FIL_RUNOUT_PIN` | PA13 | `X+` / MT_DET | **≡ `E0_DIAG_PIN`** |
| `PROBE_ENABLE_PIN` | PA8 | servo | enable mạch cảm biến |

> ⚠️ **Trên MKS Monster8, chân endstop dùng chung net với ngõ ra DIAG của TMC2209.**
> Ngõ ra DIAG có thể kéo đường endstop → đọc sai. Nếu `M119` luôn `TRIGGERED` dù đã có
> nguồn PSU, kiểm tra mức điện thô bằng menu **Advanced Settings → Endstop Pins** hoặc
> `M43 E1`, rồi chập chân `S` của header xuống `GND` để xác định.

---

## 8. Tính năng tuỳ biến thêm cho máy này

| Menu | Lệnh | Tác dụng |
|---|---|---|
| **Advanced Settings → Reboot to DFU** | `M997` | Nhảy vào ROM DFU bootloader — **nạp firmware không cần nhấn BOOT0/RESET** |
| **Advanced Settings → Endstop Pins** | — | Hiện **mức điện thô** X/Y/Z MIN lên status line (đọc thẳng `READ()`, bỏ qua logic endstop của Marlin) |
| **Motion → Probe Offset Wizard** | — | Cân `Z offset`: home → probe → hạ nozzle bằng encoder tới khi chạm bàn (test giấy) → `DONE`. Cũng có ở **Advanced Settings → Z Probe Offsets** |
| **Motion → Deploy / Stow Z-Probe** | `M401` / `M402` | Bật/tắt mạch cảm biến qua chân PA8 |
| **Motion → Tramming Wizard** | `G35` | Cân bàn 4 vít |

`PINS_DEBUGGING` đang **BẬT** (chiếm ~8KB flash) — cần cho `M43`, `M43 E1` và menu *Endstop Pins*.
Khi debug xong có thể tắt trong `Configuration_adv.h` để tiết kiệm flash (menu *Endstop Pins* sẽ mất theo).

> **Reboot to DFU trên STM32F4:** ROM bootloader F4 không phải lúc nào cũng vào DFU được
> (báo cáo trên Marlin issue #27047 là khoảng 50%, đo trên chính MKS Monster8).
> Thất bại thì board **chỉ khởi động lại bình thường** — bấm lại. Giữ cáp USB khi bấm.
> Thực tế trên máy này: thành công ngay lần đầu ở hầu hết các lần nạp.

---

## 9. Build & nạp firmware

```powershell
# Build
pio run -e mks_monster8

# Nạp qua DFU (khuyến nghị - tự tìm địa chỉ, tự dò alt setting)
.\upload-dfu.ps1

# Hoặc thủ công
dfu-util -d 0483:df11 -a 0 -s 0x0800C000:leave -t 512 -D .pio\build\mks_monster8\mks_monster8.bin

# Hoặc qua ST-Link
.\upload-firmware.ps1
```

| | |
|---|---|
| File firmware | `.pio\build\mks_monster8\mks_monster8.bin` (**không phải** `firmware.bin`) |
| **Địa chỉ nạp** | **`0x0800C000`** |
| Bootloader MKS | `0x08000000` – `0x0800BFFF` (48KB = sector 0–2) |

> ⚠️ **TUYỆT ĐỐI KHÔNG nạp vào `0x08000000`** — sẽ xoá bootloader MKS và board không boot được.
> `board_build.offset = 0xC000` và `board_upload.offset_address = 0x0800C000` trong `ini/stm32f4.ini`.
> Kiểm chứng bằng `dfu-util --list`: `@Internal Flash /0x08000000/04*016Kg,...` → 3 sector 16KB đầu = 48KB = `0xC000`.

### Xử lý sự cố DFU

| Triệu chứng | Cách xử lý |
|---|---|
| `LIBUSB_ERROR_PIPE` / `get_status` fail | Endpoint USB bị stall → **reset MCU rồi vào DFU lại** (đừng retry vô hạn) |
| Descriptor lệch (`UNKNOWN`, `Broken LANGID`, `alt=0` không phải `@Internal Flash`) | Cùng nguyên nhân trên — session DFU đã hỏng, cần reset MCU |
| Flash rớt giữa chừng | Dùng `-t 512` (đã đặt sẵn trong `upload-dfu.ps1`) |
| Mất bootloader | Luôn khôi phục được: giữ BOOT0 + nhấn RESET → vào DFU → nạp lại đúng địa chỉ |

Chi tiết thêm: [`UPLOAD_README.md`](UPLOAD_README.md)

---

## 10. Kiểm tra nhanh sau khi nạp

```
M115        ; phien ban firmware + timestamp build
M503        ; M92 X80 Y80 Z800 E415 / M203 Z10.00
M122        ; msteps 16 (ca 6 driver), khong co co loi
M119        ; trang thai endstop
M420 V      ; mesh 7x7, bien (15,15) .. (290,290)
```

---

## 11. Thay đổi so với Marlin gốc

Fork này tách ra từ Marlin **2.1.x** — mốc upstream cuối cùng trước khi dự án bắt đầu là
`6aa536c08f` (*"[cron] Bump distribution date (2022-10-19)"*, 19/10/2022). Mọi commit sau đó
trong nhánh `mks-monster8-Voron-2.1.x` là của dự án này.

```bash
# Toàn bộ thay đổi so với mốc Marlin gốc
git diff 6aa536c08f HEAD --stat

# So với bản release 2.1.2 (tag shallow, chỉ để tham chiếu)
git diff up-2.1.2 HEAD --stat
```

> ⚠️ **Đừng đọc con số `--stat` như danh sách thay đổi của mình.** Fork đi theo nhánh
> `2.1.x` đang phát triển, nên so với bản *release* `2.1.2` sẽ thấy ~90 file lệch chỉ vì
> upstream: file thì **mới hơn** (ProUI, `mintemp_error`, `is_above_target`, `MSG_HOME_FIRST`
> một tham số), file thì **cũ hơn**. Đó là drift, không phải thay đổi của dự án.
> Bảng dưới đây chỉ liệt kê những gì **thực sự được sửa cho máy này**.

### 11.1 `Marlin/Configuration.h` — cấu hình máy

| Nhóm | Giá trị |
|---|---|
| Board / màn hình | `MOTHERBOARD BOARD_MKS_MONSTER8_V2`, `MKS_MINI_12864_V3`, `SERIAL_PORT -1` (USB CDC), `BAUDRATE 250000` |
| Gốc & vùng in | `X_MIN_POS −6`, `Y_MIN_POS −22`, `X_MAX_POS 305`, `Y_MAX_POS 305`, `Z_MAX_POS 310`, `X_BED_SIZE 305`, `Y_BED_SIZE 305` |
| Hướng trục | `INVERT_Z_DIR false` (motor dựng đứng ở đáy, trục quay hướng lên) |
| Đầu dò | **Voron Tap** — `FIX_MOUNTED_PROBE`, `NOZZLE_TO_PROBE_OFFSET { 0, 0, 0 }`, `PROBING_MARGIN 15`, `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN`, `USE_PROBE_FOR_Z_HOMING`, `PROBE_ENABLE_DISABLE` |
| Cân Z offset | `PROBE_OFFSET_WIZARD` + `PROBE_OFFSET_WIZARD_START_Z 0` + `PROBE_OFFSET_WIZARD_XY_POS { X_CENTER, Y_CENTER }` (thêm mới) |
| Trục Z | `Z2_DRIVER_TYPE` + `Z3_DRIVER_TYPE` = TMC2209 → `NUM_Z_STEPPERS` **tự suy ra = 3** (`Conditionals_LCD.h:726-734`), `Z_STEPPER_AUTO_ALIGN` (**G34**) |
| Driver | `X/Y/Z/Z2/Z3/E0_DRIVER_TYPE TMC2209` chế độ UART, `*_MICROSTEPS 16`, `*_CURRENT 400`, `*_HAS_STEALTHCHOP` (`STEALTHCHOP_XY`, `STEALTHCHOP_Z`) |
| Leveling | **UBL**, `GRID_MAX_POINTS_X/Y 7`, `MESH_INSET 15`, `ASSISTED_TRAMMING` (**G35**) |
| Nhiệt độ | `TEMP_SENSOR_0/BED 1`, `PIDTEMPBED`, **`MPCTEMP`** cho hotend, `MPC_INCLUDE_FAN`, `PREHEAT_BEFORE_LEVELING`, `HOTEND_OVERSHOOT 15`, `BED_OVERSHOOT 10` |
| Chuyển động | `DEFAULT_AXIS_STEPS_PER_UNIT { 80, 80, 800, 415 }`, `DEFAULT_MAX_FEEDRATE { 500, 500, 10, 25 }`, `DEFAULT_MAX_ACCELERATION { 500, 500, 100, 1000 }`, `DEFAULT_ACCELERATION 500`, **`CLASSIC_JERK`** (không dùng Junction Deviation) |
| Khác | `EEPROM_SETTINGS`, `SDSUPPORT`, `FILAMENT_RUNOUT_SENSOR`, `HOST_ACTION_COMMANDS` |

### 11.2 `Marlin/Configuration_adv.h` — cấu hình nâng cao

| Nhóm | Giá trị |
|---|---|
| Trục Z | `INVERT_Z2_VS_Z_DIR` **tắt** (cả 3 vít me quay cùng chiều), `Z_STEPPER_ALIGN_XY { {280,285}, {25,285}, {25,25} }`, `Z_STEPPER_ALIGN_AMP 1.0`, `Z_STEPPER_ALIGN_ITERATIONS 5`, `Z_STEPPER_ALIGN_ACC 0.02` |
| Tram bàn | `ASSISTED_TRAMMING`, `ASSISTED_TRAMMING_WIZARD`, `REPORT_TRAMMING_MM`, `TRAMMING_SCREW_THREAD 40` (vít M4, bước 0.7mm), `TRAMMING_POINT_XY` 4 góc `{280,285} {25,285} {25,25} {280,25}` |
| TMC2209 | `STEALTHCHOP_XY`, `STEALTHCHOP_Z` (không dùng sensorless homing — đã bỏ `USES_DIAG_JUMPERS`, xem 11.3) |
| Debug | `PINS_DEBUGGING` (cho `M43`, `M43 E1`, menu *Endstop Pins*) |
| **Thêm mới** | `STM32_DFU_REBOOT` — cho phép `M997` nhảy vào ROM DFU bootloader, không cần nhấn BOOT0/RESET |
| **Thêm mới** | `PROBE_OFFSET_WIZARD` — menu cân `Z offset` (`START_Z 0`, probe giữa bàn tại `XY_CENTER`) |

### 11.3 Board & build

| File | Thay đổi |
|---|---|
| `platformio.ini` | `default_envs = mks_monster8` (gốc là `mega2560`), `src_dir = Marlin` |
| `ini/stm32f4.ini` | `board_build.offset = 0xC000`, `board_upload.offset_address = 0x0800C000`, `board_build.rename = mks_monster8.bin`, `upload_protocol/debug_tool = stlink`, `HSE_VALUE=8000000`, `USE_USBHOST_HS`, `USE_ADAFRUIT_SPI`, thêm env `stm32F401ccu6` |
| `pins_MKS_MONSTER8_V2.h` | `DIAG_JUMPERS_REMOVED`, X/Y dùng `X_MIN_PIN`/`Y_MIN_PIN` (thay `X_STOP_PIN`/`Y_STOP_PIN`), `NEOPIXEL2_PIN PC5` |
| `pins_MKS_MONSTER8_common.h` | **bỏ** `USES_DIAG_JUMPERS` (endstop không dùng chung net DIAG), `PROBE_ENABLE_PIN PA8`, `Z_PROBE_PIN PB13`, bỏ `SERVO0_PIN`, đổi chân `E1/E2` → **`Z2/Z3`** (Driver4/Driver5 = motor Z thứ 2, 3) |

### 11.4 Sửa / thêm vào mã nguồn Marlin

| File | Thay đổi |
|---|---|
| `src/HAL/STM32/HAL.cpp`, `HAL.h` | thêm `reboot_to_dfu()` (theo AN2606: `HAL_RCC_DeInit`, remap system flash, `SCB->VTOR`, `__set_MSP`) và cho `flashFirmware()` gọi nó khi bật `STM32_DFU_REBOOT` |
| `src/module/settings.cpp` | **sửa bug**: sau khi nạp EEPROM, ép lại `mstep_reg_select(true)` + `microsteps()` cho TMC2209. Không có bước này, `refresh_stepping_mode()` ghi đè GCONF bằng cache → chân MS1/MS2 không được điều khiển → driver rơi về **1/8**, trục chạy **gấp đôi** (đã gặp thật: `G1 Z10` đi 20mm) |
| `src/inc/Conditionals_LCD.h` | thêm `PROBE_ENABLE_DISABLE` vào `ANY(...)` của `HAS_STOWABLE_PROBE` → menu *Deploy/Stow Z-Probe* hoạt động cả với `FIX_MOUNTED_PROBE` |
| `src/gcode/calibrate/G34_M422.cpp`, `src/gcode/gcode.h` | thêm tham số `Q<nloop>` (lặp G34, home lại sau mỗi 3 lần đo), `U` (chế độ hardcode balance), hàm `InfiniteG34()` |
| `src/lcd/marlinui.cpp`, `marlinui.h` | thêm `pin_test_active` + `pin_test_update()` — in **mức điện thô** `READ(X_MIN_PIN/Y_MIN_PIN/Z_MIN_PIN)` lên status line (bỏ qua logic endstop của Marlin) |
| `src/lcd/menu/menu_advanced.cpp` | thêm 2 menu: **Reboot to DFU** (tắt heater + `planner.finish_and_disable()` rồi `flashFirmware(0)`) và **Endstop Pins** |
| `src/lcd/language/language_en.h` | thêm `MSG_REBOOT_TO_DFU`, `MSG_PIN_TEST` |
| `src/inc/Conditionals_adv.h`, `Conditionals_post.h` | guard nhỏ: bỏ `BABYSTEP_ZPROBE_OFFSET` khi không có probe, bỏ `PREHEAT_BEFORE_LEVELING` khi không bật `PIDTEMPBED` |

### 11.5 File mới của dự án

| File | Mục đích |
|---|---|
| `upload-dfu.ps1` | nạp qua DFU: tự tìm `dfu-util`, dùng `-t 512`, ghi đúng `0x0800C000`, in SHA256 |
| `upload-firmware.ps1` | nạp qua ST-Link |
| `UPLOAD_README.md` | hướng dẫn nạp + xử lý sự cố DFU |
| `cura_profile/machine_definition_changes.inst.cfg` | **Profile Cura chính** — các thông số máy ghi đè lên `voron2_300` của Cura. Đây là file `install-cura-profile.ps1` áp vào Cura |
| `cura_profile/voron21_300_mks_monster8.def.json` | Định nghĩa máy in mới (`inherits: voron2_base`) — chỉ dùng khi muốn thêm máy in riêng trong Cura |
| `install-cura-profile.ps1` | Áp profile vào Cura (mặc định sửa máy in "Voron2 300" đang có, **không cần Admin**) |
| `README.md` | tài liệu máy (file này) |
| `.vscode/extensions.json`, `.gitignore` | cấu hình môi trường phát triển |

### 11.6 Cài profile vào Cura

Cura 5 lưu mọi thay đổi thông số máy vào
`%APPDATA%\cura\<version>\definition_changes\<TênMáy>_settings.inst.cfg`. Script ghi đè file
này, nên **giữ nguyên variant / quality / material / platform** mà máy in đang dùng — không
cần quyền Admin và không cần đụng vào `Program Files`.

```powershell
# Xem trước
.\install-cura-profile.ps1 -WhatIf

# Áp dụng (ĐÓNG CURA TRƯỚC)
.\install-cura-profile.ps1

# Hoặc thêm máy in mới (cần Admin cho Program Files)
.\install-cura-profile.ps1 -AddAsNewPrinter
```

Phải **đóng Cura trước khi chạy** — Cura ghi đè file cấu hình khi thoát.

Những chỗ profile sửa so với bản Voron gốc của Cura:

| Thiết lập | Bản gốc Cura | Máy này | Vì sao |
|---|---|---|---|
| `machine_center_is_zero` | **True** | **False** | Firmware có gốc `(0,0)` ở **góc trước-trái** bàn. Để `True` là Cura dồn bản in lệch nửa bàn |
| `machine_endstop_positive_direction_x/y` | `True` | **`False`** | `X/Y/Z_HOME_DIR -1` — máy home về **MIN**, Voron gốc home về MAX |
| `machine_width/depth` | 300 | **305** | vùng in thật |
| `machine_max_feedrate_z/e` | 40 / 120 | **10 / 25** | `M203 Z10 E25` |
| `machine_max_acceleration_x/y/z/e` | 20000/20000/500 | **500/500/100/1000** | `M201` |
| `acceleration_print` / `_travel` | 5000 / (công thức) | **500 / 1000** | `M204 P500 T1000` |
| `jerk_print` / `_travel` | (mặc định 20) | **10 / 10** | `M205 X10 Y10` — `CLASSIC_JERK` |
| `machine_steps_per_mm_z/e` | 400 / – | **800 / 415** | `M92` |
| Start / End G-code | macro Klipper `PRINT_START ...` | **G-code Marlin** | Firmware là Marlin — `PRINT_START` sẽ bị báo lỗi và **không home/không hâm nóng** |

---

## Lịch sử dự án

Repo này khởi đầu là dự án **tự thiết kế PCB cho Voron 2.4** (STM32F407VET6, chịu tải bàn
740W, hỗ trợ 2 nguồn, ≥7 motor, TMC2209 USART). Board thực tế dùng là **MKS Monster8 V2**,
và repo đã chuyển thành firmware cho máy Voron 300 đang chạy.

Các mốc cũ (giữ lại để tham khảo):

```
31/10: them servo, bltouch, cam bien 24V; dao tin hieu input truc Z;
       cap nhat so do day serial cho motor; cap nhat pin config
04/11: test rotary encoder & cam bien nhiet
```
