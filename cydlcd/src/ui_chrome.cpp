/**
 * ui_chrome.cpp -- "chrome" cua UI LVGL (man hinh CYD): dong tieu de, thanh day phan trang,
 * icon trang thai (WiFi + the SD) va nut HOME/BACK.
 *
 * Tach tu ui_lvgl.cpp (chang 1/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham/bien dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom:
 *   - link_ok()/link_state_now(): trang thai link voi Marlin (PUSH cydlcd hoac TRAM tbt).
 *   - find_focus_in_band()/page_scroll(): phan trang noi dung man dang mo.
 *   - bot_sync(): bat/tat + cap nhat thanh day [PREV][TRANG x/y][NEXT].
 *   - wifi_icon_font/mk_wifi_icon/wifi_glyph_apply/wifi_icon_mark: icon WiFi (font icon hoac
 *     cham tron duong lui) o dong tieu de va tren man STATUS.
 *   - status_icons_create()/wifi_rssi_mark()/status_sd_icon_mark(): icon WiFi + chu dBm tren man
 *     STATUS (khe phai cua hang nhiet do) + mau icon the SD 64x64.
 *   - build_chrome(): dung dong tieu de + thanh day mot lan luc khoi dong.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu (xem ui_lang.h)

#include <stdio.h>                  // snprintf (chu dBm canh icon WiFi)

#if CYDLCD_USE_LVGL

// ---------------------------------------------------------------------------
//  Bien chi dung trong file nay -> VAN giu `static`
// ---------------------------------------------------------------------------
// Thanh day (sticky nhu dong tieu de): [PREV] [TRANG x/y] [NEXT]. CHI hien khi noi dung
// cua man dang mo BI TRAN theo chieu doc (xem bot_sync()); man nao gon thi giu nguyen bo cuc 206 px.
static lv_obj_t  *s_bot, *s_bot_prev, *s_bot_next, *s_bot_info;
// Nut BACK o goc phai dong tieu de: GIU CON TRO de con doi duoc CHU khi doi ngon ngu (2026-10-15 -
// xem chrome_lang_retext). Truoc day nut nay tao "truc tiep" khong luu con tro.
static lv_obj_t  *s_btn_back;
static bool       s_bot_shown = false;      // trang thai hien tai cua s_bot (tranh goi an/hien vo ich)
// Trang thai hien/an cua 2 nut PREV/NEXT: bit0 = PREV, bit1 = NEXT. 0xFF = chua set lan nao.
// Nut nao KHONG the di theo huong do thi AN HAN (khong lam mo), va chi doi CO khi trang thai doi.
static uint8_t    s_bot_nav = 0xFF;


// Trang thai hien/an cua thanh day (cho chan doan bo cuc: `UI BOX` tren COM7 - xem ui_lvgl.cpp).
bool bot_shown(void) { return s_bot_shown; }

// "Co link voi Marlin" - CO HAI giao thuc LOAI TRU nhau (PUSH: cydlcd.linked / TRAM: tbt.linked)
// nen phai xet CA HAI. Dung ham nay o MOI cho hien thi link (thanh tren + trang STATUS).
// Chinh sach hien thi (2026-10-13, theo yeu cau): chi bao link chi nam o
//   1) THANH TREN (moi man), va 2) dong trang thai + khung XYZ cua trang STATUS (home).
// Cac man khac KHONG hien lai nua (truoc day man TRAMMING va MENU cung hien -> nhieu cho mau thuan).
bool link_ok(void) { return cydlcd.linked || tbt.linked; }

// Trang thai link 3 muc - "co byte toi" KHONG du de coi la link tot (co the la byte rac do sai baud/
// nhieu/day loi), nhung no phan biet duoc 2 kieu hong khac nhau:
//   0 = NO LINK  : khong co byte nao  -> dut day / sai cong / board tat
//   1 = LINK OK  : doc duoc dong "TBT ..." hop le
//   2 = LINK ERR : CO byte toi nhung khong thanh dong hop le -> sai baud / sai khung / nhieu
uint8_t link_state_now(void) {
  if (link_ok()) return 1;
  return tbt.rx_active ? 2 : 0;
}
// ===========================================================================
//  Thanh day: phan trang noi dung man dang mo (PREV / TRANG x/y / NEXT)
// ===========================================================================
// CHIEU (da kiem chung bang ma nguon LVGL 8.4, khong phai doan):
//   lv_obj_get_scroll_top(o) = -o->spec_attr->scroll.y            (lv_obj_scroll.c:128)
//   lv_obj_scroll_to_y(o, y) dat scroll.y = -y  =>  scroll_top = y
//   lv_obj_scroll_by(o, 0, dy) cong thang vao scroll.y (lv_obj_scroll.c:334 dat anim
//       tu -sy den -sy+dy, anim ghi scroll.y = v)  =>  scroll_top_moi = scroll_top_cu - dy
//   => muon di XUONG (scroll_top TANG) phai truyen dy = -vis. Code cu truyen +vis cho NEXT
//      nen scroll_top GIAM: NEXT lai di VE DAU danh sach (dung nhu nguoi dung bao
//      "next ... cuon nguoc"). Da sua: dung lv_obj_scroll_to_y() voi dich TINH TRUOC.
//   PREV = ve phia TRUOC (dau danh sach, scroll_top giam) | NEXT = ve phia SAU (scroll_top tang).
// Ngoai ra lv_obj_scroll_by() la ban KHONG kep bien (co the tran ra khoang trang o 2 dau);
// lv_obj_scroll_to_y() di qua ban bounded nen tu dung lai o dau/cuoi.
//
// FOCUS: lv_btn (moi dong menu deu la lv_btn) co LV_OBJ_FLAG_SCROLL_ON_FOCUS (lv_btn.c:67) va
// LVGL tu goi lv_obj_scroll_to_view_recursive() khi vat nhan duoc su kien FOCUSED
// (lv_obj.c:800-803). Neu sau khi cuon ma con tro encoder VAN o dong cua trang CU thi
// lan di chuyen focus ke tiep se keo ca trang VE cho thay dong do -> nhin nhu vua cuon nguoc.
// Vi vay sau khi cuon phai chuyen focus sang vat focusable DAU TIEN nam trong vung nhin MOI.
// Tim vat focusable DAU TIEN (theo thu tu cay) dang GIAO voi vung nhin MOI [band_lo, band_hi]
// (toa do tuyet doi tren man).
// "Focusable" = CO TRONG GROUP, chu KHONG phai co LV_OBJ_FLAG_CLICK_FOCUSABLE: lv_group_focus_obj()
// chi doi hoi vat nam trong group (no duyet danh sach obj_ll cua group - lv_group.c:235), va co
// vat duoc them vao group ma khong co co do (vd bang SETTINGS: `lv_group_add_obj(s_group, me_table)`).
static lv_obj_t *find_focus_in_band(lv_obj_t *root, lv_coord_t band_lo, lv_coord_t band_hi) {
  const uint32_t n = lv_obj_get_child_cnt(root);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(root, i);
    if (!c || lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    if (s_group && lv_obj_get_group(c) == s_group) {
      // lv_obj_get_coords() tra toa do TUYET DOI tren man (LVGL da cong scroll) - dung luon,
      // khong can biet vat long bao nhieu cap. band_* cung la toa do tuyet doi.
      lv_area_t a;
      lv_obj_get_coords(c, &a);
      if (a.y2 > band_lo && a.y1 < band_hi) return c;     // giao voi vung nhin MOI
    }
    lv_obj_t *r = find_focus_in_band(c, band_lo, band_hi);
    if (r) return r;
  }
  return NULL;
}

// Cuon noi dung man dang mo them 1 "trang" (dung lai o 2 dau). dir > 0 = NEXT (xuong cuoi),
// dir < 0 = PREV (ve dau). Dung chung cho MOI man con (khong rieng MENU).
void page_scroll(const int8_t dir) {
  lv_obj_t *p = s_page[s_page_act];
  if (!p || s_page_act == PG_STATUS) return;
  const lv_coord_t vis = lv_obj_get_height(p) - 2 * PAGE_PAD;
  if (vis <= 0) return;

  const lv_coord_t top = lv_obj_get_scroll_top(p);
  const lv_coord_t all = top + lv_obj_get_scroll_bottom(p);   // tong doan cuon duoc (co dinh)
  lv_coord_t want = top + ((dir > 0) ? vis : -vis);
  if (want < 0)   want = 0;                                   // da o dau -> khong cuon them
  if (want > all) want = all;                                 // da o cuoi -> khong cuon them
  if (want == top) return;

  lv_obj_scroll_to_y(p, want, LV_ANIM_ON);                    // scroll_top = want (chinh xac)

  // Vat nao dang o trong vung nhin MOI? Tinh theo toa do NOI DUNG (khong doi theo scroll):
  //   content_ofs = coords.y1 - p->coords.y1 + scroll_top_hien_tai
  // Sau khi cuon, vat nam trong khung khi content_ofs thuoc [want, want + chieu cao trang].
  // (Phai tinh tay vi scroll dang chay ANIMATION nen coords chua cap nhat.)
  if (!s_group) return;
  const lv_coord_t shift = top - want;                        // doi coords CU -> coords SAU khi cuon
  lv_area_t pa;
  lv_obj_get_coords(p, &pa);
  const lv_coord_t band_lo = pa.y1 - shift;
  const lv_coord_t band_hi = pa.y2 - shift;
  lv_obj_t *nf = find_focus_in_band(p, band_lo, band_hi);
  if (!nf || nf == lv_group_get_focused(s_group)) return;     // khong tim duoc -> giu nguyen focus

  // lv_group_focus_obj() se ban FOCUSED -> LVGL goi lv_obj_scroll_to_view_recursive(nf) theo
  // toa do CU cua nf, tuc la keo trang NGUOC LAI. Tam bo co roi bat lai de tranh.
  const bool sof = lv_obj_has_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  if (sof) lv_obj_clear_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_group_focus_obj(nf);          // => on_focus_changed() cap nhat s_focus_prev + ve lai khung
  if (sof) lv_obj_add_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}
// Bat/tat thanh day theo noi dung THUC TE cua man dang mo:
//  - man nao noi dung da nam gon trong vung nhin => KHONG hien thanh day (giu nguyen bo cuc 206 px nhu cu)
//  - man nao tran => thu nho vung noi dung di BOT_H va hien thanh day de phan trang
// Goi o CUOI show_page() (luc trang da HIEN) va dinh ky trong ui_lvgl_task() (~400 ms).
// `remeasure = true`: CHI goi khi VUA VAO man (xem show_page). Luc do trang dang o dau danh sach
// (scroll = 0) nen co the tam tra chieu cao ve CONT_H de do, roi thu lai - khong lam nhay vi tri cuon.
// `remeasure = false`: chi cap nhat nhan/mo nut, KHONG doi chieu cao trang: doi chieu cao se lam
// LVGL KEP `scroll_y` ve muc toi da cu => moi 400 ms trang tu nhay ve dau, bam NEXT khong giu duoc.
void bot_sync(const bool remeasure) {
  if (!s_bot) return;
  const uint8_t p = s_page_act;
  lv_obj_t *pg = s_page[p];
  const bool chrome = (p != PG_STATUS);          // STATUS khong co header/thanh day
  if (!pg || !chrome) {
    // STATUS dung kin 320x240 -> khong doi kich thuoc trang, chi dam bao thanh day dang AN
    if (s_bot_shown) s_bot_shown = false;
    lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (remeasure) {
    // PHAI do khi trang dang HIEN (LVGL bo qua tinh layout cho vat HIDDEN).
    // Do o CHIEU CAO DAY DU (CONT_H) truoc: neu noi dung da gon trong 206 px thi khong can
    // thanh day; do o 192 px thi noi dung ngan cung bi coi la "tran" gia.
    lv_obj_scroll_to_y(pg, 0, LV_ANIM_OFF);            // vao man -> xem tu dau
    if (lv_obj_get_height(pg) != CONT_H) {
      lv_obj_set_height(pg, CONT_H);
      lv_obj_update_layout(pg);
    }
    // LVGL cong ca pad_bottom vao `scroll_bottom` => noi dung vua khop 206 px van bao tran ~4 px.
    // Vi vay phai so voi nguong PAGE_PAD (khong dung > 0), neu khong cac man dang gon se bi hien thanh day.
    const lv_coord_t ovf = lv_obj_get_scroll_top(pg) + lv_obj_get_scroll_bottom(pg);
    const bool need = (ovf > PAGE_PAD);
    if (need) lv_obj_set_height(pg, CONT_H - BOT_H);   // 192: nhuong 22 px cho thanh day
    lv_obj_update_layout(pg);
    if (need != s_bot_shown) {
      s_bot_shown = need;
      if (need) lv_obj_clear_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
      else      lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
    }
  }

  const bool need = s_bot_shown;
  if (!need) return;

  // nhan "TRANG x/y" + AN HAN nut nao khong con gi de cuon (truoc day chi lam mo bang OPA_40 ->
  // nguoi dung van tuong la bam duoc). Thanh day la flex ROW [PREV][nhan grow=1][NEXT]: LVGL bo
  // qua vat HIDDEN khi xep flex nen nhan tu gian ra, con lai van nam sat bien (trai/phai).
  // `top + bot` = TONG doan cuon duoc (khong doi theo vi tri dang dung) nen so trang dem theo
  // moc nhay tung trang, khop voi trang thai hien/an cua 2 nut (o dau: top=0, o day: bot=0).
  const lv_coord_t vis = lv_obj_get_height(pg) - 2 * PAGE_PAD;
  const lv_coord_t top = lv_obj_get_scroll_top(pg), bot = lv_obj_get_scroll_bottom(pg);
  const int npg = (vis > 0) ? (1 + (int)((top + bot + vis - 1) / vis)) : 1;
  const int cur = (vis > 0) ? (1 + (int)((top       + vis - 1) / vis)) : 1;
  char b[24]; snprintf(b, sizeof(b), T(UI_T_CHROME_PAGE_OF), cur, npg);
  set_text(s_bot_info, b);

  // Chi doi CO khi trang thai doi (bit0 = PREV hien, bit1 = NEXT hien) -> khong relayout moi 400 ms.
  // nav == 0 (ca 2 cung khong the di) khong the toi day: thanh day chi hien khi noi dung TRAN
  // (ovf > PAGE_PAD), ma khi do thi top > 0 hoac bot > 0. Van chan them cho chac.
  const uint8_t nav = (uint8_t)((top > 0 ? 1U : 0U) | (bot > 0 ? 2U : 0U));
  if (nav && nav != s_bot_nav) {
    s_bot_nav = nav;
    if (nav & 1U) lv_obj_clear_flag(s_bot_prev, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(s_bot_prev, LV_OBJ_FLAG_HIDDEN);
    if (nav & 2U) lv_obj_clear_flag(s_bot_next, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(s_bot_next, LV_OBJ_FLAG_HIDDEN);
  }
}
// ===========================================================================
//  Icon trang thai WiFi: 1 icon o HEADER (thanh tieu de) + 1 icon + chu dBm tren man STATUS
//  (nam trong KHE PHAI cua hang nhiet do - xem ST_WIFI_X).
//   wifi_state_now(): 0 = WiFi tat / chua cau hinh / khong co module
//                     1 = DANG ket noi (ke ca dang thu lai) -> DO
//                     2 = DA ket noi                         -> XANH
//   sd_state_now()  : 0 = chua co module (weak NULL) -> XAM, 1 = khong co the/loi -> DO,
//                     2 = the dang dung duoc -> XANH
//  Font: CA HAI noi dung ban NHO (gfx4_font_icons_get; ban to 30 px da xoa 2026-10-14).
//  Thieu font -> CHAM TRON 12x12 (build van chay).
// ===========================================================================
// ---------------------------------------------------------------------------
//  Be rong CO DINH cua cac vat tren thanh tieu de (xem bang tinh o build_chrome):
//    LINK_LBL_W : o ten may / trang thai link. Phai >= 64 px vi "LINK ERR" = 8 ky tu mono x 8 px;
//                 ten may dai hon thi TU CUON trong o (khong day nut nao ra ngoai).
//    TITLE_W    : o TEN MAN. Ten dai nhat la "FEED/FLOW" (~81 px voi Montserrat 14) nen 84 px la
//                 du; o nay CO DINH nen do dai ten khong lam xo lech thanh tieu de.
//    HOME_BTN_W : nut icon ngoi nha (glyph 12x10, xAdvance 14) - 26 x 20 nhu 2 nut II/[].
// ---------------------------------------------------------------------------
#define LINK_LBL_W   64                // truoc la 78 (nhuong cho cho nut HOME + o ten man)
#define TITLE_W      84
#define HOME_BTN_W   26
// Duong lui (khong co font icon, hoac vang ban to): CHAM TRON 12x12. Dinh nghia VO DIEU KIEN vi
// mk_wifi_icon() kiem tra font luc CHAY (co the co ban nho ma khong co ban to).
#define WIFI_ICON_SZ 12
// "LINK ERR" chi can 8x8 = 64 px (font mono advance 8) nen 78/80 px van hien du; ten may dai hon
// thi tu cuon vong nhu truoc (xem mk_label_w).
static lv_obj_t  *s_wifi;                  // glyph o HEADER (hoac hinh tron khi chua co font icon)
uint8_t           g_link_icon_ok = 0;      // 1 = o `s_link` dang dung FONT ICON (xem ui_lvgl_int.h)
static lv_obj_t  *s_st_sd_icon;            // icon THE 64x64 CO SAN cua Marlin (o SD man STATUS)
static lv_obj_t  *s_st_wifi;               // glyph WiFi tren man STATUS (hang nhiet do - xem ST_WIFI_X)
static lv_obj_t  *s_st_rssi;               // CHU do manh tin hieu canh icon WiFi do ("-58" / "--")
static uint8_t    s_wifi_drawn = 0xFF;     // trang thai WiFi dang ve (dung CHUNG cho ca 2 noi)
static uint8_t    s_sd_icon_st = 0xFE;     // mau icon the dang to (0xFE = chua to lan nao)
static uint8_t    s_rssi_st = 0xFE;        // trang thai WiFi cua lan ve chu dBm cuoi (0xFE = chua ve)
static int8_t     s_rssi_val = 0;          // so dBm dang hien (0 = khong biet / khong ket noi)

// Font icon dung cho 1 vat: big=true -> ban TO (30 px, man STATUS), false -> ban NHO (14 px, header).
// Tra NULL khi KHONG co ban font tuong ung -> noi goi se ve cham tron (duong lui).
static const lv_font_t *wifi_icon_font(const bool big) {
#if UI_HAVE_ICON_FONT_BIG
  if (big) return gfx4_font_icons_big_get();
#else
  (void)big;                                 // khong co ban to -> luon dung ban nho
#endif
#if UI_HAVE_ICON_FONT
  return gfx4_font_icons_get();
#else
  return NULL;
#endif
}

// Font icon dung cho 1 vat thanh tieu de: ban 20 px RIENG (glyph 14..18 px) — xem
// gfx4_font_icons_title.c. Ban 14 px qua nho, ban 30 px qua cao so voi thanh 26 px.
// Chua co ban 20 px -> lui ve ban 14 px (roi ban NULL -> noi goi ve cham tron).
// PHAI dinh nghia SAU wifi_icon_font() vi ham nay goi no.
static const lv_font_t *title_icon_font(void) {
#if UI_HAVE_ICON_FONT_TITLE
  return gfx4_font_icons_title_get();
#else
  return wifi_icon_font(false);
#endif
}

// Vat chi bao: KHONG cho bam (khong nhan touch) va KHONG vao group encoder.
// f != NULL -> label dung font icon f; f == NULL -> cham tron 12x12 (duong lui).
static lv_obj_t *mk_wifi_icon(lv_obj_t *parent, const lv_font_t *f) {
  lv_obj_t *o;
  if (f) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, COL_DIM, 0);              // mac dinh: xam = WiFi tat
    lv_label_set_text(l, WIFI_GLYPH_OFF);
    // KHONG dat be rong: label tu co theo glyph cua font (ban nho: 14x12; ban to: <= 30 px).
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    o = l;
  }
  else {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, WIFI_ICON_SZ, WIFI_ICON_SZ);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_DIM, 0);      // mac dinh: xam = WiFi tat (KHONG phai mau loi)
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    o = c;
  }
  // BAM ICON WIFI -> mo TRANG WIFI (truoc day icon khong bam duoc). Vao group nen encoder cung
  // bam duoc. Vat nay nho (14x12 o header / 30x25 o STATUS) nen KHONG chan vung bam nao khac:
  // o header nam trong hang flex, o STATUS nam trong khe x 64..95 (khong de o SD / o nhiet).
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_add_event_cb(o, cb_action, LV_EVENT_CLICKED, (void *)(intptr_t)A_GOTO_WIFI);
  if (s_group) lv_group_add_obj(s_group, o);
  return o;
}

// Ap trang thai WiFi (0..2) len 1 vat: mau + glyph. NULL-safe, dung CHUNG cho icon o header (font
// nho) va icon tren man STATUS (font to) - 2 noi luon cung trang thai nen chi can 1 cache.
// Vat co the la LABEL (glyph) hoac lv_obj (cham tron duong lui) -> kiem kieu luc chay de dat dung
// thuoc tinh (mau chu cho label / mau nen cho cham). Dat ca 2 loai mau la vo hai voi loai con lai.
static void wifi_glyph_apply(lv_obj_t *o, const uint8_t st) {
  if (!o) return;
  // 1 = dang ket noi -> DO, 2 = da ket noi -> XANH, 0 = tat / chua cau hinh -> XAM.
  const lv_color_t col = (st == 2) ? COL_OK : ((st == 1) ? COL_ERR : COL_DIM);
  lv_obj_set_style_text_color(o, col, 0);        // label: mau chu
  lv_obj_set_style_bg_color(o, col, 0);          // cham tron: mau nen
  // Glyph chi doi voi label: 2 = wifi (fan dac), 1 = 3 vach (dang thu), 0 = wifi_off (gach cheo).
  if (lv_obj_check_type(o, &lv_label_class))
    lv_label_set_text(o, (st == 2) ? WIFI_GLYPH_ON : ((st == 1) ? WIFI_GLYPH_CONN : WIFI_GLYPH_OFF));
}

// Cap nhat icon WiFi. Goi o nhip CHAM cua ui_lvgl_task (~400 ms), KHONG phai moi vong lap, va
// CHI ghi style khi trang thai DOI (giong s_bot_nav / jog_axis_mark) -> khong invalidate lien tuc.
// Giu icon LUON HIEN (khong an khi trang thai 0) de flex cua dong tieu de khong phai xep lai moi
// lan WiFi bat/tat; mau xam mo the hien "khong co WiFi", khong phai "loi".
void wifi_icon_mark(void) {
  const uint8_t st = wifi_state_now();       // cung nguon voi dong "WIFI:" o man INFO
  if (st == s_wifi_drawn) return;
  s_wifi_drawn = st;
  wifi_glyph_apply(s_wifi, st);              // icon o header
  wifi_glyph_apply(s_st_wifi, st);           // icon tren man STATUS (cung 1 cache)
}

// ===========================================================================
//  Icon WiFi + CHU do manh tin hieu tren man STATUS - dat trong KHU TOC DO IN/FLOW
//  (2026-10-14, theo yeu cau: "de o khu dieu chinh toc do in + flow, kich thuoc bang icon toc do in").
//
//  Hang giua man STATUS = [the SD 64x64] [FR 80x32] [FL 80x32] [SETTINGS 64x64], y 130..194.
//  Cac vung "cung khu": FR (70,136,80,32) va FL (170,136,80,32) - icon 32x32 + gia tri % o x=32.
//  Khe giua FR va FL chi rong 20 px (150..169) => KHONG du cho icon 32 px + chu dBm, nen phai
//  nhuong cho: 2 o the SD / SETTINGS thu tu 64 -> 48 px (ui_page_status.cpp: ST_TILE_SZ) va 2 khoi
//  FR/FL thu tu 80 -> 68 px (ST_RATE_W). Bo cuc moi (xem bang o ui_page_status.cpp):
//      the SD 0..47 | FR 54..121 | WiFi 128..157 + "-58" 161..185 | FL 192..259 | SETTINGS 272..319
//  Icon WiFi dung FONT TO 30 px (xAdvance 30, yAdvance 25) - dung bang co icon FR/FL 32x32.
//
//  Canh giua theo CHIEU DOC trong hang y 136..167 (cao 32):
//      - font icon to: yAdvance 25 -> y = 136 + (32 - 25) / 2 = 139
//      - font mono (chu dBm): yAdvance 17, net so nam y+4..y+14 -> y = 143 (tam net ~152)
//  Man STATUS khong dung flex (mk_page_full) nen dat toa do tuyet doi la duoc.
// ===========================================================================
#define ST_WIFI_X        128             // giua khoi FR (ket thuc 121) va khoi FL (bat dau 192)
#define ST_WIFI_Y        139
#define ST_RSSI_X        161             // ngay ben phai icon (128 + 30 rong + 3 khe)
#define ST_RSSI_Y        143

// Trang thai dac biet: dang in / tam dung tu the SD -> GIU NGUYEN mau cu cua icon the.
#define SD_ICON_KEEP 0xFF

// Tao vat trang thai cho man STATUS - goi 1 LAN trong page_status_create() (xem forward decl).
//  - sd_icon: icon THE CO SAN cua Marlin (o SD) - chi DOI MAU, khong ve them gi.
//  - icon WiFi (font TO 30 px) + chu dBm: nam giua khoi FR va khoi FL (xem khoi tren).
// Invalidate ca 3 cache de nhip cham ke tiep ve dung trang thai ngay.
void status_icons_create(lv_obj_t *p, lv_obj_t *sd_icon) {
  s_st_sd_icon = sd_icon;
  const lv_font_t *f = wifi_icon_font(true);        // ban TO 30 px (to bang icon FR/FL)
  s_st_wifi = mk_wifi_icon(p, f);
  // Chieu cao that: label = line_height cua font; cham tron (khong co font) = WIFI_ICON_SZ.
  const lv_coord_t gh = f ? f->line_height : WIFI_ICON_SZ;
  lv_obj_set_pos(s_st_wifi, ST_WIFI_X, (lv_coord_t)(ST_WIFI_Y + (25 - gh) / 2));
  s_st_rssi = mk_label(p, "", COL_DIM);
  lv_obj_set_pos(s_st_rssi, ST_RSSI_X, ST_RSSI_Y);

  s_sd_icon_st = 0xFE;      // 0xFE = chua to lan nao (0xFF danh cho "dang in -> giu mau cu")
  s_wifi_drawn = 0xFF;
  s_rssi_st = 0xFE;
  s_rssi_val = 0;
}

// Quen 3 vat trang thai cua man STATUS. Goi tu page_status_destroy() (xem ui_lvgl_int.h): sau khi
// trang STATUS bi thu hoi thi `s_st_wifi` / `s_st_sd_icon` / `s_st_rssi` se tro vao vat DA BI XOA,
// ma wifi_icon_mark() / wifi_rssi_mark() / status_sd_icon_mark() lai chay dinh ky (~400 ms) trong
// ui_lvgl_task() cho MOI trang -> phai quen di truoc khi xoa trang (wifi_glyph_apply/icon_color deu
// NULL-safe nen sau do chi la no-op). Invalidate ca 3 cache de lan tao lai trang STATUS ve dung ngay.
void status_icons_forget(void) {
  s_st_sd_icon = NULL;
  s_st_wifi    = NULL;
  s_st_rssi    = NULL;
  s_sd_icon_st = 0xFE;      // chua to lan nao
  s_wifi_drawn = 0xFF;      // chua ve lan nao
  s_rssi_st    = 0xFE;
  s_rssi_val   = 0;
}

// Cap nhat CHU do manh tin hieu (dBm) canh icon WiFi tren man STATUS. Goi cung nhip cham voi
// wifi_icon_mark() (~400 ms). RSSI doi lien tuc theo thoi gian nen phai cache CA (trang thai, so)
// va chi ghi khi THUC SU doi -> khong invalidate lien tuc.
//   0 = tat / chua cau hinh -> chu RONG (icon xam da noi "khong co WiFi")
//   1 = dang ket noi       -> "--" mau DO
//   2 = da ket noi         -> "-58" mau XANH (RSSI = 0 luc vua noi xong -> "--")
void wifi_rssi_mark(void) {
  if (!s_st_rssi) return;                          // man STATUS khong ton tai / da bi thu hoi
  const uint8_t st = wifi_state_now();
  const int8_t  rs = (st == 2) ? ui_wifi_rssi() : 0;
  if (st == s_rssi_st && rs == s_rssi_val) return;
  s_rssi_st = st;
  s_rssi_val = rs;
  char b[8];
  if (st == 0) b[0] = '\0';
  else if (rs) snprintf(b, sizeof(b), "%d", (int)rs);
  else         snprintf(b, sizeof(b), "--");
  lv_label_set_text(s_st_rssi, b);
  lv_obj_set_style_text_color(s_st_rssi, (st == 2) ? COL_OK : ((st == 1) ? COL_ERR : COL_DIM), 0);
}

// Doi mau icon THE co san theo trang thai the TF cua MAN HINH CYD (nhip cham 400 ms, chi ghi khi doi -
// icon la bitmap 64x64 nen moi lan doi mau la ve lai 64x64 pixel qua SPI).
//
// THU TU UU TIEN:
//   1) Dang IN / TAM DUNG tu the SD (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED): GIU NGUYEN mau cu
//      (MR_TIME - dung nhu truoc day, vi luc do the dang duoc Marlin doc lien tuc nen trang thai
//      the MAN HINH CYD khong con la thong tin chinh; va day cung la hanh vi cu, tranh doi hinh).
//   2) Khi ranh: to theo the TF (cyd_sd_ready(), the o khe man hinh):
//        OK (the dung duoc)            -> COL_OK  (xanh)
//        co module nhung chua mount/loi -> COL_ERR (do)
//        accessor vang / khong co module (weak NULL) -> COL_DIM (xam, KHONG phai do)
void status_sd_icon_mark(void) {
  uint8_t st;
  if (cydlcd.flags & (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED)) st = SD_ICON_KEEP;
  else                                                        st = sd_state_now();
  if (st == s_sd_icon_st) return;
  s_sd_icon_st = st;
  icon_color(s_st_sd_icon, (st == SD_ST_OK)   ? COL_OK
                         : (st == SD_ST_NO)   ? COL_ERR
                         : (st == SD_ST_NONE) ? COL_DIM
                                              : MR_TIME);   // SD_ICON_KEEP -> mau cu
}
// ===========================================================================
//  Dong tieu de cho cac man con:
//    [HOME] [ten man] ...... [II] [[]] [icon ket noi] [wifi] [BACK]
//  (STATUS khong dung thanh nay; MENU dung nhung co bo cuc rieng cho phan noi dung)
//  2026-10-16: II/[] duoc TAO TRUOC icon ket noi -> khi hien (dang in) chung la 2 icon DAU TIEN
//  cua nhom ben phai; khi AN thi flex bo qua nen nhom bat dau bang icon ket noi nhu cu.
//
//  BE RONG CO DINH (pad 4+4, gap 3 moi khe - xem pad_column o duoi):
//    (a) binh thuong (II/[] AN - flex bo qua vat HIDDEN):  6 vat, 5 khe
//        8 + 5*3 + (26 + TITLE_W + 64 + 14 + 48) = 8 + 15 + 236 = 259 -> spacer = 61 px
//    (b) dang in (II/[] HIEN): 8 vat, 7 khe
//        8 + 7*3 + (26 + TITLE_W + 64 + 14 + 26 + 26 + 48) = 8 + 21 + 288 = 317 <= 320 -> spacer = 3
//    => CA 2 truong hop deu <= 320 px: BACK va HOME KHONG BAO GIO bi day ra ngoai.
//  (Doi THU TU khong doi TONG be rong vi chi la hoan vi.)
//  TITLE_W va LINK_LBL_W la be rong CO DINH nen tong tren la con so chac chan (khong phu thuoc
//  do dai ten man / ten may); chu dai hon thi TU CUON trong o cua no.
// ===========================================================================
void build_chrome(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  s_bar = lv_obj_create(scr);
  lv_obj_set_pos(s_bar, 0, 0);
  lv_obj_set_size(s_bar, UI_LCD_HOR_RES, BAR_H);
  style_flat(s_bar, COL_TITLEBAR);
  // Duong ke 1 px o day thanh: nen man hinh qua toi nen DOI MAU NEN khong the dat 3:1 voi no
  // (xem ghi chu COL_TITLEBAR o ui_lvgl_int.h). Duong ke la thu lam BIEN cua thanh dat chuan.
  lv_obj_set_style_border_side(s_bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(s_bar, COL_BTN_OUTLINE, 0);
  lv_obj_set_style_border_width(s_bar, 1, 0);
  lv_obj_set_style_pad_left(s_bar, 4, 0);
  lv_obj_set_style_pad_right(s_bar, 4, 0);
  // Gap 3 (truoc la 4): du 7 khe khi dang in -> tiet kiem 7 px, xem bang be rong o tren.
  lv_obj_set_style_pad_column(s_bar, 3, 0);
  lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // ---- Nut HOME (icon ngoi nha) o BEN TRAI thanh tieu de, TRUOC ten man ----
  // Bam -> VE THANG man STATUS (A_GOTO_STATUS). Nut BACK (cuoi thanh) giu nguyen = lui 1 cap.
  // Glyph U+E007 cua font icon NHO (cung font voi icon WiFi o header), nhan duoc canh giua bang
  // lv_obj_center() trong mk_btn. Vao group nhu moi nut -> bam duoc bang CA cam ung lan encoder.
  // Kiem glyph luc CHAY: font cu (chua co U+E007) -> doi nhan thanh chu "H" cho nut khong trong.
  {
    lv_obj_t *home = mk_btn(s_bar, HOME_GLYPH, HOME_BTN_W, 20, cb_action,
                            (void *)(intptr_t)A_GOTO_STATUS);
    // Icon trai cung nam TRUC TIEP tren nen thanh tieu de: KHONG nen, KHONG vien (yeu cau nguoi dung
    // 2026-10-16). LV_OBJ_FLAG_USER_3 = vat "phang" -> focus_mark() khong bao gio ve vien cho no.
    lv_obj_set_style_bg_opa(home, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home, 0, 0);
    lv_obj_add_flag(home, LV_OBJ_FLAG_USER_3);   // xoa vien "outlined" ma mk_btn() vua ve
    lv_obj_t *hl = lv_obj_get_child(home, 0);
    const lv_font_t *iconf = title_icon_font();   // ban 20 px cua thanh tieu de
    lv_font_glyph_dsc_t gd;
    if (hl && iconf && lv_font_get_glyph_dsc(iconf, &gd, 0xE007U, 0))
      lv_obj_set_style_text_font(hl, iconf, 0);          // co glyph -> nut chi co icon
    else if (hl)
      set_text(hl, "H");                                 // duong lui: chu, van bam duoc
  }

  // ---- Ten man: o be rong CO DINH (TITLE_W) de tong be rong thanh tieu de la con so chac chan
  // (xem bang o tren). Ten dai hon o thi tu cuon trong o, khong day cac nut ra ngoai.
  s_title = mk_label_w(s_bar, T(UI_T_PT_STATUS), COL_TXT, TITLE_W);
#if UI_MONO_FONT == 4
  // DOI CHUNG cho phep thu mono: giu TEN MAN o font Montserrat. Neu man hinh chi con doc duoc ten
  // man ma moi chu khac mat => font mono khong ve duoc. Neu chu khac hien binh thuong => font tot.
  // (Uu tien nhat: neu CA ten man lan chu khac deu mat thi loi nam o cho khac, khong phai font.)
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_14, 0);
#endif
  // Ten man CUNG bam duoc -> ve STATUS (giong nut HOME). lv_label mac dinh KHONG clickable (xem
  // lv_label.c) nen phai tu bat co; them CLICK_FOCUSABLE + vao group de encoder cung dung duoc.
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  // Ten man cung la vat "phang": KHONG nen, KHONG vien (yeu cau nguoi dung 2026-10-16).
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_USER_3);
  lv_obj_add_event_cb(s_title, cb_action, LV_EVENT_CLICKED, (void *)(intptr_t)A_GOTO_STATUS);
  if (s_group) lv_group_add_obj(s_group, s_title);

  mk_spacer(s_bar, 0, 1, 1);                     // an het cho trong -> day cac vat sau ve phai

  // P9: 2 nut nho PAUSE / STOP - mac dinh AN, chi hien khi dang in (xem ui_lvgl_task)
  // "II" / "[]" la KY HIEU (khong phai chu) -> CO TINH khong dich, giu nguyen o MOI ngon ngu.
  // 2026-10-16 (yeu cau nguoi dung): TAO TRUOC `s_link` de 2 nut nay luon la 2 ICON DAU TIEN cua
  // nhom ben phai (thu tu flex = thu tu tao), roi moi toi icon ket noi / WiFi / BACK o phia phai
  // chung. Luc AN thi flex bo qua nen nhom ben phai bat dau bang icon ket noi nhu cu.
  s_btn_pause = mk_btn(s_bar, "II", 26, 20, cb_action, (void *)(intptr_t)A_PAUSE);
  s_btn_stop  = mk_btn(s_bar, "[]", 26, 20, cb_action, (void *)(intptr_t)A_STOP);
  lv_obj_add_flag(s_btn_pause, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_btn_stop,  LV_OBJ_FLAG_HIDDEN);

  // Gioi han be rong de ten may dai khong day cac nut P9 ra ngoai thanh tieu de.
  // 64 px: du cho "LINK ERR" (8 ky tu mono x 8 px = 64) va vua du cho cac nut + icon + ten man
  // (xem bang be rong o dau build_chrome).
  s_link  = mk_label_w(s_bar, T(UI_T_LINK_NONE), COL_ERR, LINK_LBL_W);
  // O nay KHONG con hien TEN MAY IN nua ma hien ICON KET NOI (xem LINK_GLYPH_* o ui_lvgl_int.h).
  // Do glyph luc CHAY giong nut HOME: font cu (chua co U+E00C) -> g_link_icon_ok = 0 va ui_lvgl.cpp
  // lui ve hien CHU nhu truoc, khong bao gio hien o vuong.
  {
    const lv_font_t *iconf = title_icon_font();
    lv_font_glyph_dsc_t gd;
    g_link_icon_ok = (iconf && lv_font_get_glyph_dsc(iconf, &gd, 0xE00CU, 0)) ? 1 : 0;
    if (g_link_icon_ok) lv_obj_set_style_text_font(s_link, iconf, 0);
  }
  // CANH PHAI trong o 64 px (yeu cau nguoi dung 2026-10-16): mk_label_w() dat long mode
  // SCROLL_CIRCULAR, ma che do do LUON CANH GIUA nen icon 12 px se lo lung giua o. Doi sang CLIP
  // de text_align co tac dung. Chuoi du phong "LINK ERR" dung 64 px (8 ky tu mono x 8 px) nen van vua.
  lv_label_set_long_mode(s_link, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(s_link, LV_TEXT_ALIGN_RIGHT, 0);
  // Icon trang thai WiFi: ngay SAU icon ket noi, TRUOC nut BACK. La CON CUA s_bar nen tu an/hien
  // cung dong tieu de (man STATUS an s_bar -> icon an theo, khong can lam gi).
  s_wifi = mk_wifi_icon(s_bar, title_icon_font());
  s_btn_back = mk_btn(s_bar, T(UI_T_CHROME_BACK), 48, 20, cb_action, (void *)(intptr_t)A_BACK);

  // --- Thanh day (sticky nhu dong tieu de): [PREV] [TRANG x/y] [NEXT] ---
  // CHI hien khi noi dung man dang mo BI TRAN theo chieu doc (xem bot_sync()).
  // PHAI tao luc dang HIEN roi moi an: LVGL bo qua tinh layout cho vat HIDDEN, do luc an
  // se ra 0 -> bot_sync() tuong la khong tran va khong bao gio hien thanh nay.
  s_bot = lv_obj_create(scr);
  lv_obj_set_pos(s_bot, 0, BOT_Y);
  lv_obj_set_size(s_bot, UI_LCD_HOR_RES, BOT_H);
  style_flat(s_bot, COL_BAR);
  lv_obj_set_style_pad_left(s_bot, 4, 0);
  lv_obj_set_style_pad_right(s_bot, 4, 0);
  lv_obj_set_style_pad_column(s_bot, 4, 0);
  lv_obj_set_flex_flow(s_bot, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_bot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Chu "PREV"/"NEXT"/"TRANG x/y" deu la ASCII -> dung Montserrat mac dinh (KHONG vn_font*)
  s_bot_prev = mk_btn(s_bot, T(UI_T_CHROME_PREV), 56, 18, cb_action, (void *)(intptr_t)A_PAGE_PREV);
  s_bot_info = mk_label(s_bot, "", COL_DIM);
  lv_obj_set_flex_grow(s_bot_info, 1);                                        // gian het cho trong giua
  lv_obj_set_style_text_align(s_bot_info, LV_TEXT_ALIGN_CENTER, 0);
  s_bot_next = mk_btn(s_bot, T(UI_T_CHROME_NEXT), 56, 18, cb_action, (void *)(intptr_t)A_PAGE_NEXT);

  lv_obj_update_layout(s_bot);                    // do xong (luc dang HIEN) roi moi an
  lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
  s_bot_shown = false;
}

// ===========================================================================
//  DOI NGON NGU: ve lai CHU cua "chrome" (dong tieu de + thanh day)
// ===========================================================================
// Chrome duoc dung MOT LAN luc khoi dong (build_chrome) nen khi doi ngon ngu phai tu dat lai chu cho
// cac vat co chu CO DINH:
//   - ten man (s_title): do `page_rebuild()` lo (no goi page_title()) - xem ui_lang_apply_ui();
//   - nut BACK / PREV / NEXT: o day;
//   - nhan "TRANG x/y": `bot_sync()` tu tinh lai (chay dinh ky 400 ms, `set_text` chi ve khi chu doi);
//   - o ten may / LINK ERR / NO LINK: luot cap nhat (ui_lvgl_task) tu dat lai;
//   - nut HOME khi KHONG co font icon (chu "H") va 2 nut "II"/"[]": KY HIEU, khong dich.
// Khong dung lai chrome tu dau: build_chrome() xoa/tim lai group + bo cuc, de vo hon muc can thiet.
void chrome_lang_retext(void) {
  if (s_btn_back)  set_text(lv_obj_get_child(s_btn_back, 0),  T(UI_T_CHROME_BACK));
  if (s_bot_prev)  set_text(lv_obj_get_child(s_bot_prev, 0),  T(UI_T_CHROME_PREV));
  if (s_bot_next)  set_text(lv_obj_get_child(s_bot_next, 0),  T(UI_T_CHROME_NEXT));
}

#endif  // CYDLCD_USE_LVGL
