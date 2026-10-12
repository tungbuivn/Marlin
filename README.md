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
| Microstep | **16** cho cả 6 driver, có nội suy (interpolation) + stealthChop. Xem §11.4b — đã thử 1/8 cho X/Y và **phải hoàn tác** |
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
| Mọi điểm đo phải tới được | `tramming.h:72-76` (`static_assert` + `VALIDATE_TRAMMING_POINT`), `z_stepper_align.cpp:57` (`static_assert`) | **Lưới an toàn**: đổi giới hạn trục mà quên dời điểm đo → build báo lỗi ngay |

---

## 3. Thông số chuyển động

| Thông số | Giá trị | Nguồn |
|---|---|---|
| Steps/mm | `M92 X80 Y80 Z800 E415` | 200 bước/vòng × 16 microstep ÷ 4 mm |
| Max feedrate (mm/s) | `M203 X150 Y150 Z10 E25` | đã tối ưu — xem §3.3 (trước là 100, thiết kế cũ ghi 300) |
| Accel (mm/s²) | `M201 X300 Y300 Z200 E1000` | Marlin lấy `min(M204 P, M201)` |
| Accel print/retract/travel | `M204 P500 R300 T500` | Cura đặt lại `M204 S…` mỗi lần in |
| Jerk | `M205 X8 Y8 Z0.40 E5` | `CLASSIC_JERK` + `S_CURVE_ACCELERATION` |
| Dòng driver X/Y | `M906 X800 Y800` | ⚠️ **100 % định mức** của motor 0,8 A — kiểm nhiệt `≤ 80 °C` |
| Homing feedrate | X/Y 3000, Z **480** mm/min | Z = 8 mm/s < max 10 mm/s |
| Soft endstop | `M211 S1` | |

> 📐 **Vì sao `M203` chỉ 150 chứ không 300: xem §3.3.** Motor này có **điện cảm `17 mH`** — ở 24 V
> chỉ kéo đủ dòng định mức tới **~225 mm/s**, và vì `stealthChop` đang bật toàn dải
> (`HYBRID_THRESHOLD` bị comment) nên trần thực tế còn thấp hơn, **~100–150 mm/s**. Con số 300 trong
> bản thiết kế cũ không có cơ sở vật lý cho motor này.

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
> ℹ️ **Source và EEPROM nay đã đồng bộ với nhau.** `Configuration.h` ghi
> `DEFAULT_MAX_ACCELERATION { 2000, 2000, 200, 1000 }` và `Configuration_adv.h` ghi `X/Y_CURRENT 600`,
> `Z/Z2/Z3_CURRENT 500` — **đúng bằng** giá trị đang nằm trong EEPROM. Giá trị **biên dịch** chỉ có
> tác dụng khi chạy `M502` (đừng chạy) hoặc sau khi xoá EEPROM; bình thường **EEPROM thắng**. Đồng bộ
> như vậy để nếu buộc phải `M502` thì máy rơi về đúng bộ đã tune, chứ không rơi về số cũ.

### 3.2 `INVERT_E0_DIR` — hướng extruder

```c
#define INVERT_E0_DIR false   // Bondtech BMG la extruder CO HOP SO (E = 415 steps/mm)
```
Comment ngay trên option đó trong Marlin: *"for direct drive extruder v9 set to true, for **geared
extruder** set to false"*. Để `true` thì `G1 E10` **rút** thay vì đẩy. Chiều quay **không** lưu
trong EEPROM — chỉ có trong firmware, phải flash mới đổi được.

### 3.3 Tối ưu vận tốc & gia tốc cho X/Y (NEMA17 + GT2 20 răng)

**Dữ kiện**

| | |
|---|---|
| Motor | NEMA17 1,8° — Nanotec **ST4118L0804-A**: `0,8 A` · `9,3 Ω` · **`17 mH`** · rotor `83 g·cm²` · `0,34 kg` |
| Pulley | GT2, **20 răng**, bước răng **2 mm** → chu vi **40 mm/vòng** |
| Microstep | **16** → `steps/mm = 200 × 16 ÷ 40 = 80` |
| Bước nhỏ nhất | `1/80 = 0,0125 mm` (mỗi microstep) |
| Bán kính hiệu dụng pully | `40 ÷ 2π = 6,366 mm` |
| Gia tốc trọng trường quy đổi | rotor `83 g·cm²` → `J/r² ≈ 0,205 kg` mỗi motor → **`m_eff ≈ 1,2 kg`** (đầu in + belt + 2 rotor) |

**Tốc độ ↔ vòng tua ↔ tần số xung**

| Tốc độ | vòng/s | RPM | Xung/s mỗi motor | Tần số điện |
|---|---|---|---|---|
| 100 mm/s | 2,5 | 150 | 8 000 | 125 Hz |
| 150 mm/s | 3,75 | 225 | 12 000 | 188 Hz |
| **200 mm/s** | 5,0 | 300 | 16 000 | 250 Hz |
| 250 mm/s | 6,25 | 375 | 20 000 | 313 Hz |
| 300 mm/s | 7,5 | 450 | 24 000 | 375 Hz |

> ℹ️ **STM32F407 không phải giới hạn.** Marlin sinh được ~150–200 k xung/s; ở 300 mm/s mới cần
> 24 k (đường chéo CoreXY: `80 × v × √2` → 34 k mỗi motor, 68 k tổng). Còn rất xa trần.

**Trần do ĐIỆN CẢM — đây mới là giới hạn thật của motor này**

Cuộn dây `17 mH` ở `24 V` chỉ còn kéo đủ dòng định mức khi:

```
|Z| = √(R² + (2πfL)²) ≤ V / I   →   f ≤ ~281 Hz   →   v ≤ ~225 mm/s
```

| Tốc độ | Dòng tối đa driver còn bơm được | Mô-men còn lại (ước tính) |
|---|---|---|
| ≤ 210 mm/s | đủ `0,8 A` | ~100 % → giảm dần theo `R-L` |
| 300 mm/s | ~`0,60 A` | ~⅓ định mức |
| 500 mm/s | ~`0,36 A` | rất thấp, dễ mất bước |

> 🔴 **`~210–225 mm/s` là mốc vật lý của motor này ở 24 V** với `17 mH`. Vượt qua đó không phải
> "không chạy được" mà là **mô-men tụt nhanh** — tăng tốc kém và dễ mất bước khi in. Máy nào chạy
> 300–500 mm/s là nhờ motor điện cảm thấp hơn (5–8 mH), không phải nhờ firmware.

**Mô-men → lực → gia tốc**

Cân bằng công suất cho CoreXY (belt đi 1:1 với đầu in ở chuyển động thuần trục):
`F_đầu in ≈ 2 × τ / r`. Với `τ ≈ 0,4 N·m` (giá trị danh định dải ST4118 48 mm — **cần đối chiếu
datasheet**), `F ≈ 126 N` → `a = F/m_eff ≈ 100 000 mm/s²`. Kể cả chỉ còn 10 % mô-men ở tốc độ cao
thì vẫn `~10 000 mm/s²`.

> 🔵 **Kết luận quan trọng: mô-men KHÔNG phải giới hạn của gia tốc.** Dư địa gấp 10–50 lần con số
> đang đặt. Giới hạn thật là **chất lượng in** (ringing/ghosting), **độ cứng khung–belt**, và
> **chế độ cắt của driver** (bên dưới) — không phải motor.

**Đối chiếu: trước → đã áp dụng → trần nên dùng**

Bộ dưới đây **đã được ghi vào EEPROM** với lựa chọn **giữ `stealthChop` toàn dải**.

> 🟢 **2026-10-12 (cuối ngày) — QUAY VỀ BỘ "TRƯỚC (1/16)"**, chỉ quy đổi phần **phụ thuộc microstep**:
> `M92` = **40 / 40 / 400 / 207.5** (bản 1/16 là 80/80/800/415). Vận tốc · gia tốc · jerk giữ nguyên như
> bản 1/16 vì đơn vị là mm/s · mm/s² — **không liên quan microstep**; riêng **dòng motor đặt 600 mA cho
> cả 6 trục** (không lấy 800/600/400 của cột 1/16). Đã ghi EEPROM (`M500`, crc 6571) → **verify lại sau
> khi reset board** → và sửa **giá trị mặc định** trong `Configuration.h` / `Configuration_adv.h` cho khớp
> (phần dòng motor **chưa flash**, xem ghi chú dưới bảng).

| Lệnh | Trước (1/16) | **Máy đang là (1/8)** | Trần nên dùng | Vì sao |
|---|---|---|---|---|
| `M92 X/Y/Z/E` | 80 / 80 / 800 / 415 | **40 / 40 / 400 / 207.5** | — | **chỗ DUY NHẤT phải quy đổi** cho microstep 1/8 |
| `M203 X/Y` | 150 | **150** | 225 | mốc điện cảm — 150 là mức chạy tốt với stealthChop toàn dải |
| `M203 Z` | 10 | **10** | 15 | vít me 4 mm → 150 RPM ở 10 mm/s |
| `M203 E` | 25 | **25** | 25 | BMG |
| `M201 X/Y` | 300 | **300** | 5000+ | mô-men dư 10–50×; trần thật là ringing |
| `M201 Z` | 200 | **200** | 500 | gantry nặng |
| `M201 E` | 1000 | **1000** | 1000 | |
| `M204 P` (in) | 500 | **500** | 3000 | ⚠️ `P > M201 X/Y 300` ⇒ Marlin kẹp `min()` → thực tế **300** |
| `M204 R` (retract) | 300 | **300** | 1500 | |
| `M204 T` (travel) | 500 | **500** | 2500 | ⚠️ thực tế bị kẹp còn **300** |
| `M205 X/Y` (jerk) | 8 | **8** | 10–12 | ⚠️ **không có input shaping** → tăng là tăng ringing |
| `M205 Z / E` | 0,4 / 5 | **0,4 / 5** | | |
| `M906` (dòng) | X/Y 800 · Z 600 · E 400 | **600 cho CẢ 6 trục** | 500 | ✅ 600 mA = 75 % định mức 0,8 A — vẫn nên kiểm nhiệt `≤ 80 °C` |

> ⚠️ **`M204 P/T` lại lớn hơn `M201 X/Y`, nên `M503` "nói dối" trở lại** (Marlin lấy `min()`): gia tốc
> in thật là **300**, không phải 500. Không sai về hành vi — chỉ gây hiểu nhầm khi đọc số. Muốn số khớp
> thực tế thì `M204 P300 R300 T300` + `M500`.
>
> ✅ **Dòng motor nay 600 mA cho CẢ 6 trục** (75 % định mức 0,8 A của ST4118L0804-A) — mát hơn mức
> 800 mA trước đây, và E dùng 600 thay vì 400. Vẫn kiểm sau ~30 phút in (`≤ 80 °C` là tốt). Muốn thêm
> mô-men thì nâng `M906` (tối đa 800 = 100 % định mức); nóng quá thì hạ.
> ✅ **Đã nạp vào firmware** (`Marlin 2.1.2 (Oct 12 2026 19:49:38)`): default trong
> `Configuration_adv.h` = **600 cho cả 6 trục**, khớp EEPROM. `M122` sau khi nạp: `Set current 600 ×6`,
> `msteps 8 ×6`, `off time 3`, 6/6 `connection OK` — và **EEPROM giữ nguyên** qua lần flash này.
>
> ✅ **Cura / `ClampFeeds` nay KHỚP máy**: profile ghi `max_feedrate_xy 150`, `max_acceleration 300` —
> đúng bằng `M203 X/Y 150` và `M201 X/Y 300` của máy, nên G-code Cura phát ra **không bị firmware kẹp**
> và `verify_gcode.py` kiểm đúng trần thật. (Muốn dùng trần cao hơn 150/225 thì phải sửa cả 4 file
> profile + `install-cura-profile.ps1` — xem cạm bẫy 26.)

**Hai chỗ phía Cura — ✅ ĐÃ SỬA (khớp với bản 1/16, xem cảnh báo ngay trên):**

| Chỗ | Việc đã làm |
|---|---|
| **Container máy của Cura** | `machine_max_feedrate_x/y` 300 → **150**, `machine_max_acceleration_x/y` → **300**, `machine_acceleration` → **300**, `machine_max_acceleration_z` → **200**, `_e` → **1000**. Đây là chỗ **quyết định con số Cura phát ra**: `M204 S500` trước đây không đến từ profile mà do `machine_max_acceleration_x/y` **kẹp giá trị mặc định 5000** của Cura xuống |
| **`ClampFeeds`** | `max_feedrate_xy` **150**, `max_acceleration` **300** — sửa ở **cả** `ClampFeeds.py` **và** khối setting đang lưu trong Cura (xem cạm bẫy 26). Đã cài lại bằng `install-cura-profile.ps1` |

> ✅ **Đã kiểm chứng sau khi cài**: container máy trong `%APPDATA%\cura\5.13\definition_changes\` giữ
> `machine_max_feedrate_x/y = 150`, `machine_max_acceleration_x/y = 2000`; khối đang lưu của
> `ClampFeeds` giữ `max_feedrate_xy = 150`, `max_acceleration = 2000`; `ClampFeeds.py` đã cài có
> `default_value` 150 và 2000. `tests/test_clamp_feeds.py` PASS với trần mới.

> 📌 **Việc còn lại là SLICE LẠI.** File `D:\0in\V300_Part3_proj.gcode` hiện có là bản **cũ**
> (`M204 S500`, `F18000` = 300 mm/s, và `FirstLayerTwice` theo cơ chế cũ). `tests/verify_gcode.py`
> chạy trên nó báo: **276 bước vượt trần feedrate** + **`buoc pass 2 -> layer 1 = 0.40, phải là 0.2`**.
> Đó là báo cáo đúng, không phải lỗi công cụ — chỉ cần mở Cura slice lại.

> 🔵 **Đã giữ `stealthChop` toàn dải theo lựa chọn.** Hệ quả: driver không bao giờ chuyển sang
> spreadCycle, mà stealthChop là chopper **điện áp** — rất kém ở tốc độ cao, tệ nhất với motor
> **điện cảm cao `17 mH`**. Đây là lý do số 1 gây mất bước khi vượt ~100–150 mm/s. **Nếu thấy
> layer shift ở travel nhanh, đây là thủ phạm đầu tiên cần nghĩ tới** — chứ không phải gia tốc.
>
> | Cách | Việc | Đánh đổi |
> |---|---|---|
> | **Tốt nhất — cần flash** | Bỏ comment `#define HYBRID_THRESHOLD` (`Configuration_adv.h:3123`), clean rebuild + flash | Tự chuyển chế độ ở 100 mm/s: êm khi in chậm, khỏe khi travel nhanh |
> | **Chỉ EEPROM** | `M569 S0 X Y` + `M500` → X/Y chạy **spreadCycle thường trực** | Khỏe, ổn định ở tốc độ cao, nhưng **ồn hơn rõ rệt**. Marlin lưu chế độ này **trong EEPROM** (`settings.cpp` dùng `tmc_stealth_enabled`) |
>
> ℹ️ **Input shaping KHÔNG dùng được trên máy này.** `SanityCheck.h:4332-4334` chặn thẳng:
> `"INPUT_SHAPING_X is not supported with COREXY, COREYX, COREXZ, COREZX, or MARKFORGED_*."`
> Nên muốn tăng gia tốc mà không ringing thì chỉ còn cách làm cứng khung/gantry — không có đường
> phần mềm.

**Quy trình tìm giới hạn thật (đừng lấy số trong bảng làm điểm dừng)**

1. Chạy `M503` đối chiếu EEPROM đã đúng chưa (mỗi lần `M500` in ra `crc`).
2. In **tháp gia tốc** (acceleration tower) hoặc một khối có góc nhọn ở 1500 → 2000 → 2500 mm/s².
3. Nhìn **ghosting sau góc** và **layer shift**:
   - **Ghosting** → giảm gia tốc.
   - **Layer shift** → đó là **MẤT BƯỚC**, không phải ringing. Phải giảm `M203` **hoặc** tăng
     `M906` **hoặc** bật hybrid threshold — giảm gia tốc thường **không** chữa được.
4. Nghe tiếng máy: spreadCycle rít/ồn hơn stealthChop là bình thường.
5. Kiểm nhiệt độ motor sau 30 phút in: `≤ 80 °C` an toàn; nóng hơn thì hạ `M906`.

> ⚠️ **Số `0,4 N·m` là giá trị danh định của dải ST4118 48 mm, KHÔNG lấy được từ datasheet gốc**
> (trang Nanotec là JS, file PDF trả 404). Nếu datasheet thật khác thì thay vào công thức
> `F = 2τ/r` là ra lại toàn bộ bảng — kết luận "mô-men không phải giới hạn" chỉ sai nếu mô-men
> thật **nhỏ hơn ~20 lần**.

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
| Đo bằng gì | `probe.probe_at_point(tramming_points[i], …)` — `G35.cpp:150`, `menu_tramming.cpp:190` |
| Mốc vật lý | **`z_home` = `base_home_pos(Z_AXIS)`** — khung Z của lần `G28 Z` (Z_SAFE_HOMING probe ở tâm bàn). **Không probe thêm tâm bàn** |
| Mốc để vặn vít | **`Z-MIN` (chế độ MAX, mặc định) hoặc `Z-MAX` (chế độ MIN)** của các góc đã đo: `delta = chiều_cao − mốc` — đây mới là số cần đưa về 0. Cả hai mốc được **chốt một lần** sau lần đo đủ 4 góc đầu tiên |
| Tính gì | `chiều_cao = z_probe − z_home`, **mốc so sánh = `Z-MIN` (chế độ MAX) hoặc `Z-MAX` (chế độ MIN)** của các góc đã đo ⇒ `delta = chiều_cao − mốc`. **Số cần chỉnh in theo MM, KHÔNG phải vòng**: vặn vít đi `X` mm thì góc đó lên/xuống đúng `X` mm ⇒ **lượng cần vặn = `\|delta\|` mm** (không chia cho bước ren). **Chiều vặn là chuyện riêng**, do `tramming_turn_cw()` quyết định (`tramming.h:61-64`) |
| Bước ren | `TRAMMING_SCREW_THREAD 40` → M4 × **0.7 mm/vòng** (`threads_factor = {0.5 M3, 0.7 M4, 0.8 M5}`). **Từ 2026-10-12 KHÔNG dùng để tính số hiển thị nữa** (hiển thị theo mm); mã `S` vẫn có tác dụng: ren ngược chiều thì khai `S` lẻ để **đảo chiều vặn** |
| Kết quả | `Turn <góc>: van CW/CCW x.xxxmm \| lech x.xxxmm (cao z.zzzmm)` — **CCW = vặn để HẠ góc đó xuống, CW = vặn để NÂNG lên** (đã **đảo lại 2026-10-12**, xem ghi chú dưới) |

> ℹ️ **Vì sao `chiều_cao = z_probe − z_home` mà không cần probe tâm bàn:** Z home bằng chính
> probe này, nên lúc trigger ở tâm bàn firmware gán `current_position.z = base_home_pos(Z) − probe.offset.z`
> (`set_axis_is_at_home`, `motion.cpp:2349`); `probe_at_point()` lại **cộng** `probe.offset.z` vào kết quả
> (`probe.cpp:891`). Hai số bù nhau ⇒ **giá trị probe trả về ở bất kỳ điểm nào đã chính là
> "z_probe − z_home"**. Probe lại tâm bàn chỉ tốn ~10 giây và **cộng sai số của lần đo đó vào cả 4 số**.
> (Nếu vặn vít rồi mà không home lại thì mốc vẫn là mốc cũ — giống wizard: khi đã hết góc **chưa chấp nhận** máy **tự** home lại Z + đo lại 4 góc, bấm `DONE` là **chấp nhận góc đang đứng** (coi như xong) chứ không home lại.)

> 🔴 **Vì sao KHÔNG dùng thẳng "chiều cao so với Z-home":** ví dụ 4 góc đo được `2.000 / 2.000 /
> 2.000 / 2.030 mm`. Nhìn "chiều cao so với Z-home" thì cả 4 góc đều ≈ **+2mm** — trông như cả bàn bị
> nâng 2mm, nhưng thực ra **4 góc chỉ lệch nhau 0.030mm**. Con số 2mm chỉ là **độ lệch của cả mặt bàn
> so với mốc Z-home** (phụ thuộc `M851`, độ võng tâm bàn, lần home…) và **không nói gì về độ phẳng**.
>
> 🎯 **Mốc so sánh = `Z-MIN` hoặc `Z-MAX` của các góc đã đo** (không phải trung bình):
> ```
> chế độ MAX (mặc định) -> mốc = Z-MIN = 2.000
>     delta = 0.000 / 0.000 / 0.000 / +0.030  -> chỉ vặn 1 góc 0.030mm, và chỉ VẶN XUỐNG (CCW)
> chế độ MIN            -> mốc = Z-MAX = 2.030
>     delta = -0.030 / -0.030 / -0.030 / 0.000 -> chỉ vặn 3 góc kia, và chỉ VẶN LÊN (CW)
> ```
> Nhờ vậy **chiến lược luôn MỘT CHIỀU**: hoặc chỉ **hạ** các góc cao xuống bằng góc thấp nhất, hoặc chỉ
> **nâng** các góc thấp lên bằng góc cao nhất — **không bao giờ phải vừa nâng vừa hạ**. Đổi chế độ bằng
> nút `MAX->MIN` trong wizard hoặc `Motion → WORST POINT = MAX`; mốc (`Z-MIN` và `Z-MAX`) được **chốt
> ngay sau lần đo đủ 4 góc đầu tiên** và không tính lại, nên `PROBE` lại 1 góc chỉ đổi số của góc đó.
>
> `delta > 0` (chế độ MAX) = góc **cao hơn** mốc ⇒ phải **hạ xuống** (vặn **CCW**); `delta < 0` (chế độ
> MIN) ⇒ **nâng lên** (vặn **CW**).
>
> 🔧 **Chiều vặn — ĐÃ ĐẢO LẠI ngày 2026-10-12 (kiểm chứng bằng cách vặn thử trên máy):**
> *vặn CW (kim đồng hồ) thì delta Z **TĂNG*** (góc đó **NÂNG lên**), *vặn CCW thì delta Z **giảm***
> (góc đó **HẠ xuống**) — đầu vít nằm **trên** bàn, vít M4. Ghi chú cũ trong tài liệu này ghi ngược lại
> (CW = hạ) và **SAI** — người dùng phát hiện khi làm theo hướng dẫn của `G35`.
> `tramming_turn_cw()` (`src/feature/tramming.h`) nay viết theo quy luật đúng:
> **delta > 0 (góc cao) → CCW** (hạ xuống), **delta < 0 (góc thấp) → CW** (nâng lên).
> Nếu sau này thay loại vít **ren ngược chiều** thì chỉ cần khai báo mã `S` **lẻ** (`TRAMMING_SCREW_THREAD 41`
> hoặc `G35 S41`) — code tự đảo chiều, không phải sửa gì thêm.


**Cảm biến được bật/tắt tự động** — không cần `M401` trước:
`probe_at_point()` → `deploy()` → `Probe::set_deployed()` → `Endstops::enable_z_probe(true)`
→ `WRITE(PROBE_ENABLE_PIN, on)` (`endstops.cpp:466`), tức chân **PA8 lên HIGH**. G35 kết thúc
bằng `probe.stow()` (`G35.cpp:267`) → PA8 về LOW.

Khác nhau giữa hai cách:

**Menu trên máy: `Motion` → `Tramming Wizard`.** (`LCD_LANGUAGE en` nên nhãn là tiếng Anh —
`MSG_TRAMMING_WIZARD` = *"Tramming Wizard"*, khai báo ở `menu_motion.cpp:404`.)

| | `G35` | Tramming Wizard (menu) |
|---|---|---|
| Cách chạy | Đo **cả 4 điểm một lượt** rồi in ra bảng số mm cần vặn | Đo **từng góc một**; mốc để vặn vít là **`Z-MIN` (chế độ MAX) hoặc `Z-MAX` (chế độ MIN)** của các góc đã đo (giống `G35`, `tramming_ref()` `menu_tramming.cpp:154-157`) — tức lấy **cực trị** làm mốc, **không** dùng trung bình; góc đang là **mốc** thì được **bỏ qua** khi đi vòng |
| Sau khi vặn ốc | Phải chạy `G35` lại từ đầu | Bấm **`PROBE`** (đo lại đúng góc đang đứng) — máy **tự** sang góc cần chỉnh kế tiếp ngay khi góc vừa đo `DAT` (hoặc bấm **`DONE`** để **chấp nhận góc đang đứng là xong** rồi đi tiếp), lặp tới khi ≈ 0 |
| Phù hợp | Vít đã gần đúng, muốn một bảng số đầy đủ | Vặn từng góc, cần phản hồi ngay |

> **`G35` không có mục nào trong menu LCD** — nó chỉ là lệnh G-code, phải gửi qua console/host.
> Trong `Motion` có **Tramming Wizard** là bản đồ hoạ của cùng thuật toán đó, kèm mục
> **`WORST POINT = MAX`** (bật/tắt gốc "lệch nhất" — xem §11.8c).
> `Motion` cũng có **Auto Z Align** (= `G34`), **Z ALIGN MANUAL** và **Deploy/Stow Z-Probe** (= `M401`/`M402`).
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
3. G35  (hoặc Motion > Tramming Wizard)       ; vặn vít theo số mm Marlin báo
4. G34 Q99                                    ; căn gantry; lặp tới khi sai số <= 0.02
5. G29 P1 -> G29 P3 -> G29 S0                 ; đo + lưu mesh vào slot 0
6. G29 A -> M500                              ; bật leveling + lưu -> tự bật mỗi lần khởi động
```

> Thứ tự **G34 trước, `G29` sau** ở bước 4–5 không phải tuỳ tiện — xem mục dưới.

### UBL và G34 có đụng nhau không?

**Không. Marlin tự tắt mesh trước khi G34 probe, rồi bật lại y như cũ.**

```cpp
// G34_M422.cpp:190-196 — comment gốc của Marlin: "Disable the leveling matrix before auto-aligning"
#if HAS_LEVELING
  #if ENABLED(RESTORE_LEVELING_AFTER_G34)
    const bool leveling_was_active = planner.leveling_active;   // nhớ trạng thái
  #endif
  set_bed_leveling_enabled(false);                              // TẮT mesh
#endif
// ... probe 3 điểm ...
#if BOTH(HAS_LEVELING, RESTORE_LEVELING_AFTER_G34)
  set_bed_leveling_enabled(leveling_was_active);                // G34_M422.cpp:528-530 — BẬT LẠI
#endif
```

| | |
|---|---|
| Máy này | `RESTORE_LEVELING_AFTER_G34` **đang bật** (`Configuration_adv.h:1027`) → vòng tắt/bật khép kín |
| Tắt **thật**, không chỉ hạ cờ | `bedlevel.cpp:79-81`: `apply_modifiers` → lật `planner.leveling_active` → `unapply_modifiers` |
| Hệ quả | Số đo G34 là **độ cao thô** của bàn — mesh không được cộng vào |
| Vì sao **buộc** phải tắt | G34 dùng "dirty trick" `current_position.z += z_probe * 0.5f` (`G34_M422.cpp:232`). Mesh còn bật thì phép cộng đó bị bẻ cong → G34 tính sai |

> ⚠️ **Chỗ thật sự đáng lo không phải "UBL trong lúc G34", mà là mesh đo TRƯỚC G34.**
> G34 nghiêng lại gantry, mà probe gắn trên gantry — nên mesh UBL **mã hoá luôn cả độ nghiêng gantry
> lúc đo**. G34 làm phẳng gantry xong thì mesh cũ hiệu chỉnh **thừa** đúng bằng phần nghiêng vừa sửa.
>
> G34 có tính lặp lại (`Z_STEPPER_ALIGN_ACC 0.02` → lệch tối đa 0.02 mm), nên quy tắc là:
> **tạo mesh SAU khi đã G34** (đúng thứ tự bước 4 → 5 ở trên).
> **Phải chạy lại `G29`** nếu: mới G34 lần đầu sau khi tạo mesh, vừa tháo/lắp gantry, đổi belt,
> hoặc đổi/thay Z-stepper.

> 🔴 **`G34` ĐÃ BỊ BỎ KHỎI START G-CODE CỦA CURA (theo yêu cầu).** Từ nay máy **không** căn lại
> gantry trước mỗi bản in. Hệ quả và quy tắc bù lại:
>
> | | |
> |---|---|
> | Bản in dựa vào gì | **Trạng thái cơ khí của gantry từ lần `G34` cuối** + **mesh UBL đã lưu** (`M420 S1`) |
> | Vì sao vẫn chấp nhận được | G34 có tính lặp lại tốt và gantry không tự xê dịch giữa các bản in; mesh đã đo sau lần G34 cuối nên vẫn đúng |
> | **BẮT BUỘC chạy `G34 Q99` bằng tay** khi | tháo/lắp gantry, đổi belt, đổi/thay Z-stepper, siết lại pully, hoặc sau bất kỳ va đập nào (xem cạm bẫy 25) |
> | Sau khi chạy `G34` tay | **phải `G29` lại** — vì mesh cũ mã hoá độ nghiêng gantry trước đó (lý do ở khối trên) |
> | Lợi ích | bỏ được bước căn gantry trước mỗi bản in (Q99 có thể mất hàng chục giây tới vài phút) |
>
> Quy trình bảo trì rút gọn: **`G34 Q99` → `G29` → `M500`** (thủ công, khi cần), rồi in bình thường.

> ℹ️ **`G28 Z` cũng đã bị bỏ khỏi start G-code.** Nó tồn tại **chỉ để phục hồi Z sau khi `G34` làm
> nghiêng gantry** (`HOME_AFTER_G34` bật → Z được home lại). Không còn `G34` thì `G28` đã home Z
> rồi, nên `G28 Z` chỉ tốn thêm thời gian probe giữa bàn.
> Nếu muốn tận dụng nó cho việc khác — **home lại Z SAU khi bàn và hotend đã nóng** để bù giãn nở
> nhiệt — thì nên **chuyển** nó xuống dưới `M109`, chứ không phải để nguyên vị trí cũ. Hiện tại
> **chưa làm** việc đó.

> ⚠️ **Với UBL, `M420 S1` bật leveling kể cả khi mesh hỏng.** `bedlevel.cpp:62` chỉ kiểm tra mesh
> hợp lệ cho `AUTO_BED_LEVELING_BILINEAR`:
> ```cpp
> const bool can_change = TERN1(AUTO_BED_LEVELING_BILINEAR, !enable || leveling_is_valid());
> ```
> UBL không nằm trong điều kiện đó → **đừng tin `M420 S1` là an toàn**; muốn chắc thì `M420 V`
> phải in ra `Mesh is valid`.

### `M420 S1` trong start G-code — thừa, nhưng nên giữ

Start G-code hiện tại: `G28` → `M190 S60` → `M109 S230` → `M420 S1`.

Cả `G28` đều **tự khôi phục** trạng thái leveling, nên tới dòng `M420 S1` thì mesh đã bật sẵn:

| Lệnh | Cơ chế tự bật lại |
|---|---|
| `G28` | `RESTORE_LEVELING_AFTER_G28` (`Configuration.h:1977`) → `CAN_SET_LEVELING_AFTER_G28 = 1` (`bedlevel.h:26-28`) → `G28.cpp:546` |
| `G34` *(không còn dùng trong start G-code)* | `RESTORE_LEVELING_AFTER_G34` (`Configuration_adv.h:1027`) → `G34_M422.cpp:529` |

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

### 6.1 🔴 AN TOÀN — model MPC trong EEPROM trôi đi gây điều khiển nhiệt sai

**Đây là lỗi im lặng và nguy hiểm nhất đã gặp trên máy này. Đọc trước khi hâm nóng đầu in.**

**Triệu chứng (đã gặp thật):**

- Hâm nóng, tới khoảng **130 °C thì số hiển thị bắt đầu GIẢM** trong khi heater vẫn được cấp điện
- Thỉnh thoảng nhiệt độ **nhảy −10 °C rồi nhảy lại**
- **Khởi động lại Marlin thì nhiệt độ hiển thị đúng** — nhưng chạy một lúc lại sai

**Nguyên nhân gốc:** máy chạy **`MPCTEMP`** (không phải PID — `Configuration.h:651` đang comment `PIDTEMP`, `:652` bật `MPCTEMP`), và model MPC trong **EEPROM** có:

```
M306 E0 P40.00 C7.67 R-1.5637 A0.0466 F0.0878 H0.0056
                        ^^^^^^^^^ R ÂM
```

`M306.cpp:42` định nghĩa `R<kelvin/second/kelvin>  Sensor responsiveness (= transfer coefficient / heat capacity)` — cả hai đều **dương**, nên **`R` âm là bất khả thi về vật lý**. `M306.cpp:58` nhận mọi số thực **không kiểm khoảng**, và EEPROM chỉ `EEPROM_READ` nguyên struct → số âm **nằm lại vĩnh viễn**.

**Vì sao nguy hiểm — MPC điều khiển theo MÔ HÌNH, không theo cảm biến** (`temperature.cpp:1488`):

```c
power = (hotend.target - hotend.modeled_block_temp) * ...   // <-- theo MO HINH
```

Với `R` âm, `modeled_sensor_temp` **phân kỳ ra xa** block (`:1472`) → bộ điều khiển ra lệnh theo một nhiệt độ hoàn toàn sai. **Điểm chết người: PID điều khiển trực tiếp từ cảm biến nên lỗi cảm biến sẽ kích hoạt bảo vệ nhiệt; MPC thì CHE MẤT lỗi đó.** Đó là lý do một bộ điều khiển "thông minh hơn" lại nguy hiểm hơn ở đây.

**Vì sao "restart là đúng lại":** `temperature.cpp:1435-1439` chỉ gieo model **một lần lúc khởi động**:

```c
// At startup, initialize modeled temperatures
if (isnan(hotend.modeled_block_temp)) {
  hotend.modeled_ambient_temp = _MIN(30.0f, hotend.celsius);
  hotend.modeled_block_temp = hotend.modeled_sensor_temp = hotend.celsius;
}
```

Boot thì model = cảm biến → điều khiển đúng; chạy một lúc thì model phân kỳ → sai. `modeled_*` nằm trong **RAM**, chỉ `constants` mới vào EEPROM.

**Cách chẩn đoán (không cần flash, không cần hâm nóng):**

| Việc | Đúng phải là |
|---|---|
| `M306` | `R` **dương** (mặc định `0.1284`). `R` âm = model hỏng |
| `M105` | In kèm **raw ADC** (`SHOW_TEMP_ADC_VALUES` bật). Đổi sang điện trở: `R = 4,7 × raw / (4095 − raw) [kΩ]` — 100k NTC ở 25 °C phải cho ~100 kΩ |
| `M105` đầu in vs bàn khi máy **nguội hẳn** | Phải xấp xỉ nhau (chênh ≤ 2–3 °C) |

**Cách sửa — KHÔNG cần flash:**

```
M306 E0 P40.00 C7.13 R0.1284 A0.068 F0.097 H0.0056
M500
```

> 🔴 **BẮT BUỘC RESET VẬT LÝ sau đó (nút RESET hoặc tắt/bật PSU). `M999` KHÔNG ĐỦ** —
> `M999.cpp:38-45` chỉ đặt `marlin_state = MF_RUNNING`, xả buffer serial và `ui.reset_alert_level()`.
> **RAM không bị đụng**, nên model vẫn là trạng thái đã phân kỳ, và phép thử sau đó vô nghĩa.

**Đã kiểm chứng sau khi sửa** (số đo thật, `monitor-temp.ps1`):

| Target | Vọt nhiệt lớn nhất | Ổn định | Trôi ngược |
|---|---|---|---|
| 100 °C | **+0,46 °C** | ±0,05 °C | không |
| 150 °C | **+0,51 °C** | ±0,05 °C | không — **qua mốc 130 °C trơn** |
| 230 °C | **+1,89 °C** | hội tụ về target | không |

**Công cụ:** `.\monitor-temp.ps1 -Target 100 -Seconds 130` — giữ **một** kết nối duy nhất (mở lại cổng COM giữa chừng có thể reset board và mất target), đọc `M105` **trước** khi bật heater, **tự `M104 S0`** nếu vượt `target + GuardBand`, tự tắt khi hết giờ. `-ProbeOnly` để kiểm tra đường hiển thị mà **không** bật heater.

> ⚠️ **Bộ mặc định ở trên KHÔNG phải đã autotune cho đầu in này** — nó là tham chiếu của Marlin cho
> heater 40 W, và khớp phần cứng ở đây (model 40 W + `MPC_MAX = BANG_MAX = 128` giới hạn duty 50%
> trên cartridge **80 W** → 40 W thực). Nó chạy tốt (bảng trên). Muốn khớp hơn thì `M306 T`
> — nhưng **autotune đo bằng chính cảm biến**, nên chỉ chạy khi cảm biến đã được xác nhận, và
> chạy **từ trạng thái nguội, quạt tắt**. Luôn quay lại được vì bộ mặc định đã ghi ở đây.

> 🔵 **Khuyến nghị dài hạn: cân nhắc bật lại `PIDTEMP`** (`Configuration.h:651`) và tắt `MPCTEMP`
> (`:652`). Không phải vì PID "quen thuộc hơn", mà vì **PID điều khiển trực tiếp từ cảm biến** —
> khi cảm biến hỏng thì bảo vệ nhiệt kích hoạt đúng, còn MPC che mất. Với thiết bị gia nhiệt, đó là
> tiêu chí an toàn, không phải sở thích. (Cần flash + `M303 E0 S230 C8`.)

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
| **Motion → Tramming Wizard** | `G35` | Wizard **4 nút**, thứ tự encoder: **`PROBE` → `MAX->MIN` → `BACK` → `DONE`** (mặc định chọn `PROBE`). Vòng lặp: probe 4 góc → đi tới **góc lệch nhất** → vặn vít → **`PROBE`** (đo lại **đúng góc đang đứng**, không home lại → hiện còn phải vặn bao nhiêu **mm** + chiều `CW`/`CCW`; nếu góc vừa đo **đã đạt** `\|delta\| ≤ 0.01mm` (`DAT`, hằng số `TRAM_DONE_MM`) thì máy **TỰ ĐỘNG đi tiếp** tới **góc cần chỉnh kế tiếp** và **probe luôn** góc đó — trạng thái `TR_AUTO`, màn hình hiện `Auto next`, **không cần bấm `DONE`**); **`MAX->MIN`** đổi chiến lược **MAX ⇄ MIN** (mốc `Z-MIN` ⇄ `Z-MAX`), lưu EEPROM ngay, rồi **đi ngay tới góc lệch nhất của chế độ mới và probe luôn** (góc đó **`DAT`** thì cũng **tự đi tiếp**); **`BACK`** = **thoát về màn hình chính** (stow probe + `set_axis_never_homed(Z_AXIS)`, hàm `tramming_exit()`); **`DONE`** = **"góc này XONG"** (**chấp nhận thủ công**, vẫn còn, bấm tay) → **đánh dấu góc đang đứng là ĐÃ CHẤP NHẬN** (`z_accepted[worst_index] = true`) — **coi như đạt dù `\|delta\|` chưa vào ngưỡng `0.01mm`** ⇒ máy **không quay lại góc đó nữa**; nếu còn góc **chưa chấp nhận** thì đi thẳng tới góc đó rồi **probe luôn**, còn **hết** góc chưa chấp nhận thì máy **tự home lại Z + probe lại cả 4 góc** (vòng mới, **xoá sạch `z_accepted[]`**, **mốc mới**) → đi tới góc lệch nhất. `DONE` **không phải** nút thoát (thoát bằng `BACK`). Góc đã chấp nhận **vẫn hiện số delta** trên màn hình (không có ký hiệu riêng). Nếu **ngay sau lần đo đủ 4 góc mà CẢ 4 GÓC ĐỀU `DAT`** (`tram_all_dat = true`) thì **4 số ở 2 dòng đầu đổi mốc: `z_delta[i] − base_home_pos(Z_AXIS)` = delta-Z so với TÂM BÀN (điểm home Z) — số DƯƠNG = góc CAO HƠN tâm, số ÂM = góc THẤP HƠN tâm**, dòng 2 hiện `DAT - lech so voi tam` và dòng 3 **chỉ còn `BACK`** (encoder khoá vào `BACK`; **không** tự home + probe lặp vô tận). Khi **chưa** đủ 4 góc `DAT`, mốc so sánh = **`Z-MIN` (chế độ MAX) hoặc `Z-MAX` (chế độ MIN)**, **chốt một lần** sau lần đo đủ 4 góc đầu tiên; `PROBE` lại 1 góc chỉ đổi số của chính góc đó. ⚠️ Wizard vào bằng **`G28L0`** nên **bed leveling bị tắt** và **không** tự bật lại khi thoát. Xem §11.8c |
| **Motion → WORST POINT = MAX** | — | Bật/tắt gốc "lệch nhất" cho Tramming Wizard: **ON = MAX** (góc cao nhất), **OFF = MIN** (góc thấp nhất). Bấm là **lưu ngay vào EEPROM** → nhớ cho các lần sau. Dùng khi vít của góc đang chỉnh **đã hết hành trình** (vặn mãi không được) thì chuyển sang chỉnh góc ngược lại. Xem §11.8c |
| **Màn hình riêng `lcd_panel`** | — | Firmware **riêng cho MCU của màn hình** (F411CCU6 / F103C6T6 + ILI9341 2.4" SPI): panel tự vẽ, Marlin chỉ gửi nhiệt độ/XY/tiến độ qua G-code. Xem §12 |
| **Motion → Z ALIGN MANUAL** | — | Dịch tay **Z1 / Z2 / Z3** từng bước **0.01mm** bằng encoder + **probe lại từng điểm** để xem ngay nó lên/xuống bao nhiêu (xem §11.8b) |

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

### Nạp qua thẻ TF/SD (và USB) — KHÔNG cần ST-Link/DFU

Monster8 **tự nạp firmware từ thẻ TF (microSD) hoặc USB** bằng **bootloader MKS** (không phải Marlin):

1. Copy **`mks_monster8.bin`** vào **thư mục gốc** của thẻ TF (FAT32) hoặc USB.
2. Cắm thẻ/USB vào board → **nhấn RESET** (hoặc tắt/bật nguồn).
3. Bootloader nạp rồi khởi động firmware mới.

(Nguồn: MKS wiki *[TF card or Udisk update](https://github.com/makerbase-mks/MKS-Monster8/wiki/TF_UDISK_update)* — nguyên văn: *"Copy your compiled **mks_monster8.bin** firmware to tf card or udisk. Insert to board and reset board update firmware"*. Tên file khớp đúng output build của repo này: `board_build.rename = mks_monster8.bin`.)

| | |
|---|---|
| Thông báo trên màn hình? | **Không.** Bootloader chạy **trước** Marlin nên không điều khiển được Mini12864 hay màn LVGL (2 màn đó do Marlin vẽ). Tuỳ chọn `SD_FIRMWARE_UPDATE` của Marlin (có menu + thông báo) **chỉ dành cho ATmega2560** — `SanityCheck.h:3929` chặn trên STM32. Cách kiểm chứng đã nạp xong: gọi `M115` (xem version) hoặc nhìn màn hình sau khi board chạy lại |
| Địa chỉ ghi | Vẫn là `0x0800C000` (file build của repo này đã đúng offset) |
| Cài đặt có mất không? | **Không.** EEPROM là chip I2C 4K trên board (`I2C_EEPROM`, `MARLIN_EEPROM_SIZE 0x1000`, chân PB8/PB9 — `pins_MKS_MONSTER8_common.h:44-47`) nên `M851`/mesh UBL/`M203`… giữ nguyên qua mọi cách nạp |
| Nếu nạp thẻ lỗi | Bootloader vẫn còn nguyên (vùng `0x08000000–0x0800BFFF`) → cứu bằng DFU/ST-Link bình thường |

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
M503        ; 2026-10-12 (cuoi ngay - bo "Truoc (1/16)" quy doi cho 1/8):
            ; M92 X40 Y40 Z400 E207.5 / M203 X150 Y150 Z10 E25
            ; M201 X300 Y300 Z200 E1000 / M204 P500 R300 T500 / M205 X8 Y8 Z0.40 E5
            ; M906 X600 Y600 Z600 (I1/I2 Z600) / T0 E600      <- 600 mA ca 6 truc
            ; M851 X0 Y0 Z0.70
M122        ; msteps 8 (ca 6 driver) + off time 3 + blank time 24 + interp true
            ; => tmc_init() DA ghi duoc vao driver.
            ; Neu thay off time 0 / msteps 256 / blank time 16 / Enabled false
            ;    => KHONG co dien ap motor (PSU tat) -> doc vo nghia, bat PSU roi doc lai.
M119        ; trang thai endstop
M422        ; 3 diem G34: S1 280/285, S2 25/285, S3 152.5/25
M420 V      ; mesh 7x7, bien (15,15) .. (290,290)
            ; HIEN TAI: Storage slot -1 + Bed Leveling OFF (mesh da bi G29 P0 xoa 2026-10-11)
            ; Sau khi probe lai UBL moi mong doi: Mesh is valid + slot 0 + Bed Leveling ON
```

### Checklist sau mỗi lần nạp

| Kiểm tra | Lệnh | Mong đợi |
|---|---|---|
| Firmware mới thật chưa | `M115` | timestamp khớp giờ build |
| Sau `G28` nozzle có được nâng | `G28` → `M114` | **`Z:10.00`** (`Z_AFTER_PROBING 10`) |
| Chiều extruder | `M83` · `G1 E10 F100` | **đẩy ra**, không phải rút vào |
| Offset probe còn không | `M851` | Z ≈ **0.70** (giá trị đang lưu; 2026-10-11 có lúc đọc ra 0.80 → nếu không rõ thì cân lại bằng `PROBE_OFFSET_WIZARD`) |
| Điểm G34 còn không | `M422` | `S3 X152.50 Y25` |
| Mesh còn không | `M420 V` | hiện `Storage slot -1` + `Bed Leveling OFF` (mesh đã bị `G29 P0` xoá có chủ ý, 2026-10-11) — chỉ mong đợi `Mesh is valid` + `slot 0` **sau khi** đã probe lại UBL |
| **Khoảng cách thực** (driver 1/8 ⇄ `M92`) | `G28` → `G1 X100 F600` / `G1 Y100 F600` → **đo bằng thước** | ✅ **đã test OK ngày 2026-10-12**: chạy **100 mm**, đo thực tế **khớp** ⇒ `M92 40/40/400/207.5` + microstep 1/8 **chính xác** (xem §11.4b) |

Nếu mục nào sai sau khi nạp → xem **11.7** trước khi sửa code.

> ⚠️ **EEPROM CÓ THỂ ĐANG GIỮ ẢNH CŨ SAU KHI FLASH — đã gặp thật ngày 2026-10-12.** Sau khi nạp bản
> 1/8, `M503` trả về `M92` **đúng** (`40/40/400/207.5`) nhưng `M203`/`M201`/`M204`/`M906`/`M851` lại là
> **số của bản cũ** (`150/150/10/25`, `500/500/100/500`, `500/300/500`, `500/500/500`, `Z0.70`) — không
> phải mặc định của firmware mới (`75/75/5/12`, `150/150/100/500`, `150/150/150`, `600/600/500`, `Z0`).
> Máy vẫn chạy được (vì `M92` khớp driver) nên **rất dễ tưởng là xong**.
>
> Cách xử lý **đã kiểm chứng**:
> 1. Gửi lại **mỗi lệnh một dòng riêng** rồi `M500`. ⚠️ `M906` mà trộn nhiều nhóm trong *cùng một dòng*
>    (`M906 X800 Y800 Z600 I1 Z600 I2 Z600 T0 E400`) thì parser **bỏ qua `X/Y/Z`** — đo được thật:
>    kết quả chỉ có `I2` đổi. Phải tách: `M906 X800` · `M906 Y800` · `M906 Z600` · `M906 I1 Z600` ·
>    `M906 I2 Z600` · `M906 T0 E400`.
> 2. **Mở lại cổng COM (board tự reset) rồi đọc `M503` lần nữa.** Chỉ khi số **còn đúng sau reset** thì
>    mới coi là đã lưu — `M500` báo `Settings Stored (684 bytes; crc …)` **không** bảo đảm điều đó
>    (`M503` đọc từ **RAM**, không đọc lại chip).
>
> **Quy tắc: sau mỗi lần flash, luôn `M503` đối chiếu, và đối chiếu lại một lần nữa sau khi reset.**
> ℹ️ **Đã gặp lần 2 trong cùng ngày 2026-10-12:** sau khi flash bản mới, `M906` đọc ra **600/600/600**
> trong khi EEPROM vừa được ghi và verify là **800/800/600** trước đó ⇒ đúng kiểu "flash làm đổi/đọc
> nhầm một phần EEPROM". Vì vậy **thứ tự an toàn là: FLASH TRƯỚC → ghi EEPROM (`M500`) → verify sau
> reset**, chứ không phải ngược lại.

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
| Driver | `X/Y/Z/Z2/Z3/E0_DRIVER_TYPE` TMC2209 chế độ UART, **`*_MICROSTEPS 8`** (2026-10-11 — **cả 6 driver**, xem §11.4b), **`*_CURRENT 600`** cho **cả 6** (X Y Z Z2 Z3 E0 — 2026-10-12; 75 % định mức 0,8 A), `*_HAS_STEALTHCHOP` (`STEALTHCHOP_XY`, `STEALTHCHOP_Z`) |
| Leveling | **UBL**, `GRID_MAX_POINTS_X/Y 7`, `MESH_INSET 15`, `ASSISTED_TRAMMING` (**G35**) |
| Nhiệt độ | `TEMP_SENSOR_0/BED 1`, `PIDTEMPBED`, **`MPCTEMP`** cho hotend, `MPC_INCLUDE_FAN`, `PREHEAT_BEFORE_LEVELING`, `HOTEND_OVERSHOOT 15`, `BED_OVERSHOOT 10` |
| Chuyển động | **2026-10-12 — về bộ "Trước (1/16)", chỉ quy đổi `M92` cho microstep 1/8:** `DEFAULT_AXIS_STEPS_PER_UNIT { 40, 40, 400, 207.5 }`, `DEFAULT_MAX_FEEDRATE { 150, 150, 10, 25 }`, `DEFAULT_MAX_ACCELERATION { 300, 300, 200, 1000 }`, `DEFAULT_ACCELERATION 500`, `DEFAULT_RETRACT_ACCELERATION 300`, `DEFAULT_TRAVEL_ACCELERATION 500`, `DEFAULT_XJERK/DEFAULT_YJERK 8.0` (`ZJERK 0.4`, `EJERK 5.0`). **`CLASSIC_JERK`** (không dùng Junction Deviation). Toàn bộ **đồng bộ với EEPROM** — xem §3.3 và §11.4b |
| Khác | `EEPROM_SETTINGS`, `SDSUPPORT`, `FILAMENT_RUNOUT_SENSOR`, `HOST_ACTION_COMMANDS` |

### 11.2 `Marlin/Configuration_adv.h` — cấu hình nâng cao

| Nhóm | Giá trị |
|---|---|
| Trục Z | `INVERT_Z2_VS_Z_DIR` **tắt** (cả 3 vít me quay cùng chiều), `Z_STEPPER_ALIGN_XY { {280,285}, {25,285}, {X_CENTER,25} }`, `Z_STEPPER_ALIGN_AMP 1.0`, `Z_STEPPER_ALIGN_ITERATIONS 5`, `Z_STEPPER_ALIGN_ACC 0.02` |
| Tram bàn | `ASSISTED_TRAMMING`, `ASSISTED_TRAMMING_WIZARD`, `REPORT_TRAMMING_MM`, `TRAMMING_SCREW_THREAD 40` (vít M4 ren phải, bước 0.7mm — **giữ 40**, xem ghi chú chiều vặn bên dưới), `TRAMMING_POINT_XY` 4 góc `{280,285} {25,285} {25,25} {280,25}`. **`ASSISTED_TRAMMING_WAIT_POSITION` đã TẮT** (2026-10-09): trước đây sau khi probe xong nozzle tự chạy ra giữa bàn ở Z30; nay G35/wizard **dừng ngay tại điểm probe cuối** (đầu in ~5mm trên điểm đó). Đổi lại: toolhead có thể vướng tay khi vặn vít ở góc đó. **Góc đang chỉnh (MAX/MIN) lưu trong EEPROM**, đổi bằng nút `MAX->MIN` trong wizard hoặc `Motion → WORST POINT = MAX` — xem §11.8c |
| TMC2209 | `STEALTHCHOP_XY`, `STEALTHCHOP_Z` (không dùng sensorless homing — đã bỏ `USES_DIAG_JUMPERS`, xem 11.3) |
| **Thêm mới** | `ENCODER_QUAD_ISR` — đọc encoder bằng **ngắt** + ring cố định 32 phần tử (không mất nấc khi UI bận) — xem §13/§14. `PANEL_SERIAL_UI` **đang TẮT** (màn 12864 vẫn là màn chính) — xem §12/§15 |
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
| `src/module/settings.cpp` | **sửa bug**: sau khi nạp EEPROM, ép lại `mstep_reg_select(true)` + `microsteps()` cho TMC2209. Không có bước này, `refresh_stepping_mode()` ghi đè GCONF bằng cache → chân MS1/MS2 không được điều khiển → driver rơi về **1/8**, trục chạy **gấp đôi** (đã gặp thật: `G1 Z10` đi 20mm). Lưu ý: hàm này **không** xử lý `Y2` — xem §11.4b |
| `src/inc/Conditionals_LCD.h` | thêm `PROBE_ENABLE_DISABLE` vào `ANY(...)` của `HAS_STOWABLE_PROBE` → menu *Deploy/Stow Z-Probe* hoạt động cả với `FIX_MOUNTED_PROBE` |
| `src/gcode/calibrate/G34_M422.cpp`, `src/gcode/gcode.h` | thêm tham số `Q<nloop>` (lặp G34, home lại sau mỗi 3 lần đo), `U` (chế độ hardcode balance), hàm `InfiniteG34()`. **Đã gỡ** tính năng "huỷ G34 bằng nút encoder" (2026-10) vì **không dừng được G34** — xem §11.8 |
| Start G-code Cura | **ĐÃ BỎ `G34`** — không còn căn gantry tự động trước mỗi bản in. Tham số `G34 Q<n>` vẫn có trong firmware để **chạy tay khi bảo trì**; xem §5 và mục 11.8 |
| `src/lcd/marlinui.cpp`, `marlinui.h` | thêm `pin_test_active` + `pin_test_update()` — in **mức điện thô** `READ(X_MIN_PIN/Y_MIN_PIN/Z_MIN_PIN)` lên status line (bỏ qua logic endstop của Marlin) |
| `src/lcd/menu/menu_advanced.cpp` | thêm 2 menu: **Reboot to DFU** (tắt heater + `planner.finish_and_disable()` rồi `flashFirmware(0)`) và **Endstop Pins** |
| `src/lcd/menu/menu_z_align.cpp` **(file mới)**, `menu_motion.cpp` | menu **Motion → Z ALIGN MANUAL**: màn hình dịch tay Z1/Z2/Z3 từng bước 0.01mm + probe lại từng điểm. Xem §11.8b |
| `src/gcode/bedlevel/G35.cpp`, `src/lcd/menu/menu_tramming.cpp` | Xem §11.8c — **G35/wizard: mốc vật lý = Z-home** (`base_home_pos(Z_AXIS)`, **bỏ lần probe tâm bàn**); **mốc để vặn vít = `Z-MIN` (chế độ MAX, mặc định) hoặc `Z-MAX` (chế độ MIN)**, và từ **2026-10-12 hai mốc này được CHỐT (đóng băng)** sau lần đo **đủ 4 góc đầu tiên** của wizard (`z_ref_min`/`z_ref_max`/`z_ref_set`, hàm `tramming_set_ref()`) ⇒ **`PROBE` lại 1 góc chỉ đổi số của góc đó**, còn đổi `MAX<->MIN` chỉ là **đổi lấy mốc kia** (không đo lại gì); số cần vặn in theo **mm** (`\|delta\|`, không phải vòng) và **chiều vặn đã ĐẢO LẠI: CCW = hạ góc xuống, CW = nâng lên**; probe lỗi 1 góc không mất cả phép đo; wizard **4 nút** (thứ tự encoder): `PROBE` (probe lại **góc đang đứng**, **không home lại** → hiện số **mm** còn phải vặn + `CW`/`CCW`; góc vừa đo **`DAT`** `\|delta\| ≤ 0.01mm` → **TỰ ĐỘNG đi tiếp** tới góc cần chỉnh kế tiếp và probe luôn, màn hình `Auto next`, **không cần bấm `DONE`**) / `MAX->MIN` (đổi chiến lược **MAX ⇄ MIN**, **lưu EEPROM**, **đi ngay tới góc lệch nhất mới và probe luôn**, **tự đi tiếp nếu góc đó `DAT`**) / `BACK` (**thoát về màn hình chính**: stow probe + `set_axis_never_homed(Z_AXIS)`, hàm `tramming_exit()`; khi **cả 4 góc `DAT`** thì đây là nút **duy nhất** còn lại) / `DONE` (**bấm tay** = **"góc này XONG"**: **chấp nhận góc đang đứng** bằng `z_accepted[worst_index] = true` — **coi như đạt dù `\|delta\|` chưa vào ngưỡng `0.01mm`** ⇒ **không quay lại góc đó nữa** — rồi đi thẳng tới **góc cần chỉnh kế tiếp CHƯA được chấp nhận** (**bỏ qua góc mốc** + các góc **đã đạt** `\|delta\| ≤ 0.01mm` + các góc **đã chấp nhận**), **probe luôn**; **hết góc chưa chấp nhận → TỰ home lại Z + probe lại cả 4 góc** (**xoá sạch `z_accepted[]`**), **chốt mốc mới**; **`DONE` không phải nút thoát**); **cả 4 góc `DAT` ngay sau lần đo đủ 4 góc → 4 số ở 2 dòng đầu là delta-Z so với TÂM BÀN (`z_delta[i] − base_home_pos(Z_AXIS)`, dương = góc cao hơn tâm, âm = thấp hơn tâm), dòng 2 hiện `DAT - lech so voi tam`, dòng 3 chỉ còn `>BACK`**) |
| `src/feature/tramming.h` (helper `tramming_pitch/turn_cw/turns`) | Gộp **chiều vặn** + bước ren vào một chỗ để `G35` và wizard dùng chung. **2026-10-12: ĐẢO CHIỀU** (`tramming_turn_cw()`: **CW = nâng, CCW = hạ** — đo lại trên máy, ghi chú cũ ghi ngược và sai) và **bỏ số vòng khỏi phần hiển thị** (in theo `\|delta\|` mm; `tramming_turns()` chỉ còn để tham chiếu) |
| `src/feature/tramming.h`, `tramming.cpp`, `src/module/settings.cpp`, `src/lcd/menu/menu_motion.cpp` | **`tramming_worst_max`** (góc đang chỉnh = MAX cao nhất / MIN thấp nhất): menu **`Motion` → `WORST POINT = MAX`** + nút **`MAX->MIN`** trong wizard, **lưu EEPROM** trong ô `z_offset` của khối MBL (MBL đang tắt nên ô đó trống) → **không đổi kích thước/CRC EEPROM** nên không xoá cấu hình đang có. Xem §11.8c |
| `Marlin/Configuration_adv.h` (`TRAMMING_POINT_NAME_1..4`) | **sửa tên 4 góc cho khớp toạ độ** — trước đây bị ngược 180° nên G35/wizard chỉ sai góc cần vặn |
| `src/lcd/language/language_en.h` | thêm `MSG_REBOOT_TO_DFU`, `MSG_PIN_TEST` |
| `src/inc/Conditionals_adv.h`, `Conditionals_post.h` | guard nhỏ: bỏ `BABYSTEP_ZPROBE_OFFSET` khi không có probe, bỏ `PREHEAT_BEFORE_LEVELING` khi không bật `PIDTEMPBED` |

### 11.4b Microstep 1/8 — lần thử cũ **đã hoàn tác**, nay **ĐÃ ÁP DỤNG LẠI cho cả 6 driver**

#### ✅ Trạng thái hiện tại (2026-10-11): 1/8 cho **cả 6 driver** (X Y Z Z2 Z3 E) + vận tốc/gia tốc giảm một nửa

| Chỗ | Trước (1/16) | **Hiện tại (1/8)** |
|---|---|---|
| `Configuration.h` → `TTL_MICROSTEP` | 16 | **8** — **một dòng duy nhất**, cả 6 driver thừa hưởng (`Configuration_adv.h:2748+` lấy `X_MICROSTEPS = TTL_MICROSTEP`, rồi `Z2/Z3/E0` lấy theo `X_MICROSTEPS`) |
| `Configuration_adv.h` → `MICROSTEP_MODES` | `{16×6}` | **`{8,8,8,8,8,8}`** |
| `Configuration.h` → `DEFAULT_AXIS_STEPS_PER_UNIT` | `{80,80,800,415}` | **`{40,40,400,207.5}`** |
| `DEFAULT_MAX_FEEDRATE` | `{150,150,10,25}` | **`{75,75,5,12}`** |
| `DEFAULT_MAX_ACCELERATION` | `{2000,2000,200,1000}` | **`{150,150,100,500}`** |
| `DEFAULT_ACCELERATION` / `_RETRACT_` / `_TRAVEL_` | 1500 / 1500 / 2000 | **150 / 150 / 150** |

> ℹ️ **`INTERPOLATE` vẫn bật** → TMC2209 nội suy lên 1/256, nên **độ mượt không đổi**, chỉ **tần số
> xung ra MCU giảm một nửa** ở cùng tốc độ mm/s. Trần vật lý (điện cảm ~225 mm/s) **không đổi** theo
> microstep — xem §3.3.

> 🟢 **Cập nhật 2026-10-12 — trạng thái CUỐI CÙNG: quay về bộ "Trước (1/16)", chỉ quy đổi `M92`.**
> Diễn biến trong ngày: (1) 1/8 + giảm nửa vận tốc/gia tốc → (2) nâng lên "trần nên dùng"
> (`M203 225`, `M201 5000`, **500 mA** cả 6 trục) → (3) hạ lại → (4) **chốt: bộ "Trước (1/16)"** —
> `M203 150/150/10/25`, `M201 300/300/200/1000`, `M204 500/300/500`, `M205 X8 Y8`,
> **`M906` 600 mA cho CẢ 6 trục**, `M92 40/40/400/207.5`. Đã ghi EEPROM (`M500`, crc 6571) → verify
> qua reset → đồng bộ **default trong `Configuration*.h`** → clean rebuild + flash
> (`Marlin 2.1.2 (Oct 12 2026 19:49:38)`, cùng bản có **mốc `Z-MIN`/`Z-MAX`** + sửa lỗi nhận nút).
> Bảng dưới giữ lại làm mốc lịch sử của bước 1/8; **trạng thái đang chạy xem §3.3**.

**Thứ tự thực hiện — quan trọng, và đã làm đúng thứ tự này:**

1. `M92 X40 Y40 Z400 E207.5` · `M203 X75 Y75 Z5 E12` · `M201 X150 Y150 Z100 E500` ·
   `M204 P150 R150 T150` · **`M500`** — ghi EEPROM **trước**, lúc máy còn chạy firmware 1/16.
   Chiều lệch này **an toàn**: `M92` nhỏ hơn thực tế ⇒ máy đi **chậm một nửa**, endstop vẫn ăn.
   (Chiều ngược lại mới nguy hiểm: driver đã 1/8 mà `M92` còn 80 ⇒ **chạy gấp đôi**.)
2. Clean rebuild (xoá `.pio\build\mks_monster8`) → `M997` → `dfu-util -a 0 -s 0x0800C000:leave`.
3. `M115` + `M503` + `M122` để xác nhận.

**Đã xác nhận trên máy (2026-10-11):**

```
M115  ->  Marlin 2.1.2 (Oct 11 2026 23:28:27)        <- bản 1/8
M503  ->  M92 X40.00 Y40.00 Z400.00 E207.50
          M203 X75.00 Y75.00 Z5.00 E12.00
          M201 X150.00 Y150.00 Z100.00 E500.00
          M204 P150.00 R150.00 T150.00
M122  ->  msteps      8    8    8    8    8    8      <- X Y Z Z2 Z3 E
          off time    3    3    3    3    3    3      <- tmc_init() DA ghi duoc
          blank time 24 x6 · interp true x6 · stealthChop true x6
          Run current 25/31 · 25/31 · 18/31 · 18/31 · 18/31 · 12/31
          PWM scale sum 14 / 14 / 11 / 11 / 11 / 7      (autoscale dang chay)
          Testing X/Y/Z/Z2/Z3/E connection... OK        (ca 6)
```

> ✅ **ĐÃ ĐO VÀ ĐÚNG (2026-10-12).** Microstep là **hằng số lúc biên dịch**, `M92` là **EEPROM** — hai
> cái **phải khớp nhau**, nên phép thử này vẫn **bắt buộc**: `G28` → cho chạy đúng **100 mm** ở tốc độ
> chậm → **đo bằng thước** (mong đợi 100 ± 1 mm, cả X và Y).
> **Kết quả đã kiểm chứng: chạy 100 mm, đo thực tế KHỚP** ⇒ bộ `M92 40/40/400/207.5` + driver **1/8**
> là **chính xác** (không còn nợ gì).

> 🔎 **Cách phân biệt "driver chưa init" với "không có điện áp motor" bằng `M122`** (đo thật 2026-10-11):
>
> | `M122` cho ra | Nghĩa |
> |---|---|
> | `off time 3` · `blank time 24` · `interp true` · `msteps 8` (hay 16) | ✅ `tmc_init()` đã ghi được — **trạng thái đúng** |
> | `off time 0` · `blank time 16` · `msteps 256` · `interp false` · `Enabled false` | ⚠️ **PSU tắt — không có VM**: thanh ghi về 0 nên đọc **không có ý nghĩa gì**. Bật PSU rồi đọc lại, **đừng kết luận là firmware lỗi** |
>
> `blank time` **không** phụ thuộc microstep: `trinamic.cpp:724` đặt cứng `chopconf.tbl = 0b01` (=24)
> cho TMC2209. Ghi chú cũ trong tài liệu này ("`msteps 8` ⇔ `blank time 36`") là **sai** — 36 chỉ là
> giá trị **reset** của CHOPCONF, không phải dấu hiệu của 1/8.

#### 🕘 Lần thử cũ (2026-10-07, **chỉ X/Y**) — đã hoàn tác, giữ lại làm lịch sử

> 🔴 **Kết luận lúc đó: giữ nguyên 1/16 và `M92 X80 Y80`.** Đã flash thử 1/8 (X/Y = 8, Z/Z2/Z3/E = 16,
> `M92 X40 Y40`) — bản đó **tự nó nhất quán** (driver 1/8 + `M92` 40) nên về nguyên tắc vẫn chạy
> đúng quãng đường. Sau đó **trục Y di chuyển sai**, nên đã hoàn tác toàn bộ về 1/16 + `M92 X80 Y80`.

> ✅ **ĐÍNH CHÍNH — nguyên nhân thật của "trục Y di chuyển sai" KHÔNG phải việc đổi microstep.**
> Sau khi hoàn tác, Y **vẫn** sai (chứng minh microstep không phải thủ phạm). Truy tiếp bằng phép
> thử tách motor (§11.9 bước D) thì ra: **pully bị tuột khỏi trục motor X/Y** — xem cạm bẫy 25.
> Siết lại vít hãm pully thì máy chạy bình thường. Ghi lại đây để **lần sau đừng đổ lỗi cho
> microstep** khi thấy Y đi chéo: kiểm **vít hãm pully** trước.

> 🔴🔴 **HOÀN TÁC XONG THÌ BẮT BUỘC PHẢI CLEAN REBUILD — build tăng dần cho ra firmware STALE.**
> Lần hoàn tác đầu tiên (`git checkout` rồi `pio run`) báo `SUCCESS` và relink, `M115` ra timestamp
> **mới**, và kiểm tra nhị phân thấy `DEFAULT_AXIS_STEPS_PER_UNIT` **đã** về `{80,80,800,415}` — nhưng
> `X_MICROSTEPS` thì **vẫn là 8**. Lý do: `DEFAULT_AXIS_STEPS_PER_UNIT` nằm ở translation unit khác
> với `X_MICROSTEPS` (`trinamic.cpp` / `settings.cpp`), và các object đó **không** được biên dịch lại.
> Hậu quả: driver 1/8 trong khi `M92` là 80 → **trục chạy gấp đôi**, mà nhìn bề ngoài mọi thứ "có vẻ
> đúng". Cách phát hiện: xem `blank time` của `M122` — `tmc_init()` đặt **24**; nếu thấy **36**
> (giá trị reset của CHOPCONF) nghĩa là `tmc_init()` **chưa hề ghi được** vào driver.
> **Luôn `Remove-Item -Recurse .pio\build\mks_monster8` rồi build lại** sau khi sửa
> `Configuration.h` / `Configuration_adv.h`. Xem cạm bẫy 25.

> ℹ️ **Cách đọc microstep THẬT từ driver.** `M122` in dòng `msteps` qua `st.microsteps()`, mà
> `TMCStepper::microsteps()` gọi `mres()`, và `TMC2208Stepper::mres()` viết là
> `CHOPCONF_t r{0}; r.sr = CHOPCONF();` — **có ngoặc = đọc thanh ghi qua UART**, không phải cache.
> Nên `msteps` là số **thật** của driver. Đo được trên máy này: `msteps 16 …` ⇔ `blank time 24`;
> `msteps 8 …` ⇔ `blank time 36` + `hysteresis -end -3` (= trạng thái reset, `GCONF.mstep_reg_select
> = 0` → microstep lấy theo chân MS1/MS2 → 1/8).

**Vì sao phải flash, không làm được qua USB.** `M350` (đổi microstep bằng G-code) **không tồn tại**
trên board này — `M350` chỉ được biên dịch khi có `HAS_MICROSTEPS`, mà cái đó đòi chân **MS1/MS2**
(`Conditionals_post.h:2814-2822`); MKS Monster8 V2 chạy TMC2209 **UART**, không nối MS1/MS2.
Gửi `M350` chỉ nhận `echo:Unknown command: "M350"`. Microstep vì vậy là **hằng số lúc biên dịch**.

**Bốn chỗ phải sửa cùng lúc** (đây là bản đã thử, nay đã hoàn tác — giữ lại để khỏi mò lại):

| Chỗ | Giá trị đã thử | Ghi chú |
|---|---|---|
| `Configuration.h` → `TTL_XY_MICROSTEP` | thêm mới `8` | tách khỏi `TTL_MICROSTEP` (vẫn 16) |
| `Configuration_adv.h` → `X_MICROSTEPS`, `Y_MICROSTEPS` | `TTL_XY_MICROSTEP` | |
| `Configuration_adv.h` → `Z_MICROSTEPS`, `E0_MICROSTEPS` | `TTL_MICROSTEP` | 🔴 **bắt buộc** — hai dòng này *thừa hưởng* `X_MICROSTEPS`, để nguyên là Z/E tụt xuống 1/8 |
| `Configuration.h` → `DEFAULT_AXIS_STEPS_PER_UNIT` | `{40, 40, 800, 415}` | `200 × 8 ÷ (20 răng × 2 mm) = 40` |

> ⚠️ **EEPROM đè lên mặc định mới — đây là chỗ dễ tự bắn vào chân nhất.** `M92` nằm trong EEPROM
> nên sau khi flash, `M92` **vẫn báo `X80 Y80`** trong khi driver đã ở 1/8 → trục chạy **gấp đôi**.
> Phải `M92 X40 Y40` + `M500` **ngay sau khi flash, trước khi cho máy chạy**. Đây là cạm bẫy 1 ở
> §11.7. Chiều ngược lại cũng đúng: hoàn tác về 1/16 thì phải `M92 X80 Y80` + `M500`.

**Trạng thái khi đã chạy 1/8** (số liệu thật, để đối chiếu nếu có lần thử sau):

```
M115  ->  FIRMWARE_NAME:Marlin 2.1.2 (Oct  7 2026 23:57:19)
M122  ->  msteps   8   8   16   16   16   16                    <- X Y Z Z2 Z3 E
          interp   true true true true true true
M92   ->  X40.00 Y40.00 Z800.00 E415.00
```

Tức là **driver ĐÃ nhận đúng 1/8** và **steps/mm đã khớp** — vậy lỗi trục Y **không** phải do
quên `M92` hay do firmware/driver lệch nhau. `M122` đọc MRES từ driver qua UART nên đây là số thật.

**Vì sao có thể hỏng — giả thuyết còn để ngỏ:**

1. **`settings.cpp` không xử lý `Y2`.** Đoạn ép lại microstep sau khi nạp EEPROM
   (`settings.cpp:2384-2408`) chỉ phủ `X, Y, Z, Z2, Z3, Z4, E0` — **thiếu `Y2`**. Board này không
   khai `Y2`, nhưng nếu có thì `Y2_MICROSTEPS` thừa hưởng `Y_MICROSTEPS` mà driver lại không được
   ghi lại → hai bên lệch.
2. **Máy chạy CoreXY** (`Configuration.h:873` `#define COREXY`), nên X và Y là hai motor A/B **dùng
   chung một chuyển động**. Trong CoreXY, bất kỳ sai lệch nào giữa hai driver X và Y đều biểu hiện
   thành **lệch trục/kẹt** chứ không phải "một trục chạy sai quãng đường" — nên triệu chứng "Y hỏng"
   là **hợp lý** với một sai lệch giữa X và Y, dù cả hai đều khai 8.
3. Khác biệt hành vi của TMC2209 ở MRES=8 khi có `INTERPOLATE true`.

> ℹ️ **Flash KHÔNG xoá EEPROM** (đã kiểm chứng hai lần): sau khi nạp, `M851 Z0.70`,
> `M205 X8 Y8 Z0.40 E5` và **mesh UBL** vẫn còn (`M420 V` → `Mesh is valid`). Marlin chỉ xoá EEPROM
> khi `EEPROM_VERSION` đổi. Đổi lại: `M92`/`M201`/`M203` cũ **vẫn đè** mặc định mới.

### 11.5 File mới của dự án

| File | Mục đích |
|---|---|
| `upload-dfu.ps1` | nạp qua DFU: tự tìm `dfu-util`, dùng `-t 2048`, ghi đúng `0x0800C000`, in SHA256 |
| `upload-firmware.ps1` | nạp qua ST-Link |
| `send-gcode.ps1` | gửi G-code qua cổng serial (mặc định COM4) và in phản hồi. Board dùng **USB CDC** nên `BAUDRATE` không quan trọng; đọc "cho tới khi lặng" thay vì đợi `ok`, vì `M997` làm board **biến mất** khỏi cổng. Ví dụ: `.\send-gcode.ps1 -Command M122,M92` |
| `monitor-temp.ps1` | đặt nhiệt độ hotend và **theo dõi bằng `M105` trong MỘT kết nối duy nhất** (mở lại cổng COM giữa chừng có thể reset board → mất `M104` target). Đọc + in **trước** khi bật heater; **tự `M104 S0`** nếu vượt `target + GuardBand`; tự tắt khi hết giờ. `-ProbeOnly` kiểm đường hiển thị mà không bật heater. Ví dụ: `.\monitor-temp.ps1 -Target 100 -Seconds 130` |
| `UPLOAD_README.md` | hướng dẫn nạp + xử lý sự cố DFU |
| `cura_profile/machine_definition_changes.inst.cfg` | **Profile Cura — container của MÁY IN** (bàn, gốc, endstop, feedrate/accel/jerk, steps/mm, Start/End G-code) |
| `cura_profile/extruder_definition_changes.inst.cfg` | **Profile Cura — container `definition_changes` của EXTRUDER** (Extruder Start G-code = đường purge, Extruder End G-code = retract) |
| `cura_profile/extruder_user.inst.cfg` | **Profile Cura — container `user` của EXTRUDER** (tốc độ retract). Phải nằm ở đây, xem 11.6 |
| `cura_profile/voron21_300_mks_monster8.def.json` | Định nghĩa máy in mới (`inherits: voron2_base`) — chỉ dùng khi muốn thêm máy in riêng trong Cura |
| `install-cura-profile.ps1` | Áp cả 4 container + 2 script hậu xử lý + **đồng bộ setting đang lưu của script** vào Cura (mặc định sửa máy in "Voron2 300" đang có, **không cần Admin**) |
| `cura_profile/fix-pp-settings.py` | Đồng bộ các giá trị **đang lưu** của post-processing script trong `machine_instances\*.global.cfg` (chúng đè lên `default_value` của `.py` — xem cạm bẫy 26). Chạy tay: `python cura_profile/fix-pp-settings.py [--what-if]` |
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
| **Printer** | Start G-code | `machine_start_gcode` | `M104 S230` + `M140 S60` (**cố định**) → `G28` → `M190 S60` → `M109 S230` → `M420 S1`. **`G34` và `G28 Z` đã bỏ** — xem §5 |
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

### Script hậu xử lý — `ClampFeeds.py`

**Đây là script quan trọng nhất, và là cơ chế DUY NHẤT đã chứng minh chạy được trên máy này.**

Làm 4 việc, mỗi việc có setting riêng:

| Việc | Setting | Mặc định |
|---|---|---|
| Ép mọi `F` về **trần từng trục** | `max_feedrate_xy` / `_z` / `_e` | **150** / 10 / 25 mm/s |
| Ép `M204 S` về trần | `clamp_acceleration`, `max_acceleration` | bật, **300** mm/s² |
| **Tách mọi bước XY+Z** thành 2 bước | `split_xyz_moves` | bật |
| Chèn bước **về tâm** sau purge | `after_purge_xy`, `after_purge_f` | `152.5,152.5`, 6000 |

Kết quả đo trên `V300_Part3.gcode`:

| | Trước | Sau |
|---|---|---|
| Bước vượt trần | 5 | **0** |
| `M204 S` lớn nhất | 5000 | **500** |
| Bước có cả XY lẫn Z | 1 | **0** |

Purge sau khi qua script:

```gcode
; ----- Duong purge (chay sau Start G-code cua may) -----
G1 F5000 X2.0 Y10.0          ; di NGANG o Z cao (2.0)
G1 F600 Z0.3                 ; toi noi roi MOI ha
G1 F1500 X2.0 Y100.0 E15.0   ; purge doc theo Y
G92 E0
G1 Z2.0 F600                 ; nhac Z len
G92 E0
G1 F900 E-0.75
; --- ClampFeeds: ve tam sau purge ---
G1 F6000 X152.5 Y152.5
;LAYER_COUNT:5
```

**Bật:** `Extensions` → `Post Processing` → `Add a script` → **Clamp Feeds** → `Close`.
Cura chỉ nạp script lúc khởi động nên phải **mở lại Cura**. Bật một lần là các lần slice sau tự chạy.

> 🔵 **Vì sao dùng script mà không sửa profile Cura.** Đã đo bằng `cura.log` — log ghi **chính xác** setting gửi cho CuraEngine. Sau **ba** lần thử đặt vào `definition_changes`, container `user` của máy in, và container `user` của extruder, Cura **vẫn** gửi `acceleration_print = 5000` và purge cũ. Cura không đọc các file cấu hình ghi từ bên ngoài (nó ghi lại bằng trạng thái trong bộ nhớ).
> Post-processing script thì luôn chạy — `FirstLayerTwice` đã đúng suốt từ đầu.
> ⇒ **Mọi thứ cần sửa G-code thì làm trong script**, Cura chỉ giữ phần dữ liệu in.

> ⚠️ **Hai lỗi bắt được khi test script này** (đừng lặp lại):
> - **Làm tròn LÊN khi kẹp sẽ vượt trần**: trần Z 10 mm/s → `F3648.9` làm tròn thành `F3649` = 10.0014 mm/s. Phải làm tròn **xuống**.
> - **Z không đổi thì đừng phát bước Z**: nếu không sẽ sinh `G1 F1500 Z0.3` dài 0 mm — vô nghĩa và bị audit bắt lỗi.

Test: `tests/test_clamp_feeds.py` — chạy trên file gcode thật, kiểm chứng lại từng trục sau khi sửa, và fail nếu còn bước vượt trần hoặc còn bước XY+Z.

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

**Cách làm — dùng `M221` (flow của firmware), KHÔNG nhân lại E:**

| Lượt | Body dùng | Z thực | Flow |
|---|---|---|---|
| Pass 1 = lớp **miết bám bàn** | body **đã hạ** `layer_z − first_pass_z` | **0.1** (`first_pass_z`) — mỏng hơn `layer_height` | `M221 S60` → **60 %** (bạn chọn, `pass1_flow`) |
| ↕ giữa hai pass | — | `G1 Z0.3` = `first_pass_z` + `layer_z` (0.1 + 0.2), rồi `G92 Z0.2` → khai báo lại **Z gốc của layer 0** | — |
| Pass 2 = lớp **bình thường** | body **GỐC**, không hạ | **0.3** (0.2 trong hệ đã lệch 0.1) — cao hơn pass 1 **đúng một chiều cao layer init** (0.2) | `M221 S100` → **100 %** |
| Layer 1 trở lên | không đụng | 0.4 → **0.5** | `M221 S100` → 100 % |

```gcode
; --- First Layer Twice: ket thuc pass 1 ---
M400                        ; doi in xong lop thu nhat
G90                         ; toa do tuyet doi
G1 Z0.3 F600                ; nang Z len first_pass_z + Z goc cua layer 0 (0.1 + 0.2)
G92 Z0.2                    ; khai bao lai day la Z goc cua layer 0
G92 E-0.75                  ; reset extruder ve E dau layer 0
M221 S100                   ; flow pass 2 = 100% (da nang len 1 chieu cao layer init)
; --- First Layer Twice: bat dau pass 2 (100%) ---
```

> 🔵 **Pass 1 LUÔN ở `first_pass_z` = 0.1; pass 2 ở `first_pass_z + <Z gốc của layer 0>`.**
> `first_pass_z` **không** phụ thuộc `layer_height`. Script **dịch body của layer 0 xuống**
> bằng `_shift_z(body, layer_z - first_pass_z)` cho **pass 1**, còn **pass 2 dùng lại body GỐC**
> — đây là điểm mấu chốt, xem cảnh báo ngay dưới.

> 🔴 **Pass 2 PHẢI dùng body GỐC, không được dùng body đã hạ.** Nếu pass 2 dùng lại body đã hạ
> thì toạ độ Z của nó là `first_pass_z` (0.1) trong khi layer 1 của Cura là `0.4` → bước từ
> layer 0 lên layer 1 thành **0.3**, tức **hở 0.1 mm không khí** giữa hai layer. Đã đo được trên
> file sạch bằng `tests/_z_report.py`:
> ```
> pass 2 dung body DA HA :  pass1 0.1 -> pass2 0.3 (0.2 ✓) -> layer1 0.6 (0.30 ✗)
> pass 2 dung body GOC   :  pass1 0.1 -> pass2 0.3 (0.2 ✓) -> layer1 0.5 (0.20 ✓) -> layer2 0.7 (0.20 ✓)
> ```
> Chỉ đổi `G92` **không** sửa được lỗi này — phải đổi cả body. Test hồi quy có phép thử đối
> chứng cho đúng trường hợp sai (`tests/test_first_layer_twice.py`, mục 2b).

> ⚠️ **Hệ quả: cả bản in cao hơn model đúng `first_pass_z` (0.1 mm).** Pass 2 dùng lại body gốc
> với `G92 Z0.2`, nên từ đó về sau chương trình là **y hệt bản in bình thường** — mọi bước layer
> đều đúng `layer_height`. Nhưng gốc toạ độ đã bị đẩy lên 0.1 mm, và điều đó là **đúng vật lý**:
> pass 1 đã in ra nhựa thật. Nếu cần chi tiết lắp khít theo Z thì đừng dùng cách này.

**Pass 1 và pass 2 ghi ra CÙNG một giá trị E** — không có phép nhân nào; Marlin tự nhân E với
`flow_percentage`. **Hai pass KHÔNG còn cộng lại thành 100 %** (đó là cách của bản cũ 20 % + 80 %):
nay pass 1 và pass 2 nằm ở **hai độ cao khác nhau**, nên lượng nhựa thực tế = **`pass1_flow` % + 100 %**
— pass 1 là lớp miết mỏng bám bàn, pass 2 là **một lớp đầy đủ bình thường**.

> **Vì sao `G92 E<e đầu layer 0>` là bắt buộc.** Trước layer 0 có thể có retract (đường purge kết
> thúc bằng `G1 F900 E-0.75`), nên E lúc bắt đầu layer 0 **không phải 0**. Reset về 0 thì bước
> unretract đầu tiên của pass 2 thành delta 0 → **mất 0.75 mm nhựa**. Script quét E qua **cả phần đầu
> của chính chunk layer 0** (start G-code và đường purge nằm cùng chunk với `;LAYER:0` trong file
> thật), không chỉ các chunk trước đó. Đây là lỗi đã bị test bắt.

> ⚠️ **Đọc Z của layer 0 phải lấy MIN, không được lấy giá trị đầu tiên.** `voron2_base` của UltiMaker
> bật Z-hop (`retraction_hop_enabled = true`, `retraction_hop = 0.2`), nên bước `G0/G1` đầu tiên của
> layer 0 là bước **nâng lên** `0.2 + 0.2 = 0.4`:
> ```gcode
> G1 F600 Z0.4     ; Z-hop, KHONG phai chieu cao layer
> G0 F11250 X.. Y..
> ;TYPE:SKIRT
> G1 F600 Z0.2     ; day moi la chieu cao layer 0
> ```
> Lấy nhầm `0.4` làm `layer_z` thì độ dịch thành `0.4 − 0.1 = 0.3`, đường in thật của pass 1 rơi
> xuống **`Z-0.1`** → Marlin kẹp về 0 (`motion.cpp:960`) và **đầu in cày trên mặt bàn**; bước nhấc
> Z cũng thành `G1 Z0.5`. Trong một layer, mọi Z-hop đều **cao hơn** chiều cao layer, nên `min()`
> luôn đúng.
> Xem cạm bẫy 17 và test hồi quy `tests/test_first_layer_twice.py` (có phép thử đối chứng mô phỏng
> lại đúng cách sai này để chắc rằng test thật sự bắt được lỗi).

> 🔵 **Vì sao pass 2 phải nâng lên một chiều cao lớp.** Nếu in cả hai pass ở **cùng một Z** thì
> đầu in của pass 2 **cày xuyên qua** lớp nhựa vừa in của pass 1 — đó chính là tiếng **tạch tạch
> như bị va** ở đầu bản in. Nhấc lên đúng `layer_z` thì pass 2 nằm **cách mặt lớp 1 đúng một
> chiều cao lớp**, giống hệt in layer 1 bình thường, nên không còn cọ.
>
> Vì pass 2 đã nằm **cao hơn pass 1 đúng một chiều cao layer init** (0.2), nó in y như một lớp bình
> thường ⇒ **flow pass 2 để 100 %**; giảm flow ở đây chỉ làm lớp đó thiếu nhựa.
>
> Còn pass 1 để ở **0.1** (mỏng hơn `layer_height` 0.2) với **60 % nhựa** là để **ép nhựa bám bàn**:
> đầu in miết sát mặt bàn, nhựa dẹt ra và dính chắc — đúng vai trò "lớp dính bàn". Con số này **do bạn
> chọn** (`pass1_flow`): quá ít thì lớp 1 không dính, quá nhiều thì nhựa trào/lem.

**Lượng nhựa:** không cần tính gì cả. `M221` nhân ở firmware, nên pass 1 đùn `pass1_flow` % (mặc định
**60 %**) và pass 2 đùn **100 %** của **cùng một bộ giá trị E** → tổng **160 %** nhựa nằm trên **hai độ
cao** (0.1 và 0.3): một lớp mỏng miết bám bàn + một lớp đầy đủ.

> ℹ️ **Điểm hay của cách này so với cách cũ.** Cách cũ phải nhân lại từng giá trị E và phải phân biệt
> bước có XY (nhân) với bước thuần E như retract (không nhân) — vì nhân retract với 20% sẽ biến nó
> thành vô nghĩa. Dùng `M221` thì firmware lo hết, code đơn giản hơn nhiều. Nhược điểm duy nhất:
> **retract trong layer 0 cũng bị nhân theo flow** — nhưng nay nhẹ hơn nhiều: retract 0.75 mm ở
> pass 1 (60 %) còn 0.45 mm, còn ở **pass 2 (100 %) thì không bị ảnh hưởng gì**.

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `force_temperatures` | bật | Ép nhiệt độ toàn file |
| `hotend_temp` | 230 °C | `M104`/`M109` bị ép về số này |
| `bed_temp` | 60 °C | `M140`/`M190` bị ép về số này |
| `double_first_layer` | bật | In layer 0 hai lần |
| `pass1_flow` | **60 %** | `M221 S<so nay>` trước pass 1 — **lớp miết bám bàn** (in ở `first_pass_z`). Bạn chọn: tăng nếu lớp 1 không dính, giảm nếu nhựa trào |
| `pass2_flow` | **100 %** | `M221 S<so nay>` trước pass 2 — **để 100 %**: pass 2 nằm cao hơn pass 1 đúng một chiều cao layer init nên in như lớp bình thường |
| `z_feedrate` | 600 mm/min | Tốc độ nhấc Z giữa hai pass |
| `first_pass_z` | 0.1 mm | **Z của pass 1 — cố định, không phụ thuộc `layer_height`.** Pass 2 dùng body gốc nên in ở `first_pass_z + <Z gốc của layer 0>` |
| `z_mode` / `z_raise` / `layer0_z` | — | **Không dùng nữa.** Giữ lại trong khai báo để config cũ trong Cura không báo lỗi; script bỏ qua |

**Bật trong Cura:** `Extensions` → `Post Processing` → `Add a script` → **First Layer Twice**.
Script nằm ở `%APPDATA%\cura\<version>\scripts\FirstLayerTwice.py` và **chỉ được nạp lúc Cura
khởi động** — thêm file xong phải mở lại Cura.

> ⚠️ **Giá trị ĐANG LƯU trong Cura đè lên `default_value` của script.** Cura lưu cả khối
> `[FirstLayerTwice]` vào `machine_instances\*.global.cfg`, nên **sửa default trong `.py` là CHƯA ĐỦ** —
> đã gặp thật 2026-10-12: `.py` đổi 20→60 nhưng khối đang lưu vẫn là `20`/`80`.
> `cura_profile/fix-pp-settings.py` nay **đồng bộ luôn** `pass1_flow = 60` và `pass2_flow = 100`
> (được gọi tự động trong `install-cura-profile.ps1`), và sau khi chạy phải **mở lại Cura**.

> Cura yêu cầu **tên class trùng tên file** (`PostProcessingPlugin.py:215`:
> `getattr(loaded_script, script_name)`). Đặt tên khác là Cura báo *"not a recognised script type"*.

#### Bộ công cụ kiểm tra ngoài Cura

Cả hai script đều **không cần mở Cura** để kiểm tra. Ba lệnh:

| Lệnh | Việc |
|---|---|
| `python tests\test_first_layer_twice.py` | Test hồi quy `FirstLayerTwice`: dựng gcode giả theo đúng cấu trúc Cura rồi khẳng định Z/E/M221 |
| `python tests\test_clamp_feeds.py` | Chạy `ClampFeeds` trên **file gcode thật** rồi audit lại từng trục |
| `python tests\verify_gcode.py <file.gcode>` | Soi một file đã xuất: mốc `FirstLayerTwice` + audit feed/accel của `ClampFeeds` |

Thêm một harness để **xem trước kết quả** mà không phải slice qua giao diện:

```
python tests\run_first_layer_twice.py <file-vao> [file-ra]
```

Nó nạp **đúng** `cura_profile/scripts/FirstLayerTwice.py`, đọc **setting thật** từ machine instance
của Cura (`%APPDATA%\cura\<ver>\machine_instances\*.global.cfg`), chia gcode theo kiểu Cura rồi gọi
`execute()` — nên kết quả giống hệt Cura sẽ sinh ra.

> ⚠️ **Gcode Cura lưu ra đĩa LUÔN là gcode đã qua hậu xử lý** (có `;POSTPROCESSED` + tên script
> trong header), nên file đã slice vẫn còn marker trong đó. Harness **từ chối chạy** nếu file đã
> có marker `; --- First Layer Twice:` — chạy lại lần hai sẽ chèn chồng hai khối lên nhau.
> Muốn xem trước: **tắt** `FirstLayerTwice` trong Cura → slice → chạy harness trên file vừa sinh
> (file đó chỉ còn `ClampFeeds`).

> ℹ️ **Thứ tự script quan trọng.** Header ghi `;  [FirstLayerTwice]` rồi `;  [ClampFeeds]`, tức
> `ClampFeeds` chạy **sau**, nên nó nhìn thấy cả bước nhấc Z do `FirstLayerTwice` chèn vào và kẹp
> luôn (`G1 Z0.3 F600` = 600/60 = **10 mm/s**, đúng bằng trần Z nên hợp lệ). Nếu đảo thứ tự thì
> bước nhấc đó **thoát** khỏi audit. `verify_gcode.py` cũng kiểm luôn điều kiện này.

Hai file dùng chung (`test_first_layer_twice.py`, `run_first_layer_twice.py`, `verify_gcode.py`):

| File | Việc |
|---|---|
| `tests/_cura_stub.py` | Dựng lại cây plugin của Cura (`<pkg>/Script.py` + `<pkg>/scripts/<tên>.py`) vì `FirstLayerTwice` dùng `from ..Script import Script` — **không** import thẳng file được. Kèm hàm đọc setting từ `global.cfg` và chia chunk giống Cura |
| `tests/_make_raw_fixture.py` | One-off: dựng lại file "raw" từ một gcode đã qua `FirstLayerTwice`, để thử harness trên dữ liệu thật. Body của pass 2 **chính là** body gốc của layer 0 nên chỉ cần bỏ pass 1 + khối nhảy. ⚠️ File dựng theo cách này vẫn có thể dính di chứng của lỗi cũ (marker `;LAYER:1` bị dán vào dòng trước) — muốn số liệu sạch thì dùng `_make_clean_fixture.py` |
| `tests/_make_clean_fixture.py` | Dựng file "raw" **sạch** 3 layer (Z 0.2 / 0.4 / 0.6) để đo bước Z. Đây là file đã phát hiện lỗi hở 0.1 mm |
| `tests/_z_report.py` | In **Z vật lý** của từng layer (đọc `G92 Z` để tính độ lệch hệ toạ độ) và **bước** giữa các layer. Dùng để bắt lỗi lệch bước |

> 🔴 **Cura escape newline trong `global.cfg` bằng BA dấu `\` + `n`**, không phải hai:
> `[FirstLayerTwice]\\\nenabled = True\\\n...`. Khớp cứng hai dấu `\` sẽ để lại một `\` lụng ở đầu
> mỗi dòng, dòng tiêu đề `[FirstLayerTwice]` thành `[FirstLayerTwice]\` → **không nhận ra được** →
> đọc ra setting rỗng. Phải khớp cả cụm bằng regex: `re.sub(r"\\+n", "\n", raw)`.

Những chỗ profile sửa so với bản Voron gốc của Cura:

| Thiết lập | Bản gốc Cura | Máy này | Vì sao |
|---|---|---|---|
| `machine_center_is_zero` | **True** | **False** | Firmware có gốc `(0,0)` ở **góc trước-trái** bàn. Để `True` là Cura dồn bản in lệch nửa bàn |
| `machine_endstop_positive_direction_x/y` | `True` | **`False`** | `X/Y/Z_HOME_DIR -1` — máy home về **MIN**, Voron gốc home về MAX |
| `machine_width/depth` | 300 | **305** | vùng in thật |
| `machine_height` | 300 | 300 | |
| `machine_max_feedrate_x/y` | 500 | **150** | khớp `M203 X150 Y150`; **`speed_travel` bắt nguồn từ đây** — xem cảnh báo bên dưới |
| `machine_max_feedrate_z/e` | 40 / 120 | **10 / 25** | `M203 Z10 E25` |
| `machine_max_acceleration_x/y` | 20000 | **300** | **trần cứng** — khớp `M201 X300 Y300`, **và là thứ quyết định `M204 S` Cura phát ra** |
| `machine_max_acceleration_z` | 500 | **200** | `M201 Z200` của firmware |
| `machine_max_acceleration_e` | (mặc định 10000) | **1000** | `M201 E1000` |
| `acceleration_print` | 5000 | **300** | khớp trần `M201 X300 Y300` — xem giải thích bên dưới |
| `acceleration_travel` | (công thức → 7000) | **300** | đặt thẳng, **không** dùng công thức `voron2_base` nữa |
| `machine_acceleration` | 5000 | **300** | |
| `jerk_print` / `_travel` | (mặc định 20 / 30) | **8 / 8** | `M205 X8 Y8` — `CLASSIC_JERK` |
| `machine_max_jerk_xy` | (mặc định 20) | **8** | |
| `machine_steps_per_mm_z/e` | 400 / – | **800 / 415** | `M92` |
| `retraction_speed` / `_retract_speed` / `_prime_speed` | 30 / 25 / 25 | **15 / 15 / 15** | ngưỡng `machine_max_feedrate_e − 10 = 15`, xem cảnh báo bên trên |
| Start / End G-code | macro Klipper `PRINT_START ...` | **G-code Marlin** | Firmware là Marlin — `PRINT_START` sẽ bị báo lỗi và **không home/không hâm nóng** |

> 🔵 **Vì sao `machine_max_acceleration_x/y` là chỗ QUAN TRỌNG NHẤT bên Cura (nay khớp ở 300).**
> Marlin tính `accel_thực = min(M204 P, M201 trục)`. Nhưng trước khi tới Marlin, **Cura đã kẹp rồi**:
> `acceleration_print` có `maximum_value` lấy từ `machine_max_acceleration_x/y`. Vì thế khi trần đó là
> **500**, Cura nhận `acceleration_print = 5000` (mặc định của `voron2_base`) rồi **hạ xuống 500** và
> phát `M204 S500` — đó chính là nguồn gốc con số 500, **không phải** profile ghi 500.
> Nay cả hai bên cùng ở **300**: `M201 X300 Y300` trong EEPROM **và**
> `machine_max_acceleration_x/y = 300` + `machine_acceleration = 300` trong container máy của Cura.
> Muốn nhanh hơn thì phải nâng **cả hai** cùng lúc, và **sửa cả `ClampFeeds.max_acceleration`** —
> xem §3.3.

> 📐 **Các mức còn lại tự suy ra, không cần đặt tay.** Mọi `acceleration_*` khác của Cura đều tính từ
> `acceleration_print` (hoặc từ `voron2_base`), nên khi `acceleration_print = 300` thì:
> `acceleration_wall` / `_topbottom` / `_infill` = **300**, `acceleration_support` = **150**,
> `acceleration_roofing` = `acceleration_wall_0` = **180**, `acceleration_layer_0` = **30**
> (lớp đầu chậm — chủ ý của UltiMaker), `acceleration_ironing` / `_flooring` = **300**.
> Tất cả đều ≤ 300 nên firmware **không kẹp chỗ nào nữa**.
> Đối chứng: trên `V300_Part3_proj.gcode` (bản cũ, trần Cura còn **500**) Cura phát `M204 S275`,
> `S388`, `S500` — đúng các bậc suy ra ở trần 500. Với trần 300 thì các bậc đó sẽ thấp hơn tương ứng.

> ⚠️ **Cura KHÔNG phát `M204 T`** — nó chỉ phát `M204 S<n>` (acceleration in). Kiểm chứng trên
> `V300_Part3_proj.gcode`: chỉ có `M204 S50`, `S162`, `S275`, `S388`, `S500` và **không có lệnh `M204 T` nào**.
> Nên `acceleration_travel` của Cura **chỉ ảnh hưởng phần ước lượng thời gian in**, còn travel
> acceleration thật của máy là `M204 T` lưu trong EEPROM.

> ⚠️ **`G0 F12000` không phải lỗi — nó là hệ quả của `machine_max_feedrate_x/y`.** `voron2_base` tính
> `speed_travel` bằng:
> ```
> speed_travel = max(speed_print, round((machine_max_feedrate_x + machine_max_feedrate_y) / 2, -2))
> ```
> Với `machine_max_feedrate_x/y = 500` → `round(500, -2) = 500` mm/s → Cura phát **`G0 F30000`**.
> Với **300** → **`G0 F18000`**. Với **150** → `round(150, -2) = 200` mm/s → **`G0 F12000`**.
> Con số 200 này **vượt trần 150**, nên `ClampFeeds` sẽ kéo về `F9000` — đó là lý do `ClampFeeds`
> phải khai `max_feedrate_xy = 150` cho khớp.
>
> **Đừng nhầm đơn vị:** g-code `F` là **mm/phút**, còn `M203` và Cura là **mm/giây**
> (`fdmprinter.def.json` ghi `"unit": "mm/s"`). `30000 ÷ 60 = 500` — tức `F30000` **bằng đúng trần**,
> không phải vượt. Và kể cả vượt thì Marlin cũng **kẹp** chứ không báo lỗi (`planner.cpp:2415-2419`:
> `if (cs > max_fr) NOMORE(speed_factor, max_fr / cs);`).
>
> ℹ️ `voron2_base` đặt `speed_travel.maximum_value_warning = max(500, round((mx+my)/2, -2)) + 1`, nên
> giá trị suy ra luôn nằm dưới ngưỡng cảnh báo. Ngưỡng cứng `maximum_value` của mọi `speed_*` là
> `√(mx²+my²)` = **212 mm/s** khi mx=my=150 — mà `speed_travel` suy ra là 200 mm/s nên vẫn **lọt**,
> nhưng chỉ còn dư 12 mm/s. **Nếu sau này hạ `machine_max_feedrate_x/y` xuống ≤ 100 thì phải kiểm lại
> chỗ này**, vì `round(100,-2) = 100` vẫn lọt, còn các mức khác thì đổi theo.
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
| 10 | **`microsteps` đọc ra 1/8 thay vì 16** | `refresh_stepping_mode()` ghi đè GCONF từ cache, chân MS1/MS2 không được điều khiển → **trục chạy gấp đôi** | Đã sửa trong `settings.cpp`, xem 11.4. Từ 11.4b, X/Y **cố ý** là 1/8 nên bug này là thứ giữ Z/E ở 1/16 |
| 10b | **Đổi microstep mà quên `M92`** | EEPROM giữ `M92` cũ → driver 1/8 nhưng firmware tính 80 bước/mm → trục chạy **gấp đôi** (và ngược lại: về 1/16 mà quên `M92 X80 Y80` thì chạy **nửa**). Nguy hiểm nhất là lúc **home**: trục lao vào endstop với tốc độ sai | Sau khi flash **gửi `M92` khớp với microstep + `M500` TRƯỚC khi cho máy chạy**. `M350` không có trên board này nên không thể đổi microstep qua USB — xem 11.4b |
| 10c | **Đổi X/Y sang 1/8 microstep** | ✅ **KHÔNG phải nguyên nhân** — sau khi hoàn tác về 1/16, trục Y **vẫn** sai, nên microstep bị đổ oan. Nguyên nhân thật là **pully tuột khỏi trục motor** (cạm bẫy 25). Bản 1/8 tự nó nhất quán (driver 1/8 + `M92` 40) nên vẫn đúng quãng đường | Đã hoàn tác về 1/16 + `M92 X80 Y80`. **Bài học: thấy Y đi chéo thì kiểm vít hãm pully TRƯỚC, đừng nghi microstep/firmware** — xem 11.4b và 11.9 |
| 10d | **Hoàn tác microstep mà không clean rebuild** | Firmware "nửa cũ nửa mới": `DEFAULT_AXIS_STEPS_PER_UNIT` kịp về 80 nhưng `X_MICROSTEPS` vẫn 8 → **trục chạy gấp đôi** trong khi `M92`/`M115` trông hợp lệ | `Remove-Item -Recurse -Force .pio\build\mks_monster8` rồi build lại. Kiểm chứng: `M122` phải in `msteps 16 …` **và** `blank time 24`. Xem cạm bẫy 15b |
| 11 | **Chạy USB không có PSU** | TMC2209 undervoltage → **kéo cứng đường endstop lên HIGH**, mọi endstop báo `TRIGGERED` | Luôn cấp nguồn PSU khi kiểm tra endstop |
| 12 | **Dựng mesh UBL trước khi căn gantry (G34)** | G34 nghiêng lại gantry → mesh cũ hiệu chỉnh **thừa** đúng phần vừa sửa; probe gắn trên gantry nên mesh mã hoá luôn độ nghiêng lúc đo | Luôn **G34 trước, `G29` sau**; tháo/lắp gantry, đổi belt, đổi Z-stepper thì `G29` lại |
| 13 | **Với UBL, `M420 S1` không kiểm tra mesh hợp lệ** (`bedlevel.cpp:62` chỉ check cho `AUTO_BED_LEVELING_BILINEAR`) | `M420 S1` bật leveling trên mesh hỏng → in ra rác mà **không báo lỗi gì** | Xem `M420 V` phải in `Mesh is valid` trước khi tin |
| 14 | **Grep `locked_Z_motor` không thấy chỗ nào *đọc*** | Tưởng cơ chế khoá Z-stepper là no-op → đi "sửa" một thứ đang chạy đúng, tốn cả buổi | Nó dùng **macro nối token**: `locked_##A##_motor` (`stepper.cpp:326-328`, `TRIPLE_SEPARATE_APPLY_STEP`). Grep chữ literal **không bao giờ thấy** |
| 14b | **`git log -S"TÊN_BIẾN"` KHÔNG phát hiện được việc đổi giá trị** | `-S` đếm **số lần xuất hiện của chuỗi**. Đổi `#define INVERT_Y_DIR true` → `false` **không** đổi số lần xuất hiện → commit đó **không hiện ra**. Kết luận sai rằng "dòng này chưa từng bị sửa" | Dùng **`git log -G"INVERT_[XY]_DIR" -p`** (khớp theo **nội dung diff**) hoặc `-S` với **cả dòng kèm giá trị**: `-S"INVERT_Y_DIR true"` |
| 14c | **Thấy `msteps` tụt về 1/8 hoặc về `256` rồi kết luận "firmware ghi sai microstep"** | Khi driver **mất nguồn VM**, thanh ghi đọc ra **rỗng** (`off time 0`, `msteps 256`, `stealthChop false`, `uStep count 0`) và **mọi endstop báo `TRIGGERED`** (chân bị kéo cứng lên HIGH do undervoltage). Rất dễ tưởng là lỗi firmware/`mstep_reg_select` | Trước khi nghi firmware, kiểm **nguồn 24V**: quạt 24V có quay không. Xem cạm bẫy 11 |
| 15 | **Build lỗi `*** [.pio\build\...\SrcWrapper\src] ... cannot find the path specified`** | Build dir hỏng → PlatformIO không tạo lại được thư mục wrapper, build fail ngay | **Xoá `.pio\build\mks_monster8` rồi build lại** — đã gặp và sửa trong 38 s |
| 15b | **Build TĂNG DẦN sau khi sửa `Configuration*.h` cho ra firmware STALE** | `pio run` báo `SUCCESS`, `M115` ra timestamp mới, và một phần cấu hình mới **có** vào (nếu nó nằm ở translation unit được biên dịch lại) — nhưng phần khác thì **không**, nên firmware là "nửa cũ nửa mới". Đã gặp thật khi hoàn tác microstep: `DEFAULT_AXIS_STEPS_PER_UNIT` về 80 nhưng `X_MICROSTEPS` vẫn 8 → trục chạy **gấp đôi** mà trông như đã đúng | Sau khi sửa `Configuration.h` / `Configuration_adv.h`: **`Remove-Item -Recurse -Force .pio\build\mks_monster8`** rồi `pio run`. Đừng tin `SUCCESS` + timestamp. Kiểm chứng bằng `M122` (xem `blank time` = 24 hay 36) và bằng cách tìm mảng hằng số trong `.bin` |
| 16 | **`G34 I<n>` không có tác dụng** | `G34()` gọi `InfiniteG34(3)` với `nloop=3` cứng, nên `parser.intval('I', …)` không bao giờ được đọc | Giới hạn số vòng bằng `G34 Q<n>`; đổi ngưỡng bằng `G34 T<acc>` |
| 17 | **`FirstLayerTwice` đọc nhầm Z-hop thành chiều cao layer** | `voron2_base` bật Z-hop 0.2 → bước `G0/G1` đầu tiên của layer 0 là `Z0.4` (hop) chứ không phải `Z0.2`. Lấy nhầm `layer_z = 0.4` thì độ dịch thành `0.4 − first_pass_z = 0.3`, đường in thật của pass 1 rơi xuống **`Z-0.1`** (Marlin kẹp về 0 → **đầu in cày trên mặt bàn**), và bước nhấc thành `G1 Z0.5` | `_find_layer_z()` phải lấy **min** Z trong body, không lấy Z đầu tiên. Test hồi quy: `tests/test_first_layer_twice.py` (có cả phép thử đối chứng mô phỏng lại cách sai này) |
| 20 | **`FirstLayerTwice` quét E thiếu phần đầu của chính chunk layer 0** | Start G-code và đường purge nằm **cùng chunk** với `;LAYER:0`, nên quét `data[:index]` sẽ bỏ sót retract `E-0.75` cuối cùng. `G92 E0` khi đó sai → pass 2 **mất một lần unretract** | Quét E qua **cả `prefix`** của chunk layer 0: `_scan_mode_and_e(data[:index] + [prefix])`. Test đã bắt được lỗi này |
| 18 | **Thấy `G0 F30000` tưởng vượt trần máy** | G-code `F` là **mm/phút** còn `M203`/Cura là **mm/giây** — `F30000` = 500 mm/s, đúng bằng trần chứ không vượt. Và `speed_travel` của Cura **suy ra từ `machine_max_feedrate_x/y`** nên đổi trần là đổi luôn con số này | Đổi đơn vị trước khi kết luận (`mm/s × 60`). Marlin **kẹp** feedrate chứ không báo lỗi (`planner.cpp:2419`) |
| 19 | **Đặt setting sai container → Cura XOÁ ÂM THẦM khi ghi lại** | 11 key bị xoá khỏi `definition_changes` (container cấp **MÁY**): `acceleration_print`, `acceleration_travel`, `jerk_print`, `jerk_travel`, `machine_steps_per_mm_x/y/z/e`, `machine_endstop_positive_direction_x/y/z`. Chúng **chỉ có tác dụng cho lần slice ĐẦU** sau khi cài, rồi biến mất. Hệ quả thật: `acceleration_print` rơi về mặc định **5000** của `voron2_base` → G-code chứa `M204 S5000`; `machine_endstop_positive_direction_*` mất nên Cura tưởng máy home về MAX | Đặt đúng container: `settable_per_mesh: true` (print setting) → container `user` của máy in; `settable_per_extruder: true` → container **extruder**. Cura cũng tự làm đúng như vậy (`cura/Settings/MachineManager.py:976-992`). Xem §11.6 |
| 21 | **Cộng số thực ra `0.30000000000000004` trong G-code** | `first_pass_z + layer_z` = `0.1 + 0.2` → Python ghi `G1 Z0.30000000000000004 F600`. Marlin vẫn hiểu đúng nên **máy không sai**, nhưng test hồi quy so khớp chuỗi `"G1 Z0.3 F600"` **fail**, và G-code đọc rất khó chịu | Dùng `_fmt_num()` (`"{0:.5f}".format(v).rstrip("0").rstrip(".")`) cho mọi giá trị Z ghi ra, đừng dùng `str()`/`format()` trần |
| 22 | **Đọc setting từ `global.cfg` ra rỗng** | Cura escape newline bằng **ba** dấu `\` + `n`, không phải hai. Khớp cứng hai dấu `\` để lại một `\` lụng ở đầu mỗi dòng → tiêu đề `[FirstLayerTwice]` thành `[FirstLayerTwice]\` → không nhận ra → dict rỗng, tool im lặng bỏ qua mọi phép kiểm phụ thuộc setting | Khớp **cả cụm** bằng regex: `re.sub(r"\\+n", "\n", raw)` (`tests/_cura_stub.py`) |
| 23 | **Chạy lại script hậu xử lý trên file đã xuất** | Gcode Cura lưu ra đĩa **luôn** đã qua hậu xử lý (header có `;POSTPROCESSED`), nên file cũ vẫn còn marker. Chạy lại lần hai sẽ **chèn chồng** hai khối `FirstLayerTwice` lên nhau | `tests/run_first_layer_twice.py` **từ chối** nếu thấy marker. Muốn xem trước thì tắt script trong Cura rồi slice lại |
| 24 | **Pass 2 của `FirstLayerTwice` dùng lại body ĐÃ HẠ** | Toạ độ Z của pass 2 thành `first_pass_z` (0.1) trong khi layer 1 của Cura là `0.4` → bước từ layer 0 lên layer 1 là **0.3**, tức **hở 0.1 mm không khí**. Đo được: `pass1 0.1 → pass2 0.3 ✓ → layer1 0.6 ✗`. **Chỉ đổi `G92` không sửa được** — đổi `G92` chỉ dịch cả hệ, khoảng cách vẫn sai | Pass 2 phải dùng **body GỐC**, `G92 Z<Z gốc layer 0>`. Đo lại bằng `tests/_z_report.py`: mọi bước phải đúng `layer_height`. Test hồi quy: mục 2b của `tests/test_first_layer_twice.py` (có phép thử đối chứng cho đúng cách sai) |
| 25 | **Pully tuột khỏi trục motor X/Y** | Pulley trượt trên trục → **chỉ một belt được kéo** → lệnh **Y** làm đầu in đi **CHÉO 45°** thay vì thẳng; `G28` không chạm công tắc → `kill()`. Cực dễ chẩn đoán nhầm thành lỗi firmware/`INVERT`/kinematics, vì code và cấu hình **hoàn toàn không đổi**. Triệu chứng đi kèm: lệnh X có vẻ vẫn đúng (hướng đó pulley còn bám), rồi một lệnh đột nhiên **không nhích gì** (tuột hẳn) | Siết lại **vít hãm pully** ở **cả hai** motor X/Y, rồi cân lại gantry + `G28` + `G29`. Kiểm tra bằng vít hãm + vạch bút dạ bắc qua pulley và trục. Khoanh vùng bằng **phép thử tách motor** ở §11.9 |
| 26 | **Sửa `default_value` trong script hậu xử lý mà Cura vẫn dùng số cũ** | Khi bật một script, Cura **chép toàn bộ setting của nó vào khối `post_processing_scripts`** trong `machine_instances\*.global.cfg`, và **giá trị đang lưu đó đè lên `default_value`** trong file `.py`. Sửa `.py` rồi cài lại **không có tác dụng gì**. Đã gặp thật: `ClampFeeds` đổi 150/2000 trong `.py` nhưng Cura vẫn gửi 300/500 | Chạy **`cura_profile/fix-pp-settings.py`** (đã được gọi tự động trong `install-cura-profile.ps1`) để đồng bộ khối đang lưu. Kiểm bằng `read_cura_settings()` trong `tests/_cura_stub.py` |
| 27 | **Đọc `post_processing_scripts` chỉ lấy một dòng vật lý** | Khối này **trải trên nhiều dòng** (Cura chèn newline thật, các dòng sau thụt đầu bằng TAB). Đọc mỗi dòng đầu thì **mất hẳn script thứ hai trở đi** — `ClampFeeds` trả về `None` dù nó **có** trong file, dẫn tới kết luận sai "Cura không lưu script đó" | Đọc tiếp các dòng thụt đầu (kieu INI continuation) — xem `read_cura_settings()` trong `tests/_cura_stub.py` |
| 28 | **🔴 `R` âm trong model MPC của EEPROM** | Số hiển thị **giảm** khi đang hâm nóng trong khi heater vẫn cấp điện; nhiệt độ **nhảy −10 °C**; "restart thì đúng lại". Nguy cơ **quá nhiệt/cháy**: MPC điều khiển theo **mô hình**, nên model hỏng làm bộ điều khiển ra lệnh sai và **che mất** lỗi cảm biến — bảo vệ nhiệt không cứu được kiểu này | Xem đầy đủ ở **§6.1**. `M306` phải có `R` **dương**; sửa bằng `M306 ... R0.1284 ...` + `M500` + **reset vật lý** (`M999` **không** đủ) |
| 29 | **Tưởng `M999` là "khởi động lại"** | `M999.cpp:38-45` chỉ đặt `marlin_state = MF_RUNNING`, xả buffer serial, `ui.reset_alert_level()`. **RAM không bị đụng** → mọi trạng thái trong RAM (model MPC `modeled_*`, vị trí) **giữ nguyên**. Dùng `M999` để "làm mới" model MPC là **vô ích mà tưởng là xong** | Cần reset thật: nút RESET hoặc tắt/bật PSU |
| 30 | **Bật heater trước khi xác nhận mọi thứ chạy được** | Một lỗi định dạng chuỗi trong script theo dõi (`"{3,+6:F2}"` — dấu `+` trong phần canh lề là **không hợp lệ** trong .NET) nổ ra **sau khi** `M104 S100` đã gửi → script chết, heater chạy một mình tới 73,9 °C | **Đọc và in `M105` phải xảy ra TRƯỚC khi gửi `M104`** — `monitor-temp.ps1` nay làm đúng vậy, và có `-ProbeOnly`. Tổng quát: đừng bao giờ gửi lệnh gia nhiệt từ một code path chưa chạy sạch |
| 31 | **Chạy `test_clamp_feeds.py` lên file ĐÃ qua ClampFeeds** | Chạy ClampFeeds lần hai lên file đã xử lý làm **hỏng phép đếm vị trí** (bước XY+Z bị tách hai lần, bước "về tâm" bị chèn hai lần) → audit báo **hàng nghìn lỗi GIẢ**. Đã gặp thật: file 2,2 MB đã xử lý → **2481** "bước vượt trần", trong khi chính file đó chỉ có 6 dòng lệch biên | `test_clamp_feeds.py` nay **TỪ CHỐI** file có marker `ClampFeeds:` và trỏ sang `verify_gcode.py`. **File ĐÃ xử lý → dùng `verify_gcode.py`; file CHƯA xử lý → dùng `test_clamp_feeds.py`.** Mặc định test dùng fixture `tests/fixtures/clean_raw.gcode` |
| 32 | **Ngưỡng so trần quá chặt gây lỗi oan** | Dùng `> LIMIT + 1e-6` cho feedrate: một bước `F9000` (= **đúng** trần 150 mm/s) tính lại ra `150,0000x` do sai số dấu phẩy động tích lũy → bị báo vượt trần **oan** (đã gặp: 6 dòng trên file 2,2 MB, tất cả đều `F9000`) | Dùng `TOL = 0,05 mm/s`. Vẫn bắt được mọi vi phạm thật (200 mm/s lệch 50 mm/s) |
| 33 | **Thay thế chuỗi bằng PowerShell trên file có tiếng Việt** | `Get-Content -Raw` **không có `-Encoding`** đọc UTF-8 bằng ANSI → `WriteAllText` ghi lại thành UTF-8 **hỏng**: **916 dòng** tiếng Việt trong README nát thành mojibake (`trần cứng` → `tráº§n cá»©ng`). `Set-Content` không `-Encoding` còn ghi **CRLF** vào file vốn LF | **Sửa text bằng công cụ `edit`, không bằng `-replace` của PowerShell.** Nếu buộc phải dùng Python/PowerShell thì chỉ định encoding rõ ràng và `newline`. Khôi phục: `git checkout -- <file>` |

### 11.8 `G34 Q<n>` — lặp căn gantry tới khi đạt

Tham số do dự án này thêm vào (`G34_M422.cpp:95`):

> ℹ️ **`G34` KHÔNG còn nằm trong start G-code của Cura** — nay chỉ chạy **tay khi bảo trì**
> (`G34 Q99` → `G29` → `M500`). Tham số `Q<n>` dưới đây vẫn dùng y như vậy khi chạy tay. Xem §5.

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

> ⚠️ **Tham số `I<n>` KHÔNG có tác dụng.** `G34()` luôn gọi `InfiniteG34(3)`, mà trong đó
> `z_auto_align_iterations = nloop ? nloop : parser.intval('I', …)` — `nloop=3` luôn thắng, nên
> `G34 I5` vẫn chỉ chạy **3 iteration**. Muốn chạy nhiều hơn thì tăng **số vòng `Q`**, không phải `I`.
> (Đo thực tế: `Configuration_adv.h:1025` ghi `Z_STEPPER_ALIGN_ITERATIONS 5` nhưng log in ra
> `Did 3 of 3`.)

> Thực tế `Q99` gần như tương đương "chạy tới khi xong": gần như không bao giờ chạm 99 lần, vì
> mỗi vòng đã home lại nên sai số giảm dần. Đặt `Q` nhỏ (1–3) nếu muốn giới hạn thời gian chờ.

> ⛔ **KHÔNG có cách huỷ G34 giữa chừng.** Bản trước có thêm "bấm nút encoder để dừng G34" nhưng
> **đã bỏ** (2026-10): nút bấm **không dừng được** G34 đang chạy — thực tế bấm chỉ ăn vào encoder,
> G34 vẫn chạy tiếp tới hết, nên tính năng gây hiểu nhầm. Nay G34 chạy như Marlin gốc.
> Muốn G34 **ngắn** thì giới hạn bằng `G34 Q<n>` (số vòng) và `T<acc>` (ngưỡng dừng) — xem bảng trên.

#### Màn hình G34 riêng trên LCD

Dòng status chỉ rộng **~21 ký tự** (font `ISO10646_1_5x7`, 6 px/ký tự) nên không đủ chỗ cho báo cáo
G34. Vì vậy khi G34 chạy, màn hình **đổi hẳn sang một trang riêng** gồm 5 dòng:

```
G34 PROBE 1/3 P2       <- pha / vòng probe thứ mấy / đang probe Z2
Z1 1.234 UP 0.105      <- cao độ vừa đo · lần điều chỉnh cuối (lên/xuống) · lượng dịch
Z2 1.189 DN 0.045
Z3 1.279 -- 0.000      <- -- = lần cuối không phải dịch
DEV 0.090 / 0.020      <- độ lệch đo được / ngưỡng T cần đạt
```

| | |
|---|---|
| `UP` / `DN` | Chiều dịch **vừa áp dụng** cho trục Z đó: `UP` = nâng (khe hở tăng), `DN` = hạ. Đây đúng là giá trị đưa vào `do_blocking_move_to_z(amplification * z_align_move + ...)`, đã tính cả `adjustment_reverse` |
| `--` | Lần điều chỉnh cuối của trục đó bằng 0 (không phải dịch) |
| `DEV` | `z_maxdiff` = max − min của vòng probe vừa rồi |
| Ngưỡng | `Z_STEPPER_ALIGN_ACC` = **0.02**, hoặc tham số `T<acc>` khi chạy tay |
| `R<n>` | Chỉ hiện khi `Q>1` (ví dụ `G34 Q99`): đang ở vòng lặp thứ n |
| Pha | `PROBE` → `ADJUST` → kết thúc bằng `DONE` (đạt ngưỡng) / `LIMIT` (hết số iteration mà chưa đạt) / `ABORT` (lỗi probe hoặc sai số tăng) |
| Giữ kết quả | **15 giây** sau khi G34 kết thúc rồi tự trả về status screen (`G34_SCREEN_HOLD_MS`, `marlinui.cpp`) |

Các chỗ đã sửa:

| File | Việc |
|---|---|
| `lcd/marlinui.h` | `struct G34Screen` + `MarlinUI::g34_screen` (dữ liệu), `enum G34Phase`, API `g34_screen_begin/refresh/end/tick`, `draw_g34_screen()` |
| `lcd/marlinui.cpp` | Cài đặt API; `g34_screen_tick()` được gọi trong `MarlinUI::update()` để tự trả về status screen |
| `lcd/dogm/status_screen_DOGM.cpp` | `draw_g34_screen()` + nhánh `if (g34_screen.active) return draw_g34_screen();` ở đầu `draw_status_screen()` |
| `gcode/calibrate/G34_M422.cpp` | `g34_screen_begin()` khi bắt đầu, ghi số liệu từng bước probe/dịch, `g34_screen_end()` **sau khi hết cả vòng `Q`** (không đặt trong `InfiniteG34` để trang khỏi bị ẩn/hiện giữa các vòng) |

> **Vì sao phải `PAGE_CONTAINS` cho từng dòng:** LCD này (`MKS_MINI_12864_V3` →
> `U8GLIB_MINI12864_2X_HAL`, xem `marlinui_DOGM.h:102-110`) vẽ theo **8 dải 8 px**, mỗi lần `draw_*`
> chỉ vẽ **một** dải. Font status cao 12 px nên màn 64 px chỉ xếp được **5 dòng**, baseline ở
> y = 10, 22, 34, 46, 58.

> **Chuỗi được dựng ở `first_page`** (dải đầu tiên của mỗi khung hình) rồi mới vẽ ở dải tương ứng —
> dựng ở mọi dải thì `dtostrf`/`snprintf_P` phải chạy 8 lần cho một khung hình.

Cuối mỗi lần G34, serial in thêm một dòng cho từng trục (dễ copy vào log):

```
G34 Z1 last move UP 0.105
G34 Z2 last move DN 0.045
G34 Z3 last move = 0.000
```

### 11.8b `Z ALIGN MANUAL` — dịch tay Z1/Z2/Z3 rồi probe lại từng điểm

**Vì sao cần:** vị trí probe **không trùng** vị trí trục Z. Dịch Z1 (hoặc Z2, Z3) một lượng `x`
thì điểm probe chỉ lên/xuống một lượng **khác** (tỉ lệ đòn bẩy), và tỉ lệ đó còn đổi theo độ
nghiêng của gantry. Nên muốn điểm probe lên/xuống đúng ý thì phải lặp: **probe → dịch một chút →
probe lại → dịch tiếp**. G34 tự động làm việc này bằng thuật toán; màn hình này cho làm **bằng tay**.

**Vào:** `Motion → Z ALIGN MANUAL` (`menu_z_align.cpp`). Nếu máy chưa home, nó tự chèn `G28` rồi
vào màn hình; trước khi vào nó **chỉ nâng** Z lên `Z_CLEARANCE_BETWEEN_PROBES` (5mm) nếu đang thấp
hơn — không bao giờ hạ xuống, để không đâm vào vật đang in.

**Màn hình:**

```
Z2 MOVE    SP 0.090      <- đang chọn Z2, chế độ MOVE, SP = độ lệch max-min giữa các điểm đã probe
>Z1  0.712  0.000        <- '>' = đang chọn | kết quả probe | đã dịch tay bao nhiêu từ lần probe đó
 Z2  0.630  0.100
 Z3  0.622 -0.020
TURN .01 CLICK PROBE     <- dòng hướng dẫn, đổi theo chế độ
```

| Chế độ | Quay encoder | Bấm encoder |
|---|---|---|
| **MOVE** (`Z2 MOVE`) | dịch **đúng trục đó** `0.01mm` mỗi nấc (giữ `set_separate_multi_axis` + `set_all_z_lock` quanh lệnh dịch — cùng cách G34 bù sai số) | sang chế độ **PROBE** |
| **PROBE** (`Z2 PROBE`) | sang **ô kế tiếp**: Z1 → Z2 → Z3 → EXIT → Z1 | **probe điểm đó** (bấm lại được nhiều lần), xong tự về MOVE |
| **EXIT** | về Z1 | **thoát** — và **home lại Z** (`G28Z`) vì khung Z đã lệch sau khi dịch tay |

- **`SP`** (dòng đầu) là con số cần đưa về 0 — giống `DEV` của màn hình G34. Giá trị từng điểm chỉ
  có ý nghĩa **tương đối** (khung toạ độ Z đổi mỗi lần dịch), nên hãy nhìn `SP` và mức thay đổi.
- Số ở cột thứ ba là **lượng đã dịch tay kể từ lần probe gần nhất của điểm đó** (tự về 0 sau khi probe).

**Ba lớp chặn an toàn** (đều nằm trong `menu_z_align.cpp`):

| Chặn | Giá trị | Ý nghĩa |
|---|---|---|
| Bước mỗi nấc | `ZA_MOVE_SCALE` = **0.01mm** | độ phân giải khi dịch |
| Một lần quay nhanh | `ZA_MAX_DETENTS` = **50 nấc** = 0.5mm | `ENCODER_RATE_MULTIPLIER` đang bật (10×/100×) nên quay nhanh sinh ra rất nhiều nấc; chặn lại để không nhảy một cái thật xa |
| Hạ xuống | `ZA_DOWN_BUDGET` = **1mm** kể từ lần probe gần nhất | 🔴 **Đây là chặn quan trọng nhất.** Vì tỉ lệ đòn bẩy có thể **lớn hơn 1**, không thể tin `current_position.z` để biết đầu in cách bàn bao xa. Muốn hạ tiếp thì **phải probe lại** — đúng vòng lặp mà công cụ này sinh ra để làm |

> ⚠️ **Thoát màn hình là Z được home lại** (`set_axis_never_homed(Z_AXIS)` + `G28Z`), giống
> `HOME_AFTER_G34`. Cần thiết vì sau khi dịch tay từng trục thì khung Z không còn đúng nữa; nếu
> không home lại mà chạy `G29` thì mesh sẽ sai theo.

> ℹ️ **Chưa nạp thử lên máy** (viết lúc đang in). Đã build sạch thành công
> (`Flash 25.0%`, `RAM 9.0%`) nhưng hành vi trên máy cần bạn kiểm lần đầu: vào menu → bấm 1 lần
> (sang PROBE) → bấm lần nữa để probe điểm 1, rồi quay thử vài nấc xem Z1 có nhích đúng chiều không.

### 11.8c `G35` / Tramming Wizard — mốc vật lý = Z-home, mốc vặn vít = `Z-MIN` / `Z-MAX`, 4 nút (PROBE / MAX-MIN / BACK / DONE), **tự động đi tiếp khi `DAT`**, **`DONE` = chấp nhận góc đang đứng**

**Nguyên tắc mốc:** sau mỗi lần home Z, **gốc khung Z nằm ngay tại TÂM BÀN** (vị trí Z-home
`Z_SAFE_HOMING_X/Y_POINT` = `152.5,152.5`). Nên giá trị probe ở mỗi góc **chính là** "z_probe − z_home"
— **không cần probe riêng tâm bàn**, và `z_home = base_home_pos(Z_AXIS)` (thường = `0.00`).

**Lệnh `G35`** (không tương tác): **không probe tâm bàn** → probe 4 góc → in
**chiều cao** (`z_probe − z_home`) **và độ lệch so với mốc** (`delta = chiều_cao − mốc`, với
`mốc = Z-MIN` ở chế độ MAX và `mốc = Z-MAX` ở chế độ MIN) + số mm cần vặn. Không tự di chuyển đi đâu sau khi xong (không park).

```
G35 moc vat ly = Z-HOME (khung Z sau G28 Z)  Z=0.000
Chieu cao TB cac goc = 2.007mm  |  Do lech max-min = 0.030mm
MOC SO SANH = Z-MIN (goc thap nhat) = 2.000mm  -> chien luoc: HA cac goc cao xuong (van CCW)
Chieu van: CCW = HA goc do xuong, CW = NANG goc do len.
Turn Back-Right: DAT (khong can van)  | lech 0.000mm  (cao 2.000mm)
Turn Back-Left: DAT (khong can van)  | lech 0.000mm  (cao 2.000mm)
Turn Front-Left: DAT (khong can van)  | lech 0.000mm  (cao 2.000mm)
Turn Front-Right: van CCW 0.030mm  | lech 0.030mm  (cao 2.030mm)
Chinh truoc: Front-Right (lech 0.030mm) -> van CCW
```

> 🔴 **Vì sao phải trừ đi một MỐC (không dùng thẳng "chiều cao so với Z-home"):** 4 góc
> `2.000/2.000/2.000/2.030` đều "cao +2mm so với Z-home" — nhìn như cả bàn bị nâng 2mm, nhưng thực ra
> **4 góc chỉ lệch nhau 0.030mm** (gần như đã phẳng). "Chiều cao so với Z-home" chỉ là **độ lệch của cả mặt
> bàn so với mốc**, nó phụ thuộc `M851 Z offset`, độ võng tâm bàn và lần home — **không nói gì về độ phẳng**.
> `delta = chiều_cao − mốc` mới là phần **cần vặn**, và firmware nói luôn **vặn chiều nào**: chế độ **MAX**
> (mốc = `Z-MIN`) ⇒ **mọi `delta ≥ 0`** và hướng dẫn luôn là **vặn CCW** (hạ các góc cao xuống); chế độ **MIN**
> (mốc = `Z-MAX`) ⇒ **mọi `delta ≤ 0`** và hướng dẫn luôn là **vặn CW** (nâng các góc thấp lên).
> `|delta| ≤ 0.01mm` (`TRAM_DONE_MM` = 0.01mm) thì in `DAT (khong can van)` — 0.01mm chính là mức nhiễu thật của probe (đo lặp 20 lần lệch 7–10µm), vặn nữa là vô nghĩa.

**Wizard `Motion → Tramming Wizard`** — vòng lặp tự động:

| Bước | Việc |
|---|---|
| 1 | **`G28L0`** = home cả 3 trục **kèm TẮT bed leveling** → **tự động probe 4 góc** (màn hình hiện `Probing corner n/4`). Ngay sau lần đo **đủ 4 góc đầu tiên** firmware **CHỐT MỐC** (`z_ref_min` / `z_ref_max` / `z_ref_set`, hàm `tramming_set_ref()`) và **không tính lại nữa**. ⚠️ Wizard **không bật lại** leveling khi thoát ⇒ sau khi dùng wizard phải `G28` (không `L`) hoặc `M420 S1` trước khi in |
| 2 | Tự động **đi nozzle tới góc lệch nhất** (nâng lên 10mm rồi đi XY ở `XY_PROBE_FEEDRATE`) để bạn vặn vít góc đó. Góc lệch nhất = **cao nhất** (chế độ MAX, mốc = `Z-MIN`, mặc định) hay **thấp nhất** (chế độ MIN, mốc = `Z-MAX`), đổi bằng nút `MAX->MIN` |
| 3 | Hiện độ lệch 4 góc + **4 nút**, thứ tự encoder: **`PROBE`** → **`MAX->MIN`** → **`BACK`** → **`DONE`** (mặc định chọn `PROBE`). Nếu **cả 4 góc đã `DAT`** (`tram_all_dat`) thì **4 số ở 2 dòng đầu là delta-Z so với TÂM BÀN** (dương = góc cao hơn tâm), dòng 2 là `DAT - lech so voi tam`, và **chỉ còn `BACK`** — encoder bị **khoá vào `BACK`** |
| 4 | **`PROBE`** = **CHỈ probe góc đang đứng** (KHÔNG probe tâm, KHÔNG home lại) → hiện **còn phải vặn bao nhiêu mm** + chiều vặn `CW`/`CCW` cho góc đó. Góc vừa đo **đã đạt** (`\|delta\| ≤ 0.01mm` = `DAT`) ⇒ máy **TỰ ĐỘNG đi tới góc cần chỉnh kế tiếp và probe luôn** (`TR_AUTO`, màn hình `Auto next`) — **không cần bấm `DONE`**. Khi quét vòng, máy bỏ qua góc **mốc**, góc **đã đạt** và cả góc **đã bấm `DONE`** (đã chấp nhận) |
| 5 | **`MAX->MIN`** = đổi chiến lược giữa `MAX (mốc = Z-MIN)` ⇄ `MIN (mốc = Z-MAX)`, **lưu ngay vào EEPROM**, rồi **đi ngay tới góc lệch nhất của chế độ mới và probe luôn** góc đó (góc đó **`DAT`** thì cũng **tự đi tiếp**). Đây chỉ là **đổi lấy mốc kia** (cả hai mốc đã chốt), **không đo lại gì** |
| 6 | **`BACK`** = **thoát về màn hình chính**: gọi `tramming_exit()` → stow probe + đánh dấu **Z chưa home** (`set_axis_never_homed(Z_AXIS)`) rồi quay về màn hình trước đó. Vì vít đã bị vặn nên **vẫn phải `G28` trước khi in**. Khi **cả 4 góc `DAT`** thì đây là nút **duy nhất** còn lại trên màn hình (`>BACK`) |
| 7 | **`DONE`** = **"góc này XONG"** (**chấp nhận thủ công**, **vẫn còn, bấm tay**): máy **đánh dấu góc đang đứng là ĐÃ CHẤP NHẬN** (`z_accepted[worst_index] = true`) — **coi như đạt dù `\|delta\|` chưa vào ngưỡng `0.01mm`** ⇒ **không quay lại góc đó nữa**. Nếu còn góc **CHƯA chấp nhận** thì máy **đi thẳng tới góc CẦN CHỈNH kế tiếp** (đi vòng tròn từ góc hiện tại, **bỏ qua góc MỐC**, các góc **đã đạt** `\|delta\| ≤ 0.01mm` **và các góc đã chấp nhận**) rồi **probe luôn** góc đó; con trỏ tự về `PROBE`. Đây là cách **chốt một góc là xong** khi vặn mãi không vào ngưỡng (không còn kiểu bấm `DONE` mà quay vòng mãi giữa các góc đó). Góc đã chấp nhận **vẫn hiện số delta** trên màn hình, chỉ là máy **không tự chạy tới** đó nữa |
| 8 | **Khi KHÔNG còn góc nào CHƯA chấp nhận** (máy **tự** làm, hoặc do bấm `DONE` ở bước 7) = **home lại Z** (vì vặn vít làm Z-home đổi) → **probe lại CẢ 4 GÓC** (vòng mới) → **xoá sạch `z_accepted[]`** → **CHỐT MỐC MỚI** → đi tới góc lệch nhất. **Không cần bấm gì**; `DONE` ở đây là **"góc này xong"** (chấp nhận), **không phải nút thoát** (thoát bằng `BACK`) |
| 9 | **CẢ 4 GÓC ĐỀU `DAT`** ngay sau lần đo **đủ 4 góc** (`tram_all_dat = true`, cũng gồm trường hợp **lần đầu home + probe đã `DAT` hết**) → **4 số ở 2 dòng đầu KHÔNG còn so với MỐC mà là delta-Z so với TÂM BÀN (điểm home Z): `z_delta[i] − base_home_pos(Z_AXIS)`** — số **dương = góc CAO HƠN tâm**, số **âm = góc THẤP HƠN tâm**; dòng 2 hiện **`DAT - lech so voi tam`**, dòng 3 **chỉ còn `>BACK`** (encoder khoá vào `BACK`) và máy **không** tự home + probe lặp vô tận |

```
*RB -0.007 LB -0.007       <- '*' = goc nozzle dang dung (goc dang chinh)
 FL -0.007 FR +0.013       <- so la CON LAI (lech so voi MOC Z-MIN/Z-MAX, 3 so le = 0.001mm)
>PROBE CCW con +0.013       <- PROBE: so mm con phai van + chieu van cho goc dang dung
>MAX->MIN >BACK >DONE      <- '>' = muc dang chon | MAX->MIN: doi chien luoc (moc Z-MIN <-> Z-MAX) | BACK: thoat ve man hinh chinh | DONE: chap nhan goc dang dung la XONG (du chua vao nguong) -> di tiep goc chua chap nhan / het goc -> home lai + do lai 4 goc
```

Khi vừa probe xong một góc mà góc đó **đã `DAT`**, máy **tự đi tiếp** (màn hình hiện `Auto next` trong lúc di chuyển) — không phải bấm gì. Còn khi **cả 4 góc đều `DAT`**, màn hình chỉ còn:

```
*RB +0.004 LB -0.002       <- ca 4 goc |delta| <= 0.01mm = DAT
 FL -0.006 FR +0.008       <- 4 so o 2 dong dau luc nay = delta-Z so voi TAM BAN (diem home Z)
DAT - lech so voi tam      <- tat ca da dat -> khong con gi de van | + = goc CAO hon tam, - = THAP hon tam
>BACK                       <- CHI CON NUT BACK ('>' = muc dang chon, encoder khoa vao BACK)
```

- `*` = **góc đang đứng** (góc đang chỉnh). Góc này **luôn là số vừa đo** (`PROBE`, `MAX->MIN`, `DONE` — và cả bước
  **tự động đi tiếp** khi `DAT` — đều probe luôn góc mới) ⇒ số mm còn phải vặn + chiều vặn ở dòng `PROBE` luôn đúng cho góc đó.
- Các góc chưa đo lại trong vòng này **vẫn dùng số cũ** (máy không xoá) cho tới khi **đã hết góc CHƯA chấp nhận** (lúc đó
  máy **tự** home lại Z + đo lại cả 4 góc; `DONE` **không** home lại — nó chỉ **chấp nhận** góc). Góc nào **chưa có số trong khung Z hiện tại**
  (probe lỗi, hoặc chưa tới lượt đo) thì hiện `  ---` thay cho số (`tram_fmt_delta`, `menu_tramming.cpp:175`)
  — đó là dấu hiệu "chưa đo", không phải "0.000".
- **Tự động đi tiếp khi `DAT`:** ngay sau khi probe một góc mà góc đó **đã đạt** (`|delta| ≤ 0.01mm`, hằng số
  `TRAM_DONE_MM`), máy **tự đi tới góc CẦN CHỈNH kế tiếp và probe luôn** góc đó — **không cần bấm `DONE`**
  (trạng thái `TR_AUTO`, mỗi khung hình 1 bước, màn hình hiện `Auto next`). Hết góc **chưa chấp nhận** thì máy
  **tự home lại Z + probe lại CẢ 4 GÓC** (vòng mới, mốc mới). `DONE` **vẫn còn** để **chấp nhận** một góc bằng tay.
- **`DONE` = chấp nhận góc đang đứng là XONG:** `z_accepted[worst_index] = true` — góc đó được **coi như đạt dù
  `|delta|` chưa vào ngưỡng `0.01mm`**. Hàm `tramming_need_adjust(i)` trả `false` cho góc đã chấp nhận, nên góc đó bị
  **bỏ qua khi quét vòng** (cả bước **tự động đi tiếp** sau `PROBE` lẫn `DONE`) ⇒ máy **không quay lại góc đó nữa**
  (trước đây `DONE` chỉ nhảy tới góc còn lệch quá ngưỡng, nên một góc vặn mãi không vào ngưỡng làm `DONE` **quay vòng mãi**).
  Còn góc **chưa chấp nhận** ⇒ đi thẳng tới góc đó + **probe luôn**; **hết** góc chưa chấp nhận ⇒ **tự home lại Z + probe
  lại cả 4 góc** (vòng mới, **`z_accepted[]` xoá sạch**, **`z_ref_*` chốt mốc mới**). `z_accepted[]` cũng bị **xoá sạch** khi
  **vào lại menu** (`goto_tramming_wizard()`) và trong `tramming_restart()`. Góc đã chấp nhận **vẫn hiện số delta**
  bình thường trên màn hình (không có ký hiệu riêng) — chỉ là máy **không tự chạy tới** đó nữa. `DONE` **không phải nút thoát**.
- **Cả 4 góc `DAT`:** nếu ngay sau lần đo **đủ 4 góc** mà cả 4 đều trong ngưỡng (`tram_all_dat = true` — cũng gồm
  trường hợp **lần đầu home + probe đã `DAT` hết**) thì **4 số ở 2 dòng đầu đổi sang delta-Z so với TÂM BÀN**
  (`z_delta[i] − base_home_pos(Z_AXIS)`, tâm = điểm home Z do `G28 Z` probe tại `Z_SAFE_HOMING_X/Y_POINT`):
  **dương = góc CAO HƠN tâm, âm = góc THẤP HƠN tâm** (khi đã phẳng thì điều còn lại cần biết là 4 góc cao/thấp
  hơn tâm bao nhiêu), dòng 2 hiện **`DAT - lech so voi tam`** và dòng 3 **chỉ còn `>BACK`**;
  encoder bị **khoá vào `BACK`** và máy **không** tự home + probe lặp vô tận.

> 🔀 **Nút `MAX->MIN` (dòng 3)** — nhãn luôn đọc là *"đang chỉnh gì → bấm sẽ chuyển sang gì"*:
> `MAX->MIN` (đang chỉnh góc **cao nhất**, bấm để chuyển sang góc **thấp nhất**) hoặc `MIN->MAX`.
> Bấm: máy **đổi ngay**, **lưu EEPROM**, rồi **đi ngay tới góc lệch nhất của chế độ mới và probe luôn** góc đó.
> ➡️ **Nút `DONE` (dòng 3)** — **"góc này XONG"**, **chấp nhận thủ công**: máy **đánh dấu góc đang đứng là ĐÃ CHẤP NHẬN**
> (`z_accepted[worst_index] = true`) — **coi như đạt dù `|delta|` chưa vào ngưỡng `0.01mm`** ⇒ **không quay lại góc đó nữa**;
> nếu còn góc **chưa chấp nhận** thì **tự đi tới góc CẦN CHỈNH kế tiếp** (vòng tròn từ góc hiện tại, **bỏ qua góc MỐC**,
> các góc **đã đạt** `\|delta\| ≤ 0.01mm` **và các góc đã chấp nhận**) **và probe luôn** góc đó (số của góc đó phải mới, vì vừa vặn một góc
> thì các góc kia đã lệch đi). Bình thường **không cần bấm**: ngay khi góc vừa probe **`DAT`** thì máy đã **tự** đi tiếp (màn hình `Auto next`) — `DONE` dùng khi **vặn mãi không vào ngưỡng** mà muốn **chốt góc đó là xong** (trước đây bấm `DONE` trong tình huống này sẽ **quay vòng mãi**).
> Khi **hết góc chưa chấp nhận** thì máy **tự home lại Z + probe lại cả 4 góc** (vòng mới, **`z_accepted[]` xoá sạch**, mốc mới).
> `DONE` **không phải nút thoát** — thoát bằng `BACK`.
> Chỉ 1 mục được chọn tại một thời điểm nên chỉ có **1 dấu `>`** trên màn hình.
> ⬅️ **Nút `BACK` (dòng 3)** — **thoát về màn hình chính**: `tramming_exit()` → stow probe + `set_axis_never_homed(Z_AXIS)`
> rồi quay về màn hình trước đó. Vì **không home lại** mà vít đã bị vặn, **vẫn phải `G28` trước khi in**.

- Quay encoder = chọn 1 trong **4 mục**, theo đúng thứ tự trên màn hình:
  **PROBE → MAX->MIN → BACK → DONE**; bấm = chạy mục đang chọn. Mặc định chọn **PROBE**. Khi **cả 4 góc đã `DAT`**
  (`DAT - lech so voi tam`) thì encoder bị **khoá vào `BACK`** — màn hình chỉ còn đúng 1 mục.
- **Dòng `PROBE` hiện luôn hướng vặn cho góc đang đứng.** Trước khi bấm: `CCW 0.150mm` = vặn CCW
  0.150 mm (`CCW` = vặn để **hạ** góc đó xuống, `CW` = vặn để **nâng** lên); `0.01mm < |delta| < 0.02mm` thì
  hiện `CCW nhe` (vặn nhẹ một nấc). Sau khi bấm: `CCW con +0.013` = vặn tiếp **CCW**, **còn phải vặn
  +0.013mm** nữa. `|delta| ≤ 0.01mm` (`TRAM_DONE_MM`) thì hiện `DAT` (đã đạt, trong ngưỡng nhiễu) — và máy
  **tự đi tiếp** tới góc cần chỉnh kế tiếp ngay, **không cần bấm `DONE`**.
- **Sau mỗi lần `PROBE` chỉ lấy `z` CUỐI** (giá trị probe mới nhất, cùng khung Z vì không home lại)
  ⇒ hiện ngay **số mm còn lại**. Đây là cách tự kiểm tra chiều vặn: vặn **đúng** chiều thì số đó
  **giảm**, vặn **sai** chiều thì số đó **tăng**. Không cần cộng dồn gì cả. Nếu góc vừa probe **đã `DAT`**
  (`|delta| ≤ 0.01mm`) thì máy **tự đi tiếp** tới góc cần chỉnh kế tiếp ngay (màn hình `Auto next`).
- **4 số trên 2 dòng đầu = `delta` so với MỐC** (`Z-MIN` ở chế độ MAX, `Z-MAX` ở chế độ MIN — giống `G35`);
  **riêng khi CẢ 4 GÓC ĐỀU `DAT` thì mốc đổi thành TÂM BÀN** (`base_home_pos(Z_AXIS)` = điểm home Z,
  `Z_SAFE_HOMING_X/Y_POINT`): 4 số = `z_delta[i] − base_home_pos(Z_AXIS)` ⇒ **dương = góc CAO HƠN tâm,
  âm = góc THẤP HƠN tâm** (lúc đó mới là thứ còn cần biết),
  in **3 số lẻ** để thấy được sai số 0.001mm — vì sau khi đã gần phẳng thì các số chỉ còn vài phần trăm mm.
  Góc nào lệch ≥ 1mm thì tự rút về 2 số lẻ cho vừa dòng (màn hình rộng 21 ký tự). Đây chính là **phần còn lại
  phải vặn** ở góc đang đứng (`*`).
- **Nút `DONE` (dòng 3) — chấp nhận góc đang đứng là XONG:** máy **đánh dấu `z_accepted[góc đang đứng] = true`** (coi như đạt
  **dù `|delta|` chưa vào ngưỡng `0.01mm`**) ⇒ góc đó **không bao giờ được chọn lại** khi quét vòng, rồi **đi tới góc CẦN CHỈNH
  kế tiếp** (vòng tròn 1→2→3→4→1, **bỏ qua góc MỐC**, các góc **đã đạt** `\|delta\| ≤ 0.01mm` **và các góc đã chấp nhận**)
  **và probe luôn góc đó**. Bình thường **không cần bấm**: máy **tự** làm đúng việc này ngay khi góc vừa probe **`DAT`** (màn hình `Auto next`);
  `DONE` để **chốt** một góc vặn mãi không vào ngưỡng (không còn **quay vòng mãi**). Sau mỗi lần bấm, con trỏ tự về `PROBE`; khi
  **hết góc CHƯA chấp nhận** (dù do tự động hay do bấm `DONE`) máy **tự home lại Z và đo lại cả 4 góc**, đồng thời **xoá sạch `z_accepted[]`**.
- **Nút đổi chiến lược (`MAX->MIN` / `MIN->MAX`, dòng 3):** vít của góc đang
  chỉnh có thể **hết hành trình** (siết/nới mãi không được). Khi đó thay vì điều chỉnh góc **cao nhất**
  thì **chuyển sang góc thấp nhất** (và ngược lại): bấm nút một lần là máy **đổi ngay** (đổi mốc giữa
  `Z-MIN` và `Z-MAX`), **đi ngay tới góc lệch nhất mới và probe luôn** góc đó, và **nhớ luôn** cho các lần sau.
  Nhãn nút luôn đọc theo kiểu *"đang chỉnh gì → bấm sẽ sang gì"*.
- **Khi đã hết góc CHƯA chấp nhận** (máy **tự** làm, hoặc do bấm `DONE` để chốt) = `G28 Z` → đo lại **cả 4 góc** (khung Z mới, `z_ok[]` **và `z_accepted[]`** xoá hết) →
  **chốt MỐC MỚI** → đi tới góc lệch nhất. Đây là cách **"làm lại từ đầu"**; muốn **thoát** thì bấm **`BACK`** (mục giữa `MAX->MIN` và `DONE`) — và khi **cả 4 góc `DAT`** thì `BACK` là **nút duy nhất** còn lại.
- **Chế độ đang chỉnh (MAX/MIN) được nhớ qua các lần sau** — lưu vào EEPROM ngay khi bấm (không cần `M500`), nạp lại lúc
  khởi động. Cũng đổi được ở **`Motion` → `WORST POINT = MAX`** (ON = MAX, OFF = MIN) và xem lại bằng
  `M503` (`; Tramming worst point: MAX (cao nhat)`).
- Mặc định là **MAX** (góc cao nhất) — giống hành vi cũ (góc lệch nhiều nhất khi bàn nghiêng).
- **Mốc vật lý là KHUNG Z của lần home cuối** (`Z-home` = tâm bàn tại thời điểm đó). Mốc này **không đổi khi vặn ốc** — chỉ `G28` (**tự động khi đã hết góc CHƯA chấp nhận**, hoặc vào lại menu) hay `G92` mới đổi. Vì vậy:
  - giá trị probe trả về = **chiều cao của điểm đó so với `Z-home`** (đúng khung Z hiện tại); số hiện trên màn hình = `delta` so với **mốc đã chốt** (`Z-MIN` hoặc `Z-MAX`)
  - giữa các lần `PROBE`, **tự động đi tiếp** và `DONE` (khi còn góc chưa chấp nhận) **không home lại** ⇒ mọi giá trị nằm trong **cùng một khung Z** ⇒ so sánh trực tiếp được với nhau: **vặn đúng chiều thì delta giảm, vặn sai thì delta tăng**
- 🔴 **KHÔNG probe lại tâm bàn** (theo yêu cầu) — cả `G35` lẫn wizard. Vặn ốc làm tâm bàn *vật lý* dịch, nhưng **mốc thì không** — probe tâm chỉ tốn thời gian, cộng sai số vào cả 4 số và gây hiểu nhầm là mốc bị đổi. Mốc mới chỉ được thiết lập khi **đã hết góc chưa chấp nhận** (máy **tự** `G28 Z` → `z_ok[]` + `z_accepted[]` xoá hết → 4 góc được đo lại rồi **chốt mốc mới**, hàm `tramming_set_ref()`).
- `PROBE … con x.xxx` sau mỗi lần bấm = **số mm còn lại** của góc đang đứng (độ lệch so với **mốc `Z-MIN` /
  `Z-MAX`**, 3 số lẻ, cùng dấu với 4 số ở 2 dòng trên) — chỉ lấy **`z` cuối**, **không cộng dồn**.
  `DAT` = đã trong ngưỡng nhiễu (`|delta| ≤ 0.01mm`, hằng số `TRAM_DONE_MM`).
- **Chiều vặn (CW/CCW) lấy từ đâu:** hàm `tramming_turn_cw()` trong `src/feature/tramming.h`, theo đúng
  quy luật **đo lại trên máy (2026-10-12)**: **CCW = hạ góc đó xuống**, **CW = nâng góc đó lên** (xem §5).
  Trên **màn hình wizard** chỉ có `CW`/`CCW` + **số mm**, nhưng **`G35` in thêm 1 dòng giải thích**:
  `Chieu van: CCW = HA goc do xuong, CW = NANG goc do len.` (`G35.cpp:194`) — vì khi vặn ốc thì chiều vặn là
  thứ cần hình dung.
- **Ba chuyển động Z quanh một lần probe** — **không cái nào là `probe.offset.z`**:
  1. *(nhấc rất nhỏ, trước khi probe)* firmware đưa nozzle về đúng `Z_CLEARANCE_BETWEEN_PROBES` = 5mm. Lần probe trước kết thúc ở `trigger + 5mm`; nếu bàn ở góc đó cao/thấp hơn 0 một chút thì bước này chỉ nhích vài phần mười mm — đây chính là cú "nhấc lên một đoạn rất nhỏ" thấy trước khi probe.
  2. *(5mm)* `MULTIPLE_PROBING 2`: giữa lần chạm **nhanh** và lần chạm **chậm**, firmware nhấc lên `Z_CLEARANCE_MULTI_PROBE` = 5mm (`probe.cpp:737`).
  3. *(5mm)* sau khi probe xong: nhấc `Z_CLEARANCE_BETWEEN_PROBES` = 5mm (`PROBE_PT_RAISE`, `probe.cpp:898`) — **giữ nguyên**.
- `probe.offset.z` (`M851 Z0.70`) **không tạo ra chuyển động nào**: nó chỉ vào phép tính (`measured_z = run_z_probe() + offset.z`, `probe.cpp:891`) và vào giới hạn `z_probe_low_point = -offset.z + Z_PROBE_LOW_POINT` (`probe.cpp:721`).
- `Z_HOMING_HEIGHT` (`Configuration.h:1754`) đang **tắt**, nhưng `G28` vẫn nâng Z: cuối quá trình home Z, nozzle được đưa lên `Z_AFTER_PROBING` = 10mm (quan sát `M114` sau `G28`: `Z:10.00`).
- Nhãn 2 ký tự (`L/R` theo X, `F/B` theo Y) **suy ra từ toạ độ**, không hard-code thứ tự điểm.
- **Nút thoát là `BACK`** (mục thứ 3 trên dòng 3, giữa `MAX->MIN` và `DONE`): bấm là gọi `tramming_exit()` —
  stow probe + `set_axis_never_homed(Z_AXIS)` rồi quay về **màn hình chính**. `DONE` **không** phải nút thoát:
  nó là **"góc này XONG"** — **chấp nhận góc đang đứng** (coi như đạt dù chưa vào ngưỡng `0.01mm`) rồi đi tiếp, hoặc (khi hết góc chưa chấp nhận) làm lại từ đầu; **vẫn là nút bấm tay** (máy đã **tự đi tiếp** ngay khi góc vừa probe `DAT`).
  Khi **cả 4 góc đều `DAT`** (`DAT - lech so voi tam`) thì dòng 3 **chỉ còn `>BACK`** — encoder khoá vào `BACK` — nên đây là
  lúc `BACK` là lựa chọn **duy nhất**. Cứ để nguyên màn hình khi không dùng nữa cũng được — lần sau vào lại
  menu wizard sẽ `G28` cả 3 trục và đo lại từ đầu. Vì vít đã bị vặn nên **vẫn phải `G28` trước khi in**.
- Nozzle đứng ở góc cần vặn tại `Z_AFTER_PROBING` = 10mm.

#### Điều chỉnh nhiều góc — số của các góc khác có bị đổi không?

> 🟢 **Từ 2026-10-12: KHÔNG đổi.** Mốc = **`Z-MIN` (chế độ MAX) hoặc `Z-MAX` (chế độ MIN)** của các góc,
> được **CHỐT (đóng băng)** ngay sau lần đo **đủ 4 góc đầu tiên** (lúc vào menu, hoặc khi **hết góc chưa chấp nhận** —
> máy **tự** home lại + đo lại 4 góc) và **không tính lại nữa**. Nên **`PROBE` lại 1 góc CHỈ làm đổi số của chính góc đó**; 3 góc
> kia giữ nguyên. Đổi `MAX <-> MIN` chỉ là **đổi lấy mốc kia** (cả hai mốc đều đã chốt), không đo lại gì.
>
> **Trước đây đổi là vì** mốc cũ = "trung bình các góc đã đo" **tính lại mỗi lần vẽ màn hình**: probe lại
> góc `i` thấy cao/thấp hơn `Δ` ⇒ trung bình đổi `Δ/4` ⇒ **3 số kia mỗi số dịch `−Δ/4`** (ngược dấu, bằng
> 1/4) dù chưa hề vặn gì. Cộng thêm độ lặp của probe (σ ≈ 2 µm; 20 lần đo chênh 7–10 µm) nên `Δ` luôn
> khác 0 ⇒ các số kia cứ "nhảy" vài phần nghìn mm mỗi lần bấm `PROBE`.

**Cách hiểu đúng bây giờ:**

| Điều | Ý nghĩa |
|---|---|
| 4 số ở 2 dòng đầu | "**góc này còn cao/thấp hơn MỐC bao nhiêu mm**" — đích là đưa **từng số về 0**. **Riêng khi cả 4 góc `DAT`:** mốc là **TÂM BÀN** ⇒ 4 số là **delta-Z so với tâm** (**dương = góc cao hơn tâm**, âm = thấp hơn tâm) |
| `PROBE` | **chỉ cập nhật số của góc đang đứng** = còn phải vặn bao nhiêu mm nữa để góc đó về mốc |
| 3 góc kia | số **không tự đổi** — nhưng **thực tế đã dịch** khi bạn vặn (bàn là một tấm liền, 4 vít liên kết với nhau). Muốn số mới thì `PROBE` lại, hoặc để máy **tự đi tiếp** khi góc vừa đo `DAT` / bấm `DONE` để **chấp nhận góc đang đứng** rồi tới góc chưa chấp nhận kế tiếp (máy probe luôn) |
| Hết góc chưa chấp nhận (**tự động**, hoặc do bấm `DONE` chấp nhận góc cuối) | đo lại **cả 4 góc** trong khung Z mới (**`z_accepted[]` xoá sạch**) ⇒ **chốt MỐC MỚI** — cách "làm lại từ đầu" |

> ⚠️ **Hệ quả phải nhớ:** vì mốc không đổi, sau khi vặn một góc thì 3 số còn lại là **số CŨ** (không còn
> đúng vật lý) — đừng cộng dồn; hãy `PROBE` lại góc cần kiểm tra, hoặc cứ để máy chạy hết vòng (**tự đi tiếp**
> khi `DAT`, bấm `DONE` để **chấp nhận** góc vặn mãi không vào ngưỡng) cho tới khi **hết góc chưa
> chấp nhận** để máy **tự** đo lại tất cả.
> Đổi lại: bạn luôn biết **chính xác từng góc cần lên/xuống bao nhiêu mm** so với mốc ban đầu, và số của
> một góc **không nhảy** chỉ vì bạn vừa probe một góc khác.
>
> ℹ️ **`G35` không bị ảnh hưởng:** nó đo cả 4 góc trong **một lần chạy** rồi in báo cáo ngay ⇒ mốc của
> nó đã đúng nghĩa "một lần đo đủ 4 góc".

**Màn hình không hiện ký hiệu gì thêm** (chỉ `*` = góc đang đứng và `---` = góc chưa có số trong khung Z này) — số nào cũng dùng được để định hướng. **Góc đã bấm `DONE` (đã chấp nhận) cũng không có ký hiệu riêng** — nó **vẫn hiện số delta** như mọi góc; chỉ khác là máy **không tự chạy tới đó nữa**. Quy tắc duy nhất cần nhớ:

> ✅ **KHÔNG cần bấm `DONE` liên tục để đo lại tất cả.** Vì:
> - **Đứng ở góc nào thì tin số của góc đó**: dấu `*` = góc đang đứng, và nó **luôn là số vừa đo**
>   (`PROBE`, `MAX->MIN`, `DONE` — và cả bước **tự động đi tiếp** — đều probe luôn góc mới) ⇒ dòng `PROBE` (số mm còn lại + chiều vặn) đúng cho góc đó.
> - **Máy TỰ đi tới góc CẦN CHỈNH kế tiếp ngay khi góc vừa probe `DAT`** (`|delta| ≤ 0.01mm`, màn hình hiện `Auto next`) — **bỏ qua góc MỐC, các góc đã đạt VÀ các góc đã bấm `DONE` (đã chấp nhận)** rồi **đo luôn góc đó** ⇒ sang góc khác vặn tiếp ngay, **không cần bấm gì**. Bấm **`DONE`** = **chấp nhận góc đang đứng là xong** (dù chưa vào ngưỡng) rồi đi tiếp — dùng khi **vặn mãi không vào ngưỡng**.
> - Các góc chưa đo lại vẫn **dùng số cũ** (máy không xoá) — đủ để biết góc nào đang cao/thấp mà điều chỉnh tiếp.
> - Máy chỉ **home lại Z + đo lại cả 4 góc** khi **đã hết góc CHƯA chấp nhận** (tự động, hoặc sau khi bấm `DONE` chấp nhận góc cuối): đúng lúc cần xem `max−min` còn bao nhiêu rồi quyết định vòng tiếp theo.
>
> 🔁 **Vòng làm việc:** vặn 1 góc → `PROBE` (xem số giảm; nếu hiện `DAT` thì máy **tự** sang góc kế tiếp, không phải bấm gì)
> → vặn → … hết 4 góc → máy **tự** `G28 Z` + đo lại cả 4 góc (**mốc mới**) → xem `max−min` → lặp lại. Thường **2–3 vòng** là xuống dưới
> ngưỡng nhiễu (0.02mm). Góc nào hiện `DAT` (hoặc là góc **mốc**, hoặc đã bấm **`DONE`** = **đã chấp nhận**) thì máy **tự bỏ qua**, **đừng** vặn thêm góc đó.
> Nếu **cả 4 góc đều `DAT`** thì dừng vòng lặp: **4 số ở 2 dòng đầu đổi sang delta-Z so với TÂM BÀN** (**dương = góc cao hơn tâm**, âm = thấp hơn tâm), dòng 2 hiện **`DAT - lech so voi tam`**, dòng 3 **chỉ còn `>BACK`**.

#### Độ tin cậy của thuật toán (đã kiểm lại)

| Điểm | Kết luận |
|---|---|
| **Vì sao không còn dùng trung bình làm mốc?** | Mốc cũ = **mặt phẳng trung bình** (LS qua trọng tâm) cho **tổng 4 delta = 0** và `\|delta\|` lớn nhất là nhỏ nhất có thể — nhưng nó **tính lại mỗi lần vẽ màn hình**, nên `PROBE` lại một góc làm **cả 3 số kia dịch** `−Δ/4`. Mốc mới = **`Z-MIN` (chế độ MAX) hoặc `Z-MAX` (chế độ MIN)** — một **cực trị** của chính 4 góc, **chốt một lần** sau lần đo đủ 4 góc đầu tiên ⇒ số của một góc **không nhảy** khi probe góc khác, và chiến lược chỉ còn **một chiều** (chỉ hạ hoặc chỉ nâng). |
| **Phụ thuộc gì không?** | **Không phụ thuộc** `z_home`, `M851` (`probe.offset.z`), lần home nào, hay việc có `G92` — vì mọi giá trị đều bị trừ đi cùng một mốc. ⇒ **Số liệu vẫn đúng kể cả khi Z offset chưa cân chuẩn.** (Đây là lý do thật sự nên dùng `delta` so với mốc thay vì chiều cao tuyệt đối.) |
| **Probe lỗi 1 góc?** | `G35` **không bỏ cả phép đo**: nó đo tiếp các góc còn lại, in rõ góc lỗi, và chốt mốc trên các góc **đo được** (cần ≥ 3). Trước đây chỉ cần 1 góc lỗi là mất toàn bộ kết quả. |
| **Độ phân giải / nhiễu** | In **3 số lẻ (0.001mm)**. Nhiễu thật của probe ~**±0.005–0.01mm** (`MULTIPLE_PROBING 2` — trung bình có trọng số của pha nhanh + pha chậm) ⇒ **\|delta\| < ~0.01mm nằm trong nhiễu, đừng đuổi theo**; `max-min ≤ 0.02–0.03mm` coi như đạt. Thuật toán in `DAT` (auto đạt) khi `\|delta\| ≤ 0.01mm` (`TRAM_DONE_MM`) — trước đây là 0.002mm, ngưỡng cũ **hầu như không bao giờ đạt** nên vòng lặp cứ xoay mãi và **không bao giờ** chạy tới bước home + probe lại 4 góc. Với ngưỡng 0.01mm, góc vừa đo `DAT` thì máy **tự đi tiếp**; góc nào **vặn mãi không vào ngưỡng** thì bấm **`DONE`** để **chấp nhận nó là xong** (`z_accepted[]`) — máy **không quay lại** góc đó nữa (trước đây `DONE` chỉ nhảy tới góc còn quá ngưỡng nên **quay vòng mãi**); và nếu **cả 4 góc đều `DAT`** thì dừng hẳn — **4 số ở 2 dòng đầu là delta-Z so với TÂM BÀN** (dương = góc cao hơn tâm), dòng 2 hiện `DAT - lech so voi tam`, dòng 3 **chỉ còn `BACK`**. |
| **Bàn không phẳng (võng/vênh)** | 4 điểm góc chỉ mô tả được **một mặt phẳng**; phần võng/vênh ở giữa bàn **không** sửa được bằng 4 vít — đó là việc của mesh (`G29`). `G35` cũng chỉ đưa 4 góc về cùng một mặt phẳng. |
| **Có bị "ảo" khi cả bàn cao/thấp đều?** | Không: đó chính là ca `2.000/2.000/2.000/2.030` — delta chỉ ra **0.030mm**, không đòi vặn cả 4 vít như cách tính theo Z-home. |
| **Nút `WORST` (MAX/MIN) có bị lệch theo?** | Không: trừ đi một hằng số **không đổi thứ tự** ⇒ chọn góc cao nhất/thấp nhất cho kết quả y hệt dù xếp theo số thô hay theo delta. |

#### Vặn 1 lần không tới đích — bấm `PROBE` nhiều lần

Ca dùng thật: vặn một nấc **không đạt** ngay giá trị cần, nên vặn → `PROBE` → vặn → `PROBE`… Màn hình
được thiết kế cho đúng vòng lặp này, và **không cần cộng dồn gì** — chỉ lấy **`z` cuối** sau mỗi lần probe:

| Hiển thị | Nghĩa |
|---|---|
| `CCW con +0.013` (dòng `PROBE` sau khi bấm) | **Số mm còn lại** của góc đang đứng: còn lệch `+0.013mm` so với **MỐC**, phải vặn tiếp **CCW** |
| `+0.013` (số ở dòng có dấu `*`) | Cùng con số đó, nằm trong bảng 4 góc (3 số lẻ) — cập nhật ngay sau mỗi lần probe |
| `DAT` | `\|delta\| ≤ 0.01mm` (`TRAM_DONE_MM`) — đã đạt, trong ngưỡng nhiễu; máy **tự bỏ qua** góc này và **tự đi tiếp**. Góc **vặn mãi không vào ngưỡng** thì bấm **`DONE`** để **chấp nhận** — máy cũng **bỏ qua** góc đó ở mọi lần quét vòng sau |
| `DAT - lech so voi tam` (dòng 2) | **cả 4 góc** đều `\|delta\| ≤ 0.01mm` ngay sau lần đo đủ 4 góc (`tram_all_dat = true`) ⇒ **4 số ở 2 dòng đầu là delta-Z so với TÂM BÀN** (`base_home_pos(Z_AXIS)`; **dương = góc CAO HƠN tâm**, âm = thấp hơn tâm), dòng 3 **chỉ còn `>BACK`**; encoder bị **khoá vào `BACK`** |

- **Tự kiểm tra chiều vặn:** vặn **đúng** chiều → số còn lại **giảm**; vặn **sai** chiều → số còn lại **tăng**.
  Không cần biết "tổng đã vặn bao nhiêu", chỉ cần nhìn số đó đi về 0.
- **Vì sao so sánh được:** giữa các lần `PROBE` **không home lại** ⇒ mọi giá trị nằm trong **cùng một
  khung Z** ⇒ so sánh trực tiếp, và **không** phụ thuộc `M851`/Z-home.
- Bấm `PROBE` liên tục mà **không vặn gì**: số đứng yên ⇒ nhìn là biết không nhích. Chênh lệch giữa 2
  lần bấm khi đó chính là **độ lặp lại của probe** (±0.005–0.01mm).
- **Quy tắc vặn cho nhanh tới đích:** cứ vặn **đúng bằng số đang hiện** — vì **mốc đã đóng băng**, vặn
  một đoạn `x` mm thì số của **chính góc đó giảm đúng `x` mm**. Thực tế nên vặn ~80–90 % con số rồi `PROBE`
  lại: bàn là **một tấm liền** nên vặn một góc sẽ kéo 3 góc kia nhích theo một chút.
- **Số của 3 góc kia là số CŨ** khi bạn chỉ `PROBE` 1 góc (bàn thật đã nghiêng đi một chút khi vặn) ⇒
  chúng chỉ để tham khảo; **máy tự sang góc cần chỉnh kế tiếp khi góc vừa đo `DAT`** (hoặc bấm `DONE` để
  **chấp nhận góc đang đứng là xong** — máy probe luôn góc kế tiếp), hoặc cứ để máy chạy tới khi **hết góc chưa chấp nhận** nếu muốn cả 4 số cùng "mới" trong một khung Z mới.

| Tình huống khác | Kết luận |
|---|---|
| Chạy `G35` nhiều lần liên tiếp | **Đúng và độc lập.** Mỗi lần `G35` đo mới cả 4 điểm rồi lấy **mốc mới** (`Z-MIN` ở chế độ MAX, `Z-MAX` ở chế độ MIN); delta không phụ thuộc Z-home nên giữa 2 lần có home lại Z vẫn so sánh được. |
| Probe lỗi giữa chừng | Góc lỗi **không** được đưa vào mốc và được in rõ là lỗi; cần ≥ 3 góc đo được, còn lại vẫn ra kết quả. |
| Muốn **giảm nhiễu** cho một góc | Máy chỉ giữ **giá trị mới nhất** của mỗi góc (không tự lấy trung bình) — đo lặp rồi tự chia, hoặc tin vào `max-min`: ≤ 0.02–0.03mm là đạt, đừng vặn theo các delta < 0.01mm. |

#### Chọn điểm `MIN` thì chiều vặn có ngược với `MAX` không?

**Không ngược.** Nút `WORST` **chỉ đổi góc mà đầu in chạy tới VÀ đổi mốc so sánh**, không đụng tới phép
tính: `delta = chiều_cao − mốc`, với **`mốc = Z-MIN` ở chế độ MAX** và **`mốc = Z-MAX` ở chế độ MIN**;
chiều vặn luôn theo **cùng một quy ước** ở cả 2 chế độ — `delta > 0` = góc **cao hơn** mốc ⇒ **vặn CCW**
(hạ xuống); `delta < 0` ⇒ **vặn CW** (nâng lên).

Vì mốc là một **cực trị** của chính 4 góc nên:

- `MAX` (mốc = `Z-MIN`) → **mọi** `delta ≥ 0`, hướng dẫn luôn là **vặn CCW** (hạ các góc cao xuống).
- `MIN` (mốc = `Z-MAX`) → **mọi** `delta ≤ 0`, hướng dẫn luôn là **vặn CW** (nâng các góc thấp lên).

Nói cách khác mỗi chế độ là **một chiến lược một chiều**: MAX = "hạ tất cả về mức thấp nhất", MIN = "nâng
tất cả lên mức cao nhất". `CW/CCW` vẫn luôn nói về **chiều vặn ốc**, không phụ thuộc chế độ.

Nhìn thì thấy "ngược" (MAX → CCW, MIN → CW) nhưng thực ra bạn chỉ đang đứng ở **góc đối diện**;
quy ước **không hề đổi**. `CW/CCW` luôn nói về **chiều vặn ốc**, không phụ thuộc chế độ MAX/MIN.

> 🔧 **Hai điều thực dụng khi dùng `MIN` (vít góc cao đã hết hành trình):**
> - Chiến lược "chỉ nâng các góc thấp" hội tụ về **mặt phẳng của góc cao nhất** (không phải về trung
>   bình): nâng mỗi góc thấp lên đúng số mm đang hiện là chúng về cùng mặt phẳng mốc — **một vòng là gần
>   xong** (mốc đóng băng nên không còn kiểu co 25 % mỗi vòng). Cứ để máy **tự** chạy tới khi **hết góc chưa chấp nhận**
>   (máy home lại Z + đo lại cả 4 góc, **mốc mới**) rồi làm theo số mới; `DONE` chỉ cần khi muốn **chấp nhận** một góc vặn mãi không vào ngưỡng.
> - Nâng/hạ **cả mặt phẳng** không làm sai `Z offset`: mỗi lần `G28` máy probe lại tâm bàn nên mốc Z
>   đi theo bàn. Chỉ cần để ý **hành trình Z** còn đủ.

> 📌 **`WORST` lưu ở đâu (không xoá EEPROM):** cờ này nằm trong **ô `z_offset` của khối MBL** — mà
> `MESH_BED_LEVELING` đang **TẮT** nên ô đó vốn bỏ trống (`settings.cpp:875-895` lúc lưu,
> `settings.cpp:1832-1839` lúc đọc). Nhờ vậy **kích thước `SettingsData` không đổi** →
> `EEPROM_VERSION` và CRC **không đổi** → **dữ liệu EEPROM đang có (`M851 Z0.70`, mesh UBL,
> `M203/M201/M906`…) KHÔNG bị xoá** khi nạp bản này. Ô đó ghi `0.0f` = MAX, `1.0f` = MIN (giá trị cũ
> trong EEPROM là 0 ⇒ MAX, đúng bằng mặc định).
> ⚠️ Vì là mã nguồn Marlin bị sửa, nếu sau này ai bật `MESH_BED_LEVELING` thì cờ này **không** được lưu
> nữa (code tự bỏ qua) — không hỏng gì, chỉ mất tính "nhớ".

> ⚠️ Trong lúc wizard probe/đi, màn hình đứng ở dòng tiến độ — bình thường (mỗi probe ~10s);
> khi máy **tự đi tiếp** sau một góc vừa `DAT` thì dòng đó hiện `Auto next` (trạng thái `TR_AUTO`, mỗi khung hình 1 bước).
> Máy đo `G35` lần đầu sau khi viết lại: 4 góc lệch **≤ 0,05mm** so với tâm.

### 11.9 Chiều motor X/Y trên CoreXY — cách chẩn đoán

Máy chạy CoreXY (`COREXY`, `Configuration.h:873`) nên X và Y là **hai motor A/B dùng chung một
chuyển động**, không phải mỗi trục một motor. Điều này làm việc chẩn đoán "trục chạy sai" khác hẳn
máy Cartesian.

**Ánh xạ trong firmware** (`planner.cpp:2092-2093`, `stepper.cpp:603-604`):

```c
steps_dist_mm.a = (da + db) * mm_per_step[A_AXIS];   // motor A  <-  X + Y
steps_dist_mm.b = CORESIGN(da - db) * ...;            // motor B  <-  X - Y
SET_STEP_DIR(X); // A   ->  INVERT_X_DIR  dao motor A
SET_STEP_DIR(Y); // B   ->  INVERT_Y_DIR  dao motor B
```

Gọi `σA`, `σB` là dấu hiệu dụng của hai motor (gộp cả `INVERT_*_DIR` lẫn cực dây motor):

```
p_x = x·(σA + σB) + y·(σA − σB)
p_y = x·(σA − σB) + y·(σA + σB)
```

| | Kết luận |
|---|---|
| Máy chạy đúng | **`σA = σB`** |
| `σA = −σB` | `p_x = −2y`, `p_y = −2x` → **lệnh Y làm đầu in chạy theo X** (và ngược lại). `G28 Y` không bao giờ chạm công tắc Y → hết thời gian → `kill()` → LCD `Printer halted. kill() called!` |

> 🔵 **Vì `INVERT_X_DIR` và `INVERT_Y_DIR` phải BẰNG NHAU** (khi hai motor đấu và lắp giống nhau):
> hai giá trị này chỉ là dấu của `σA`, `σB`. Nếu chúng khác nhau thì `σA = −σB` → lỗi trộn trục ở
> trên. Đảo **cả hai** cùng lúc chỉ là **lật gương toàn cục** (X và Y cùng đổi chiều), **không** sửa
> được lỗi trộn trục — đây là chỗ rất dễ sửa nhầm.

**Ba bước tách nguyên nhân** (hai bước đầu **không cần cấp điện**, motor tắt là đẩy tay được):

| Bước | Làm gì | Kết quả |
|---|---|---|
| **A** | Quay **một** motor X/Y bằng tay vài răng, xem đầu in đi đâu | Đi **chéo** → đúng là CoreXY · Đi **thẳng 1 trục** → máy là Cartesian, `COREXY` **sai** |
| **B** | Đẩy đầu in bằng tay **+Y** và xem hai pully | Quay **ngược chiều** và đầu in đi **thẳng** → đường belt đúng · Quay **cùng chiều** → **belt lắp sai đường** |
| **C** | Đẩy đầu in bằng tay tới sát công tắc Y, đọc `M119` | `y_min: TRIGGERED` → công tắc tốt · vẫn `open` → **công tắc/đứt dây** là nguyên nhân, không liên quan CoreXY |
| **D** | **Phép thử tách motor** (bảng dưới) | Khoanh vùng được **từng motor/pully** — đây là bước tìm ra lỗi pully tuột |

**Bước D — phép thử tách từng motor.** Trong CoreXY, chọn toạ độ sao cho chỉ một motor phải chạy:

```
G1 X+n Y+n     →  a = 2n, b = 0   →  CHỈ motor A chạy
G1 X+n Y−n     →  a = 0,  b = 2n  →  CHỈ motor B chạy
```

Hai lệnh này phải cho **hai đường chéo 45° VUÔNG GÓC và DÀI BẰNG NHAU**. Đây là phép thử **rẻ nhất và
khoanh vùng giỏi nhất** — nó tách được lỗi ở motor/pully khỏi lỗi ở firmware/`INVERT` mà không cần
đụng dây hay flash gì:

| Hiện tượng | Kết luận |
|---|---|
| Hai đường chéo **vuông góc, bằng nhau** | Hai motor + hai belt đều tốt → tìm nguyên nhân chỗ khác |
| Một lệnh **không nhích gì** | Motor/driver/giắc của motor đó có vấn đề — hoặc **pully tuột hẳn** |
| Hai đường chéo **không đều nhau** | Một bên **yếu hoặc trượt** (pully tuột một phần) |
| Lệnh Y ra **đường chéo** nhưng lệnh X ra **thẳng** | 🔴 **Dấu hiệu đặc trưng của pully tuột** — xem cạm bẫy 25 |

> 🔴 **Pully tuột là nghi phạm số 1 khi "máy tự nhiên hỏng" mà code không đổi.** Pulley trượt trên
> trục motor → chỉ còn một belt được kéo → đầu in đi chéo; sau đó tuột hẳn thì **không nhích gì**.
> Vì code và cấu hình **không hề đổi**, rất dễ đi sai đường hàng giờ vào firmware/`INVERT`. **Luôn
> kiểm vít hãm pully trước khi nghi firmware.** Cách kiểm: vẽ một vạch bút dạ **bắc qua pully và
> trục motor**, đẩy đầu in qua lại vài vòng — vạch lệch là tuột.

> ⚠️ **Nếu `M122` đọc ra thanh ghi RỖNG thì DỪNG chẩn đoán firmware.** Dấu hiệu: `off time 0`,
> `msteps 256`, `stealthChop false`, `uStep count 0`, và **mọi endstop `TRIGGERED`** — đó là TMC2209
> **mất nguồn VM**, không phải `mstep_reg_select` bị xoá. Xem cạm bẫy 11 và 14c.

> ⚠️ **Tuyệt đối KHÔNG chạy `G28` hay bất kỳ lệnh chuyển động nào để "thử" khi chưa xác nhận driver
> đang ở đúng microstep.** `M92` khớp sai với microstep thật (ví dụ `M92 X80` trong khi driver ở 1/8)
> làm trục chạy **gấp đôi quãng đường và gấp đôi tốc độ** — home kiểu đó là một cú va mạnh. Thử bằng
> **jog 3–5 mm giữa bàn**, không phải `G28`.

Hai cách sửa `INVERT`, **khác nhau ở chỗ có đụng firmware hay không** (chỉ dùng khi bước A–D đã loại
trừ cơ khí):

| Cách | Việc | Khi nào dùng |
|---|---|---|
| **1. Đảo dây** | Đảo thứ tự hai dây của **một** cuộn ở **một** motor (hoặc xoay giắc 180° nếu giắc cho phép) → `σ` của motor đó đổi dấu, khôi phục `σA = σB`. **Giữ nguyên** `INVERT_X_DIR = INVERT_Y_DIR = true` | Khi hai giắc có **thứ tự dây khác nhau** (một giắc bị đảo) |
| **2. Sửa firmware** | Đổi **một** trong hai `INVERT` cho khác nhau (`INVERT_Y_DIR` → `false`), clean rebuild + flash | Khi hai giắc **giống hệt nhau** nhưng hai motor **lắp đối xứng** |

> ⚠️ **Cách 2 có hai lựa chọn là ảnh gương của nhau** (đảo `INVERT_X_DIR` hoặc đảo `INVERT_Y_DIR`).
> Cả hai đều sửa được lỗi trộn trục, nhưng **chỉ một** cho X/Y chạy đúng **chiều**; cái còn lại làm
> cả X lẫn Y chạy **ngược hướng**. Vì vậy sau khi flash phải thử bằng một cú jog **nhỏ (3–5 mm)**
> với tay đặt gần công tắc nguồn — đừng chạy `G28` để "thử".

> 🔴 **`M114` KHÔNG dùng được để chẩn đoán chiều motor.** Với máy core, `stepper.cpp:3075` lưu
> **toạ độ Cartesian** vào `count_position` (`count_position.set(spos.a + spos.b, CORESIGN(spos.a - spos.b), ...)`),
> nên dòng `Count A:/B:` của `M114` chỉ là `X×steps_per_mm` và `Y×steps_per_mm` — **giống hệt nhau
> dù firmware trộn trục hay không**. Nó luôn khớp với con số firmware tự tính, không phản ánh motor
> quay thế nào. Muốn biết chiều thật thì phải **nhìn máy chạy** (hoặc quay tay).

---

## 12. Màn hình riêng `lcd_panel` — MCU tự render, Marlin chỉ gửi dữ liệu

Thay vì để Marlin vẽ màn hình 12864, dự án có thêm một **firmware màn hình độc lập** trong thư mục
`lcd_panel/`: một MCU riêng (**STM32F411CCU6** hoặc **STM32F103C6T6**) cắm màn **2.4" SPI ILI9341
240×320**, tự vẽ nhiệt độ / vị trí / tiến độ in, và **hỏi Marlin bằng G-code chuẩn**
(`M105`, `M114`, `M27`) qua **1 UART** ⇒ **Marlin không phải sửa code, không phải render**.

**Có 2 kiểu, xem §15 để biết kiểu mới (Marlin đẩy dữ liệu + LVGL):**

| | |
|---|---|
| Firmware panel | `lcd_panel/` — **project PlatformIO riêng** (`lcd_panel/platformio.ini`), không phải env của Marlin |
| Build (kiểu G-code) | `pio run -d lcd_panel -e panel_f411cc` (F411CCU6) · `pio run -d lcd_panel -e panel_f103c6` (F103C6T6) |
| Build (kiểu PUSH + LVGL) | `pio run -d lcd_panel -e panel_f407vgz_lvgl` · `panel_f411cc_lvgl` · `panel_esp32_cyd` |
| Kích thước thực tế (G-code UI) | F411: **20,7 KB** flash / 1,9 KB RAM · F103C6T6: **21,1 / 32 KB (64%)** flash, 1,8 / 10 KB RAM |
| Thư viện ngoài | Kiểu G-code: **không có** (driver ILI9341 + font 8×8 tự viết). Kiểu LVGL: `lvgl@^8.3.11` (+ `TFT_eSPI` cho ESP32-CYD) |
| Marlin phải làm | kiểu G-code: bật 1 UART (`SERIAL_PORT_2 3` + `BAUDRATE_2 250000`). Kiểu PUSH: bật `PANEL_SERIAL_UI` — **1 lệnh** `pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode on` |
| Dây (Monster8 → panel) | EXP1 chân 5 (PD9 = USART3-**RX**) ← TX panel · chân 6 (PD8 = USART3-**TX**) → RX panel · chân 9 GND chung · chân 10 = 5V nếu board panel có regulator |

> 🔎 **MKS làm thế nào?** Trong `Marlin/src/lcd/extui/` có **2 kiểu**, đừng lẫn:
> * `extui/mks_ui` (MKS Robin), `lcd/tft`, `e2v2`… = màn gắn thẳng vào mainboard, **Marlin tự vẽ**
>   (LVGL/DWIN biên dịch vào firmware) — đúng thứ kiến trúc này **tránh**.
> * `extui/nextion`, `extui/dgus` (kể cả `dgus/mks`) = **màn có MCU riêng**, Marlin chỉ gửi dữ liệu.
>   `lcd_panel` đi theo kiểu này, nhưng ở mức **"TFT panel"** giống **MKS TFT28/32/35**: panel tự
>   poll bằng G-code nên **không cần thêm `ExtUI`** nào trong Marlin.

> ⚠️ **USART2 (PA2/PA3) của Monster8 đã dùng cho `HEATER_2`/`FAN0`** → dùng **USART3**, nhưng nhớ
> đúng chiều chân: **PD8 = USART3_TX** (EXP1 chân 6), **PD9 = USART3_RX** (EXP1 chân 5) → dây **chéo**.
> Cờ build đúng: `-DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8`.
> ⚠️ **PD8 còn là `NEOPIXEL_PIN` = `EXP1_06_PIN`** (đèn nền RGB của màn 12864 V3, `NEOPIXEL_LED` đang
> bật) và PD9 là `LCD_PINS_D4` của màn 12864 song song → phải **tắt cả `MKS_MINI_12864_V3` và
> `NEOPIXEL_LED`** (và với mọi cổng khác `Serial` thì cờ `-DPIN_SERIALx_TX/RX` là **bắt buộc**,
> nếu thiếu core không gán chân → `begin()` thất bại). Không muốn đụng vào LCD thì dùng
> **USART1 = header WiFi** (PA9=TX/PA10=RX): `SERIAL_PORT_2 1` + `-DENABLE_HWSERIAL1
> -DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9` — `MKS_WIFI_MODULE` đang tắt.
> Chi tiết đầy đủ (nối dây, xử lý sự cố, giao thức, cách đổi font): **`lcd_panel/README.md`**.

---

## 13. Encoder: đọc bằng NGẮT + hàng đợi (`ENCODER_QUAD_ISR`)

Đã thay cách đọc encoder của cả Marlin và panel bằng thuật toán lấy ý từ `D:\summer-fan`.

| | Marlin gốc | summer-fan (`encode2.cpp`) | **Dùng bây giờ** |
|---|---|---|---|
| Đọc chân A/B | **polling** trong `MarlinUI::update_buttons()` | **ngắt đổi chân** + `std::queue` | **ngắt đổi chân** + **ring cố định** |
| Mất cạnh khi UI bận? | **Có** (đọc SD, ghi EEPROM, vẽ màn hình, delay khi probe…) → phải bù bằng `ENCODER_RATE_MULTIPLIER` (**đoán** số bước mất theo tốc độ) | Không | **Không** |
| Chống dội / nhiễu | Bảng chuyển tiếp **đã** bỏ qua bước nhảy 2 bit ✓ | Máy trạng thái full-cycle, rất chống dội ✓ | Bảng chuyển tiếp y hệt Marlin (nhảy 2 bit = 0) ✓ |
| Đơn vị | 1 xung = 1 cạnh, chia `ENCODER_PULSES_PER_STEP` | 1 sự kiện = **cả chu kỳ 4 cạnh** (thô hơn) | **Y hệt Marlin** → mọi logic menu không đổi |
| Cấp phát trong ngắt | — | **`std::queue` + `new` trong ISR** ✗ (phân mảnh, không xác định) | **Không** (mảng `volatile uint8_t[32]`) ✓ |
| Chiều quay | — | — | **Giống hệt Marlin cũ** (bảng cùng cực tính) → không bị đảo chiều |

**Kết luận:** summer-fan **không** "tốt hơn toàn diện" — bảng chuyển tiếp của Marlin vốn đã bỏ qua
bước nhảy 2 bit, và cách của summer-fan thô hơn (1 sự kiện/chu kỳ) lại còn cấp phát heap trong
ngắt. **Điểm hay thật sự** của nó là **ngắt + hàng đợi ⇒ không mất cạnh**, đúng chỗ Marlin yếu
(vì thế Marlin phải có `ENCODER_RATE_MULTIPLIER` để *đoán* bù). Nên đã lấy đúng ý đó, làm lại cho
an toàn: **ring cố định trong ngắt + đúng bảng/đúng đơn vị của Marlin**.

* Marlin: option **`ENCODER_QUAD_ISR`** (`Configuration_adv.h`, đang **BẬT**) + `src/lcd/encoder_quad.cpp`.
  Muốn quay lại cách cũ: comment `#define ENCODER_QUAD_ISR`. Chi phí: +1,2 KB flash, +40 B RAM.
* Panel (`lcd_panel`): cùng thuật toán trong `lcd_panel/src/encoder.cpp` — quan trọng hơn vì panel
  vẽ SPI theo từng khối lớn (một lần `fillScreen` 240×320 ≈ 150 KB ⇒ hàng chục ms **không đọc được
  chân**), polling ở đó chắc chắn mất nấc.
* Đã kiểm bằng mô phỏng: 1 vòng CW = +1 nấc, CCW = −1, **có dội phím vẫn đúng**, **nhiễu nhảy 2 bit
  = 0 nấc**, và quay quá nhanh trong một lần đọc thì bị **kẹp trần** (chống chạy loạn).

---

## 14. ISR encoder (`ENCODER_QUAD_ISR`) có đụng ISR nào của Marlin không? — **Không**

Đã kiểm tận code (không đoán). Chân ngắt của encoder trên MKS Monster8 V2:

| | Chân | Digital pin | EXTI |
|---|---|---|---|
| `BTN_EN1` = `EXP2_03_PIN` | **PE9** | 73 (`variant.cpp` dòng 104, `NUM_DIGITAL_PINS 82`) | line **9** |
| `BTN_EN2` = `EXP2_05_PIN` | **PE8** | 72 (`variant.cpp` dòng 103) | line **8** |

**1. Bảng callback của core chỉ có 1 callback cho mỗi LINE (không phân biệt port).**
`attachInterrupt(pin, cb, CHANGE)` → `stm32_interrupt_enable()` lưu vào
`gpio_irq_conf[16]` (`framework-arduinoststm32/libraries/SrcWrapper/src/stm32/interrupt.cpp`), index =
số thứ tự chân 0…15. PE8 và PE9 là **2 line khác nhau** → 2 callback cùng tồn tại, **không ghi đè nhau**.
(`digitalPinToPinName(73)` → `digitalPin[73]` = `PE_9`; `PNUM_MASK 0xFF`, pin < 82 nên ánh xạ đúng.)

**2. Vector dùng chung `EXTI9_5_IRQHandler` — không phải xung đột.**
Line 5…9 chung 1 vector; handler quét từng line còn cờ rồi gọi `HAL_GPIO_EXTI_IRQHandler()`
(line nào xong line đó: xoá cờ → gọi callback của đúng line). Nên 2 chân encoder nằm gọn trong
1 vector và vẫn độc lập; sau này thêm ngắt ở line 6/7 cũng sống chung được.

**3. Trong bản build này, EXTI chỉ có đúng 2 người dùng — chính là 2 chân encoder:**

| Tính năng có thể gắn ngắt | Trạng thái | Bằng chứng |
|---|---|---|
| `ENDSTOP_INTERRUPTS_FEATURE` | **tắt** | `Configuration.h:1158` (comment) |
| `EMERGENCY_PARSER` | **tắt** | `Configuration_adv.h:2493` (comment) |
| Flowmeter (`LASER_COOLANT_FLOW_METER` → `attachInterrupt` trong `feature/cooler.h:69`) | **không biên dịch** | `SPINDLE_FEATURE`/`LASER_FEATURE` comment (3372/3373), flowmeter 3519, `TEMP_SENSOR_COOLER 0` (`Configuration.h:572`) |
| SD_DETECT (`PC4`/`EXP2_07`) | **polling**, không ngắt | `cardreader` đọc chân |
| BLTouch/servo, NeoPixel, TMC UART | timer/PWM/bit-bang | — |
| USB flash drive host | chỉ ở `env:mks_monster8_usb_flash_drive*` | không phải env đang dùng |

Trong `Marlin/src/HAL/STM32`, `Marlin/src/feature`, `Marlin/src/lcd` **không còn chỗ nào** gọi
`attachInterrupt`/`HAL_NVIC_EnableIRQ` cho GPIO — chỉ `lcd/encoder_quad.cpp`.

**4. Ưu tiên ngắt — encoder không thể làm hỏng nhịp bước (step).**

| ISR | Priority | Ghi chú |
|---|---|---|
| Servo / SoftwareSerial | 1 | `timers.cpp` default |
| **Step timer** | **2** | cắt ngang được ISR encoder ⇒ bước đi vẫn đúng nhịp |
| USB device (CDC) | 5 | `-DUSBD_IRQ_PRIO=5` (env `mks_monster8`) |
| **EXTI (encoder)** | **6** | `interrupt.h` (Cortex-M4 → 6); **không** override ở pins/variant/ini |
| Temp timer | 14 | ISR encoder cắt ngang được temp ISR |

`HAL_Init()` gọi `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)` ⇒ toàn bộ 4 bit là preemption,
sub-priority bị bỏ qua ⇒ so sánh số là so sánh quyền cắt ngang. `quad_isr()` chỉ đọc 2 chân
(`READ()` = đọc IDR) và ghi 1 byte vào ring: **không** `millis()`, không cấp phát, không in, không
khoá ngắt lồng nhau ⇒ vài chục chu kỳ, không ảnh hưởng ISR khác.

**5. Vài điều phải nhớ (đã kiểm, không phải suy đoán):**

* **Cấm trùng line:** bảng core chỉ giữ **1 callback/line**. Nếu sau này bật
  `ENDSTOP_INTERRUPTS_FEATURE` (hoặc bật lại laser/flowmeter) mà chân ngắt rơi vào **cùng line 8/9 ở
  port khác** (PA8/PA9, PB8/PB9, PC8/PC9, PD8/PD9) thì người gắn sau **ghi đè** callback của encoder.
  Line 5/6/7 khác slot nên vô hại.
* **NeoPixel khoá ngắt:** `Adafruit_NeoPixel::show()` gọi `noInterrupts()` trong lúc đẩy bit
  (~30 µs/byte ở 800 kHz ⇒ ~90 µs cho 1 LED RGB, ~1 ms cho 10 LED). Cờ EXTI vẫn được phần cứng
  **chốt lại**, nên cùng lắm mẫu đến muộn — không đếm sai (nhảy 2 bit = 0 xung).
* **Ring 32 mẫu**: ISR ghi dữ liệu **trước** rồi mới tăng `quad_head` (chỉ số `uint8_t volatile` →
  atomic trên M4), luồng chính chốt `head` rồi đọc tới đó ⇒ SPSC ring an toàn. Ring đầy thì **bỏ mẫu
  mới** (mất xung, không bao giờ sai chiều).
* **Tắt màn 12864 ⇒ không còn ngắt nào:** không có `HAS_ENCODER_WHEEL` thì cả `encoder_quad.cpp`
  không được biên dịch (guard `#if ENABLED(ENCODER_QUAD_ISR) && HAS_ENCODER_WHEEL && BUTTONS_EXIST(EN1, EN2)`).
* **Phía panel** (`lcd_panel`, MCU riêng, không liên quan ISR của Marlin): encoder PB12/PB13 →
  `EXTI15_10_IRQHandler` (line 12/13), trên panel **không** có ngắt nào khác; nút PB14 đọc bằng polling.
  Đã kiểm 2 variant: `F411C(C-E)(U-Y)` (PB12=27/PB13=28/PB14=29) và `F103C4T_F103C6` (28/29/30) —
  đều có thật trong `digitalPin[]`.

---

## 15. Màn hình LVGL riêng — Marlin **ĐẨY** dữ liệu (`PANEL_SERIAL_UI`)

Màn hình mới (**không** dùng G-code để vẽ): Marlin chủ động đẩy **frame nhị phân** xuống MCU màn
hình qua UART, panel vẽ bằng **LVGL**. Panel chỉ gửi G-code cho **hành động** (G28 / M104 / M140 /
M24 / M524…). Cách này cho panel **đủ dữ liệu để vẽ mọi khối** mà G-code không lấy được: danh sách
file SD, lưới bed mesh, PID / steps-mm / jerk, probe offset, hộp thoại xác nhận, tên file đang in…

**Vì sao không thể cứ dùng G-code?** Panel ở MCU riêng không đọc được biến nội bộ của Marlin; nếu chỉ
hỏi `M105/M114/M27` thì thiếu rất nhiều thứ, và MKS cũng phải thêm hàng loạt M-code riêng cho TFT
của họ (`makerbase-mks/MKS-TFT28-32-Firmware` — tham khảo để đối chiếu, không dùng lại).

| | |
|---|---|
| Trong Marlin | `Marlin/src/lcd/extui/panel_serial/` — ExtUI mới (hooks `ExtUI::on*`), bật bằng `#define PANEL_SERIAL_UI` |
| Giao thức | `panel_protocol.h` — **một file dùng chung** cho cả Marlin và panel (copy y nguyên hai bên) |
| UART | `MSerial3` = USART3 **PD8 (TX) / PD9 (RX)** = EXP1 chân 6/5. **Không** dùng `SERIAL_PORT_2` (Marlin sẽ coi đó là cổng host thứ 2 và diễn giải frame nhị phân như G-code) |
| Build flag (đã có trong `ini/stm32f4.ini`) | `-DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8` |
| Bật/tắt 1 lệnh | `pwsh lcd_panel/tools/set_panel_mode.ps1 -Mode on\|off\|status` (đổi 5 dòng config; xem `lcd_panel/README.md` §3) |
| Panel | `lcd_panel/src/panel_link.cpp` (nhận/phân tích frame) + `ui_lvgl.cpp` (LVGL 8.3) |
| Env panel | `panel_f407vgz_lvgl` · `panel_f411cc_lvgl` · `panel_esp32_cyd` (ESP32-2432S028R "CYD", ILI9341 + XPT2046) |
| Build thật (đã kiểm) | F411 LVGL: **134.012 B** flash / 46.180 B RAM · F407 LVGL: **135.928 B** / 74.900 B · ESP32-CYD: **441.561 B** / 95.284 B · (G-code UI: F411 20.688 B, F103C6 21.096 B/64 %) |
| Kiểm thử offline | `pwsh lcd_panel/tools/run_host_test.ps1` — 40 kiểm tra giao thức chạy trên máy tính (đã bắt được 2 lỗi thật: lệch offset `feed/flow %`, điều kiện gửi PING) |
| Flash Marlin | chế độ panel **196.444 B** (18,7 %) so với 266.536 B (25,4 %) khi dùng màn 12864 |

Khi bật `PANEL_SERIAL_UI`, Marlin **không thể đồng thời** có màn gắn trực tiếp (ExtUI chiếm
`MarlinUI::init_lcd()/update()` — sẽ lỗi `multiple definition`), nên script tắt luôn
`MKS_MINI_12864_V3`, `NEOPIXEL_LED` (PD8 trùng USART3_TX), `CUSTOM_MENU_MAIN` và `LED_CONTROL_MENU`
(ExtUI không dùng hệ thống menu của MarlinUI — các mục menu tuỳ biến được đưa sang màn **CUSTOM**
của panel, đúng 2 mục cũ `G34 Q99` và `M22`+`M21`). SanityCheck của Marlin báo lỗi rõ ràng nếu còn
màn hình gắn trực tiếp.

> ⚠️ **Chưa nạp thử lên máy**: code Marlin đã build sạch ở cả 2 chế độ và giao thức đã test trên máy
> tính, nhưng phần cứng thật (ESP32-CYD / F407 + màn) thì phải chờ board về mới kiểm được.
>
> 📌 **Việc còn nợ:** chế độ panel phải tắt `CUSTOM_MENU_MAIN` (+ `LED_CONTROL_MENU`) nên **menu tuỳ
> biến của Marlin hiện không có**; 2 mục cũ đã đưa sang màn CUSTOM của panel nhưng thêm mục mới thì
> phải sửa code panel. Kế hoạch & tiêu chí nghiệm thu ở
> **`docs/issues/0001-panel-serial-ui-tat-custom-menu.md`** (issue nháp, chưa tạo trên GitHub — máy
> không có `gh` CLI/token, trong file có sẵn lệnh tạo).

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

Mốc gần đây:

```
08/10/2026: THU cho X/Y chay 1/8 microstep (TTL_XY_MICROSTEP=8, M92 X40 Y40)
            -> truc Y di chuyen sai => DA HOAN TAC ve 1/16 + M92 X80 Y80.
            Phat hien build tang dan cho ra firmware stale (phai clean rebuild).
            Them send-gcode.ps1. Xem 11.4b + cam bay 15b/10c/10d
```
