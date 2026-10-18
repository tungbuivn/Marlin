/**
 * ui_overlay.h -- LOP PHU toan man hinh cua man hinh CYD (tien do upload / thong bao ket qua) + khoa input.
 *
 * DA LUONG (2026-10-15, xem cydlcd/docs/ke-hoach-da-luong.md - B3):
 *   LVGL chi duoc goi tu MOT task duy nhat (task `ui`). Nhung lop phu nay duoc BAT/TAT tu duong upload
 *   (task `net`, core 0) va tu console -> nen o day tach lam 2 nua:
 *     1) Cac ham `ov_*` / `ui_lock()` (goi duoc tu MOI task): CHI ghi TRANG THAI (khong goi LVGL).
 *     2) `ui_overlay_update()`: CHI task `ui` goi (trong nhip cua no) -> tao/cap nhat/huy widget LVGL
 *        theo trang thai do.
 *   => Duong upload khong bao gio cham vao LVGL (khong con nguy co 2 task cung ve).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---- Nua 1: goi duoc tu MOI task (chi ghi trang thai, khong LVGL) ----
// Khoa/mo input device (touch): true = khoa (dang co viec cham chan UI). Task `ui` se ap dung.
void ui_lock(bool lock);
// Hien lop phu (tao o lan cap nhat ke tiep cua task `ui`). Tra true = da nhan yeu cau.
bool ov_create_full(void);
// An + huy lop phu.
void ov_destroy(void);
// Dat 2 dong chu cua lop phu (`state` = dong to, `info` = dong nho). ok_col = mau xanh (true) / do (false).
void ov_set(const char *state, const char *info, bool ok_col);
// Thong bao ket qua (giu nguyen lop phu, chi doi chu).
void ov_note(const char *state, bool ok);
// Tien do: tu tinh `%  KB  KB/s` (toi da 5 lan/s) roi goi ov_set(). Khong cham LVGL.
void ov_progress(const char *state, uint32_t done, uint32_t total);
void ov_test_show(void);      // 2026-10-17: hien lop phu upload 40% (de chup anh)
// Tick tinh toc do (goi tu duong ghi the).
void rate_tick(uint32_t done);

// ---- Nua 2: CHI task `ui` goi ----
// Ap dung trang thai (khoa input / tao-cap nhat-huy lop phu). Goi 1 lan moi nhip cua task `ui`.
void ui_overlay_update(void);
