// ===========================================================================
//  User_Setup.h -- TFT_eSPI cho ESP32-2432S028R "CYD" (2.8" ILI9341 + XPT2046)
//
//  Board: ESP32-2432S028R ("Cheap Yellow Display")
//      LCD   ILI9341  240x320   MOSI 13 / MISO 12 / SCLK 14 / CS 15 / DC 2 / RST -1 / BL 21
//      Touch XPT2046            T_DIN 32 / T_DO 39 / T_CLK 25 / T_CS 33 / T_IRQ 36
//
//  File nay duoc dua vao include path bang `-I${PROJECT_DIR}/tools/tft_setup`
//  (platformio.ini, env panel_esp32_cyd): TFT_eSPI.h -> User_Setup_Select.h ->
//  `#include <User_Setup.h>` se lay file NAY thay vi ban goc trong thu vien.
//  Neu TFT_eSPI van doc ban goc, them vao build_flags:
//      -DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h
//
//  QUAN TRONG (touch): TFT_eSPI chi biet dieu khien XPT2046 TREN CUNG BUS SPI voi LCD
//  (chi khac chan CS). CYD lai di day rieng: T_CLK 25, T_DIN 32, T_DO 39, T_CS 33.
//  => `tft.getTouch()` KHONG doc duoc cam ung tren CYD. Vi vay:
//      - TOUCH_CS 33 van duoc dinh nghia o day (de co the bat lai tft.getTouch() bang
//        PANEL_CYD_TOUCH_USE_TFTESPI neu ban patch thu vien),
//      - nhung mac dinh ui_lvgl_port.cpp doc XPT2046 bang SPI mem (bit-bang) dung cac
//        chan T_CLK/T_DIN/T_DO/T_CS o duoi.
// ===========================================================================

// Thong tin cho example "Read_User_Setup"
#define USER_SETUP_INFO "PANEL_CYD_2432S028R"

// Danh dau de ui_lvgl_port.cpp kiem tra xem file nay co duoc nap khong
#define PANEL_CYD_SETUP_LOADED 1

// ---------------------------------------------------------------------------
// Section 1: driver + kich thuoc
// ---------------------------------------------------------------------------
#define ILI9341_2_DRIVER      // driver ILI9341 thay the (CYD dung ban nay)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Neu mau bi dao (xanh <-> do): bo comment dong duoi
//#define TFT_RGB_ORDER TFT_BGR
// Neu trang thanh den / den thanh trang: thu lan luot 1 trong 2 dong duoi
//#define TFT_INVERSION_ON
//#define TFT_INVERSION_OFF

// ---------------------------------------------------------------------------
// Section 2: chan LCD
// ---------------------------------------------------------------------------
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1           // CYD noi RST vao EN cua ESP32 (-1 = khong dung chan)
#define TFT_BL   21           // den nen
#define TFT_BACKLIGHT_ON HIGH

// CYD: LCD o HSPI (VSPI de danh cho khe the SD: CS 5 / MOSI 23 / MISO 19 / SCLK 18)
#define USE_HSPI_PORT

// ---------------------------------------------------------------------------
// Section 2b: cam ung XPT2046
//   TFT_eSPI chi dung duoc TOUCH_CS khi touch nam chung bus SPI voi LCD, nen tren CYD
//   phan doc cam ung do ui_lvgl_port.cpp tu lam (SPI mem) voi cac chan ben duoi.
//   Cac macro PANEL_CYD_TOUCH_* duoc ui_lvgl_port.cpp dung; doi o day neu board ban
//   khac chan.
// ---------------------------------------------------------------------------
#define TOUCH_CS 33                          // T_CS (giu lai de tuong thich TFT_eSPI)
#define PANEL_CYD_TOUCH_CS   33
#define PANEL_CYD_TOUCH_IRQ  36              // T_IRQ: muc THAP khi co cham
#define PANEL_CYD_TOUCH_DIN  32              // T_DIN  (MOSI tu ESP32)
#define PANEL_CYD_TOUCH_DO   39              // T_DO   (MISO ve ESP32, chan chi doc)
#define PANEL_CYD_TOUCH_CLK  25              // T_CLK

// ---------------------------------------------------------------------------
// Section 3: font cua TFT_eSPI (chi can GLCD: LVGL tu ve chu, TFT_eSPI chi push pixel)
// ---------------------------------------------------------------------------
#define LOAD_GLCD

// ---------------------------------------------------------------------------
// Section 4: toc do SPI
// ---------------------------------------------------------------------------
#define SPI_FREQUENCY         40000000      // 40MHz: ILI9341 chay tot
#define SPI_READ_FREQUENCY    20000000
#define SPI_TOUCH_FREQUENCY    2500000      // XPT2046 toi da ~2.5MHz
