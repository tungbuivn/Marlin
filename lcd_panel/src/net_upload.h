/**
 * net_upload.h -- HOP DONG GIAO DIEN voi nhanh UI: TRANG THAI WiFi (icon o header).
 *
 * File nay CO Y chi khai bao that nhe (chi <stdint.h>): nhanh UI include no ma khong keo
 * WiFi.h / BluetoothSerial.h / SD.h vao build cua ui_lvgl.cpp.
 *
 * Ban implement nam trong panel_bt_upload.cpp (module nhan file BT -> the TF -> POST ESP3D).
 * Khi module do KHONG duoc build (board STM32, hoac -DPANEL_BT_UPLOAD=0) thi van co ham nay
 * (stub tra ve 0) nen moi env deu link duoc.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 0 = WiFi TAT / chua cau hinh / khong co module
// 1 = DANG ket noi (dang thu, ke ca dang thu lai sau khi mat ket noi)   -> icon DO
// 2 = DA ket noi                                                       -> icon XANH
//
// AN TOAN khi goi tu vong lap UI (ui_lvgl_task): khong chan, khong cap phat, khong goi
// WiFi.* -- chi doc bien trang thai da cache (cap nhat trong panel_bt_upload_task()).
// Chi tra ve 2 khi WiFi.status() == WL_CONNECTED (khong phai chi vi da goi WiFi.begin()).
uint8_t net_wifi_state(void);

#ifdef __cplusplus
}
#endif
