/**
 * net_upload.h -- HOP DONG GIAO DIEN voi nhanh UI: TRANG THAI WiFi (icon o header) + trang WiFi
 *                 settings (net_wifi.h).
 *
 * File nay CO Y khai bao that nhe (chi <stdint.h>/<stdbool.h>): nhanh UI include no ma khong keo
 * WiFi.h / BluetoothSerial.h / SD.h vao build cua ui_lvgl.cpp.
 *
 * Ban implement nam trong net_wifi.cpp (module quan ly WiFi doc lap voi Bluetooth). Khi
 * PANEL_WIFI_MGMT = 0 (board STM32 / khong co WiFi) thi net_wifi.cpp chi con STUB tra ve 0 nen
 * moi env van link duoc.
 *
 * QUAN TRONG (linkage): tat ca 8 ham WiFi deu duoc khai bao trong `extern "C"` (o net_wifi.h) va
 * dinh nghia voi linkage C trong net_wifi.cpp - khop voi khai bao `extern "C"` + weak ma nhanh UI
 * da dat trong ui_lvgl.cpp. Neu de linkage C++ (ten bi mang hoa) thi weak cua UI KHONG bind duoc
 * => trang WiFi hien "KHONG HO TRO" ma khong co loi nao.
 */
#pragma once

#include <stdint.h>

#include "net_wifi.h"    // 8 ham net_wifi_* (extern "C"): state/ssid/ip/rssi/scan/connect/disconnect

#ifdef __cplusplus
extern "C" {
#endif

// 0 = WiFi TAT / chua cau hinh / khong co module
// 1 = DANG ket noi (dang thu, ke ca dang thu lai sau khi mat ket noi)   -> icon DO
// 2 = DA ket noi                                                       -> icon XANH
//
// AN TOAN khi goi tu vong lap UI (ui_lvgl_task): khong chan, khong cap phat, khong goi
// WiFi.* -- chi doc bien trang thai da cache (cap nhat trong net_wifi_task()).
// Chi tra ve 2 khi WiFi.status() == WL_CONNECTED (khong phai chi vi da goi WiFi.begin()).
// (Dinh nghia nam trong net_wifi.cpp.)
uint8_t net_wifi_state(void);

#ifdef __cplusplus
}
#endif
