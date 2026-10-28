#pragma once
// ==========================================================================
//  Wi-Fi: joining the shop network at boot, the setup hotspot (captive
//  portal) and getting back out of it when the saved network returns.
// ==========================================================================

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
  bool timeSynced = getLocalTime(&tmv, 0);
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

// Setup mode with a saved network (router still booting after a power cut,
// or briefly down): keep trying it, and go back to normal once it answers.
// Retries pause while a phone is on the hotspot, because joining scans
// channels and would drop that phone.
void portalRetryWifi() {
  if (!portalMode || cfg.ssid.isEmpty()) return;
  bool phoneOnHotspot = WiFi.softAPgetStationNum() > 0;
  if (WiFi.status() == WL_CONNECTED && !phoneOnHotspot) {
    if (ordersDirty) saveOrdersNow();
    ESP.restart();                     // boot normally on the saved network
  }
  static uint32_t lastTry = 0;
  if (!phoneOnHotspot && millis() - lastTry > 30000) {
    lastTry = millis();
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }
}

// Captive Portal Auto-Detect Handlers for iOS / Android / Windows.
// Only in setup mode: on the shop LAN 192.168.4.1 doesn't exist.
void handleCaptiveRedirect() {
  if (!portalMode) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "Redirecting to setup");
}
