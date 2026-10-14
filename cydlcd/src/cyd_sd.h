/**
 * cyd_sd.h -- TANG THE SD CUA MAN HINH CYD (khe microSD tren board CYD la noi DUY NHAT co the).
 *
 * The SD nam o khe cua man hinh CYD. Marlin (board chinh) KHONG doc SPI the nua ma hoi man hinh
 * tung sector 512 B qua UART (xem cyd_sd_raw_read/raw_write + cydlcd_link.cpp). Cung the do,
 * web server tren man hinh ghi file upload vao /cydlcd_tmp/<ten>.tmp roi doi ten vao GOC the.
 *
 * KHONG con lien quan gi toi ESP3D: duong POST file len module ESP3D da bi XOA (2026-10-14) cung
 * voi duong upload qua Bluetooth.
 *
 * HAI CHE DO (co CYDLCD_SD_SHARED trong cydlcd_config.h):
 *   CYDLCD_SD_SHARED 0 (MAC DINH): the thuoc MAN HINH. Man hinh mount that su, Marlin hoi sector qua
 *     UART. Day la che do dang dung.
 *   CYDLCD_SD_SHARED 1 (duong lui, code con giu): the DUNG CHUNG voi Marlin qua MUX SPI 74HC4053 -
 *     man hinh chi gianh bus khi nhan file roi tra bus + M22/M21.
 *
 * Cac ham `cyd_sd_*` (tru cyd_sd_finish_upload) deu KHONG chan - goi duoc tu vong lap chinh/UI.
 * `cyd_sd_finish_upload()` CHAN (co ve lop phu + nuoi UART) - goi tu vong lap chinh (task UI).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Vong doi (main.cpp / net_upload_http.cpp goi)
void cyd_sd_begin(void);        // mount the (leo thang toc do) + don file .tmp mo coi
void cyd_sd_task(void);         // theo doi the nong (rut ra / cam vao) - khong chan
void cyd_sd_log(void);          // 1 dong log 1 Hz (the, WiFi, phien upload, heap, bo dem sector)

// The SD
bool cyd_sd_ready(void);        // cache - khong SPI, khong chan
bool cyd_sd_mount_now(void);    // mount NGAY (chan ~20-100 ms) - dung khi nguoi dung vua cam the
void cyd_sd_retry(void);        // xin mount lai o vong lap ke tiep (khong chan)

// ---- Ghi file tam /cydlcd_tmp/<ten>.tmp (net_upload_http.cpp goi khi nhan duoc file) ----
// `name` = ten file DICH (nhu Marlin se nhin thay tren the). Dong thoi gian: khong chan (chi FATFS).
// `same_request` = true khi day la PHAN FILE thu >=2 cua CUNG mot request multipart: khi do KHONG duoc
// mo lai file tam (mo lai = xoa sach du lieu da nhan) ma phai GHI TIEP vao file dang mo.
bool cyd_sd_temp_open(const char *name, uint32_t size_hint, bool same_request = false);
size_t cyd_sd_temp_write(const uint8_t *d, size_t n);
uint32_t cyd_sd_temp_close(void);       // dong file, tra ve so byte da ghi
void cyd_sd_temp_abort(void);           // dong + xoa file tam (khi loi / bi ngat)
const char *cyd_sd_temp_path(void);     // duong dan file tam dang mo ("" neu khong co)
uint32_t cyd_sd_temp_bytes(void);       // so byte da ghi vao file tam
const char *cyd_sd_temp_err(void);      // ly do loi cua phien dang chay ("" neu khong co)
bool cyd_sd_printer_busy(void);         // Marlin dang in? (de tu choi phien upload moi)

// ---- Chot phien upload: doi ten /cydlcd_tmp/<ten>.tmp -> /<ten> + bao Marlin mount lai (CHAN) ----
// Tra true = OK. Khi false: `err` (co the NULL) nhan ly do, *written = so byte da ghi vao the.
bool cyd_sd_finish_upload(const char *name, uint32_t size, uint32_t *written, char *err, size_t errn);

// ---- DOC/GHI SECTOR THO + XOA FILE/THU MUC (man hinh la "may chu the SD" cho Marlin) ----
// cydlcd_link.cpp goi cyd_sd_raw_read() khi Marlin hoi 1 sector (frame CYDLCD_F_SDREQ) roi tra ket qua
// qua CYDLCD_F_SDDATA. Xem cydlcd/README.md muc 12.
bool cyd_sd_raw_read(uint32_t sector, uint8_t *dst);
bool cyd_sd_raw_write(uint32_t sector, const uint8_t *src);
bool cyd_sd_delete(const char *name);   // xoa file o GOC the + bao Marlin mount lai
/**
 * Dem so MUC CON cua 1 thu muc o goc the ("." va ".." khong tinh). Tra -1 khi khong mo duoc thu muc
 * (khong co the / khong phai thu muc / loi FATFS). KHONG xoa gi - dung de bao truoc "con N muc con".
 */
int cyd_sd_dir_count(const char *name);
/**
 * Xoa 1 THU MUC RONG o goc the (FATFS chi cho xoa thu muc trong). Tra true = da xoa.
 * false + *not_empty = true khi thu muc CON muc con (khong xoa, khong mat du lieu gi).
 * `not_empty` co the NULL (chi can biet xoa duoc hay khong).
 */
bool cyd_sd_delete_dir(const char *name, bool *not_empty);
/**
 * CHAN DOAN (lenh `UI WTEST <KB>` tren COM7): ghi thu `kb` KB vao file tam tren the bang CHINH duong
 * cua upload nhung KHONG qua WiFi - de tach nguyen nhan "ghi the TF loi" (the/FATFS hay duong upload).
 * Xoa file tam sau khi xong. Tra true = ghi du.
 */
bool cyd_sd_write_test(uint32_t kb);
/**
 * CHAN DOAN: liet ke cac muc o GOC the bang chinh FATFS cua MAN HINH - ten DAY DU (khong bi cat o 32
 * byte nhu truong `name` cua giao thuc) + co thu muc hay khong + kich thuoc. Dung khi ten hien tren
 * man hinh khac ten THAT tren the (=> lenh `M23 <ten>` se KHONG mo duoc file).
 */
void cyd_sd_log_dir(const char *path);   // path rong/NULL -> goc the

// Ket qua phien upload gan nhat (de /status tra JSON, khong chan)
const char *cyd_sd_last_result(void);   // "DONE 12345" / "ERR ..." / ""
uint32_t    cyd_sd_last_written(void);  // so byte da ghi vao the trong phien gan nhat

#ifdef __cplusplus
}
#endif
