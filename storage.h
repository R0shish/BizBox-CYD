#pragma once
// ==========================================================================
//  Everything kept in NVS (Preferences namespace "cydbiz"): settings, today's
//  numbers, the orders list and the payment readers' positions.
//  Key names are part of the upgrade path: existing devices keep their
//  settings, so never rename one.
// ==========================================================================

ImapState imapState = {0, 0};   // where the IMAP reader is in the mailbox (NVS guidv/guid)
String appsScriptSince = "0";   // Apps Script cursor (NVS since)

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
  cfg.apiToken     = prefs.getString("apitok", "");
  prefs.end();

  if (cfg.apiToken.length() != 32) {
    cfg.apiToken = newApiToken();
    prefs.begin("cydbiz", false);
    prefs.putString("apitok", cfg.apiToken);
    prefs.end();
  }

  if (cfg.volume < 5 || cfg.volume > 100) cfg.volume = 25;
  if (cfg.brightness < 10 || cfg.brightness > 100) cfg.brightness = 80;
  if (cfg.cycleSecs < 5 || cfg.cycleSecs > 120) cfg.cycleSecs = 15;
  cfg.voiceMode = constrain(cfg.voiceMode, 0, 2);
  cfg.emailMode = constrain(cfg.emailMode, 0, 2);
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

// ---------- Orders in NVS ----------
// One versioned blob ("orders"): [ver][count][nextId:4] then per order
// [id:4][paisa:4][status:1][titleLen:1][title][extLen:1][ext]. Saves are
// debounced so a burst of taps is one flash write.
const uint8_t ORDERS_BLOB_VER = 1;
bool ordersDirty = false;
uint32_t ordersDirtyAt = 0;

int statusIndex(const String& s) {
  for (int i = 0; i < 4; i++) if (s == ORDER_STATUS[i]) return i;
  return 0;
}

void saveOrdersNow() {
  uint8_t buf[6 + MAX_ORDERS * (10 + 2 * 255)];
  size_t n = 0;
  buf[n++] = ORDERS_BLOB_VER;
  buf[n++] = (uint8_t)orderCount;
  memcpy(buf + n, &nextOrderId, 4); n += 4;
  for (int i = 0; i < orderCount; i++) {
    const OrderItem& o = orders[i];
    memcpy(buf + n, &o.id, 4); n += 4;
    memcpy(buf + n, &o.amount, 4); n += 4;
    buf[n++] = (uint8_t)statusIndex(o.status);
    String t = utf8Cut(o.title, 64), e = utf8Cut(o.ext, 64);
    buf[n++] = (uint8_t)t.length(); memcpy(buf + n, t.c_str(), t.length()); n += t.length();
    buf[n++] = (uint8_t)e.length(); memcpy(buf + n, e.c_str(), e.length()); n += e.length();
  }
  prefs.begin("cydbiz", false);
  prefs.putBytes("orders", buf, n);
  prefs.end();
  ordersDirty = false;
}

void loadOrders() {
  static uint8_t buf[6 + MAX_ORDERS * (10 + 2 * 255)];
  prefs.begin("cydbiz", true);
  size_t n = prefs.getBytesLength("orders");
  if (n >= 6 && n <= sizeof buf) n = prefs.getBytes("orders", buf, n); else n = 0;
  prefs.end();
  if (n < 6 || buf[0] != ORDERS_BLOB_VER) return;
  int count = min((int)buf[1], MAX_ORDERS);
  size_t p = 2;
  int32_t nid; memcpy(&nid, buf + p, 4); p += 4;
  orderCount = 0;
  for (int i = 0; i < count; i++) {
    if (p + 10 > n) break;
    OrderItem o;
    memcpy(&o.id, buf + p, 4); p += 4;
    memcpy(&o.amount, buf + p, 4); p += 4;
    o.status = ORDER_STATUS[buf[p++] & 3];
    uint8_t tl = buf[p++];
    if (p + tl + 1 > n) break;
    o.title = String((const char*)buf + p, tl); p += tl;
    uint8_t el = buf[p++];
    if (p + el > n) break;
    o.ext = String((const char*)buf + p, el); p += el;
    orders[orderCount++] = o;
  }
  if (nid > nextOrderId) nextOrderId = nid;
}

void ordersChanged() {
  ordersDirty = true;
  ordersDirtyAt = millis();
}
