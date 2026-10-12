/**
 * encoder_quad.h -- quadrature decoder for the LCD encoder wheel.
 *
 * WHY THIS EXISTS (so sánh với thuật toán cũ của Marlin và với D:\summer-fan):
 *
 *  - Marlin gốc đọc 2 chân A/B bằng POLLING trong `MarlinUI::update_buttons()` rồi cộng ±1
 *    cho mỗi cạnh. Bảng chuyển tiếp của Marlin **đã** bỏ qua bước nhảy 2 bit (nó chỉ quay
 *    khi trạng thái trước là một trong hai "hàng xóm" hợp lệ), nên về mặt chống nhiễu thì
 *    nó đã tốt. Điểm yếu thật sự là **mất cạnh**: khi vòng UI bị chặn lâu (đọc SD, ghi
 *    EEPROM, vẽ màn hình, delay lúc probe...) thì encoder quay mà không ai đọc chân, và
 *    Marlin phải bù bằng `ENCODER_RATE_MULTIPLIER` - tức là ĐOÁN số bước bị mất theo tốc
 *    độ quay, không phải số đếm thật.
 *
 *  - summer-fan (D:\summer-fan\src\encode2.cpp) giải đúng điểm đó: ngắt đổi chân A/B +
 *    hàng đợi nên KHÔNG mất cạnh, và máy trạng thái chỉ tính khi đi hết một chu kỳ 4 cạnh
 *    theo đúng thứ tự -> rất chống dội. Nhưng nó dùng `std::queue` + `new` NGAY TRONG NGẮT
 *    (cấp phát heap trong ISR: phân mảnh, không xác định - không dùng được trong Marlin),
 *    chỉ tính 1 sự kiện/chu kỳ đầy (thô hơn `ENCODER_PULSES_PER_STEP`), và bỏ bớt sự kiện
 *    khi quay nhanh.
 *
 *  - Ở đây lấy ý tốt của summer-fan (ngắt + hàng đợi) nhưng làm cho an toàn: ISR chỉ đẩy
 *    mẫu 2 bit vào một RING CỐ ĐỊNH (không heap, vài lệnh), giải mã ở luồng chính bằng
 *    đúng bảng chuyển tiếp + đúng đơn vị của Marlin (±1 mỗi cạnh, chia
 *    `ENCODER_PULSES_PER_STEP` như cũ), cộng thêm chốt trần để một lần quá nhanh không
 *    làm giá trị chạy loạn.
 *
 * => Lợi ích thật: ĐẾM CHÍNH XÁC kể cả khi UI đang bận, nên `ENCODER_RATE_MULTIPLIER` chỉ
 *    còn là "tăng tốc khi quay nhanh" (đúng như tên nó) chứ không phải bù bước bị mất.
 *
 * Bật bằng `#define ENCODER_QUAD_ISR` trong Configuration_adv.h.
 */
#pragma once

#include "../inc/MarlinConfig.h"
#include "buttons.h"          // HAS_ENCODER_WHEEL, BUTTONS_EXIST, BTN_EN1 / BTN_EN2

#if ENABLED(ENCODER_QUAD_ISR) && HAS_ENCODER_WHEEL && BUTTONS_EXIST(EN1, EN2)

  static constexpr uint8_t QUAD_RING_SIZE = 32;   // samples of 2 bits (power of two)

  void quad_encoder_init();      // attach the two pin-change interrupts (call once)
  int8_t quad_encoder_pulses();  // signed pulses since the last call (clamped to +-64)

#endif
