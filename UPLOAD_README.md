# Marlin Firmware Upload Scripts

Hai script PowerShell để upload firmware Marlin lên MKS Monster8 V2.

## 📋 Yêu cầu

- **Python 3** và **PlatformIO Core** (đã cài)
- PowerShell 5.1+ (hoặc PowerShell Core)
- Board MKS Monster8 V2 (STM32F407VGT6)

## ⚠️ ĐỊA CHỈ FLASH — QUAN TRỌNG NHẤT

MKS Monster8 có **bootloader MKS chiếm 0xC000 = 48KB đầu flash** (sector 0–2, mỗi sector 16KB). Firmware Marlin được link tại:

```
0x08000000 + 0xC000 = 0x0800C000
```

(xem `board_build.offset` và `board_upload.offset_address` trong `ini/stm32f4.ini`)

| Địa chỉ ghi | Kết quả |
|---|---|
| ✅ **`0x0800C000`** | Đúng — chỉ xoá sector 3 trở đi, bootloader còn nguyên |
| ❌ `0x08000000` | **Xoá bootloader MKS (sector 0–2) → board không boot** |

Kiểm chứng bố cục flash bằng `dfu-util --list`:
```
@Internal Flash /0x08000000/04*016Kg,01*064Kg,07*128Kg
```
3 sector đầu = 3 × 16KB = 48KB = 0xC000 → chính là vùng bootloader.

**Tên file:** `board_build.rename` đổi `firmware.bin` → **`mks_monster8.bin`**, nên sau khi build **không còn file `firmware.bin`**. Đường dẫn đúng là:
```
.pio\build\mks_monster8\mks_monster8.bin
```

> 💡 Board **không thể "chết" hoàn toàn**: bootloader hệ thống của STM32 nằm trong ROM, nên luôn khôi phục được bằng cách giữ BOOT0 + nhấn RESET rồi flash lại đúng địa chỉ.

---

## 🔧 Script 1: upload-firmware.ps1 (ST-Link) ⭐ Khuyên dùng

**Upload qua ST-Link debugger** — phương pháp mặc định cho MKS Monster8.

### Yêu cầu phần cứng
- Cáp **ST-Link V2** (USB → 4 pin: GND, CLK, DIO, +3.3V)
- Nối như sau:
  - GND → GND
  - CLK → SWCLK (trên board)
  - DIO → SWDIO (trên board)
  - 3.3V → 3.3V

### Cách dùng

```powershell
# Build + Upload (có hỏi xác nhận)
.\upload-firmware.ps1

# Upload mà không hỏi xác nhận
.\upload-firmware.ps1 -NoPrompt

# Chỉ build, không upload
.\upload-firmware.ps1 -SkipUpload

# Build env khác (nếu có)
.\upload-firmware.ps1 -Environment "mks_monster8_usb_flash_drive"
```

### Kết quả thành công
```
SUCCESS: Build completed!

Firmware artifact:
  Binary: D:\Marlin\.pio\build\mks_monster8\mks_monster8.bin
  Size: 240.22 KB

Next steps:
  1. Monitor via serial: pio device monitor -b 250000
  2. Verify Configuration.h settings
  3. Run calibration if needed
```

---

## 🔌 Script 2: upload-dfu.ps1 (USB Bootloader)

**Upload qua DFU bootloader** — nếu ST-Link không khả dụng.

### Yêu cầu
- Cài **dfu-util** — cách gọn nhất là để PlatformIO tự tải (không cần quyền admin):
  ```powershell
  pio pkg install -g -t tool-dfuutil
  # -> C:\Users\<ten>\.platformio\packages\tool-dfuutil\bin\dfu-util.exe
  ```
  Hoặc cài toàn hệ thống:
  ```powershell
  winget install dfu-util
  ```
  Script `upload-dfu.ps1` tự tìm cả hai vị trí này.
- Board **ở chế độ DFU mode**

### Cách vào DFU Mode

1. **Giữ phím BOOT0** (nếu có trên board)
2. **Nhấn RESET** (hoặc cắt/bật nguồn)
3. **Thả BOOT0**
4. Board sẽ xuất hiện dưới dạng "STM32 DFU" device

### Cách dùng

```powershell
# Build + Upload qua DFU
.\upload-dfu.ps1

# Chỉ build, không upload
.\upload-dfu.ps1 -BuildOnly

# Chỉ định device DFU cụ thể
.\upload-dfu.ps1 -DfuDevice "0"

# Tắt prompt xác nhận
.\upload-dfu.ps1 -NoPrompt
```

### Kiểm tra DFU device

```powershell
dfu-util --list
```

Kết quả mong đợi:
```
Found STM32 STM32F407xx ...
```

---

## 🚀 Các bước chính

### 1️⃣ Chuẩn bị
```powershell
cd D:\Marlin

# Kiểm tra cấu hình
pio boards | grep -i "mks_monster8"
pio run -d . -e mks_monster8 --target build
```

### 2️⃣ Upload
```powershell
# Cách 1: ST-Link (nhanh, đáng tin cậy)
.\upload-firmware.ps1

# Cách 2: DFU bootloader
.\upload-dfu.ps1
```

### 3️⃣ Xác minh
- Quan sát LED trên board sáng
- Cắm cáp USB serial → máy tính
- Mở serial monitor (baud 250000):
  ```powershell
  pio device monitor -d . -b 250000
  ```

---

## ⚠️ Khắc phục sự cố

### ST-Link không được nhận diện
```powershell
# Kiểm tra driver
Get-PnpDevice | Where-Object {$_.Name -like "*STM32*"}

# Cài lại driver: 
# https://github.com/stm32duino/wiki/wiki/Upload-methods
```

### DFU không nhận diện
```powershell
# Liệt kê tất cả USB device
dfu-util --list

# Flash lại — DÙNG ĐÚNG ĐỊA CHỈ 0x0800C000 (xem mục "ĐỊA CHỈ FLASH" ở trên)
dfu-util -d 0483:df11 -a 0 -s 0x0800C000:leave -D .pio\build\mks_monster8\mks_monster8.bin

# ⚠️ KHÔNG dùng 0x08000000 — sẽ xoá bootloader MKS và board không boot được
```

### Build lỗi
```powershell
# Xóa cache build
Remove-Item -Recurse -Force .\.pio\build\mks_monster8

# Build lại
pio run -d . -e mks_monster8 --target build
```

---

## 📊 Kích thước firmware

| Phần | Kích thước | Phần trăm |
|------|-----------|---------|
| Flash (used) | 246.052 KB | 23.5% |
| RAM (used) | 12.012 KB | 9.2% |
| **Flash (free)** | **802.524 KB** | **76.5%** |
| **Total Flash** | **1.048.576 KB** | — |

---

## 🔗 Tài liệu tham khảo

- [PlatformIO Docs](https://docs.platformio.org/)
- [STM32F407 Datasheet](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)
- [Marlin Firmware](https://marlinfw.org/)
- [DFU Bootloader Guide](https://docs.platformio.org/en/latest/platforms/ststm32.html#upload-using-dfu)

---

## 💡 Mẹo

- **Serial monitor** liên tục:
  ```powershell
  pio device monitor -d . -e mks_monster8
  ```

- **Rebuild sạch**:
  ```powershell
  pio run -d . -e mks_monster8 --target clean
  pio run -d . -e mks_monster8
  ```

- **Xem chi tiết build**:
  ```powershell
  pio run -d . -e mks_monster8 -v
  ```

---

**Hỏi đáp**: Nếu gặp lỗi, kiểm tra:
1. ✓ Python + PlatformIO đã cài
2. ✓ Cáp kết nối đúng
3. ✓ Board được cấp nguồn
4. ✓ `platformio.ini` có cấu hình `mks_monster8`
