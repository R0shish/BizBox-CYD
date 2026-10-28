#pragma once
// ==========================================================================
//  Pure text helpers: no hardware, no globals. Unit-tested on the desktop
//  by tools/check/logic_test.cpp, so keep them free of TFT/WiFi calls.
//   - words() / sayAmount(): numbers read out the Nepali/Indian way
//   - parseAmount(): pick the paid amount out of a notification subject
//   - mimeDecodeHeader(): RFC 2047 =?UTF-8?B?...?= / =?...?Q?...?= words
//   - utf8Cut(): shorten without splitting a multi-byte character
//   - esc() / jsonEsc() / urlEnc(): escaping for HTML, JSON and URLs
// ==========================================================================

// ---------- Numbers to words ----------
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

// "five hundred", "five hundred and fifty paisa", "fifty paisa"
String sayAmount(uint32_t paisa) {
  uint32_t r = paisa / 100, p = paisa % 100;
  if (!p) return words(r);
  if (!r) return words(p) + " paisa";
  return words(r) + " and " + words(p) + " paisa";
}

// ---------- UTF-8 ----------
// At most maxBytes bytes, never ending inside a multi-byte character
String utf8Cut(const String& s, unsigned maxBytes) {
  if (s.length() <= maxBytes) return s;
  unsigned n = maxBytes;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;   // s[n] starts the dropped part
  return s.substring(0, n);
}

// Drop the last character (not just the last byte)
void utf8PopBack(String& s) {
  int n = s.length();
  if (!n) return;
  n--;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;
  s.remove(n);
}

// ---------- Escaping ----------
String esc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;"; else o += c;
  }
  return o;
}

// Contents of a JSON string literal (no surrounding quotes)
String jsonEsc(const String& s) {
  String o;
  o.reserve(s.length() + 4);
  for (unsigned i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o;
}

String urlEnc(const String& s) {
  String o; char b[4];
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((uint8_t)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else { snprintf(b, sizeof b, "%%%02X", (uint8_t)c); o += b; }
  }
  return o;
}

// ---------- RFC 2047 header decoding ----------
static int tu_b64(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static int tu_hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// Append one byte from a single-byte charset (Latin-1 / Windows-1252) as UTF-8
static void tu_putLatin1(String& o, uint8_t b) {
  if (b < 0x80) { o += (char)b; return; }
  o += (char)(0xC0 | (b >> 6));
  o += (char)(0x80 | (b & 0x3F));
}

// Decode one encoded word's payload into o. Returns false if malformed.
static bool tu_decodeWord(const String& cs, char enc, const char* p, int n, String& o) {
  String lc = cs; lc.toLowerCase();
  bool latin1 = lc.startsWith("iso-8859-1") || lc.startsWith("latin1") || lc == "windows-1252" || lc == "cp1252";
  String raw;
  if (enc == 'B' || enc == 'b') {
    uint32_t acc = 0; int bits = 0;
    for (int i = 0; i < n; i++) {
      if (p[i] == '=') break;
      int v = tu_b64(p[i]);
      if (v < 0) return false;
      acc = (acc << 6) | (uint32_t)v; bits += 6;
      if (bits >= 8) { bits -= 8; raw += (char)((acc >> bits) & 0xFF); }
    }
  } else if (enc == 'Q' || enc == 'q') {
    for (int i = 0; i < n; i++) {
      if (p[i] == '_') raw += ' ';
      else if (p[i] == '=') {
        if (i + 2 >= n) return false;
        int h = tu_hex(p[i + 1]), l = tu_hex(p[i + 2]);
        if (h < 0 || l < 0) return false;
        raw += (char)((h << 4) | l); i += 2;
      } else raw += p[i];
    }
  } else return false;
  if (latin1) { for (unsigned i = 0; i < raw.length(); i++) tu_putLatin1(o, (uint8_t)raw[i]); }
  else o += raw;
  return true;
}

String mimeDecodeHeader(const String& in) {
  String o;
  const char* s = in.c_str();
  int n = in.length();
  int i = 0;
  bool lastWasWord = false;
  while (i < n) {
    if (s[i] == '=' && i + 1 < n && s[i + 1] == '?') {
      // =?charset?E?text?=
      int q1 = in.indexOf('?', i + 2);
      int q2 = q1 > 0 ? in.indexOf('?', q1 + 1) : -1;
      int end = q2 > 0 ? in.indexOf("?=", q2 + 1) : -1;
      if (q1 > 0 && q2 == q1 + 2 && end > 0) {
        String cs = in.substring(i + 2, q1);
        int star = cs.indexOf('*');                 // RFC 2231 language suffix
        if (star >= 0) cs = cs.substring(0, star);
        String dec;
        if (tu_decodeWord(cs, s[q1 + 1], s + q2 + 1, end - q2 - 1, dec)) {
          o += dec;
          i = end + 2;
          lastWasWord = true;
          continue;
        }
      }
    }
    if (lastWasWord && (s[i] == ' ' || s[i] == '\t')) {
      int j = i;
      while (j < n && (s[j] == ' ' || s[j] == '\t')) j++;
      if (j + 1 < n && s[j] == '=' && s[j + 1] == '?') { i = j; continue; }  // space between words is dropped
    }
    lastWasWord = false;
    char c = s[i++];
    o += (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
  }
  return o;
}

// ---------- Amount parsing ----------
// Lower-cased ASCII copy of a subject: Devanagari digits become 0-9,
// "रु"/"रू"/"₹" become " rs ", other non-ASCII bytes become spaces.
static String tu_normalise(const String& s) {
  String t;
  t.reserve(s.length() + 8);
  const uint8_t* p = (const uint8_t*)s.c_str();
  int n = s.length();
  for (int i = 0; i < n;) {
    uint8_t c = p[i];
    if (c < 0x80) { t += (char)tolower(c); i++; continue; }
    // ० - ९  U+0966..U+096F = E0 A5 A6..AF
    if (c == 0xE0 && i + 2 < n && p[i + 1] == 0xA5 && p[i + 2] >= 0xA6 && p[i + 2] <= 0xAF) {
      t += (char)('0' + (p[i + 2] - 0xA6)); i += 3; continue;
    }
    // र (E0 A4 B0) + ु (E0 A5 81) or ू (E0 A5 82)
    if (c == 0xE0 && i + 5 < n && p[i + 1] == 0xA4 && p[i + 2] == 0xB0 && p[i + 3] == 0xE0 &&
        p[i + 4] == 0xA5 && (p[i + 5] == 0x81 || p[i + 5] == 0x82)) {
      t += " rs "; i += 6; continue;
    }
    // ₹ U+20B9 = E2 82 B9
    if (c == 0xE2 && i + 2 < n && p[i + 1] == 0x82 && p[i + 2] == 0xB9) { t += " rs "; i += 3; continue; }
    // any other multi-byte character: one space
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    t += ' ';
    i += len;
  }
  return t;
}

static bool tu_inList(const String& w, const char* const* list) {
  for (int i = 0; list[i]; i++) if (w == list[i]) return true;
  return false;
}

// The k-th word before position `pos` (k=0 is the nearest). Words are runs of
// letters (and '/', so "a/c" is one word). Digits stop the search.
static String tu_wordBefore(const String& t, int pos, int k) {
  int i = pos - 1;
  for (int w = 0; w <= k; w++) {
    while (i >= 0 && !isalpha((uint8_t)t[i]) && !isdigit((uint8_t)t[i])) i--;
    if (i < 0 || isdigit((uint8_t)t[i])) return "";
    int e = i;
    while (i >= 0 && (isalpha((uint8_t)t[i]) || t[i] == '/')) i--;
    if (w == k) return t.substring(i + 1, e + 1);
  }
  return "";
}

static String tu_wordAfter(const String& t, int pos, int k) {
  int n = t.length(), i = pos;
  for (int w = 0; w <= k; w++) {
    while (i < n && !isalpha((uint8_t)t[i]) && !isdigit((uint8_t)t[i])) i++;
    if (i >= n || isdigit((uint8_t)t[i])) return "";
    int b = i;
    while (i < n && (isalpha((uint8_t)t[i]) || t[i] == '/')) i++;
    if (w == k) return t.substring(b, i);
  }
  return "";
}

// Finds the paid amount in a notification subject, in paisa.
// Numbers only count when they sit next to a currency ("Rs 500", "NPR 1,500.50",
// "500/-", "रु ५००") or a payment word ("received 500", "500 credited").
// Dates, times, phone numbers, IDs, percentages and balances are skipped.
// `currency` is the shop's own symbol from settings (e.g. "Rs") and counts too.
bool parseAmount(const String& subject, uint32_t& paisa, const String& currency = "") {
  static const char* const CUR[] = {"rs", "npr", "nrs", "inr", "rupees", "rupee", "re", nullptr};
  static const char* const PAY[] = {"amount", "amt", "received", "receive", "credited", "credit", "paid",
                                    "payment", "deposited", "deposit", "transferred", "transfer", "sent",
                                    "total", "worth", "collected", nullptr};
  static const char* const IDS[] = {"txn", "id", "ref", "reference", "transaction", "trx", "trace", "order",
                                    "invoice", "inv", "no", "number", "otp", "code", "pin", "mobile", "phone",
                                    "mob", "a/c", "ac", "acc", "account", "acct", "card", "ending", "ticket",
                                    "booking", "voucher", "batch", "terminal", "tid", "mid", "rrn", nullptr};
  static const char* const BAL[] = {"balance", "bal", "avl", "available", "limit", "fee", "fees", "charge",
                                    "charges", "cashback", "commission", "discount", "off", "save", nullptr};
  String cur = currency; cur.trim(); cur.toLowerCase();
  if (cur.endsWith(".")) cur.remove(cur.length() - 1);
  String t = tu_normalise(subject);
  int n = t.length();
  int best = 0; uint32_t bestVal = 0;
  for (int i = 0; i < n; i++) {
    if (!isdigit((uint8_t)t[i]) || (i > 0 && isdigit((uint8_t)t[i - 1]))) continue;
    int start = i;
    // integer part: digits with optional comma groups
    uint64_t rup = 0; int digits = 0; int j = i; bool badGroup = false; bool commas = false;
    while (j < n) {
      if (isdigit((uint8_t)t[j])) { rup = rup * 10 + (t[j] - '0'); digits++; j++; }
      else if (t[j] == ',' && j + 1 < n && isdigit((uint8_t)t[j + 1]) && digits > 0) {
        int g = 0; while (j + 1 + g < n && isdigit((uint8_t)t[j + 1 + g])) g++;
        if (g != 2 && g != 3) { badGroup = true; break; }
        commas = true; j++;
      } else break;
      if (digits > 12) break;
    }
    int end = j;
    uint32_t dec = 0;
    if (j + 1 < n && t[j] == '.' && isdigit((uint8_t)t[j + 1])) {
      int k = j + 1, dd = 0;
      while (k < n && isdigit((uint8_t)t[k])) { if (dd < 2) dec = dec * 10 + (t[k] - '0'); dd++; k++; }
      if (dd > 2) { i = k; continue; }          // 1.500 / 12.05.2024: not money
      if (dd == 1) dec *= 10;
      end = k;
    }
    char p = start > 0 ? t[start - 1] : ' ';
    char p2 = start > 1 ? t[start - 2] : ' ';
    char q = end < n ? t[end] : ' ';
    char q2 = end + 1 < n ? t[end + 1] : ' ';
    i = end - 1;                                 // continue after this number
    if (badGroup || digits > 12) continue;

    // Shape rejections
    if (p == '#' || p == '+' || q == '%') continue;
    if (p == '/' || (q == '/' && q2 != '-')) continue;                    // 12/05, a/c 12
    if ((p == '-' && isdigit((uint8_t)p2)) || (q == '-' && isdigit((uint8_t)q2))) continue; // 2024-05-12
    if ((p == ':' && isdigit((uint8_t)p2)) || (q == ':' && isdigit((uint8_t)q2))) continue; // 13:42
    if (q == '.' && isdigit((uint8_t)q2)) continue;                      // 12.05.2024 tail
    if (!commas && digits >= 9) continue;                                // phones, account numbers

    // letters glued to the number: only a currency may be ("rs500", "500rs")
    bool gluedBefore = start > 0 && (isalpha((uint8_t)t[start - 1]));
    bool gluedAfter = end < n && isalpha((uint8_t)t[end]);
    String before = tu_wordBefore(t, start, 0);
    String after = tu_wordAfter(t, end, 0);
    bool curBefore = tu_inList(before, CUR) || (cur.length() && before == cur);
    bool curAfter = tu_inList(after, CUR) || (cur.length() && after == cur) || (q == '/' && q2 == '-');
    if (gluedBefore && !curBefore) continue;
    if (gluedAfter && !curAfter) continue;
    if (tu_inList(before, IDS)) continue;

    int score = 1;
    if (curBefore || curAfter) score = 3;
    else {
      for (int k = 0; k < 4 && score < 2; k++) if (tu_inList(tu_wordBefore(t, start, k), PAY)) score = 2;
      for (int k = 0; k < 2 && score < 2; k++) if (tu_inList(tu_wordAfter(t, end, k), PAY)) score = 2;
    }
    for (int k = 0; k < 3; k++) {
      String w = tu_wordBefore(t, start, k);
      if (tu_inList(w, BAL)) { score -= 2; break; }
      if (tu_inList(w, PAY)) break;          // "balance ... received 500": the payment word is closer
    }
    if (tu_inList(after, BAL)) score -= 2;
    if (rup == 0 && dec == 0) continue;
    if (rup >= 10000000ULL) continue;          // 1 crore+: an ID, not a shop payment
    if (score > best) { best = score; bestVal = (uint32_t)rup * 100 + dec; }
  }
  if (best < 2) return false;
  paisa = bestVal;
  return true;
}

// Money typed by a person or sent by an API: "500", "1,500.50", "499.5".
// Returns false for negatives, junk, more than 2 decimals or 1 crore+.
bool parseMoney(const String& s, uint32_t& paisa) {
  String t = s; t.trim();
  if (!t.length()) return false;
  uint64_t r = 0; uint32_t d = 0; int dd = 0; bool dot = false, any = false;
  for (unsigned i = 0; i < t.length(); i++) {
    char c = t[i];
    if (c >= '0' && c <= '9') {
      any = true;
      if (dot) { if (++dd > 2) return false; d = d * 10 + (c - '0'); }
      else { r = r * 10 + (c - '0'); if (r >= 10000000ULL) return false; }
    } else if (c == ',' && !dot) continue;
    else if (c == '.' && !dot) dot = true;
    else return false;
  }
  if (!any) return false;
  if (dd == 1) d *= 10;
  uint64_t v = r * 100 + d;
  if (!v) return false;
  paisa = (uint32_t)v;
  return true;
}

// Same for a JSON-ish double (ArduinoJson numbers): rounds to the paisa.
bool moneyFromDouble(double v, uint32_t& paisa) {
  if (!(v > 0) || v >= 10000000.0) return false;
  uint32_t p = (uint32_t)(v * 100.0 + 0.5);
  if (!p) return false;
  paisa = p;
  return true;
}

// "1,250" or "1,250.50"
String fmtPaisa(uint32_t paisa, String (*grp)(uint32_t)) {
  String s = grp(paisa / 100);
  uint32_t p = paisa % 100;
  if (p) { s += '.'; s += (char)('0' + p / 10); s += (char)('0' + p % 10); }
  return s;
}

// ---------- IMAP helpers ----------
// "quoted string" for IMAP commands
String imapQuote(const String& s) {
  String o = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if (c != '\r' && c != '\n') o += c;
  }
  return o + "\"";
}

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
static int32_t tu_days(int y, int m, int d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  int32_t yoe = y - era * 400;
  int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// IMAP INTERNALDATE "12-May-2024 13:42:05 +0545" -> Unix time (UTC)
bool parseImapDate(const String& s, uint32_t& epoch) {
  static const char* M = "janfebmaraprmayjunjulaugsepoctnovdec";
  int d, y, hh, mm, ss, z; char mon[4]; char sign;
  if (sscanf(s.c_str(), " %d-%3c-%d %d:%d:%d %c%4d", &d, mon, &y, &hh, &mm, &ss, &sign, &z) != 8) return false;
  mon[3] = 0;
  for (int i = 0; i < 3; i++) mon[i] = (char)tolower((uint8_t)mon[i]);
  const char* f = strstr(M, mon);
  if (!f || (f - M) % 3) return false;
  int m = (int)(f - M) / 3 + 1;
  if (d < 1 || d > 31 || y < 1970 || hh > 23 || mm > 59 || ss > 60) return false;
  int32_t off = ((z / 100) * 60 + (z % 100)) * 60;
  if (sign == '-') off = -off; else if (sign != '+') return false;
  int64_t t = (int64_t)tu_days(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss - off;
  if (t < 0) return false;
  epoch = (uint32_t)t;
  return true;
}

// ---------- IMAP response parsing ----------
// Display name of a From header: "\"eSewa\" <no-reply@esewa.com.np>" -> "eSewa",
// "alerts@fonepay.com" -> "fonepay.com"
String fromName(const String& from) {
  String f = mimeDecodeHeader(from);
  f.trim();
  int lt = f.indexOf('<');
  String name = lt >= 0 ? f.substring(0, lt) : String("");
  name.trim();
  if (name.length() >= 2 && name[0] == '"' && name[name.length() - 1] == '"') name = name.substring(1, name.length() - 1);
  name.trim();
  if (name.length()) return name;
  String addr = lt >= 0 ? f.substring(lt + 1) : f;
  int gt = addr.indexOf('>');
  if (gt >= 0) addr = addr.substring(0, gt);
  int at = addr.indexOf('@');
  return at >= 0 ? addr.substring(at + 1) : addr;
}

// Numbers after "* SEARCH" that are > after, ascending, at most max of them.
// Returns how many matched in total (may be more than max).
int imapSearchUids(const String& line, uint32_t after, uint32_t* out, int max) {
  int total = 0, n = 0;
  const char* p = line.c_str();
  const char* s = strstr(p, "SEARCH");
  if (!s) return 0;
  s += 6;
  while (*s) {
    while (*s == ' ') s++;
    if (!isdigit((uint8_t)*s)) { while (*s && *s != ' ') s++; continue; }
    uint32_t v = 0;
    while (isdigit((uint8_t)*s)) v = v * 10 + (uint32_t)(*s++ - '0');
    if (v <= after) continue;
    total++;
    // keep the lowest `max` UIDs, sorted
    if (n < max) out[n++] = v;
    else if (v < out[n - 1]) out[n - 1] = v;
    else continue;
    for (int i = n - 1; i > 0 && out[i] < out[i - 1]; i--) { uint32_t t = out[i]; out[i] = out[i - 1]; out[i - 1] = t; }
  }
  return total;
}

// "[UIDVALIDITY 123]" / "[UIDNEXT 456]" in a SELECT response line
bool imapRespCode(const String& line, const char* code, uint32_t& v) {
  String key = String("[") + code + " ";
  int i = line.indexOf(key);
  if (i < 0) return false;
  v = strtoul(line.c_str() + i + key.length(), nullptr, 10);
  return true;
}

// Feeds on the lines of a UID FETCH (UID INTERNALDATE BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)])
// response and collects one record per message. Handles folded headers and
// a UID that comes before or after the header literal.
struct ImapMsg { uint32_t uid; String date, subject, from; };
struct ImapFetchParser {
  ImapMsg* msgs; int max; int count = 0;
  bool inMsg = false; char lastHdr = 0;   // 's' subject, 'f' from
  ImapFetchParser(ImapMsg* m, int mx) : msgs(m), max(mx) {}

  static uint32_t uidIn(const String& l) {
    int i = l.indexOf("UID ");
    return i >= 0 ? strtoul(l.c_str() + i + 4, nullptr, 10) : 0;
  }
  void line(const String& raw) {
    String l = raw;
    while (l.length() && (l[l.length() - 1] == '\r' || l[l.length() - 1] == '\n')) l.remove(l.length() - 1);
    if (l.startsWith("* ") && l.indexOf(" FETCH (") > 0) {
      if (count >= max) { inMsg = false; return; }
      ImapMsg& m = msgs[count++];
      m = ImapMsg();
      m.uid = uidIn(l);
      int d = l.indexOf("INTERNALDATE \"");
      if (d >= 0) { int e = l.indexOf('"', d + 14); if (e > d) m.date = l.substring(d + 14, e); }
      inMsg = true; lastHdr = 0;
      return;
    }
    if (!inMsg || !count) return;
    ImapMsg& m = msgs[count - 1];
    if (l.length() && l[0] == ')') { inMsg = false; return; }
    if (l.length() && (l[0] == ' ' || l[0] == '\t')) {
      // a header continuation line, or " UID 12)" closing the FETCH
      String t = l; t.trim();
      if (t.startsWith("UID ") && t.endsWith(")")) { if (!m.uid) m.uid = uidIn(t); inMsg = false; return; }
      if (lastHdr == 's') m.subject += l;      // RFC 5322 unfolding keeps the whitespace
      else if (lastHdr == 'f') m.from += l;
      return;
    }
    int c = l.indexOf(':');
    if (c <= 0) { lastHdr = 0; return; }
    String k = l.substring(0, c); k.toLowerCase();
    if (k == "subject") { m.subject = l.substring(c + 1); lastHdr = 's'; }
    else if (k == "from") { m.from = l.substring(c + 1); lastHdr = 'f'; }
    else lastHdr = 0;
  }
};

// ---------- Connect helpers ----------
// Fills {{name}} placeholders (spaces inside the braces are fine). Unknown
// names become empty. Values are inserted as given; callers escape them.
String renderTemplate(const String& tpl, const char* const* keys, const String* vals, int n) {
  String o;
  o.reserve(tpl.length() + 64);
  int i = 0, len = tpl.length();
  while (i < len) {
    int open = tpl.indexOf("{{", i);
    if (open < 0) { o += tpl.substring(i); break; }
    int close = tpl.indexOf("}}", open + 2);
    if (close < 0) { o += tpl.substring(i); break; }
    o += tpl.substring(i, open);
    String key = tpl.substring(open + 2, close);
    key.trim();
    for (int k = 0; k < n; k++) if (key == keys[k]) { o += vals[k]; break; }
    i = close + 2;
  }
  return o;
}

// Status map "NEW=new,pending; PREP=cooking; ...; REMOVE=cancelled" applied to a
// value from another system. Returns 0..3 (NEW, PREP, READY, PAID), 4 for
// REMOVE, or -1 when the value isn't known. The device's own names always work.
int mapStatus(const String& map, const String& value) {
  static const char* const NAMES[5] = {"NEW", "PREP", "READY", "PAID", "REMOVE"};
  String v = value; v.trim(); v.toLowerCase();
  if (!v.length()) return -1;
  for (int i = 0; i < 5; i++) { String n = NAMES[i]; n.toLowerCase(); if (v == n) return i; }
  int pos = 0, len = map.length();
  while (pos < len) {
    int semi = map.indexOf(';', pos);
    if (semi < 0) semi = len;
    String grp = map.substring(pos, semi);
    pos = semi + 1;
    int eq = grp.indexOf('=');
    if (eq < 0) continue;
    String target = grp.substring(0, eq); target.trim(); target.toUpperCase();
    int t = -1;
    for (int i = 0; i < 5; i++) if (target == NAMES[i]) t = i;
    if (t < 0) continue;
    String vals = grp.substring(eq + 1);
    int p = 0, vl = vals.length();
    while (p <= vl) {
      int comma = vals.indexOf(',', p);
      if (comma < 0) comma = vl;
      String one = vals.substring(p, comma); one.trim(); one.toLowerCase();
      if (one.length() && one == v) return t;
      p = comma + 1;
    }
  }
  return -1;
}

// 32-bit FNV-1a, for remembering command IDs cheaply
uint32_t fnv1a(const String& s) {
  uint32_t h = 2166136261u;
  for (unsigned i = 0; i < s.length(); i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
  return h;
}

// Lower-case hex of a byte buffer
String toHex(const uint8_t* b, size_t n) {
  static const char* H = "0123456789abcdef";
  String o;
  o.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { o += H[b[i] >> 4]; o += H[b[i] & 15]; }
  return o;
}

// ASCII only, for the SAM voice (it can't say anything else), max n chars
String asciiOnly(const String& s, unsigned n) {
  String o;
  for (unsigned i = 0; i < s.length() && o.length() < n; i++) {
    uint8_t c = (uint8_t)s[i];
    bool space = c == ' ' || (c >= 0x80 && (c & 0xC0) != 0x80);   // non-ASCII char -> one space
    if (space) { if (o.length() && o[o.length() - 1] != ' ') o += ' '; }
    else if (c > 0x20 && c < 0x7F) o += (char)c;
  }
  o.trim();
  return o;
}
