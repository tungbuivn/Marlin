# `cydlcd/sim` — giả lập UI màn hình CYD **trên PC** (không cần board)

Mục đích: xem/sửa **giao diện thật** của màn hình (bố cục, màu, font, icon, menu, đa ngôn ngữ) **ngay trên
máy tính**, không phải nạp board mỗi lần. Ra đời sau lỗi *"tại trục X = 285.98 thì số `2` đè vào nhãn `X`"*
(2026-10-16) — lỗi chỉ nhìn mới thấy, mà lúc đó không có board trong tay.

## Chạy

```powershell
# 1) Chạy kịch bản MẶC ĐỊNH: mở lần lượt MỌI trang, chụp ảnh + tự kiểm tra hàng X/Y/Z
pwsh cydlcd/tools/run_sim.ps1

# 2) Chạy kịch bản tự viết (xem mục "Kịch bản" dưới)
pwsh cydlcd/tools/run_sim.ps1 -Script cydlcd/sim/scenarios/test-xyz-dai.txt

# 3) Kiểm tra "bẫy kiểm thử có thật sự bắt được lỗi không": build lại bằng BỐ CỤC CŨ
pwsh cydlcd/tools/run_sim.ps1 -OldXyz -Script cydlcd/sim/scenarios/test-xyz-dai.txt   # -> PHẢI báo lỗi
```

Ảnh `.bmp` ghi vào `cydlcd/sim/out/` (mở bằng Paint/Photos app). **Exit code = số kiểm tra tự động thất bại**
(0 = đạt) nên có thể dùng trong CI/script.

**Cần:** `g++`/`gcc` (WinLibs/MinGW — `winget install BrechtSanders.WinLibs.POSIX.UCRT`) và **nguồn LVGL**,
đã có sẵn sau khi build env CYD một lần: `pio run -d cydlcd -e cydlcd_esp32` (PlatformIO tải LVGL vào
`cydlcd/.pio/libdeps/cydlcd_esp32/lvgl`). Không cần WiFi, không cần mạng sau đó.

## Cái gì là THẬT, cái gì là giả

| Thành phần | Thật hay giả |
|---|---|
| Toàn bộ UI: `src/ui_*.cpp` + `ui_lvgl_icons.h` + 3 font GFX (`gfx4_font_*`, `lv_font_gfxfont`) | **THẬT** — build đúng file của firmware, không vẽ lại |
| Chuỗi đa ngôn ngữ (`ui_lang_vi.h` / `ui_lang_en.h`), menu Configuration, trang BED MESH/JOG/… | **THẬT** |
| Logic link (`src/cydlcd_link.cpp`) + TRAMMING (`src/tbt_link.cpp`), giao thức PUSH | **THẬT** (UART giả) |
| Màn hình | giả: 1 mảng framebuffer 320×240 RGB565 trong RAM → ghi ra `.bmp` (`sim_port.cpp`) |
| Cảm ứng | giả: hàng đợi "chạm" do kịch bản bơm vào (`tap x y`, hoặc `hit k` = bấm nút thứ k đúng đường sự kiện) |
| Thẻ SD, WiFi, upload HTTP, NVS | giả (`sim_stubs.cpp`, `sim_net_stubs.cpp`): báo "chưa có thẻ"/"WiFi tắt" |
| Đồng hồ | giả: `sim_advance(ms)` cho thời gian trôi **nhanh hơn thực tế** (không phải chờ thật) |

## Kịch bản (mỗi dòng 1 lệnh)

| Lệnh | Việc |
|---|---|
| `state basic\|offline\|home` | nạp dữ liệu máy giả (nhiệt, vị trí, mesh 7×7, danh sách file, 45 thông số…) |
| `pos <x> <y> <z>` | đổi toạ độ (mm) — dùng để thử số DÀI (vd `-1234.56`) |
| `page <KEY>` | mở trang: `STATUS`, `MENU`, `FEED/FLOW`, `NHIET DO`, `JOG`, `FILE SD`, `THONG SO`, `BED MESH`, `CUSTOM`, `TRAMMING`, `INFO`, `ENDSTOP`, `WIFI`, `CAU HINH` |
| `hit <k>` | bấm nút thứ k của trang (đi đúng `ui_lvgl_test_hit` của firmware) |
| `tap <x> <y>` | chạm vào toạ độ (đi qua indev, như chạm tay) |
| `settle <ms>` | cho đồng hồ giả chạy để LVGL vẽ xong |
| `shot <ten.bmp>` | ghi ảnh + đếm pixel "có mực" (cảnh báo nếu trang gần như trắng) |
| `checkxyz` | **kiểm tra tự động** hàng X/Y/Z: nhãn trục và số phải cách nhau ≥ 3 px |
| `ui <lenh UI ...>` | gọi đúng `ui_console_dispatch()` của firmware (vd `ui HIT 5`, `ui BOX`, `ui CFG`) |
| `echo <chu>` / `# ...` | in ra console / chú thích |

## Kiểm tra tự động (không cần mắt người)

`checkxyz` đọc **chính framebuffer**: tách các "nhóm cột có mực" trong dải y của hàng XYZ (nhãn `X` là nhóm
1, số là nhóm 2) rồi đo khoảng trống. Nhãn và số **dính liền** (1 nhóm) ⇒ FAIL.

Bằng chứng bẫy này có thật sự bắt được lỗi (đã chạy, 2026-10-16):

| Bố cục | Kết quả `checkxyz` với `pos -1234.56 …` |
|---|---|
| **Cũ** (giá trị căn phải trong hộp `[LX .. LX+58]` — bắt đầu NGAY tại nhãn) | `X: 1 nhóm [14..72] <-- nhan va so DINH LIEN (loi!)` · `Y: … loi!` ⇒ **4 lỗi**, exit code 4 |
| **Mới** (`MR_XYZ_VX` = nhãn + 14; hộp 66/66/68 px) | `X: [14..24] [30..94] khe = 5 px OK` · `Y: … 5 px OK` ⇒ **0 lỗi** |

Chạy lại bố cục cũ bất cứ lúc nào: `pwsh cydlcd/tools/run_sim.ps1 -OldXyz -Script cydlcd/sim/scenarios/test-xyz-dai.txt`
(macro `MR_XYZ_*` trong `src/ui_lvgl_int.h` được bọc `#ifndef` để ghi đè từ ngoài — đó là mục đích của việc bọc).

## KHÔNG thay thế được gì

- **Phần cứng**: cảm ứng thật (hiệu chuẩn XPT2046), đèn nền, SPI/ tốc độ vẽ thật, thẻ SD, WiFi, NVS.
- **Marlin**: giả lập này chỉ chạy firmware **màn hình**. Muốn thử Marlin trên PC thì phải dùng hướng khác
  (`[env:linux_native]` của Marlin hoặc `simulator_windows` — xem README gốc), và board STM32 thì cần board.
- **Thời gian thật**: `sim_advance()` làm đồng hồ chạy nhanh, nên **không** đo được hiệu năng thật (dùng
  `UI PERF` trên board cho việc đó).

## File

| File | Việc |
|---|---|
| `sim_main.cpp` | `main()`: dữ liệu máy giả + đọc kịch bản + kiểm tra tự động (`checkxyz`) |
| `sim_port.cpp` | hiện thực `ui_port_*`: framebuffer, xuất BMP, cảm ứng giả, đồng hồ giả |
| `sim_stubs.cpp` | UART Marlin giả, thẻ SD giả, `logq`, heartbeat, `ui_console_dispatch` |
| `sim_net_stubs.cpp` | WiFi/upload/notify giả — **file riêng** để tránh thuộc tính `weak` (xem đầu file) |
| `lv_conf_sim.h` | cấu hình LVGL cho PC: pool 256 KB + bật font 20 (giống `lv_conf_cydlcd.h` của board CYD) |
| `shim/Arduino.h` | `Serial` + `millis()/delay()` + `min/max`… cho môi trường không có Arduino |
| `scenarios/*.txt` | kịch bản mẫu |
