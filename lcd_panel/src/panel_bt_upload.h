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

// ---------------------------------------------------------------------------
// HOP DONG VOI NHANH UI: trang thai THE TF cua panel (theo doi nong - rut ra / cam vao).
//   true = the TF dang mount va dung duoc. CHI doc bien da CACHE: khong chan, khong giao tiep SPI,
//   khong cap phat -> goi duoc tu ui_lvgl_task()/trong luc ve man hinh. Khi PANEL_BT_UPLOAD = 0
//   (hoac board khac) luon tra false.
// ---------------------------------------------------------------------------
bool panel_bt_upload_sd_ready(void);

// UI co the goi de YEU CAU thu mount lai the NGAY (vd nguoi dung vua cam the vao roi bam REFRESH).
// Ham chi BAT CO roi tra ve ngay (khong chan, khong SPI); viec mount that su chay trong
// panel_bt_upload_task() o vong lap ke tiep (SD.begin ton ~20-100 ms, chi chay khi dang ranh).
void panel_bt_upload_sd_retry(void);

// ---------------------------------------------------------------------------
// HOP DONG VOI NHANH UI: TIEN DO PHA 1 (dang nhan file qua BT) - de UI ve o THANH TIEU DE.
//   Tra ve true  = DANG o PHA 1 (dang nhan file qua BT).
//   *got         = so byte da nhan duoc (byte, KHONG phai KB).
//   *total       = tong so byte theo header cua app; 0 = app khong bao truoc kich thuoc.
//   got / total co the la NULL (chi can biet "dang nhan hay khong").
//   CHI doc bien da CACHE (khong chan, khong cap phat, khong cham vao BT/LVGL/the TF) nen goi
//   duoc tu ui_lvgl_task() o nhip ~400 ms. Cap nhat moi lan hut duoc du lieu + moi nhip task.
//   Khi PHA 1 ket thuc (bat dau day sang ESP3D): tra ve false, got/total GIU gia tri cuoi cung
//   cua phien (UI tu quyet dinh con hien hay khong) - tranh nhay ve 0 ngay truoc lop phu PHA 2.
//   PHA 1 KHONG khoa UI va KHONG co lop phu nao; lop phu toan man hinh + khoa UI chi xuat hien o
//   PHA 2 (luc day file len ESP3D) - xem README 9.18.
//   Khi PANEL_BT_UPLOAD = 0 (hoac board khac): luon tra false, got=total=0. Stub nam trong
//   panel_bt_upload.cpp (moi env deu build file nay: platformio.ini build_src_filter = +<*>)
//   nen nhanh UI khong can khai bao lai - khong can weak/__has_include.
// ---------------------------------------------------------------------------
bool panel_bt_upload_rx(uint32_t *got, uint32_t *total);

// ---------------------------------------------------------------------------
// BAT/TAT CHE DO NHAN FILE (cho nut bam tren UI)
//   set_enabled(true) : bat BT (co cua an toan RAM) + DISCOVERABLE de dien thoai quet thay.
//   set_enabled(false): tu choi/ngat client hien tai + NON-DISCOVERABLE + tat BT (tra ~100 KB heap).
//   Mac dinh luc khoi dong: PANEL_BT_UPLOAD_AUTOSTART (0 = TAT). Tu tat sau PANEL_BT_ENABLE_MIN
//   phut khong co phien.
//   enabled() = chi doc co da cache (khong chan, khong cap phat) - dung de UI ve trang thai nut.
//   Khi PANEL_BT_UPLOAD = 0: enabled() luon false, set_enabled() khong lam gi.
// ---------------------------------------------------------------------------
void panel_bt_upload_set_enabled(bool on);
bool panel_bt_upload_enabled(void);
