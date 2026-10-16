// ===========================================================================
//  User_Setup.h -- TFT_eSPI cho board ESP32 2.8" 240x320 (CYD / clone)
//
//  Board: ESP32 + man 2.8" 240x320 **ST7789** (ban ILI9341 chi khac 1 dong driver)
//      LCD   ST7789  240x320   MOSI 13 / MISO 12 / SCLK 14 / CS 15 / DC 2 / RST -1
//      Den nen (BL): thu GPIO21 (CYD/ST7789 thuong dung) HOAC GPIO27
//                    (nhieu ban ST7789 dung 27) -> do bang -DCYDLCD_CYD_DEBUG
//      Touch XPT2046            T_DIN 32 / T_DO 39 / T_CLK 25 / T_CS 33 / T_IRQ 36
//
//  File nay duoc dua vao include path bang `-I${PROJECT_DIR}/tools/tft_setup`
//  (platformio.ini, env cydlcd_esp32): TFT_eSPI.h -> User_Setup_Select.h ->
//  `#include <User_Setup.h>` se lay file NAY thay vi ban goc trong thu vien.
//  Neu TFT_eSPI van doc ban goc, them vao build_flags:
//      -DUSER_SETUP_LOADED=1 -include tools/tft_setup_cyd/User_Setup.h
//
//  QUAN TRONG (touch): TFT_eSPI chi biet dieu khien XPT2046 TREN CUNG BUS SPI voi LCD
//  (chi khac chan CS). CYD lai di day rieng: T_CLK 25, T_DIN 32, T_DO 39, T_CS 33.
//  => `tft.getTouch()` KHONG doc duoc cam ung tren CYD. Vi vay:
//      - TOUCH_CS 33 van duoc dinh nghia o day (de co the bat lai tft.getTouch() bang
//        CYDLCD_CYD_TOUCH_USE_TFTESPI neu ban patch thu vien),
//      - nhung mac dinh ui_lvgl_port.cpp doc XPT2046 bang SPI mem (bit-bang) dung cac
//        chan T_CLK/T_DIN/T_DO/T_CS o duoi.
// ===========================================================================

// Thong tin cho example "Read_User_Setup"
#define USER_SETUP_INFO "CYDLCD_CYD_2432S028R_ST7789"

// Danh dau de ui_lvgl_port.cpp kiem tra xem file nay co duoc nap khong
#define CYDLCD_CYD_SETUP_LOADED 1

// ---------------------------------------------------------------------------
// Section 1: driver + kich thuoc
// ---------------------------------------------------------------------------
#define ST7789_DRIVER         // <-- man cua board nay la ST7789 (240x320)
                              //     (neu la ban ILI9341 thi doi thanh ILI9341_2_DRIVER)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// THU TU KENH MAU — do tren board that 2026-10-16:
// KHONG dinh nghia thi TFT_eSPI (TFT_Drivers/ST7789_Defines.h) chi lay BGR khi co
// CGRAM_OFFSET, ma CGRAM_OFFSET chi duoc dat cho cac co (240,240) (240,135) (280,240)
// (320,172) (320,170) (300,240)... — KHONG co co 240x320 cua CYD. Nen mac dinh la
// TFT_MAD_RGB, trong khi panel CYD 2432S028R nay la BGR => PHAI ep TFT_BGR.
// Trieu chung khi de mac dinh (RGB): nen #1E156E hien ra DO, do hien ra XANH DUONG,
// xanh la hien ra TIM (khi bat them dao mau thi thanh vang).
#define TFT_RGB_ORDER TFT_BGR

// CANH BAO: dong duoi VO TAC DUNG voi ST7789. TFT_Drivers/ST7789_Init.h:102 gui
// `writecommand(ST7789_INVON)` KHONG dieu kien va khong he doc TFT_INVERSION_ON/OFF.
// Tat dao mau that su = goi `invertDisplay(false)` SAU init, tuc bat co
// `-DCYDLCD_CYD_INVERT_OFF=1` trong cydlcd/platformio.ini (xem src/ui_port_cyd_st7789.cpp:429).
//#define TFT_INVERSION_ON

// ---------------------------------------------------------------------------
// Section 2: chan LCD
// ---------------------------------------------------------------------------
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1           // RST noi vao EN cua ESP32 (-1 = khong dung chan)
// DEN NEN (BL): dat -1 = TFT_eSPI KHONG dung toi chan den nen nua.
//   Tu 2026-10-14 FIRMWARE tu dieu khien den nen qua `CYDLCD_BL_PIN` (cydlcd_config.h), vi:
//     (1) TFT_eSPI chi biet MOT chan co dinh, trong khi CYD co 2 ban (BL = GPIO21 hoac GPIO27);
//     (2) chan den nen la ung vien cho CHAN CHON MUX SPI the SD (74HC4053) - neu TFT_eSPI giu
//         chan do o muc HIGH vinh vien thi khong the doi muc luc chay duoc (xem README muc 11.3).
//   TFT_eSPI.cpp:787 co san `if (TFT_BL >= 0)` nen -1 la an toan.
#define TFT_BL  -1
#define TFT_BACKLIGHT_ON HIGH

// CYD: LCD o HSPI (VSPI de danh cho khe the SD: CS 5 / MOSI 23 / MISO 19 / SCLK 18)
#define USE_HSPI_PORT

// ---------------------------------------------------------------------------
// Section 2b: cam ung XPT2046
//   TFT_eSPI chi dung duoc TOUCH_CS khi touch nam chung bus SPI voi LCD, nen tren CYD
//   phan doc cam ung do ui_lvgl_port.cpp tu lam (SPI mem) voi cac chan ben duoi.
//   Cac macro CYDLCD_CYD_TOUCH_* duoc ui_lvgl_port.cpp dung; doi o day neu board ban
//   khac chan.
// ---------------------------------------------------------------------------
#define TOUCH_CS 33                          // T_CS (giu lai de tuong thich TFT_eSPI)
#define CYDLCD_CYD_TOUCH_CS   33
#define CYDLCD_CYD_TOUCH_IRQ  36              // T_IRQ: muc THAP khi co cham
#define CYDLCD_CYD_TOUCH_DIN  32              // T_DIN  (MOSI tu ESP32)
#define CYDLCD_CYD_TOUCH_DO   39              // T_DO   (MISO ve ESP32, chan chi doc)
#define CYDLCD_CYD_TOUCH_CLK  25              // T_CLK

// ---------------------------------------------------------------------------
// Section 3: font cua TFT_eSPI
//   LVGL tu ve chu nen TFT_eSPI chi can font cho MAN HINH HIEU CHUAN CAM UNG
//   (ui_port_touch_calibrate): font 4 (26px) = ten goc, font 2 (16px) = huong dan/ket qua.
// ---------------------------------------------------------------------------
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

// ---------------------------------------------------------------------------
// Section 4: toc do SPI
// ---------------------------------------------------------------------------
#define SPI_FREQUENCY         40000000      // 40MHz: ILI9341 chay tot
#define SPI_READ_FREQUENCY    20000000
#define SPI_TOUCH_FREQUENCY    2500000      // XPT2046 toi da ~2.5MHz
