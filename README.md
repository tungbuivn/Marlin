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
| Max feedrate (mm/s) | `M203 X300 Y300 Z10 E25` | 300 mm/s = `F18000` trong G-code (trước là 500) |
| Accel (mm/s²) | `M201 X500 Y500 Z100 E1000` | **Trần cứng 500** cho X/Y — Marlin lấy `min(M204 P, M201)` |
| Accel print/retract/travel | `M204 P500 R500 T500` | Cura đặt lại `M204 S…` mỗi lần in, luôn ≤ 500 |
| Jerk | `M205 X8 Y8 Z0.40 E5` | |
| Homing feedrate | X/Y 3000, Z **480** mm/min | Z = 8 mm/s < max 10 mm/s |
| Soft endstop | `M211 S1` | |

### 3.1 Vì sao X/Y = 500 chứ không phải 3000 của Voron

`printer.cfg` gốc của Voron Design đặt `max_accel: 3000` (kèm ghi chú `# Max 4000`), và profile
Cura chính thức của Voron còn cao hơn (`acceleration_print 5000`). Nhưng **những con số đó là cho
Klipper đã chạy input shaping**; Marlin **không có input shaping**, nên lấy nguyên 3000 sẽ thấy
ghosting ở góc.

Hiện tại: **500**. Trước đó từng để 1500 rồi hạ tiếp xuống 500. Muốn nâng lại thì tăng dần và dừng
ngay khi thấy vệt rung — nhớ phải nâng **cả `M201` lẫn Cura**.

> ⚠️ **Marlin lấy `min(M204 P, M201 của trục)`.** `M201 X/Y` là **trần cứng 500**, nên Cura có khai
> gì thì máy cũng không vượt 500. Profile Cura đã được đặt khớp ở 500 (`acceleration_print` /
> `_travel` / `machine_acceleration` / `machine_max_acceleration_x/y`) để con số trong Cura không còn
> "nói dối". Chi tiết ở §11.6.
> Z giữ `M201 Z100` (đã dưới 500) và E `M201 E1000`, nhưng Cura khai `machine_max_acceleration_e = 500`
> nên **E thực tế cũng bị chặn ở 500**.
>
> ℹ️ `Configuration.h` vẫn ghi `DEFAULT_MAX_ACCELERATION { 1500, 1500, 100, 1000 }` — giá trị **biên
> dịch** đó chỉ có tác dụng nếu chạy `M502` (đừng chạy). EEPROM đang là 500 và **EEPROM thắng**.

### 3.2 `INVERT_E0_DIR` — hướng extruder

```c
#define INVERT_E0_DIR false   // Bondtech BMG la extruder CO HOP SO (E = 415 steps/mm)
```
Comment ngay trên option đó trong Marlin: *"for direct drive extruder v9 set to true, for **geared
extruder** set to false"*. Để `true` thì `G1 E10` **rút** thay vì đẩy. Chiều quay **không** lưu
trong EEPROM — chỉ có trong firmware, phải flash mới đổi được.

---

## 4. Đầu dò Z

Cảm biến **Voron Tap** — probe **chính là nozzle**, kèm mạch enable qua chân PA8.

| | |
|---|---|
| Tín hiệu probe | **PB13** (header `Z-`), `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN` + `USE_PROBE_FOR_Z_HOMING` |
| Enable probe | **PA8** (`PROBE_ENABLE_PIN`, header servo) — `1` = bật mạch cảm biến, `0` = tắt |
| Điều khiển | **`M401`** = deploy (PA8 HIGH) · **`M402`** = stow (PA8 LOW) |
| Offset nozzle→probe | `M851 X0.00 Y0.00 Z0.70` — **XY = 0 vì probe là chính nozzle (Voron Tap)**. Số Z là kết quả cân thực tế, có thể đổi mỗi lần cân lại |
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
> `G1 Z0.2` sẽ đẩy nozzle **3.15 mm xuyên xuống bàn**. Offset hiện tại **`Z0.70`** đã cân bằng
> `Probe Offset Wizard` rồi; nếu đổi nozzle/toolhead thì phải cân lại.

> **Quy trình cân Z offset:** `Motion → Probe Offset Wizard` → home → probe giữa bàn → hạ nozzle
> từng bước 0.1 mm tới khi tờ giấy kẹt nhẹ → `DONE` → **`M500`**. Không có `M500` là mất khi tắt máy.

> Probe chỉ có tín hiệu khi **đã deploy** (`M401`). Khi stow thì `z_min` không phản ánh bàn.
> `G35` và các wizard tự deploy/stow, không cần `M401` tay.

---

## 5. Cân bàn & căn gantry

| Tính năng | Cấu hình |
|---|---|
| Leveling | **UBL (Unified Bed Leveling)** |
| Mesh | **7 × 7 = 49 điểm**, `MESH_INSET 15` → phủ **`(15,15)` … `(290,290)`** |
| Fade height | 10 mm |
| Trạng thái | **Đã có mesh ở slot 0** — `Mesh is valid`, `Storage slot: 0`, `Bed Leveling ON`. `M500` báo `Mesh saved in slot 0` |
| Căn gantry | `Z_STEPPER_AUTO_ALIGN` (**G34**) — 3 điểm `{280,285} {25,285} {152.5,25}` = **sau-phải, sau-trái, trước-GIỮA** (layout 2, đúng vị trí 3 vít me) |
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

> ⚠️ **Điểm G34 nằm trong EEPROM, không chỉ trong code.** `M422 S<n> X.. Y..` ghi đè
> `Z_STEPPER_ALIGN_XY` và **được `M500` lưu lại**; lúc khởi động Marlin nạp lại từ EEPROM nên
> **giá trị EEPROM thắng giá trị biên dịch**. Sửa `Configuration_adv.h` rồi flash mà không ghi
> lại EEPROM thì G34 vẫn chạy điểm cũ. Kiểm tra và sửa:
> ```
> M422                                   ; xem 3 điểm hiện hành
> M422 S3 X152.5 Y25                     ; Z3 ra giữa trục X
> M500                                   ; lưu
> ```
> Đừng dùng `M502` để "nạp lại mặc định" — nó xoá luôn `M851` (Z offset đã cân) và các thông số
> khác. **Bản đồ 3 vít me phải khớp thứ tự driver** `Z, Z2, Z3` (Marlin `Configuration_adv.h:1007`:
> *"one position per Z stepper in stepper driver order"*), và mỗi điểm phải nằm **ngay trên vít me
> của nó** — không phải ở góc bàn.

### Thứ tự vận hành

```
1. Siết đều 4 vít lò xo (~50% hành trình)     ; bàn không xê dịch khi in
2. G28                                        ; home
3. G35  (hoặc Motion > Tramming Wizard)       ; vặn vít theo số vòng Marlin báo
4. G34 Q99                                    ; căn gantry; lặp tới khi sai số <= 0.02
5. G29 P1 -> G29 P3 -> G29 S0                 ; đo + lưu mesh vào slot 0
6. G29 A -> M500                              ; bật leveling + lưu -> tự bật mỗi lần khởi động
```

> Thứ tự **G34 trước, `G29` sau** ở bước 4–5 không phải tuỳ tiện — xem mục dưới.

### UBL và G34 có đụng nhau không?

**Không. Marlin tự tắt mesh trước khi G34 probe, rồi bật lại y như cũ.**

```cpp
// G34_M422.cpp:202-208 — comment gốc của Marlin: "Disable the leveling matrix before auto-aligning"
#if HAS_LEVELING
  #if ENABLED(RESTORE_LEVELING_AFTER_G34)
    const bool leveling_was_active = planner.leveling_active;   // nhớ trạng thái
  #endif
  set_bed_leveling_enabled(false);                              // TẮT mesh
#endif
// ... probe 3 điểm ...
#if BOTH(HAS_LEVELING, RESTORE_LEVELING_AFTER_G34)
  set_bed_leveling_enabled(leveling_was_active);                // G34_M422.cpp:552-554 — BẬT LẠI
#endif
```

| | |
|---|---|
| Máy này | `RESTORE_LEVELING_AFTER_G34` **đang bật** (`Configuration_adv.h:1027`) → vòng tắt/bật khép kín |
| Tắt **thật**, không chỉ hạ cờ | `bedlevel.cpp:79-81`: `apply_modifiers` → lật `planner.leveling_active` → `unapply_modifiers` |
| Hệ quả | Số đo G34 là **độ cao thô** của bàn — mesh không được cộng vào |
| Vì sao **buộc** phải tắt | G34 dùng "dirty trick" `current_position.z += z_probe * 0.5f` (`G34_M422.cpp:244`). Mesh còn bật thì phép cộng đó bị bẻ cong → G34 tính sai |

> ⚠️ **Chỗ thật sự đáng lo không phải "UBL trong lúc G34", mà là mesh đo TRƯỚC G34.**
> G34 nghiêng lại gantry, mà probe gắn trên gantry — nên mesh UBL **mã hoá luôn cả độ nghiêng gantry
> lúc đo**. G34 làm phẳng gantry xong thì mesh cũ hiệu chỉnh **thừa** đúng bằng phần nghiêng vừa sửa.
>
> G34 có tính lặp lại (`Z_STEPPER_ALIGN_ACC 0.02` → lệch tối đa 0.02 mm), nên quy tắc là:
> **tạo mesh SAU khi đã G34** (đúng thứ tự bước 4 → 5 ở trên), và mỗi lần in đều G34 về đúng trạng
> thái đó thì mesh giữ nguyên giá trị.
> **Phải chạy lại `G29`** nếu: mới G34 lần đầu sau khi tạo mesh, vừa tháo/lắp gantry, đổi belt,
> hoặc đổi/thay Z-stepper.

> ⚠️ **Với UBL, `M420 S1` bật leveling kể cả khi mesh hỏng.** `bedlevel.cpp:62` chỉ kiểm tra mesh
> hợp lệ cho `AUTO_BED_LEVELING_BILINEAR`:
> ```cpp
> const bool can_change = TERN1(AUTO_BED_LEVELING_BILINEAR, !enable || leveling_is_valid());
> ```
> UBL không nằm trong điều kiện đó → **đừng tin `M420 S1` là an toàn**; muốn chắc thì `M420 V`
> phải in ra `Mesh is valid`.

### `M420 S1` trong start G-code — thừa, nhưng nên giữ

Start G-code hiện tại: `G28` → `G34 Q99` → `G28 Z` → `M420 S1`.

Cả `G28` lẫn `G34` đều **tự khôi phục** trạng thái leveling, nên tới dòng `M420 S1` thì mesh đã bật sẵn:

| Lệnh | Cơ chế tự bật lại |
|---|---|
| `G28` | `RESTORE_LEVELING_AFTER_G28` (`Configuration.h:1977`) → `CAN_SET_LEVELING_AFTER_G28 = 1` (`bedlevel.h:26-28`) → `G28.cpp:546` |
| `G34` | `RESTORE_LEVELING_AFTER_G34` (`Configuration_adv.h:1027`) → `G34_M422.cpp:553` |

`M420 S1` vì thế gần như no-op — **nhưng cứ giữ**, nó là lưới an toàn nếu sau này bạn lỡ `M420 S0`
rồi `M500` (trạng thái leveling **có** được lưu vào EEPROM, xem §11.7 cạm bẫy 1).

> ℹ️ `G29` (dựng mesh) cũng tự tắt leveling trong lúc đo rồi khôi phục — `ubl_G29.cpp:1244-1245`
> (`set_bed_leveling_enabled(false)`) và `ubl_G29.cpp:1257` (khôi phục). Nên không cần `M420 S0`
> thủ công trước khi `G29`.

### G34 — số đo thực tế (đã kiểm chứng bằng log debug)

#### 1. Probe lặp lại cực tốt — `M48 P20 V4` tại đúng 3 điểm G34

| Điểm | Vị trí | Mean | σ | Range (20 lần) |
|---|---|---|---|---|
| S1 | (280, 285) | 0.0623 mm | 0.0024 mm | 0.009 mm |
| S2 | (25, 285) | 0.0705 mm | 0.0016 mm | 0.007 mm |
| S3 | (152.5, 25) | 0.0678 mm | 0.0022 mm | 0.010 mm |

→ Nhiễu probe chỉ **~2 µm**, còn khoảng lệch **tĩnh** giữa 3 điểm là **8 µm**
(`0.0705 − 0.0623`). **Nhiễu probe không phải thứ giới hạn G34.**

#### 2. G34 tính và bù đúng

Log debug một vòng (`G34 Q3 T0.01`):

```
DBG raw Z1 um=9807   Z2 um=9802   Z3 um=9798      <- khoang do that: 9 um
DBG fix Z1 move_um=8  err_um=8
DBG fix Z2 move_um=3  err_um=3                     <- lenh bu khop chinh xac
DBG fix Z3 move_um=0  err_um=0                     <- diem thap nhat khong dung
Target accuracy achieved.  Did 1 of 3
```

`move = z_đo − z_min` **đúng dấu, đúng độ lớn**. Cơ chế nghiêng gantry hoạt động thật:
`set_all_z_lock(true, zstepper)` được tiêu thụ ở macro `TRIPLE_SEPARATE_APPLY_STEP`
(`stepper.cpp:324-334`) qua `locked_##A##_motor`.
*(Lưu ý khi đi tìm: grep chữ `locked_Z_motor` sẽ **không** thấy chỗ dùng, vì nó là macro nối token.)*

#### 3. Thời gian & số vòng — đo được

| Tình huống | Số vòng | Thời gian |
|---|---|---|
| Gantry đã căn (khoảng đo 5–15 µm) | **1 vòng** | ~35 s |
| Chạy từ **Z=100** | **1 vòng** | **61.5 s** (phải hạ 100 mm mỗi lần probe) |
| Máy vừa nằm không tải, gantry lệch ~20–30 µm | **2 vòng Q = 6 lượt probe** | **171.3 s** |

→ **Z xuất phát KHÔNG ảnh hưởng kết quả**, chỉ ảnh hưởng thời gian. Lý do: `run_z_probe` tự hạ
nhanh xuống `Z_CLEARANCE_DEPLOY_PROBE + 5 + |offset.z|` trước khi probe chậm (`probe.cpp:739-748`),
nên probe chậm luôn bắt đầu từ cùng một khoảng cách tương đối so với điểm trigger.

#### 4. Vì sao đôi khi phải chạy nhiều vòng

Ngưỡng `Z_STEPPER_ALIGN_ACC 0.02` (20 µm) nằm **ngay trên** khoảng lệch thật của máy khi đã căn
(5–15 µm), nên bình thường chỉ 1 vòng. Khi gantry còn lệch 20–30 µm thì cần thêm vòng — và trong
lúc đó Marlin có thể in `Decreasing Accuracy Detected.`: đó là heuristic `adjustment_reverse`
(`G34_M422.cpp`) **đảo chiều bù** khi sai số *tăng* thay vì giảm. Comment gốc của Marlin ngay cạnh
đó nói rõ nó viết cho máy **2 trục Z**: *"Will match reversed Z steppers on dual steppers.
Triple will need more work to map."* — máy này **3 trục Z**.

**Không phải lỗi firmware, không phải nhiễu probe** — chỉ là trạng thái cơ khí lúc đo.
Muốn chặn trần thời gian thì giới hạn số vòng (`G34 Q3` thay vì `G34 Q99`), hoặc nới ngưỡng bằng
tham số chạy được, **không cần flash**: `G34 T0.05`.

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
dfu-util -d 0483:df11 -a 0 -s 0x0800C000:leave -t 2048 -D .pio\build\mks_monster8\mks_monster8.bin

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

### Tốc độ nạp DFU — đo thực tế

Cùng một file 254 KB (`mks_monster8.bin`), board cắm **trực tiếp** vào PC:

| `-t` (byte mỗi gói) | Thời gian | Tốc độ |
|---|---|---|
| 512 | **117.3 s** | 2.2 KB/s |
| **2048** | **34.3 s** | 7.4 KB/s |

`2048` chính là `wTransferSize` tối đa mà ROM DFU của STM32F407 báo ra → **~34 s là sàn của đường DFU**, đặt 4096 sẽ bị từ chối.

> ⚠️ **Bài học:** từng hạ `-t` xuống 512 để chống lỗi `get_status`, nhưng 512 làm flash chậm **3.4 lần** — đó chính là lý do "flash rất chậm". Lỗi `get_status` thật ra đến từ **USB hub**, không phải từ `-t`. **Gặp lỗi thì bỏ hub trước, đừng hạ `-t`.**
> Kiểm tra đang qua hub hay không: `dfu-util --list` → `path="2-4.4"` là qua hub, `path="2-2"` là cắm trực tiếp.

Dòng `DFU state(10) = dfuERROR ... firmware is corrupt` hiện ở đầu **mọi** lần flash là **bình thường** — app đang chạy không phải DFU nên ROM bootloader báo vậy. Cứ để nó chạy tiếp.

Muốn nhanh hơn nữa thì rời khỏi DFU: **ST-Link** (`.\upload-firmware.ps1`) hoặc **thẻ nhớ** — copy `.bin` vào thẻ rồi power-cycle, bootloader MKS tự nạp, không cần PC.

### Xử lý sự cố DFU

| Triệu chứng | Cách xử lý |
|---|---|
| `LIBUSB_ERROR_PIPE` / `get_status` fail | Endpoint USB bị stall → **reset MCU rồi vào DFU lại** (đừng retry vô hạn) |
| Descriptor lệch (`UNKNOWN`, `Broken LANGID`, `alt=0` không phải `@Internal Flash`) | Cùng nguyên nhân trên — session DFU đã hỏng, cần reset MCU |
| Flash rớt giữa chừng / `get_status` fail | **Bỏ USB hub, cắm trực tiếp** rồi thử lại ở `-t 2048`; chỉ hạ `-t` khi đã cắm trực tiếp mà vẫn lỗi |
| Mất bootloader | Luôn khôi phục được: giữ BOOT0 + nhấn RESET → vào DFU → nạp lại đúng địa chỉ |

Chi tiết thêm: [`UPLOAD_README.md`](UPLOAD_README.md)

---

## 10. Kiểm tra nhanh sau khi nạp

```
M115        ; phien ban firmware + timestamp build
M503        ; M92 X80 Y80 Z800 E415 / M203 X300 Y300 Z10 E25
            ; M201 X500 Y500 Z100 E1000 / M204 P500 R500 T500 / M205 X8 Y8 Z0.40 E5
            ; M851 X0 Y0 Z0.70
M122        ; msteps 16 (ca 6 driver), khong co co loi
M119        ; trang thai endstop
M422        ; 3 diem G34: S1 280/285, S2 25/285, S3 152.5/25
M420 V      ; mesh 7x7, bien (15,15) .. (290,290), Storage slot: 0, Bed Leveling ON
```

### Checklist sau mỗi lần nạp

| Kiểm tra | Lệnh | Mong đợi |
|---|---|---|
| Firmware mới thật chưa | `M115` | timestamp khớp giờ build |
| Sau `G28` nozzle có được nâng | `G28` → `M114` | **`Z:10.00`** (`Z_AFTER_PROBING 10`) |
| Chiều extruder | `M83` · `G1 E10 F100` | **đẩy ra**, không phải rút vào |
| Offset probe còn không | `M851` | Z ≈ 0.70 (số đã cân) |
| Điểm G34 còn không | `M422` | `S3 X152.50 Y25` |
| Mesh còn không | `M420 V` | `Mesh is valid`, `Storage slot: 0`, `Bed Leveling ON` |

Nếu mục nào sai sau khi nạp → xem **11.7** trước khi sửa code.

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
| Đầu dò | **Voron Tap** — `FIX_MOUNTED_PROBE`, `NOZZLE_TO_PROBE_OFFSET { 0, 0, 0 }`, `PROBING_MARGIN 15`, `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN`, `USE_PROBE_FOR_Z_HOMING`, `PROBE_ENABLE_DISABLE`, **`Z_AFTER_PROBING 10`** |
| Cân Z offset | `PROBE_OFFSET_WIZARD` + `PROBE_OFFSET_WIZARD_START_Z 0` + `PROBE_OFFSET_WIZARD_XY_POS { X_CENTER, Y_CENTER }` (thêm mới) |
| Hướng extruder | **`INVERT_E0_DIR false`** — Bondtech BMG là extruder có hộp số (từng để `true` → extruder quay ngược) |
| Trục Z | `Z2_DRIVER_TYPE` + `Z3_DRIVER_TYPE` = TMC2209 → `NUM_Z_STEPPERS` **tự suy ra = 3** (`Conditionals_LCD.h:726-734`), `Z_STEPPER_AUTO_ALIGN` (**G34**) |
| Driver | `X/Y/Z/Z2/Z3/E0_DRIVER_TYPE TMC2209` chế độ UART, `*_MICROSTEPS 16`, `*_CURRENT 400`, `*_HAS_STEALTHCHOP` (`STEALTHCHOP_XY`, `STEALTHCHOP_Z`) |
| Leveling | **UBL**, `GRID_MAX_POINTS_X/Y 7`, `MESH_INSET 15`, `ASSISTED_TRAMMING` (**G35**) |
| Nhiệt độ | `TEMP_SENSOR_0/BED 1`, `PIDTEMPBED`, **`MPCTEMP`** cho hotend, `MPC_INCLUDE_FAN`, `PREHEAT_BEFORE_LEVELING`, `HOTEND_OVERSHOOT 15`, `BED_OVERSHOOT 10` |
| Chuyển động | `DEFAULT_AXIS_STEPS_PER_UNIT { 80, 80, 800, 415 }`, `DEFAULT_MAX_FEEDRATE { 500, 500, 10, 25 }`, **`DEFAULT_MAX_ACCELERATION { 1500, 1500, 100, 1000 }`**, **`DEFAULT_ACCELERATION 1500`**, **`DEFAULT_TRAVEL_ACCELERATION 2000`** (`DEFAULT_RETRACT_ACCELERATION 500` giữ nguyên), **`DEFAULT_XJERK/DEFAULT_YJERK 8.0`** (`ZJERK 0.4`, `EJERK 5.0` giữ nguyên), **`CLASSIC_JERK`** (không dùng Junction Deviation) |
| Khác | `EEPROM_SETTINGS`, `SDSUPPORT`, `FILAMENT_RUNOUT_SENSOR`, `HOST_ACTION_COMMANDS` |

### 11.2 `Marlin/Configuration_adv.h` — cấu hình nâng cao

| Nhóm | Giá trị |
|---|---|
| Trục Z | `INVERT_Z2_VS_Z_DIR` **tắt** (cả 3 vít me quay cùng chiều), `Z_STEPPER_ALIGN_XY { {280,285}, {25,285}, {X_CENTER,25} }`, `Z_STEPPER_ALIGN_AMP 1.0`, `Z_STEPPER_ALIGN_ITERATIONS 5`, `Z_STEPPER_ALIGN_ACC 0.02` |
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
| Start G-code Cura | dùng **`G34 Q99`** — lặp tối đa 99 lần, **dừng ngay khi sai số ≤ `Z_STEPPER_ALIGN_ACC` (0.02)**. Xem mục 11.8 |
| `src/lcd/marlinui.cpp`, `marlinui.h` | thêm `pin_test_active` + `pin_test_update()` — in **mức điện thô** `READ(X_MIN_PIN/Y_MIN_PIN/Z_MIN_PIN)` lên status line (bỏ qua logic endstop của Marlin) |
| `src/lcd/menu/menu_advanced.cpp` | thêm 2 menu: **Reboot to DFU** (tắt heater + `planner.finish_and_disable()` rồi `flashFirmware(0)`) và **Endstop Pins** |
| `src/lcd/language/language_en.h` | thêm `MSG_REBOOT_TO_DFU`, `MSG_PIN_TEST` |
| `src/inc/Conditionals_adv.h`, `Conditionals_post.h` | guard nhỏ: bỏ `BABYSTEP_ZPROBE_OFFSET` khi không có probe, bỏ `PREHEAT_BEFORE_LEVELING` khi không bật `PIDTEMPBED` |

### 11.5 File mới của dự án

| File | Mục đích |
|---|---|
| `upload-dfu.ps1` | nạp qua DFU: tự tìm `dfu-util`, dùng `-t 2048`, ghi đúng `0x0800C000`, in SHA256 |
| `upload-firmware.ps1` | nạp qua ST-Link |
| `UPLOAD_README.md` | hướng dẫn nạp + xử lý sự cố DFU |
| `cura_profile/machine_definition_changes.inst.cfg` | **Profile Cura — container của MÁY IN** (bàn, gốc, endstop, feedrate/accel/jerk, steps/mm, Start/End G-code) |
| `cura_profile/extruder_definition_changes.inst.cfg` | **Profile Cura — container `definition_changes` của EXTRUDER** (Extruder Start G-code = đường purge, Extruder End G-code = retract) |
| `cura_profile/extruder_user.inst.cfg` | **Profile Cura — container `user` của EXTRUDER** (tốc độ retract). Phải nằm ở đây, xem 11.6 |
| `cura_profile/voron21_300_mks_monster8.def.json` | Định nghĩa máy in mới (`inherits: voron2_base`) — chỉ dùng khi muốn thêm máy in riêng trong Cura |
| `install-cura-profile.ps1` | Áp cả 3 container vào Cura (mặc định sửa máy in "Voron2 300" đang có, **không cần Admin**) |
| `README.md` | tài liệu máy (file này) |
| `.vscode/extensions.json`, `.gitignore` | cấu hình môi trường phát triển |

### 11.6 Cài profile vào Cura

Cura 5 lưu mọi thay đổi thông số máy vào `%APPDATA%\cura\<version>\definition_changes\` và
`...\user\`. Script ghi vào **3 container**, nên **giữ nguyên variant / quality / material /
platform** mà máy in đang dùng — không cần quyền Admin và không cần đụng vào `Program Files`.

| Container | File | Ghi kiểu |
|---|---|---|
| `definition_changes` của **máy in** | `definition_changes\Voron2+300_settings.inst.cfg` | ghi đè |
| `definition_changes` của **extruder** | `definition_changes\voron2_extruder_0+%232_settings.inst.cfg` | ghi đè |
| `user` của **extruder** | `user\voron2_extruder_0+%232_user.inst.cfg` | **merge** — giữ lại `infill_pattern`, `infill_sparse_density`… anh đặt tay |

```powershell
# Xem trước
.\install-cura-profile.ps1 -WhatIf

# Áp dụng (ĐÓNG CURA TRƯỚC)
.\install-cura-profile.ps1

# Hoặc thêm máy in mới (cần Admin cho Program Files)
.\install-cura-profile.ps1 -AddAsNewPrinter
```

Phải **đóng Cura trước khi chạy** — Cura ghi đè file cấu hình khi thoát. Script sẽ **tự từ chối
chạy** nếu thấy tiến trình Cura (`-Force` để bỏ qua, không nên).

### Bốn ô G-code trong Cura

Cura có **4** ô G-code, nằm ở 2 tab khác nhau của `Machine settings` — dễ tưởng là thiếu:

| Tab | Ô | Key | Script điền gì |
|---|---|---|---|
| **Printer** | Start G-code | `machine_start_gcode` | `M104 S230` + `M140 S60` (**cố định**) → `G28` → **`G34 Q99`** → `G28 Z` → `M190 S60` → `M109 S230` → `M420 S1` |
| **Printer** | End G-code | `machine_end_gcode` | `M400` → nâng Z → `G27` park → tắt nhiệt → `M84 X Y E` |
| **Extruder 1** | Extruder Start G-code | `machine_extruder_start_code` | `G1 Z2.0` → **đi ngang** tới `X2 Y10` → **rồi mới** hạ `Z0.3` → purge `Y10 → Y100` → `G92 E0` → nhấc `Z2.0` → về tâm `X152.5 Y152.5` |
| **Extruder 1** | Extruder End G-code | `machine_extruder_end_code` | retract `G1 E-2 F2700` |

`machine_extruder_*` nằm trong `fdmextruder.def.json` với `default_value = ""`, nên **mặc định
Cura để trống** và chúng thuộc container của **extruder**, không phải của máy in — script ghi
vào cả hai container.

> ⚠️ **Nhiệt độ đã cố định trong Start G-code** — `M104 S230` / `M140 S60` / `M190 S60` /
> `M109 S230` là số cứng, **không còn placeholder `{material_print_temperature_layer_0}`**.
> Nên đổi vật liệu trong Cura **không** làm đổi nhiệt độ in; muốn đổi thì sửa Start G-code.

> ⚠️ **Tốc độ retract bị kẹp ở 15 mm/s.** `voron2_base` đặt
> `maximum_value_warning = machine_max_feedrate_e − 10`. Firmware chạy `M203 E25` nên ngưỡng là
> **15**; để 25 hay 30 (mặc định Cura) là **Cura chặn slice** với lỗi *"Retraction Prime Speed /
> Retraction Speed / Retraction Retract Speed"*. Muốn retract nhanh hơn thì phải nâng `M203 E`
> lên ≥ 40 trong firmware rồi đặt lại cho khớp.

> ⚠️ **Đừng gộp XY và Z trong một lệnh purge.** `G1 X2 Y10 Z0.3 F5000` là **một bước di chuyển đồng
> thời 3 trục** — Marlin nội suy nên đầu in vừa chạy ngang vừa hạ dần, tạo một **đường dốc cắt qua
> mặt bàn** từ `(152, 152, 2.0)` xuống `(2, 10, 0.3)`. Đúng thứ tự phải là tách ra:
> ```gcode
> G1 Z2.0 F3000       ; giu Z cao
> G1 X2 Y10 F5000     ; di NGANG toi diem purge, chua ha
> G1 Z0.3 F600        ; toi noi roi MOI ha dau in
> G1 X2 Y100 F1500 E15
> G92 E0
> G1 Z2.0 F3000       ; purge xong NHAC Z len
> G1 X152.5 Y152.5 F6000   ; roi moi di tiep, ve tam ban
> ```
> Quên nhấc Z sau khi purge thì mọi travel sau đó đều **kéo nozzle quét mặt bàn** ở `Z0.3`.

> ⚠️ **Tốc độ retract PHẢI nằm ở container `user`, không phải `definition_changes`.**
> `fdmextruder.def.json` **không có dòng `inherits`** — nó đứng riêng, không kế thừa
> `fdmprinter`. Nên definition `Toolhead` (= `voron2_extruder_0` → `fdmextruder`) **không chứa**
> `retraction_speed` (setting này ở `fdmprinter.def.json` dòng 4702). Đặt vào container
> `definition_changes` của extruder thì Cura **âm thầm bỏ qua** và ghi log:
> `InstanceContainer.setProperty: ... has no SettingInstance ... SettingDefinition Toolhead`.
> Container `user` của extruder khai `definition = voron2_300` (definition của **máy**), nên có
> đủ chuỗi `voron2_300` → `voron2_base` → `fdmprinter`. Script ghi vào đó và **merge** — giữ lại
> các giá trị anh đã đặt tay (ví dụ `infill_pattern`, `infill_sparse_density`).

### Script hậu xử lý — `FirstLayerTwice.py`

Script làm **hai việc độc lập**, bật/tắt riêng:

1. **Ép nhiệt độ toàn file** (kể cả khối init)
2. **In layer 0 hai lần**

#### 1. Ép nhiệt độ — `force_temperatures`

**Vì sao cần, dù Start G-code đã hardcode:** Cura **vẫn tự phát `M104`/`M140` riêng** khi
`material_print_temperature_layer_0` khác `material_print_temperature` (vật liệu đang là ABS:
250/100). Lệnh đó nằm **sau** Start G-code nên **ghi đè 230/60**.

Script quét **mọi chunk** và ép lại:

| Lệnh | Ép về |
|---|---|
| `M104`, `M109` | **`hotend_temp`** (230) |
| `M140`, `M190` | **`bed_temp`** (60) |

Cả `S` lẫn `R` (bản "chờ nguội" của `M109`/`M190`) đều bị ép.

> 🔴 **Lệnh TẮT nhiệt (`S0` / `R0`) được giữ nguyên.** Nếu ép `M104 S0` thành `M104 S230`
> thì **hotend bật lại ngay tại bước kết thúc in**. Đây là quy tắc an toàn quan trọng nhất
> của phần này: chỉ ép khi giá trị **> 0**.

#### 2. In layer 0 hai lần — `double_first_layer`

Có **2 chế độ Z**, chọn bằng `z_mode`:

**`z_mode = "split"` (mặc định)** — chia layer 0 thành **2 lớp mỏng**, **KHÔNG đổi hệ toạ độ**:

| Lượt | Z | Ví dụ (layer 0.2) |
|---|---|---|
| Pass 1 | `first_pass_z` | **0.1** |
| Pass 2 | Z gốc của layer 0 | **0.2** |
| Layer 1+ | không đụng | 0.4, 0.6… |

Không có `G92` — chỉ **hạ Z của pass 1 xuống** rồi pass 2 in lại ở đúng Z gốc. Hệ toạ độ giữ
nguyên nên **bản in cao đúng bằng mô hình** ✓. Nhựa vẫn 20% + 80% = **100%** trải trên 0.2 mm →
**mật độ lớp đầu bình thường**, nhưng đường in được chạy 2 lượt nên đặc và phẳng hơn.

Script tự đọc Z của layer 0 từ chính G-code, và **hạ Z bằng cách trừ một hằng số** khỏi mọi giá
trị Z của pass 1 — nên **Z-hop (nếu bật) vẫn còn tác dụng**, không bị dẹp mất.

> ⚠️ **Đọc Z của layer 0 phải lấy MIN, không được lấy giá trị đầu tiên.** `voron2_base` của UltiMaker
> bật Z-hop (`retraction_hop_enabled = true`, `retraction_hop = 0.2`), nên bước `G0/G1` đầu tiên của
> layer 0 là bước **nâng lên** `0.2 + 0.2 = 0.4`:
> ```gcode
> G1 F600 Z0.4     ; Z-hop, KHONG phai chieu cao layer
> G0 F11250 X.. Y..
> ;TYPE:SKIRT
> G1 F600 Z0.2     ; day moi la chieu cao layer 0
> ```
> Lấy nhầm `0.4` làm `layer_z` thì `offset = 0.4 − 0.1 = 0.3` (đúng phải là `0.1`) và chiều cao thật
> `0.2` biến thành **`Z-0.1`**; Marlin có `Z_MIN_POS 0` nên kẹp về 0 → pass 1 in ngay trên mặt bàn.
> Xem cạm bẫy 17 và test hồi quy `tests/test_first_layer_twice.py`.

> Nếu `first_pass_z` **không hợp lệ** (≤ 0, hoặc ≥ Z của layer 0) thì script **tự lùi về an toàn**:
> hai pass cùng độ cao, không hạ Z, không `G92`, và ghi chú lý do vào G-code.

**`z_mode = "shift"`** — cách cũ: `G1 Z<z_raise>` rồi `G92 Z<layer0_z>`, làm bản in **cao hơn
mô hình đúng 0.2 mm**.

> 🔴 **Guard:** nếu `layer0_z` > `z_raise` thì độ dịch hệ toạ độ là **âm**, mọi bước Z sau đó đi
> **xuống** → **nozzle đâm vào bàn**. Script **tự bỏ qua lệnh `G92`** trong trường hợp này và ghi
> cảnh báo vào G-code.

**Toán lượng nhựa:** script giữ **hai bộ đếm** — `E gốc` (để tính delta) và `E mới` (để ghi ra).
Dùng chung một biến là sai, vì delta sẽ bị tính trên giá trị đã nhân. Pass 2 chạy lại đúng body
đó nên `E gốc` xuất phát từ **cùng điểm** như pass 1, chỉ `E mới` nối tiếp từ cuối pass 1.

```
delta gốc:      0.5  0.5  0.5  (-0.2 retract)  (+0.2 unretract)  0.7
pass 1 @ 20%:   0.1  0.1  0.1   -0.2           +0.2             0.14   -> E = 0.44
pass 2 @ 80%:   0.4  0.4  0.4   -0.2           +0.2             0.56   -> E = 2.20
                                                                        = đúng 1 lớp
```
Tổng hai pass = **đúng bằng một lớp bình thường** (20% + 80% = 100%) — hai lượt in nhưng không
thừa nhựa.

> **Chỉ nhân E của bước CÓ X hoặc Y.** Bước thuần E là retract/unretract — nhân chúng với
> 20% sẽ biến lần retract thành vô nghĩa. Đây là lý do script phải phân biệt.

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `force_temperatures` | bật | Ép nhiệt độ toàn file |
| `hotend_temp` | 230 °C | `M104`/`M109` bị ép về số này |
| `bed_temp` | 60 °C | `M140`/`M190` bị ép về số này |
| `double_first_layer` | bật | In layer 0 hai lần |
| `z_mode` | `split` | `split` = chia 2 lớp mỏng (không đổi Z) · `shift` = nâng Z + `G92` |
| `first_pass_z` | 0.1 mm | Z của pass 1 (chế độ `split`) — phải **nhỏ hơn** chiều cao lớp đầu |
| `z_raise` / `layer0_z` | 0.4 / 0.2 mm | Chỉ dùng ở chế độ `shift` |
| `pass1_flow` | 20 % | Flow lần in đầu của layer 0 |
| `pass2_flow` | 80 % | Flow lần in thứ hai của layer 0 |
| `z_feedrate` | 600 mm/min | Tốc độ di chuyển Z giữa hai pass |

**Bật trong Cura:** `Extensions` → `Post Processing` → `Add a script` → **First Layer Twice**.
Script nằm ở `%APPDATA%\cura\<version>\scripts\FirstLayerTwice.py` và **chỉ được nạp lúc Cura
khởi động** — thêm file xong phải mở lại Cura.

> Cura yêu cầu **tên class trùng tên file** (`PostProcessingPlugin.py:215`:
> `getattr(loaded_script, script_name)`). Đặt tên khác là Cura báo *"not a recognised script type"*.

Những chỗ profile sửa so với bản Voron gốc của Cura:

| Thiết lập | Bản gốc Cura | Máy này | Vì sao |
|---|---|---|---|
| `machine_center_is_zero` | **True** | **False** | Firmware có gốc `(0,0)` ở **góc trước-trái** bàn. Để `True` là Cura dồn bản in lệch nửa bàn |
| `machine_endstop_positive_direction_x/y` | `True` | **`False`** | `X/Y/Z_HOME_DIR -1` — máy home về **MIN**, Voron gốc home về MAX |
| `machine_width/depth` | 300 | **305** | vùng in thật |
| `machine_height` | 300 | 300 | |
| `machine_max_feedrate_x/y` | 500 | **300** | khớp `M203 X300 Y300`; **`speed_travel` bắt nguồn từ đây** — xem cảnh báo bên dưới |
| `machine_max_feedrate_z/e` | 40 / 120 | **10 / 25** | `M203 Z10 E25` |
| `machine_max_acceleration_x/y` | 20000 | **500** | **trần cứng** — khớp `M201 X500 Y500` của firmware |
| `machine_max_acceleration_z` | 500 | **100** | `M201 Z100` của firmware |
| `machine_max_acceleration_e` | (mặc định 10000) | **500** | |
| `acceleration_print` | 5000 | **500** | khớp trần `M201 X500 Y500` — xem giải thích bên dưới |
| `acceleration_travel` | (công thức → 7000) | **500** | đặt thẳng, **không** dùng công thức `voron2_base` nữa |
| `machine_acceleration` | 5000 | **500** | |
| `jerk_print` / `_travel` | (mặc định 20 / 30) | **8 / 8** | `M205 X8 Y8` — `CLASSIC_JERK` |
| `machine_max_jerk_xy` | (mặc định 20) | **8** | |
| `machine_steps_per_mm_z/e` | 400 / – | **800 / 415** | `M92` |
| `retraction_speed` / `_retract_speed` / `_prime_speed` | 30 / 25 / 25 | **15 / 15 / 15** | ngưỡng `machine_max_feedrate_e − 10 = 15`, xem cảnh báo bên trên |
| Start / End G-code | macro Klipper `PRINT_START ...` | **G-code Marlin** | Firmware là Marlin — `PRINT_START` sẽ bị báo lỗi và **không home/không hâm nóng** |

> 🔵 **Vì sao chặn ở 500 (đã chốt).** Marlin tính `accel_thực = min(M204 P, M201 trục)`. `M201 X/Y`
> của firmware là **trần cứng 500**, nên để Cura khai 1500 hay 5000 thì máy vẫn chỉ chạy 500 — con số
> trong Cura thành **nói dối**. Đã hạ **cả hai bên về 500**: `M201` trong EEPROM (không cần flash, chỉ
> cần `M201 X500 Y500` + `M500`) và toàn bộ `acceleration_*` của Cura — kể cả
> `machine_max_acceleration_x/y` để Cura không cho đặt cao hơn. Muốn nhanh hơn về sau thì phải nâng
> **cả hai** cùng lúc.

> 📐 **Các mức còn lại tự suy ra, không cần đặt tay.** Mọi `acceleration_*` khác của Cura đều tính từ
> `acceleration_print` (hoặc từ `voron2_base`), nên khi `acceleration_print = 500` thì:
> `acceleration_wall` / `_topbottom` / `_infill` = **500**, `acceleration_support` = **250**,
> `acceleration_roofing` = `acceleration_wall_0` = **300**, `acceleration_layer_0` = **50**
> (lớp đầu chậm — chủ ý của UltiMaker), `acceleration_ironing` / `_flooring` = **500**.
> Tất cả đều ≤ 500 nên firmware **không kẹp chỗ nào nữa**.

> ⚠️ **Cura KHÔNG phát `M204 T`** — nó chỉ phát `M204 S<n>` (acceleration in). Kiểm chứng trên
> `V300_Part3.gcode`: chỉ có `M204 S150`, `S488`, `S825`, `S1162`… và **không có lệnh `M204 T` nào**.
> Nên `acceleration_travel` của Cura **chỉ ảnh hưởng phần ước lượng thời gian in**, còn travel
> acceleration thật của máy là `M204 T` lưu trong EEPROM.

> ⚠️ **`G0 F30000` không phải lỗi — nó là hệ quả của `machine_max_feedrate_x/y`.** `voron2_base` tính
> `speed_travel` bằng:
> ```
> speed_travel = max(speed_print, round((machine_max_feedrate_x + machine_max_feedrate_y) / 2, -2))
> ```
> Với `machine_max_feedrate_x/y = 500` → `round(500, -2) = 500` mm/s → Cura phát **`G0 F30000`**.
> Với **300** → `speed_travel = 300` mm/s → **`G0 F18000`**.
>
> **Đừng nhầm đơn vị:** g-code `F` là **mm/phút**, còn `M203` và Cura là **mm/giây**
> (`fdmprinter.def.json` ghi `"unit": "mm/s"`). `30000 ÷ 60 = 500` — tức `F30000` **bằng đúng trần**,
> không phải vượt. Và kể cả vượt thì Marlin cũng **kẹp** chứ không báo lỗi (`planner.cpp:2415-2419`:
> `if (cs > max_fr) NOMORE(speed_factor, max_fr / cs);`).
>
> ℹ️ `voron2_base` đặt `speed_travel.maximum_value_warning = max(500, round((mx+my)/2, -2)) + 1` nên
> giá trị suy ra luôn nằm dưới ngưỡng cảnh báo — hạ `machine_max_feedrate_x/y` **không** làm Cura
> báo lỗi. Ngưỡng cứng `maximum_value` của mọi `speed_*` là `√(mx²+my²)` = 424 mm/s khi mx=my=300,
> còn giá trị thật chỉ 30–120 mm/s.

### 11.7 Những chỗ dễ sai — đọc trước khi sửa

Đây là các cạm bẫy đã **thực sự gặp** trên máy này, mỗi cái tốn ít nhất một lần build + flash vô ích.

| # | Cạm bẫy | Hệ quả | Cách đúng |
|---|---|---|---|
| 1 | **EEPROM đè lên code.** `M851`, `M422`, `M92`, `M203`, `M201`, `M204`, `M205` đều lưu trong EEPROM | Sửa `Configuration.h` rồi flash mà giá trị vẫn cũ | Sửa cả hai: code **và** gửi lệnh tương ứng + `M500` |
| 2 | **Đừng dùng `M502` để "nạp lại mặc định"** | Xoá luôn `M851 Z0.70` (offset đã cân), mesh, điểm G34 | Dùng `M422` / `M851` cho từng giá trị |
| 3 | **Hướng extruder chỉ nằm trong firmware** (`INVERT_E0_DIR`) | Cura không có setting nào đảo chiều, sửa Cura vô ích | Sửa firmware + flash |
| 4 | **`Z_AFTER_PROBING` bị comment → `move_z_after_probing()` rỗng** | `G28` kết thúc với nozzle **nằm trên bàn**, lệnh XY sau đó **kéo nozzle quét mặt bàn** | Bật `Z_AFTER_PROBING` |
| 5 | **Marlin lấy `min(M204 P, M201 trục)`** | Cura khai 5000 mà `M201 X/Y` = 500 → máy vẫn chỉ chạy 500, con số trong Cura thành vô nghĩa | Cho **hai bên bằng nhau**. Máy này chặn ở **500**: `M201 X500 Y500` + `M500` (không cần flash) và hạ toàn bộ `acceleration_*` của Cura về 500 |
| 6 | **Cura lưu thông số ở 3 container khác nhau** | Ghi sai container → Cura **âm thầm bỏ qua** | Xem bảng ở 11.6 |
| 7 | **`fdmextruder.def.json` không có `inherits`** | Setting của `fdmprinter` (vd `retraction_speed`) **không tồn tại** trong definition `Toolhead` | Đặt vào container `user` của extruder (khai `definition = voron2_300`) |
| 8 | **`voron2_base` đặt `maximum_value_warning = machine_max_feedrate_e − 10`** cho 3 tốc độ retract | Hạ `machine_max_feedrate_e` xuống 25 → ngưỡng 15 → Cura **chặn slice** | Đặt retract ≤ ngưỡng, hoặc nâng `M203 E` |
| 9 | **Cura ghi đè file cấu hình khi thoát** | Ghi file lúc Cura đang mở → mất sạch khi đóng Cura | **Đóng Cura trước**; script đã tự từ chối nếu thấy tiến trình Cura |
| 10 | **`microsteps` đọc ra 1/8 thay vì 16** | `refresh_stepping_mode()` ghi đè GCONF từ cache, chân MS1/MS2 không được điều khiển → **trục chạy gấp đôi** | Đã sửa trong `settings.cpp`, xem 11.4 |
| 11 | **Chạy USB không có PSU** | TMC2209 undervoltage → **kéo cứng đường endstop lên HIGH**, mọi endstop báo `TRIGGERED` | Luôn cấp nguồn PSU khi kiểm tra endstop |
| 12 | **Dựng mesh UBL trước khi căn gantry (G34)** | G34 nghiêng lại gantry → mesh cũ hiệu chỉnh **thừa** đúng phần vừa sửa; probe gắn trên gantry nên mesh mã hoá luôn độ nghiêng lúc đo | Luôn **G34 trước, `G29` sau**; tháo/lắp gantry, đổi belt, đổi Z-stepper thì `G29` lại |
| 13 | **Với UBL, `M420 S1` không kiểm tra mesh hợp lệ** (`bedlevel.cpp:62` chỉ check cho `AUTO_BED_LEVELING_BILINEAR`) | `M420 S1` bật leveling trên mesh hỏng → in ra rác mà **không báo lỗi gì** | Xem `M420 V` phải in `Mesh is valid` trước khi tin |
| 14 | **Grep `locked_Z_motor` không thấy chỗ nào *đọc*** | Tưởng cơ chế khoá Z-stepper là no-op → đi "sửa" một thứ đang chạy đúng, tốn cả buổi | Nó dùng **macro nối token**: `locked_##A##_motor` (`stepper.cpp:326-328`, `TRIPLE_SEPARATE_APPLY_STEP`). Grep chữ literal **không bao giờ thấy** |
| 15 | **Build lỗi `*** [.pio\build\...\SrcWrapper\src] ... cannot find the path specified`** | Build dir hỏng → PlatformIO không tạo lại được thư mục wrapper, build fail ngay | **Xoá `.pio\build\mks_monster8` rồi build lại** — đã gặp và sửa trong 38 s |
| 16 | **`G34 I<n>` không có tác dụng** | `G34()` gọi `InfiniteG34(3)` với `nloop=3` cứng, nên `parser.intval('I', …)` không bao giờ được đọc | Giới hạn số vòng bằng `G34 Q<n>`; đổi ngưỡng bằng `G34 T<acc>` |
| 17 | **`FirstLayerTwice` đọc nhầm Z-hop thành chiều cao layer** | `voron2_base` bật Z-hop 0.2 → bước `G0/G1` đầu tiên của layer 0 là `Z0.4` (hop) chứ không phải `Z0.2` → `offset = 0.4 − 0.1 = 0.3` → chiều cao thật thành **`Z-0.1`**; Marlin có `Z_MIN_POS 0` nên kẹp về 0 → **pass 1 in ngay trên mặt bàn** | `_find_layer_z()` phải lấy **min** Z trong body, không lấy Z đầu tiên. Test hồi quy: `tests/test_first_layer_twice.py` |
| 18 | **Thấy `G0 F30000` tưởng vượt trần máy** | G-code `F` là **mm/phút** còn `M203`/Cura là **mm/giây** — `F30000` = 500 mm/s, đúng bằng trần chứ không vượt. Và `speed_travel` của Cura **suy ra từ `machine_max_feedrate_x/y`** nên đổi trần là đổi luôn con số này | Đổi đơn vị trước khi kết luận (`mm/s × 60`). Marlin **kẹp** feedrate chứ không báo lỗi (`planner.cpp:2419`) |
| 19 | **Đặt setting sai container → Cura XOÁ ÂM THẦM khi ghi lại** | 11 key bị xoá khỏi `definition_changes` (container cấp **MÁY**): `acceleration_print`, `acceleration_travel`, `jerk_print`, `jerk_travel`, `machine_steps_per_mm_x/y/z/e`, `machine_endstop_positive_direction_x/y/z`. Chúng **chỉ có tác dụng cho lần slice ĐẦU** sau khi cài, rồi biến mất. Hệ quả thật: `acceleration_print` rơi về mặc định **5000** của `voron2_base` → G-code chứa `M204 S5000`; `machine_endstop_positive_direction_*` mất nên Cura tưởng máy home về MAX | Đặt đúng container: `settable_per_mesh: true` (print setting) → container `user` của máy in; `settable_per_extruder: true` → container **extruder**. Cura cũng tự làm đúng như vậy (`cura/Settings/MachineManager.py:976-992`). Xem §11.6 |

### 11.8 `G34 Q<n>` — lặp căn gantry tới khi đạt

Tham số do dự án này thêm vào (`G34_M422.cpp:90`):

```c
int8_t isInf = parser.intval('Q', 1);          // mac dinh 1 = chay nhu G34 goc
...
while ((isInf-- > 0) && !InfiniteG34(3)) { }   // lap, home lai sau moi 3 lan do
```

`InfiniteG34()` trả về `true` khi căn xong trong ngưỡng, và vòng `while` **dừng ngay** khi đó
(điều kiện `!InfiniteG34(3)` thành false) hoặc khi hết `Q` lần.

| | |
|---|---|
| `G34` | 1 lần, như Marlin gốc |
| **`G34 Q99`** | lặp tối đa 99 lần, **dừng ngay khi đạt** |
| Ngưỡng dừng | `Z_STEPPER_ALIGN_ACC` = **0.02** (`Configuration_adv.h:1026`), đổi bằng `T<acc>` |
| Mỗi vòng | 3 iteration (tham số `nloop` truyền vào `InfiniteG34`), **home lại Z ở giữa** |

> Thực tế `Q99` gần như tương đương "chạy tới khi xong": gần như không bao giờ chạm 99 lần, vì
> mỗi vòng đã home lại nên sai số giảm dần. Đặt `Q` nhỏ (1–3) nếu muốn giới hạn thời gian chờ.

#### Huỷ G34 bằng nút encoder

Vì `Q99` có thể chạy lâu, **bấm nút encoder bất kỳ lúc nào là dừng G34** (dự án này thêm vào).

```c
// G34_M422.cpp — doc thang chan BTN_ENC, khong phu thuoc vong lap giao dien
if (ui.button_pressed()) { g34_cancelled_by_user = true; err_break = true; break; }
```

| | |
|---|---|
| Đọc bằng gì | `ui.button_pressed()` → `hw_button_pressed()`, đọc **thẳng chân `BTN_ENC`** và có debounce (`ENCODER_SAMPLES`) — nên dùng được trong lúc G34 đang chặn |
| Kiểm tra ở đâu | **Trước từng điểm probe** (`LOOP_L_N(i, NUM_Z_STEPPERS)`) → phản hồi trong khoảng **một lần probe (~2–5 s)**; và ở đầu mỗi vòng lặp iteration |
| Thông báo | Serial: `G34 cancelled by encoder button.` · LCD: `G34 STOP` |
| Sau khi huỷ | `HOME_AFTER_G34` đang bật → **Z được home lại**; probe được stow; leveling được khôi phục. Bản in **vẫn tiếp tục** bình thường |

> **Cờ `g34_cancelled_by_user` phải ở phạm vi FILE.** `InfiniteG34()` trả về `true/false` để
> vòng lặp `Q` biết "đã xong chưa" — mà **"bị huỷ" khác với "đã xong"**. Nếu chỉ dựa vào giá trị
> trả về thì vòng `Q` sẽ **chạy lại tiếp** và nút bấm thành vô nghĩa. Vòng lặp ngoài vì vậy kiểm
> tra thêm cờ: `while ((isInf-- > 0) && !g34_cancelled_by_user && !InfiniteG34(3))`.

> **Guard khi `HOME_AFTER_G34` bị tắt:** nếu huỷ *trước khi probe được điểm nào*,
> `z_measured_min` còn là giá trị rác `100000.0f`, và nhánh `#else` sẽ trừ nó vào
> `current_position.z` → ra toạ độ vô lý. Script firmware nay kiểm tra cờ và **home lại Z** thay
> vì trừ. Trên máy này `HOME_AFTER_G34` đang bật nên nhánh đó không chạy, nhưng guard vẫn giữ.

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
