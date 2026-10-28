#pragma once
// ==========================================
// Shared state: config, orders, today's numbers
// ==========================================

enum Screen {
  SCREEN_QR = 0,
  SCREEN_ORDERS = 1,
  SCREEN_PROMO = 2,
  SCREEN_STATS = 3,
  SCREEN_SETTINGS = 4,
  SCREEN_COUNT = 5
};
Screen currentScreen = SCREEN_QR;

struct OrderItem {
  int id;
  String title;
  uint32_t amount; // paisa
  String status; // "NEW", "PREP", "READY", "PAID"
};

const int MAX_ORDERS = 10;
OrderItem orders[MAX_ORDERS];
int orderCount = 0;
int nextOrderId = 101;

uint32_t todaySales = 0;        // paid amounts only, whole rupees
uint8_t salesPaisa = 0;         // paisa carried towards the next rupee
uint32_t totalTransactions = 0; // paid count only
uint32_t hourly[24] = {0};      // today's paid amounts per hour
String lastPayHM = "";          // "HH:MM" of the last payment
int salesDay = -1;              // day-of-year the totals belong to

// Network/listener state shown by the status dot
enum NetState { NET_OFF = 0, NET_OK, NET_SYNC, NET_ERR };
NetState netState = NET_OFF;

struct Cfg {
  String bizName;       // Store Name
  String currency;      // Currency Symbol
  String storeNote;     // Promo note / special
  bool storeOpen;       // Open/Closed status
  String qr;            // Raw Payment QR Text
  String qrSub;         // Footer instructions
  int volume;           // 5 to 100, default 25
  int brightness;       // 10 to 100, default 80
  int voiceMode;        // 0="Rupees X received", 1="Payment of X confirmed", 2="X received"
  bool autoCycle;       // Kiosk carousel
  int cycleSecs;        // Carousel delay (secs)
  int emailMode;        // 0=Native Gmail IMAP, 1=Apps Script, 2=Local API
  String gmailUser;     // Gmail address
  String gmailPass;     // 16-char App Password
  String imapQuery;     // Search filter
  bool verifyGoogle;    // Check Google's TLS certificate (IMAP + Apps Script)
  String url;           // Apps Script URL (fallback)
  String key;           // Apps Script Secret
  String ssid;
  String pass;
  String adminPin;      // PIN protection
  bool swapTouch;
  bool invertTouchX;
  bool invertTouchY;
} cfg;

#define AP_NAME   "BizBox-Setup"
#define MDNS_NAME "bizbox"
#define FW_VERSION "3.0"
