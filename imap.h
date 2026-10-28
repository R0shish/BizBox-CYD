#pragma once
// ==========================================================================
//  Gmail over IMAP (imap.gmail.com:993, App Password).
//  One session reads every NEW message since the last one it looked at
//  (tracked by UIDVALIDITY + last UID in NVS), announces the ones whose
//  subject parses as a payment and marks only those as read. Everything
//  else in the inbox is left exactly as it was.
//  Every read has a deadline: a stalled or chatty server can't hang us.
//  Pure parsing lives in textutil.h and is unit-tested on the desktop.
// ==========================================================================

const int IMAP_BATCH = 10;                  // messages handled per session
const uint32_t IMAP_SESSION_MS = 25000;     // whole session budget
const uint32_t IMAP_CMD_MS = 10000;         // one command's reply
const uint32_t IMAP_STALE_S = 10 * 60;      // older mail is booked, not spoken

struct ImapCfg {
  String user, pass, query, currency;
  bool verifyTls;
};

struct ImapState {      // persisted: where we are in the mailbox
  uint32_t uidValidity;
  uint32_t lastUid;
};

struct ImapPayment {
  uint32_t paisa;
  String note;          // sender name, e.g. "eSewa"
  String title;         // subject, for the orders list
  bool stale;           // received long ago (device was off): don't speak it
};

struct ImapResult {
  bool ok = false;
  bool more = false;    // more new mail is waiting: poll again soon
  String status;
  ImapPayment pays[IMAP_BATCH];
  int count = 0;
};

class ImapSession {
 public:
  explicit ImapSession(uint32_t budgetMs) : end_(millis() + budgetMs) {}
  ~ImapSession() { cl_.stop(); }

  bool open(bool verifyTls);
  // Sends a tagged command; untagged lines go to onLine. Returns 'O' (OK),
  // 'N' (NO), 'B' (BAD) or 0 on timeout / lost connection.
  char cmd(const String& c, void (*onLine)(const String&, void*) = nullptr, void* ctx = nullptr,
           bool secret = false);
  String lastText;      // human text of the last tagged reply

 private:
  bool readLine(String& out, uint32_t deadline);
  uint32_t left() const { int32_t l = (int32_t)(end_ - millis()); return l > 0 ? (uint32_t)l : 0; }
  WiFiClientSecure cl_;
  uint32_t end_;
  int tag_ = 0;
};

// Google Trust Services roots (GTS Root R1 and R4): every Gmail / Apps Script
// chain ends at one of them. Valid until 2036.
static const char GOOGLE_ROOTS_PEM[] PROGMEM =
"-----BEGIN CERTIFICATE-----\n"
"MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw\n"
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
"MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
"MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
"Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA\n"
"A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo\n"
"27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w\n"
"Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw\n"
"TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl\n"
"qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH\n"
"szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8\n"
"Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk\n"
"MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92\n"
"wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p\n"
"aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN\n"
"VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID\n"
"AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n"
"FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb\n"
"C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe\n"
"QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy\n"
"h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4\n"
"7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J\n"
"ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef\n"
"MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/\n"
"Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT\n"
"6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ\n"
"0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm\n"
"2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb\n"
"bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\n"
"-----END CERTIFICATE-----\n"
"-----BEGIN CERTIFICATE-----\n"
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
"-----END CERTIFICATE-----\n";

bool ImapSession::open(bool verifyTls) {
  if (verifyTls) cl_.setCACert(GOOGLE_ROOTS_PEM);
  else cl_.setInsecure();
  cl_.setHandshakeTimeout(10);              // seconds, on both cores
  if (!cl_.connect("imap.gmail.com", 993, (int32_t)min<uint32_t>(left(), 12000))) return false;
  String greet;
  return readLine(greet, millis() + min<uint32_t>(left(), IMAP_CMD_MS)) && greet.startsWith("* OK");
}

bool ImapSession::readLine(String& out, uint32_t deadline) {
  out = "";
  for (;;) {
    while (cl_.available()) {
      int c = cl_.read();
      if (c < 0) break;
      if (c == '\n') return true;
      if (out.length() < 1024) out += (char)c;    // long lines are cut, not buffered whole
    }
    if ((int32_t)(deadline - millis()) <= 0 || left() == 0) return false;
    if (!cl_.connected() && !cl_.available()) return false;
    delay(5);
  }
}

char ImapSession::cmd(const String& c, void (*onLine)(const String&, void*), void* ctx, bool secret) {
  char tag[8];
  snprintf(tag, sizeof tag, "a%d ", ++tag_);
  cl_.print(String(tag) + c + "\r\n");
  (void)secret;                           // the command is never logged
  uint32_t deadline = millis() + min<uint32_t>(left(), IMAP_CMD_MS);
  String line;
  while (readLine(line, deadline)) {
    if (line.startsWith(tag)) {
      String r = line.substring(strlen(tag));
      lastText = r;
      return r.startsWith("OK") ? 'O' : r.startsWith("NO") ? 'N' : r.startsWith("BAD") ? 'B' : 0;
    }
    if (onLine) onLine(line, ctx);
  }
  lastText = "timeout";
  return 0;
}

// --- callbacks for cmd() ---
struct SelectInfo { uint32_t validity = 0, next = 0; };
static void imapOnSelect(const String& l, void* c) {
  SelectInfo* s = (SelectInfo*)c;
  imapRespCode(l, "UIDVALIDITY", s->validity);
  imapRespCode(l, "UIDNEXT", s->next);
}
struct SearchInfo { uint32_t after; uint32_t uids[IMAP_BATCH]; int n = 0, total = 0; };
static void imapOnSearch(const String& l, void* c) {
  SearchInfo* s = (SearchInfo*)c;
  if (!l.startsWith("* SEARCH")) return;
  s->total = imapSearchUids(l, s->after, s->uids, IMAP_BATCH);
  s->n = min(s->total, IMAP_BATCH);
}
static void imapOnFetch(const String& l, void* c) { ((ImapFetchParser*)c)->line(l); }

// One polling session. `st` is updated (caller persists it) only for mail
// that was fully handled, so nothing is skipped after a failure.
bool imapPoll(const ImapCfg& c, ImapState& st, ImapResult& r, uint32_t nowEpoch) {
  if (c.user.isEmpty() || c.pass.isEmpty()) { r.status = "Credentials missing"; return false; }
  ImapSession s(IMAP_SESSION_MS);
  if (!s.open(c.verifyTls)) {
    r.status = c.verifyTls ? "Can't reach imap.gmail.com (or certificate check failed)"
                           : "Can't reach imap.gmail.com";
    return false;
  }
  if (s.cmd("LOGIN " + imapQuote(c.user) + " " + imapQuote(c.pass), nullptr, nullptr, true) != 'O') {
    r.status = "Login rejected - check the 16-char App Password";
    return false;
  }
  SelectInfo sel;
  if (s.cmd("SELECT INBOX", imapOnSelect, &sel) != 'O' || !sel.validity || !sel.next) {
    r.status = "Can't open INBOX";
    return false;
  }
  // First run (stored validity is 0) or the mailbox was rebuilt: start from
  // now, never replay old mail. (Not keyed on lastUid: an empty inbox has
  // UIDNEXT 1, so lastUid stays 0 until the first mail arrives.)
  if (sel.validity != st.uidValidity) {
    st.uidValidity = sel.validity;
    st.lastUid = sel.next - 1;
    s.cmd("LOGOUT");
    r.ok = true;
    r.status = "Connected - watching for new mail";
    return true;
  }
  SearchInfo se;
  se.after = st.lastUid;
  String q = c.query.length() ? c.query : String("UNSEEN");
  if (s.cmd("UID SEARCH UID " + String(st.lastUid + 1) + ":* " + q, imapOnSearch, &se) != 'O') {
    r.status = "Search filter rejected: " + utf8Cut(s.lastText, 60);
    return false;
  }
  if (!se.n) {
    s.cmd("LOGOUT");
    r.ok = true;
    r.status = "Connected - inbox clean";
    return true;
  }
  String set;
  for (int i = 0; i < se.n; i++) { if (i) set += ','; set += String(se.uids[i]); }
  ImapMsg msgs[IMAP_BATCH];
  ImapFetchParser fp(msgs, IMAP_BATCH);
  if (s.cmd("UID FETCH " + set + " (UID INTERNALDATE BODY.PEEK[HEADER.FIELDS (SUBJECT FROM)])",
            imapOnFetch, &fp) != 'O') {
    r.status = "Fetch failed";
    return false;
  }
  String seen;
  int other = 0;
  for (int i = 0; i < fp.count; i++) {
    ImapMsg& m = msgs[i];
    String subject = mimeDecodeHeader(m.subject);
    subject.trim();
    uint32_t paisa = 0;
    if (!parseAmount(subject, paisa, c.currency)) { other++; continue; }
    ImapPayment& p = r.pays[r.count++];
    p.paisa = paisa;
    p.note = utf8Cut(fromName(m.from), 24);
    p.title = utf8Cut(subject, 40);
    uint32_t sent = 0;
    p.stale = nowEpoch > 1600000000UL && parseImapDate(m.date, sent) && nowEpoch > sent + IMAP_STALE_S;
    if (seen.length()) seen += ',';
    seen += String(m.uid);
  }
  if (seen.length()) s.cmd("UID STORE " + seen + " +FLAGS.SILENT (\\Seen)");
  s.cmd("LOGOUT");
  // Everything up to the highest UID we fetched has been looked at
  st.lastUid = se.uids[se.n - 1];
  r.more = se.total > se.n;
  r.ok = true;
  r.status = r.count ? "Announced " + String(r.count) + " payment" + (r.count > 1 ? "s" : "")
                     : "Connected - " + String(other) + " new non-payment email" + (other == 1 ? "" : "s") + " left unread";
  return true;
}
