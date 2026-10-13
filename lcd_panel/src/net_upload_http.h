/**
 * net_upload_http.h -- WEB SERVER tren PANEL: PC/dien thoai mo http://<panel>/ la gui duoc file.
 * (Hop dong da chot voi nhanh script - xem README 9.21.)
 *
 *   GET  /                       -> trang HTML (co chuoi "TBT-PANEL")
 *   GET  /status                 -> JSON: panel, device, state, rx_bytes, total_bytes, posted_bytes,
 *                                   file, result, message
 *   POST /upload?token=<token>   -> multipart/form-data: field text name="name" (ten file dich,
 *                                   dat TRUOC phan file) + phan file name="file" filename="<ten>"
 *                                   -> {"status":"Ok"|"Error", ...}
 *   Token: query ?token=  HOAC header X-Panel-Token  (PANEL_HTTP_TOKEN; "" = khong kiem)
 *
 * LUONG: nhan file -> ghi /btup/<ten>.tmp tren the TF -> tra loi NGAY -> panel TU HOAN TAT:
 *   - PANEL_SD_SHARED 1 (mac dinh, xem panel_config.h muc "THE SD DUNG CHUNG"): doi ten file tam
 *     thanh /<ten> tren THE SD DUNG CHUNG voi Marlin (mux 74HC4053) + tra bus + M22/M21.
 *   - PANEL_SD_SHARED 0: POST multipart sang module ESP3D (nhu duong BT) + M22/M21 + xoa file tam.
 * /status phan anh tien do (state=posting -> done/error).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void net_upload_http_begin(void);   // bat WiFi (net_wifi) + the TF + WebServer + mDNS
void net_upload_http_task(void);    // goi MOI vong lap: server.handleClient() + day file khi can
void net_upload_http_log(void);     // 1 dong log 1 Hz (de doc tren COM7)
bool net_upload_http_active(void);  // true = WebServer dang chay (de main.cpp khong goi log 2 lan)

// true = DANG co phien nhan/ghi file (ke tu luc bat dau nhan den luc ghi xong vao the).
// UI dung co nay de CHAN cac lenh lam Marlin doc the SD (M23/M24/M30/M21/xin danh sach file)
// trong luc panel dang giu bus SPI cua the dung chung (xem ui_lvgl.cpp: sd_host_busy).
bool net_upload_active(void);

// XOA 1 file NGAY TREN PANEL (che do PANEL_SD_UART: the o khe man hinh, panel giu the con Marlin
// chay SDCARD_READONLY nen M30 se loi). Tra true = da xoa xong. Nguoi goi PHAI tu bao Marlin mount
// lai the (ui_lvgl.cpp goi files_init_sd() -> M22/M21 qua UART + xin lai danh sach file).
bool net_sd_delete_file(const char *name);

// Tam dung / bat lai mDNS. Dung trong luc QUET MANG: khi quet, STA roi kenh => interface "down",
// mDNS (responder) rat de panic/assert trong tinh huong do. Tra ve true = mDNS dang BAT sau loi goi.
// Neu PANEL_HTTP_USE_MDNS 0 thi luon tra false.
bool net_upload_mdns_pause(bool pause);

#ifdef __cplusplus
}
#endif
