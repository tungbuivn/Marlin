/**
 * net_upload_http.h -- WEB SERVER tren MAN HINH CYD: PC/dien thoai mo http://<man hinh CYD>/ la gui duoc file.
 * (Hop dong da chot voi nhanh script - xem README 9.21.)
 *
 *   GET  /                       -> trang HTML (co chuoi "TBT-CYDLCD")
 *   GET  /status                 -> JSON: cydlcd, device, state, rx_bytes, total_bytes, written_bytes,
 *                                   file, result, message
 *   POST /upload?token=<token>   -> multipart/form-data: field text name="name" (ten file dich,
 *                                   dat TRUOC phan file) + phan file name="file" filename="<ten>"
 *                                   -> {"status":"Ok"|"Error", ...}
 *   Token: query ?token=  HOAC header X-CYDLCD-Token  (CYDLCD_HTTP_TOKEN; "" = khong kiem)
 *
 * LUONG: nhan file -> ghi /cydlcd_tmp/<ten>.tmp tren the TF -> tra loi NGAY -> man hinh CYD TU HOAN TAT:
 *   - CYDLCD_SD_SHARED 0 (MAC DINH - the thuoc man hinh): doi ten file tam thanh /<ten> o GOC the
 *     + bao Marlin mount lai (M22/M21).
 *   - CYDLCD_SD_SHARED 1 (duong lui, mux 74HC4053): tra bus SPI cho Marlin + M22/M21.
 * /status phan anh tien do (state=receiving -> writing -> done/error).
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
// trong luc man hinh CYD dang giu bus SPI cua the dung chung (xem ui_lvgl.cpp: sd_host_busy).
bool net_upload_active(void);

// XOA 1 file NGAY TREN MAN HINH CYD (che do CYDLCD_SD_UART: the o khe man hinh, man hinh CYD giu the con Marlin
// chay SDCARD_READONLY nen M30 se loi). Tra true = da xoa xong. Nguoi goi PHAI tu bao Marlin mount
// lai the (ui_lvgl.cpp goi files_init_sd() -> M22/M21 qua UART + xin lai danh sach file).
bool net_sd_delete_file(const char *name);

// Dem so MUC CON cua 1 THU MUC o goc the (-1 = khong mo duoc). KHONG xoa gi.
int net_sd_dir_count(const char *name);

// XOA 1 THU MUC RONG ngay tren man hinh CYD (giong net_sd_delete_file: the o khe man hinh, Marlin
// chay SDCARD_READONLY nen `M30` khong dung duoc - va M30 cua Marlin cung KHONG xoa duoc thu muc).
// Tra true = da xoa; false + *not_empty = true khi thu muc CON muc con (khong mat du lieu gi).
// Xoa xong nguoi goi PHAI bao Marlin mount lai the (ui_page_files.cpp: files_init_sd()).
bool net_sd_delete_dir(const char *name, bool *not_empty);

// Tam dung / bat lai mDNS. Dung trong luc QUET MANG: khi quet, STA roi kenh => interface "down",
// mDNS (responder) rat de panic/assert trong tinh huong do. Tra ve true = mDNS dang BAT sau loi goi.
// Neu CYDLCD_HTTP_USE_MDNS 0 thi luon tra false.
bool net_upload_mdns_pause(bool pause);

#ifdef __cplusplus
}
#endif
