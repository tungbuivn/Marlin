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
 * LUONG: nhan file -> ghi /btup/<ten>.tmp tren the TF -> tra loi NGAY -> panel TU DAY sang ESP3D
 * (net_esp3d_push: dung lai luong pha 2 cua duong BT: POST multipart + M22/M21 + xoa file tam) ->
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

#ifdef __cplusplus
}
#endif
