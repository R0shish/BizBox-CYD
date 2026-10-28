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
#include <mbedtls/md.h>   // HMAC-SHA256 for Connect webhooks
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #include <esp_random.h>
#endif
#include "textutil.h"

#include "hw.h"

// ==========================================
// Globals
// ==========================================
TFT_eSPI tft;
CYDTouch touch;
WebServer server(80);
DNSServer dns;
Preferences prefs;

// Modules, in dependency order (one translation unit, see CLAUDE.md)
#include "model.h"     // shared state: config, orders, today's numbers
#include "ui.h"        // everything drawn on the screen
#include "speaker.h"   // SAM voice -> DAC on GPIO26
#include "imap.h"      // Gmail reader
#include "net.h"       // network task: events, payment polling
#include "storage.h"   // NVS
#include "connect.h"   // Connect: webhooks out, polling in
#include "nettask.h"   // the network task's loop
#include "app.h"       // orders, sales, announcements, screens
#include "input.h"     // touch + BOOT button
#include "wifi.h"      // joining Wi-Fi, setup hotspot
#include "presets.h"   // Connect presets (vendor presets go here)
#include "web.h"       // phone dashboard + REST API

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
  loadOrders();

  // Initialize Backlight PWM
  initBacklight(cfg.brightness);

  // Initialize Touch Driver (GPIO 25 is clean and unencumbered!)
  touch.begin();

  webBegin();

  if (cfg.ssid.isEmpty() || !connectWifi()) {
    startPortal();
  } else {
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
    }
    netState = NET_OK;
    drawScreen();
  }
  netBegin(imapState, appsScriptSince, connectCursor);

  server.begin();
  lastUserActivity = millis();
  lastCycleTick = millis();
  Serial.println("[BizBox ready]");
}

void loop() {
  server.handleClient();
  if (portalMode) {
    dns.processNextRequest();
    portalRetryWifi();
  }
  handleButton();
  handleTouch();

  if (pendingRefresh) {
    pendingRefresh = false;
    drawScreen();
    netPollNow = true;
  }

  handleNetEvents();
  handleAnnouncements();
  if (ordersDirty && millis() - ordersDirtyAt > 2000) saveOrdersNow();

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
          redrawContent();
        } else if (currentScreen == SCREEN_PROMO) {
          currentScreen = SCREEN_QR;
          redrawContent();
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
