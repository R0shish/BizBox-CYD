#pragma once
// ==========================================================================
//  App logic on the loop task: today's numbers, orders, screens, the voice
//  and the announcement queue, and events coming from the network task.
// ==========================================================================

// ==========================================
// Screen plumbing (drawing lives in ui.h)
// ==========================================
// Full repaint: after the payment takeover or when the status bar changed
void drawScreen() {
  ui_drawScreen();
}

// Content area + nav only (status bar stays): screen switches and data changes
void redrawContent() {
  if (portalMode) return;
  ui_redrawContent();
}

void setScreen(int s) {
  if (s < 0) s = SCREEN_COUNT - 1;
  if (s >= SCREEN_COUNT) s = 0;
  currentScreen = (Screen)s;
  lastUserActivity = millis();
  redrawContent();
}

void setNet(NetState s) {
  if (netState == s) return;
  netState = s;
  if (!portalMode) ui_drawDot();
}

// Zero today's numbers (midnight, or "Reset today" on the dashboard)
void resetToday() {
  todaySales = 0;
  salesPaisa = 0;
  totalTransactions = 0;
  lastPayHM = "";
  memset(hourly, 0, sizeof(hourly));
}

// Reset today's numbers when the date changes (needs the clock)
void checkDayRollover() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return;
  if (salesDay == t.tm_yday) return;
  if (salesDay != -1) resetToday();
  salesDay = t.tm_yday;
  saveSales();
  if (currentScreen == SCREEN_STATS) redrawContent();
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
  if (orderCount >= MAX_ORDERS) {
    // Full: drop the oldest PAID order; only if none is paid, the oldest one
    int drop = 0;
    for (int i = 0; i < orderCount; i++) if (orders[i].status == "PAID") { drop = i; break; }
    for (int i = drop; i < orderCount - 1; i++) orders[i] = orders[i + 1];
    orderCount--;
  }
  OrderItem& o = orders[orderCount++];
  o.id = nextOrderId++;
  o.title = title;
  o.amount = amount;
  o.status = status;
  o.ext = "";
  ordersChanged();
  if (status == "PAID") recordPayment(amount);
  if (currentScreen == SCREEN_ORDERS || currentScreen == SCREEN_STATS) redrawContent();
}

// ==========================================
// Voice + announcements
// ==========================================
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

void testVoice() {
  playVoice("volume " + String(cfg.volume) + " percent ready");
}

// Shows the takeover and speaks the amount. Test announcements look and sound
// different ("TEST" on screen, "test" spoken) and are never booked as sales.
void announce(uint32_t paisa, const String& note, bool test) {
  ui_paymentAlert(paisa, note, test);

  String phrase = test ? "test, " : "";
  String said = sayAmount(paisa);
  if (cfg.voiceMode == 0) {
    phrase += (paisa >= 100 ? "rupees " : "") + said + " received";
  } else if (cfg.voiceMode == 1) {
    phrase += "payment of " + said + " confirmed, thank you";
  } else {
    phrase += said + " received";
  }
  playVoice(phrase);

  delay(1800);
  lastUserActivity = millis();
  drawScreen();
}

// Announcements wait here so whoever triggered them (an HTTP request, the
// network task) never waits for the animation and speech.
struct Announcement { uint32_t paisa; String note; bool test; };
const int ANN_MAX = 6;
Announcement annQ[ANN_MAX];
int annHead = 0, annCount = 0;

bool queueAnnouncement(uint32_t paisa, const String& note, bool test = false) {
  if (annCount >= ANN_MAX) return false;
  Announcement& a = annQ[(annHead + annCount++) % ANN_MAX];
  a.paisa = paisa;
  a.note = note;
  a.test = test;
  return true;
}

void handleAnnouncements() {
  if (!annCount) return;
  Announcement a = annQ[annHead];
  annHead = (annHead + 1) % ANN_MAX;
  annCount--;
  announce(a.paisa, a.note, a.test);
}

// ==========================================
// Events from the network task (see net.h)
// ==========================================
void handleNetEvents() {
  NetEvent e;
  while (netQueue && xQueueReceive(netQueue, &e, 0) == pdTRUE) {
    if (e.type == EV_PAYMENT) {
      addOrder(e.title[0] ? String(e.title) : String("Payment"), e.paisa, "PAID");
      if (!e.stale) queueAnnouncement(e.paisa, e.note);
    }
  }
}
