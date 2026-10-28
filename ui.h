#pragma once
// ==========================================================================
//  UI for the 320x240 CYD screen
//  Design language: PrintLab navy + filament accents, rosh.sh mono readouts.
//  Space Grotesk for headlines and numbers, JetBrains Mono for HUD labels.
//  Needs `tft`, the globals in model.h and qr_encode.h to be available.
// ==========================================================================
#include "fonts.h"
#include "textutil.h"

#ifndef SMOOTH_FONT
  #warning "Enable SMOOTH_FONT in TFT_eSPI's User_Setup.h for the full design (falls back to built-in fonts)"
#endif

// ---------- Palette ----------
constexpr uint16_t C565(uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}
const uint16_t UI_BG     = C565(0x0D1F33); // deep navy
const uint16_t UI_SURF   = C565(0x153250); // PrintLab navy
const uint16_t UI_LINE   = C565(0x24496F); // hairlines
const uint16_t UI_GRID   = C565(0x16304C); // background dot grid
const uint16_t UI_INK    = C565(0xF0F0F0); // rosh.sh paper white
const uint16_t UI_MUTED  = C565(0x8FA6BF);
const uint16_t UI_DIM    = C565(0x5B7896);
const uint16_t UI_CYAN   = C565(0x2ED3F0);
const uint16_t UI_GREEN  = C565(0x3BE08A);
const uint16_t UI_YELLOW = C565(0xFFC93C);
const uint16_t UI_RED    = C565(0xFF5B5B);
const uint16_t UI_PAPER  = C565(0xF0F0F0);
const uint16_t UI_QRINK  = C565(0x0B1A2B);
const uint16_t UI_PAPER_MUTED = C565(0x6B7F95);
const uint16_t UI_BAR    = C565(0x2A6E99); // past-hour bars

// The seven filament colours from the PrintLab colour turntable
const uint16_t UI_FILAMENT[7] = {
  C565(0x1A1A1A), C565(0xF0F0F0), C565(0xE63946), C565(0x2F6FE4),
  C565(0x2ED3F0), C565(0x3BE08A), C565(0xFFC93C)
};

// ---------- Layout ----------
const int UI_W = 320, UI_H = 240;
const int UI_TOP = 22;     // status bar height (hairline at y=21)
const int UI_NAV_Y = 208;  // nav bar top (hairline at y=208)

// Touch targets (shared with handleTap)
const int ORD_LIST_Y = 76, ORD_ROW_H = 32, ORD_ROWS = 4;
const int SET_BTN_W = 34, SET_BTN_H = 30;
const int SET_MINUS_X = 232, SET_PLUS_X = 274;
const int SET_VOL_Y = 82, SET_BRI_Y = 126;
const int SET_TEST_X = 232, SET_TEST_Y = 168, SET_TEST_W = 76, SET_TEST_H = 30;

// ---------- Fonts ----------
enum UiFont : uint8_t { FT_LABEL = 0, FT_MONO, FT_BODY, FT_HEAD, FT_NUM, FT_HERO };
static const uint8_t* const UI_FONT_DATA[] = {
  F_LABEL_DATA, F_MONO_DATA, F_BODY_DATA, F_HEAD_DATA, F_NUM_DATA, F_HERO_DATA
};
enum UiAlign : uint8_t { AL_L = 0, AL_C, AL_R };

static int8_t uiCurFont = -1;

#ifdef SMOOTH_FONT
// TFT_eSPI re-parses a font's whole glyph table (and frees/mallocs 7 arrays)
// on every loadFont(). A redraw switches fonts dozens of times, so each font
// is parsed once and then swapped in by its public metrics + arrays, which is
// all drawGlyph()/textWidth() read for array fonts. ~7 KB of heap, allocated
// once at the first draws. Nothing else may call tft.loadFont/unloadFont.
struct UiFontSlot {
  bool ready;
  TFT_eSPI::fontMetrics m;
  uint16_t* unicode; uint8_t* height; uint8_t* width; uint8_t* xAdvance;
  int16_t* dY; int8_t* dX; uint32_t* bitmap;
};
static UiFontSlot uiFontSlots[6];

static void ui_fontInstall(const UiFontSlot& c) {
  tft.gFont = c.m;
  tft.gUnicode = c.unicode; tft.gHeight = c.height; tft.gWidth = c.width; tft.gxAdvance = c.xAdvance;
  tft.gdY = c.dY; tft.gdX = c.dX; tft.gBitmap = c.bitmap;
  tft.fontLoaded = true;
}
#endif

void ui_font(uint8_t f) {
  if (uiCurFont == (int8_t)f) return;
#ifdef SMOOTH_FONT
  UiFontSlot& c = uiFontSlots[f];
  if (!c.ready) {
    // Detach the cached font that's installed, so loadFont() can't free it
    tft.fontLoaded = false;
    tft.gUnicode = nullptr; tft.gHeight = nullptr; tft.gWidth = nullptr; tft.gxAdvance = nullptr;
    tft.gdY = nullptr; tft.gdX = nullptr; tft.gBitmap = nullptr;
    tft.loadFont(UI_FONT_DATA[f]);
    c.m = tft.gFont;
    c.unicode = tft.gUnicode; c.height = tft.gHeight; c.width = tft.gWidth; c.xAdvance = tft.gxAdvance;
    c.dY = tft.gdY; c.dX = tft.gdX; c.bitmap = tft.gBitmap;
    c.ready = c.unicode != nullptr;
  } else {
    ui_fontInstall(c);
  }
#else
  static const uint8_t fallback[] = {1, 2, 2, 4, 4, 4};
  tft.setTextFont(fallback[f]);
#endif
  uiCurFont = (int8_t)f;
}

// Spaces -> U+00A0 so smooth fonts use their real space width (see fonts.h)
String ui_sp(const String& s) {
#ifdef SMOOTH_FONT
  String o = s;
  o.replace(" ", "\xC2\xA0");
  return o;
#else
  return s;
#endif
}

int16_t ui_w(const String& s, uint8_t f) {
  ui_font(f);
  return tft.textWidth(ui_sp(s));
}

// Draw text with its baseline at y. Returns the drawn width.
int16_t ui_text(const String& s, int x, int y, uint8_t f, uint16_t fg, uint16_t bg, uint8_t align = AL_L) {
  ui_font(f);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(align == AL_C ? C_BASELINE : align == AL_R ? R_BASELINE : L_BASELINE);
  return tft.drawString(ui_sp(s), x, y);
}

// Cut a string to fit maxW, ending with an ellipsis
String ui_fit(const String& s, uint8_t f, int maxW) {
  if (ui_w(s, f) <= maxW) return s;
  String t = s;
  while (t.length() > 1) {
    utf8PopBack(t);
    t.trim();
    if (ui_w(t + "…", f) <= maxW) return t + "…";
  }
  return t;
}

// Word-wrap into at most maxLines lines; the last line gets an ellipsis if cut
int ui_wrap(const String& s, int x, int y, int maxW, int lineH, int maxLines,
            uint8_t f, uint16_t fg, uint16_t bg, uint8_t align = AL_L) {
  String rest = s;
  rest.trim();
  int line = 0;
  while (rest.length() && line < maxLines) {
    String cur = "";
    int i = 0;
    while (i < (int)rest.length()) {
      int sp = rest.indexOf(' ', i);
      if (sp < 0) sp = rest.length();
      String cand = rest.substring(0, sp);
      if (ui_w(cand, f) > maxW && cur.length()) break;
      cur = cand;
      i = sp + 1;
      if (sp >= (int)rest.length()) break;
    }
    if (!cur.length()) cur = rest; // single long word
    rest = rest.substring(min((int)rest.length(), (int)cur.length())); // drop what we used
    rest.trim();
    if (line == maxLines - 1 && rest.length()) cur = ui_fit(cur + " " + rest, f, maxW);
    else cur = ui_fit(cur, f, maxW);
    ui_text(cur, x, y + line * lineH, f, fg, bg, align);
    line++;
  }
  return line;
}

// Indian/Nepali digit grouping: 125000 -> "1,25,000"
String fmtAmt(uint32_t n) {
  String d = String(n);
  if (d.length() <= 3) return d;
  String tail = d.substring(d.length() - 3);
  String head = d.substring(0, d.length() - 3);
  String out = "";
  while (head.length() > 2) {
    out = "," + head.substring(head.length() - 2) + out;
    head = head.substring(0, head.length() - 2);
  }
  return head + out + "," + tail;
}

String shopName() { return cfg.bizName.length() ? cfg.bizName : String("Business Hub"); }

String money(uint32_t n) {
  return cfg.currency + " " + fmtAmt(n);
}

String two(int v) { return v < 10 ? "0" + String(v) : String(v); }

bool ui_now(struct tm& t) { return getLocalTime(&t, 0); }

String ui_clockText() {
  struct tm t;
  if (!ui_now(t)) return "--:--:--";
  return two(t.tm_hour) + ":" + two(t.tm_min) + ":" + two(t.tm_sec);
}

String ui_upper(const String& s) { String u = s; u.toUpperCase(); return u; }

// ---------- Primitives ----------
void ui_hline(int x, int y, int w, uint16_t c = UI_LINE) { tft.drawFastHLine(x, y, w, c); }

// Background dot grid on a fixed 16px lattice (origin 8,8), clipped to a box
void ui_dotgrid(int x0, int y0, int x1, int y1) {
  const int step = 16;
  int sx = 8 + ((max(x0, 8) - 8 + step - 1) / step) * step;
  int sy = 8 + ((max(y0, 8) - 8 + step - 1) / step) * step;
  for (int y = sy; y < y1; y += step)
    for (int x = sx; x < x1; x += step) tft.drawPixel(x, y, UI_GRID);
}

void ui_strip(int x, int y, int w, int h) {
  for (int i = 0; i < 7; i++) {
    int a = x + (w * i) / 7, b = x + (w * (i + 1)) / 7;
    tft.fillRect(a, y, b - a, h, UI_FILAMENT[i]);
  }
}

void ui_pill(int x, int y, int w, int h, const String& label, uint16_t c, uint16_t bg, bool filled) {
  if (filled) {
    tft.fillSmoothRoundRect(x, y, w, h, h / 2, c, bg);
    ui_text(label, x + w / 2, y + h / 2 + 4, FT_LABEL, bg, c, AL_C);
  } else {
    tft.drawSmoothRoundRect(x, y, h / 2, h / 2, w, h, c, bg);
    ui_text(label, x + w / 2, y + h / 2 + 4, FT_LABEL, c, bg, AL_C);
  }
}

void ui_outlineBtn(int x, int y, int w, int h, uint16_t c) {
  tft.drawSmoothRoundRect(x, y, 8, 8, w, h, c, UI_BG);
}

void ui_eyebrow(const String& num, const String& label, int x, int y, uint16_t c = UI_CYAN) {
  int w = ui_text(num, x, y, FT_LABEL, c, UI_BG);
  ui_text("/ " + label, x + w + 6, y, FT_LABEL, UI_DIM, UI_BG);
}

// Wi-Fi strength as four little bars, right edge at x, bottom at y
void ui_wifiBars(int x, int y) {
  int bars = 0;
  if (WiFi.status() == WL_CONNECTED) {
    long r = WiFi.RSSI();
    bars = r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : 1;
  }
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 2;
    tft.fillRect(x - 13 + i * 4, y - h, 2, h, i < bars ? UI_INK : UI_LINE);
  }
}

// ---------- QR ----------
bool ui_qr(const String& text, int bx, int by, int box, uint16_t ink, uint16_t paper) {
  static QRCode qr;
  static uint8_t buf[900];
  static String encoded;          // text currently in qr/buf: re-encode only when it changes
  static bool encodedOk = false;
  static const uint16_t capacityLow[] = {0, 17, 32, 53, 78, 106, 134, 154, 192,
                                         230, 271, 321, 367, 425, 458, 520, 586};
  if (!encodedOk || text != encoded) {
    uint8_t version = 0;
    for (int v = 1; v <= 16; v++) {
      if (text.length() <= capacityLow[v] && qrcode_getBufferSize((uint8_t)v) <= sizeof(buf)) {
        version = (uint8_t)v;
        break;
      }
    }
    encoded = text;
    encodedOk = version != 0 && qrcode_initText(&qr, buf, version, ECC_LOW, text.c_str()) == 0;
  }
  if (!encodedOk) return false;
  int scale = max(1, box / (qr.size + 2));
  int px = qr.size * scale;
  int x0 = bx + (box - px) / 2, y0 = by + (box - px) / 2;
  for (int y = 0; y < qr.size; y++) {
    int run = -1;
    for (int x = 0; x <= qr.size; x++) {
      bool on = x < qr.size && qrcode_getModule(&qr, x, y);
      if (on && run < 0) run = x;
      if (!on && run >= 0) {
        tft.fillRect(x0 + run * scale, y0 + y * scale, (x - run) * scale, scale, ink);
        run = -1;
      }
    }
  }
  return true;
}

// ---------- Chrome: status bar + nav ----------
uint16_t ui_netColor() {
  switch (netState) {
    case NET_OK:   return UI_GREEN;
    case NET_SYNC: return UI_YELLOW;
    case NET_ERR:  return UI_RED;
    default:       return UI_DIM;
  }
}

void ui_drawDot() {
  tft.fillRect(8, 5, 13, 13, UI_BG);
  tft.drawSpot(14, 11, 3.5, ui_netColor(), UI_BG);
}

void ui_drawClock() {
  // Left-aligned from a fixed x so the digits never shift as they change
  static int x = -1;
  if (x < 0) x = 308 - ui_w("00:00:00", FT_MONO);
  tft.fillRect(x - 2, 3, 312 - x, 16, UI_BG);
  ui_text(ui_clockText(), x, 15, FT_MONO, UI_MUTED, UI_BG);
}

void ui_statusBar() {
  tft.fillRect(0, 0, UI_W, UI_TOP - 1, UI_BG);
  ui_hline(0, UI_TOP - 1, UI_W);
  ui_drawDot();
  String name = shopName();
  ui_text(ui_fit(ui_upper(name), FT_LABEL, 196), 26, 15, FT_LABEL, UI_INK, UI_BG);
  ui_wifiBars(246, 15);
  ui_drawClock();
}

void ui_nav() {
  static const char* labels[SCREEN_COUNT] = {"PAY", "ORDERS", "BOARD", "STATS", "SETUP"};
  tft.fillRect(0, UI_NAV_Y, UI_W, UI_H - UI_NAV_Y, UI_BG);
  ui_hline(0, UI_NAV_Y, UI_W);
  for (int i = 0; i < SCREEN_COUNT; i++) {
    int cx = i * 64 + 32;
    bool on = (int)currentScreen == i;
    if (on) tft.fillRect(i * 64 + 14, UI_NAV_Y, 36, 2, UI_CYAN);
    ui_text("0" + String(i + 1), cx, UI_NAV_Y + 14, FT_LABEL, on ? UI_CYAN : UI_DIM, UI_BG, AL_C);
    ui_text(labels[i], cx, UI_NAV_Y + 27, FT_LABEL, on ? UI_INK : UI_MUTED, UI_BG, AL_C);
  }
}

// ---------- Screens ----------
void ui_drawPay() {
  const int cx = 10, cy = 30, cs = 168;
  tft.fillSmoothRoundRect(cx, cy, cs, cs, 12, UI_PAPER, UI_BG);

  if (cfg.qr.isEmpty() || !ui_qr(cfg.qr, cx + 4, cy + 4, cs - 8, UI_QRINK, UI_PAPER)) {
    // Viewfinder corners
    const int in = 16, L = 16, t = 3;
    int x0 = cx + in, y0 = cy + in, x1 = cx + cs - in, y1 = cy + cs - in;
    tft.fillRect(x0, y0, L, t, UI_QRINK);      tft.fillRect(x0, y0, t, L, UI_QRINK);
    tft.fillRect(x1 - L, y0, L, t, UI_QRINK);  tft.fillRect(x1 - t, y0, t, L, UI_QRINK);
    tft.fillRect(x0, y1 - t, L, t, UI_QRINK);  tft.fillRect(x0, y1 - L, t, L, UI_QRINK);
    tft.fillRect(x1 - L, y1 - t, L, t, UI_QRINK); tft.fillRect(x1 - t, y1 - L, t, L, UI_QRINK);
    bool bad = !cfg.qr.isEmpty();
    ui_text(bad ? "QR too long." : "No QR yet.", cx + cs / 2, cy + 80, FT_BODY, UI_QRINK, UI_PAPER, AL_C);
    ui_text("ADD IT AT", cx + cs / 2, cy + 102, FT_LABEL, UI_PAPER_MUTED, UI_PAPER, AL_C);
    ui_text(MDNS_NAME ".local", cx + cs / 2, cy + 118, FT_MONO, UI_QRINK, UI_PAPER, AL_C);
  }

  const int rx = 190, rw = 118;
  ui_eyebrow("01", "PAY", rx, 42);
  ui_text("Scan to", rx, 70, FT_HEAD, UI_INK, UI_BG);
  ui_text("pay.", rx, 96, FT_HEAD, UI_INK, UI_BG);
  ui_wrap(cfg.qrSub, rx, 122, rw, 18, 3, FT_BODY, UI_MUTED, UI_BG);

  ui_hline(rx, 166, rw);
  ui_text("PAYING TO", rx, 182, FT_LABEL, UI_DIM, UI_BG);
  String name = shopName();
  ui_text(ui_fit(name, FT_BODY, rw), rx, 199, FT_BODY, UI_INK, UI_BG);
}

uint16_t ui_statusColor(const String& s) {
  if (s == "NEW") return UI_CYAN;
  if (s == "PREP") return UI_YELLOW;
  if (s == "READY") return UI_GREEN;
  return UI_DIM; // PAID / done
}

// "3 OPEN" chip, top right of the Orders screen
void ui_ordersChip(bool clear = false) {
  int open = 0;
  for (int i = 0; i < orderCount; i++) if (orders[i].status != "PAID") open++;
  String chip = String(open) + " OPEN";
  int cw = ui_w(chip, FT_LABEL) + 22;
  if (clear) tft.fillRect(150, 44, 160, 24, UI_BG);
  ui_pill(308 - cw, 46, cw, 20, chip, open ? UI_CYAN : UI_DIM, UI_BG, false);
}

// One order row (between the hairlines), e.g. after a status tap
void ui_orderRow(int i, bool clear = false) {
  int y = ORD_LIST_Y + i * ORD_ROW_H;
  if (clear) tft.fillRect(12, y + 1, 296, ORD_ROW_H - 1, UI_BG);
  uint16_t sc = ui_statusColor(orders[i].status);
  bool done = orders[i].status == "PAID";
  tft.fillRect(12, y + 9, 3, 14, sc);
  ui_text("#" + String(orders[i].id), 22, y + 21, FT_MONO, UI_DIM, UI_BG);
  String amt = orders[i].amount ? fmtPaisa(orders[i].amount, fmtAmt) : String("-");
  int aw = ui_text(amt, 232, y + 21, FT_MONO, done ? UI_MUTED : UI_INK, UI_BG, AL_R);
  ui_text(ui_fit(orders[i].title, FT_BODY, 232 - aw - 10 - 60), 60, y + 21, FT_BODY,
          done ? UI_MUTED : UI_INK, UI_BG);
  ui_pill(244, y + 6, 64, 20, orders[i].status, sc, UI_BG, false);
}

void ui_drawOrders() {
  ui_eyebrow("02", "ORDERS", 12, 38);
  ui_text("Orders.", 12, 64, FT_HEAD, UI_INK, UI_BG);
  ui_ordersChip();

  ui_hline(12, ORD_LIST_Y, 296);
  if (orderCount == 0) {
    ui_text("Nothing queued.", 160, 134, FT_BODY, UI_MUTED, UI_BG, AL_C);
    ui_text("ADD ORDERS FROM " MDNS_NAME ".LOCAL", 160, 156, FT_LABEL, UI_DIM, UI_BG, AL_C);
    return;
  }
  for (int i = 0; i < orderCount && i < ORD_ROWS; i++) {
    ui_orderRow(i);
    if (i < ORD_ROWS - 1 && i < orderCount - 1) ui_hline(12, ORD_LIST_Y + (i + 1) * ORD_ROW_H, 296);
  }
  if (orderCount > ORD_ROWS) {
    ui_text("+" + String(orderCount - ORD_ROWS) + " MORE", 308, 38, FT_LABEL, UI_DIM, UI_BG, AL_R);
  }
}

void ui_drawBoard() {
  ui_dotgrid(0, UI_TOP, UI_W, 204);
  String name = cfg.bizName.length() ? cfg.bizName : String("Our Store");
  name += name.endsWith(".") ? "" : ".";
  int lines = (ui_w(name, FT_HEAD) > 288) ? 2 : 1;
  int top = lines == 1 ? 50 : 42;
  ui_text("NAMASTE · WELCOME TO", 16, top, FT_LABEL, UI_CYAN, UI_BG);
  ui_wrap(name, 16, top + 30, 288, 26, lines, FT_HEAD, UI_INK, UI_BG);

  const int ky = top + 30 + (lines - 1) * 26 + 20, kh = 66;
  tft.fillSmoothRoundRect(12, ky, 296, kh, 10, UI_SURF, UI_BG);
  tft.fillRect(24, ky + 12, 6, 6, UI_YELLOW);
  ui_text("TODAY'S SPECIAL", 36, ky + 18, FT_LABEL, UI_YELLOW, UI_SURF);
  String note = cfg.storeNote.length() ? cfg.storeNote : String("Fresh brews and pastries, every day.");
  ui_wrap(note, 24, ky + 39, 272, 18, 2, FT_BODY, UI_INK, UI_SURF);

  if (cfg.storeOpen) {
    tft.fillSmoothRoundRect(12, 180, 100, 22, 11, UI_GREEN, UI_BG);
    tft.drawSpot(25, 191, 3, UI_BG, UI_GREEN);
    ui_text("OPEN NOW", 68, 195, FT_LABEL, UI_BG, UI_GREEN, AL_C);
  } else {
    ui_pill(12, 180, 100, 22, "CLOSED", UI_RED, UI_BG, false);
  }
  ui_text(cfg.storeOpen ? "Pay by QR · instant" : "See you soon.", 308, 195, FT_MONO, UI_MUTED, UI_BG, AL_R);
  ui_strip(0, 204, UI_W, 3);
}

// Open/closed switch, top right of the Stats screen
void ui_statsSwitch(bool clear = false) {
  if (clear) tft.fillRect(150, 42, 160, 28, UI_BG);
  uint16_t sc = cfg.storeOpen ? UI_GREEN : UI_RED;
  ui_text(cfg.storeOpen ? "OPEN" : "CLOSED", 252, 60, FT_LABEL, sc, UI_BG, AL_R);
  tft.fillSmoothRoundRect(260, 44, 48, 24, 12, cfg.storeOpen ? UI_GREEN : UI_LINE, UI_BG);
  tft.fillSmoothCircle(cfg.storeOpen ? 296 : 272, 56, 9, cfg.storeOpen ? UI_BG : UI_DIM,
                       cfg.storeOpen ? UI_GREEN : UI_LINE);
}

void ui_drawStats() {
  ui_eyebrow("04", "STATS", 12, 38);
  ui_text("Today.", 12, 64, FT_HEAD, UI_INK, UI_BG);

  ui_statsSwitch();

  // Big number
  int cw = ui_text(cfg.currency, 12, 112, FT_HEAD, UI_MUTED, UI_BG);
  ui_text(fmtAmt(todaySales), 12 + cw + 6, 112, FT_NUM, UI_INK, UI_BG);

  // Tiles
  uint32_t avg = totalTransactions ? todaySales / totalTransactions : 0;
  String vals[3] = {String(totalTransactions), money(avg), lastPayHM.length() ? lastPayHM : String("--:--")};
  const char* labs[3] = {"PAYMENTS", "AVG TICKET", "LAST"};
  for (int i = 0; i < 3; i++) {
    int x = 12 + i * 102;
    tft.fillSmoothRoundRect(x, 124, 92, 38, 8, UI_SURF, UI_BG);
    ui_text(labs[i], x + 10, 139, FT_LABEL, UI_DIM, UI_SURF);
    ui_text(ui_fit(vals[i], FT_MONO, 74), x + 10, 155, FT_MONO, UI_INK, UI_SURF);
  }

  // Hourly bars
  ui_text("BY HOUR", 12, 178, FT_LABEL, UI_DIM, UI_BG);
  uint32_t peak = 0;
  int peakH = -1;
  for (int h = 0; h < 24; h++) if (hourly[h] > peak) { peak = hourly[h]; peakH = h; }
  if (peakH >= 0) ui_text("PEAK " + two(peakH) + ":00", 308, 178, FT_LABEL, UI_DIM, UI_BG, AL_R);
  struct tm t;
  int nowH = ui_now(t) ? t.tm_hour : -1;
  for (int h = 0; h < 24; h++) {
    int x = 18 + h * 12;
    bool cur = h == nowH;
    if (hourly[h] && peak) {
      int bh = max(3, (int)((hourly[h] * 20ULL) / peak));
      tft.fillRect(x, 204 - bh, 8, bh, cur ? UI_CYAN : UI_BAR);
    } else {
      tft.fillRect(x, 202, 8, 2, cur ? UI_CYAN : UI_LINE);
    }
  }
}

void ui_meter(int x, int y, int value, int maxV, uint16_t c) {
  const int segs = 16;
  int on = (value * segs + maxV / 2) / maxV;
  for (int i = 0; i < segs; i++) tft.fillRect(x + i * 8, y, 6, 12, i < on ? c : UI_LINE);
}

void ui_stepBtn(int x, int y, bool plus) {
  ui_outlineBtn(x, y, SET_BTN_W, SET_BTN_H, UI_LINE);
  int mx = x + SET_BTN_W / 2, my = y + SET_BTN_H / 2;
  tft.fillRect(mx - 5, my - 1, 10, 2, UI_INK);
  if (plus) tft.fillRect(mx - 1, my - 5, 2, 10, UI_INK);
}

// Value + meter of the volume (row 0) or brightness (row 1) setting
void ui_settingValue(int row, bool clear = false) {
  int y = row == 0 ? 92 : 136;
  int v = row == 0 ? cfg.volume : cfg.brightness;
  if (clear) tft.fillRect(12, y + 3, 74, 19, UI_BG);
  ui_text(String(v) + "%", 12, y + 17, FT_BODY, UI_INK, UI_BG);
  ui_meter(90, y, v, 100, row == 0 ? UI_CYAN : UI_YELLOW);
}

void ui_drawSettings() {
  ui_eyebrow("05", "SETUP", 12, 38);
  ui_text("Settings.", 12, 64, FT_HEAD, UI_INK, UI_BG);

  ui_hline(12, 76, 296);
  ui_text("VOLUME", 12, 92, FT_LABEL, UI_DIM, UI_BG);
  ui_settingValue(0);
  ui_stepBtn(SET_MINUS_X, SET_VOL_Y, false);
  ui_stepBtn(SET_PLUS_X, SET_VOL_Y, true);

  ui_hline(12, 119, 296);
  ui_text("BRIGHTNESS", 12, 136, FT_LABEL, UI_DIM, UI_BG);
  ui_settingValue(1);
  ui_stepBtn(SET_MINUS_X, SET_BRI_Y, false);
  ui_stepBtn(SET_PLUS_X, SET_BRI_Y, true);

  ui_hline(12, 162, 296);
  bool on = WiFi.status() == WL_CONNECTED;
  ui_text("WIFI", 12, 177, FT_LABEL, UI_DIM, UI_BG);
  ui_text(ui_fit(on ? WiFi.SSID() : String("offline"), FT_MONO, 160), 52, 177, FT_MONO, UI_INK, UI_BG);
  ui_text("IP", 12, 190, FT_LABEL, UI_DIM, UI_BG);
  ui_text(on ? WiFi.localIP().toString() : String("-"), 52, 190, FT_MONO, UI_INK, UI_BG);
  ui_text("URL", 12, 203, FT_LABEL, UI_DIM, UI_BG);
  ui_text(MDNS_NAME ".local", 52, 203, FT_MONO, UI_CYAN, UI_BG);

  ui_outlineBtn(SET_TEST_X, SET_TEST_Y, SET_TEST_W, SET_TEST_H, UI_CYAN);
  int ty = SET_TEST_Y + SET_TEST_H / 2;
  tft.fillTriangle(SET_TEST_X + 16, ty - 5, SET_TEST_X + 16, ty + 5, SET_TEST_X + 24, ty, UI_CYAN);
  ui_text("TEST", SET_TEST_X + 48, ty + 4, FT_LABEL, UI_CYAN, UI_BG, AL_C);
}

static void ui_drawContent() {
  switch (currentScreen) {
    case SCREEN_QR:       ui_drawPay(); break;
    case SCREEN_ORDERS:   ui_drawOrders(); break;
    case SCREEN_PROMO:    ui_drawBoard(); break;
    case SCREEN_STATS:    ui_drawStats(); break;
    case SCREEN_SETTINGS: ui_drawSettings(); break;
    default: break;
  }
}

// Everything (after the payment takeover, or when the shop name changed)
void ui_drawScreen() {
  tft.fillScreen(UI_BG);
  ui_drawContent();
  ui_statusBar();
  ui_nav();
}

// Screen switch / content change: the status bar stays as it is, so only
// the content area and the nav repaint (no full-panel flash)
void ui_redrawContent() {
  tft.fillRect(0, UI_TOP, UI_W, UI_NAV_Y - UI_TOP, UI_BG);
  ui_drawContent();
  ui_nav();
}

// ---------- Payment received takeover ----------
void ui_paymentAlert(uint32_t paisa, const String& note, bool test = false) {
  const uint16_t accent = test ? UI_YELLOW : UI_GREEN;
  tft.fillScreen(UI_BG);
  ui_dotgrid(0, 0, UI_W, 92);   // grid above and below the hero, none behind it
  ui_dotgrid(0, 186, UI_W, 228);

  // Badge grows in
  for (int r = 6; r <= 24; r += 3) {
    tft.fillSmoothCircle(160, 58, r, accent, UI_BG);
    delay(16);
  }
  tft.drawWideLine(150, 59, 157, 66, 4, UI_BG, accent);
  delay(40);
  tft.drawWideLine(157, 66, 171, 51, 4, UI_BG, accent);

  ui_text(test ? "TEST · NOT A REAL PAYMENT" : "PAYMENT RECEIVED", 160, 104, FT_LABEL, accent, UI_BG, AL_C);

  // Amount: currency + big digits, centred as one group
  String digits = fmtPaisa(paisa, fmtAmt);
  uint32_t amount = paisa / 100;
  uint8_t nf = ui_w(digits, FT_HERO) > 250 ? FT_NUM : FT_HERO;
  int cw = ui_w(cfg.currency, FT_HEAD);
  int dw = ui_w(digits, nf);
  int gx = (UI_W - (cw + 8 + dw)) / 2;
  const int base = 170;

  // Count-up animation (skipped for tiny amounts)
  if (amount >= 10) {
    const int frames = 10;
    for (int f = 1; f < frames; f++) {
      float p = (float)f / frames;
      float e = 1 - (1 - p) * (1 - p) * (1 - p);
      String s = fmtAmt((uint32_t)(amount * e));
      if (paisa % 100) s += "." + two((int)((paisa % 100) * e));
      tft.fillRect(gx + cw + 8, base - 46, UI_W - (gx + cw + 8), 56, UI_BG);
      ui_text(s, gx + cw + 8, base, nf, UI_INK, UI_BG);
      if (f == 1) ui_text(cfg.currency, gx, base, FT_HEAD, UI_MUTED, UI_BG);
      delay(28);
    }
    tft.fillRect(gx + cw + 8, base - 46, UI_W - (gx + cw + 8), 56, UI_BG);
  }
  ui_text(cfg.currency, gx, base, FT_HEAD, UI_MUTED, UI_BG);
  ui_text(digits, gx + cw + 8, base, nf, UI_INK, UI_BG);

  String meta = note.length() ? ui_fit(note, FT_MONO, 200) + " · " : String("");
  meta += ui_clockText();
  ui_text(meta, 160, 200, FT_MONO, UI_MUTED, UI_BG, AL_C);
  if (test) tft.fillRect(0, 232, UI_W, 8, UI_YELLOW);
  else ui_strip(0, 232, UI_W, 8);
}

// ---------- Boot / portal / reset ----------
enum BootState { BOOT_WAIT = 0, BOOT_RUN, BOOT_OK, BOOT_FAIL };

void ui_bootStep(int i, const String& text, BootState st, int tick = 0) {
  int y = 104 + i * 22;
  tft.fillRect(12, y - 14, 296, 20, UI_BG);
  ui_text("0" + String(i + 1), 16, y, FT_LABEL, st == BOOT_WAIT ? UI_DIM : UI_CYAN, UI_BG);
  ui_text(ui_fit(text, FT_MONO, 200), 40, y, FT_MONO, st == BOOT_WAIT ? UI_DIM : UI_INK, UI_BG);
  String s;
  uint16_t c;
  switch (st) {
    case BOOT_OK:   s = "OK";   c = UI_GREEN; break;
    case BOOT_FAIL: s = "FAIL"; c = UI_RED; break;
    case BOOT_RUN:  s = (tick % 3 == 0) ? "·  " : (tick % 3 == 1) ? "·· " : "···"; c = UI_YELLOW; break;
    default:        s = "-";    c = UI_DIM; break;
  }
  ui_text(s, 304, y, FT_LABEL, c, UI_BG, AL_R);
  ui_hline(16, y + 7, 288, UI_SURF);
}

void ui_bootScreen() {
  tft.fillScreen(UI_BG);
  ui_strip(0, 0, UI_W, 4);
  ui_text("BOOTING · V" FW_VERSION, 16, 34, FT_LABEL, UI_CYAN, UI_BG);
  String name = shopName();
  ui_text(ui_fit(name + ".", FT_HEAD, 288), 16, 62, FT_HEAD, UI_INK, UI_BG);
  ui_text("QR payments, read out loud.", 16, 80, FT_MONO, UI_DIM, UI_BG);
  ui_bootStep(0, "Loading config", BOOT_OK);
  ui_bootStep(1, cfg.ssid.length() ? "Joining " + cfg.ssid : String("Joining Wi-Fi"), BOOT_WAIT);
  ui_bootStep(2, "Syncing clock", BOOT_WAIT);
  ui_bootStep(3, "Listening for payments", BOOT_WAIT);
  ui_text("27.7° N, 85.3° E", 16, 228, FT_LABEL, UI_DIM, UI_BG);
  ui_text(ui_upper(MDNS_NAME ".local"), 304, 228, FT_LABEL, UI_DIM, UI_BG, AL_R);
}

void ui_portal() {
  tft.fillScreen(UI_BG);
  ui_dotgrid(0, 0, UI_W, 232);
  ui_text("SETUP MODE", 16, 34, FT_LABEL, UI_YELLOW, UI_BG);
  ui_text("Set me up.", 16, 62, FT_HEAD, UI_INK, UI_BG);

  const char* titles[3] = {"Join this Wi-Fi", "A page pops up", "Add QR + Gmail"};
  String details[3] = {AP_NAME, "or open 192.168.4.1", "done in a minute"};
  for (int i = 0; i < 3; i++) {
    int y = 94 + i * 44;
    ui_text("0" + String(i + 1), 16, y, FT_LABEL, UI_CYAN, UI_BG);
    ui_text(titles[i], 38, y + 1, FT_BODY, UI_INK, UI_BG);
    ui_text(details[i], 38, y + 18, FT_MONO, i == 0 ? UI_CYAN : UI_MUTED, UI_BG);
  }

  const int qx = 188, qy = 40, qs = 120;
  tft.fillSmoothRoundRect(qx, qy, qs, qs, 10, UI_PAPER, UI_BG);
  ui_qr("WIFI:T:nopass;S:" AP_NAME ";;", qx + 6, qy + 6, qs - 12, UI_QRINK, UI_PAPER);
  ui_text("SCAN TO JOIN", qx + qs / 2, qy + qs + 18, FT_LABEL, UI_MUTED, UI_BG, AL_C);
  ui_strip(0, 236, UI_W, 4);
}

void ui_resetScreen() {
  tft.fillScreen(UI_BG);
  ui_strip(0, 0, UI_W, 4);
  ui_text("FACTORY RESET", 160, 108, FT_LABEL, UI_RED, UI_BG, AL_C);
  ui_text("Wiping settings.", 160, 138, FT_HEAD, UI_INK, UI_BG, AL_C);
}
