/**
 * sim.h -- API noi bo cua ban GIA LAP UI cydlcd tren PC (khong can board).
 *
 * Xem cydlcd/sim/README.md. Cac file:
 *   sim_port.cpp   : hien thuc hop dong `ui_port_*` (ui_lvgl.h) -> ve vao framebuffer roi xuat BMP,
 *                    cam ung = hang doi "cham" do kich ban bam vao.
 *   sim_stubs.cpp  : cac module phan cung (the SD, WiFi, log khong chan, UART Marlin gia...).
 *   sim_main.cpp   : main() - doc kich ban (page/hit/tap/shot/state...) va chay.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Dong ho AO (ms) cua sim. Tang trong sim_advance(). */
uint32_t sim_millis(void);
/** Cho dong ho AO chay `ms` mili giay (goi lv_tick_inc + lv_timer_handler trong luc do). */
void     sim_advance(uint32_t ms);
/** Chay UI them `ms` (dung khi can "de man hinh on dinh" truoc khi chup anh). */
void     sim_settle(uint32_t ms);

/** Xoa framebuffer ve mau nen. */
void sim_fb_clear(uint16_t rgb565);
/** Ghi framebuffer ra file BMP 24-bit. Tra ve false neu khong ghi duoc. */
bool sim_dump_bmp(const char *path);

/** Mau nen man hinh hien tai (lay o goc duoi-trai). */
uint16_t sim_bg_color(void);
/** So pixel khac mau nen trong vung - dung de biet trang co VE GI khong (trang trang = loi). */
int  sim_ink_count(int x0, int y0, int x1, int y1, uint16_t bg);
/** Tach cac nhom cot "co muc" trong dai y (toi da max_runs nhom). Tra ve SO nhom. */
int  sim_ink_runs(int x0, int y0, int x1, int y1, int min_gap, int *starts, int *ends, int max_runs);

/** Xep 1 lan CHAM vao hang doi (se duoc "tha" ra o cac vong doc indev ke tiep). */
void sim_tap(int x, int y);
/** So lan cham con lai trong hang doi (de kich ban biet da bam xong chua). */
int  sim_tap_pending(void);

#ifdef __cplusplus
}
#endif
