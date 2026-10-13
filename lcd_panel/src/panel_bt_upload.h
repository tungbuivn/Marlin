/**
 * panel_bt_upload.h -- Nhan file G-code qua Bluetooth SPP roi day len module ESP3D.
 *
 * LUONG (2 pha):
 *   PHA 1: dien thoai gui file qua BT SPP (BluetoothSerial) -> panel ghi dan xuong file TAM
 *          tren the TF CUA CHINH PANEL (/btup/<ten>.tmp). Khong giu file trong RAM.
 *   PHA 2: POST file tam len ESP3D (module WiFi cam tren board, ghi thang vao the SD cua
 *          Marlin) bang multipart/form-data + Content-Length (biet truoc vi file da o tren the).
 *          Trong luc day: KHOI DONG lop phu toan man hinh tren lv_layer_top() + TAT moi input
 *          device (lv_indev_enable(i,false)) -> "khoa UI", khong can sua ui_lvgl.cpp.
 *   XONG:  gui M22 (nha media) + M21 (init media) cho Marlin, cho ~800 ms roi
 *          panel_link_request(PANEL_REQ_FILES) (Marlin chi thay file moi sau khi init lai the),
 *          xoa file tam, bao DONE qua BT.
 *
 * Bat/tat bang `PANEL_BT_UPLOAD` trong panel_config.h (chi co tac dung voi board CYD).
 * Cau hinh WiFi/ESP3D cung nam trong panel_config.h.
 *
 * main.cpp goi:
 *   setup(): panel_bt_upload_begin();
 *   loop() : panel_bt_upload_task();     // + panel_bt_upload_log() o nhip 1 Hz (debug)
 *
 * File nay KHONG dung den ui_lvgl.cpp / panel_protocol.h.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "panel_config.h"
#include "net_upload.h"          // net_wifi_state() -- hop dong voi nhanh UI (icon WiFi header)

// Co duoc build khong: chi board CYD (ESP32) + UI LVGL + PANEL_BT_UPLOAD = 1.
#if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD && PANEL_USE_LVGL && defined(PANEL_BOARD_CYD)
  #define PANEL_BTUP_ENABLED 1
#else
  #define PANEL_BTUP_ENABLED 0
#endif

// Khoi tao BT SPP + task doc BT. Goi 1 lan trong setup() (voi moi board: rong neu tat).
void panel_bt_upload_begin(void);

// May trang thai: goi lien tuc trong loop(). PHA 2 CHAY BLOCKING trong ham nay (co khoa UI).
void panel_bt_upload_task(void);

// 1 dong log 1 Hz: trang thai BT/WiFi/phien + heap (+ canh bao neu heap < PANEL_BT_HEAP_WARN).
void panel_bt_upload_log(void);

// true khi dang nhan file hoac dang day len ESP3D (dung cho log/chan phien chong nhau).
bool panel_bt_upload_busy(void);
