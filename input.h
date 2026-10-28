#pragma once
// ==========================================================================
//  Touch (taps + swipes) and the BOOT button (hold 3.5 s = factory reset)
// ==========================================================================

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
      saveCfg(); ui_settingValue(0, true);
    } else if (hit(x, y, SET_PLUS_X, SET_VOL_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.volume = min(100, cfg.volume + 5);
      saveCfg(); ui_settingValue(0, true);
    } else if (hit(x, y, SET_MINUS_X, SET_BRI_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.brightness = max(10, cfg.brightness - 10);
      setBacklight(cfg.brightness); saveCfg(); ui_settingValue(1, true);
    } else if (hit(x, y, SET_PLUS_X, SET_BRI_Y, SET_BTN_W, SET_BTN_H)) {
      cfg.brightness = min(100, cfg.brightness + 10);
      setBacklight(cfg.brightness); saveCfg(); ui_settingValue(1, true);
    } else if (hit(x, y, SET_TEST_X, SET_TEST_Y, SET_TEST_W, SET_TEST_H)) {
      testVoice();
    }
  } else if (currentScreen == SCREEN_STATS) {
    if (x >= 200 && y >= 36 && y <= 76) { // store switch
      cfg.storeOpen = !cfg.storeOpen;
      saveCfg(); ui_statsSwitch(true);
    }
  } else if (currentScreen == SCREEN_ORDERS) {
    for (int i = 0; i < orderCount && i < ORD_ROWS; i++) {
      int oy = ORD_LIST_Y + i * ORD_ROW_H;
      if (y >= oy && y < oy + ORD_ROW_H) {
        if (orders[i].status == "NEW") orders[i].status = "PREP";
        else if (orders[i].status == "PREP") orders[i].status = "READY";
        else if (orders[i].status == "READY") orders[i].status = "PAID";
        else orders[i].status = "NEW";
        ordersChanged();
        ui_orderRow(i, true);
        ui_ordersChip(true);
        break;
      }
    }
  } else if (currentScreen == SCREEN_PROMO) {
    cfg.storeOpen = !cfg.storeOpen;
    saveCfg();
    redrawContent();
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
