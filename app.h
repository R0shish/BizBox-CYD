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

// Content area + nav only (status bar stays): screen switches and data changes.
// While a batch of network events is applied, redraws are folded into one.
bool redrawBatch = false, redrawPending = false;
void redrawContent() {
  if (portalMode) return;
  if (redrawBatch) { redrawPending = true; return; }
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

// Adds an order to the screen list. PAID orders count towards today's
// numbers unless book is false (orders whose payment is booked elsewhere).
OrderItem* addOrder(const String& title, uint32_t amount, const String& status = "NEW", bool book = true) {
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
  if (book && status == "PAID") recordPayment(amount);
  if (currentScreen == SCREEN_ORDERS || currentScreen == SCREEN_STATS) redrawContent();
  return &o;
}

// What a webhook says about an order
ConnectData orderEvent(const OrderItem& o, const char* source) {
  ConnectData d;
  d.source = source;
  d.paisa = o.amount;
  d.orderId = o.id;
  d.ext = o.ext;
  d.title = o.title;
  d.status = o.status;
  return d;
}

// A payment from any source: on the list, in today's numbers, out as a webhook
OrderItem* bookPayment(const String& title, uint32_t paisa, const String& note, const char* source, bool webhook = true) {
  OrderItem* o = addOrder(title, paisa, "PAID");
  if (webhook) {
    ConnectData d = orderEvent(*o, source);
    d.note = note;
    connectEmit("payment.received", d);
  }
  return o;
}

// Orders from the Connect pull, matched by their external ID. Updates from
// the other system don't touch today's totals and aren't echoed back.
void applyExternalOrder(const NetEvent& e) {
  String ext = e.ext;
  int idx = -1;
  for (int i = 0; i < orderCount; i++) if (orders[i].ext == ext) { idx = i; break; }
  bool remove = (e.has & HAS_STATUS) && e.status == 4;
  if (idx < 0) {
    if (remove) return;
    String status = (e.has & HAS_STATUS) ? ORDER_STATUS[e.status] : "NEW";
    OrderItem* o = addOrder((e.has & HAS_TITLE) && e.text[0] ? String(e.text) : "Order " + ext,
                            (e.has & HAS_AMOUNT) ? e.paisa : 0, status, false);
    o->ext = ext;
    ordersChanged();
    return;
  }
  if (remove) {
    for (int i = idx; i < orderCount - 1; i++) orders[i] = orders[i + 1];
    orderCount--;
  } else {
    OrderItem& o = orders[idx];
    if (e.has & HAS_TITLE) o.title = e.text;
    if (e.has & HAS_AMOUNT) o.amount = e.paisa;
    if (e.has & HAS_STATUS) o.status = ORDER_STATUS[e.status];
  }
  ordersChanged();
  if (currentScreen == SCREEN_ORDERS) redrawContent();
}

// ==========================================
// Voice + announcements
// ==========================================
// ---------- Voice: SAM speech straight to the DAC on GPIO26 (see speaker.h) ----------
void playVoice(const String& phrase) {
  CYDSpeaker out;
  out.SetGain(voiceGainForVolume(cfg.volume));
  ESP8266SAM sam;
  // SAM's settings are global and stick between calls, so set all of them.
  // A little slower than SAM's default (72) is easier to follow at a counter.
  sam.SetSpeed(84);
  sam.SetPitch(64);
  sam.SetMouth(128);
  sam.SetThroat(128);
  sam.Say(&out, phrase.c_str());
  out.stop();
}

// The speaker amp, the backlight and the touch controller share one 3.3 V
// regulator with the panel (see speaker.h). If a loud moment ever dips it
// far enough to reset the ILI9341, the panel shows garbage and ignores the
// redraw. Re-sending what it needs to show pixels (sleep out, 16-bit
// colour, rotation, display on) recovers it, and changes nothing otherwise.
void displayReassert() {
  tft.writecommand(0x11);      // SLPOUT
  delay(5);
  tft.writecommand(0x3A);      // COLMOD
  tft.writedata(0x55);         // 16 bits per pixel
  tft.setRotation(1);          // MADCTL
  tft.writecommand(0x29);      // DISPON
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
  displayReassert();
  lastUserActivity = millis();
  drawScreen();
}

// Announcements wait here so whoever triggered them (an HTTP request, the
// network task) never waits for the animation and speech. An entry with
// `say` set just speaks that text (Connect "say" command).
struct Announcement { uint32_t paisa; String note; bool test; String say; };
const int ANN_MAX = 6;
Announcement annQ[ANN_MAX];
int annHead = 0, annCount = 0;

bool queueAnnouncement(uint32_t paisa, const String& note, bool test = false, const String& say = "") {
  if (annCount >= ANN_MAX) return false;
  Announcement& a = annQ[(annHead + annCount++) % ANN_MAX];
  a.paisa = paisa;
  a.note = note;
  a.test = test;
  a.say = say;
  return true;
}

void handleAnnouncements() {
  if (!annCount) return;
  Announcement a = annQ[annHead];
  annHead = (annHead + 1) % ANN_MAX;
  annCount--;
  if (a.say.length()) playVoice(a.say);
  else announce(a.paisa, a.note, a.test);
}

// ==========================================
// Events from the network task (see net.h)
// ==========================================
void handleNetEvents() {
  NetEvent e;
  redrawBatch = true;
  while (netQueue && xQueueReceive(netQueue, &e, 0) == pdTRUE) {
    switch (e.type) {
      case EV_PAYMENT:
        bookPayment(e.text[0] ? String(e.text) : String("Payment"), e.paisa, e.note,
                    e.source == SRC_GMAIL ? "gmail" : "apps_script");
        if (!e.stale) queueAnnouncement(e.paisa, e.note);
        break;
      case EV_ANNOUNCE:     // the other system's own payment: no webhook echo
        bookPayment(e.note[0] ? String(e.note) : String("Payment"), e.paisa, e.note, "connect", false);
        queueAnnouncement(e.paisa, e.note);
        break;
      case EV_ORDER:  applyExternalOrder(e); break;
      case EV_SAY:    queueAnnouncement(0, "", false, e.text); break;
      case EV_SCREEN: if (!portalMode) setScreen(e.status); break;
      default: break;
    }
  }
  redrawBatch = false;
  if (redrawPending) { redrawPending = false; redrawContent(); }
}
