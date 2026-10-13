/**
 * net_esp3d.h -- LOI DUNG cho duong upload qua WiFi: the TF + hoan tat file + M22/M21.
 *
 * HAI CHE DO (co PANEL_SD_SHARED trong panel_config.h, xem README muc 11):
 *   PANEL_SD_SHARED 1 (MAC DINH tu 2026-10-14): THE SD DUNG CHUNG voi Marlin qua MUX SPI
 *     (74HC4053). Panel chi GIANH bus khi nhan file (khong in), ghi /btup/<ten>.tmp roi DOI TEN
 *     thanh /<ten>, tra bus cho Marlin + M22/M21. KHONG dung mang, KHONG can ESP3D.
 *   PANEL_SD_SHARED 0 (kieu cu): POST file len module ESP3D roi M22/M21 (xem net_upload_http.cpp).
 *
 * VI SAO KHONG TAI DUNG panel_bt_upload.cpp: khi `PANEL_BT_UPLOAD 0` thi file do chi con stub
 * (Bluedroid bi loai khoi build de tiet kiem ~100 KB heap + ~600 KB flash). Vi vay duong WiFi co
 * phan loi rieng nay. HAI DUONG KHONG BAO GIO CHAY CUNG LUC: panel_config.h ep
 * PANEL_WIFI_UPLOAD=0 khi PANEL_BT_UPLOAD=1 (2 radio khong the cung ton tai trong heap).
 *
 * Cac ham `net_esp3d_*` (trư push) deu KHONG chan - goi duoc tu vong lap chinh/UI.
 * `net_esp3d_push()` CHAN (co ve lop phu + nuoi UART) - chi goi tu vong lap chinh (task UI),
 * KHONG duoc goi tu callback cua WebServer... that ra duoc: WebServer chay trong cung task
 * (server.handleClient() o loop()), nhung khi do lop phu/khoa UI se hien giua luc tra loi HTTP.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Vong doi (main.cpp / net_upload_http.cpp goi)
void net_esp3d_begin(void);     // mount the TF (leo thang toc do) + don file .tmp mo coi
void net_esp3d_task(void);      // theo doi the TF nong (rut ra / cam vao) - khong chan
void net_esp3d_log(void);       // 1 dong log 1 Hz (the TF, WiFi, phien upload, heap)

// The TF
bool net_esp3d_sd_ready(void);  // cache - khong SPI, khong chan
bool net_esp3d_sd_mount_now(void);   // mount NGAY (chan ~20-100 ms) - dung khi nguoi dung vua cam the
void net_esp3d_sd_retry(void);       // xin mount lai o vong lap ke tiep (khong chan)

// ---- Ghi file tam /btup/<ten>.tmp (net_upload_http.cpp goi khi nhan duoc file) ----
// `name` = ten file DICH (nhu tren the SD cua Marlin). Dong thoi gian: khong chan (chi FATFS).
bool net_esp3d_temp_open(const char *name, uint32_t size_hint);
size_t net_esp3d_temp_write(const uint8_t *d, size_t n);
uint32_t net_esp3d_temp_close(void);   // dong file, tra ve so byte da ghi
void net_esp3d_temp_abort(void);       // dong + xoa file tam (khi loi / bi ngat)
const char *net_esp3d_temp_path(void); // duong dan file tam dang mo ("" neu khong co)
uint32_t net_esp3d_temp_bytes(void);   // so byte da ghi vao file tam
bool net_esp3d_printer_busy(void);     // Marlin dang in? (de tu choi phien upload moi)

// ---- Day file tam len ESP3D + M22/M21 + xoa file tam (CHAN - co lop phu + khoa UI) ----
// Tra true = OK. Khi false: `err` (co the NULL) nhan ly do, *posted = so byte da day duoc.
bool net_esp3d_push(const char *name, uint32_t size, uint32_t *posted, char *err, size_t errn);

// Ket qua phien upload gan nhat (de /status tra JSON, khong chan)
const char *net_esp3d_last_result(void);   // "DONE 12345" / "ERR ..." / ""
uint32_t    net_esp3d_last_posted(void);

#ifdef __cplusplus
}
#endif
