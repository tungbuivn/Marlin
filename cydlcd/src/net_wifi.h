/**
 * net_wifi.h -- QUAN LY WiFi CUA MAN HINH CYD (net_wifi.cpp).
 *
 * VI SAO CO FILE NAY: phan WiFi tung nam trong module upload qua Bluetooth (da XOA 2026-10-14).
 * Tach rieng de WiFi khong phu thuoc tinh nang upload nao: icon WiFi o header + trang WiFi
 * settings van chay khi khong bat upload nao. Compile theo co `CYDLCD_WIFI_MGMT` (mac dinh 1 voi
 * board CYD). Chi MOT noi duy nhat duoc goi WiFi.begin/mode/disconnect: file nay.
 *
 * AN TOAN DRAM: header nay CHI khai bao ham (khong include WiFi.h/Preferences.h) nen nhanh UI
 * include no ma khong keo them thu vien vao build cua ui_lvgl.cpp.
 *
 * Khi `CYDLCD_WIFI_MGMT 0` (board khac / STM32): net_wifi.cpp chi con STUB - moi ham tra
 * 0 / "" / false nen UI van build va link duoc (ke ca `net_wifi_state()`).
 *
 * ---------------------------------------------------------------------------
 * HOP DONG CHO TRANG WiFi SETTINGS (nhanh UI):
 *   - Doc trang thai: `net_wifi_state/ssid/ip/rssi/configured/saved_ssid` -> CHI doc bien da
 *     cache: khong chan, khong cap phat, khong goi WiFi.* => goi duoc tu ui_lvgl_task() moi nhip.
 *   - `net_wifi_scan()` la ham DUY NHAT CHAN LAU (~2-4 s: Arduino scan dong bo). Goi tu task UI
 *     (khong phai ISR); trong luc quet man hinh se dung hinh - nen hien "DANG QUET..." TRUOC khi
 *     goi, va goi xong thi doc danh sach bang scan_ssid/scan_rssi.
 *   - `net_wifi_connect()` va `net_wifi_disconnect()` KHONG chan (chi goi WiFi.begin/disconnect +
 *     ghi NVS ~10-50 ms): tra ve ngay, tien do xem qua `net_wifi_state()` (1 = dang ket noi).
 *   - Sau khi doi cau hinh, UI chi can ve lai theo `net_wifi_state()`: khong can cho.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===========================================================================
// 1) TRANG THAI (doc cache - khong chan)
// ===========================================================================

// 0 = WiFi TAT / chua cau hinh / loi        -> icon XAM
// 1 = DANG ket noi (ke ca dang thu lai)      -> icon DO
// 2 = DA ket noi                            -> icon XANH
// Chi tra 2 khi WiFi.status() == WL_CONNECTED (khong phai chi vi da goi begin).
uint8_t net_wifi_state(void);

// SSID dang ket noi (hoac dang thu ket noi). "" neu chua biet.
const char *net_wifi_ssid(void);

// IP dang co (dang "192.168.0.42"); "" neu chua ket noi.
const char *net_wifi_ip(void);

// RSSI cua ket noi hien tai (dBm, vd -62). 0 = khong biet / chua ket noi.
int8_t net_wifi_rssi(void);

// true neu da co cau hinh (tu NVS hoac tu CYDLCD_WIFI_SSID_DEF trong cydlcd_config.h).
bool net_wifi_configured(void);

// SSID dang LUU trong NVS (man settings hien "dang luu: <ssid>"); "" neu chua luu gi.
const char *net_wifi_saved_ssid(void);

// ===========================================================================
// 2) QUET MANG (net_wifi_scan CHAN ~2-4 s)
// ===========================================================================

// Quet va LUU toi da CYDLCD_WIFI_SCAN_MAX mang (RSSI manh nhat truoc).
// !!! BLOCKING ~2-8 s (WiFi.scanNetworks dong bo) - goi tu task UI, hien "DANG QUET..." truoc.
// DO THAT 2026-10-14: khu vuc 30 mang -> **8,2 giay** (tran 150 ms/kenh). Trong suot thoi gian do
// man hinh CYD KHONG tra loi sector the SD cho Marlin => TUYET DOI khong goi khi Marlin dang in.
// UI nen dung bo ham ASYNC (net_wifi_scan_begin/poll) ben duoi de khong chan vong lap.
// Tra ve: so mang LUU DUOC (0..CYDLCD_WIFI_SCAN_MAX). Mang yeu hon suc chua se bi bo qua, nen
// danh sach co the ngan hon so mang thuc te trong khu vuc.
int net_wifi_scan(void);

// ---------------------------------------------------------------------------
// QUET KHONG CHAN (dung cho UI - lap vao nhip 400 ms, KHONG bao gio chan vong lap)
//   net_wifi_scan_begin(): 0 = da bat dau quet; -1 = dang quet roi (bo qua)
//   net_wifi_scan_poll() : -1 = dang quet | -2 = loi/qua han | >= 0 = XONG (so mang luu duoc)
//   net_wifi_scan_cancel(): huy phien dang quet (goi khi roi trang WIFI)
// Trong luc quet, vong lap van chay binh thuong (tra loi Marlin + web server).
// ---------------------------------------------------------------------------
int  net_wifi_scan_begin(void);
int  net_wifi_scan_poll(void);
void net_wifi_scan_cancel(void);
bool net_wifi_scanning(void);

// So o toi da cua danh sach (de UI ve vong lap cho an toan).
int net_wifi_scan_max(void);

// SSID mang thu i (i = 0..net_wifi_scan()-1). NULL neu i ngoai khoang.
const char *net_wifi_scan_ssid(int i);

// RSSI mang thu i (dBm). 0 neu i ngoai khoang.
int8_t net_wifi_scan_rssi(int i);

// Thoi gian cua lan quet cuoi (ms) - de UI biet du lieu cu hay moi. 0 = chua quet lan nao.
uint32_t net_wifi_scan_ms(void);

// ===========================================================================
// 3) KET NOI / NGAT (khong chan - xem ghi chu dau file)
// ===========================================================================

// Dat SSID/mat khau va BAT DAU ket noi. `save=true` -> ghi vao NVS (Preferences "wifi") de
// tu ket noi lai o lan boot sau; `save=false` -> chi dung cho phien nay (khong ghi flash).
// Tra ve true = da bat dau (cau hinh hop le); tien do xem net_wifi_state().
// Tu choi (false) khi: ssid rong / dai qua CYDLCD_WIFI_SSID_MAX / pass dai qua CYDLCD_WIFI_PASS_MAX.
bool net_wifi_connect(const char *ssid, const char *pass, bool save);

// Ngat ket noi va TAT WiFi (tra ~40-50 KB heap). `forget=true` -> xoa cau hinh trong NVS luon.
// Khong chan (WiFi.disconnect + WiFi.mode(WIFI_OFF) ~ vai chuc ms).
void net_wifi_disconnect(bool forget);

// ===========================================================================
// 4) VONG DOI - main.cpp goi (khong can thiet cho nhanh UI)
// ===========================================================================

// Doc NVS + (neu CYDLCD_WIFI_AUTOSTART) bat dau ket noi. Goi 1 lan trong setup().
void net_wifi_begin(void);

// Cap nhat cache trang thai + tu ket noi lai khi mat (khong chan). Goi lien tuc trong loop().
void net_wifi_task(void);

// ===========================================================================
// 5) API NOI BO (dung trong net_wifi.cpp + net_upload_http.cpp)
// ===========================================================================

// Bat STA + begin() voi cau hinh hien co (tu NVS, khong thi CYDLCD_WIFI_SSID_DEF/PASS_DEF).
// KHONG chan (chi begin). false = thieu RAM (cua an toan) hoac chua co cau hinh.
bool net_wifi_up(void);

// Tat WiFi NGAY va NGUNG tu ket noi lai (dung khi nguoi dung bam TAT).
void net_wifi_down(void);

// Cap nhat cache + tu ket noi lai (khong chan).
void net_wifi_poll(void);

// true = dang ket noi duoc (WiFi.status() == WL_CONNECTED).
bool net_wifi_connected(void);

// Cua an toan RAM truoc khi bat WiFi STA.
bool net_wifi_ram_ok(void);

// Bat/tat WiFi modem sleep (tat khi dang nhan file cho nhanh; xong bat lai de tiet kiem dien).
void net_wifi_ps(bool power_save);

#ifdef __cplusplus
}
#endif
