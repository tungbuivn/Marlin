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
// Do hieu nang (`UI PERF`): thoi gian `sd_alive()` (= `SD.exists("/")`, cham vao the) va `clean_dir()` trong
// `cyd_sd_task()`, roi XOA bo dem. Dung khi man hinh cham ma phan ve (LVGL/SPI) lai nhanh.
void cyd_sd_perf_report(void);

// UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15: "neu Marlin request thi huy access ben phia LCD, tru truong
// hop dang upload"): true = Marlin dang dung the (co request sector trong 400 ms vua qua) VA khong co phien
// upload -> MOI thao tac the CUA MAN HINH phai nhuong (bo qua/thu lai sau, vong quet thu muc thi thoat giua
// duong). Dung cho cac ham liet ke/xoa trong cyd_sd.cpp va cho trang web `/files`.
bool cyd_sd_yield_to_marlin(void);

// Trang thai thao tac THE cuoi cung cua MAN HINH - de phan biet "BAN vi Marlin dang doc/ghi the SD" (=> THU LAI
// SAU, khong phai loi) voi "loi that" (y kien nguoi dung 2026-10-15: *"thao tac gi fail thi cu thu lai sau,
// nhung chi khi lcd tra ve la co ai do dang doc hoac ghi sd"*).
enum { CYDLCD_SDOP_OK = 0, CYDLCD_SDOP_ERR = 1, CYDLCD_SDOP_BUSY = 2 };
uint8_t cyd_sd_last_op(void);

// The SD
bool cyd_sd_ready(void);        // cache - khong SPI, khong chan
bool cyd_sd_mount_now(void);    // mount NGAY (chan ~20-100 ms) - dung khi nguoi dung vua cam the
void cyd_sd_retry(void);        // xin mount lai o vong lap ke tiep (khong chan)

// ---- Ghi file tam theo KIEN TRUC PART (net_upload_http.cpp goi khi nhan duoc file) ----
// `name` = ten file DICH (nhu Marlin se nhin thay tren the). Dong thoi gian: khong chan (chi FATFS).
// `part` = so thu tu PHAN FILE (1..255): MOI part duoc ghi vao MOT FILE RIENG `/cydlcd_tmp/<ten>_NNN.tmp`
//   va duoc DONG NGAY khi het part (`cyd_sd_temp_close`) -> du lieu da xong nam chac tren the, part sau
//   loi/ngat khong lam mat part truoc. part = 1 = PHIEN MOI (don part/.tmp cu cua cung ten truoc).
// `size_hint` = tong so byte khach KHAI BAO (neu biet) -> kiem tra cho trong truoc khi nhan.
// Xong het request: `cyd_sd_finish_upload()` se doi ten thang (1 part) hoac GHEP cac part (>=2 part).
bool cyd_sd_temp_open(const char *name, uint32_t size_hint, uint8_t part = 1);
size_t cyd_sd_temp_write(const uint8_t *d, size_t n);
uint32_t cyd_sd_temp_close(void);       // DONG PART dang mo, tra ve TONG byte da ghi ca phien
void cyd_sd_temp_abort(void);           // dong + xoa MOI file part/.tmp cua phien (khi loi / bi ngat)
const char *cyd_sd_temp_path(void);     // duong dan file dang mo ("" neu khong co)
uint32_t cyd_sd_temp_bytes(void);       // TONG byte da ghi ca phien (cong don qua cac part)
uint8_t  cyd_sd_temp_parts_done(void);  // so PART da dong (da nam tren the)
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
 * BAO MAT TEN FILE cho cac endpoint WEB (upload + xoa). Whitelist: a-z A-Z 0-9 '.' '_' + cac ky tu trong
 * `CYDLCD_NAME_ALLOW_EXTRA` (mac dinh "-"); dong thoi chan: ten rong, > 64 ky tu, bat dau bang '.', chua
 * "..". Tra ve NULL neu ten HOP LE, hoac chuoi ly do (ASCII, de tra JSON) neu KHONG hop le.
 */
const char *cyd_sd_name_check(const char *name);
/**
 * LAM SACH ten file truoc khi kiem tra: bo phan duong dan (lay sau dau '/' cuoi cung), doi moi ky tu ngoai
 * whitelist thanh '_', gioi han do dai, va THEM ".gco" khi ten khong co duoi nao. Dung cho ten khach gui len
 * (field `name` cua multipart hoac ten file cua trinh duyet).
 */
void cyd_sd_name_clean(const char *in, char *out, size_t n);
/**
 * DUYET GOC THE cho TRANG WEB cua man hinh (net_upload_http.cpp): lay muc thu `idx` (0..) o goc the.
 * Ghi ten (KHONG co dau '/') vao `name` (toi da `name_n` byte), kich thuoc vao *size, thu muc vao *isdir.
 * Tra ve false khi het muc / loi / chua co the. CAC MUC HE THONG (CYDLCD_HIDE_DIRS: cydlcd_tmp, BTUP)
 * duoc AN (khong tinh vao idx) - cung quy tac voi man FILES tren LCD.
 */
bool cyd_sd_root_entry(uint8_t idx, char *name, size_t name_n, uint32_t *size, bool *isdir);
/**
 * Bao Marlin mount lai the (M22/M21) + xin lai danh sach file. CHAN ~800 ms (nuoi UART + LVGL).
 * Dung SAU khi xoa file qua trang web (giong `files_init_sd()` cua man FILES). Goi tu vong lap chinh
 * (HTTP handler) - KHONG goi tu callback LVGL.
 */
void cyd_sd_notify_marlin_remount(void);
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
