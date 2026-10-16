/**
 * syscalls_stm32.c -- 2 stub syscall newlib CHO BOARD STM32 LVGL (STM32F407).
 *
 * VI SAO CO FILE NAY (2026-10-16):
 *   Env `cydlcd_f407vgz_lvgl` loi LIEN KET:
 *       libc_nano.a(lib_a-fstatr.o): undefined reference to `_fstat'
 *       libc_nano.a(lib_a-isattyr.o): undefined reference to `_isatty'
 *   Framework ststm32 CO san 2 ham nay trong
 *   `libraries/SrcWrapper/src/syscalls.c` nhung deu la `__attribute__((weak))`, va object do
 *   CO mat tren dong link (`nm` thay `W _fstat`). Van thieu => LTO (`-flto` trong build_flags)
 *   loai bo dinh nghia WEAK khong duoc tham chieu truc tiep trong cung module.
 *   Dinh nghia lai o day voi linkage THUONG (strong) => thang ban weak cua framework, khong
 *   xung dot (strong > weak la hop le trong C).
 *
 *   Board CYD (ESP32) KHONG compile file nay: ESP-IDF da co san syscall cua no.
 *
 * 2 ham nay chi de newlib khong keu khi printf/snprintf can `struct stat` va `isatty`:
 *   _fstat  : bao "day la thiet bi ky tu" (khong phai file thuong)
 *   _isatty : bao "la terminal" -> newlib do line-buffering
 */
#include "cydlcd_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL && !defined(CYDLCD_BOARD_CYD)

#include <sys/stat.h>
#include <sys/types.h>

int _fstat(int fd, struct stat *st) {
  (void)fd;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int fd) {
  (void)fd;
  return 1;
}

#endif  /* STM32 + LVGL */
