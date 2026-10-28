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
#include "qrcode.h"
#include <AudioOutputI2S.h>
#include <ESP8266SAM.h>
#include "driver/i2s.h"

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

bool portalMode = false;
bool lastPollOk = true;
bool timeSynced = false;
String lastImapStatus = "Idle";
String since = "0";
volatile uint32_t pendingAmt = 0;
volatile bool pendingRefresh = false;
uint32_t lastUserActivity = 0;
uint32_t lastCycleTick = 0;

// Forward declarations
void drawScreen();
void announce(uint32_t rupees, const String& note = "");
void setScreen(int s);
void setBacklight(int percent);

// ---------- Audio Gain Scaler (Prevents 3.3V Sag & Display Flicker) ----------
float getAudioGain() {
  return constrain(((float)cfg.volume / 100.0f) * 0.40f, 0.02f, 0.45f);
}

// ---------- Isolated Audio Synthesizer (Leaves GPIO 25 Free For Touch!) ----------
void playVoice(const String& phrase) {
  // 1. Temporarily initialize AudioOutputI2S on internal DAC
  AudioOutputI2S* out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
  out->SetOutputModeMono(true);
  out->SetGain(getAudioGain());
  i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN); // Speaker is GPIO 26 only

  ESP8266SAM sam;
  sam.Say(out, phrase.c_str());
  out->stop();
  delete out;

  // 2. Tear down I2S driver so GPIO 25 is completely released back for Touch Clock!
  i2s_driver_uninstall(I2S_NUM_0);

  // 3. Re-assert Touch CLK and CS pins
  touch.begin();
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
  cfg.url          = prefs.getString("url", "");
  cfg.key          = prefs.getString("key", "");
  cfg.ssid         = prefs.getString("ssid", "");
  cfg.pass         = prefs.getString("pass", "");
  cfg.adminPin     = prefs.getString("pin", "");
  cfg.swapTouch    = prefs.getBool("tswap", false);
  cfg.invertTouchX = prefs.getBool("tinvx", false);
  cfg.invertTouchY = prefs.getBool("tinvy", false);
  todaySales       = prefs.getUInt("sales", 0);
  totalTransactions= prefs.getUInt("txcount", 0);
  salesDay         = prefs.getInt("sday", -1);
  lastPayHM        = prefs.getString("lastpay", "");
  if (prefs.getBytesLength("hourly") == sizeof(hourly)) prefs.getBytes("hourly", hourly, sizeof(hourly));
  prefs.end();

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
  prefs.putString("url", cfg.url);
  prefs.putString("key", cfg.key);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("pin", cfg.adminPin);
  prefs.putBool("tswap", cfg.swapTouch);
  prefs.putBool("tinvx", cfg.invertTouchX);
  prefs.putBool("tinvy", cfg.invertTouchY);
  prefs.end();
}

void saveSales() {
  prefs.begin("cydbiz", false);
  prefs.putUInt("sales", todaySales);
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
    totalTransactions = 0;
    lastPayHM = "";
    memset(hourly, 0, sizeof(hourly));
  }
  salesDay = t.tm_yday;
  saveSales();
  if (!portalMode && currentScreen == SCREEN_STATS) drawScreen();
}

// ---------- Helper Utilities ----------
String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;"; else o += c;
  }
  return o;
}

String urlEnc(const String& s) {
  String o; char b[4];
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else { snprintf(b, 4, "%%%02X", (uint8_t)c); o += b; }
  }
  return o;
}

String words(uint32_t n) {
  static const char* ones[] = {"zero","one","two","three","four","five","six","seven","eight","nine","ten",
    "eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
  static const char* tens[] = {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
  if (n < 20)       return ones[n];
  if (n < 100)      return String(tens[n/10]) + (n%10 ? String(" ") + ones[n%10] : String(""));
  if (n < 1000)     return String(ones[n/100]) + " hundred" + (n%100 ? " " + words(n%100) : String(""));
  if (n < 100000)   return words(n/1000) + " thousand" + (n%1000 ? " " + words(n%1000) : String(""));
  if (n < 10000000) return words(n/100000) + " lakh" + (n%100000 ? " " + words(n%100000) : String(""));
  return words(n/10000000) + " crore" + (n%10000000 ? " " + words(n%10000000) : String(""));
}

// Count a payment towards today's numbers
void recordPayment(uint32_t amount) {
  if (!amount) return;
  todaySales += amount;
  totalTransactions++;
  struct tm t;
  if (getLocalTime(&t, 0)) {
    hourly[t.tm_hour] += amount;
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

void announce(uint32_t rupees, const String& note) {
  ui_paymentAlert(rupees, note);

  String phrase;
  if (cfg.voiceMode == 0) {
    phrase = "rupees " + words(rupees) + " received";
  } else if (cfg.voiceMode == 1) {
    phrase = "payment of " + words(rupees) + " confirmed, thank you";
  } else {
    phrase = words(rupees) + " received";
  }
  playVoice(phrase);

  delay(1800);
  lastUserActivity = millis();
  drawScreen();
}

// ==========================================
// Gmail Polling (Native IMAP - Direct TLS)
// ==========================================
uint32_t extractAmount(const String& s) {
  String clean = "";
  bool foundDigit = false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isDigit(c)) {
      clean += c;
      foundDigit = true;
    } else if (foundDigit && (c == ',' || c == '.')) {
      if (c == '.') break;
    } else if (foundDigit) {
      break;
    }
  }
  return clean.toInt();
}

bool pollGmailIMAP() {
  if (cfg.gmailUser.isEmpty() || cfg.gmailPass.isEmpty()) {
    lastImapStatus = "Credentials missing";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(6000);

  lastImapStatus = "Connecting...";
  if (!client.connect("imap.gmail.com", 993)) {
    lastImapStatus = "Failed to connect to imap.gmail.com";
    return false;
  }

  client.readStringUntil('\n');

  client.print("A01 LOGIN \"" + cfg.gmailUser + "\" \"" + cfg.gmailPass + "\"\r\n");
  bool loginOk = false;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("A01 OK")) { loginOk = true; break; }
    if (line.startsWith("A01 NO") || line.startsWith("A01 BAD")) {
      lastImapStatus = "Login rejected - check 16-char App Password";
      break;
    }
  }
  if (!loginOk) {
    client.stop();
    return false;
  }

  client.print("A02 SELECT INBOX\r\n");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("A02 OK")) break;
  }

  String searchCmd = "A03 SEARCH " + (cfg.imapQuery.length() ? cfg.imapQuery : "UNSEEN");
  client.print(searchCmd + "\r\n");

  String searchRes = "";
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("* SEARCH")) {
      searchRes = line.substring(8);
      searchRes.trim();
    }
    if (line.startsWith("A03 OK")) break;
  }

  if (searchRes.length() == 0) {
    client.print("A04 LOGOUT\r\n");
    client.stop();
    lastImapStatus = "Connected - Inbox clean";
    return true;
  }

  int sp = searchRes.indexOf(' ');
  String msgId = (sp > 0) ? searchRes.substring(0, sp) : searchRes;

  client.print("A04 FETCH " + msgId + " (BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)])\r\n");
  String subject = "";
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Subject:") || line.startsWith("subject:")) {
      subject = line.substring(8);
      subject.trim();
    }
    if (line.startsWith("A04 OK")) break;
  }

  client.print("A05 STORE " + msgId + " +FLAGS (\\Seen)\r\n");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("A05 OK")) break;
  }

  client.print("A06 LOGOUT\r\n");
  client.stop();

  uint32_t amt = extractAmount(subject);
  if (amt > 0) {
    addOrder(subject.length() ? subject.substring(0, 22) : "Payment", amt, "PAID");
    announce(amt, subject.substring(0, 22));
    lastImapStatus = "Announced " + cfg.currency + " " + String(amt);
  } else if (subject.length()) {
    addOrder(subject.substring(0, 22), 0, "NEW");
    lastImapStatus = "Notification logged";
  }
  return true;
}

void poll() {
  bool ok = false;
  if (cfg.emailMode != 2) setNet(NET_SYNC);
  if (cfg.emailMode == 0) {
    ok = pollGmailIMAP();
  } else if (cfg.emailMode == 1) {
    if (!cfg.url.isEmpty()) {
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      String u = cfg.url + "?key=" + urlEnc(cfg.key) + "&since=" + since;
      if (cfg.imapQuery.length()) u += "&q=" + urlEnc(cfg.imapQuery);
      http.begin(client, u);
      int code = http.GET();
      if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString()) && !doc["error"].is<const char*>()) {
          ok = true;
          since = doc["latest"].as<String>();
          for (JsonObject p : doc["payments"].as<JsonArray>()) {
            uint32_t a = (uint32_t)p["amount"].as<double>();
            String note = p["from"].as<String>();
            addOrder(note.length() ? note : "eSewa Pay", a, "PAID");
            announce(a, note);
          }
        }
      }
      http.end();
    }
  } else {
    ok = true;
  }

  lastPollOk = ok;
  setNet(ok ? NET_OK : NET_ERR);
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

  // Web routes & REST API
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/test", HTTP_GET, handleTest);
  server.on("/api/state", HTTP_GET, handleApiState);
  server.on("/api/order", HTTP_POST, handleApiOrder);
  server.on("/api/payment", HTTP_POST, handleApiPayment);
  server.on("/api/reset", HTTP_GET, handleApiReset);
  server.on("/api/clear", HTTP_GET, handleApiClear);

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
    poll();
  }

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
    addOrder("Voice test", a, "PAID");
    announce(a, "Test");
  }

  if (pendingRefresh) {
    pendingRefresh = false;
    since = "0";
    lastPollOk = true;
    drawScreen();
    poll();
  }

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

    // Polling scheduler
    static uint32_t lastPoll = 0;
    if (WiFi.status() != WL_CONNECTED) {
      setNet(NET_ERR);
      static uint32_t lastReconnect = 0;
      if (millis() - lastReconnect > 10000) {
        lastReconnect = millis();
        WiFi.reconnect();
      }
    } else if (millis() - lastPoll > 15000) {
      lastPoll = millis();
      poll();
    }
  }

  delay(5);
}
