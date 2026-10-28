// Desktop TFT_eSPI stand-in: renders into a 320x240 RGB565 framebuffer.
// Smooth-font parsing, datum handling, textWidth and alphaBlend follow
// TFT_eSPI 2.5.x so text lands where it will on the panel.
#pragma once
#include "Arduino.h"
#include <vector>

#define SMOOTH_FONT
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8
#define L_BASELINE 9
#define C_BASELINE 10
#define R_BASELINE 11
#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF

class TFT_eSPI {
 public:
  int W = 320, H = 240;
  std::vector<uint16_t> fb;
  TFT_eSPI() : fb(320 * 240, 0) {}

  void init() {}
  void setRotation(uint8_t) {}
  // ---------- pixels ----------
  void drawPixel(int32_t x, int32_t y, uint32_t c) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    fb[y * W + x] = (uint16_t)c;
  }
  uint16_t readPixel(int32_t x, int32_t y) { return (x < 0 || y < 0 || x >= W || y >= H) ? 0 : fb[y * W + x]; }
  static uint16_t alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc) {
    uint32_t rxb = bgc & 0xF81F;
    rxb += ((fgc & 0xF81F) - rxb) * (alpha >> 2) >> 6;
    uint32_t xgx = bgc & 0x07E0;
    xgx += ((fgc & 0x07E0) - xgx) * alpha >> 8;
    return (rxb & 0xF81F) | (xgx & 0x07E0);
  }
  void blendPixel(int x, int y, uint16_t c, float cov, uint32_t bg) {
    if (cov <= 0) return;
    if (cov >= 1) { drawPixel(x, y, c); return; }
    uint16_t b = (bg == 0x00FFFFFF) ? readPixel(x, y) : (uint16_t)bg;
    drawPixel(x, y, alphaBlend((uint8_t)(cov * 255 + 0.5f), c, b));
  }

  // ---------- basic shapes ----------
  void fillScreen(uint32_t c) { std::fill(fb.begin(), fb.end(), (uint16_t)c); }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    drawFastHLine(x, y, w, c); drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c); drawFastVLine(x + w - 1, y, h, c);
  }
  void fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t c) {
    int minx = std::min({x1, x2, x3}), maxx = std::max({x1, x2, x3});
    int miny = std::min({y1, y2, y3}), maxy = std::max({y1, y2, y3});
    auto e = [](float ax, float ay, float bx, float by, float px, float py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
      float a = e(x1, y1, x2, y2, x, y), b = e(x2, y2, x3, y3, x, y), d = e(x3, y3, x1, y1, x, y);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) drawPixel(x, y, c);
    }
  }
  void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r) drawPixel(x0 + x, y0 + y, c);
  }

  // ---------- anti-aliased shapes (coverage by 4x4 supersampling) ----------
  template <class F> void aa(int x0, int y0, int x1, int y1, uint16_t c, uint32_t bg, F inside) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
      int n = 0;
      for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++)
        if (inside(x + (sx + 0.5f) / 4 - 0.5f, y + (sy + 0.5f) / 4 - 0.5f)) n++;
      if (n) blendPixel(x, y, c, n / 16.0f, bg);
    }
  }
  static bool inRR(float px, float py, float x, float y, float w, float h, float r) {
    // pixel-centre coords; rect spans [x, x+w-1]
    float cx = std::min(std::max(px, x + r), x + w - 1 - r);
    float cy = std::min(std::max(py, y + r), y + h - 1 - r);
    if (px < x - 0.5f || py < y - 0.5f || px > x + w - 0.5f || py > y + h - 0.5f) return false;
    float dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy <= (r + 0.5f) * (r + 0.5f);
  }
  void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c, uint32_t bg = 0x00FFFFFF) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    aa(x, y, x + w - 1, y + h - 1, c, bg, [&](float px, float py) { return inRR(px, py, x, y, w, h, r - 0.5f); });
  }
  void drawSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir, int32_t w, int32_t h, uint32_t fg, uint32_t bg = 0x00FFFFFF, uint8_t = 0xF) {
    float t = r - ir + 1;
    aa(x, y, x + w - 1, y + h - 1, fg, bg, [&](float px, float py) {
      return inRR(px, py, x, y, w, h, r - 0.5f) && !inRR(px, py, x + t, y + t, w - 2 * t, h - 2 * t, std::max(0.0f, r - t - 0.5f));
    });
  }
  void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t c, uint32_t bg = 0x00FFFFFF) {
    aa(x - r - 1, y - r - 1, x + r + 1, y + r + 1, c, bg, [&](float px, float py) {
      return (px - x) * (px - x) + (py - y) * (py - y) <= (r + 0.5f) * (r + 0.5f);
    });
  }
  void drawSpot(float ax, float ay, float r, uint32_t c, uint32_t bg = 0x00FFFFFF) {
    aa((int)(ax - r - 1), (int)(ay - r - 1), (int)(ax + r + 1), (int)(ay + r + 1), c, bg, [&](float px, float py) {
      return (px - ax) * (px - ax) + (py - ay) * (py - ay) <= r * r;
    });
  }
  void drawWideLine(float ax, float ay, float bx, float by, float wd, uint32_t c, uint32_t bg = 0x00FFFFFF) {
    float r = wd / 2;
    aa((int)(std::min(ax, bx) - r - 1), (int)(std::min(ay, by) - r - 1), (int)(std::max(ax, bx) + r + 1), (int)(std::max(ay, by) + r + 1), c, bg,
       [&](float px, float py) {
         float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
         float t = l2 > 0 ? std::min(1.0f, std::max(0.0f, ((px - ax) * vx + (py - ay) * vy) / l2)) : 0;
         float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
         return dx * dx + dy * dy <= r * r;
       });
  }

  // ---------- smooth fonts (VLW from PROGMEM) ----------
  // Same public members and semantics as TFT_eSPI's Smooth_font.h, so code that
  // caches loaded fonts by swapping gFont + the g* arrays works here too.
  typedef struct {
    const uint8_t* gArray; uint16_t gCount; uint16_t yAdvance; uint16_t spaceWidth;
    int16_t ascent; int16_t descent; uint16_t maxAscent; uint16_t maxDescent;
  } fontMetrics;
  fontMetrics gFont = {nullptr, 0, 0, 0, 0, 0, 0, 0};
  uint16_t* gUnicode = nullptr; uint8_t* gHeight = nullptr; uint8_t* gWidth = nullptr; uint8_t* gxAdvance = nullptr;
  int16_t* gdY = nullptr; int8_t* gdX = nullptr; uint32_t* gBitmap = nullptr;
  bool fontLoaded = false;
  int loadCount = 0;   // how many times a font was parsed (for tests)
  uint16_t textcolor = 0xFFFF, textbgcolor = 0;
  uint8_t textdatum = 0;
  int32_t cursor_x = 0, cursor_y = 0;

  static int32_t rd32(const uint8_t*& p) { int32_t v = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4; return v; }
  void loadFont(const uint8_t* arr) {
    if (fontLoaded) unloadFont();
    loadCount++;
    const uint8_t* p = arr;
    gFont.gArray = arr;
    gFont.gCount = (uint16_t)rd32(p); rd32(p); gFont.yAdvance = (uint16_t)rd32(p); rd32(p);
    gFont.ascent = (int16_t)rd32(p); gFont.descent = (int16_t)rd32(p);
    gFont.maxAscent = gFont.ascent; gFont.maxDescent = gFont.descent;
    int n = gFont.gCount;
    gUnicode = (uint16_t*)malloc(n * 2); gHeight = (uint8_t*)malloc(n); gWidth = (uint8_t*)malloc(n);
    gxAdvance = (uint8_t*)malloc(n); gdY = (int16_t*)malloc(n * 2); gdX = (int8_t*)malloc(n); gBitmap = (uint32_t*)malloc(n * 4);
    uint32_t bmp = 24 + n * 28;
    for (int i = 0; i < n; i++) {
      gUnicode[i] = (uint16_t)rd32(p); gHeight[i] = (uint8_t)rd32(p); gWidth[i] = (uint8_t)rd32(p);
      gxAdvance[i] = (uint8_t)rd32(p); gdY[i] = (int16_t)rd32(p); gdX[i] = (int8_t)rd32(p); rd32(p);
      if (((int16_t)gHeight[i] - gdY[i]) > (int16_t)gFont.maxDescent &&
          ((gUnicode[i] > 0x20 && gUnicode[i] < 0xA0 && gUnicode[i] != 0x7F) || gUnicode[i] > 0xFF))
        gFont.maxDescent = gHeight[i] - gdY[i];
      gBitmap[i] = bmp; bmp += gWidth[i] * gHeight[i];
    }
    gFont.yAdvance = gFont.maxAscent + gFont.maxDescent;
    gFont.spaceWidth = (gFont.ascent + gFont.descent) * 2 / 7;
    fontLoaded = true;
  }
  void unloadFont() {
    free(gUnicode); free(gHeight); free(gWidth); free(gxAdvance); free(gdY); free(gdX); free(gBitmap);
    gUnicode = nullptr; gHeight = gWidth = gxAdvance = nullptr; gdY = nullptr; gdX = nullptr; gBitmap = nullptr;
    gFont.gArray = nullptr;
    fontLoaded = false;
  }
  void setTextFont(uint8_t) {}
  void setTextColor(uint16_t fg, uint16_t bg, bool = false) { textcolor = fg; textbgcolor = bg; }
  void setTextDatum(uint8_t d) { textdatum = d; }
  int16_t fontHeight() { return gFont.yAdvance; }

  static std::vector<uint16_t> decode(const char* s) {
    std::vector<uint16_t> out;
    const uint8_t* p = (const uint8_t*)s;
    while (*p) {
      uint16_t c = *p++;
      if ((c & 0xE0) == 0xC0 && *p) { c = ((c & 0x1F) << 6) | (*p++ & 0x3F); }
      else if ((c & 0xF0) == 0xE0 && p[0] && p[1]) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
      out.push_back(c);
    }
    return out;
  }
  int find(uint16_t u) { for (int i = 0; i < gFont.gCount; i++) if (gUnicode[i] == u) return i; return -1; }

  int16_t textWidth(const String& str) {
    auto cs = decode(str.c_str());
    int w = 0;
    for (size_t i = 0; i < cs.size(); i++) {
      uint16_t u = cs[i];
      if (!u) continue;
      if (u == 0x20) { w += gFont.spaceWidth; continue; }
      int g = find(u);
      if (g >= 0) {
        if (w == 0 && gdX[g] < 0) w -= gdX[g];
        if (i + 1 < cs.size()) w += gxAdvance[g]; else w += gdX[g] + gWidth[g];
      } else w += gFont.spaceWidth + 1;
    }
    return w;
  }
  void drawGlyph(uint16_t code) {
    if (code == 0x20) { cursor_x += gFont.spaceWidth; return; }
    int g = find(code);
    if (g < 0) {
      drawRect(cursor_x, cursor_y + gFont.maxAscent - gFont.ascent, gFont.spaceWidth, gFont.ascent, textcolor);
      cursor_x += gFont.spaceWidth + 1;
      return;
    }
    if (cursor_x == 0) cursor_x -= gdX[g];
    int cy = cursor_y + gFont.maxAscent - gdY[g], cx = cursor_x + gdX[g];
    for (int y = 0; y < gHeight[g]; y++) for (int x = 0; x < gWidth[g]; x++) {
      uint8_t a = gFont.gArray[gBitmap[g] + x + gWidth[g] * y];
      if (!a) continue;
      drawPixel(cx + x, cy + y, a == 0xFF ? textcolor : alphaBlend(a, textcolor, textbgcolor));
    }
    cursor_x += gxAdvance[g];
  }
  int16_t drawString(const String& str, int32_t x, int32_t y) {
    int cw = textWidth(str), ch = fontHeight(), base = gFont.maxAscent;
    switch (textdatum) {
      case TC_DATUM: x -= cw / 2; break;
      case TR_DATUM: x -= cw; break;
      case ML_DATUM: y -= ch / 2; break;
      case MC_DATUM: x -= cw / 2; y -= ch / 2; break;
      case MR_DATUM: x -= cw; y -= ch / 2; break;
      case BL_DATUM: y -= ch; break;
      case BC_DATUM: x -= cw / 2; y -= ch; break;
      case BR_DATUM: x -= cw; y -= ch; break;
      case L_BASELINE: y -= base; break;
      case C_BASELINE: x -= cw / 2; y -= base; break;
      case R_BASELINE: x -= cw; y -= base; break;
    }
    cursor_x = x; cursor_y = y;
    for (uint16_t u : decode(str.c_str())) drawGlyph(u);
    return cw;
  }

  // ---------- output ----------
  void savePPM(const char* path) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (uint16_t c : fb) {
      uint8_t r = ((c >> 11) & 0x1F) * 255 / 31, g = ((c >> 5) & 0x3F) * 255 / 63, b = (c & 0x1F) * 255 / 31;
      fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
  }
};
