#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "qr_encode.h"   // ricmoo/QRCode, vendored (see file)
#include <ESP8266SAM.h>
#include <esp_system.h>
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #include <esp_random.h>
#endif
#include "textutil.h"

// Fallback font handling
#ifdef LOAD_FONT4
  #define FONT_BIG 4
#else
  #define FONT_BIG 2
#endif

// ==========================================
// CYD Hardware Pin Definitions (ESP32-2432S028)
// ==========================================
#define CYD_T_MOSI 32
#define CYD_T_MISO 39
#define CYD_T_CLK  25
#define CYD_T_CS   33
#define CYD_T_IRQ  36
#define CYD_BACKLIGHT 21

// ==========================================
// Robust CYD Touch Driver (XPT2046 Bit-Bang)
// ==========================================
class CYDTouch {
private:
  uint16_t transfer(uint8_t cmd) {
    digitalWrite(CYD_T_CS, LOW);
    delayMicroseconds(2);

    // 1. Send 8-bit command (MSB first)
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CYD_T_MOSI, (cmd >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(CYD_T_CLK, HIGH);
      delayMicroseconds(2);
      digitalWrite(CYD_T_CLK, LOW);
      delayMicroseconds(1);
    }

    // 2. 9th clock: XPT2046 conversion dummy / BUSY bit
    digitalWrite(CYD_T_CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(CYD_T_CLK, LOW);
    delayMicroseconds(1);

    // 3. Read 12-bit ADC result (MSB first)
    uint16_t val = 0;
    for (int i = 11; i >= 0; i--) {
      digitalWrite(CYD_T_CLK, HIGH);
      delayMicroseconds(2);
      if (digitalRead(CYD_T_MISO)) {
        val |= (1 << i);
      }
      digitalWrite(CYD_T_CLK, LOW);
      delayMicroseconds(1);
    }

    digitalWrite(CYD_T_CS, HIGH);
    delayMicroseconds(2);
    return val;
  }

public:
  void begin() {
    pinMode(CYD_T_CS,   OUTPUT);
    pinMode(CYD_T_CLK,  OUTPUT);
    pinMode(CYD_T_MOSI, OUTPUT);
    pinMode(CYD_T_MISO, INPUT);
    pinMode(CYD_T_IRQ,  INPUT);

    digitalWrite(CYD_T_CS, HIGH);
    digitalWrite(CYD_T_CLK, LOW);
    digitalWrite(CYD_T_MOSI, LOW);
  }

  bool getRaw(int &x, int &y, int &z) {
    // Read Pressure Z1 and Z2 (Commands 0xB0 and 0xC0)
    uint16_t z1 = transfer(0xB0);
    uint16_t z2 = transfer(0xC0);
    z = (int)z1 + 4095 - (int)z2;

    // Detect touch via pressure Z OR hardware IRQ pin
    bool isDown = (digitalRead(CYD_T_IRQ) == LOW) || (z > 350) || (z1 > 120 && z1 < 3900);
    if (!isDown) return false;

    // Read X (0xD0) and Y (0x90) with oversampling
    int x1 = transfer(0xD0);
    int y1 = transfer(0x90);
    int x2 = transfer(0xD0);
    int y2 = transfer(0x90);

    if (abs(x1 - x2) > 150 || abs(y1 - y2) > 150) {
      x = x2;
      y = y2;
    } else {
      x = (x1 + x2) / 2;
      y = (y1 + y2) / 2;
    }

    return (x > 150 && x < 3950 && y > 150 && y < 3950);
  }

  bool getScreenTouch(int &screenX, int &screenY, bool swap, bool invX, bool invY) {
    int rx, ry, rz;
    if (!getRaw(rx, ry, rz)) return false;

    int mx, my;
    if (swap) {
      mx = map(ry, 240, 3800, 0, 320);
      my = map(rx, 240, 3800, 0, 240);
    } else {
      mx = map(rx, 240, 3800, 0, 320);
      my = map(ry, 240, 3800, 0, 240);
    }

    if (invX) mx = 319 - mx;
    if (invY) my = 239 - my;

    screenX = constrain(mx, 0, 319);
    screenY = constrain(my, 0, 239);

    static uint32_t lastDbg = 0;
    if (millis() - lastDbg > 150) {
      lastDbg = millis();
      Serial.printf("[TOUCH] Raw X=%d Y=%d Z=%d => Screen (%d, %d)\n", rx, ry, rz, screenX, screenY);
    }

    return true;
  }
};


// ==========================================
// Globals
// ==========================================
TFT_eSPI tft;
CYDTouch touch;
WebServer server(80);
DNSServer dns;
Preferences prefs;

#include "model.h"
#include "ui.h"
#include "speaker.h"
#include "imap.h"
#include "net.h"

bool portalMode = false;
bool timeSynced = false;
ImapState imapState = {0, 0};   // where the IMAP reader is in the mailbox (NVS guidv/guid)
String appsScriptSince = "0";   // Apps Script cursor (NVS since)
volatile uint32_t pendingAmt = 0;
volatile bool pendingRefresh = false;
uint32_t lastUserActivity = 0;
uint32_t lastCycleTick = 0;

// Forward declarations
void drawScreen();
void announce(uint32_t paisa, const String& note = "");
void setScreen(int s);
void setBacklight(int percent);

// ---------- Audio Gain Scaler (Prevents 3.3V Sag & Display Flicker) ----------
float getAudioGain() {
  return constrain(((float)cfg.volume / 100.0f) * 0.40f, 0.02f, 0.45f);
}

// ---------- Voice: SAM speech straight to the DAC on GPIO26 (see speaker.h) ----------
void playVoice(const String& phrase) {
  CYDSpeaker out;
  out.SetGain(getAudioGain());
  ESP8266SAM sam;
  sam.Say(&out, phrase.c_str());
  out.stop();
}

// ---------- Display Backlight PWM Dimmer ----------
void setBacklight(int percent) {
  percent = constrain(percent, 10, 100);
  int duty = map(percent, 10, 100, 25, 255);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(CYD_BACKLIGHT, duty);
#else
  ledcWrite(0, duty);
#endif
}

void initBacklight() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(CYD_BACKLIGHT, 5000, 8);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(CYD_BACKLIGHT, 0);
#endif
  setBacklight(cfg.brightness);
}

// ---------- Storage (Preferences) ----------
// 128-bit random token for the REST API (hardware RNG; Wi-Fi/BT is up or the
// bootloader seeded it, either way it's good entropy on the ESP32)
String newApiToken() {
  String t;
  char b[9];
  for (int i = 0; i < 4; i++) { snprintf(b, sizeof b, "%08lx", (unsigned long)esp_random()); t += b; }
  return t;
}

void loadCfg() {
  prefs.begin("cydbiz", true);
  cfg.bizName      = prefs.getString("bname", "Business Hub");
  cfg.currency     = prefs.getString("curr", "Rs");
  cfg.storeNote    = prefs.getString("snote", "Scan the QR to pay. Instant voice confirmation.");
  cfg.storeOpen    = prefs.getBool("sopen", true);
  cfg.qr           = prefs.getString("qr", "");
  cfg.qrSub        = prefs.getString("qrsub", "Scan with eSewa, Khalti or any bank app");
  cfg.volume       = prefs.getInt("vol", 25);
  cfg.brightness   = prefs.getInt("bright", 80);
  cfg.voiceMode    = prefs.getInt("vmode", 0);
  cfg.autoCycle    = prefs.getBool("cycle", true);
  cfg.cycleSecs    = prefs.getInt("csec", 15);
  cfg.emailMode    = prefs.getInt("emode", 0);
  cfg.gmailUser    = prefs.getString("guser", "");
  cfg.gmailPass    = prefs.getString("gpass", "");
  cfg.imapQuery    = prefs.getString("query", "UNSEEN");
  cfg.verifyGoogle = prefs.getBool("gtls", true);
  imapState.uidValidity = prefs.getULong("guidv", 0);
  imapState.lastUid     = prefs.getULong("guid", 0);
  appsScriptSince       = prefs.getString("since", "0");
  cfg.url          = prefs.getString("url", "");
  cfg.key          = prefs.getString("key", "");
  cfg.ssid         = prefs.getString("ssid", "");
  cfg.pass         = prefs.getString("pass", "");
  cfg.adminPin     = prefs.getString("pin", "");
  cfg.swapTouch    = prefs.getBool("tswap", false);
  cfg.invertTouchX = prefs.getBool("tinvx", false);
  cfg.invertTouchY = prefs.getBool("tinvy", false);
  todaySales       = prefs.getUInt("sales", 0);
  salesPaisa       = prefs.getUChar("salesp", 0) % 100;
  totalTransactions= prefs.getUInt("txcount", 0);
  salesDay         = prefs.getInt("sday", -1);
  lastPayHM        = prefs.getString("lastpay", "");
  if (prefs.getBytesLength("hourly") == sizeof(hourly)) prefs.getBytes("hourly", hourly, sizeof(hourly));
  prefs.end();

  cfg.apiToken     = prefs.getString("apitok", "");
  if (cfg.apiToken.length() != 32) {
    cfg.apiToken = newApiToken();
    prefs.begin("cydbiz", false);
    prefs.putString("apitok", cfg.apiToken);
    prefs.end();
  }

  if (cfg.volume < 5 || cfg.volume > 100) cfg.volume = 25;
  if (cfg.brightness < 10 || cfg.brightness > 100) cfg.brightness = 80;
  if (cfg.cycleSecs < 5 || cfg.cycleSecs > 120) cfg.cycleSecs = 15;
}

void saveCfg() {
  prefs.begin("cydbiz", false);
  prefs.putString("bname", cfg.bizName);
  prefs.putString("curr", cfg.currency);
  prefs.putString("snote", cfg.storeNote);
  prefs.putBool("sopen", cfg.storeOpen);
  prefs.putString("qr", cfg.qr);
  prefs.putString("qrsub", cfg.qrSub);
  prefs.putInt("vol", cfg.volume);
  prefs.putInt("bright", cfg.brightness);
  prefs.putInt("vmode", cfg.voiceMode);
  prefs.putBool("cycle", cfg.autoCycle);
  prefs.putInt("csec", cfg.cycleSecs);
  prefs.putInt("emode", cfg.emailMode);
  prefs.putString("guser", cfg.gmailUser);
  prefs.putString("gpass", cfg.gmailPass);
  prefs.putString("query", cfg.imapQuery);
  prefs.putBool("gtls", cfg.verifyGoogle);
  prefs.putString("url", cfg.url);
  prefs.putString("key", cfg.key);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("pin", cfg.adminPin);
  prefs.putString("apitok", cfg.apiToken);
  prefs.putBool("tswap", cfg.swapTouch);
  prefs.putBool("tinvx", cfg.invertTouchX);
  prefs.putBool("tinvy", cfg.invertTouchY);
  prefs.end();
}

void saveSales() {
  prefs.begin("cydbiz", false);
  prefs.putUInt("sales", todaySales);
  prefs.putUChar("salesp", salesPaisa);
  prefs.putUInt("txcount", totalTransactions);
  prefs.putInt("sday", salesDay);
  prefs.putString("lastpay", lastPayHM);
  prefs.putBytes("hourly", hourly, sizeof(hourly));
  prefs.end();
}

// Reset today's numbers when the date changes (needs the clock)
void checkDayRollover() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return;
  if (salesDay == t.tm_yday) return;
  if (salesDay != -1) {
    todaySales = 0;
    salesPaisa = 0;
    totalTransactions = 0;
    lastPayHM = "";
    memset(hourly, 0, sizeof(hourly));
  }
  salesDay = t.tm_yday;
  saveSales();
  if (!portalMode && currentScreen == SCREEN_STATS) drawScreen();
}

// Count a payment towards today's numbers. Totals are whole rupees with the
// paisa carried separately, so 0.50 + 0.50 adds up to one rupee.
void recordPayment(uint32_t paisa) {
  if (!paisa) return;
  uint32_t p = salesPaisa + paisa % 100;
  todaySales += paisa / 100 + p / 100;
  salesPaisa = p % 100;
  totalTransactions++;
  struct tm t;
  if (getLocalTime(&t, 0)) {
    hourly[t.tm_hour] += (paisa + 50) / 100;
    lastPayHM = two(t.tm_hour) + ":" + two(t.tm_min);
  }
  saveSales();
}

void addOrder(const String& title, uint32_t amount, const String& status = "NEW") {
  if (orderCount < MAX_ORDERS) {
    orders[orderCount++] = { nextOrderId++, title, amount, status };
  } else {
    for (int i = 0; i < MAX_ORDERS - 1; i++) orders[i] = orders[i+1];
    orders[MAX_ORDERS - 1] = { nextOrderId++, title, amount, status };
  }
  if (status == "PAID") recordPayment(amount);
  if (!portalMode && (currentScreen == SCREEN_ORDERS || currentScreen == SCREEN_STATS)) {
    drawScreen();
  }
}

// ==========================================
// Screen plumbing (drawing lives in ui.h)
// ==========================================
void drawScreen() {
  ui_drawScreen();
}

void setScreen(int s) {
  if (s < 0) s = SCREEN_COUNT - 1;
  if (s >= SCREEN_COUNT) s = 0;
  currentScreen = (Screen)s;
  lastUserActivity = millis();
  drawScreen();
}

void setNet(NetState s) {
  if (netState == s) return;
  netState = s;
  if (!portalMode) ui_drawDot();
}

// ==========================================
// Announcements Dispatcher
// ==========================================
void testVoice() {
  playVoice("volume " + String(cfg.volume) + " percent ready");
}

void announce(uint32_t paisa, const String& note) {
  ui_paymentAlert(paisa, note);

  String phrase;
  String said = sayAmount(paisa);
  if (cfg.voiceMode == 0) {
    phrase = (paisa >= 100 ? "rupees " : "") + said + " received";
  } else if (cfg.voiceMode == 1) {
    phrase = "payment of " + said + " confirmed, thank you";
  } else {
    phrase = said + " received";
  }
  playVoice(phrase);

  delay(1800);
  lastUserActivity = millis();
  drawScreen();
}

// ==========================================
// Events from the network task (see net.h)
// ==========================================
void handleNetEvents() {
  NetEvent e;
  while (netQueue && xQueueReceive(netQueue, &e, 0) == pdTRUE) {
    if (e.type == EV_PAYMENT) {
      addOrder(e.title[0] ? String(e.title) : String("Payment"), e.paisa, "PAID");
      if (!e.stale) announce(e.paisa, e.note);
    }
  }
}

// ==========================================
// Touch & Gestures Handling
// ==========================================
bool hit(int x, int y, int bx, int by, int bw, int bh) {
  return x >= bx - 4 && x <= bx + bw + 4 && y >= by - 4 && y <= by + bh + 4;
}

void handleTap(int x, int y) {
  lastUserActivity = millis();

  // Bottom nav
  if (y >= UI_NAV_Y) {
    int tab = x / 64;
    if (tab >= 0 && tab < SCREEN_COUNT) setScreen(tab);
    return;
  }

  if (currentScreen == SCREEN_SETTINGS) {
    if (hit(x, y, SET_MINUS_X, SET_VOL_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.volume = max(5, cfg.volume - 5);
      saveCfg(); drawScreen();
    } else if (hit(x, y, SET_PLUS_X, SET_VOL_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.volume = min(100, cfg.volume + 5);
      saveCfg(); drawScreen();
    } else if (hit(x, y, SET_MINUS_X, SET_BRI_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.brightness = max(10, cfg.brightness - 10);
      setBacklight(cfg.brightness); saveCfg(); drawScreen();
    } else if (hit(x, y, SET_PLUS_X, SET_BRI_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.brightness = min(100, cfg.brightness + 10);
      setBacklight(cfg.brightness); saveCfg(); drawScreen();
    } else if (hit(x, y, SET_TEST_X, SET_TEST_Y, SET_TEST_W, SET_TEST_H)) {
      testVoice();
    }
  } else if (currentScreen == SCREEN_STATS) {
    if (x >= 200 && y >= 36 && y <= 76) { // store switch
      cfg.storeOpen = !cfg.storeOpen;
      saveCfg(); drawScreen();
    }
  } else if (currentScreen == SCREEN_ORDERS) {
    for (int i = 0; i < orderCount && i < ORD_ROWS; i++) {
      int oy = ORD_LIST_Y + i * ORD_ROW_H;
      if (y >= oy && y < oy + ORD_ROW_H) {
        if (orders[i].status == "NEW") orders[i].status = "PREP";
        else if (orders[i].status == "PREP") orders[i].status = "READY";
        else if (orders[i].status == "READY") orders[i].status = "PAID";
        else orders[i].status = "NEW";
        drawScreen();
        break;
      }
    }
  } else if (currentScreen == SCREEN_PROMO) {
    cfg.storeOpen = !cfg.storeOpen;
    saveCfg();
    drawScreen();
  }
}

void handleTouch() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 20) return;
  lastCheck = millis();

  int x, y;
  bool isDown = touch.getScreenTouch(x, y, cfg.swapTouch, cfg.invertTouchX, cfg.invertTouchY);

  static bool wasDown = false;
  static int startX = 0, startY = 0;
  static int lastX = 0, lastY = 0;
  static uint32_t downTime = 0;

  if (isDown) {
    if (!wasDown) {
      wasDown = true;
      startX = x;
      startY = y;
      lastX = x;
      lastY = y;
      downTime = millis();
    } else {
      lastX = x;
      lastY = y;
    }
  } else {
    if (wasDown) {
      wasDown = false;
      int dx = lastX - startX;
      int dy = lastY - startY;
      uint32_t dur = millis() - downTime;

      // Swipe detected: horizontal swipe > 35px
      if (abs(dx) > 35 && abs(dx) > abs(dy) * 1.2 && dur < 900) {
        if (dx < 0) {
          setScreen(currentScreen + 1); // Swipe Left -> Next
        } else {
          setScreen(currentScreen - 1); // Swipe Right -> Prev
        }
      } else if (abs(dx) < 30 && abs(dy) < 30 && dur < 700) {
        handleTap(startX, startY);      // Tap / Click
      }
    }
  }
}

// ==========================================
// Phone dashboard (HTML lives in web.h)
// ==========================================
#include "web.h"

// Captive Portal Auto-Detect Handlers for iOS / Android / Windows
void handleCaptiveRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "Redirecting to setup");
}

// ==========================================
// WiFi Management & Setup Mode
// ==========================================
bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  ui_bootScreen();

  String label = "Joining " + cfg.ssid;
  uint32_t t = millis();
  int tick = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t < 18000) {
    ui_bootStep(1, label, BOOT_RUN, tick++);
    delay(250);
  }
  bool ok = WiFi.status() == WL_CONNECTED;
  ui_bootStep(1, label, ok ? BOOT_OK : BOOT_FAIL);
  if (!ok) { delay(600); return false; }

  // Clock: Nepal time (UTC+5:45)
  configTzTime("<+0545>-5:45", "pool.ntp.org", "time.google.com");
  struct tm tmv;
  t = millis();
  tick = 0;
  while (!getLocalTime(&tmv, 0) && millis() - t < 6000) {
    ui_bootStep(2, "Syncing clock", BOOT_RUN, tick++);
    delay(250);
  }
  timeSynced = getLocalTime(&tmv, 0);
  ui_bootStep(2, timeSynced ? "Clock " + ui_clockText() : String("Clock (will retry)"), timeSynced ? BOOT_OK : BOOT_FAIL);
  if (timeSynced) checkDayRollover();

  ui_bootStep(3, "Listening for payments", BOOT_OK);
  delay(500);
  return true;
}

void startPortal() {
  portalMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME);
  dns.start(53, "*", WiFi.softAPIP());
  ui_portal();
}

void handleButton() {
  static uint32_t downAt = 0;
  if (digitalRead(0) == LOW) {
    if (!downAt) downAt = millis();
    if (millis() - downAt > 3500) {
      prefs.begin("cydbiz", false); prefs.clear(); prefs.end();
      ui_resetScreen();
      delay(1200);
      ESP.restart();
    }
  } else downAt = 0;
}

// ==========================================
// Main Arduino Setup & Loop
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[Starting BizBox]");

  pinMode(0, INPUT_PULLUP);

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(UI_BG);

  // Load Business Config
  loadCfg();

  // Initialize Backlight PWM
  initBacklight();

  // Initialize Touch Driver (GPIO 25 is clean and unencumbered!)
  touch.begin();

  formNonce = newApiToken();   // per-boot form nonce (web.h)

  // Web routes & REST API
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/test", HTTP_GET, handleTest);
  // REST API: every /api/* needs "Authorization: Bearer <token>" (see web.h)
  server.on("/api/state", HTTP_GET, handleApiState);
  server.on("/api/order", HTTP_POST, handleApiOrder);
  server.on("/api/payment", HTTP_POST, handleApiPayment);
  server.on("/api/reset", HTTP_ANY, handleApiReset);   // POST from the dashboard, GET kept for old scripts
  server.on("/api/clear", HTTP_ANY, handleApiClear);

  // Captive Portal Detection Routes (for automatic popup on phone)
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect); // Apple iOS
  server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);        // Android
  server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);            // Windows
  server.onNotFound(handleCaptiveRedirect);

  if (cfg.ssid.isEmpty() || !connectWifi()) {
    startPortal();
  } else {
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
    }
    netState = NET_OK;
    drawScreen();
  }
  netBegin(imapState, appsScriptSince);

  server.begin();
  lastUserActivity = millis();
  lastCycleTick = millis();
  Serial.println("[BizBox ready]");
}

void loop() {
  server.handleClient();
  if (portalMode) dns.processNextRequest();
  handleButton();
  handleTouch();

  // Instant Voice Test Dispatcher
  if (pendingAmt) {
    uint32_t a = pendingAmt;
    pendingAmt = 0;
    addOrder("Voice test", a * 100, "PAID");
    announce(a * 100, "Test");
  }

  if (pendingRefresh) {
    pendingRefresh = false;
    drawScreen();
    netPollNow = true;
  }

  handleNetEvents();

  if (!portalMode) {
    // Live clock in the status bar
    static uint32_t lastSec = 0;
    if (millis() - lastSec >= 1000) {
      lastSec = millis();
      ui_drawClock();
    }

    // Day rollover + late clock sync
    static uint32_t lastDayCheck = 0;
    if (millis() - lastDayCheck > 30000) {
      lastDayCheck = millis();
      checkDayRollover();
    }

    // Kiosk Auto-Cycle (flips between QR and Board when the customer is idle)
    if (cfg.autoCycle) {
      uint32_t idle = (uint32_t)cfg.cycleSecs * 1000UL;
      if (millis() - lastUserActivity > idle && millis() - lastCycleTick > idle) {
        lastCycleTick = millis();
        if (currentScreen == SCREEN_QR) {
          currentScreen = SCREEN_PROMO;
          drawScreen();
        } else if (currentScreen == SCREEN_PROMO) {
          currentScreen = SCREEN_QR;
          drawScreen();
        }
      }
    }

    // Polling runs in the network task; the loop only shows its state
    if (WiFi.status() != WL_CONNECTED) {
      setNet(NET_ERR);
      static uint32_t lastReconnect = 0;
      if (millis() - lastReconnect > 10000) {
        lastReconnect = millis();
        WiFi.reconnect();
      }
    } else {
      setNet((NetState)netDot);
    }
  }

  delay(5);
}
